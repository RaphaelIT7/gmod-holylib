#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"
#include "sourcesdk/sv_steamauth.h"
#include <sv_plugin.h>

class CSteamWorksModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "steamworks"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; };
};

CSteamWorksModule g_pSteamWorksModule;
IModule* pSteamWorksModule = &g_pSteamWorksModule;

static Symbols::Steam3ServerT func_Steam3Server;
static Symbols::CSteam3Server_Shutdown func_CSteam3Server_Shutdown;
static Symbols::CSteam3Server_Activate func_CSteam3Server_Activate;
static Symbols::SV_InitGameServerSteam func_SV_InitGameServerSteam;

static Detouring::Hook detour_CSteam3Server_OnLoggedOff;
static void hook_CSteam3Server_OnLoggedOff(CSteam3Server* srv, SteamServersDisconnected_t* info)
{		
	detour_CSteam3Server_OnLoggedOff.GetTrampoline<Symbols::CSteam3Server_OnLoggedOff>()(srv, info);

	if (Lua::PushHook("HolyLib:OnSteamDisconnect"))
	{
		g_Lua->PushNumber(info->m_eResult);
		g_Lua->CallFunctionProtected(2, 0, true);
	}
}

static Detouring::Hook detour_CSteam3Server_OnLogonSuccess;
static void hook_CSteam3Server_OnLogonSuccess(CSteam3Server* srv, SteamServersConnected_t* info)
{		
	detour_CSteam3Server_OnLogonSuccess.GetTrampoline<Symbols::CSteam3Server_OnLogonSuccess>()(srv, info);

	if (Lua::PushHook("HolyLib:OnSteamConnect"))
	{
		g_Lua->CallFunctionProtected(1, 0, true);
	}
}

LUA_FUNCTION_STATIC(steamworks_Shutdown)
{
	if (!func_CSteam3Server_Shutdown)
		LUA->ThrowError("Failed to load CSteam3Server::Shutdown!\n");

	if (!func_Steam3Server)
		LUA->ThrowError("Failed to load Steam3Server!\n");

	func_CSteam3Server_Shutdown(&func_Steam3Server());
	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION_STATIC(steamworks_Activate)
{
	if (!func_CSteam3Server_Activate)
		LUA->ThrowError("Failed to load CSteam3Server::Activate!\n");

	if (!func_Steam3Server)
		LUA->ThrowError("Failed to load Steam3Server!\n");

	func_CSteam3Server_Activate(&func_Steam3Server(), CSteam3Server::EServerType::eServerTypeNormal);
	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION_STATIC(steamworks_IsConnected)
{
	if (!func_Steam3Server)
		LUA->ThrowError("Failed to load Steam3Server!\n");

	LUA->PushBool(func_Steam3Server().BLoggedOn());
	return 1;
}

LUA_FUNCTION_STATIC(steamworks_ForceActivate)
{
	if (!func_SV_InitGameServerSteam)
		LUA->ThrowError("Failed to load SV_InitGameServerSteam!\n");

	func_SV_InitGameServerSteam();
	LUA->PushBool(true);
	return 1;
}

extern CServerPlugin* g_pServerPluginHandler;
LUA_FUNCTION_STATIC(steamworks_ForceAuthenticate)
{
	int iClient = LUA->CheckNumber(1);
	CBaseClient* pClient = Util::GetClientByUserID(iClient);
	if (!pClient || !g_pServerPluginHandler)
	{
		LUA->PushBool(false);
		return 1;
	}

	g_pServerPluginHandler->NetworkIDValidated(pClient->GetClientName(), pClient->GetNetworkIDString());
	Util::servergameclients->NetworkIDValidated(pClient->GetClientName(), pClient->GetNetworkIDString());

	// Verify: Do we need to forcefully change the signonstate?
	pClient->SetFullyAuthenticated();
	LUA->PushBool(true);
	return 1;
}

void CSteamWorksModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	if (Util::PushTable("steamworks"))
	{
		Util::AddFunc(steamworks_Shutdown, "Shutdown");
		Util::AddFunc(steamworks_Activate, "Activate");
		Util::AddFunc(steamworks_IsConnected, "IsConnected");
		Util::AddFunc(steamworks_ForceActivate, "ForceActivate");
		Util::AddFunc(steamworks_ForceAuthenticate, "ForceAuthenticate");
		Util::PopTable();
	}
}

void CSteamWorksModule::LuaShutdown()
{
	if (Util::PushTable("steamworks"))
	{
		Util::RemoveFunc("Shutdown");
		Util::RemoveFunc("Activate");
		Util::RemoveFunc("IsConnected");
		Util::RemoveFunc("ForceActivate");
		Util::PopTable();
	}
}

void CSteamWorksModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }

	SourceSDK::ModuleLoader engine_loader("engine");
	Detour::Create(
		&detour_CSteam3Server_OnLoggedOff, "CSteam3Server::OnLoggedOff",
		engine_loader.GetModule(), Symbols::CSteam3Server_OnLoggedOffSym,
		(void*)hook_CSteam3Server_OnLoggedOff, m_pID
	);

	Detour::Create(
		&detour_CSteam3Server_OnLogonSuccess, "CSteam3Server::OnLogonSuccess",
		engine_loader.GetModule(), Symbols::CSteam3Server_OnLogonSuccessSym,
		(void*)hook_CSteam3Server_OnLogonSuccess, m_pID
	);

	func_Steam3Server = (Symbols::Steam3ServerT)Detour::GetFunction(engine_loader.GetModule(), Symbols::Steam3ServerSym);
	func_CSteam3Server_Shutdown = (Symbols::CSteam3Server_Shutdown)Detour::GetFunction(engine_loader.GetModule(), Symbols::CSteam3Server_ShutdownSym);
	func_CSteam3Server_Activate = (Symbols::CSteam3Server_Activate)Detour::GetFunction(engine_loader.GetModule(), Symbols::CSteam3Server_ActivateSym);
	func_SV_InitGameServerSteam = (Symbols::SV_InitGameServerSteam)Detour::GetFunction(engine_loader.GetModule(), Symbols::SV_InitGameServerSteamSym);
}