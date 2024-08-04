#include "detours.h"
#include <tier2/tier2.h>
#include "modules/_modules.h"
#include "convar.h"
#include "tier0/icommandline.h"

CModule::~CModule()
{
	if ( m_pCVar )
		delete m_pCVar; // Could this cause a crash? idk.
}

void OnModuleConVarChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	CModule* module = g_pModuleManager.FindModuleByConVar((ConVar*)convar);
	if (!module)
	{
		Warning("Failed to find CModule for convar %s!\n", convar->GetName());
		return;
	}

	module->SetEnabled(((ConVar*)convar)->GetBool(), true);
}

void CModule::SetModule(IModule* module)
{
	m_bStartup = true;
	m_pModule = module;
#ifdef SYSTEM_LINUX // Welcome to ifdef hell
#ifdef ARCHITECTURE_X86
	if (module->Compatibility() & LINUX32)
		m_bCompatible = true;
#else
	if (module->Compatibility() & LINUX64)
		m_bCompatible = true;
#endif
#else
#ifdef ARCHITECTURE_X86
	if (module->Compatibility() & WINDOWS32)
		m_bCompatible = true;
#else
	if (module->Compatibility() & WINDOWS64)
		m_bCompatible = true;
#endif
#endif

	std::string pStrName = "holylib_enable_";
	pStrName = pStrName + module->Name();
	std::string cmdStr = "-";
	cmdStr.append(pStrName);
	int cmd = CommandLine()->ParmValue(cmdStr.c_str(), -1);
	if (cmd > -1)
		SetEnabled(cmd == 1, true);
	else
		m_bEnabled = m_bCompatible;

	m_pCVar = new ConVar(pStrName.c_str(), m_bEnabled ? "1" : "0", FCVAR_ARCHIVE, "Whether this module should be active or not", OnModuleConVarChange);
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
				Warning("module %s is not compatible with this platform!\n", m_pModule->Name());

				if (!bForced)
					return;
			}

			int status = g_pModuleManager.GetStatus();
			if (status & LoadStatus_Init)
				m_pModule->Init(&g_pModuleManager.GetAppFactory(), &g_pModuleManager.GetGameFactory());

			if (status & LoadStatus_DetourInit)
			{
				m_pModule->InitDetour(true); // I want every module to be able to be disabled/enabled properly
				m_pModule->InitDetour(false);
			}

			if (status & LoadStatus_LuaInit)
				m_pModule->LuaInit(false);

			if (status & LoadStatus_LuaServerInit)
				m_pModule->LuaInit(true);

			if (!m_bStartup)
				Msg("Enabled module %s\n", m_pModule->Name());
		} else {
			int status = g_pModuleManager.GetStatus();
			if (status & LoadStatus_Init)
				m_pModule->Shutdown();

			if (status & LoadStatus_LuaInit)
				m_pModule->LuaShutdown();

			if (!m_bStartup)
				Msg("Disabled module %s\n", m_pModule->Name());
		}
	}

	m_bEnabled = bEnabled;
}

CModuleManager::CModuleManager() // ToDo: Look into how IGameSystem works and use something similar. I don't like to add each one manually
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
}

int g_pIDs = 0;
void CModuleManager::RegisterModule(IModule* pModule)
{
	++g_pIDs;
	pModule->m_pID = g_pIDs;
	CModule* module = new CModule();
	module->SetModule(pModule);
	Msg("Registered module %-*s (Enabled: %s Compatible: %s)\n", 
		15,
		module->GetModule()->Name(), 
		module->IsEnabled() ? "true, " : "false,", 
		module->IsCompatible() ? "true " : "false"
	);

	m_pModules.push_back(module);
}

CModule* CModuleManager::FindModuleByConVar(ConVar* convar)
{
	for (CModule* module : m_pModules)
	{
		if (convar == module->GetConVar())
			return module;
	}

	return NULL;
}

CModule* CModuleManager::FindModuleByName(const char* name)
{
	for (CModule* module : m_pModules)
	{
		if (V_stricmp(module->GetModule()->Name(), name) == 0)
			return module;
	}

	return NULL;
}

void CModuleManager::Init(CreateInterfaceFn appfn, CreateInterfaceFn gamefn)
{
	m_pAppFactory = appfn;
	m_pGameFactory = gamefn;
	m_pStatus |= LoadStatus_Init;
	for (CModule* pModule : m_pModules)
	{
		if ( !pModule->IsEnabled() ) { continue; }
		pModule->GetModule()->Init(&appfn, &gamefn);
	}
}

void CModuleManager::LuaInit(bool bServerInit)
{
	if (bServerInit)
		m_pStatus |= LoadStatus_LuaServerInit;
	else
		m_pStatus |= LoadStatus_LuaInit;

	for (CModule* pModule : m_pModules)
	{
		if ( !pModule->IsEnabled() ) { continue; }
		pModule->GetModule()->LuaInit(bServerInit);
	}
}

void CModuleManager::LuaShutdown()
{
	for (CModule* pModule : m_pModules)
	{
		if ( !pModule->IsEnabled() ) { continue; }
		pModule->GetModule()->LuaShutdown();
	}
}

void CModuleManager::InitDetour(bool bPreServer)
{
	m_pStatus |= LoadStatus_DetourInit;
	for (CModule* pModule : m_pModules)
	{
		if ( !pModule->IsEnabled() ) { continue; }
		pModule->GetModule()->InitDetour(bPreServer);
	}
}

void CModuleManager::Think(bool bSimulating)
{
	for (CModule* pModule : m_pModules)
	{
		if ( !pModule->IsEnabled() ) { continue; }
		pModule->GetModule()->Think(bSimulating);
	}
}

void CModuleManager::Shutdown()
{
	m_pStatus = 0;
	for (CModule* pModule : m_pModules)
	{
		if ( !pModule->IsEnabled() ) { continue; }
		pModule->GetModule()->Shutdown();
	}
}

CModuleManager g_pModuleManager;