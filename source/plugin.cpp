#include <GarrysMod/InterfacePointers.hpp>
#include <GarrysMod/FactoryLoader.hpp>
#include "LuaInterface.h"
#include <GarrysMod/Lua/LuaShared.h>
#include "filesystem.h"
#include "detours.h"
#include <tier2/tier2.h>
#include <tier1.h>
#include "module.h"
#include "plugin.h"
#include "vprof.h"
#include "holylua.h"

struct edict_t;
#include "playerinfomanager.h"

#define DEDICATED
#include "vstdlib/jobthread.h"
#include <eiface.h>
#include <icommandline.h>
#include <datacache/imdlcache.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// The plugin is a static singleton that is exported as an interface
static CServerPlugin g_HolyLibServerPlugin;
CServerPlugin* g_pHolyLibServerPlugin = &g_HolyLibServerPlugin;
#ifdef LIB_HOLYLIB
IServerPluginCallbacks* GetHolyLibPlugin()
{
	return g_pHolyLibServerPlugin;
}
#else
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CServerPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_HolyLibServerPlugin);
#endif

//---------------------------------------------------------------------------------
// Purpose: constructor/destructor
//---------------------------------------------------------------------------------
CServerPlugin::CServerPlugin()
{
	m_iClientCommandIndex = 0;
}

CServerPlugin::~CServerPlugin()
{
}

void CServerPlugin::GhostInj()
{
	if (!Util::ShouldLoad())
	{
		Msg("HolyLib already exists? Stopping.\n");
		return;
	}

#ifdef LIB_HOLYLIB
	g_pModuleManager.LoadModules();
#endif

	g_pModuleManager.SetGhostInj();
	g_pModuleManager.InitDetour(true);
}

