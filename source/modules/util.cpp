#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"
#include "Bootil/Bootil.h"

class CUtilModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn);
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour(bool bPreServer);
	virtual void Think(bool bSimulating);
	virtual void Shutdown();
	virtual const char* Name() { return "util"; };
};

CUtilModule g_pUtilModule;
IModule* pUtilModule = &g_pUtilModule;

struct CompressEntry
{
	int iCallback = -1;
	bool bCompress = true;

	const char* pData;
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
unsigned CompressThread(void* data)
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

			//cData->pMutex.Lock();
			cData->pFinished.push_back(entry);
			//cData->pMutex.Unlock();
		}

		if(cData->bInvalidEverything)
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
}

ThreadHandle_t threadhandle;
CompressData threaddata;
LUA_FUNCTION_STATIC(util_AsyncCompress)
{
	const char* pData = LUA->CheckString(1);
	int iLength = LUA->ObjLen(1);
	int iLevel = g_Lua->CheckNumberOpt(2, 5);
	int iDictSize = g_Lua->CheckNumberOpt(3, 65536);
	LUA->CheckType(4, GarrysMod::Lua::Type::Function);
	LUA->Push(4);
	int iCallback = LUA->ReferenceCreate();

	CompressEntry* entry = new CompressEntry;
	entry->iCallback = iCallback;
	entry->iDictSize = iDictSize;
	entry->iLength = iLength;
	entry->iLevel = iLevel;
	entry->pData = pData;

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
	entry->iLength = iLength;
	entry->pData = pData;

	threaddata.pQueue.push_back(entry);

	return 0;
}

void CUtilModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	threaddata.bRun = true;
	threadhandle = CreateSimpleThread(CompressThread, &threaddata);
}

void CUtilModule::LuaInit(bool bServerInit)
{
	if (!bServerInit)
	{
		g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
			g_Lua->GetField(-1, "util");
			if (g_Lua->IsType(-1, GarrysMod::Lua::Type::Table))
			{
				Util::AddFunc(util_AsyncCompress, "AsyncCompress");
				Util::AddFunc(util_AsyncDecompress, "AsyncDecompress");
			}
		g_Lua->Pop(2);
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
}

void CUtilModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }
}

void CUtilModule::Think(bool simulating)
{
	if (threaddata.bInvalidEverything) // Wait for the Thread to be ready again
		return;

	for(CompressEntry* entry : threaddata.pFinished)
	{
		g_Lua->ReferencePush(entry->iCallback);
			g_Lua->PushString((const char*)entry->buffer.GetBase());
		g_Lua->CallFunctionProtected(1, 0, true);
		delete entry;
	}
	threaddata.pFinished.clear();
}

void CUtilModule::Shutdown()
{
	threaddata.bRun = false;
}