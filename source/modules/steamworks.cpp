#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "sourcesdk/sv_steamauth.h"
#include <sv_plugin.h>
#include "utlbuffer.h"

class CSteamWorksModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void Think(bool bSimulating) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "steamworks"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; };
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

extern CServerPlugin* g_pServerPluginHandler;
LUA_FUNCTION_STATIC(steamworks_ForceAuthenticate)
{
	int iClient = (int)LUA->CheckNumber(1);
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

void CSteamWorksModule::Think(bool bSimulating)
{
	for (auto it = g_pApprovedClients.begin(); it != g_pApprovedClients.end(); )
	{
		CBaseClient* pClient = *it;
		if (!pClient->IsConnected())
		{
			Msg("holylib: removed client as it wasn't connected\n");
			it = g_pApprovedClients.erase(it);
			continue;
		}

		if (!pClient->m_bSendServerInfo)
		{
			Msg("holylib: skipped client as it didn't want the serverinfo\n");
			it++;
			continue;
		}

		pClient->SendServerInfo();
		Msg("holylib: sent client the server info it desired\n");
		it = g_pApprovedClients.erase(it);
	}
}

void CSteamWorksModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
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

void CSteamWorksModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	if (Util::PushTable("steamworks"))
	{
		Util::RemoveField("Shutdown");
		Util::RemoveField("Activate");
		Util::RemoveField("IsConnected");
		Util::RemoveField("ForceActivate");
		Util::PopTable();
	}

	g_pApprovedClients.clear();
	g_pApprovedSteamIDs.clear();
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

	Detour::Create(
		&detour_CSteam3Server_NotifyClientConnect, "CSteam3Server::NotifyClientConnect",
		engine_loader.GetModule(), Symbols::CSteam3Server_NotifyClientConnectSym,
		(void*)hook_CSteam3Server_NotifyClientConnect, m_pID
	);

	Detour::Create(
		&detour_CSteam3Server_CheckForDuplicateSteamID, "CSteam3Server::CheckForDuplicateSteamID",
		engine_loader.GetModule(), Symbols::CSteam3Server_CheckForDuplicateSteamIDSym,
		(void*)hook_CSteam3Server_CheckForDuplicateSteamID, m_pID
	);

	func_Steam3Server = (Symbols::Steam3ServerT)Detour::GetFunction(engine_loader.GetModule(), Symbols::Steam3ServerSym);
	func_CSteam3Server_Shutdown = (Symbols::CSteam3Server_Shutdown)Detour::GetFunction(engine_loader.GetModule(), Symbols::CSteam3Server_ShutdownSym);
	func_CSteam3Server_Activate = (Symbols::CSteam3Server_Activate)Detour::GetFunction(engine_loader.GetModule(), Symbols::CSteam3Server_ActivateSym);
	func_SV_InitGameServerSteam = (Symbols::SV_InitGameServerSteam)Detour::GetFunction(engine_loader.GetModule(), Symbols::SV_InitGameServerSteamSym);
	func_CSteam3Server_SendUpdatedServerDetails = (Symbols::CSteam3Server_SendUpdatedServerDetails)Detour::GetFunction(engine_loader.GetModule(), Symbols::CSteam3Server_SendUpdatedServerDetailsSym);
}