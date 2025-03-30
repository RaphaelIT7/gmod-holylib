#include "LuaInterface.h"
#include "module.h"
#include "lua.h"
#include "Bootil/Bootil.h"

class CUtilModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void Think(bool bSimulating) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual const char* Name() { return "util"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
};

static CUtilModule g_pUtilModule;
IModule* pUtilModule = &g_pUtilModule;

static IThreadPool* pCompressPool = NULL;
static IThreadPool* pDecompressPool = NULL;
static void OnCompressThreadsChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	pCompressPool->ExecuteAll(); // ToDo: Do we need to do this?
	pCompressPool->Stop();
	Util::StartThreadPool(pCompressPool, ((ConVar*)convar)->GetInt());
}

static void OnDecompressThreadsChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	pDecompressPool->ExecuteAll();
	pDecompressPool->Stop();
	Util::StartThreadPool(pDecompressPool, ((ConVar*)convar)->GetInt());
}

static ConVar compressthreads("holylib_util_compressthreads", "1", 0, "The number of threads to use for util.AsyncCompress", OnCompressThreadsChange);
static ConVar decompressthreads("holylib_util_decompressthreads", "1", 0, "The number of threads to use for util.AsyncDecompress", OnDecompressThreadsChange);


struct CompressEntry
{
	~CompressEntry()
	{
		if (!ThreadInMainThread())
		{
			Warning("holylib: A CompressEntry was deleted on a random thread! This should never happen!\n");
			return; // This will be a memory, but we would never want to potentially break the Lua state.
		}

		if (iDataReference != -1 && g_Lua)
			Util::ReferenceFree(iDataReference, "CompressEntry::~CompressEntry - Data");

		if (iCallback != -1 && g_Lua)
			Util::ReferenceFree(iCallback, "CompressEntry::~CompressEntry - Callback");
	}

	int iCallback = -1;
	bool bCompress = true;
	bool bFailed = false;

	const char* pData = NULL;
	int iDataReference = -1; // Keeping a reference to stop GC from potentially nuking it.

	int iLength;
	int iLevel;
	int iDictSize;
	double iRatio;
	Bootil::AutoBuffer buffer;
	GarrysMod::Lua::ILuaInterface* pLua = NULL;
};

static bool bInvalidateEverything = false;
static std::vector<CompressEntry*> pFinishedEntries;
static CThreadFastMutex pFinishMutex;
static void CompressJob(CompressEntry*& entry)
{
	if (bInvalidateEverything) {
		return;
	}

	if (entry->bCompress) {
		entry->bFailed = Bootil::Compression::LZMA::Compress(entry->pData, entry->iLength, entry->buffer, entry->iLevel, entry->iDictSize);
	} else {
		entry->bFailed = Bootil::Compression::LZMA::Extract(entry->pData, entry->iLength, entry->buffer, entry->iRatio);
	}

	pFinishMutex.Lock();
	pFinishedEntries.push_back(entry);
	pFinishMutex.Unlock();
}

/*
 * If the Async function's arent used. We simply won't create the threadpools.
 * This should save a bit of CPU usage since we won't have a thread that is permantly in a while loop,
 * calling ThreadSleep every 10 milliseconds or so.
 */
inline void StartThread()
{
	if (pCompressPool)
		return;

	pCompressPool = V_CreateThreadPool();
	Util::StartThreadPool(pCompressPool, compressthreads.GetInt());

	pDecompressPool = V_CreateThreadPool();
	Util::StartThreadPool(pDecompressPool, decompressthreads.GetInt());
}

LUA_FUNCTION_STATIC(util_AsyncCompress)
{
	const char* pData = LUA->CheckString(1);
	int iLength = LUA->ObjLen(1);
	int iLevel = 5;
	int iDictSize = 65536;
	int iCallback = -1;
	if (LUA->IsType(2, GarrysMod::Lua::Type::Function))
	{
		LUA->Push(2);
		iCallback = Util::ReferenceCreate("util.AsyncCompress - Callback1");
	} else {
		iLevel = (int)LUA->CheckNumberOpt(2, 5);
		iDictSize = (int)LUA->CheckNumberOpt(3, 65536);
		LUA->CheckType(4, GarrysMod::Lua::Type::Function);
		LUA->Push(4);
		iCallback = Util::ReferenceCreate("util.AsyncCompress - Callback2");
	}

	CompressEntry* entry = new CompressEntry;
	entry->iCallback = iCallback;
	entry->iDictSize = iDictSize;
	entry->iLength = iLength;
	entry->iLevel = iLevel;
	entry->pData = pData;
	LUA->Push(1);
	entry->iDataReference = Util::ReferenceCreate("util.AsyncCompress - Data");
	entry->pLua = LUA;

	StartThread();

	pCompressPool->QueueCall(CompressJob, entry);

	return 0;
}

