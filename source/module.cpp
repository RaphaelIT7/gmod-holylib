#include "detours.h"
#include <tier2/tier2.h>
#include "modules/_modules.h"
#include "convar.h"
#include "tier0/icommandline.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// We cannot include util.h as it breaks stuff for some magical reason.
extern GarrysMod::Lua::ILuaInterface* g_Lua;

static ConVar module_debug("holylib_module_debug", "0");

CModule::~CModule()
{
	if ( m_pCVar )
	{
		if ( g_pCVar )
			g_pCVar->UnregisterConCommand(m_pCVar);

		delete m_pCVar; // Could this cause a crash? idk.
		m_pCVar = NULL;
	}

	if ( m_pCVarName )
	{
		delete[] m_pCVarName;
		m_pCVarName = NULL;
	}

	if ( m_pDebugCVar )
	{
		if ( g_pCVar )
			g_pCVar->UnregisterConCommand(m_pDebugCVar);

		delete m_pDebugCVar; // Could this cause a crash? idk either. But it didn't. Yet. Or has it.
		m_pDebugCVar = NULL;
	}

	if ( m_pDebugCVarName )
	{
		delete[] m_pDebugCVarName;
		m_pDebugCVarName = NULL;
	}
}

void OnModuleConVarChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	CModule* module = (CModule*)g_pModuleManager.FindModuleByConVar((ConVar*)convar);
	if (!module)
	{
		Warning(PROJECT_NAME ": Failed to find CModule for convar %s!\n", convar->GetName());
		return;
	}

	module->SetEnabled(((ConVar*)convar)->GetBool(), true);
}

static bool bIgnoreCallback = false;
void OnModuleDebugConVarChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	if (bIgnoreCallback)
		return;

	CModule* module = (CModule*)g_pModuleManager.FindModuleByConVar((ConVar*)convar);
	if (!module)
	{
		Warning(PROJECT_NAME ": Failed to find CModule for convar %s!\n", convar->GetName());
		return;
	}

	module->FastGetModule()->SetDebug(((ConVar*)convar)->GetInt());
}

bool CModule::IsEnabled()
{
	return m_bEnabled;
}

void CModule::SetupConfig()
{
	IConfig* pConfig = g_pModuleManager.GetConfig();
	if (!pConfig)
		return;

	Bootil::Data::Tree& pData = pConfig->GetData().GetChild(m_pModule->Name());
	m_bEnabled = pData.EnsureChildVar<bool>("enabled", m_bEnabled);
	m_pModule->SetDebug(pData.EnsureChildVar<int>("debugLevel", m_pModule->InDebug()));

	m_pModule->OnConfigLoad(pData);
}

/*
	Module activation priority from low to high.
	Based on this priority modules are enabled/disabled.

	1 - IModule::IsEnabledByDefault - The module's default state
	2 - CModule::SetupConfig - The module config
	3 - Command line - The module's command line argument
	4 - holylib_startdisabled - If all modules should start disabled.
*/
void CModule::SetModule(IModule* module)
{
	m_bStartup = true;
	m_pModule = module;
#ifdef SYSTEM_LINUX // Welcome to ifdef hell
#if ARCHITECTURE_IS_X86
	if (module->Compatibility() & LINUX32)
		m_bCompatible = true;
#else
	if (module->Compatibility() & LINUX64)
		m_bCompatible = true;
#endif
#else
#if ARCHITECTURE_IS_X86
	if (module->Compatibility() & WINDOWS32)
		m_bCompatible = true;
#else
	if (module->Compatibility() & WINDOWS64)
		m_bCompatible = true;
#endif
#endif

	m_bEnabled = m_pModule->IsEnabledByDefault() ? m_bCompatible : false;

	SetupConfig();

	std::string pStrName = "holylib_enable_";
	pStrName.append(module->Name());
	std::string cmdStr = "-";
	cmdStr.append(pStrName);
	int cmd = CommandLine()->ParmValue(cmdStr.c_str(), -1); // checks for "-holylib_enable_[module name] [1 / 0]"
	if (cmd > -1) {
		SetEnabled(cmd == 1, true);
	} else {
		if (CommandLine()->FindParm("-holylib_startdisabled"))
		{
			m_bEnabled = false;
		}
	}

	m_pCVarName = new char[48];
	V_strncpy(m_pCVarName, pStrName.c_str(), 48);
	m_pCVar = new ConVar(m_pCVarName, m_bEnabled ? "1" : "0", FCVAR_ARCHIVE, "Whether this module should be active or not", OnModuleConVarChange);

	std::string pDebugStrName = "holylib_debug_";
	pDebugStrName.append(module->Name());

	int cmdDebug = CommandLine()->ParmValue(((std::string)"-" + pDebugStrName).c_str(), -1);
	if (cmdDebug > -1)
		m_pModule->SetDebug(cmdDebug);

	m_strDebugValue = new char[4];
	V_strncpy(m_strDebugValue, std::to_string(m_pModule->InDebug()).c_str(), 4); // I dislike this :/

	m_pDebugCVarName = new char[48];
	V_strncpy(m_pDebugCVarName, pDebugStrName.c_str(), 48);
	m_pDebugCVar = new ConVar(m_pDebugCVarName, m_strDebugValue, FCVAR_ARCHIVE, "Whether this module will show debug stuff", OnModuleDebugConVarChange);

	m_bStartup = false;
}

