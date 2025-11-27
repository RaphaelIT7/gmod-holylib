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
	m_pModule = nullptr; // We should NEVER access it in the deconstructor since our module might already be unloaded!

	if ( m_pCVar )
	{
		if ( g_pCVar )
			g_pCVar->UnregisterConCommand(m_pCVar);

		delete m_pCVar; // Could this cause a crash? idk.
		m_pCVar = nullptr;
	}

	if ( m_pCVarName )
	{
		delete[] m_pCVarName;
		m_pCVarName = nullptr;
	}

	if ( m_pDebugCVar )
	{
		if ( g_pCVar )
			g_pCVar->UnregisterConCommand(m_pDebugCVar);

		delete m_pDebugCVar; // Could this cause a crash? idk either. But it didn't. Yet. Or has it.
		m_pDebugCVar = nullptr;
	}

	if ( m_pDebugCVarName )
	{
		delete[] m_pDebugCVarName;
		m_pDebugCVarName = nullptr;
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
	if (pData.EnsureChildVar<bool>("enabledByDefault", m_bEnabled) != m_bEnabled)
	{ // Module was disabled by default, and now is enabled. Happens when compatibility changes
		pData.GetChild("enabled").Var<bool>(m_bEnabled);
	} else {
		m_bEnabled = pData.EnsureChildVar<bool>("enabled", m_bEnabled);
	}

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
void CModule::SetModule(IModule* pModule)
{
	m_bStartup = true;
	m_pModule = pModule;
#ifdef SYSTEM_LINUX // Welcome to ifdef hell
#if ARCHITECTURE_IS_X86
	if (pModule->Compatibility() & LINUX32)
		m_bCompatible = true;
#else
	if (pModule->Compatibility() & LINUX64)
		m_bCompatible = true;
#endif
#else
#if ARCHITECTURE_IS_X86
	if (pModule->Compatibility() & WINDOWS32)
		m_bCompatible = true;
#else
	if (pModule->Compatibility() & WINDOWS64)
		m_bCompatible = true;
#endif
#endif

	m_bEnabled = m_pModule->IsEnabledByDefault() ? m_bCompatible : false;

	// Setup here so that command line arguments do not intentionally change the config.
	SetupConfig();

	std::string pStrName = "holylib_enable_";
	pStrName.append(pModule->Name());
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
	pDebugStrName.append(pModule->Name());

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
				{
					m_pModule->LuaInit(pLua, false);
					m_pModule->PostLuaInit(pLua, false);
				}
			}

			if (status & LoadStatus_LuaServerInit)
			{
				for (auto& pLua : g_pModuleManager.GetLuaInterfaces())
				{
					m_pModule->LuaInit(pLua, true);
					m_pModule->PostLuaInit(pLua, true);
				}
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

	if (CommandLine()->FindParm("-holylib_allowunsafe"))
	{
		Msg(PROJECT_NAME " - code: Found -holylib_allowunsafe enabling unsafe code...\n");
		m_bEnabledUnsafeCode = true;
	}

	if (CommandLine()->FindParm("-holylib_denyunsafe"))
	{
		Msg(PROJECT_NAME " - code: Found -holylib_denyunsafe disabling unsafe code...\n");
		m_bEnabledUnsafeCode = false;
	}

	if (CommandLine()->FindParm("-holylib_allowunsafe") && CommandLine()->FindParm("-holylib_denyunsafe"))
		Msg(PROJECT_NAME " - code: Both -holylib_allowunsafe and -holylib_denyunsafe were found -holylib_denyunsafe takes priority! (But... Why... Why both...)\n");

	if (!m_pConfig)
	{
		m_pConfig = g_pConfigSystem->LoadConfig(HOLYLIB_CONFIG_PATH "modules.json");
		if (m_pConfig->GetState() == ConfigState::INVALID_JSON)
		{
			Warning(PROJECT_NAME " - modulesystem: Failed to load modules.json!\n- Check if the json is valid or delete the config to let a new one be generated!\n");
			m_pConfig->Destroy(); // Our config is in a invaid state :/
			m_pConfig = nullptr;
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
		m_pConfig = nullptr;
	}
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

	return nullptr;
}

IModuleWrapper* CModuleManager::FindModuleByName(const char* name)
{
	for (CModule* module : m_pModules)
		if (V_stricmp(module->FastGetModule()->Name(), name) == 0)
			return module;

	return nullptr;
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
	if (pLua == g_Lua) // We only update our state based off the main lua state
	{
		if (bServerInit)
			m_pStatus |= LoadStatus_LuaServerInit;
		else
			m_pStatus |= LoadStatus_LuaInit;

		if (bServerInit)
		{
			for (GarrysMod::Lua::ILuaInterface* LUA : m_pLuaInterfaces)
			{
				if (LUA == pLua)
					continue;

				VCALL_LUA_ENABLED_MODULES(LuaInit(pLua, bServerInit));
				VCALL_LUA_ENABLED_MODULES(PostLuaInit(pLua, bServerInit));
			}
		}
	}

	/*if (!Lua::GetLuaData(pLua))
	{
		Warning("holylib: tried to Initialize a LuaInterface when it had no allocated StateData!\n");
		return;
	}*/

	AddLuaInterface(pLua);
	VCALL_LUA_ENABLED_MODULES(LuaInit(pLua, bServerInit));
	VCALL_LUA_ENABLED_MODULES(PostLuaInit(pLua, bServerInit));

	if (!bServerInit && pLua != g_Lua && (m_pStatus & LoadStatus_LuaServerInit))
	{ // LuaServerInit was already called, so we need to call it too
		VCALL_LUA_ENABLED_MODULES(LuaInit(pLua, true));
		VCALL_LUA_ENABLED_MODULES(PostLuaInit(pLua, true));
	}
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

void CModuleManager::OnClientConnect(CBaseClient* pClient)
{
	VCALL_ENABLED_MODULES(OnClientConnect(pClient));
}

void CModuleManager::OnClientDisconnect(CBaseClient* pClient)
{
	VCALL_ENABLED_MODULES(OnClientDisconnect(pClient));
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

void CModuleManager::ClientActive(edict_t* pClient)
{
	VCALL_ENABLED_MODULES(ClientActive(pClient));
}

void CModuleManager::ClientDisconnect(edict_t* pClient)
{
	VCALL_ENABLED_MODULES(ClientDisconnect(pClient));
}

void CModuleManager::ClientPutInServer(edict_t* pClient, const char* pPlayerName)
{
	VCALL_ENABLED_MODULES(ClientPutInServer(pClient, pPlayerName));
}

MODULE_RESULT CModuleManager::ClientConnect(bool* bAllowConnect, edict_t* pClient, const char* pszName, const char* pszAddress, char* reject, int maxrejectlen)
{
	MODULE_RESULT result = MODULE_RESULT::CONTINUE;
	BASE_CALL_ENABLED_MODULES(
		result = pModule->GetModule()->ClientConnect(bAllowConnect, pClient, pszName, pszAddress, reject, maxrejectlen);
		if (result != MODULE_RESULT::CONTINUE)
			break;
	, );

	return result;
}

MODULE_RESULT CModuleManager::ClientCommand(edict_t *pClient, const CCommand* args)
{
	MODULE_RESULT result = MODULE_RESULT::CONTINUE;
	BASE_CALL_ENABLED_MODULES(
		result = pModule->GetModule()->ClientCommand(pClient, args);
		if (result != MODULE_RESULT::CONTINUE)
			break;
	, );

	return result;
}

MODULE_RESULT CModuleManager::NetworkIDValidated(const char *pszUserName, const char *pszNetworkID)
{
	MODULE_RESULT result = MODULE_RESULT::CONTINUE;
	BASE_CALL_ENABLED_MODULES(
		result = pModule->GetModule()->NetworkIDValidated(pszUserName, pszNetworkID);
		if (result != MODULE_RESULT::CONTINUE)
			break;
	, );

	return result;
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