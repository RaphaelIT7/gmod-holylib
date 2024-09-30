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

static ThreadHandle_t threadhandle;
static CompressData threaddata;
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

	return 0;
}

extern void TableToJSONRecursive(Bootil::Data::Tree& pTree);
void TableToJSONRecursive(Bootil::Data::Tree& pTree)
{
	int idx = 1;
	while (g_Lua->Next(-2)) {
		g_Lua->Push(-2);

		// In bootil, you just don't give a child a name to indicate that it's sequentail. 
		// so we need to support that.
		bool isSequential = false; 
		int iKeyType = g_Lua->GetType(-1);
		double iKey = iKeyType == GarrysMod::Lua::Type::Number ? g_Lua->GetNumber(-1) : 0;
		if (iKey != 0 && iKey == idx)
		{
			isSequential = true;
			++idx;
		}

		const char* key = g_Lua->GetString(-1); // In JSON a key is ALWAYS a string
		//Msg("Key: %s (%s)\n", key, g_Lua->GetActualTypeName(iKeyType));
		switch (g_Lua->GetType(-1))
		{
			case GarrysMod::Lua::Type::String:
				if (isSequential)
					pTree.AddChild().Value(g_Lua->GetString(-2));
				else
					pTree.SetChild(key, g_Lua->GetString(-2));
				//Msg("Value: %s\n", g_Lua->GetString(-2));
				break;
			case GarrysMod::Lua::Type::Number:
				{
					double pNumber = g_Lua->GetNumber(-2);
					if (floor(pNumber) == pNumber && INT32_MAX >= pNumber && pNumber >= INT32_MIN)
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
					pTree.AddChild().Var(g_Lua->GetBool(-2));
				else
					pTree.SetChildVar(key, g_Lua->GetBool(-2));
				//Msg("Value: %s\n", g_Lua->GetBool(-2) ? "true" : "false");
				break;
			case GarrysMod::Lua::Type::Table: // now make it recursive >:D
				g_Lua->Push(-2);
				g_Lua->PushNil();
				TableToJSONRecursive(pTree.AddChild(key));
				break;
			default:
				break; // We should fallback to nil
		}

		g_Lua->Pop(2);
	}

	g_Lua->Pop(1);
}

LUA_FUNCTION_STATIC(util_TableToJSON)
{
	LUA->CheckType(1, GarrysMod::Lua::Type::Table);
	bool bPretty = LUA->GetBool(2);

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
	threadhandle = CreateSimpleThread((ThreadFunc_t)CompressThread, &threaddata);
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