void CModule::SetEnabled(bool bEnabled, bool bForced)
{
	if (m_bEnabled != bEnabled)
	{
		if (bEnabled && !m_bEnabled)
		{
			if (!m_bCompatible)
			{
				Warning(PROJECT_NAME ": module %s is not compatible with this platform!\n", m_pModule->Name());

				if (!bForced)
					return;
			}

			int status = g_pModuleManager.GetStatus();
			if (status & LoadStatus_PreDetourInit)
				m_pModule->InitDetour(true); // I want every module to be able to be disabled/enabled properly

			if (status & LoadStatus_Init)
				m_pModule->Init(&g_pModuleManager.GetAppFactory(), &g_pModuleManager.GetGameFactory());

			if (status & LoadStatus_DetourInit)
				m_pModule->InitDetour(false);

			if (status & LoadStatus_LuaInit)
			{
				for (auto& pLua : g_pModuleManager.GetLuaInterfaces())
					m_pModule->LuaInit(pLua, false);
			}

			if (status & LoadStatus_LuaServerInit)
			{
				for (auto& pLua : g_pModuleManager.GetLuaInterfaces())
					m_pModule->LuaInit(pLua, true);
			}

			if (status & LoadStatus_ServerActivate)
				m_pModule->ServerActivate(g_pModuleManager.GetEdictList(), g_pModuleManager.GetEdictCount(), g_pModuleManager.GetClientMax());

			if (!m_bStartup)
				Msg(PROJECT_NAME ": Enabled module %s\n", m_pModule->Name());
		} else {
			int status = g_pModuleManager.GetStatus();
			if (status & LoadStatus_LuaInit)
			{
				for (auto& pLua : g_pModuleManager.GetLuaInterfaces())
					m_pModule->LuaShutdown(pLua);
			}

			if (status & LoadStatus_Init)
				Shutdown();

			if (!m_bStartup)
				Msg(PROJECT_NAME ": Disabled module %s\n", m_pModule->Name());
		}
	}

	m_bEnabled = bEnabled;
}

void CModule::Shutdown()
{
	Detour::Remove(m_pModule->m_pID);
	m_pModule->Shutdown();
}

/*
	Initially I wanted to do it like the IGameSystem but I think the current approach is better.
*/
CModuleManager::CModuleManager()
{
	/*
	BUG: Calling SetValue causes a instant crash! GG :3
	if (CommandLine()->FindParm("-holylib_module_debug") > -1)
	{
		module_debug.SetValue("1");
	}*/

	if (!m_pConfig)
	{
		m_pConfig = g_pConfigSystem->LoadConfig("garrysmod/holylib/cfg/modules.json");
		if (m_pConfig->GetState() == ConfigState::INVALID_JSON)
		{
			Warning(PROJECT_NAME " - modulesystem: Failed to load modules.json!\n- Check if the json is valid or delete the config to let a new one be generated!\n");
			m_pConfig->Destroy(); // Our config is in a invaid state :/
			m_pConfig = NULL;
		}
	}

#ifndef LIB_HOLYLIB
	LoadModules();
#endif

	if (m_pConfig)
	{
		m_pConfig->Save();
	}
}

CModuleManager::~CModuleManager()
{
	for (CModule* pModule : m_pModules)
		delete pModule;

	m_pModules.clear();

	if (m_pConfig)
	{
		//m_pConfig->Save(); // We don't save because we else would possibly override any changes made while the server was running, which we don't want.
		m_pConfig->Destroy();
		m_pConfig = NULL;
	}
}

