#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"
#include "Bootil/Bootil.h"

class CUtilModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void Think(bool bSimulating) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual const char* Name() { return "util"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; };
};

static CUtilModule g_pUtilModule;
IModule* pUtilModule = &g_pUtilModule;

struct CompressEntry
{
	int iCallback = -1;
	bool bCompress = true;

	const char* pData;
	int iDataReference = -1; // Keeping a reference to stop GC from potentially nuking it.

	int iLength;
	int iLevel;
	int iDictSize;
	Bootil::AutoBuffer buffer;
};

struct CompressData
{
	bool bRun = true;
	bool bInvalidEverything = false;
	std::vector<CompressEntry*> pQueue;
	std::vector<CompressEntry*> pFinished;
	CThreadFastMutex pMutex;
};
#if ARCHITECTURE_IS_X86_64
static long long unsigned CompressThread(void* data)
#else
static unsigned CompressThread(void* data)
#endif
{
	CompressData* cData = (CompressData*)data;
	while (cData->bRun)
	{
		cData->pMutex.Lock();
		std::vector<CompressEntry*> batch;
		for (CompressEntry* entry : cData->pQueue)
		{
			batch.push_back(entry);
		}
		cData->pQueue.clear();
		cData->pMutex.Unlock();

		for (CompressEntry* entry : batch)
		{
			if (cData->bInvalidEverything) { continue; }

			if (entry->bCompress)
				Bootil::Compression::LZMA::Compress(entry->pData, entry->iLength, entry->buffer, entry->iLevel, entry->iDictSize);
			else
				Bootil::Compression::LZMA::Extract(entry->pData, entry->iLength, entry->buffer);

			cData->pMutex.Lock();
			cData->pFinished.push_back(entry);
			cData->pMutex.Unlock();
		}

		if(cData->bInvalidEverything) // Should me make a lock? Idk.
		{
			for (CompressEntry* entry : batch)
			{
				delete entry;
			}

			for (CompressEntry* entry : cData->pFinished)
			{
				delete entry;
			}
			cData->pFinished.clear();
		}

		batch.clear();

		if (cData->bInvalidEverything)
			cData->bInvalidEverything = false;

		ThreadSleep(10);
	}

	return 0;
}

static ThreadHandle_t threadHandle = NULL;
static CompressData threaddata;
/*
 * If the Async function's arent used. We simply won't create a thread.
 * This should save a bit of CPU usage since we won't have a thread that is permantly in a while loop,
 * calling ThreadSleep every 10 milliseconds or so.
 */
