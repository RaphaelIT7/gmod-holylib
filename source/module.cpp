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
	RegisterModule(pSurfFuxModule);
}

int g_pIDs = 0;
void CModuleManager::RegisterModule(IModule* pModule)
{
	++g_pIDs;
	pModule->m_pID = g_pIDs;
	m_pModules.push_back(pModule);
}

void CModuleManager::LoadConfig() // ToDo: Finish this config system.
{
	if (!m_pConfig)
		m_pConfig = new KeyValues("holylib");

	m_pConfig->LoadFromFile((IBaseFileSystem*)g_pFullFileSystem, "cfg/holylib.vdf", "MOD");

	for (IModule* pModule : m_pModules)
	{
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

void CModuleManager::Init(CreateInterfaceFn* fn)
{
	LoadConfig(); // If we call it in the constructor, it will crash, since the g_pFullFilesystem wasn't set yet.

	for (IModule* pModule : m_pModules)
	{
		pModule->Init(fn);
	}
}

void CModuleManager::LuaInit(bool bServerInit)
{
	for (IModule* pModule : m_pModules)
	{
		pModule->LuaInit(bServerInit);
	}
}

void CModuleManager::LuaShutdown()
{
	for (IModule* pModule : m_pModules)
	{
		pModule->LuaShutdown();
	}
}

void CModuleManager::InitDetour(bool bPreServer)
{
	for (IModule* pModule : m_pModules)
	{
		pModule->InitDetour(bPreServer);
	}
}

void CModuleManager::Think(bool bSimulating)
{
	for (IModule* pModule : m_pModules)
	{
		pModule->Think(bSimulating);
	}
}

void CModuleManager::Shutdown()
{
	for (IModule* pModule : m_pModules)
	{
		pModule->Shutdown();
	}
}

CModuleManager g_pModuleManager;