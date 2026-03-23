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
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void Shutdown() override;
	const char* Name() override { return "luathreads"; };
	int Compatibility() override { return LINUX32; };
	bool SupportsMultipleLuaStates() override { return true; };
};

static IThreadPool* pLuaThreadPool = nullptr;
static void OnLuaThreadsChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	if (!pLuaThreadPool)
		return;

	pLuaThreadPool->ExecuteAll();
	pLuaThreadPool->Stop();
	Util::StartThreadPool(pLuaThreadPool, ((ConVar*)convar)->GetInt());
}

static ConVar luathreads("holylib_luathreads_threadpool", "4", FCVAR_ARCHIVE, "The number of threads for the threadpool", true, 1, true, 16, OnLuaThreadsChange);


static CLuaThreadsModule g_pLuaThreadsModule;
IModule* pLuaThreadsModule = &g_pLuaThreadsModule;

enum class InterfaceStatus
{
	INTERFACE_STOPPED = 0, // Set by the thread if it successfully stopped
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

class LuaInterface;
static std::unordered_set<LuaInterface*> g_pLuaInterfaces;
class LuaInterface
{
public:
	LuaInterface()
	{
		g_pLuaInterfaces.insert(this);
	}

	~LuaInterface()
	{
		DestroyThread();

		auto it = g_pLuaInterfaces.find(this);
		if (it != g_pLuaInterfaces.end())
			g_pLuaInterfaces.erase(it);
	}

	void EnsureThread();

	void DestroyThread()
	{
		if (m_iStatus != InterfaceStatus::INTERFACE_STOPPED)
		{
			m_iStatus = InterfaceStatus::INTERFACE_STOPPING;
			while (m_iStatus != InterfaceStatus::INTERFACE_STOPPED)
			{
				ThreadSleep(0);
			}
		}

		if (m_pThreadID != nullptr)
		{
			ReleaseThreadHandle(m_pThreadID);
			m_pThreadID = nullptr;
		}

		if (m_pTasks.size() > 0)
		{
			for (auto& task : m_pTasks)
				delete task;

			m_pTasks.clear();
		}

		g_pModuleManager.LuaShutdown(m_pInterface);
		Lua::DestroyInterface(m_pInterface);
	}

	void AddTask(InterfaceTask* pTask)
	{
		m_pMutex.Lock();
		m_pTasks.push_back(pTask);
		m_pMutex.Unlock();
	}

	inline void SetName(const char* pName)
	{
		V_strncpy(m_strName, pName, sizeof(m_strName));
	}

	inline const char* GetName()
	{
		return m_strName;
	}

	inline CLuaInterface* GetInterface()
	{
		return m_pInterface;
	}

	void CreateInterface()
	{
		m_pInterface = (CLuaInterface*)Lua::CreateInterface();
	}

	inline bool HasThread()
	{
		return m_pThreadID != nullptr;
	}

	void RunTasks()
	{
		// Execute all tasks first
		m_pMutex.Lock();
		std::vector<InterfaceTask*> pTasks = m_pTasks; // Copy in case a task results in a new task being added
		m_pTasks.clear();
		m_pMutex.Unlock();

		Lua::ScopedThreadAccess pThreadScope;
		Lua::ThreadAccess pScope(m_pInterface);
		if (pScope.IsValid())
		{
			for (auto& task : pTasks)
			{
				task->DoTask(this);
				delete task;
			}

			// Execute any module's think code
			g_pModuleManager.LuaThink(m_pInterface);
			m_pInterface->Cycle();
		}
	}

private:
	static SIMPLETHREAD_RETURNVALUE LuaInterfaceThread(void* data)
	{
		// ThreadSetDebugName(ThreadGetCurrentId(), PROJECT_NAME " - LuaInterfaceThread");
		LuaInterface* pData = (LuaInterface*)data;
		pData->m_iStatus = InterfaceStatus::INTERFACE_RUNNING;
		while (pData->m_iStatus == InterfaceStatus::INTERFACE_RUNNING)
		{
			pData->RunTasks();

			// eep
			ThreadSleep(pData->m_iSleepTime);
		}
		pData->m_iStatus = InterfaceStatus::INTERFACE_STOPPED;
	
		return 0;
	}

	CLuaInterface* m_pInterface;
	ThreadHandle_t m_pThreadID = nullptr;
	InterfaceStatus m_iStatus = InterfaceStatus::INTERFACE_STOPPED;
	unsigned int m_iSleepTime = 1; // Time in ms to sleep
	std::vector<InterfaceTask*> m_pTasks;
	CThreadFastMutex m_pMutex;
	char m_strName[MAX_PATH] = "NONAME";
};

class RunStringTask : public InterfaceTask
{
public:
	~RunStringTask() = default;
	void DoTask(LuaInterface* pData) override
	{
		pData->GetInterface()->RunString("RunString", "", strCode.c_str(), true, true);
	}

public:
	std::string strCode;
};

void LuaInterface::EnsureThread()
{
	if (HasThread())
		return;

	m_pThreadID = CreateSimpleThread((ThreadFunc_t)LuaInterfaceThread, this);
}

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
	V_snprintf(szBuf, sizeof(szBuf), "LuaInterface [%s]", pData->GetName());
	LUA->PushString(szBuf);
	return 1;
}