inline void StartThread()
{
	if (threadHandle)
		return;

	threadHandle = CreateSimpleThread((ThreadFunc_t)CompressThread, &threaddata);
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

	threaddata.pQueue.push_back(entry);

	StartThread();

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

	threaddata.pQueue.push_back(entry);

	StartThread();

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

int iStartTop = -1;
bool bNoError = false;
std::vector<int> pCurrentTableScope;
extern void TableToJSONRecursive(Bootil::Data::Tree& pTree);
void TableToJSONRecursive(Bootil::Data::Tree& pTree)
{
	g_Lua->Push(-2);
	bool bEqual = false;
	for (int iReference : pCurrentTableScope)
	{
		g_Lua->ReferencePush(iReference);
		if (g_Lua->Equal(-1, -2))
		{
			bEqual = true;
			g_Lua->Pop(1);
			break;
		}

		g_Lua->Pop(1);
	}

	if (bEqual) {
		if (bNoError)
			return;

		g_Lua->Pop(g_Lua->Top() - iStartTop); // Since we added a unknown amount to the stack, we need to throw everything back out
		g_Lua->ThrowError("attempt to serialize structure with cyclic reference");
	}

	pCurrentTableScope.push_back(g_Lua->ReferenceCreate());

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
			if (iKeyType == GarrysMod::Lua::Type::String)
				key = g_Lua->GetString(-2); // lua_next won't nuke itself since we don't convert the value
			else {
				g_Lua->Push(-2);
				key = g_Lua->GetString(-1); // lua_next nukes itself when the key isn't an actual string
				g_Lua->Pop(1);
			}
		}
		//Msg("Key: %s (%s)\n", key, g_Lua->GetActualTypeName(iKeyType));
		switch (g_Lua->GetType(-1))
		{
			case GarrysMod::Lua::Type::String:
				if (isSequential)
					pTree.AddChild().Value(g_Lua->GetString(-1));
				else
					pTree.SetChild(key, g_Lua->GetString(-1));
				//Msg("Value: %s\n", g_Lua->GetString(-1));
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

					//Msg("Value: %f\n", pNumber);
				}
				break;
			case GarrysMod::Lua::Type::Bool:
				if (isSequential)
					pTree.AddChild().Var(g_Lua->GetBool(-1));
				else
					pTree.SetChildVar(key, g_Lua->GetBool(-1));
				//Msg("Value: %s\n", g_Lua->GetBool(-1) ? "true" : "false");
				break;
			case GarrysMod::Lua::Type::Table: // now make it recursive >:D
				g_Lua->Push(-1);
				g_Lua->PushNil();
				//Msg("Value: Table recursive start\n");
				if (isSequential)
					TableToJSONRecursive(pTree.AddChild());
				else
					TableToJSONRecursive(pTree.AddChild(key));
				//Msg("Value: Table recursive end\n");
				break;
			case GarrysMod::Lua::Type::Vector:
				{
					Vector* vec = Get_Vector(-1, true);
					if (!vec)
						break;

					char buffer[56];
					snprintf(buffer, sizeof(buffer), "[%.16g %.16g %.16g]", vec->x, vec->y, vec->z); // Do we even need to be this percice?
					if (isSequential)
						pTree.AddChild().Value(buffer);
					else
						pTree.SetChild(key, buffer);
					//Msg("Value: %s\n", buffer);
				}
				break;
			case GarrysMod::Lua::Type::Angle:
				{
					QAngle* ang = Get_Angle(-1, true);
					if (!ang)
						break;

					char buffer[48];
					snprintf(buffer, sizeof(buffer), "{%.12g %.12g %.12g}", ang->x, ang->y, ang->z);
					if (isSequential)
						pTree.AddChild().Value(buffer);
					else
						pTree.SetChild(key, buffer);
					//Msg("Value: %s\n", buffer);
				}
				break;
			default:
				break; // We should fallback to nil
		}

		g_Lua->Pop(1);
	}

	g_Lua->Pop(1);

	g_Lua->ReferenceFree(pCurrentTableScope.back());
	pCurrentTableScope.pop_back();
}

LUA_FUNCTION_STATIC(util_TableToJSON)
{
	LUA->CheckType(1, GarrysMod::Lua::Type::Table);
	bool bPretty = LUA->GetBool(2);
	bNoError = LUA->GetBool(3);

	iStartTop = LUA->Top();
	Bootil::Data::Tree pTree;
	LUA->Push(1);
	LUA->PushNil();

	TableToJSONRecursive(pTree);

	/*for (const Bootil::Data::Tree& pChild : pTree.Children())
	{
		Msg("Name: %s\n", pChild.Name().c_str());
		Msg("Value: %s\n", pChild.Value().c_str());
	}*/

	Bootil::BString pOut;
	Bootil::Data::Json::Export(pTree, pOut, bPretty);

	LUA->PushString(pOut.c_str());

	return 1;
}

void CUtilModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	threaddata.bRun = true;
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
	threaddata.bInvalidEverything = true;

	for (CompressEntry* entry : threaddata.pQueue)
	{
		delete entry;
	}
	threaddata.pQueue.clear();

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

	if (threaddata.bInvalidEverything) // Wait for the Thread to be ready again
		return;

	if (threaddata.pFinished.size() == 0)
		return;

	threaddata.pMutex.Lock();
	for(CompressEntry* entry : threaddata.pFinished)
	{
		g_Lua->ReferencePush(entry->iCallback);
			g_Lua->PushString((const char*)entry->buffer.GetBase(), entry->buffer.GetWritten());
			g_Lua->ReferenceFree(entry->iDataReference); // Free our data
		g_Lua->CallFunctionProtected(1, 0, true);
		delete entry;
	}
	threaddata.pFinished.clear();
	threaddata.pMutex.Unlock();
}

void CUtilModule::Shutdown()
{
	threaddata.bRun = false;
}