#include "detours.h"
#include "KeyValues.h"
#include <tier2/tier2.h>
#include "modules/_modules.h"
#include "convar.h"

void OnModuleConVarChange(IConVar* cvar, const char* pOldValue, float flOldValue)
{
	CModule* module = g_pModuleManager.FindModuleByConVar((ConVar*)cvar);
	if (!module)
	{
		Warning("Failed to find CModule for convar %s!\n", cvar->GetName());
		return;
	}

	module->SetEnabled(((ConVar*)cvar)->GetBool());
}

void CModule::SetModule(IModule* module)
{
	m_pModule = module;
	m_strName = "holylib_enable_";
	m_strName = m_strName + module->Name();
	m_pCVar = new ConVar(m_strName.c_str(), "1", 0, "Whether this module should be active or not", OnModuleConVarChange);
	m_bEnabled = true;
}
void CModule::SetEnabled(bool bEnabled)
{
	if (m_bEnabled != bEnabled)
	{
		if (bEnabled && !m_bEnabled)
		{
			int status = g_pModuleManager.GetStatus();
			if (status & LoadStatus_Init)
				m_pModule->Init(g_pModuleManager.GetAppFactory(), g_pModuleManager.GetGameFactory());

			if (status & LoadStatus_DetourInit)
			{
				m_pModule->InitDetour(true); // I want every module to be able to be disabled/enabled properly
				m_pModule->InitDetour(false);
			}

			if (status & LoadStatus_LuaInit)
				m_pModule->LuaInit(false);

			if (status & LoadStatus_LuaServerInit)
				m_pModule->LuaInit(true);
		} else {
			int status = g_pModuleManager.GetStatus();
			if (status & LoadStatus_Init)
				m_pModule->Shutdown();

			if (status & LoadStatus_LuaInit)
				m_pModule->LuaShutdown();
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
}

int g_pIDs = 0;
void CModuleManager::RegisterModule(IModule* pModule)
{
	++g_pIDs;
	pModule->m_pID = g_pIDs;
	CModule* module = new CModule();
	module->SetModule(pModule);

	m_pModules.push_back(module);
}

CModule* CModuleManager::FindModuleByConVar(ConVar* cvar)
{
	for (CModule* module : m_pModules)
	{
		if (cvar == module->GetConVar())
			return module;
	}

	return NULL;
}

void CModuleManager::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	m_pAppFactory = appfn;
	m_pGameFactory = gamefn;
	m_pStatus |= LoadStatus_Init;
	for (CModule* pModule : m_pModules)
	{
		if ( !pModule->IsEnabled() ) { continue; }
		pModule->GetModule()->Init(appfn, gamefn);
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