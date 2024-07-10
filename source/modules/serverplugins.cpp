#include "sourcesdk/sv_plugin.h"
#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"
#include "filesystem.h"

class CServerPluginLibModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* fn);
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour();
	virtual void Think(bool bSimulating);
	virtual void Shutdown();
	virtual const char* Name() { return "serverplugin"; };
	virtual void LoadConfig(KeyValues* config) {};
};

CServerPluginLibModule g_pServerPluginLibModule;
IModule* pServerPluginLibModule = &g_pServerPluginLibModule;

void CPlugin::SetName( const char *name )
{
	Q_strncpy( m_szName, name, sizeof(m_szName) );
}

IServerPluginCallbacks *CPlugin::GetCallback()
{
	Assert( m_pPlugin );
	if ( m_pPlugin ) 
	{	
		return m_pPlugin;
	}
	else
	{
		Assert( !"Unable to get plugin callback interface" );
		Warning( "Unable to get callback interface for \"%s\"\n", GetName() );
		return NULL;
	}
}

// Source: engine/sv_plugins.cpp
// helper macro to stop this being typed for every passthrough
CServerPlugin* g_pServerPluginHandler;
#define FORALL_PLUGINS	for( int i = 0; i < g_pServerPluginHandler->m_Plugins.Count(); i++ ) 
#define CALL_PLUGIN_IF_ENABLED(call) \
	do { \
		CPlugin *plugin = g_pServerPluginHandler->m_Plugins[i]; \
		if ( ! plugin->IsDisabled() ) \
		{ \
			IServerPluginCallbacks *callbacks = plugin->GetCallback(); \
			if ( callbacks ) \
			{ \
				callbacks-> call ; \
			} \
		} \
	} while (0)

Detouring::Hook detour_CPlugin_Load;
bool hook_CPlugin_Load(CPlugin* pPlugin, const char* fileName)
{
	char fixedFileName[MAX_PATH];
	Q_strncpy(fixedFileName, fileName, sizeof(fixedFileName));
	Q_FixSlashes(fixedFileName);

	pPlugin->m_pPluginModule = g_pFullFileSystem->LoadModule(fixedFileName, "GAME", false);
	if (pPlugin->m_pPluginModule)
	{
		CreateInterfaceFn pluginFactory = Sys_GetFactory(pPlugin->m_pPluginModule);
		if (pluginFactory)
		{
			pPlugin->m_iPluginInterfaceVersion = 4;
			pPlugin->m_bDisable = true;
			pPlugin->m_pPlugin = (IServerPluginCallbacks*)pluginFactory(INTERFACEVERSION_ISERVERPLUGINCALLBACKS, NULL);
			if (!pPlugin->m_pPlugin)
			{
				pPlugin->m_iPluginInterfaceVersion = 3;
				pPlugin->m_pPlugin = (IServerPluginCallbacks*)pluginFactory(INTERFACEVERSION_ISERVERPLUGINCALLBACKS_VERSION_3, NULL);
				if (!pPlugin->m_pPlugin)
				{
					pPlugin->m_iPluginInterfaceVersion = 2;
					pPlugin->m_pPlugin = (IServerPluginCallbacks*)pluginFactory(INTERFACEVERSION_ISERVERPLUGINCALLBACKS_VERSION_2, NULL);
					if (!pPlugin->m_pPlugin)
					{
						pPlugin->m_iPluginInterfaceVersion = 1;
						pPlugin->m_pPlugin = (IServerPluginCallbacks*)pluginFactory(INTERFACEVERSION_ISERVERPLUGINCALLBACKS_VERSION_1, NULL);
						if (!pPlugin->m_pPlugin)
						{				
							Warning("Could not get IServerPluginCallbacks interface from plugin \"%s\"", fileName);
							return false;
						}
					}
				}
			}

			if (!pPlugin->m_pPlugin->Load(g_interfaceFactory,  g_gameServerFactory))
			{
				Warning("Failed to load plugin \"%s\"\n", fileName);
				return false;
			}

			pPlugin->SetName(pPlugin->m_pPlugin->GetPluginDescription());
			pPlugin->m_bDisable = false;
		}
	}
	else
	{
		Warning("Unable to load plugin \"%s\"\n", fileName);
		return false;
	}

	return true;
}

void CServerPluginLibModule::Init(CreateInterfaceFn* fn)
{
	g_pServerPluginHandler = (CServerPlugin*)fn[0](INTERFACEVERSION_ISERVERPLUGINHELPERS, NULL);
	Detour::CheckValue("get interface", "g_pServerPluginHandler", g_pServerPluginHandler != NULL);
}

void CServerPluginLibModule::LuaInit(bool bServerInit)
{
	if (!bServerInit)
	{
		FORALL_PLUGINS
		{
			CALL_PLUGIN_IF_ENABLED(OnLuaInit(g_Lua));
		}
	}
}

void CServerPluginLibModule::LuaShutdown() // ToDo: Change this to be called when the Lua Interface is actually shutdown -> Lua::Kill
{
	FORALL_PLUGINS
	{
		CALL_PLUGIN_IF_ENABLED(OnLuaShutdown(g_Lua));
	}
}

void CServerPluginLibModule::InitDetour()
{
	SourceSDK::ModuleLoader engine_loader("engine");
	Detour::Create(
		&detour_CPlugin_Load, "CPlugin::Load", engine_loader.GetModule(),
		Symbols::CPlugin_LoadSym, (void*)hook_CPlugin_Load, m_pID
	);
}

void CServerPluginLibModule::Think(bool simulating)
{
}

void CServerPluginLibModule::Shutdown()
{
}