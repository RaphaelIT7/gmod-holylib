#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"
#include "Bootil/Bootil.h"

class CUtilModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void Think(bool bSimulating) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual const char* Name() { return "util"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; };
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
		if (iDataReference != -1)
			g_Lua->ReferenceFree(iDataReference);

		if (iCallback != -1)
			g_Lua->ReferenceFree(iCallback);
	}

	int iCallback = -1;
	bool bCompress = true;

	const char* pData;
	int iDataReference = -1; // Keeping a reference to stop GC from potentially nuking it.

	int iLength;
	int iLevel;
	int iDictSize;
	Bootil::AutoBuffer buffer;
};

static bool bInvalidateEverything = false;
static std::vector<CompressEntry*> pFinishedEntries;
static CThreadFastMutex pFinishMutex;
static void CompressJob(CompressEntry*& entry)
{
	if (bInvalidateEverything) { return; }

	if (entry->bCompress)
		Bootil::Compression::LZMA::Compress(entry->pData, entry->iLength, entry->buffer, entry->iLevel, entry->iDictSize);
	else
		Bootil::Compression::LZMA::Extract(entry->pData, entry->iLength, entry->buffer);

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
		iCallback = LUA->ReferenceCreate();
	} else {
		iLevel = g_Lua->CheckNumberOpt(2, 5);
		iDictSize = g_Lua->CheckNumberOpt(3, 65536);
		LUA->CheckType(4, GarrysMod::Lua::Type::Function);
		LUA->Push(4);
		iCallback = LUA->ReferenceCreate();
	}

	CompressEntry* entry = new CompressEntry;
	entry->iCallback = iCallback;
	entry->iDictSize = iDictSize;
	entry->iLength = iLength;
	entry->iLevel = iLevel;
	entry->pData = pData;
	LUA->Push(1);
	entry->iDataReference = LUA->ReferenceCreate();

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
	int iCallback = LUA->ReferenceCreate();

	CompressEntry* entry = new CompressEntry;
	entry->bCompress = false;
	entry->iCallback = iCallback;
	entry->iLength = iLength;
	entry->pData = pData;
	LUA->Push(1);
	entry->iDataReference = LUA->ReferenceCreate();

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

int iRecursiveStartTop = -1;
bool bRecursiveNoError = false;
std::vector<int> pRecursiveTableScope;
extern void TableToJSONRecursive(Bootil::Data::Tree& pTree);
static char buffer[128];
void TableToJSONRecursive(Bootil::Data::Tree& pTree)
{
	bool bEqual = false;
	for (int iReference : pRecursiveTableScope)
	{
		g_Lua->ReferencePush(iReference);
		if (g_Lua->Equal(-1, -3))
		{
			bEqual = true;
			g_Lua->Pop(1);
			break;
		}

		g_Lua->Pop(1);
	}

	if (bEqual) {
		if (bRecursiveNoError)
		{
			g_Lua->Pop(2); // Don't forget to cleanup
			return;
		}

		g_Lua->Pop(g_Lua->Top() - iRecursiveStartTop); // Since we added a unknown amount to the stack, we need to throw everything back out
		g_Lua->ThrowError("attempt to serialize structure with cyclic reference");
	}

	g_Lua->Push(-2);
	pRecursiveTableScope.push_back(g_Lua->ReferenceCreate());

	int idx = 1;
	while (g_Lua->Next(-2)) {
		// In bootil, you just don't give a child a name to indicate that it's sequentail. 
		// so we need to support that.
		bool isSequential = false; 
		int iKeyType = g_Lua->GetType(-2);
		double iKey = iKeyType == GarrysMod::Lua::Type::Number ? g_Lua->GetNumber(-2) : 0;
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
				case GarrysMod::Lua::Type::STRING:
					key = g_Lua->GetString(-2); // lua_next won't nuke itself since we don't convert the value
					break;
				case GarrysMod::Lua::Type::Number:
					g_Lua->Push(-2);
					key = g_Lua->GetString(-1); // lua_next nukes itself when the key isn't an actual string
					g_Lua->Pop(1);
					break;
				case GarrysMod::Lua::Type::Bool:
					g_Lua->Push(-2);
					key = g_Lua->GetString(-1); // lua_next nukes itself when the key isn't an actual string
					g_Lua->Pop(1);
					break;
				default:
					break;
			}

			if (!key)
			{
				g_Lua->Pop(1); // Pop the value off the stack for lua_next to work
				continue;
			}
		}

		switch (g_Lua->GetType(-1))
		{
			case GarrysMod::Lua::Type::String:
				if (isSequential)
					pTree.AddChild().Value(g_Lua->GetString(-1));
				else
					pTree.SetChild(key, g_Lua->GetString(-1));
				break;
			case GarrysMod::Lua::Type::Number:
				{
					double pNumber = g_Lua->GetNumber(-1);
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
					pTree.AddChild().Var(g_Lua->GetBool(-1));
				else
					pTree.SetChildVar(key, g_Lua->GetBool(-1));
				break;
			case GarrysMod::Lua::Type::Table: // now make it recursive >:D
				g_Lua->Push(-1);
				g_Lua->PushNil();
				if (isSequential)
					TableToJSONRecursive(pTree.AddChild());
				else
					TableToJSONRecursive(pTree.AddChild(key));
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
					QAngle* ang = Get_Angle(-1, true);
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

		g_Lua->Pop(1);
	}

	g_Lua->Pop(1);

	g_Lua->ReferenceFree(pRecursiveTableScope.back());
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

	TableToJSONRecursive(pTree);

	Bootil::BString pOut;
	Bootil::Data::Json::Export(pTree, pOut, bPretty);

	LUA->PushString(pOut.c_str());

	return 1;
}

void CUtilModule::LuaInit(bool bServerInit)
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

void CUtilModule::LuaShutdown()
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
		Util::RemoveFunc("AsyncCompress");
		Util::RemoveFunc("AsyncDecompress");
		Util::RemoveFunc("FancyTableToJSON");
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
		g_Lua->ReferencePush(entry->iCallback);
			g_Lua->PushString((const char*)entry->buffer.GetBase(), entry->buffer.GetWritten());
		g_Lua->CallFunctionProtected(1, 0, true);
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