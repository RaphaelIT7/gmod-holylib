#include "detours.h"
#include "module.h"
#include "KeyValues.h"
#include <tier2/tier2.h>
#include "modules/_modules.h"

int g_pIDs = 0;
void CModuleManager::RegisterModule(IModule* pModule)
{
	++g_pIDs;
	pModule->m_pID = g_pIDs;
	m_pModules.push_back(pModule);
}

void CModuleManager::LoadConfig()
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

	m_pConfig->SaveToFile((IBaseFileSystem*)g_pFullFileSystem, "cfg/holylib.vdf", "MOD");
}

void CModuleManager::Init(CreateInterfaceFn* fn)
{
	RegisterModule(pHolyLibModule);
	RegisterModule(pGameeventLibModule);
	RegisterModule(pServerPluginLibModule);
	RegisterModule(pSourceTVLibModule);
	RegisterModule(pThreadpoolFixModule);

	LoadConfig();

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

void CModuleManager::InitDetour()
{
	for (IModule* pModule : m_pModules)
	{
		pModule->InitDetour();
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