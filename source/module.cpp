#include "detours.h"
#include <tier2/tier2.h>
#include "modules/_modules.h"
#include "convar.h"
#include "tier0/icommandline.h"

CModule::~CModule()
{
	if ( m_pCVar )
	{
		if ( g_pCVar )
			g_pCVar->UnregisterConCommand(m_pCVar);

		delete m_pCVar; // Could this cause a crash? idk.
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
		Warning("holylib: Failed to find CModule for convar %s!\n", convar->GetName());
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
		Warning("holylib: Failed to find CModule for convar %s!\n", convar->GetName());
		return;
	}

	module->GetModule()->SetDebug(((ConVar*)convar)->GetInt());
}

bool CModule::IsEnabled()
{
	return m_bEnabled;
}

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

	std::string pStrName = "holylib_enable_";
	pStrName.append(module->Name());
	std::string cmdStr = "-";
	cmdStr.append(pStrName);
	int cmd = CommandLine()->ParmValue(cmdStr.c_str(), -1); // checks for "-holylib_enable_[module name] [1 / 0]"
	if (cmd > -1)
		SetEnabled(cmd == 1, true);
	else
		if (!CommandLine()->FindParm("-holylib_startdisabled"))
			m_bEnabled = m_pModule->IsEnabledByDefault() ? m_bCompatible : false;

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
				Warning("holylib: module %s is not compatible with this platform!\n", m_pModule->Name());

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
				m_pModule->LuaInit(false);

			if (status & LoadStatus_LuaServerInit)
				m_pModule->LuaInit(true);

			if (status & LoadStatus_ServerActivate)
				m_pModule->ServerActivate(g_pModuleManager.GetEdictList(), g_pModuleManager.GetEdictCount(), g_pModuleManager.GetClientMax());

			if (!m_bStartup)
				Msg("holylib: Enabled module %s\n", m_pModule->Name());
		} else {
			int status = g_pModuleManager.GetStatus();
			if (status & LoadStatus_LuaInit)
				m_pModule->LuaShutdown();

			if (status & LoadStatus_Init)
				Shutdown();

			if (!m_bStartup)
				Msg("holylib: Disabled module %s\n", m_pModule->Name());
		}
	}

	m_bEnabled = bEnabled;
}

void CModule::Shutdown()
{
	Detour::Remove(m_pModule->m_pID);
	m_pModule->Shutdown();
}

CModuleManager::CModuleManager() // ToDo: Look into how IGameSystem works and use something similar. I don't like to add each one manually
{
#ifndef LIB_HOLYLIB
	LoadModules();
#endif
}

CModuleManager::~CModuleManager()
{
	for (CModule* pModule : m_pModules)
		delete pModule;

	m_pModules.clear();
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
}

int g_pIDs = 0;
IModuleWrapper* CModuleManager::RegisterModule(IModule* pModule)
{
	++g_pIDs;
	CModule* module = new CModule;
	m_pModules.push_back(module); // Add it first in case any ConVar callbacks get called in SetModule.
	module->SetModule(pModule);
	module->SetID(g_pIDs);
	Msg("holylib: Registered module %-*s (%-*i Enabled: %s Compatible: %s)\n", 
		15,
		module->GetModule()->Name(), 
		2,
		g_pIDs,
		module->IsEnabled() ? "true, " : "false,", 
		module->IsCompatible() ? "true " : "false"
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
		if (V_stricmp(module->GetModule()->Name(), name) == 0)
			return module;

	return NULL;
}

void CModuleManager::Setup(CreateInterfaceFn appfn, CreateInterfaceFn gamefn)
{
	m_pAppFactory = appfn;
	m_pGameFactory = gamefn;
}

#define VCALL_ENABLED_MODULES(call) \
	for (CModule* pModule : m_pModules) { \
		if ( !pModule->FastIsEnabled() ) { continue; } \
		pModule->GetModule()-> call; \
	}

#define CALL_ENABLED_MODULES(call) \
	for (CModule* pModule : m_pModules) { \
		if ( !pModule->FastIsEnabled() ) { continue; } \
		pModule-> call; \
	}


void CModuleManager::Init()
{
	if (!(m_pStatus & LoadStatus_PreDetourInit))
	{
		DevMsg("holylib: ghostinj didn't call InitDetour! Calling it now\n");
		InitDetour(true);
	}

	m_pStatus |= LoadStatus_Init;
	VCALL_ENABLED_MODULES(Init(& GetAppFactory(), &GetGameFactory()));
}

void CModuleManager::LuaInit(bool bServerInit)
{
	if (bServerInit)
		m_pStatus |= LoadStatus_LuaServerInit;
	else
		m_pStatus |= LoadStatus_LuaInit;

	VCALL_ENABLED_MODULES(LuaInit(bServerInit));
}

void CModuleManager::LuaShutdown()
{
	VCALL_ENABLED_MODULES(LuaShutdown());
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

CModuleManager g_pModuleManager;

static void NukeModules(const CCommand &args)
{
	for (IModuleWrapper* module : g_pModuleManager.GetModules())
		module->SetEnabled(false, true);
}
static ConCommand nukemodules("holylib_nukemodules", NukeModules, "Debug command. Disables all modules.", 0);