LUA_FUNCTION_STATIC(util_AsyncDecompress)
{
	const char* pData = LUA->CheckString(1);
	int iLength = LUA->ObjLen(1);
	LUA->CheckType(2, GarrysMod::Lua::Type::Function);
	LUA->Push(2);
	int iCallback = Util::ReferenceCreate("util.AsyncDecompress - Callback");

	double ratio = LUA->CheckNumberOpt(3, 0.98); // 98% ratio by default

	CompressEntry* entry = new CompressEntry;
	entry->bCompress = false;
	entry->iCallback = iCallback;
	entry->iLength = iLength;
	entry->pData = pData;
	entry->iRatio = ratio;
	LUA->Push(1);
	entry->iDataReference = Util::ReferenceCreate("util.AsyncDecompress - Data");
	entry->pLua = LUA;

	StartThread();

	pDecompressPool->QueueCall(CompressJob, entry);

	return 0;
}

inline bool IsInt(double pNumber)
{
	return static_cast<int>(pNumber) == pNumber && INT32_MAX >= pNumber && pNumber >= INT32_MIN;
}

inline bool IsInt(float pNumber)
{
	return static_cast<int>(pNumber) == pNumber && INT32_MAX >= pNumber && pNumber >= INT32_MIN;
}

static int iRecursiveStartTop = -1;
static bool bRecursiveNoError = false;
static std::vector<int> pRecursiveTableScope;
extern void TableToJSONRecursive(GarrysMod::Lua::ILuaInterface* pLua, Bootil::Data::Tree& pTree);
static char buffer[128];
void TableToJSONRecursive(GarrysMod::Lua::ILuaInterface* pLua, Bootil::Data::Tree& pTree)
{
	bool bEqual = false;
	for (int iReference : pRecursiveTableScope)
	{
		Util::ReferencePush(pLua, iReference);
		if (pLua->Equal(-1, -3))
		{
			bEqual = true;
			pLua->Pop(1);
			break;
		}

		pLua->Pop(1);
	}

	if (bEqual) {
		if (bRecursiveNoError)
		{
			pLua->Pop(2); // Don't forget to cleanup
			return;
		}

		pLua->Pop(pLua->Top() - iRecursiveStartTop); // Since we added a unknown amount to the stack, we need to throw everything back out
		pLua->ThrowError("attempt to serialize structure with cyclic reference");
	}

	pLua->Push(-2);
	pRecursiveTableScope.push_back(Util::ReferenceCreate("TableToJSONRecursive - Scope"));

	int idx = 1;
	while (pLua->Next(-2)) {
		// In bootil, you just don't give a child a name to indicate that it's sequentail. 
		// so we need to support that.
		bool isSequential = false; 
		int iKeyType = pLua->GetType(-2);
		double iKey = iKeyType == GarrysMod::Lua::Type::Number ? pLua->GetNumber(-2) : 0;
		if (iKey != 0 && iKey == idx)
		{
			isSequential = true;
			++idx;
		}

		const char* key = NULL; // In JSON a key is ALWAYS a string
		if (!isSequential)
		{
			switch (iKeyType)
			{
				case GarrysMod::Lua::Type::String:
					key = pLua->GetString(-2); // lua_next won't nuke itself since we don't convert the value
					break;
				case GarrysMod::Lua::Type::Number:
					pLua->Push(-2);
					key = pLua->GetString(-1); // lua_next nukes itself when the key isn't an actual string
					pLua->Pop(1);
					break;
				case GarrysMod::Lua::Type::Bool:
					pLua->Push(-2);
					key = pLua->GetString(-1); // lua_next nukes itself when the key isn't an actual string
					pLua->Pop(1);
					break;
				default:
					break;
			}

			if (!key)
			{
				pLua->Pop(1); // Pop the value off the stack for lua_next to work
				continue;
			}
		}

		switch (pLua->GetType(-1))
		{
			case GarrysMod::Lua::Type::String:
				if (isSequential)
					pTree.AddChild().Value(pLua->GetString(-1));
				else
					pTree.SetChild(key, pLua->GetString(-1));
				break;
			case GarrysMod::Lua::Type::Number:
				{
					double pNumber = pLua->GetNumber(-1);
					if (IsInt(pNumber))
						if (isSequential)
							pTree.AddChild().Var(static_cast<int>(pNumber));
						else
							pTree.SetChildVar(key, static_cast<int>(pNumber));
					else
						if (isSequential)
							pTree.AddChild().Var(pNumber);
						else
							pTree.SetChildVar(key, pNumber);
				}
				break;
			case GarrysMod::Lua::Type::Bool:
				if (isSequential)
					pTree.AddChild().Var(pLua->GetBool(-1));
				else
					pTree.SetChildVar(key, pLua->GetBool(-1));
				break;
			case GarrysMod::Lua::Type::Table: // now make it recursive >:D
				pLua->Push(-1);
				pLua->PushNil();
				if (isSequential)
					TableToJSONRecursive(pLua, pTree.AddChild());
				else
					TableToJSONRecursive(pLua, pTree.AddChild(key));
				break;
			case GarrysMod::Lua::Type::Vector:
				{
					Vector* vec = Get_Vector(-1, true);
					if (!vec)
						break;

					snprintf(buffer, sizeof(buffer), "[%.16g %.16g %.16g]", vec->x, vec->y, vec->z); // Do we even need to be this percice?
					if (isSequential)
						pTree.AddChild().Value(buffer);
					else
						pTree.SetChild(key, buffer);
				}
				break;
			case GarrysMod::Lua::Type::Angle:
				{
					QAngle* ang = Get_QAngle(-1, true);
					if (!ang)
						break;

					snprintf(buffer, sizeof(buffer), "{%.12g %.12g %.12g}", ang->x, ang->y, ang->z);
					if (isSequential)
						pTree.AddChild().Value(buffer);
					else
						pTree.SetChild(key, buffer);
				}
				break;
			default:
				break; // We should fallback to nil
		}

		pLua->Pop(1);
	}

	pLua->Pop(1);

	Util::ReferenceFree(pRecursiveTableScope.back(), "TableToJSONRecursive - Free scope");
	pRecursiveTableScope.pop_back();
}

