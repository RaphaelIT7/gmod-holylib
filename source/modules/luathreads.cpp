#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "player.h"
#include "unordered_set"
#include "lua/CLuaInterface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CLuaThreadsModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual const char* Name() { return "luathreads"; };
	virtual int Compatibility() { return LINUX32; };
	virtual bool SupportsMultipleLuaStates() { return true; };
};

CLuaThreadsModule g_pLuaThreadsModule;
IModule* pLuaThreadsModule = &g_pLuaThreadsModule;

enum InterfaceStatus
{
	INTERFACE_STOPPED = 0, // Set by the thread if it succesfully stopped
	INTERFACE_RUNNING = 1,
	INTERFACE_STOPPING = 2, // This should be set if you request it to stop.
};

struct LuaInterface;
class InterfaceTask
{
public:
	virtual ~InterfaceTask() = default;
	virtual void DoTask(LuaInterface* pData) = 0;
};

struct LuaInterface
{
	~LuaInterface()
	{
		if (iStatus != INTERFACE_STOPPED)
		{
			iStatus = INTERFACE_STOPPING;
			while (iStatus != INTERFACE_STOPPED)
			{
				ThreadSleep(0);
			}
		}

		if (pTasks.size() > 0)
		{
			for (auto& task : pTasks)
				delete task;

			pTasks.clear();
		}

		g_pModuleManager.LuaShutdown(pInterface);
		Lua::DestroyInterface(pInterface);
	}

	CLuaInterface* pInterface;
	ThreadHandle_t pThreadID;
	InterfaceStatus iStatus = InterfaceStatus::INTERFACE_STOPPED;
	unsigned int iSleepTime = 1; // Time in ms to sleep
	std::vector<InterfaceTask*> pTasks;
	CThreadFastMutex pMutex;
};

class RunStringTask : public InterfaceTask
{
public:
	virtual void DoTask(LuaInterface* pData)
	{
		pData->pInterface->RunString("RunString", "", strCode.c_str(), true, true);
	}

public:
	std::string strCode;
};

PushReferenced_LuaClass(LuaInterface)
Get_LuaClass(LuaInterface, "LuaInterface")

LUA_FUNCTION_STATIC(LuaInterface__tostring)
{
	LuaInterface* pData = Get_LuaInterface(LUA, 1, false);
	if (!pData)
	{
		LUA->PushString("LuaInterface [NULL]");
		return 1;
	}

	char szBuf[64] = {};
	V_snprintf(szBuf, sizeof(szBuf), "LuaInterface [%s]", pData->pInterface->GetPath());
	LUA->PushString(szBuf);
	return 1;
}

Default__index(LuaInterface);
Default__newindex(LuaInterface);
Default__GetTable(LuaInterface);

// ToDo: Make a Default__IsValid macro
LUA_FUNCTION_STATIC(LuaInterface_IsValid)
{
	LUA->PushBool(Get_LuaInterface(LUA, 1, false) != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(LuaInterface_RunString)
{
	LuaInterface* pData = Get_LuaInterface(LUA, 1, true);
	RunStringTask* pTask = new RunStringTask;
	pTask->strCode = LUA->CheckString(2);

	pData->pMutex.Lock();
	pData->pTasks.push_back(pTask);
	pData->pMutex.Unlock();

	return 0;
}

#if ARCHITECTURE_IS_X86_64
static long long unsigned
#else
static unsigned
#endif
LuaThread(void* data)
{
	LuaInterface* pData = (LuaInterface*)data;
	pData->iStatus = INTERFACE_RUNNING;
	while (pData->iStatus == INTERFACE_RUNNING)
	{
		// Execute all tasks first
		pData->pMutex.Lock();
		for (auto& task : pData->pTasks)
		{
			task->DoTask(pData);
			delete task;
		}
		pData->pTasks.clear();
		pData->pMutex.Unlock();

		// Execute any module's think code
		g_pModuleManager.LuaThink(pData->pInterface);

		// eep
		ThreadSleep(pData->iSleepTime);
	}
	pData->iStatus = INTERFACE_STOPPED;
	
	return 0;
}

LUA_FUNCTION_STATIC(luathreads_CreateInterface)
{
	LuaInterface* pData = new LuaInterface;
	pData->pInterface = (CLuaInterface*)Lua::CreateLuaInterface(true);
	pData->pThreadID = CreateSimpleThread((ThreadFunc_t)LuaThread, pData);

	Push_LuaInterface(LUA, pData);
	return 1;
}

void CLuaThreadsModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::LuaInterface, pLua->CreateMetaTable("LuaInterface"));
		Util::AddFunc(pLua, LuaInterface__tostring, "__tostring");
		Util::AddFunc(pLua, LuaInterface__index, "__index");
		Util::AddFunc(pLua, LuaInterface__newindex, "__newindex");
		Util::AddFunc(pLua, LuaInterface_GetTable, "GetTable");
		Util::AddFunc(pLua, LuaInterface_IsValid, "IsValid");
		Util::AddFunc(pLua, LuaInterface_RunString, "RunString");
	pLua->Pop(1);

	Util::StartTable(pLua);
		Util::AddFunc(pLua, luathreads_CreateInterface, "CreateInterface");
	Util::FinishTable(pLua, "luathreads");
}

void CLuaThreadsModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "luathreads");

	DeleteAll_LuaInterface(pLua); // Memory leak! ToDo: Clean things up properly.
}