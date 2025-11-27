#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "sourcesdk/sv_steamauth.h"
#include <sv_plugin.h>
#include "utlbuffer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CSteamWorksModule : public IModule
{
public:
	void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) override;
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void Think(bool bSimulating) override;
	void InitDetour(bool bPreServer) override;
	void LevelShutdown() override;
	const char* Name() override { return "steamworks"; };
	int Compatibility() override { return LINUX32 | LINUX64; };
	bool SupportsMultipleLuaStates() override { return true; };
};

CSteamWorksModule g_pSteamWorksModule;
IModule* pSteamWorksModule = &g_pSteamWorksModule;

static ConVar allow_duplicate_steamid("holylib_steamworks_allow_duplicate_steamid", "0", 0, "If enabled, the same steamid can be used multiple times.");

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

static std::vector<CBaseClient*> g_pApprovedClients;
static std::unordered_set<uint64> g_pApprovedSteamIDs;
static Detouring::Hook detour_CSteam3Server_NotifyClientConnect;
static Symbols::CSteam3Server_SendUpdatedServerDetails func_CSteam3Server_SendUpdatedServerDetails;
static bool hook_CSteam3Server_NotifyClientConnect(CSteam3Server* srv, CBaseClient* client, uint32 unUserID, netadr_t& adr, const void *pvCookie, uint32 ucbCookie)
{
	bool bRet = detour_CSteam3Server_NotifyClientConnect.GetTrampoline<Symbols::CSteam3Server_NotifyClientConnect>()(srv, client, unUserID, adr, pvCookie, ucbCookie);

	if (Lua::PushHook("HolyLib:OnNotifyClientConnect"))
	{
		CUtlBuffer buffer( pvCookie, ucbCookie, CUtlBuffer::READ_ONLY );
		uint64 ulSteamID = buffer.GetInt64();
		CSteamID steamID( ulSteamID );

		pvCookie = (uint8 *)pvCookie + sizeof( uint64 );
		ucbCookie -= sizeof( uint64 );

		g_Lua->PushNumber(unUserID);
		g_Lua->PushString(adr.ToString());
		g_Lua->PushString(std::to_string(ulSteamID).c_str());

		int status = 0;
		if (!bRet)
		{
			// Try it again so that we get the reason why it failed.
			status = SteamGameServer()->BeginAuthSession( pvCookie, ucbCookie, steamID );
		}
		g_Lua->PushNumber(status);

		if (g_Lua->CallFunctionProtected(5, 1, true))
		{
			if (g_Lua->IsType(-1, GarrysMod::Lua::Type::Bool))
			{
				bool bOverride = g_Lua->GetBool(-1);

				if (!bRet && bOverride)
				{
					client->SetSteamID(steamID);

					func_CSteam3Server_SendUpdatedServerDetails(srv);

					// BUG: Gmod by default refuses to send the ServerInfo since it has some additional checks in CBaseServer::SendPendingServerInfo that prevent it.
					client->m_bSendServerInfo = true;
					g_pApprovedClients.push_back(client);
				}

				if (bOverride) // Steam may not always deny a connection but a steamID may still already be in use.
				{
					g_pApprovedSteamIDs.insert(ulSteamID);
				}

				bRet = bOverride;
			}

			g_Lua->Pop(1);
		}
	}

	return bRet;
}

static Detouring::Hook detour_CSteam3Server_CheckForDuplicateSteamID;
static bool hook_CSteam3Server_CheckForDuplicateSteamID(CSteam3Server* srv, CBaseClient* pClient)
{
	bool bRet = detour_CSteam3Server_CheckForDuplicateSteamID.GetTrampoline<Symbols::CSteam3Server_CheckForDuplicateSteamID>()(srv, pClient);

	if (bRet)
	{
		if (allow_duplicate_steamid.GetBool())
		{
			bRet = false;
		}

		auto it = g_pApprovedSteamIDs.find(pClient->m_SteamID.ConvertToUint64());
		if (it != g_pApprovedSteamIDs.end())
		{
			bRet = false;
			g_pApprovedSteamIDs.erase(it);
		}
	}

	return bRet;
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

static CServerPlugin* pServerPluginHandler = nullptr;
LUA_FUNCTION_STATIC(steamworks_ForceAuthenticate)
{
	int iClient = (int)LUA->CheckNumber(1);
	CBaseClient* pClient = Util::GetClientByUserID(iClient);
	if (!pClient)
	{
		LUA->PushBool(false);
		return 1;
	}

	if (pServerPluginHandler) // just to be sure
		pServerPluginHandler->NetworkIDValidated(pClient->GetClientName(), pClient->GetNetworkIDString());
	
	Util::servergameclients->NetworkIDValidated(pClient->GetClientName(), pClient->GetNetworkIDString());

	// Verify: Do we need to forcefully change the signonstate?
	pClient->SetFullyAuthenticated();
	LUA->PushBool(true);
	return 1;
}

void CSteamWorksModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	if (appfn[0])
	{
		pServerPluginHandler = (CServerPlugin*)appfn[0](INTERFACEVERSION_ISERVERPLUGINHELPERS, nullptr);
	} else {
		SourceSDK::FactoryLoader engine_loader("engine");
		pServerPluginHandler = engine_loader.GetInterface<CServerPlugin>(INTERFACEVERSION_ISERVERPLUGINHELPERS);
	}
	Detour::CheckValue("get interface", "pServerPluginHandler", pServerPluginHandler != nullptr);
}

