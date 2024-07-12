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

void CModuleManager::LoadConfig() // ToDo: Finish this config system.
{
	if (!m_pConfig)
		m_pConfig = new KeyValues("holylib");

	m_pConfig->LoadFromFile((IBaseFileSystem*)g_pFullFileSystem, "cfg/holylib.vdf", "MOD");

	for (CModule* module : m_pModules)
	{
		IModule* pModule = module->GetModule();
		KeyValues* pKey = NULL;
		if(m_pConfig->IsEmpty(pModule->Name()))
		{
			pKey = m_pConfig->CreateNewKey();
			pKey->SetName(pModule->Name());
		} else {
			for (KeyValues* sub = m_pConfig->GetFirstSubKey(); sub; sub=sub->GetNextKey())
			{
				if (V_stricmp(sub->GetName(), pModule->Name()) == 0)
				{
					pKey = sub;
					break;
				}
			}
		}

		if (!pKey)
		{
			DevMsg(1, "Failed to get an KeyValues* for %s!\n", pModule->Name());
			continue;
		}

		pModule->LoadConfig(pKey);
	}

	m_pConfig->SaveToFile((IBaseFileSystem*)g_pFullFileSystem, "cfg/holylib.vdf");
}

void CModuleManager::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	LoadConfig(); // If we call it in the constructor, it will crash, since the g_pFullFilesystem wasn't set yet.

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