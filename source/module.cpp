#include "detours.h"
#include "KeyValues.h"
#include <tier2/tier2.h>
#include "modules/_modules.h"

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

void CModuleManager::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	for (CModule* pModule : m_pModules)
	{
		if ( !pModule->IsEnabled() ) { continue; }
		pModule->GetModule()->Init(appfn, gamefn);
	}
}

void CModuleManager::LuaInit(bool bServerInit)
{
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
	for (CModule* pModule : m_pModules)
	{
		if ( !pModule->IsEnabled() ) { continue; }
		pModule->GetModule()->Shutdown();
	}
}

CModuleManager g_pModuleManager;