void CModuleManager::LoadModules()
{
	RegisterModule(pHolyLibModule);
	RegisterModule(pGameeventLibModule);
	RegisterModule(pServerPluginLibModule);
	RegisterModule(pSourceTVLibModule);
	RegisterModule(pThreadPoolFixModule);
	RegisterModule(pStringTableModule);
	RegisterModule(pPrecacheFixModule);
	RegisterModule(pPVSModule);
	RegisterModule(pSurfFixModule);
	RegisterModule(pFileSystemModule);
	RegisterModule(pUtilModule);
	RegisterModule(pConCommandModule);
	RegisterModule(pVProfModule);
	RegisterModule(pCVarsModule);
	RegisterModule(pBitBufModule);
	RegisterModule(pNetworkingModule);
	RegisterModule(pSteamWorksModule);
	RegisterModule(pPASModule);
	RegisterModule(pBassModule);
	RegisterModule(pSysTimerModule);
	RegisterModule(pVoiceChatModule);
	RegisterModule(pPhysEnvModule);
	RegisterModule(pNetModule);
	RegisterModule(pEntListModule);
	RegisterModule(pHttpServerModule);
	RegisterModule(pLuaJITModule);
	RegisterModule(pGameServerModule);
	RegisterModule(pSoundscapeModule);
	RegisterModule(pLuaThreadsModule);
	RegisterModule(pNW2DebuggingModule);
}

int g_pIDs = 0;
IModuleWrapper* CModuleManager::RegisterModule(IModule* pModule)
{
	++g_pIDs;
	CModule* module = new CModule;
	m_pModules.push_back(module); // Add it first in case any ConVar callbacks get called in SetModule.
	module->SetModule(pModule);
	module->SetID(g_pIDs);
	Msg(PROJECT_NAME ": Registered module %-*s (%-*i Enabled: %s Compatible: %s MultiLua: %s)\n", 
		15,
		module->FastGetModule()->Name(), 
		2,
		g_pIDs,
		module->IsEnabled() ? "true, " : "false,", 
		module->FastIsCompatible() ? "true " : "false",
		module->GetModule()->SupportsMultipleLuaStates() ? "true " : "false"
	);

	return module;
}

IModuleWrapper* CModuleManager::FindModuleByConVar(ConVar* convar)
{
	for (CModule* module : m_pModules)
	{
		if (convar == module->GetConVar())
			return module;

		if (convar == module->GetDebugConVar())
			return module;
	}

	return NULL;
}

IModuleWrapper* CModuleManager::FindModuleByName(const char* name)
{
	for (CModule* module : m_pModules)
		if (V_stricmp(module->FastGetModule()->Name(), name) == 0)
			return module;

	return NULL;
}

void CModuleManager::Setup(CreateInterfaceFn appfn, CreateInterfaceFn gamefn)
{
	m_pAppFactory = appfn;
	m_pGameFactory = gamefn;
}