Default__index(LuaInterface);
Default__newindex(LuaInterface);
Default__GetTable(LuaInterface);

// ToDo: Make a Default__IsValid macro
LUA_FUNCTION_STATIC(LuaInterface_IsValid)
{
	LUA->PushBool(Get_LuaInterface(LUA, 1, false) != nullptr);
	return 1;
}

LUA_FUNCTION_STATIC(LuaInterface_RunString)
{
	LuaInterface* pData = Get_LuaInterface(LUA, 1, true);
	RunStringTask* pTask = new RunStringTask;
	pTask->strCode = LUA->CheckString(2);

	pData->AddTask(pTask);

	return 0;
}

LUA_FUNCTION_STATIC(LuaInterface_GetName)
{
	LuaInterface* pData = Get_LuaInterface(LUA, 1, true);

	LUA->PushString(pData->GetName());
	return 1;
}

LUA_FUNCTION_STATIC(LuaInterface_SetName)
{
	LuaInterface* pData = Get_LuaInterface(LUA, 1, true);

	pData->SetName(LUA->CheckString(2));
	return 0;
}

LUA_FUNCTION_STATIC(LuaInterface_EnableThinking)
{
	LuaInterface* pData = Get_LuaInterface(LUA, 1, true);
	bool bEnable = LUA->GetBool(2);

	if (bEnable && pData->HasThread())
		return 0;

	if (!bEnable && !pData->HasThread())
		return 0;

	if (bEnable)
	{
		pData->EnsureThread();
	} else {
		pData->DestroyThread();
	}

	return 0;
}

LUA_FUNCTION_STATIC(LuaInterface_CanThink)
{
	LuaInterface* pData = Get_LuaInterface(LUA, 1, true);

	LUA->PushBool(pData->HasThread());
	return 1;
}

inline void StartLuaThreadPool()
{
	if (pLuaThreadPool)
		return;

	pLuaThreadPool = V_CreateThreadPool();
	Util::StartThreadPool(pLuaThreadPool, luathreads.GetInt());
}

static void RunTasksJob(LuaInterface*& entry)
{
	entry->RunTasks();
}

LUA_FUNCTION_STATIC(LuaInterface_RunTasks)
{
	LuaInterface* pData = Get_LuaInterface(LUA, 1, true);
	if (pData->HasThread())
		return 0;

	StartLuaThreadPool();
	pLuaThreadPool->QueueCall(RunTasksJob, pData);
	return 0;
}

LUA_FUNCTION_STATIC(luathreads_CreateInterface)
{
	LuaInterface* pData = new LuaInterface;
	pData->CreateInterface();

	g_pModuleManager.LuaInit(pData->GetInterface(), false);

	Push_LuaInterface(LUA, pData);
	return 1;
}

LUA_FUNCTION_STATIC(luathreads_FindInterface)
{
	const char* pName = LUA->CheckString(1);
	for (LuaInterface* pInterface : g_pLuaInterfaces)
	{
		if (V_stricmp(pName, pInterface->GetName()) != 0)

		Push_LuaInterface(LUA, pInterface);
		return 1;
	}

	LUA->PushNil();
	return 1;
}

LUA_FUNCTION_STATIC(luathreads_GetInterfaces)
{
	LUA->PreCreateTable(g_pLuaInterfaces.size(), 0);
	int idx = 0;
	for (LuaInterface* pInterface : g_pLuaInterfaces)
	{
		Push_LuaInterface(LUA, pInterface);
		Util::RawSetI(LUA, -2, ++idx);
	}

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
		Util::AddFunc(pLua, LuaInterface_GetName, "GetName");
		Util::AddFunc(pLua, LuaInterface_SetName, "SetName");
		Util::AddFunc(pLua, LuaInterface_EnableThinking, "EnableThinking");
		Util::AddFunc(pLua, LuaInterface_CanThink, "CanThink");
		Util::AddFunc(pLua, LuaInterface_RunTasks, "RunTasks");
	pLua->Pop(1);

	Util::StartTable(pLua);
		Util::AddFunc(pLua, luathreads_CreateInterface, "CreateInterface");
		Util::AddFunc(pLua, luathreads_FindInterface, "FindInterface");
		Util::AddFunc(pLua, luathreads_GetInterfaces, "GetInterfaces");
	Util::FinishTable(pLua, "luathreads");
}

void CLuaThreadsModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "luathreads");

	DeleteAll_LuaInterface(pLua); // Memory leak! ToDo: Clean things up properly.
}

void CLuaThreadsModule::Shutdown()
{
	if (pLuaThreadPool)
	{
		Util::DestroyThreadPool(pLuaThreadPool);
		pLuaThreadPool = nullptr;
	}

	for (LuaInterface* pInterface : g_pLuaInterfaces)
		delete pInterface;

	g_pLuaInterfaces.clear();
}