LUA_FUNCTION_STATIC(util_TableToJSON)
{
	LUA->CheckType(1, GarrysMod::Lua::Type::Table);
	bool bPretty = LUA->GetBool(2);
	bRecursiveNoError = LUA->GetBool(3);

	iRecursiveStartTop = LUA->Top();
	Bootil::Data::Tree pTree;
	LUA->Push(1);
	LUA->PushNil();

	TableToJSONRecursive(LUA, pTree);

	Bootil::BString pOut;
	Bootil::Data::Json::Export(pTree, pOut, bPretty);

	LUA->PushString(pOut.c_str());

	return 1;
}

void CUtilModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	if (Util::PushTable("util"))
	{
		Util::AddFunc(util_AsyncCompress, "AsyncCompress");
		Util::AddFunc(util_AsyncDecompress, "AsyncDecompress");
		Util::AddFunc(util_TableToJSON, "FancyTableToJSON");
		Util::PopTable();
	}
}

void CUtilModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	bInvalidateEverything = true;
	if (pCompressPool)
		pCompressPool->ExecuteAll();

	if (pDecompressPool) // Apparently can be NULL?
		pDecompressPool->ExecuteAll();

	for (CompressEntry* entry : pFinishedEntries)
	{
		delete entry;
	}
	pFinishedEntries.clear();

	if (Util::PushTable("util"))
	{
		Util::RemoveField("AsyncCompress");
		Util::RemoveField("AsyncDecompress");
		Util::RemoveField("FancyTableToJSON");
		Util::PopTable();
	}
}

void CUtilModule::Think(bool simulating)
{
	VPROF_BUDGET("HolyLib - CUtilModule::Think", VPROF_BUDGETGROUP_HOLYLIB);

	if (bInvalidateEverything) // Wait for the Thread to be ready again
		return;

	if (pFinishedEntries.size() == 0)
		return;

	pFinishMutex.Lock();
	for(CompressEntry* entry : pFinishedEntries)
	{
		if (entry->pLua)
		{
			Util::ReferencePush(entry->pLua, entry->iCallback);
			if (entry->bFailed)
			{
				entry->pLua->PushNil();
			} else {
				entry->pLua->PushString((const char*)entry->buffer.GetBase(), entry->buffer.GetWritten());
			}
			entry->pLua->CallFunctionProtected(1, 0, true);
		}
		delete entry;
	}
	pFinishedEntries.clear();
	pFinishMutex.Unlock();
}

void CUtilModule::Shutdown()
{
	if (pCompressPool)
	{
		V_DestroyThreadPool(pCompressPool);
		V_DestroyThreadPool(pDecompressPool);
		pCompressPool = NULL;
		pDecompressPool = NULL;
	}
}