#define BASE_CALL_ENABLED_MODULES(call, additionalChecks) \
	for (CModule* pModule : m_pModules) { \
		if ( !pModule->FastIsEnabled() ) { continue; } \
		additionalChecks; \
		if ( module_debug.GetBool() ) { Msg(PROJECT_NAME ": Calling(V) %s on %s\n", #call, pModule->GetModule()->Name()); } \
		call; \
		if ( module_debug.GetBool() ) { Msg(PROJECT_NAME ": Finished calling(V) %s on %s\n", #call, pModule->GetModule()->Name()); } \
	}

#define VCALL_ENABLED_MODULES(call) \
	BASE_CALL_ENABLED_MODULES(pModule->GetModule()-> call, )

#define VCALL_LUA_ENABLED_MODULES(call) \
	BASE_CALL_ENABLED_MODULES(pModule->GetModule()-> call, \
	if ( g_Lua != pLua && !pModule->GetModule()->SupportsMultipleLuaStates() ) { continue; })

#define CALL_ENABLED_MODULES(call) \
	BASE_CALL_ENABLED_MODULES(pModule-> call, )


void CModuleManager::Init()
{
	if (!(m_pStatus & LoadStatus_PreDetourInit))
	{
		DevMsg(PROJECT_NAME ": ghostinj didn't call InitDetour! Calling it now\n");
		InitDetour(true);
	}

	m_pStatus |= LoadStatus_Init;
	VCALL_ENABLED_MODULES(Init(& GetAppFactory(), &GetGameFactory()));
}

void CModuleManager::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		m_pStatus |= LoadStatus_LuaServerInit;
	else
		m_pStatus |= LoadStatus_LuaInit;

	/*if (!Lua::GetLuaData(pLua))
	{
		Warning("holylib: tried to Initialize a LuaInterface when it had no allocated StateData!\n");
		return;
	}*/

	AddLuaInterface(pLua);
	VCALL_LUA_ENABLED_MODULES(LuaInit(pLua, bServerInit));
}

void CModuleManager::LuaThink(GarrysMod::Lua::ILuaInterface* pLua)
{
	VCALL_LUA_ENABLED_MODULES(LuaThink(pLua));
}

void CModuleManager::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	VCALL_LUA_ENABLED_MODULES(LuaShutdown(pLua));
	RemoveLuaInterface(pLua);
}

void CModuleManager::InitDetour(bool bPreServer)
{
	if (bPreServer)
		m_pStatus |= LoadStatus_PreDetourInit;
	else
		m_pStatus |= LoadStatus_DetourInit;

	VCALL_ENABLED_MODULES(InitDetour(bPreServer));
}

void CModuleManager::Think(bool bSimulating)
{
	VCALL_ENABLED_MODULES(Think(bSimulating));
}

void CModuleManager::Shutdown()
{
	m_pStatus = 0;
	CALL_ENABLED_MODULES(Shutdown());
}

void CModuleManager::ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
	m_pEdictList = pEdictList;
	m_iEdictCount = edictCount;
	m_iClientMax = clientMax;

	m_pStatus |= LoadStatus_ServerActivate;
	VCALL_ENABLED_MODULES(ServerActivate(pEdictList, edictCount, clientMax));
}

void CModuleManager::OnEdictAllocated(edict_t* pEdict)
{
	VCALL_ENABLED_MODULES(OnEdictAllocated(pEdict));
}

void CModuleManager::OnEdictFreed(const edict_t* pEdict)
{
	VCALL_ENABLED_MODULES(OnEdictFreed(pEdict));
}

void CModuleManager::OnEntityCreated(CBaseEntity* pEntity)
{
	VCALL_ENABLED_MODULES(OnEntityCreated(pEntity));
}

void CModuleManager::OnEntitySpawned(CBaseEntity* pEntity)
{
	VCALL_ENABLED_MODULES(OnEntitySpawned(pEntity));
}

void CModuleManager::OnEntityDeleted(CBaseEntity* pEntity)
{
	VCALL_ENABLED_MODULES(OnEntityDeleted(pEntity));
}

void CModuleManager::LevelShutdown()
{
	VCALL_ENABLED_MODULES(LevelShutdown());
}

void CModuleManager::PreLuaModuleLoaded(lua_State* L, const char* pFileName)
{
	VCALL_ENABLED_MODULES(PreLuaModuleLoaded(L, pFileName));
}

void CModuleManager::PostLuaModuleLoaded(lua_State* L, const char* pFileName)
{
	VCALL_ENABLED_MODULES(PostLuaModuleLoaded(L, pFileName));
}

CModuleManager g_pModuleManager;

static void NukeModules(const CCommand &args)
{
	for (IModuleWrapper* module : g_pModuleManager.GetModules())
		module->SetEnabled(false, true);
}
static ConCommand nukemodules("holylib_nukemodules", NukeModules, "Debug command. Disables all modules.", 0);

static void ModuleStatus(const CCommand &args)
{
	Msg("------- Modules -------\n");

	for (CModule* module : g_pModuleManager.GetModules())
	{
		Msg(PROJECT_NAME ": Registered module %-*s (%-*i Enabled: %s Compatible: %s MultiLua: %s)\n", 
			15,
			module->FastGetModule()->Name(), 
			2,
			g_pIDs,
			module->IsEnabled() ? "true, " : "false,", 
			module->FastIsCompatible() ? "true " : "false",
			module->GetModule()->SupportsMultipleLuaStates() ? "true " : "false"
		);
	}

	Msg("------- Lua Interfaces -------\n");
	Msg("Count: %i\n", (int)g_pModuleManager.GetLuaInterfaces().size());

	for (GarrysMod::Lua::ILuaInterface* interface : g_pModuleManager.GetLuaInterfaces())
		Msg("\"%p\"", interface);
}
static ConCommand modulestatus("holylib_modulestatus", ModuleStatus, "Debug command. Prints out the status of all modules.", 0);

static void SaveModuleConfig(const CCommand &args)
{
	IConfig* pConfig = g_pModuleManager.GetConfig();
	if (!pConfig)
		return;

	for (auto pModule : g_pModuleManager.GetModules())
	{
		pConfig->GetData().SetChildVar<bool>(pModule->FastGetModule()->Name(), pModule->FastIsEnabled());
	}

	pConfig->Save();
}
static ConCommand savemoduleconfig("holylib_savemoduleconfig", SaveModuleConfig, "Saves the module config by storing all currently enabled/disabled modules", 0);