void CSteamWorksModule::Think(bool bSimulating)
{
	for (auto it = g_pApprovedClients.begin(); it != g_pApprovedClients.end(); )
	{
		CBaseClient* pClient = *it;
		if (!pClient->IsConnected())
		{
			Msg(PROJECT_NAME ": removed client as it wasn't connected\n");
			it = g_pApprovedClients.erase(it);
			continue;
		}

		if (!pClient->m_bSendServerInfo)
		{
			Msg(PROJECT_NAME ": skipped client as it didn't want the serverinfo\n");
			it++;
			continue;
		}

		pClient->SendServerInfo();
		Msg(PROJECT_NAME ": sent client the server info it desired\n");
		it = g_pApprovedClients.erase(it);
	}
}

void CSteamWorksModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	if (Util::PushTable(pLua, "steamworks"))
	{
		Util::AddFunc(pLua, steamworks_Shutdown, "Shutdown");
		Util::AddFunc(pLua, steamworks_Activate, "Activate");
		Util::AddFunc(pLua, steamworks_IsConnected, "IsConnected");
		Util::AddFunc(pLua, steamworks_ForceActivate, "ForceActivate");
		Util::AddFunc(pLua, steamworks_ForceAuthenticate, "ForceAuthenticate");
		Util::PopTable(pLua);
	}
}

void CSteamWorksModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	if (Util::PushTable(pLua, "steamworks"))
	{
		Util::RemoveField(pLua, "Shutdown");
		Util::RemoveField(pLua, "Activate");
		Util::RemoveField(pLua, "IsConnected");
		Util::RemoveField(pLua, "ForceActivate");
		Util::PopTable(pLua);
	}
}

void CSteamWorksModule::LevelShutdown()
{
	g_pApprovedClients.clear();
	g_pApprovedSteamIDs.clear();
}

#if SYSTEM_WINDOWS
DETOUR_THISCALL_START()
	DETOUR_THISCALL_ADDFUNC1( hook_CSteam3Server_OnLoggedOff, OnLoggedOff, CSteam3Server*, SteamServersDisconnected_t* );
	DETOUR_THISCALL_ADDFUNC1( hook_CSteam3Server_OnLogonSuccess, OnLogonSuccess, CSteam3Server*, SteamServersConnected_t* );
	DETOUR_THISCALL_ADDRETFUNC5( hook_CSteam3Server_NotifyClientConnect, bool, NotifyClientConnect, CSteam3Server*, CBaseClient*, uint32, netadr_t&, const void*, uint32 );
	DETOUR_THISCALL_ADDRETFUNC1( hook_CSteam3Server_CheckForDuplicateSteamID, bool, CheckForDuplicateSteamID, CSteam3Server*, CBaseClient* );
DETOUR_THISCALL_FINISH();
#endif

void CSteamWorksModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }

	DETOUR_PREPARE_THISCALL();
	SourceSDK::ModuleLoader engine_loader("engine");
	Detour::Create(
		&detour_CSteam3Server_OnLoggedOff, "CSteam3Server::OnLoggedOff",
		engine_loader.GetModule(), Symbols::CSteam3Server_OnLoggedOffSym,
		(void*)DETOUR_THISCALL(hook_CSteam3Server_OnLoggedOff, OnLoggedOff), m_pID
	);

	Detour::Create(
		&detour_CSteam3Server_OnLogonSuccess, "CSteam3Server::OnLogonSuccess",
		engine_loader.GetModule(), Symbols::CSteam3Server_OnLogonSuccessSym,
		(void*)DETOUR_THISCALL(hook_CSteam3Server_OnLogonSuccess, OnLogonSuccess), m_pID
	);

	Detour::Create(
		&detour_CSteam3Server_NotifyClientConnect, "CSteam3Server::NotifyClientConnect",
		engine_loader.GetModule(), Symbols::CSteam3Server_NotifyClientConnectSym,
		(void*)DETOUR_THISCALL(hook_CSteam3Server_NotifyClientConnect, NotifyClientConnect), m_pID
	);

	Detour::Create(
		&detour_CSteam3Server_CheckForDuplicateSteamID, "CSteam3Server::CheckForDuplicateSteamID",
		engine_loader.GetModule(), Symbols::CSteam3Server_CheckForDuplicateSteamIDSym,
		(void*)DETOUR_THISCALL(hook_CSteam3Server_CheckForDuplicateSteamID, CheckForDuplicateSteamID), m_pID
	);

	func_Steam3Server = (Symbols::Steam3ServerT)Detour::GetFunction(engine_loader.GetModule(), Symbols::Steam3ServerSym);
	func_CSteam3Server_Shutdown = (Symbols::CSteam3Server_Shutdown)Detour::GetFunction(engine_loader.GetModule(), Symbols::CSteam3Server_ShutdownSym);
	func_CSteam3Server_Activate = (Symbols::CSteam3Server_Activate)Detour::GetFunction(engine_loader.GetModule(), Symbols::CSteam3Server_ActivateSym);
	func_SV_InitGameServerSteam = (Symbols::SV_InitGameServerSteam)Detour::GetFunction(engine_loader.GetModule(), Symbols::SV_InitGameServerSteamSym);
	func_CSteam3Server_SendUpdatedServerDetails = (Symbols::CSteam3Server_SendUpdatedServerDetails)Detour::GetFunction(engine_loader.GetModule(), Symbols::CSteam3Server_SendUpdatedServerDetailsSym);
}