#ifdef LIB_HOLYLIB
void HolyLib_PreLoad()
#else
DLL_EXPORT void HolyLib_PreLoad()
#endif
{
	g_HolyLibServerPlugin.GhostInj();
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is loaded, load the interface we need from the engine
//---------------------------------------------------------------------------------
CGlobalVars *gpGlobals = NULL;
static bool bIgnoreNextUnload = false;
#if ARCHITECTURE_IS_X86
IMDLCache *mdlcache = NULL;
#endif
bool CServerPlugin::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory)
{
	VPROF_BUDGET("HolyLib - CServerPlugin::Load", VPROF_BUDGETGROUP_HOLYLIB);

	Msg("--- HolyLib Plugin loading ---\n");

	if (!Util::ShouldLoad())
	{
		Msg("HolyLib already exists? Stopping.\n");
		Msg("--- HolyLib Plugin finished loading ---\n");
		bIgnoreNextUnload = true;
		return false; // What if we return false?
	}
	 
	if (interfaceFactory)
	{
		ConnectTier1Libraries(&interfaceFactory, 1);
		ConnectTier2Libraries(&interfaceFactory, 1);

		engine = (IVEngineServer*)interfaceFactory(INTERFACEVERSION_VENGINESERVER, NULL);
		mdlcache = (IMDLCache*)interfaceFactory(MDLCACHE_INTERFACE_VERSION, NULL);
	} else {
		engine = InterfacePointers::VEngineServer();
		g_pFullFileSystem = InterfacePointers::FileSystemServer();
		g_pCVar = InterfacePointers::Cvar();
	}

	IPlayerInfoManager* playerinfomanager = NULL;
	if (gameServerFactory)
		playerinfomanager = (IPlayerInfoManager*)gameServerFactory(INTERFACEVERSION_PLAYERINFOMANAGER, NULL);
	else {
		SourceSDK::FactoryLoader server_loader("server");
		playerinfomanager = server_loader.GetInterface<IPlayerInfoManager>(INTERFACEVERSION_PLAYERINFOMANAGER);
	}
	Detour::CheckValue("get interface", "playerinfomanager", playerinfomanager != NULL);

	if (playerinfomanager)
		gpGlobals = playerinfomanager->GetGlobalVars();

	g_pModuleManager.Setup(interfaceFactory, gameServerFactory); // Setup so that Util won't cause issues
	Lua::AddDetour();
	Util::AddDetour();
	g_pModuleManager.Init();
	g_pModuleManager.InitDetour(false);
	HolyLua::Init();

	GarrysMod::Lua::ILuaInterface* LUA = Lua::GetRealm(g_pModuleManager.GetModuleRealm());
	if (LUA) // If we got loaded by plugin_load we need to manually call Lua::Init
		Lua::Init(LUA);

#if ARCHITECTURE_IS_X86_64
	//if (CommandLine()->FindParm("-holylib_debug_forceregister"))
#endif
	{
		ConVar_Register(); // ConVars currently cause a crash on level shutdown. I probably need to find some hidden vtable function AGAIN.
	}
	/*
	 * Debug info about the crash from what I could find(could be wrong):
	 * - Where: engine.so
	 * - Offset: 0x106316
	 * - Manifest: 8648260017074424678
	 * - Which function: CUtlLinkedList<T,S,ML,I,M>::AllocInternal( bool multilist )
	 * - Which line (Verify): typename M::Iterator_t it = m_Memory.IsValidIterator( m_LastAlloc ) ? m_Memory.Next( m_LastAlloc ) : m_Memory.First();
	 * - Why: I have no Idea. Maybe the class is different in our sdk?
	 */

	Msg("--- HolyLib Plugin finished loading ---\n");

	return true;
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is unloaded (turned off)
//---------------------------------------------------------------------------------
void CServerPlugin::Unload(void)
{
	VPROF_BUDGET("HolyLib - CServerPlugin::Unload", VPROF_BUDGETGROUP_HOLYLIB);

	if (bIgnoreNextUnload)
	{
		bIgnoreNextUnload = false;
		return;
	}

	HolyLua::Shutdown();
	Lua::ManualShutdown(); // Called to make sure that everything is shut down properly in cases of plugin_unload. does nothing if g_Lua is already NULL.
	g_pModuleManager.Shutdown();
	Util::RemoveDetour();
	Detour::Remove(0);
	Detour::ReportLeak();

#if ARCHITECTURE_IS_X86_64
	//if (CommandLine()->FindParm("-holylib_debug_forceregister"))
#endif
	{
		ConVar_Unregister();
	}

	CommandLine()->RemoveParm("-holylibexists"); // Allow holylib to load again since this instance is gone.

	DisconnectTier1Libraries();
	DisconnectTier2Libraries();
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is paused (i.e should stop running but isn't unloaded)
//---------------------------------------------------------------------------------
void CServerPlugin::Pause(void)
{
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is unpaused (i.e should start executing again)
//---------------------------------------------------------------------------------
void CServerPlugin::UnPause(void)
{
}

//---------------------------------------------------------------------------------
// Purpose: the name of this plugin, returned in "plugin_print" command
//---------------------------------------------------------------------------------
const char* CServerPlugin::GetPluginDescription(void)
{
#if !HOLYLIB_BUILD_RELEASE // DATA should always fallback to 0. We will set it to 1 in releases.
	return "HolyLib Serverplugin V0.73 DEV (Workflow: " GITHUB_RUN_NUMBER ")";
#else
	return "HolyLib Serverplugin V0.73";
#endif
}

//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void CServerPlugin::LevelInit(char const *pMapName)
{

}

//---------------------------------------------------------------------------------
// Purpose: called after LUA Initialized.
//---------------------------------------------------------------------------------
bool CServerPlugin::LuaInit()
{
	VPROF_BUDGET("HolyLib - CServerPlugin::LuaInit", VPROF_BUDGETGROUP_HOLYLIB);
	GarrysMod::Lua::ILuaInterface* LUA = Lua::GetRealm(g_pModuleManager.GetModuleRealm());
	if (LUA == nullptr) {
		Warning(PROJECT_NAME ": Failed to get ILuaInterface! (Realm: %i)\n", (int)g_pModuleManager.GetModuleRealm());
		return false;
	}

	Lua::Init(LUA);
	return true;
}

//---------------------------------------------------------------------------------
// Purpose: called just before LUA is shutdown.
//---------------------------------------------------------------------------------
void CServerPlugin::LuaShutdown()
{
	VPROF_BUDGET("HolyLib - CServerPlugin::LuaShutdown", VPROF_BUDGETGROUP_HOLYLIB);
	Lua::Shutdown();
}

//---------------------------------------------------------------------------------
// Purpose: called on level start, when the server is ready to accept client connections
//		edictCount is the number of entities in the level, clientMax is the max client count
//---------------------------------------------------------------------------------
void CServerPlugin::ServerActivate(edict_t *pEdictList, int edictCount, int clientMax)
{
	VPROF_BUDGET("HolyLib - CServerPlugin::ServerActivate", VPROF_BUDGETGROUP_HOLYLIB);
	if (!g_Lua)
		LuaInit();

	Lua::ServerInit();
	g_pModuleManager.ServerActivate(pEdictList, edictCount, clientMax);
	Util::CheckVersion();
}

//---------------------------------------------------------------------------------
// Purpose: called once per server frame, do recurring work here (like checking for timeouts)
//---------------------------------------------------------------------------------
void CServerPlugin::GameFrame(bool simulating)
{
	VPROF_BUDGET("HolyLib - CServerPlugin::GameFrame", VPROF_BUDGETGROUP_HOLYLIB);
	g_pModuleManager.Think(simulating);
}

//---------------------------------------------------------------------------------
// Purpose: called on level end (as the server is shutting down or going to a new map)
//---------------------------------------------------------------------------------
void CServerPlugin::LevelShutdown(void) // !!!!this can get called multiple times per map change
{
	g_pModuleManager.LevelShutdown();
}

//---------------------------------------------------------------------------------
// Purpose: called when a client spawns into a server (i.e as they begin to play)
//---------------------------------------------------------------------------------
void CServerPlugin::ClientActive(edict_t *pEntity)
{
}

//---------------------------------------------------------------------------------
// Purpose: called when a client leaves a server (or is timed out)
//---------------------------------------------------------------------------------
void CServerPlugin::ClientDisconnect(edict_t *pEntity)
{
}

//---------------------------------------------------------------------------------
// Purpose: called when a client spawns?
//---------------------------------------------------------------------------------
void CServerPlugin::ClientPutInServer(edict_t *pEntity, char const *playername)
{
}

//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void CServerPlugin::SetCommandClient(int index)
{
	m_iClientCommandIndex = index;
}

//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void CServerPlugin::ClientSettingsChanged(edict_t *pEdict)
{
}

//---------------------------------------------------------------------------------
// Purpose: called when a client joins a server
//---------------------------------------------------------------------------------
PLUGIN_RESULT CServerPlugin::ClientConnect(bool* bAllowConnect, edict_t* pEntity, const char* pszName, const char* pszAddress, char* reject, int maxrejectlen)
{
	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: called when a client types in a command (only a subset of commands however, not CON_COMMAND's)
//---------------------------------------------------------------------------------
PLUGIN_RESULT CServerPlugin::ClientCommand(edict_t *pEntity, const CCommand &args)
{
	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: called when a client is authenticated
//---------------------------------------------------------------------------------
PLUGIN_RESULT CServerPlugin::NetworkIDValidated(const char *pszUserName, const char *pszNetworkID)
{
	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: called when a cvar value query is finished
//---------------------------------------------------------------------------------
void CServerPlugin::OnQueryCvarValueFinished(QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue)
{
}

void CServerPlugin::OnEdictAllocated(edict_t *edict)
{
	g_pModuleManager.OnEdictAllocated(edict);
}

void CServerPlugin::OnEdictFreed(const edict_t *edict)
{
	g_pModuleManager.OnEdictFreed(edict);
}

GMOD_MODULE_OPEN()
{
	LUA->GetField(LUA_GLOBALSINDEX, "CLIENT");
	bool bClient = LUA->GetBool(-1);
	LUA->Pop(1);

	LUA->GetField(LUA_GLOBALSINDEX, "SERVER");
	bool bServer = LUA->GetBool(-1);
	LUA->Pop(1);

	if (bClient)
		g_pModuleManager.SetModuleRealm(Module_Realm::CLIENT);
	else if (bServer)
		g_pModuleManager.SetModuleRealm(Module_Realm::SERVER);
	else
		g_pModuleManager.SetModuleRealm(Module_Realm::MENU);

	Util::CheckVersion();
	g_pModuleManager.MarkAsBinaryModule();
	Lua::SetManualShutdown();
	g_HolyLibServerPlugin.Load(NULL, NULL); // Yes. I don't like it but I can't get thoes fancy interfaces.

	return 0;
}

GMOD_MODULE_CLOSE()
{
	g_HolyLibServerPlugin.Unload();

	return 0;
}