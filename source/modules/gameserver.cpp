#include "module.h"
#include "LuaInterface.h"
#include "lua.h"
#include "detours.h"
#include "usermessages.h"
#include "sv_client.h"
#include "eiface.h"
#include "tier0/etwprof.h"
#include "sourcesdk/baseserver.h"
#include "sourcesdk/net_chan.h"
#include <framesnapshot.h>
#include <netadr_new.h> // Better than the normal sdk one as this one actually sets stuff properly.
#include <shareddefs.h>
#include <unordered_set>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CGameServerModule : public IModule
{
public:
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void InitDetour(bool bPreServer) override;
	void OnClientDisconnect(CBaseClient* pClient) override;
	void Think(bool bSimulating) override;
	const char* Name() override { return "gameserver"; };
	int Compatibility() override { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
	bool SupportsMultipleLuaStates() override { return true; };
};

static ConVar gameserver_disablespawnsafety("holylib_gameserver_disablespawnsafety", "0", 0, "If enabled, players can spawn on slots above 128 but this WILL cause stability and many other issues!");
static ConVar gameserver_connectionlesspackethook("holylib_gameserver_connectionlesspackethook", "1", 0, "If enabled, the HolyLib:ProcessConnectionlessPacket hook is active and will be called.");
ConVar sv_filter_nobanresponse("sv_filter_nobanresponse", "0", 0, "If enabled, a blocked ip won't be informed that its even blocked.");
static ConVar gameserver_rawclients("holylib_gameserver_rawclients", "0", 0, "Experimental - Exposes the CBaseClient's even when their empty/have no clients connected");

static CGameServerModule g_pGameServerModule;
IModule* pGameServerModule = &g_pGameServerModule;

static std::vector<CGameClient*> g_pQueueClients;
extern CGlobalVars* gpGlobals;

/*
 * Queue CGameClients use slots at/above gpGlobals->maxClients, but the engine's
 * CGameClient::Connect still derives an edict from slot + 1. Those edicts are
 * map entities, not players. In particular, slot 128 aliases the first edict
 * after the reserved player range.
 *
 * Never classify a parked client against CBaseServer::m_nMaxclients. HolyLib
 * temporarily raises that field while publishing the extended queue capacity
 * to Steam, so a re-entrant teardown can otherwise pass a 128+ client into
 * CGameServer::RemoveClientFromGame. The engine then forwards the aliased
 * edict to the game DLL's ClientDisconnect path and removes/corrupts an
 * unrelated map entity (observed live with the singleton soundent).
 *
 * A parked client keeps that alias for the engine's own bookkeeping until the
 * queue shell is retired. It must never reach a game-DLL player callback:
 * CGameClient::SetSignonState is intercepted before CheckConnect, commands are
 * blocked, and RemoveClientFromGame is guarded. The edict references are
 * detached only at the final retirement boundary, immediately before Clear.
 */
static bool IsParkedQueueClient(CBaseClient* pCandidate)
{
	if (!pCandidate || !gpGlobals)
		return false;

	// RemoveClientFromGame is called by the engine with a live CBaseClient. Slot
	// ownership is the authoritative boundary here: vector membership is only
	// bookkeeping and can change during re-entrant disconnect/promotion hooks.
	return pCandidate->m_nClientSlot >= gpGlobals->maxClients;
}

static void DetachParkedQueueClientEdict(CBaseClient* pCandidate)
{
	if (!IsParkedQueueClient(pCandidate))
		return;

	CGameClient* pClient = (CGameClient*)pCandidate;
	if (pClient->edict || pClient->m_PackInfo.m_pClientEnt)
		Msg(PROJECT_NAME " - gameserver: detached aliased edict while parking queue slot %i\n", pClient->m_nClientSlot);

	pClient->edict = nullptr;
	pClient->m_PackInfo.m_pClientEnt = nullptr;
}

// Set by InitDetour when the compiled CBaseClient mirror disagrees with the
// engine's real field layout (read from the SetSignonState prologue). Every
// direct field access (m_nSignonState slot-scans, m_NetChannel guards, the
// m_SteamID compare in ClientFindFromSteamID) would hit the wrong memory -
// on GMod x64 build 260709 a stale mirror made occupied slots scan as free,
// relocating queue clients onto live players (the 2026-07-10 crash class).
// With this set we refuse to create/park queue clients entirely.
static bool g_bClientLayoutMismatch = false;

double net_time;
class SVC_CustomMessage : public CNetMessage
{
public:
	bool			ReadFromBuffer( bf_read &buffer ) { return true; };
	bool			WriteToBuffer( bf_write &buffer ) {
		if (m_iLength == -1)
			m_iLength = m_DataOut.GetNumBitsWritten();

		buffer.WriteUBitLong(GetType(), NETMSG_TYPE_BITS);

		if (m_iLengthBits != -1)
			buffer.WriteUBitLong(m_iLength, m_iLengthBits);

		return buffer.WriteBits(m_DataOut.GetData(), m_iLength);
	};
	const char		*ToString() const { return PROJECT_NAME ":CustomMessage"; };
	int				GetType() const { return m_iType; }
	const char		*GetName() const { return m_strName; }

	INetMessageHandler *m_pMessageHandler = nullptr;
	bool Process() { Warning(PROJECT_NAME ": Tried to process this message? This should never happen!\n"); return true; };

	SVC_CustomMessage() { m_bReliable = false; }

	int	GetGroup() const { return INetChannelInfo::GENERIC; }

	int m_iType = 0;
	int m_iLength = -1;
	char m_strName[64] = "";
	bf_write m_DataOut;
	int m_iLengthBits = -1;
};

PushReferenced_LuaClass(CBaseClient)
SpecialGet_LuaClass(CBaseClient, CHLTVClient, "CBaseClient", (gameserver_rawclients.GetBool() || pVar->IsConnected()))

Default__index(CBaseClient);
Default__newindex(CBaseClient);
Default__GetTable(CBaseClient);
Default__IsValidEXT(CBaseClient, if (!gameserver_rawclients.GetBool() && !pData->IsConnected()) { return false; } );

// While IsValid obeys gameserver_rawclients this function will allow one to know if the pointer is truly invalid
LUA_FUNCTION_STATIC(CBaseClient_IsInvalid)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, false, true);

	LUA->PushBool(!pClient);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetPlayerSlot)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushNumber(pClient->GetPlayerSlot());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetUserID)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushNumber(pClient->GetUserID());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetName)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushString(pClient->GetClientName());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetSteamID)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushString(pClient->GetNetworkIDString());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_Reconnect)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	pClient->Reconnect();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_ClientPrint)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	pClient->ClientPrintf(LUA->CheckString(2));
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_SendLua)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	const char* strLuaCode = LUA->CheckString(2);
	bool bForceReliable = LUA->GetBool(3);

	// NOTE: Original bug was that we had the wrong bitcount for the net messages type which broke every netmessage we created including this one.
	// It should work now, so let's test it later. (Never tested it ._., I should really try it once)
	SVC_UserMessage msg;
	msg.m_nMsgType = Util::pUserMessages->LookupUserMessage("LuaCmd");
	if (msg.m_nMsgType == -1)
	{
		LUA->PushBool(false);
		return 1;
	}

	byte pUserData[PAD_NUMBER(MAX_USER_MSG_DATA, 4)];
	msg.m_DataOut.StartWriting(pUserData, sizeof(pUserData));
	msg.m_DataOut.WriteString(strLuaCode);

	LUA->PushBool(pClient->SendNetMsg(msg, bForceReliable));
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_FireGameEvent)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
#if MODULE_EXISTS_GAMEEVENT
	IGameEvent* pEvent = Get_IGameEvent(LUA, 2, true);

	pClient->FireGameEvent(pEvent);
#else
	MISSING_MODULE_ERROR(LUA, gameevent);
#endif
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_GetFriendsID)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushNumber(pClient->GetFriendsID());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetFriendsName)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushString(pClient->GetFriendsName());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetClientChallenge)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushNumber(pClient->GetClientChallenge());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_SetReportThisFakeClient)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	bool bReport = LUA->GetBool(2);

	pClient->SetReportThisFakeClient(bReport);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_ShouldReportThisFakeClient)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushNumber(pClient->ShouldReportThisFakeClient());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_Inactivate)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	pClient->Inactivate();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_Disconnect)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	const char* strReason = LUA->CheckString(2);
	bool bSilent = LUA->GetBool(3);
	bool bNoEvent = LUA->GetBool(4);

	if (bSilent)
		pClient->GetNetChannel()->Shutdown(nullptr); // nullptr = Send no disconnect message

	if (bNoEvent)
		Util::BlockGameEvent("player_disconnect");

	pClient->Disconnect(strReason);

	if (bNoEvent)
		Util::UnblockGameEvent("player_disconnect");

	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_SetRate)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	int nRate = (int)LUA->CheckNumber(2);
	bool bForce = LUA->GetBool(3);

	pClient->SetRate(nRate, bForce);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_GetRate)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushNumber(pClient->GetRate());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_SetUpdateRate)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	int nUpdateRate = (int)LUA->CheckNumber(2);
	bool bForce = LUA->GetBool(3);

	pClient->SetUpdateRate(nUpdateRate, bForce);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_GetUpdateRate)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushNumber(pClient->GetUpdateRate());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_Clear)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	pClient->Clear();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_DemoRestart)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	pClient->DemoRestart();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_GetMaxAckTickCount)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushNumber(pClient->GetMaxAckTickCount());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_ExecuteStringCommand)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	const char* strCommand = LUA->CheckString(2);

	LUA->PushBool(pClient->ExecuteStringCommand(strCommand));
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_SendNetMsg)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	int iType = (int)LUA->CheckNumber(2);
	const char* strName = LUA->CheckString(3);

	SVC_CustomMessage msg;
	msg.m_iType = iType;
	strncpy(msg.m_strName, strName, sizeof(msg.m_strName));

#if MODULE_EXISTS_BITBUF
	bf_write* bf = Get_bf_write(LUA, 4, true);

	if (bf->IsOverflowed())
		LUA->ArgError(4, "Tried to use a buffer that is overflowed!");

	msg.m_DataOut.StartWriting(bf->GetData(), 0, 0, bf->GetMaxNumBits());
	msg.m_iLength = bf->GetNumBitsWritten();
#else
	size_t nLength;
	const char* pData = Util::CheckLString(LUA, 4, &nLength);

	msg.m_DataOut.StartWriting((void*)pData, nLength);
	msg.m_iLength = nLength * 8;
#endif

	LUA->PushBool(pClient->SendNetMsg(msg));
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_IsConnected)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushBool(pClient->IsConnected());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_IsSpawned)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushBool(pClient->IsSpawned());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_IsActive)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushBool(pClient->IsActive());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetSignonState)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushNumber(pClient->m_nSignonState);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_IsFakeClient)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushBool(pClient->IsFakeClient());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_IsHLTV)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushBool(pClient->IsHLTV());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_IsHearingClient)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	int nPlayerSlot = (int)LUA->CheckNumber(2);
	const int nRealPlayerSlots = gpGlobals ? MIN(gpGlobals->maxClients, MAX_PLAYERS) : 0;

	if (pClient->GetPlayerSlot() < 0 || pClient->GetPlayerSlot() >= nRealPlayerSlots ||
		nPlayerSlot < 0 || nPlayerSlot >= nRealPlayerSlots)
	{
		LUA->PushBool(false);
		return 1;
	}

	LUA->PushBool(pClient->IsHearingClient(nPlayerSlot));
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_IsProximityHearingClient)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	int nPlayerSlot = (int)LUA->CheckNumber(2);
	const int nRealPlayerSlots = gpGlobals ? MIN(gpGlobals->maxClients, MAX_PLAYERS) : 0;

	if (pClient->GetPlayerSlot() < 0 || pClient->GetPlayerSlot() >= nRealPlayerSlots ||
		nPlayerSlot < 0 || nPlayerSlot >= nRealPlayerSlots)
	{
		LUA->PushBool(false);
		return 1;
	}

	LUA->PushBool(pClient->IsProximityHearingClient(nPlayerSlot));
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_SetMaxRoutablePayloadSize)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	int nMaxRoutablePayloadSize = (int)LUA->CheckNumber(2);

	pClient->SetMaxRoutablePayloadSize(nMaxRoutablePayloadSize);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_UpdateAcknowledgedFramecount)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	int nTick = (int)LUA->CheckNumber(2);

	LUA->PushBool(pClient->UpdateAcknowledgedFramecount(nTick));
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_ShouldSendMessages)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushBool(pClient->ShouldSendMessages());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_UpdateSendState)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	pClient->UpdateSendState();
	return 0;
}

// Not doing FillUserInfo since it's useless

LUA_FUNCTION_STATIC(CBaseClient_UpdateUserSettings)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	pClient->UpdateUserSettings();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_SetSignonState) // At some point will replace HolyLib.SetSignOnState
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	int iSignOnState = (int)LUA->CheckNumber(2);
	int iSpawnCount = (int)LUA->GetNumber(3);
	bool bRawSet = LUA->GetBool(4);

	if (!pClient)
	{
		LUA->PushBool(false);
		return 1;
	}

	if (bRawSet)
	{
		pClient->m_nSignonState = iSignOnState;
		LUA->PushBool(true);
		return 1;
	}

	LUA->PushBool(pClient->SetSignonState(iSignOnState, iSpawnCount));
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_WriteGameSounds)
{
#if MODULE_EXISTS_BITBUF
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	bf_write* bf = Get_bf_write(LUA, 2, true);

	pClient->WriteGameSounds(*bf);
#else
	MISSING_MODULE_ERROR(LUA, bitbuf);
#endif
	return 0;
}

/*LUA_FUNCTION_STATIC(CBaseClient_GetDeltaFrame)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	int nTick = LUA->CheckNumber(2);

	pClient->GetDeltaFrame(nTick);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_SendSnapshot)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	pClient->SendSnapshot(nullptr);
	return 0;
}*/

LUA_FUNCTION_STATIC(CBaseClient_SendServerInfo)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	pClient->SendServerInfo();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_FillServerInfo)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	int clientIndex = (int)LUA->CheckNumberOpt(2, -1);
	if (clientIndex != -1 && !g_pModuleManager.IsUnsafeCodeEnabled() && !(clientIndex > 0 && clientIndex <= gpGlobals->maxClients))
		LUA->ArgError(2, "client index is out of range!");

	SVC_ServerInfo info;
	CBaseServer* pServer = (CBaseServer*)pClient->GetServer();
	pServer->FillServerInfo(info);

	if (clientIndex != -1)
		info.m_nPlayerSlot = clientIndex;

	pClient->SendNetMsg(info, true);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_SendSignonData)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	pClient->SendSignonData();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_SpawnPlayer)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	pClient->SpawnPlayer();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_ActivatePlayer)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	pClient->ActivatePlayer();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_SetName)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	const char* strName = LUA->CheckString(2);

	pClient->SetName(strName);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_SetUserCVar)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	const char* strName = LUA->CheckString(2);
	const char* strValue = LUA->CheckString(3);

	pClient->SetUserCVar(strName, strValue);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_FreeBaselines)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	pClient->FreeBaselines();
	return 0;
}

static Symbols::CBaseClient_OnRequestFullUpdate func_CBaseClient_OnRequestFullUpdate;
LUA_FUNCTION_STATIC(CBaseClient_OnRequestFullUpdate)
{
	if (!func_CBaseClient_OnRequestFullUpdate)
		LUA->ThrowError("Failed to load CBaseClient::OnRequestFullUpdate");

	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	func_CBaseClient_OnRequestFullUpdate(pClient);

	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_SetSteamID)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	const char* steamID64 = LUA->CheckString(2);
	uint64 steamID = strtoull(steamID64, nullptr, 0);

	if (steamID == 0)
	{
		LUA->PushBool(false);
		return 1;
	}

	pClient->SetSteamID(CSteamID(steamID));
	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_HasNetChannel)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	LUA->PushBool(pClient->GetNetChannel() != nullptr);
	return 1;
}

struct QueuePromotionResult
{
	bool ok;
	bool retryable;
	int newSlot;
	const char* reason;
};

static QueuePromotionResult PromoteQueueClient(CGameClient* origin);
static QueuePromotionResult PromoteQueueClientIntoTarget(CGameClient* origin, CGameClient* target);

static int PushQueuePromotionResult(GarrysMod::Lua::ILuaInterface* pLua, const QueuePromotionResult& result)
{
	pLua->PushBool(result.ok);
	if (result.newSlot >= 0)
		pLua->PushNumber(result.newSlot);
	else
		pLua->PushNil();

	pLua->PushBool(result.retryable);
	if (result.reason)
		pLua->PushString(result.reason);
	else
		pLua->PushNil();

	return 4;
}

LUA_FUNCTION_STATIC(CBaseClient_PromoteFromQueue)
{
	// Ignore the regular IsValid/IsConnected filter here so the native API can
	// fail closed with a stable reason instead of raising a Lua argument error.
	// Pointer identity is established against g_pQueueClients before the
	// promotion implementation dereferences the candidate.
	LuaUserData* pData = Get_CBaseClient_Data(LUA, 1, false);
	CBaseClient* pClient = pData ? (CBaseClient*)pData->GetData() : nullptr;
	return PushQueuePromotionResult(LUA, PromoteQueueClient((CGameClient*)pClient));
}

LUA_FUNCTION_STATIC(CBaseClient_MoveIntoClient)
{
	Util::DoUnsafeCodeCheck(LUA);

	CBaseClient* pSourceClient = Get_CBaseClient(LUA, 1, true);
	CBaseClient* pTargetClient = Get_CBaseClient(LUA, 2, true);

	if (pSourceClient->GetServer()->IsHLTV())
		LUA->ArgError(1, "the source client is a HLTV client!");

	if (pTargetClient->GetServer()->IsHLTV())
		LUA->ArgError(1, "the target client is a HLTV client!");

	const QueuePromotionResult result = PromoteQueueClientIntoTarget(
		(CGameClient*)pSourceClient,
		(CGameClient*)pTargetClient
	);
	if (!result.ok)
	{
		Warning(PROJECT_NAME " - gameserver: legacy MoveIntoClient rejected queue promotion (%s)\n",
			result.reason ? result.reason : "unknown");
	}
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_AddToQueueList)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	if (pClient->GetServer()->IsHLTV())
		LUA->ArgError(1, "the client is a HLTV client!");

	CBaseServer* pServer = (CBaseServer*)Util::server;
	pServer->m_Clients.FindAndRemove(pClient);
	if (std::find(g_pQueueClients.begin(), g_pQueueClients.end(), (CGameClient*)pClient) == g_pQueueClients.end())
		g_pQueueClients.push_back((CGameClient*)pClient);

	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_AddToServerList)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	if (pClient->GetServer()->IsHLTV())
		LUA->ArgError(1, "the client is a HLTV client!");

	CBaseServer* pServer = (CBaseServer*)Util::server;
	if (pServer->m_Clients.Find(pClient) == -1)
		pServer->m_Clients.AddToTail(pClient);

	auto it = std::find(g_pQueueClients.begin(), g_pQueueClients.end(), (CGameClient*)pClient);
	if (it != g_pQueueClients.end())
		g_pQueueClients.erase(it);

	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_RemoveFromAllLists)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	if (pClient->GetServer()->IsHLTV())
		LUA->ArgError(1, "the client is a HLTV client!");

	CBaseServer* pServer = (CBaseServer*)Util::server;
	pServer->m_Clients.FindAndRemove(pClient);
	auto it = std::find(g_pQueueClients.begin(), g_pQueueClients.end(), (CGameClient*)pClient);
	if (it != g_pQueueClients.end())
		g_pQueueClients.erase(it);

	return 0;
}

/*
 * CNetChannel exposed things.
 * I should probably move it into a separate class...
 */
LUA_FUNCTION_STATIC(CBaseClient_GetProcessingMessages)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushBool(pNetChannel->m_bProcessingMessages);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetClearedDuringProcessing)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushBool(pNetChannel->m_bClearedDuringProcessing);
	return 1;
}

// If anyone sees a point in having this function, open a issue and ask for it to be added.
/*LUA_FUNCTION_STATIC(CBaseClient_GetShouldDelete)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushBool(pNetChannel->m_bShouldDelete);
	return 1;
}*/

LUA_FUNCTION_STATIC(CBaseClient_GetOutSequenceNr)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_nOutSequenceNr);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetInSequenceNr)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_nInSequenceNr);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetOutSequenceNrAck)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_nOutSequenceNrAck);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetOutReliableState)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_nOutReliableState);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetInReliableState)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_nInReliableState);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetChokedPackets)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_nChokedPackets);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetStreamReliable)
{
#if MODULE_EXISTS_BITBUF
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	Push_bf_write(LUA, &pNetChannel->m_StreamReliable, false);
#else
	MISSING_MODULE_ERROR(LUA, bitbuf);
#endif
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetStreamUnreliable)
{
#if MODULE_EXISTS_BITBUF
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	Push_bf_write(LUA, &pNetChannel->m_StreamUnreliable, false);
#else
	MISSING_MODULE_ERROR(LUA, bitbuf);
#endif
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetStreamVoice)
{
#if MODULE_EXISTS_BITBUF
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	Push_bf_write(LUA, &pNetChannel->m_StreamVoice, false);
#else
	MISSING_MODULE_ERROR(LUA, bitbuf);
#endif
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetStreamSocket)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_StreamSocket);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetMaxReliablePayloadSize)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_MaxReliablePayloadSize);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetLastReceived)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushNumber(pNetChannel->last_received);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetConnectTime)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushNumber(pNetChannel->connect_time);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetClearTime)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_fClearTime);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetTimeout)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_Timeout);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_SetTimeout)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);
	float seconds = (float)LUA->CheckNumber(2);

	pNetChannel->SetTimeout(seconds);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_GetRemoteFramerate)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	float framerate, deviation;
	pNetChannel->GetRemoteFramerate(&framerate, &deviation);

	LUA->PushNumber(framerate);
	LUA->PushNumber(deviation);

	return 2;
}

static bool g_bFreeSubChannels = false;
LUA_FUNCTION_STATIC(CBaseClient_Transmit)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);
	bool bOnlyReliable = LUA->GetBool(2);
	bool bFreeSubChannels = LUA->GetBool(3);

	g_bFreeSubChannels = bFreeSubChannels;
	LUA->PushBool(pNetChannel->Transmit(bOnlyReliable));
	g_bFreeSubChannels = false;

	return 1;
}

/*LUA_FUNCTION_STATIC(CBaseClient_HasQueuedPackets)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushBool(pNetChannel->HasQueuedPackets());
	return 1;
}*/

LUA_FUNCTION_STATIC(CBaseClient_ProcessStream)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushBool(pNetChannel->ProcessStream());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_SetMaxBufferSize)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);
	bool bReliable = LUA->GetBool(2);
	int nBytes = (int)LUA->CheckNumber(3);
	bool bVoice = LUA->GetBool(4);

	pNetChannel->SetMaxBufferSize(bReliable, nBytes, bVoice);
	return 0;
}

// Purely debug function, has no real use.
/*LUA_FUNCTION_STATIC(CBaseClient_GetRegisteredMessages)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PreCreateTable(pNetChannel->m_NetMessages.Count(), 0);
		int idx = 0;
		for (int i=0 ; i< pNetChannel->m_NetMessages.Count(); i++ )
		{
			INetMessage* msg = pNetChannel->m_NetMessages[ i ];
			LUA->PushString(msg->GetName());
			Util::RawSetI(LUA, -2, msg->GetType());
		}

	return 1;
}*/

LUA_FUNCTION_STATIC(CBaseClient_GetMaxRoutablePayloadSize)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushNumber(pNetChannel->GetMaxRoutablePayloadSize());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetTimeConnected)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushNumber(pNetChannel->GetTimeConnected());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetAvgLatency)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);
	int flow = (int)LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetAvgLatency(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetAvgLoss)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);
	int flow = (int)LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetAvgLoss(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetAddress)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushString(pNetChannel->GetAddress());
	return 1;
}

// Added for CHLTVClient to inherit functions.
void Push_CBaseClientMeta(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::AddFunc(pLua, CBaseClient__newindex, "__newindex");
	Util::AddFunc(pLua, CBaseClient__index, "__index");
	LUA_REGISTER_JIT(pLua, CBaseClient_GetTable, "GetTable");
	LUA_REGISTER_JIT(pLua, CBaseClient_IsValid, "IsValid");
	Util::AddFunc(pLua, CBaseClient_IsInvalid, "IsInvalid");

	Util::AddFunc(pLua, CBaseClient_GetPlayerSlot, "GetPlayerSlot");
	Util::AddFunc(pLua, CBaseClient_GetUserID, "GetUserID");
	Util::AddFunc(pLua, CBaseClient_GetName, "GetName");
	Util::AddFunc(pLua, CBaseClient_GetSteamID, "GetSteamID");
	Util::AddFunc(pLua, CBaseClient_Reconnect, "Reconnect");
	Util::AddFunc(pLua, CBaseClient_ClientPrint, "ClientPrint");
	Util::AddFunc(pLua, CBaseClient_SendLua, "SendLua");
	Util::AddFunc(pLua, CBaseClient_FireGameEvent, "FireGameEvent");
	Util::AddFunc(pLua, CBaseClient_GetFriendsID, "GetFriendsID");
	Util::AddFunc(pLua, CBaseClient_GetFriendsName, "GetFriendsName");
	Util::AddFunc(pLua, CBaseClient_GetClientChallenge, "GetClientChallenge");
	Util::AddFunc(pLua, CBaseClient_SetReportThisFakeClient, "SetReportThisFakeClient");
	Util::AddFunc(pLua, CBaseClient_ShouldReportThisFakeClient, "ShouldReportThisFakeClient");
	Util::AddFunc(pLua, CBaseClient_Inactivate, "Inactivate");
	Util::AddFunc(pLua, CBaseClient_Disconnect, "Disconnect");
	Util::AddFunc(pLua, CBaseClient_SetRate, "SetRate");
	Util::AddFunc(pLua, CBaseClient_GetRate, "GetRate");
	Util::AddFunc(pLua, CBaseClient_SetUpdateRate, "SetUpdateRate");
	Util::AddFunc(pLua, CBaseClient_GetUpdateRate, "GetUpdateRate");
	Util::AddFunc(pLua, CBaseClient_Clear, "Clear");
	Util::AddFunc(pLua, CBaseClient_DemoRestart, "DemoRestart");
	Util::AddFunc(pLua, CBaseClient_GetMaxAckTickCount, "GetMaxAckTickCount");
	Util::AddFunc(pLua, CBaseClient_ExecuteStringCommand, "ExecuteStringCommand");
	Util::AddFunc(pLua, CBaseClient_SendNetMsg, "SendNetMsg");
	Util::AddFunc(pLua, CBaseClient_IsConnected, "IsConnected");
	Util::AddFunc(pLua, CBaseClient_IsSpawned, "IsSpawned");
	Util::AddFunc(pLua, CBaseClient_IsActive, "IsActive");
	Util::AddFunc(pLua, CBaseClient_GetSignonState, "GetSignonState");
	Util::AddFunc(pLua, CBaseClient_IsFakeClient, "IsFakeClient");
	Util::AddFunc(pLua, CBaseClient_IsHLTV, "IsHLTV");
	Util::AddFunc(pLua, CBaseClient_IsHearingClient, "IsHearingClient");
	Util::AddFunc(pLua, CBaseClient_IsProximityHearingClient, "IsProximityHearingClient");
	Util::AddFunc(pLua, CBaseClient_SetMaxRoutablePayloadSize, "SetMaxRoutablePayloadSize");
	Util::AddFunc(pLua, CBaseClient_UpdateAcknowledgedFramecount, "UpdateAcknowledgedFramecount");
	Util::AddFunc(pLua, CBaseClient_ShouldSendMessages, "ShouldSendMessages");
	Util::AddFunc(pLua, CBaseClient_UpdateSendState, "UpdateSendState");
	Util::AddFunc(pLua, CBaseClient_UpdateUserSettings, "UpdateUserSettings");
	Util::AddFunc(pLua, CBaseClient_SetSignonState, "SetSignonState");
	Util::AddFunc(pLua, CBaseClient_WriteGameSounds, "WriteGameSounds");
	Util::AddFunc(pLua, CBaseClient_SendServerInfo, "SendServerInfo");
	Util::AddFunc(pLua, CBaseClient_FillServerInfo, "FillServerInfo");
	Util::AddFunc(pLua, CBaseClient_SendSignonData, "SendSignonData");
	Util::AddFunc(pLua, CBaseClient_SpawnPlayer, "SpawnPlayer");
	Util::AddFunc(pLua, CBaseClient_ActivatePlayer, "ActivatePlayer");
	Util::AddFunc(pLua, CBaseClient_SetName, "SetName");
	Util::AddFunc(pLua, CBaseClient_SetUserCVar, "SetUserCVar");
	Util::AddFunc(pLua, CBaseClient_FreeBaselines, "FreeBaselines");
	Util::AddFunc(pLua, CBaseClient_OnRequestFullUpdate, "OnRequestFullUpdate");
	Util::AddFunc(pLua, CBaseClient_SetSteamID, "SetSteamID");
	Util::AddFunc(pLua, CBaseClient_HasNetChannel, "HasNetChannel");
	Util::AddFunc(pLua, CBaseClient_PromoteFromQueue, "PromoteFromQueue");
	Util::AddFunc(pLua, CBaseClient_MoveIntoClient, "MoveIntoClient");
	Util::AddFunc(pLua, CBaseClient_AddToQueueList, "AddToQueueList");
	Util::AddFunc(pLua, CBaseClient_AddToServerList, "AddToServerList");
	Util::AddFunc(pLua, CBaseClient_RemoveFromAllLists, "RemoveFromAllLists");

	// CNetChan related functions
	Util::AddFunc(pLua, CBaseClient_GetProcessingMessages, "GetProcessingMessages");
	Util::AddFunc(pLua, CBaseClient_GetClearedDuringProcessing, "GetClearedDuringProcessing");
	//Util::AddFunc(pLua, CBaseClient_GetShouldDelete, "GetShouldDelete");
	Util::AddFunc(pLua, CBaseClient_GetOutSequenceNr, "GetOutSequenceNr");
	Util::AddFunc(pLua, CBaseClient_GetInSequenceNr, "GetInSequenceNr");
	Util::AddFunc(pLua, CBaseClient_GetOutSequenceNrAck, "GetOutSequenceNrAck");
	Util::AddFunc(pLua, CBaseClient_GetOutReliableState, "GetOutReliableState");
	Util::AddFunc(pLua, CBaseClient_GetInReliableState, "GetInReliableState");
	Util::AddFunc(pLua, CBaseClient_GetChokedPackets, "GetChokedPackets");
	Util::AddFunc(pLua, CBaseClient_GetStreamReliable, "GetStreamReliable");
	Util::AddFunc(pLua, CBaseClient_GetStreamUnreliable, "GetStreamUnreliable");
	Util::AddFunc(pLua, CBaseClient_GetStreamVoice, "GetStreamVoice");
	Util::AddFunc(pLua, CBaseClient_GetStreamSocket, "GetStreamSocket");
	Util::AddFunc(pLua, CBaseClient_GetMaxReliablePayloadSize, "GetMaxReliablePayloadSize");
	Util::AddFunc(pLua, CBaseClient_GetLastReceived, "GetLastReceived");
	Util::AddFunc(pLua, CBaseClient_GetConnectTime, "GetConnectTime");
	Util::AddFunc(pLua, CBaseClient_GetClearTime, "GetClearTime");
	Util::AddFunc(pLua, CBaseClient_GetTimeout, "GetTimeout");
	Util::AddFunc(pLua, CBaseClient_SetTimeout, "SetTimeout");
	Util::AddFunc(pLua, CBaseClient_GetRemoteFramerate, "GetRemoteFramerate");
	Util::AddFunc(pLua, CBaseClient_Transmit, "Transmit");
	Util::AddFunc(pLua, CBaseClient_ProcessStream, "ProcessStream");
	//Util::AddFunc(pLua, CBaseClient_GetRegisteredMessages, "GetRegisteredMessages");
	Util::AddFunc(pLua, CBaseClient_SetMaxBufferSize, "SetMaxBufferSize");
	//Util::AddFunc(pLua, CBaseClient_HasQueuedPackets, "HasQueuedPackets");
	Util::AddFunc(pLua, CBaseClient_GetMaxRoutablePayloadSize, "GetMaxRoutablePayloadSize");
	Util::AddFunc(pLua, CBaseClient_GetTimeConnected, "GetTimeConnected");
	Util::AddFunc(pLua, CBaseClient_GetAvgLatency, "GetAvgLatency");
	Util::AddFunc(pLua, CBaseClient_GetAvgLoss, "GetAvgLoss");
	Util::AddFunc(pLua, CBaseClient_GetAddress, "GetAddress");
}

LUA_FUNCTION_STATIC(CGameClient__tostring)
{
	CGameClient* pClient = (CGameClient*)Get_CBaseClient(LUA, 1, false);
	if (!pClient || !pClient->IsConnected())
	{
		// I removed the gameserver_rawclients check just to make things easier for developers :)
		if (pClient /*&& gameserver_rawclients.GetBool()*/)
			LUA->PushString("GameClient [EMPTY]");
		else
			LUA->PushString("GameClient [NULL]");
	} else {
		char szBuf[128] = {};
		V_snprintf(szBuf, sizeof(szBuf),"GameClient [%i][%s]", pClient->GetPlayerSlot(), pClient->GetClientName());
		LUA->PushString(szBuf);
	}

	return 1;
}

PushReferenced_LuaClass(CNetChan)
Get_LuaClass(CNetChan, "CNetChan")

Default__index(CNetChan);
Default__newindex(CNetChan);
Default__GetTable(CNetChan);
Default__IsValid(CNetChan);

LUA_FUNCTION_STATIC(CNetChan__tostring)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, false);
	if (!pNetChannel)
	{
		LUA->PushString("CNetChan [NULL]");
	} else {
		char szBuf[128] = {};
		V_snprintf(szBuf, sizeof(szBuf),"CNetChan [%s]", pNetChannel->GetName());
		LUA->PushString(szBuf);
	}

	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetAvgLoss)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	int flow = (int)LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetAvgLoss(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetAvgChoke)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	int flow = (int)LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetAvgChoke(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetAvgData)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	int flow = (int)LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetAvgData(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetAvgLatency)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	int flow = (int)LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetAvgLatency(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetAvgPackets)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	int flow = (int)LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetAvgPackets(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetChallengeNr)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->GetChallengeNr());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetAddress)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushString(pNetChannel->GetAddress());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetDataRate)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->GetDataRate());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetDropNumber)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->GetDropNumber());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_SetChoked)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	pNetChannel->SetChoked();
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_SetFileTransmissionMode)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	bool bBackgroundMode = LUA->GetBool(2);

	pNetChannel->SetFileTransmissionMode(bBackgroundMode);
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_SetCompressionMode)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	bool bCompression = LUA->GetBool(2);

	pNetChannel->SetCompressionMode(bCompression);
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_SetDataRate)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	float rate = (float)LUA->CheckNumber(2);

	pNetChannel->SetDataRate(rate);
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_GetTime)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->GetTime());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetTimeConnected)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->GetTimeConnected());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetTimeoutSeconds)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->GetTimeoutSeconds());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetTimeSinceLastReceived)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->GetTimeSinceLastReceived());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetTotalData)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	int flow = (int)LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetTotalData(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetBufferSize)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->GetBufferSize());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetProtocolVersion)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->GetProtocolVersion());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetName)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushString(pNetChannel->GetName());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetProcessingMessages)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushBool(pNetChannel->m_bProcessingMessages);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetClearedDuringProcessing)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushBool(pNetChannel->m_bClearedDuringProcessing);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetOutSequenceNr)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_nOutSequenceNr);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetInSequenceNr)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_nInSequenceNr);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetOutSequenceNrAck)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_nOutSequenceNrAck);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetOutReliableState)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_nOutReliableState);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetInReliableState)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_nInReliableState);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetChokedPackets)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_nChokedPackets);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetStreamReliable)
{
#if MODULE_EXISTS_BITBUF
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	Push_bf_write(LUA, &pNetChannel->m_StreamReliable, false);
#else
	MISSING_MODULE_ERROR(LUA, bitbuf);
#endif
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetStreamUnreliable)
{
#if MODULE_EXISTS_BITBUF
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	Push_bf_write(LUA, &pNetChannel->m_StreamUnreliable, false);
#else
	MISSING_MODULE_ERROR(LUA, bitbuf);
#endif
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetStreamVoice)
{
#if MODULE_EXISTS_BITBUF
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	Push_bf_write(LUA, &pNetChannel->m_StreamVoice, false);
#else
	MISSING_MODULE_ERROR(LUA, bitbuf);
#endif
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetStreamSocket)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_StreamSocket);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetMaxReliablePayloadSize)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_MaxReliablePayloadSize);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetLastReceived)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->last_received);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetConnectTime)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->connect_time);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetClearTime)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_fClearTime);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetTimeout)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->m_Timeout);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_SetTimeout)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	float seconds = (float)LUA->CheckNumber(2);

	pNetChannel->SetTimeout(seconds);
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_GetRate)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->GetDataRate());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_SetRate)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	float rate = (float)LUA->CheckNumber(2);

	pNetChannel->SetDataRate(rate);
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_GetRemoteFramerate)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	float framerate, deviation;
	pNetChannel->GetRemoteFramerate(&framerate, &deviation);

	LUA->PushNumber(framerate);
	LUA->PushNumber(deviation);

	return 2;
}

LUA_FUNCTION_STATIC(CNetChan_Transmit)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	bool bOnlyReliable = LUA->GetBool(2);

	LUA->PushBool(pNetChannel->Transmit(bOnlyReliable));

	return 1;
}

/*LUA_FUNCTION_STATIC(CNetChan_HasQueuedPackets)
{
	CNetChan* pNetChannel = (CNetChan*)Util::Get_NetChannel(LUA, 1, true);

	LUA->PushBool(pNetChannel->HasQueuedPackets());
	return 1;
}*/

LUA_FUNCTION_STATIC(CNetChan_ProcessStream)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushBool(pNetChannel->ProcessStream());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_SetMaxBufferSize)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	bool bReliable = LUA->GetBool(2);
	int nBytes = (int)LUA->CheckNumber(3);
	bool bVoice = LUA->GetBool(4);

	pNetChannel->SetMaxBufferSize(bReliable, nBytes, bVoice);
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_GetMaxRoutablePayloadSize)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	LUA->PushNumber(pNetChannel->GetMaxRoutablePayloadSize());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_Shutdown)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	const char* reason = LUA->CheckStringOpt(2, nullptr);

	pNetChannel->Shutdown(reason);
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_CanPacket)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	net_time = Util::engineserver->Time();
	LUA->PushBool(pNetChannel->CanPacket());
	return 1;
}

static Detouring::Hook detour_CNetChan_D2;
static void hook_CNetChan_D2(CNetChan* pNetChan)
{
	if (!ThreadInMainThread())
	{
		Warning(PROJECT_NAME ": CNetChan was deleted from another thread...\n");
		return;
	}

	if (g_Lua)
		Delete_CNetChan(g_Lua, pNetChan);

	/*
	 * Scrub every client-side reference to the dying channel by pointer identity.
	 * Queue clients live outside m_Clients, so no engine bookkeeping ever nulls
	 * their m_NetChannel - any alias left behind (relocation windows, teardown
	 * paths racing Lua, reconnect churn) becomes a dangling pointer that
	 * CGameServerModule::Think later dispatches through a stale vtable (the x64
	 * queue-servicing UAF: wild jumps, rip=0 / rip inside libtier0). A client
	 * mid-Disconnect still points at its channel while it destructs - nulling
	 * early there is harmless; keeping a stale pointer anywhere else is fatal.
	 * pNetChan is mid-teardown and must never be dereferenced here.
	 */
	for (CGameClient* pClient : g_pQueueClients)
	{
		if (pClient->m_NetChannel == (INetChannel*)pNetChan)
		{
			if (g_pGameServerModule.InDebug())
				Msg(PROJECT_NAME " - gameserver: scrubbed dying netchannel off queue client (slot %i)\n", pClient->m_nClientSlot);

			pClient->m_NetChannel = nullptr;
		}
	}

	if (Util::server)
	{
		int count = Util::server->GetClientCount();
		for (int i = 0; i < count; ++i)
		{
			CBaseClient* pClient = (CBaseClient*)Util::server->GetClient(i);
			if (pClient->m_NetChannel == (INetChannel*)pNetChan)
				pClient->m_NetChannel = nullptr;
		}
	}

	detour_CNetChan_D2.GetTrampoline<Symbols::CNetChan_D2>()(pNetChan);
}

class NET_LuaNetChanMessage;
class ILuaNetMessageHandler : INetChannelHandler
{
public:
	ILuaNetMessageHandler(GarrysMod::Lua::ILuaInterface* pLua);
	~ILuaNetMessageHandler();

	void ConnectionStart(INetChannel *chan);	// called first time network channel is established
	void ConnectionClosing(const char *reason); // network channel is being closed by remote site
	void ConnectionCrashed(const char *reason); // network error occurred
	void PacketStart(int incoming_sequence, int outgoing_acknowledged);	// called each time a new packet arrived
	void PacketEnd(void); // all messages has been parsed
	void FileRequested(const char *fileName, unsigned int transferID ); // other side request a file for download
	void FileReceived(const char *fileName, unsigned int transferID ); // we received a file
	void FileDenied(const char *fileName, unsigned int transferID );	// a file request was denied by other side
	void FileSent(const char *fileName, unsigned int transferID );	// we sent a file
	bool ShouldAcceptFile(const char *fileName, unsigned int transferID);

	bool ProcessLuaNetChanMessage( [[maybe_unused]] NET_LuaNetChanMessage *msg );

public:
	CNetChan* m_pChan = nullptr;
	NET_LuaNetChanMessage* m_pLuaNetChanMessage = nullptr;
	int m_iMessageCallbackFunction = -1;
	int m_iConnectionStartFunction = -1;
	int m_iConnectionClosingFunction = -1;
	int m_iConnectionCrashedFunction = -1;
	int m_iPacketStartFunction = -1;
	int m_iPacketEndFunction = -1;
	int m_iFileRequestedFunction = -1;
	int m_iFileReceivedFunction = -1;
	int m_iFileDeniedFunction = -1;
	int m_iFileSentFunction = -1;
	int m_iShouldAcceptFileFunction = -1;
	GarrysMod::Lua::ILuaInterface* m_pLua;
};

#define net_LuaNetChanMessage 33
class NET_LuaNetChanMessage : public CNetMessage
{
public:
	bool ReadFromBuffer( bf_read &buffer )
	{
		//Msg("NET_LuaNetChanMessage::ReadFromBuffer\n");
		m_iLength = buffer.ReadUBitLong( 32 );
		m_DataIn = buffer;

		return buffer.SeekRelative( m_iLength );
	};

	bool WriteToBuffer( bf_write &buffer )
	{
		//Msg("NET_LuaNetChanMessage::WriteToBuffer\n");
		if ( m_iLength == -1 )
			m_iLength = m_DataOut.GetNumBitsWritten();

		buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
		buffer.WriteUBitLong( m_iLength, 32);
		return buffer.WriteBits( m_DataOut.GetData(), m_iLength );
	};

	const char *ToString() const { return PROJECT_NAME ":LuaNetChanMessage"; };
	int GetType() const { return net_LuaNetChanMessage; }
	const char *GetName() const { return "NET_LuaNetChanMessage"; }

	ILuaNetMessageHandler *m_pMessageHandler = nullptr;
	bool Process() { return m_pMessageHandler->ProcessLuaNetChanMessage( this ); };

	NET_LuaNetChanMessage() { m_bReliable = true; }

	int	GetGroup() const { return INetChannelInfo::GENERIC; }

	int m_iLength = -1;
	bf_write m_DataOut;
	bf_read m_DataIn;
};

static unordered_set<ILuaNetMessageHandler*> g_pNetMessageHandlers;
ILuaNetMessageHandler::ILuaNetMessageHandler(GarrysMod::Lua::ILuaInterface* pLua)
{
	m_pLuaNetChanMessage = new NET_LuaNetChanMessage;
	m_pLuaNetChanMessage->m_pMessageHandler = this;
	g_pNetMessageHandlers.insert(this);
	m_pLua = pLua;
}

#define HANDLER_FREE_LUA_REFERENCE(name) \
if (name != -1) \
{ \
	Util::ReferenceFree(m_pLua, name, "ILuaNetMessageHandler::~ILuaNetMessageHandler"); \
	name = -1; \
} \

ILuaNetMessageHandler::~ILuaNetMessageHandler()
{
	if (m_pLuaNetChanMessage)
	{
		delete m_pLuaNetChanMessage;
		m_pLuaNetChanMessage = nullptr;
	}

	g_pNetMessageHandlers.erase(this);

	if (!ThreadInMainThread())
	{
		Warning(PROJECT_NAME ": Tried to delete a ILuaNetMessageHandler from another thread! How could you! Now were leaking a reference...\n");
		return;
	}

	HANDLER_FREE_LUA_REFERENCE(m_iMessageCallbackFunction);
	HANDLER_FREE_LUA_REFERENCE(m_iConnectionStartFunction);
	HANDLER_FREE_LUA_REFERENCE(m_iConnectionClosingFunction);
	HANDLER_FREE_LUA_REFERENCE(m_iConnectionCrashedFunction);
	HANDLER_FREE_LUA_REFERENCE(m_iPacketStartFunction);
	HANDLER_FREE_LUA_REFERENCE(m_iPacketEndFunction);
	HANDLER_FREE_LUA_REFERENCE(m_iFileRequestedFunction);
	HANDLER_FREE_LUA_REFERENCE(m_iFileReceivedFunction);
	HANDLER_FREE_LUA_REFERENCE(m_iFileDeniedFunction);
	HANDLER_FREE_LUA_REFERENCE(m_iFileSentFunction);
	HANDLER_FREE_LUA_REFERENCE(m_iShouldAcceptFileFunction);
}

#define HANDLER_CALL_LUA_CALLBACK(name, returnVal) \
if (!ThreadInMainThread()) \
{ \
	Warning(PROJECT_NAME ": Trying to call " #name " outside the main thread!\n"); \
	return returnVal; \
} \
if (m_i##name##Function == -1) /*We have no callback function set. */ \
	return returnVal; \
m_pLua->ReferencePush(m_i##name##Function);

void ILuaNetMessageHandler::ConnectionStart(INetChannel* pChan)
{
	HANDLER_CALL_LUA_CALLBACK(ConnectionStart, )
	Push_CNetChan(m_pLua, (CNetChan*)pChan);
	m_pLua->CallFunctionProtected(1, 0, true);
}

void ILuaNetMessageHandler::ConnectionClosing(const char* reason)
{
	HANDLER_CALL_LUA_CALLBACK(ConnectionClosing, )
	Push_CNetChan(m_pLua, m_pChan);
	m_pLua->PushString(reason);
	m_pLua->CallFunctionProtected(2, 0, true);
}

void ILuaNetMessageHandler::ConnectionCrashed(const char* reason)
{
	HANDLER_CALL_LUA_CALLBACK(ConnectionCrashed, )
	Push_CNetChan(m_pLua, m_pChan);
	m_pLua->PushString(reason);
	m_pLua->CallFunctionProtected(2, 0, true);
}

void ILuaNetMessageHandler::PacketStart(int incoming_sequence, int outgoing_acknowledged)
{
	//Msg("ILuaNetMessageHandler::PacketStart - %i | %i\n", incoming_sequence, outgoing_acknowledged);
	HANDLER_CALL_LUA_CALLBACK(PacketStart, )
	Push_CNetChan(m_pLua, m_pChan);
	m_pLua->PushNumber(incoming_sequence);
	m_pLua->PushNumber(outgoing_acknowledged);
	m_pLua->CallFunctionProtected(3, 0, true);
}

void ILuaNetMessageHandler::PacketEnd()
{
	//Msg("ILuaNetMessageHandler::PacketEnd\n");
	HANDLER_CALL_LUA_CALLBACK(PacketEnd, )
	Push_CNetChan(m_pLua, m_pChan);
	m_pLua->CallFunctionProtected(1, 0, true);
}

void ILuaNetMessageHandler::FileRequested(const char *fileName, unsigned int transferID)
{
	//Msg("ILuaNetMessageHandler::FileRequested - %s | %d\n", fileName, transferID);
	HANDLER_CALL_LUA_CALLBACK(FileRequested, )
	Push_CNetChan(m_pLua, m_pChan);
	m_pLua->PushString(fileName);
	m_pLua->PushNumber(transferID);
	m_pLua->CallFunctionProtected(3, 0, true);
}

void ILuaNetMessageHandler::FileReceived(const char *fileName, unsigned int transferID)
{
	//Msg("ILuaNetMessageHandler::FileReceived - %s | %d\n", fileName, transferID);
	HANDLER_CALL_LUA_CALLBACK(FileReceived, )
	Push_CNetChan(m_pLua, m_pChan);
	m_pLua->PushString(fileName);
	m_pLua->PushNumber(transferID);
	m_pLua->CallFunctionProtected(3, 0, true);
}

void ILuaNetMessageHandler::FileDenied(const char *fileName, unsigned int transferID)
{
	//Msg("ILuaNetMessageHandler::FileDenied - %s | %d\n", fileName, transferID);
	HANDLER_CALL_LUA_CALLBACK(FileDenied, )
	Push_CNetChan(m_pLua, m_pChan);
	m_pLua->PushString(fileName);
	m_pLua->PushNumber(transferID);
	m_pLua->CallFunctionProtected(3, 0, true);
}

void ILuaNetMessageHandler::FileSent(const char *fileName, unsigned int transferID)
{
	//Msg("ILuaNetMessageHandler::FileSent - %s | %d\n", fileName, transferID);
	HANDLER_CALL_LUA_CALLBACK(FileSent, )
	Push_CNetChan(m_pLua, m_pChan);
	m_pLua->PushString(fileName);
	m_pLua->PushNumber(transferID);
	m_pLua->CallFunctionProtected(3, 0, true);
}

bool ILuaNetMessageHandler::ShouldAcceptFile(const char *fileName, unsigned int transferID)
{
	//Msg("ILuaNetMessageHandler::ShouldAcceptFile - %s | %d\n", fileName, transferID);
	HANDLER_CALL_LUA_CALLBACK(ShouldAcceptFile, false)
	Push_CNetChan(m_pLua, m_pChan);
	m_pLua->PushString(fileName);
	m_pLua->PushNumber(transferID);
	if (m_pLua->CallFunctionProtected(3, 1, true))
	{
		bool bAccept = m_pLua->GetBool(-1);
		m_pLua->Pop(1);
		return bAccept;
	}

	return false;
}

bool ILuaNetMessageHandler::ProcessLuaNetChanMessage(NET_LuaNetChanMessage *msg)
{
	if (!ThreadInMainThread())
	{
		Warning(PROJECT_NAME ": Trying to process a lua net channel message outside the main thread!\n");
		return false;
	}

	if (m_iMessageCallbackFunction == -1) // We have no callback function set.
		return true;

	m_pLua->ReferencePush(m_iMessageCallbackFunction);

	Push_CNetChan(m_pLua, m_pChan);
#if MODULE_EXISTS_BITBUF
	LuaUserData* pLuaData = Push_bf_read(m_pLua, &msg->m_DataIn, false);
#else
	m_pLua->PushString((const char*)msg->m_DataIn.GetBasePointer(), msg->m_DataIn.GetNumBytesLeft());
#endif
	m_pLua->PushNumber(msg->m_iLength);
	m_pLua->CallFunctionProtected(3, 0, true);

#if MODULE_EXISTS_BITBUF
	if (pLuaData)
		pLuaData->Release(m_pLua);
#endif

	return true;
}

LUA_FUNCTION_STATIC(CNetChan_SendMessage)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	bool bReliable = LUA->GetBool(3);

	NET_LuaNetChanMessage msg;
#if MODULE_EXISTS_BITBUF
	bf_write* bf = Get_bf_write(LUA, 2, true);

	if (bf->IsOverflowed())
		LUA->ArgError(2, "Tried to use a buffer that is overflowed!");

	msg.m_DataOut.StartWriting(bf->GetData(), 0, 0, bf->GetMaxNumBits());
	msg.m_iLength = bf->GetNumBitsWritten();
#else
	size_t nLength;
	const char* pData = Util::CheckLString(LUA, 2, &nLength);

	msg.m_DataOut.StartWriting((void*)pData, nLength);
	msg.m_iLength = nLength * 8;
#endif

	LUA->PushBool(pNetChannel->SendNetMsg(msg, bReliable));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_SendFile)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	const char* pFileName = LUA->CheckString(2);
	int transferID = (int)LUA->CheckNumber(3);

	LUA->PushBool(pNetChannel->SendFile(pFileName, transferID));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_RequestFile)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	const char* pFileName = LUA->CheckString(2);

	LUA->PushNumber(pNetChannel->RequestFile(pFileName));
	return 1;
}

#define HANDLER_DEFINE_CALLBACK_FUNCTION(name) \
LUA_FUNCTION_STATIC(CNetChan_Set##name) \
{ \
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true); \
	ILuaNetMessageHandler* pHandler = (ILuaNetMessageHandler*)pNetChannel->m_MessageHandler; \
	LUA->CheckType(2, GarrysMod::Lua::Type::Function); \
\
	if (!pHandler) \
		return 0; \
\
	if (pHandler->m_i##name##Function != -1) \
	{ \
		Util::ReferenceFree(LUA, pHandler->m_i##name##Function, "CNetChan:SetCallback"); \
	} \
\
	LUA->Push(2); \
	pHandler->m_i##name##Function = Util::ReferenceCreate(LUA, "CNetChan:SetCallback"); \
	return 0; \
} \
LUA_FUNCTION_STATIC(CNetChan_Get##name) \
{ \
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true); \
	ILuaNetMessageHandler* pHandler = (ILuaNetMessageHandler*)pNetChannel->m_MessageHandler; \
\
	if (pHandler && pHandler->m_i##name##Function != -1) \
	{ \
		Util::ReferencePush(LUA, pHandler->m_i##name##Function); \
	} else { \
		LUA->PushNil(); \
	} \
	return 1; \
}

HANDLER_DEFINE_CALLBACK_FUNCTION(MessageCallback)
HANDLER_DEFINE_CALLBACK_FUNCTION(ConnectionStart)
HANDLER_DEFINE_CALLBACK_FUNCTION(ConnectionClosing)
HANDLER_DEFINE_CALLBACK_FUNCTION(ConnectionCrashed)
HANDLER_DEFINE_CALLBACK_FUNCTION(PacketStart)
HANDLER_DEFINE_CALLBACK_FUNCTION(PacketEnd)
HANDLER_DEFINE_CALLBACK_FUNCTION(FileRequested)
HANDLER_DEFINE_CALLBACK_FUNCTION(FileReceived)
HANDLER_DEFINE_CALLBACK_FUNCTION(FileDenied)
HANDLER_DEFINE_CALLBACK_FUNCTION(FileSent)
HANDLER_DEFINE_CALLBACK_FUNCTION(ShouldAcceptFile)

/*
 * gameserver library
 */

LUA_FUNCTION_STATIC(gameserver_GetNumClients)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushNumber(Util::server->GetNumClients());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetNumProxies)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushNumber(Util::server->GetNumProxies());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetNumFakeClients)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushNumber(Util::server->GetNumFakeClients());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetMaxClients)
{
	if (!Util::server || !Util::server->GetMaxClients())
		return 0;

	LUA->PushNumber(Util::server->GetMaxClients());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetUDPPort)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushNumber(Util::server->GetUDPPort());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetClient)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	int iClientIndex = (int)LUA->CheckNumber(1);
	if (iClientIndex >= Util::server->GetClientCount())
	{
		iClientIndex -= Util::server->GetClientCount();
		if (iClientIndex >= (int)g_pQueueClients.size())
			return 0;

		CBaseClient* pClient = g_pQueueClients[iClientIndex];
		if (pClient && (!gameserver_rawclients.GetBool() && !pClient->IsConnected()))
			pClient = nullptr;

		Push_CBaseClient(LUA, pClient);
		return 1;
	}

	CBaseClient* pClient = (CBaseClient*)((IServer*)Util::server)->GetClient(iClientIndex);
	if (pClient && !pClient->IsConnected())
		pClient = nullptr;

	Push_CBaseClient(LUA, pClient);
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetClientByUserID)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	int userID = (int)LUA->CheckNumber(1);
	if (userID < 0)
		return 0;

	for(int iClientIndex=0; iClientIndex<Util::server->GetClientCount(); ++iClientIndex)
	{
		CBaseClient* pClient = (CBaseClient*)Util::server->GetClient(iClientIndex);
		if (!pClient->IsConnected() || pClient->GetUserID() != userID)
			continue;

		Push_CBaseClient(LUA, pClient);
		return 1;
	}

	for (CBaseClient* pClient : g_pQueueClients)
	{
		if (!pClient->IsConnected() || pClient->GetUserID() != userID)
			continue;

		Push_CBaseClient(LUA, pClient);
		return 1;
	}

	return 0;
}

LUA_FUNCTION_STATIC(gameserver_GetClientBySteamID)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	const char* steamID = LUA->CheckString(1);
	for(int iClientIndex=0; iClientIndex<Util::server->GetClientCount(); ++iClientIndex)
	{
		CBaseClient* pClient = (CBaseClient*)Util::server->GetClient(iClientIndex);
		if (!pClient->IsConnected() || V_stricmp(pClient->GetNetworkIDString(), steamID) != 0)
			continue;

		Push_CBaseClient(LUA, pClient);
		return 1;
	}

	for (CBaseClient* pClient : g_pQueueClients)
	{
		if (!pClient->IsConnected() || V_stricmp(pClient->GetNetworkIDString(), steamID) != 0)
			continue;

		Push_CBaseClient(LUA, pClient);
		return 1;
	}

	return 0;
}

LUA_FUNCTION_STATIC(gameserver_GetClientCount)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushNumber(Util::server->GetClientCount() + g_pQueueClients.size());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetAll)
{
	LUA->CreateTable();
		if (!Util::server || !Util::server->IsActive())
			return 1;

		int iTableIndex = 0;
		for (int iClientIndex=0; iClientIndex<Util::server->GetClientCount(); ++iClientIndex)
		{
			CBaseClient* pClient = (CBaseClient*)Util::server->GetClient(iClientIndex);
			if (!gameserver_rawclients.GetBool() && !pClient->IsConnected())
				continue;

			Push_CBaseClient(LUA, pClient);
			Util::RawSetI(LUA, -2, ++iTableIndex);
		}

		for (CBaseClient* pClient : g_pQueueClients)
		{
			if (!gameserver_rawclients.GetBool() && !pClient->IsConnected())
				continue;

			Push_CBaseClient(LUA, pClient);
			Util::RawSetI(LUA, -2, ++iTableIndex);
		}

	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetTime)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushNumber(Util::server->GetTime());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetTick)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushNumber(Util::server->GetTick());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetTickInterval)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushNumber(Util::server->GetTickInterval());
	return 1;
}

LUA_JIT_WRAPPED_0R(gameserver_GetName,
	const char*, pName, LUA->PushString(pName)	
)
{
	if (!Util::server || !Util::server->IsActive())
		return nullptr;

	return Util::server->GetName();
}

LUA_JIT_WRAPPED_0R(gameserver_GetMapName,
	const char*, pName, LUA->PushString(pName)	
)
{
	if (!Util::server || !Util::server->IsActive())
		return nullptr;

	return Util::server->GetMapName();
}

LUA_FUNCTION_STATIC(gameserver_GetSpawnCount)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushNumber(Util::server->GetSpawnCount());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetNumClasses)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushNumber(Util::server->GetNumClasses());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetClassBits)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushNumber(Util::server->GetClassBits());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_IsActive)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_IsLoading)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushBool(Util::server->IsLoading());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_IsDedicated)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushBool(Util::server->IsDedicated());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_IsPaused)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushBool(Util::server->IsPaused());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_IsMultiplayer)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushBool(Util::server->IsMultiplayer());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_IsPausable)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushBool(Util::server->IsPausable());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_IsHLTV)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushBool(Util::server->IsHLTV());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetPassword)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushString(Util::server->GetPassword());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_SetMaxClients)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	int nSlots = (int)LUA->CheckNumber(1);

	((CBaseServer*)Util::server)->SetMaxClients(nSlots);
	return 0;
}

LUA_FUNCTION_STATIC(gameserver_SetPaused)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	bool bPaused = LUA->GetBool(1);

	Util::server->SetPaused(bPaused);
	return 0;
}

LUA_FUNCTION_STATIC(gameserver_SetPassword)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	const char* strPassword = LUA->CheckString(1);

	Util::server->SetPassword(strPassword);
	return 0;
}

LUA_FUNCTION_STATIC(gameserver_BroadcastMessage)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	int iType = (int)LUA->CheckNumber(1);
	const char* strName = LUA->CheckString(2);

	SVC_CustomMessage msg;
	msg.m_iType = iType;
	strncpy(msg.m_strName, strName, sizeof(msg.m_strName));

#if MODULE_EXISTS_BITBUF
	bf_write* bf = Get_bf_write(LUA, 3, true);

	if (bf->IsOverflowed())
		LUA->ArgError(3, "Tried to use a buffer that is overflowed!");

	msg.m_DataOut.StartWriting(bf->GetData(), 0, 0, bf->GetMaxNumBits());
	msg.m_iLength = bf->GetNumBitsWritten();
#else
	size_t nLength;
	const char* pData = Util::CheckLString(LUA, 3, &nLength);

	msg.m_DataOut.StartWriting((void*)pData, nLength);
	msg.m_iLength = nLength * 8;
#endif

	Util::server->BroadcastMessage(msg);
	return 0;
}

static Symbols::NET_SendPacket func_NET_SendPacket;
LUA_FUNCTION_STATIC(gameserver_SendConnectionlessPacket)
{
#if MODULE_EXISTS_BITBUF
	bf_write* msg = Get_bf_write(LUA, 1, true);
#else
	size_t nLength;
	const char* pData = Util::CheckLString(LUA, 1, &nLength);
#endif

	netadrnew_t adr;
	adr.SetFromString(LUA->CheckString(2), LUA->GetBool(3));

	CBaseServer* pServer = (CBaseServer*)Util::server;
	int nSocket = (int)LUA->CheckNumberOpt(4, pServer->m_Socket);

	if (!adr.IsValid())
	{
		LUA->PushNumber(-1);
		return 1;
	}

	if (!func_NET_SendPacket)
		LUA->ThrowError("Failed to load NET_SendPacket");

	LUA->PushNumber(func_NET_SendPacket(nullptr, nSocket, (netadr_t&)adr,
#if MODULE_EXISTS_BITBUF
		msg->GetData(), msg->GetNumBytesWritten(),
#else
		(const unsigned char*)pData, nLength,
#endif
	nullptr, false));
	return 1;
}

static CUtlVectorMT<CUtlVector<CNetChan*>>* s_NetChannels = nullptr;
CNetChan* NET_CreateHolyLibNetChannel(int socket, netadrnew_t* adr, const char* name, INetChannelHandler* handler, bool bForceNewChannel, int nProtocolVersion)
{
	if (!s_NetChannels)
		return nullptr;

	CNetChan* pChan = new CNetChan;

	(*s_NetChannels).Lock();
	(*s_NetChannels).AddToTail(pChan);
	(*s_NetChannels).Unlock();

	pChan->Setup(socket, (netadr_t*)adr, name, handler, nProtocolVersion);

	return pChan;
}

static Symbols::NET_CreateNetChannel func_NET_CreateNetChannel;
LUA_FUNCTION_STATIC(gameserver_CreateNetChannel)
{
	if (!s_NetChannels)
		LUA->ThrowError("Failed to load s_NetChannels!");

	netadrnew_t adr;
	adr.SetFromString(LUA->CheckString(1), LUA->GetBool(2));
	int nProtocolVersion = (int)LUA->CheckNumberOpt(3, 1);

	CBaseServer* pServer = (CBaseServer*)Util::server;
	int nSocket = (int)LUA->CheckNumberOpt(4, pServer->m_Socket);

	if (!adr.IsValid())
	{
		Push_CNetChan(LUA, nullptr);
		return 1;
	}

	ILuaNetMessageHandler* pHandler = new ILuaNetMessageHandler(LUA);
	CNetChan* pNetChannel = NET_CreateHolyLibNetChannel(nSocket, &adr, adr.ToString(), (INetChannelHandler*)pHandler, true, nProtocolVersion);
	pNetChannel->RegisterMessage(pHandler->m_pLuaNetChanMessage);
	pHandler->m_pChan = pNetChannel;

	Push_CNetChan(LUA, pNetChannel);
	return 1;
}

static Symbols::NET_RemoveNetChannel func_NET_RemoveNetChannel;
LUA_FUNCTION_STATIC(gameserver_RemoveNetChannel)
{
	if (!func_NET_RemoveNetChannel)
		LUA->ThrowError("Failed to load NET_RemoveNetChannel!");

	LuaUserData* pLuaData = Get_CNetChan_Data(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pLuaData->GetData();

	ILuaNetMessageHandler* pHandler = (ILuaNetMessageHandler*)pNetChannel->m_MessageHandler;
	func_NET_RemoveNetChannel(pNetChannel, true);
	pLuaData->Release(LUA);

	if (pHandler)
	{
		delete pHandler;
	}

	return 0;
}

LUA_FUNCTION_STATIC(gameserver_GetCreatedNetChannels)
{
	LUA->PreCreateTable(g_pNetMessageHandlers.size(), 0);
		int idx = 0;
		for (auto& handler : g_pNetMessageHandlers)
		{
			Push_CNetChan(LUA, handler->m_pChan);
			Util::RawSetI(LUA, -2, ++idx);
		}

	return 1;
}

LUA_FUNCTION_STATIC(gameserver_CreateFakeClient)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	const char* pName = LUA->CheckString(1);
	CBaseServer* pServer = (CBaseServer*)Util::server;
	Push_CBaseClient(LUA, pServer->CreateFakeClient(pName));
	return 1;
}

static bool hook_CBaseClient_SetSignonState(CBaseClient* cl, int state, int spawncount);
LUA_FUNCTION_STATIC(gameserver_CreateFakeQueueClient)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	if (g_bClientLayoutMismatch)
		return 0; // see g_bClientLayoutMismatch - direct field writes below would corrupt engine state

	const char* pName = LUA->CheckString(1);
	CBaseServer* pServer = (CBaseServer*)Util::server;

	netadrnew_s adr;
	CBaseClient* fakeclient = pServer->GetFreeClient(*((netadr_t*)&adr)); // Very "safe"
	if (!fakeclient)
		return 0;

	// Fk sv_stressbots

	int userID = ++pServer->m_nUserid;
	pServer->m_nNumConnections++;

	fakeclient->SetReportThisFakeClient( pServer->m_bReportNewFakeClients );
	fakeclient->Connect( pName, userID, nullptr, true, 0 );

	fakeclient->SetUserCVar( "rate", "30000" );
	fakeclient->SetUserCVar( "cl_updaterate", "20" );
	fakeclient->SetUserCVar( "cl_interp_ratio", "1.0" );
	fakeclient->SetUserCVar( "cl_interp", "0.1" );
	fakeclient->SetUserCVar( "cl_interpolate", "0" );
	fakeclient->SetUserCVar( "cl_predict", "1" );
	fakeclient->SetUserCVar( "cl_predictweapons", "1" );
	fakeclient->SetUserCVar( "cl_lagcompensation", "1" );
	fakeclient->SetUserCVar( "closecaption","0" );
	fakeclient->SetUserCVar( "english", "1" );

	fakeclient->SetUserCVar( "cl_clanid", "0" );
	fakeclient->SetUserCVar( "cl_team", "blue" );
	fakeclient->SetUserCVar( "hud_classautokill", "1" );
	fakeclient->SetUserCVar( "tf_medigun_autoheal", "0" );
	fakeclient->SetUserCVar( "cl_autorezoom", "1" );
	fakeclient->SetUserCVar( "fov_desired", "75" );
	fakeclient->SetUserCVar( "tf_remember_lastswitched", "0" );

	fakeclient->SetUserCVar( "cl_autoreload", "0" );
	fakeclient->SetUserCVar( "tf_remember_activeweapon", "0" );
	fakeclient->SetUserCVar( "hud_combattext", "0" );
	fakeclient->SetUserCVar( "cl_flipviewmodels", "0" );

	hook_CBaseClient_SetSignonState(fakeclient, SIGNONSTATE_PRESPAWN, pServer->GetSpawnCount());
	CGameClient* pClient = (CGameClient*)fakeclient;
	pClient->edict = nullptr;
	pClient->m_bConVarsChanged = false;

	Push_CBaseClient(LUA, fakeclient);
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_CreateNewClient)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	CBaseServer* pServer = (CBaseServer*)Util::server;
	Push_CBaseClient(LUA, pServer->CreateNewClient(pServer->GetClientCount()));
	return 1;
}

static thread_local bool g_bNoQueueLookup = false;
static thread_local bool g_bDontRunLuaInFreeClient = false;
LUA_FUNCTION_STATIC(gameserver_GetFreeClient)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	netadrnew_t adr;
	adr.SetFromString(LUA->CheckString(1), LUA->GetBool(2));

	g_bNoQueueLookup = LUA->GetBool(3);
	g_bDontRunLuaInFreeClient = true;
	CBaseServer* pServer = (CBaseServer*)Util::server;
	CBaseClient* pClient = pServer->GetFreeClient(*((netadr_t*)&adr));
	g_bDontRunLuaInFreeClient = false;
	g_bNoQueueLookup = false;

	Push_CBaseClient(LUA, pClient);
	return 1;
}

static CBaseClient* GetFreeQueueClient(CBaseServer* _this, netadr_t& adr);
LUA_FUNCTION_STATIC(gameserver_GetFreeQueueClient)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	netadrnew_t adr;
	adr.SetFromString(LUA->CheckString(1), LUA->GetBool(2));

	CBaseServer* pServer = (CBaseServer*)Util::server;
	Push_CBaseClient(LUA, GetFreeQueueClient(pServer, *((netadr_t*)&adr)));
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetSocket)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	CBaseServer* pServer = (CBaseServer*)Util::server;
	LUA->PushNumber(pServer->m_Socket);
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetCPUUsage)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	CBaseServer* pServer = (CBaseServer*)Util::server;
	LUA->PushNumber(pServer->GetCPUUsage());

	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetCurrentRandomNonce)
{
	Util::DoUnsafeCodeCheck(LUA);
	if (!Util::server || !Util::server->IsActive())
		return 0;

	CBaseServer* pServer = (CBaseServer*)Util::server;
	LUA->PushNumber(pServer->m_CurrentRandomNonce);
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetLastRandomNonce)
{
	Util::DoUnsafeCodeCheck(LUA);
	if (!Util::server || !Util::server->IsActive())
		return 0;

	CBaseServer* pServer = (CBaseServer*)Util::server;
	LUA->PushNumber(pServer->m_LastRandomNonce);
	return 1;
}

static ConVar* sv_stressbots;
void CGameServerModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	sv_stressbots = g_pCVar->FindVar("sv_stressbots");
	if (!sv_stressbots)
		Warning(PROJECT_NAME ": Failed to find sv_stressbots convar!\n");

	Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::CBaseClient, pLua->CreateMetaTable("CGameClient"));
		Push_CBaseClientMeta(pLua);

		Util::AddFunc(pLua, CGameClient__tostring, "__tostring");
	pLua->Pop(1);

	Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::CNetChan, pLua->CreateMetaTable("CNetChan"));
		Util::AddFunc(pLua, CNetChan__tostring, "__tostring");
		Util::AddFunc(pLua, CNetChan__index, "__index");
		Util::AddFunc(pLua, CNetChan__newindex, "__newindex");
		LUA_REGISTER_JIT(pLua, CNetChan_GetTable, "GetTable");
		LUA_REGISTER_JIT(pLua, CNetChan_IsValid, "IsValid");
		Util::AddFunc(pLua, CNetChan_GetAvgLoss, "GetAvgLoss");
		Util::AddFunc(pLua, CNetChan_GetAvgChoke, "GetAvgChoke");
		Util::AddFunc(pLua, CNetChan_GetAvgData, "GetAvgData");
		Util::AddFunc(pLua, CNetChan_GetAvgLatency, "GetAvgLatency");
		Util::AddFunc(pLua, CNetChan_GetAvgPackets, "GetAvgPackets");
		Util::AddFunc(pLua, CNetChan_GetChallengeNr, "GetChallengeNr");
		Util::AddFunc(pLua, CNetChan_GetAddress, "GetAddress");
		Util::AddFunc(pLua, CNetChan_GetDataRate, "GetDataRate");
		Util::AddFunc(pLua, CNetChan_GetDropNumber, "GetDropNumber");
		Util::AddFunc(pLua, CNetChan_SetChoked, "SetChoked");
		Util::AddFunc(pLua, CNetChan_SetFileTransmissionMode, "SetFileTransmissionMode");
		Util::AddFunc(pLua, CNetChan_SetCompressionMode, "SetCompressionMode");
		Util::AddFunc(pLua, CNetChan_SetDataRate, "SetDataRate");
		Util::AddFunc(pLua, CNetChan_SetTimeout, "SetTimeout");
		Util::AddFunc(pLua, CNetChan_GetTime, "GetTime");
		Util::AddFunc(pLua, CNetChan_GetTimeConnected, "GetTimeConnected");
		Util::AddFunc(pLua, CNetChan_GetTimeoutSeconds, "GetTimeoutSeconds");
		Util::AddFunc(pLua, CNetChan_GetTimeSinceLastReceived, "GetTimeSinceLastReceived");
		Util::AddFunc(pLua, CNetChan_GetTotalData, "GetTotalData");
		Util::AddFunc(pLua, CNetChan_GetBufferSize, "GetBufferSize");
		Util::AddFunc(pLua, CNetChan_GetProtocolVersion, "GetProtocolVersion");
		Util::AddFunc(pLua, CNetChan_GetName, "GetName");
		Util::AddFunc(pLua, CNetChan_GetProcessingMessages, "GetProcessingMessages");
		Util::AddFunc(pLua, CNetChan_GetClearedDuringProcessing, "GetClearedDuringProcessing");
		Util::AddFunc(pLua, CNetChan_GetOutSequenceNr, "GetOutSequenceNr");
		Util::AddFunc(pLua, CNetChan_GetInSequenceNr, "GetInSequenceNr");
		Util::AddFunc(pLua, CNetChan_GetOutSequenceNrAck, "GetOutSequenceNrAck");
		Util::AddFunc(pLua, CNetChan_GetOutReliableState, "GetOutReliableState");
		Util::AddFunc(pLua, CNetChan_GetInReliableState, "GetInReliableState");
		Util::AddFunc(pLua, CNetChan_GetChokedPackets, "GetChokedPackets");
		Util::AddFunc(pLua, CNetChan_GetStreamReliable, "GetStreamReliable");
		Util::AddFunc(pLua, CNetChan_GetStreamUnreliable, "GetStreamUnreliable");
		Util::AddFunc(pLua, CNetChan_GetStreamVoice, "GetStreamVoice");
		Util::AddFunc(pLua, CNetChan_GetStreamSocket, "GetStreamSocket");
		Util::AddFunc(pLua, CNetChan_GetMaxReliablePayloadSize, "GetMaxReliablePayloadSize");
		Util::AddFunc(pLua, CNetChan_GetLastReceived, "GetLastReceived");
		Util::AddFunc(pLua, CNetChan_GetConnectTime, "GetConnectTime");
		Util::AddFunc(pLua, CNetChan_GetClearTime, "GetClearTime");
		Util::AddFunc(pLua, CNetChan_GetTimeout, "GetTimeout");
		Util::AddFunc(pLua, CNetChan_SetTimeout, "SetTimeout");
		Util::AddFunc(pLua, CNetChan_GetRate, "GetRate");
		Util::AddFunc(pLua, CNetChan_SetRate, "SetRate");
		Util::AddFunc(pLua, CNetChan_GetRemoteFramerate, "GetRemoteFramerate");
		Util::AddFunc(pLua, CNetChan_Transmit, "Transmit");
		Util::AddFunc(pLua, CNetChan_ProcessStream, "ProcessStream");
		Util::AddFunc(pLua, CNetChan_SetMaxBufferSize, "SetMaxBufferSize");
		Util::AddFunc(pLua, CNetChan_GetMaxRoutablePayloadSize, "GetMaxRoutablePayloadSize");
		Util::AddFunc(pLua, CNetChan_SendMessage, "SendMessage");
		Util::AddFunc(pLua, CNetChan_SendFile, "SendFile");
		Util::AddFunc(pLua, CNetChan_RequestFile, "RequestFile");
		Util::AddFunc(pLua, CNetChan_Shutdown, "Shutdown");
		Util::AddFunc(pLua, CNetChan_CanPacket, "CanPacket");

		// Callbacks
		Util::AddFunc(pLua, CNetChan_SetMessageCallback, "SetMessageCallback");
		Util::AddFunc(pLua, CNetChan_GetMessageCallback, "GetMessageCallback");
		Util::AddFunc(pLua, CNetChan_SetConnectionStart, "SetConnectionStartCallback");
		Util::AddFunc(pLua, CNetChan_GetConnectionStart, "GetConnectionStartCallback");
		Util::AddFunc(pLua, CNetChan_SetConnectionClosing, "SetConnectionClosingCallback");
		Util::AddFunc(pLua, CNetChan_GetConnectionClosing, "GetConnectionClosingCallback");
		Util::AddFunc(pLua, CNetChan_SetConnectionCrashed, "SetConnectionCrashedCallback");
		Util::AddFunc(pLua, CNetChan_GetConnectionCrashed, "GetConnectionCrashedCallback");
		Util::AddFunc(pLua, CNetChan_SetPacketStart, "SetPacketStartCallback");
		Util::AddFunc(pLua, CNetChan_GetPacketStart, "GetPacketStartCallback");
		Util::AddFunc(pLua, CNetChan_SetPacketEnd, "SetPacketEndCallback");
		Util::AddFunc(pLua, CNetChan_GetPacketEnd, "GetPacketEndCallback");
		Util::AddFunc(pLua, CNetChan_SetFileRequested, "SetFileRequestedCallback");
		Util::AddFunc(pLua, CNetChan_GetFileRequested, "GetFileRequestedCallback");
		Util::AddFunc(pLua, CNetChan_SetFileReceived, "SetFileReceivedCallback");
		Util::AddFunc(pLua, CNetChan_GetFileReceived, "GetFileReceivedCallback");
		Util::AddFunc(pLua, CNetChan_SetFileDenied, "SetFileDeniedCallback");
		Util::AddFunc(pLua, CNetChan_GetFileDenied, "GetFileDeniedCallback");
		Util::AddFunc(pLua, CNetChan_SetFileSent, "SetFileSentCallback");
		Util::AddFunc(pLua, CNetChan_GetFileSent, "GetFileSentCallback");
		Util::AddFunc(pLua, CNetChan_SetShouldAcceptFile, "SetShouldAcceptFileCallback");
		Util::AddFunc(pLua, CNetChan_GetShouldAcceptFile, "GetShouldAcceptFileCallback");
	pLua->Pop(1);

	Util::StartTable(pLua);
		Util::AddFunc(pLua, gameserver_GetNumClients, "GetNumClients");
		Util::AddFunc(pLua, gameserver_GetNumProxies, "GetNumProxies");
		Util::AddFunc(pLua, gameserver_GetNumFakeClients, "GetNumFakeClients");
		Util::AddFunc(pLua, gameserver_GetMaxClients, "GetMaxClients");
		Util::AddFunc(pLua, gameserver_GetUDPPort, "GetUDPPort");
		Util::AddFunc(pLua, gameserver_GetClient, "GetClient");
		Util::AddFunc(pLua, gameserver_GetClientByUserID, "GetClientByUserID");
		Util::AddFunc(pLua, gameserver_GetClientBySteamID, "GetClientBySteamID");
		Util::AddFunc(pLua, gameserver_GetClientCount, "GetClientCount");
		Util::AddFunc(pLua, gameserver_GetAll, "GetAll");
		Util::AddFunc(pLua, gameserver_GetTime, "GetTime");
		Util::AddFunc(pLua, gameserver_GetTick, "GetTick");
		Util::AddFunc(pLua, gameserver_GetTickInterval, "GetTickInterval");
		LUA_REGISTER_JIT(pLua, gameserver_GetName, "GetName");
		LUA_REGISTER_JIT(pLua, gameserver_GetMapName, "GetMapName");
		Util::AddFunc(pLua, gameserver_GetSpawnCount, "GetSpawnCount");
		Util::AddFunc(pLua, gameserver_GetNumClasses, "GetNumClasses");
		Util::AddFunc(pLua, gameserver_GetClassBits, "GetClassBits");
		Util::AddFunc(pLua, gameserver_IsActive, "IsActive");
		Util::AddFunc(pLua, gameserver_IsLoading, "IsLoading");
		Util::AddFunc(pLua, gameserver_IsDedicated, "IsDedicated");
		Util::AddFunc(pLua, gameserver_IsPaused, "IsPaused");
		Util::AddFunc(pLua, gameserver_IsMultiplayer, "IsMultiplayer");
		Util::AddFunc(pLua, gameserver_IsPausable, "IsPausable");
		Util::AddFunc(pLua, gameserver_IsHLTV, "IsHLTV");
		Util::AddFunc(pLua, gameserver_GetPassword, "GetPassword");
		Util::AddFunc(pLua, gameserver_SetMaxClients, "SetMaxClients");
		Util::AddFunc(pLua, gameserver_SetPaused, "SetPaused");
		Util::AddFunc(pLua, gameserver_SetPassword, "SetPassword");
		Util::AddFunc(pLua, gameserver_BroadcastMessage, "BroadcastMessage");
		Util::AddFunc(pLua, gameserver_SendConnectionlessPacket, "SendConnectionlessPacket");
		Util::AddFunc(pLua, gameserver_CreateFakeClient, "CreateFakeClient");
		Util::AddFunc(pLua, gameserver_CreateFakeQueueClient, "CreateFakeQueueClient");
		Util::AddFunc(pLua, gameserver_CreateNewClient, "CreateNewClient");
		Util::AddFunc(pLua, gameserver_GetFreeClient, "GetFreeClient");
		Util::AddFunc(pLua, gameserver_GetFreeQueueClient, "GetFreeQueueClient");
		Util::AddFunc(pLua, gameserver_GetCPUUsage, "GetCPUUsage");
		Util::AddFunc(pLua, gameserver_GetSocket, "GetSocket");
		Util::AddFunc(pLua, gameserver_GetCurrentRandomNonce, "GetCurrentRandomNonce");
		Util::AddFunc(pLua, gameserver_GetLastRandomNonce, "GetLastRandomNonce");

		Util::AddFunc(pLua, gameserver_CreateNetChannel, "CreateNetChannel");
		Util::AddFunc(pLua, gameserver_RemoveNetChannel, "RemoveNetChannel");
		Util::AddFunc(pLua, gameserver_GetCreatedNetChannels, "GetCreatedNetChannels");

		Util::AddValue(pLua, NS_CLIENT, "NS_CLIENT");
		Util::AddValue(pLua, NS_SERVER, "NS_SERVER");
		Util::AddValue(pLua, NS_HLTV, "NS_HLTV");

		Util::AddValue(pLua, FLOW_OUTGOING, "FLOW_OUTGOING");
		Util::AddValue(pLua, FLOW_INCOMING, "FLOW_INCOMING");
	Util::FinishTable(pLua, "gameserver");
}

void CGameServerModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "gameserver");

	DeleteAll_CBaseClient(pLua);
	DeleteAll_CNetChan(pLua);
}

// This is a total CGameClient-object limit, not a real-player limit. The engine's
// gpGlobals->maxClients / m_nMaxclients and edict range stay unchanged; objects
// above that range are parked queue clients. Client slots are networked in a
// byte and ABSOLUTE_PLAYER_LIMIT is a count, so the highest valid slot is 254.
static ConVar gameserver_maxplayers("holylib_gameserver_maxplayers", "128", 0,
	"Experimental - total real + parked client limit. Real players remain capped by the engine; parked client slots are capped at 254.",
	true, 1, true, ABSOLUTE_PLAYER_LIMIT);

static inline int GetConfiguredClientLimit(int nRealMaxClients)
{
	return clamp(gameserver_maxplayers.GetInt(), nRealMaxClients, ABSOLUTE_PLAYER_LIMIT);
}

static int FindFreeQueueClientSlot(int nFirstQueueSlot, int nClientLimit)
{
	for (int nSlot = nFirstQueueSlot; nSlot < nClientLimit; ++nSlot)
	{
		bool bUsed = false;
		for (CGameClient* pClient : g_pQueueClients)
		{
			if (pClient && pClient->m_nClientSlot == nSlot)
			{
				bUsed = true;
				break;
			}
		}

		if (!bUsed)
			return nSlot;
	}

	return -1;
}

static CBaseClient* GetFreeQueueClient(CBaseServer* _this, netadr_t& adr)
{
	if (g_bClientLayoutMismatch)
		return nullptr; // parking disabled: see comment on g_bClientLayoutMismatch

	const int nFirstQueueSlot = gpGlobals->maxClients;
	const int nClientLimit = GetConfiguredClientLimit(nFirstQueueSlot);
	CBaseClient* freeclient = nullptr;
	for (CBaseClient* pClient : g_pQueueClients)
	{
		// Lowering the ConVar must not recycle an already-created object whose slot
		// is now outside the configured/networkable range. Connected clients in that
		// range are allowed to finish disconnecting but will not be reused.
		if (!pClient || pClient->m_nClientSlot < nFirstQueueSlot || pClient->m_nClientSlot >= nClientLimit)
			continue;

		if (pClient->IsFakeClient())
			continue;

		if (pClient->IsConnected())
		{
			if (pClient->m_NetChannel && adr.CompareAdr(pClient->m_NetChannel->GetRemoteAddress()))
			{
				pClient->m_NetChannel->Shutdown( NULL );
				pClient->m_NetChannel = NULL;
		
				pClient->Clear();
				return pClient;
			}
		} else {
			if (!freeclient)
				freeclient = pClient;
		}
	}

	if (!freeclient)
	{
		// Queue slots MUST be above maxClients. Do not derive the slot from vector
		// size: promotion/removal leaves holes and would otherwise duplicate a live
		// queue slot. The configured value is a count, hence slot == limit is invalid.
		const int nQueueSlot = FindFreeQueueClientSlot(nFirstQueueSlot, nClientLimit);
		if (nQueueSlot < 0)
			return nullptr;

		freeclient = _this->CreateNewClient(nQueueSlot);
		if (!freeclient)
			return nullptr;

		g_pQueueClients.push_back((CGameClient*)freeclient);
	}
	// We do not register it to m_Clients of the CBaseServer

	return freeclient;
}

static Detouring::Hook detour_CBaseServer_GetFreeClient;
static CBaseClient* hook_CBaseServer_GetFreeClient(CBaseServer* _this, netadr_t& adr)
{
	if (!g_bDontRunLuaInFreeClient && Lua::PushHook("HolyLib:GetFreeClient"))
	{
		g_Lua->PushString(adr.ToString());
		if (g_Lua->CallFunctionProtected(2, 1, true))
		{
			CBaseClient* pClient = Get_CBaseClient(g_Lua, -1, false, true);
			g_Lua->Pop(1);
			if (pClient)
			{
				if (pClient->IsConnected())
				{
					if (g_pModuleManager.IsUnsafeCodeEnabled())
					{
						// If unsafe code is enabled we assume code always knows what it's doing!
						Warning(PROJECT_NAME " - gameserver: \"HolyLib:GetFreeClient\" returned a connected client! Dropping!\n");
						pClient->Disconnect("Natural Selection (Check Server Logs)");
						return pClient;
					} else
						Warning(PROJECT_NAME " - gameserver: \"HolyLib:GetFreeClient\" returned a connected client! Ignoring!\n");
				} else
					return pClient;
			}
		}
	}

	CBaseClient* freeclient = detour_CBaseServer_GetFreeClient.GetTrampoline<Symbols::CBaseServer_GetFreeClient>()(_this, adr);
	if (freeclient || g_bNoQueueLookup)
		return freeclient;

	return GetFreeQueueClient(_this, adr);
}

static Detouring::Hook detour_CBaseServer_CreateFakeClient;
static CBaseClient* hook_CBaseServer_CreateFakeClient(CBaseServer* _this, const char* pName)
{
	netadr_t adr;
	CBaseClient* pClient = detour_CBaseServer_GetFreeClient.GetTrampoline<Symbols::CBaseServer_GetFreeClient>()(_this, adr);
	if (!pClient || pClient->m_nClientSlot >= _this->m_nMaxclients)
		return nullptr;

	return detour_CBaseServer_CreateFakeClient.GetTrampoline<Symbols::CBaseServer_CreateFakeClient>()(_this, pName);
}

static Detouring::Hook detour_CBaseServer_UserInfoChanged;
static void hook_CBaseServer_UserInfoChanged(CBaseServer* _this, int nClientIndex)
{
	if (nClientIndex >= _this->m_nMaxclients)
		return;

	detour_CBaseServer_UserInfoChanged.GetTrampoline<Symbols::CBaseServer_UserInfoChanged>()(_this, nClientIndex);
}

static Detouring::Hook detour_CGameServer_RemoveClientFromGame;
static void hook_CGameServer_RemoveClientFromGame(CBaseServer* _this, CBaseClient* pClient)
{
	// m_nMaxclients is temporarily raised to the advertised queue capacity in
	// hook_CSteam3Server_SendUpdatedServerDetails. It is therefore not a safe
	// real-player boundary for teardown. A parked client's edict aliases a map
	// entity, so make the ownership test authoritative and keep the game DLL
	// from receiving it. The engine itself must retain the edict for later
	// sign-on-state transitions.
	if (IsParkedQueueClient(pClient))
		return;

	detour_CGameServer_RemoveClientFromGame.GetTrampoline<Symbols::CGameServer_RemoveClientFromGame>()(_this, pClient);
}

static ConVar gameserver_steamlookup_queueclients("holylib_gameserver_steamlookup_queueclients", "1", 0, "If enabled, CSteam3Server::ClientFindFromSteamID also resolves queue clients so Steam auth callbacks reach them. Disable to keep engine Steam3 code (dup-session handling, GC deny/kick) away from clients in slots >= maxclients if it misbehaves on them.");

static Detouring::Hook detour_CSteam3Server_ClientFindFromSteamID;
static CBaseClient* hook_CSteam3Server_ClientFindFromSteamID(void* _this, CSteamID* steamID)
{
	CBaseClient* pFoundClient = detour_CSteam3Server_ClientFindFromSteamID.GetTrampoline<Symbols::CSteam3Server_ClientFindFromSteamID>()(_this, steamID);
	if (pFoundClient)
		return pFoundClient;

	if (!gameserver_steamlookup_queueclients.GetBool())
		return nullptr;

	for (CBaseClient* pClient : g_pQueueClients)
	{
		if (!pClient->IsConnected() || pClient->IsFakeClient())
			continue;

		// USERID_t id = pClient->GetNetworkID();
		if (pClient->m_SteamID == *steamID)
			return pClient;
	}

	return nullptr;
}

static Detouring::Hook detour_CServerPlugin_ClientSettingsChanged;
static void hook_CServerPlugin_ClientSettingsChanged(void* _this, edict_t* pEdict)
{
	if (pEdict->m_EdictIndex > gpGlobals->maxClients)
		return;

	detour_CServerPlugin_ClientSettingsChanged.GetTrampoline<Symbols::CServerPlugin_ClientSettingsChanged>()(_this, pEdict);
}

static Detouring::Hook detour_CVEngineServer_GMOD_SendToClient;
static void hook_CVEngineServer_GMOD_SendToClient(void* _this, int client, void *data, int dataSize)
{
	if (client < gpGlobals->maxClients)
	{
		detour_CVEngineServer_GMOD_SendToClient.GetTrampoline<Symbols::CVEngineServer_GMOD_SendToClient>()(_this, client, data, dataSize);
		return;
	}

	/*
	 * Queue slots are handed out as maxClients + insertion index, but vector order
	 * stops matching slot order after any erase (AddToServerList /
	 * RemoveFromAllLists). Resolve by slot instead of indexing.
	 */
	CBaseClient* pClient = nullptr;
	for (CGameClient* pQueueClient : g_pQueueClients)
	{
		if (pQueueClient->m_nClientSlot == client)
		{
			pClient = (CBaseClient*)pQueueClient;
			break;
		}
	}

	if (!pClient)
		return; // Invalid?

	if (pClient->IsFakeClient())
	{
		DevMsg(PROJECT_NAME " - gameserver: Not sending to fake client '%s'.\n", pClient->GetClientName());
		return;
	}

	// IsConnected() does NOT imply a live channel (same bug class as the old
	// GetFreeQueueClient null-netchannel crash) - check both before SendNetMsg.
	if (!pClient->IsConnected() || !pClient->m_NetChannel)
	{
		Msg(PROJECT_NAME " - gameserver: Not sending to null client.\n");
		return;
	}

	// Not 1:1 to GMod but should be good enouth

	SVC_CustomMessage msg;
	msg.m_DataOut.StartWriting(data, 0, 0, dataSize);
	msg.m_iLength = dataSize;
	msg.m_iLengthBits = 20;
	msg.m_iType = svc_GMod_ServerToClient;
	
	pClient->m_NetChannel->SendNetMsg(msg, true, false);
}

#if defined(SYSTEM_LINUX) && defined(ARCHITECTURE_X86_64)
static Detouring::Hook detour_CVarIterator_Get;
static bool g_bSafeCVarIteratorInstalled = false;
static bool g_bWarnedNullConVar = false;
using CVarIteratorGetFn = ConCommandBase* (*)(void*);

static ConCommandBase* GetNullCVarIteratorFallback()
{
	// This HolyLib-owned ConVar is constructed with flags=0 at file scope. A
	// zero-flag entry is rejected by every non-zero Host_* flag query, unlike the
	// engine's "developer" ConVar whose non-replicated flags can legitimately
	// match other global iterator consumers hooked through the same Get method.
	ConCommandBase* pFallback = &gameserver_disablespawnsafety;
	if (pFallback->IsCommand() || pFallback->GetFlags() != 0)
		return nullptr;

	return pFallback;
}

/*
 * sourcesdk-minimal still declares ICVarIteratorInternal without its virtual
 * destructor. GMod 260709 has the destructor, shifting SetFirst/Next/IsValid/Get
 * from vtable slots 0..3 to 2..5. Use the ABI verified in the engine helper we
 * detour instead of ICvar::Iterator, whose stale declaration would call every
 * iterator method two slots early.
 */
class CX64CVarIterator260709
{
public:
	explicit CX64CVarIterator260709(ICvar* pCVar)
	{
		if (!pCVar)
			return;

		void** pVTable = *reinterpret_cast<void***>(pCVar);
		m_pIterator = reinterpret_cast<void* (*)(ICvar*)>(pVTable[42])(pCVar); // FactoryInternalIterator @ +0x150
	}

	~CX64CVarIterator260709()
	{
		if (!m_pIterator)
			return;

		void** pVTable = *reinterpret_cast<void***>(m_pIterator);
		reinterpret_cast<void (*)(void*)>(pVTable[1])(m_pIterator); // deleting destructor @ +0x08
	}

	bool IsAvailable() const { return m_pIterator != nullptr; }
	void* GetMethod(size_t nSlot) const
	{
		if (!m_pIterator)
			return nullptr;

		return (*reinterpret_cast<void***>(m_pIterator))[nSlot];
	}
	void SetFirst() { CallVoid(2); }
	void Next() { CallVoid(3); }
	bool IsValid() { return Call<bool (*)(void*)>(4, false); }
	ConCommandBase* Get() { return Call<ConCommandBase* (*)(void*)>(5, (ConCommandBase*)nullptr); }

private:
	void CallVoid(size_t nSlot)
	{
		if (!m_pIterator)
			return;

		void** pVTable = *reinterpret_cast<void***>(m_pIterator);
		reinterpret_cast<void (*)(void*)>(pVTable[nSlot])(m_pIterator);
	}

	template<typename TFn, typename TResult>
	TResult Call(size_t nSlot, TResult pFallback)
	{
		if (!m_pIterator)
			return pFallback;

		void** pVTable = *reinterpret_cast<void***>(m_pIterator);
		return reinterpret_cast<TFn>(pVTable[nSlot])(m_pIterator);
	}

	void* m_pIterator = nullptr;
};

/*
 * GMod x86-64's Host_CountVariablesWithFlags and
 * Host_BuildConVarUpdateMessage share ICvar's factory iterator. Build 260709
 * can report IsValid() while Get() returns nullptr during a connect burst; both
 * stock helpers immediately dereference that result. Detour only Get() and
 * substitute a verified flagless ConVar for a null entry. The engine then
 * rejects that entry through its normal flag filter while retaining its own
 * string cleanup, 129-entry batching and bit-buffer serialization unchanged.
 */
static ConCommandBase* hook_CVarIterator_Get(void* pIterator)
{
	ConCommandBase* pCommand = detour_CVarIterator_Get.GetTrampoline<CVarIteratorGetFn>()(pIterator);
	if (pCommand)
		return pCommand;

	ConCommandBase* pFallback = GetNullCVarIteratorFallback();
	if (!pFallback)
		return nullptr;

	if (!g_bWarnedNullConVar)
	{
		g_bWarnedNullConVar = true;
		Warning(PROJECT_NAME " - gameserver: replaced a null ConVar iterator entry while building server info (engine crash prevented)\n");
	}

	return pFallback;
}
#endif

static void SendPendingServerInfos(CBaseServer* pServer)
{
	if (g_pQueueClients.empty())
		return;

#if defined(SYSTEM_LINUX) && defined(ARCHITECTURE_X86_64)
	if (!g_bSafeCVarIteratorInstalled)
		return;
#endif

	// SendServerInfo can fail internally and Disconnect the client; Lua hooks
	// running off that may mutate g_pQueueClients - iterate a snapshot.
	std::vector<CGameClient*> pQueueSnapshot(g_pQueueClients);
	for (CBaseClient* pClient : pQueueSnapshot)
	{
		if (pClient->m_bSendServerInfo)
		{
			INetChannel* pChan = pClient->m_NetChannel;
			if (pChan)
			{
				netadrnew_s addr = *(netadrnew_s*)&pChan->GetRemoteAddress();
				if (pClient->m_bFullyAuthenticated || 
					addr.IsLocalhost() ||
					addr.IsLoopback() ||
					addr.IsReservedAdr() ||
					pServer->m_nMaxclients == 1
				) // Also checks something for Steam3Server() but naah.
				{
					pClient->SendServerInfo();
				}
			}
		}
	}
}

static void SendClientMessages()
{
	if (g_pQueueClients.empty())
		return;

	// ShouldSendMessages() Disconnects the client internally when its reliable
	// channel overflowed - Lua hooks running off that may mutate g_pQueueClients,
	// so iterate a snapshot. (hook_CNetChan_D2 scrubs dying channels off every
	// entry, so the m_NetChannel re-checks here stay trustworthy.)
	std::vector<CGameClient*> pQueueSnapshot(g_pQueueClients);
	for (CBaseClient* pClient : pQueueSnapshot)
	{
		// Queue clients are outside the engine's client list, so establish channel
		// liveness before entering even our guarded ShouldSendMessages path, then
		// re-check after it because overflow handling can Disconnect synchronously.
		if (!pClient->m_NetChannel || !pClient->ShouldSendMessages() || !pClient->m_NetChannel)
			continue;

		pClient->m_NetChannel->Transmit();
		pClient->UpdateSendState();
	}
}

// Since the Engine can't handle our queue clients, we instead handle them ourselves. Take that engine >:3c
void CGameServerModule::Think(bool bSimulating)
{
	VPROF_BUDGET("HolyLib - CGameServerModule::Think", VPROF_BUDGETGROUP_HOLYLIB);

	CBaseServer* pServer = (CBaseServer*)Util::server;
	SendPendingServerInfos(pServer);
	SendClientMessages();
}

/*
	ToDo: Ask Rubat if this is fine.
	NOTE: This for now is only for testing!
*/
static Detouring::Hook detour_CSteam3Server_SendUpdatedServerDetails;
static void hook_CSteam3Server_SendUpdatedServerDetails(void* _this)
{
	CBaseServer* pServer = (CBaseServer*)Util::server;
	int nOrigMaxClients = pServer->m_nMaxclients;
	pServer->m_nMaxclients = GetConfiguredClientLimit(nOrigMaxClients);

	detour_CSteam3Server_SendUpdatedServerDetails.GetTrampoline<Symbols::CSteam3Server_SendUpdatedServerDetails>()(_this);

	pServer->m_nMaxclients = nOrigMaxClients;
}

static Detouring::Hook detour_CBaseServer_ProcessConnectionlessPacket;
static bool hook_CBaseServer_ProcessConnectionlessPacket(IServer* server, netpacket_s* packet)
{
	if (!gameserver_connectionlesspackethook.GetBool() || server->IsHLTV())
		return detour_CBaseServer_ProcessConnectionlessPacket.GetTrampoline<Symbols::CBaseServer_ProcessConnectionlessPacket>()(server, packet);

#if MODULE_EXISTS_NETWORKTHREADING
	if (!ThreadInMainThread()) // Happens when processing packets in the networking thread :^
		return detour_CBaseServer_ProcessConnectionlessPacket.GetTrampoline<Symbols::CBaseServer_ProcessConnectionlessPacket>()(server, packet);
#endif

	int originalPos = packet->message.GetNumBitsRead();
	if (Lua::PushHook("HolyLib:ProcessConnectionlessPacket"))
	{
#if MODULE_EXISTS_BITBUF
		LuaUserData* pLuaData = Push_bf_read(g_Lua, &packet->message, false);
#else
		g_Lua->PushString((const char*)packet->message.GetBasePointer(), packet->message.GetNumBytesLeft());
#endif

		bool bHandled = false;
		g_Lua->PushString(packet->from.ToString());
		if (g_Lua->CallFunctionProtected(3, 1, true))
		{
			bHandled = g_Lua->GetBool(-1);
			g_Lua->Pop(1);
		}

#if MODULE_EXISTS_BITBUF
		if (pLuaData)
			pLuaData->Release(g_Lua);
#endif

		if (bHandled)
			return true;
	}

	packet->message.Seek(originalPos);
	return detour_CBaseServer_ProcessConnectionlessPacket.GetTrampoline<Symbols::CBaseServer_ProcessConnectionlessPacket>()(server, packet);
}

#if MODULE_EXISTS_GMODDATAPACK
extern bool GMODDataPack_SetSignOnState(CBaseClient* cl, int state);
#endif
/*
 * NCG: validity guard for the client-lifecycle detours below.
 *
 * During a mass-disconnect storm (20-30+ clients dropped in the same frame —
 * e.g. gluapack kicking every mid-download client at repack start, or a queue
 * eviction burst), these detours can be handed a CBaseClient pointer that no
 * live container owns anymore. Any dereference of such a pointer — the virtual
 * GetServer() call, name/SteamID reads for the Lua push — is a use-after-free;
 * through a freed vtable it produces the recurring `segfault at 0 ip 0` /
 * libc GPF crash pair seen during hotfix autorefresh storms.
 *
 * This helper establishes liveness WITHOUT dereferencing the candidate: it is
 * a pure pointer-IDENTITY scan over the two containers that own every client
 * this module works with (the main server's client list + our queue list).
 * Engine slot clients persist in m_Clients across disconnects, so normal
 * clients always pass — behavior is unchanged for them. A freed-then-reused
 * allocation also passes, but then the pointer refers to a valid live object
 * again and dereferencing it is memory-safe (worst case a hook sees the
 * successor client — a bookkeeping error, not a crash).
 */
static bool IsKnownClient(CBaseClient* pClient)
{
	if (!pClient)
		return false;

	for (CBaseClient* pQueueClient : g_pQueueClients)
		if (pQueueClient == pClient)
			return true;

	if (!Util::server)
		return false;

	int count = Util::server->GetClientCount();
	for (int i = 0; i < count; ++i)
		if ((CBaseClient*)Util::server->GetClient(i) == pClient)
			return true;

	return false;
}

static Detouring::Hook detour_CBaseClient_SetSignonState;
static bool hook_CBaseClient_SetSignonState(CBaseClient* cl, int state, int spawncount)
{
	// NCG: UAF guard (see IsKnownClient above). An unknown pointer is handed
	// straight to the engine untouched — HolyLib (Lua hook + datapack) stays
	// off it entirely. Note this also skips clients owned by other server
	// instances (HLTV); we don't run SourceTV.
	if (!IsKnownClient(cl))
		return detour_CBaseClient_SetSignonState.GetTrampoline<Symbols::CBaseClient_SetSignonState>()(cl, state, spawncount);

	if (Lua::PushHook("HolyLib:OnSetSignonState"))
	{
		Push_CBaseClient(g_Lua, cl);
		g_Lua->PushNumber(state);
		g_Lua->PushNumber(spawncount);
		if (g_Lua->CallFunctionProtected(4, 1, true))
		{
			bool ret = g_Lua->GetBool(-1);
			g_Lua->Pop(1);

			if (ret)
				return false;
		}
	}

#if MODULE_EXISTS_GMODDATAPACK
	if (GMODDataPack_SetSignOnState(cl, state))
		return false;
#endif

	return detour_CBaseClient_SetSignonState.GetTrampoline<Symbols::CBaseClient_SetSignonState>()(cl, state, spawncount);
}

static Detouring::Hook detour_CGameClient_SetSignonState;
static bool hook_CGameClient_SetSignonState(CGameClient* client, int state, int spawncount)
{
	// CGameClient performs CheckConnect (and therefore game-DLL ClientConnect)
	// before delegating to CBaseClient. Intercept the derived method so a parked
	// queue client's aliased map edict can never reach that callback.
	if (!IsKnownClient(client) || !IsParkedQueueClient(client) || state != SIGNONSTATE_CONNECTED)
	{
		return detour_CGameClient_SetSignonState.GetTrampoline<Symbols::CGameClient_SetSignonState>()(
			client,
			state,
			spawncount
		);
	}

	// The parked CONNECTED path needs the real base implementation after the
	// HolyLib/datapack hooks, but must bypass CGameClient::CheckConnect entirely.
	if (!detour_CBaseClient_SetSignonState.IsEnabled())
		return false;

	INetChannel* channel = client->GetNetChannel();
	if (!channel)
		return false;

	channel->SetTimeout(SIGNON_TIME_OUT);
	channel->SetFileTransmissionMode(false);
	channel->SetMaxBufferSize(true, NET_MAX_PAYLOAD);

	return hook_CBaseClient_SetSignonState(client, state, spawncount);
}

static Detouring::Hook detour_CBaseServer_IsMultiplayer;
static bool hook_CBaseServer_IsMultiplayer(CBaseServer* srv)
{
	if (srv->IsDedicated())
		return true;

	return detour_CBaseServer_IsMultiplayer.GetTrampoline<Symbols::CBaseServer_IsMultiplayer>()(srv);
}

static Detouring::Hook detour_GModDataPack_IsSingleplayer;
static bool hook_GModDataPack_IsSingleplayer(void* dataPack)
{
	if (Util::server && Util::server->IsDedicated())
		return false;

	return detour_GModDataPack_IsSingleplayer.GetTrampoline<Symbols::GModDataPack_IsSingleplayer>()(dataPack);
}

static Detouring::Hook detour_CBaseClient_ShouldSendMessages;
static bool hook_CBaseClient_ShouldSendMessages(CGameClient* cl) // NOTE: We use a CGameClient so that in a debug break I can verify the class here xd
{
#if PLATFORM_64BITS
	/*
	 * Before the queue-UAF hardening, x64 real clients ran the engine's own
	 * implementation. Keep that engine-parity path for slots backed by real
	 * players: it avoids making their snapshot cadence depend on our partial
	 * CBaseClient mirror. The detour remains installed and the guarded HolyLib
	 * implementation below remains mandatory for parked clients, which the
	 * engine does not service and whose netchannels are protected by the D2
	 * destructor sweep. This preserves the queue-UAF fix without replacing a
	 * healthy engine hot path for all real x64 clients.
	 */
	const int nPlayerSlot = cl->GetPlayerSlot();
	if (gpGlobals && nPlayerSlot >= 0 && nPlayerSlot < gpGlobals->maxClients)
		return detour_CBaseClient_ShouldSendMessages.GetTrampoline<Symbols::CBaseClient_ShouldSendMessages>()(cl);
#endif

	if ( !cl->IsConnected() )
		return false;

	// RaphaelIT7: We want to avoid accessing cl->m_NetChannel directly as it may move in rare cases.
	INetChannel* netChannel = cl->GetNetChannel();

	// if the reliable message overflowed, drop the client
	if ( netChannel && netChannel->IsOverflowed() )
	{
		bool bKick = true;
		if (Lua::PushHook("HolyLib:OnChannelOverflow"))
		{
			Push_CBaseClient(g_Lua, (CBaseClient*)cl);
			if (g_Lua->CallFunctionProtected(2, 1, true))
			{
				bKick = !g_Lua->GetBool(-1);
				g_Lua->Pop(1);
			}

			// The hook can Disconnect clients - the pre-hook local would then
			// dangle (hook_CNetChan_D2 nulls m_NetChannel on destruction, so a
			// re-fetch is authoritative).
			netChannel = cl->GetNetChannel();
			if (!netChannel)
				return false;
		}

		if (bKick)
		{
			netChannel->Reset();
			cl->Disconnect( "%s overflowed reliable buffer", cl->m_Name);
			return false;
		}
	}

	if ( netChannel )
		net_time = netChannel->GetTime(); // Required as we use net_time below.

	// check, if it's time to send the next packet
	bool bSendMessage = cl->m_fNextMessageTime <= net_time;
	if ( !bSendMessage && !cl->IsActive() )
	{
		// if we are in signon modem instantly reply if
		// we got a answer and have reliable data waiting
		if ( cl->m_bReceivedPacket && netChannel && netChannel->HasPendingReliableData() )
		{
			bSendMessage = true;
		}
	}

	if ( bSendMessage && netChannel && !netChannel->CanPacket() )
	{
		// we would like to send a message, but bandwidth isn't available yet
		// tell netchannel that we are choking a packet
		netChannel->SetChoked();
		// Record an ETW event to indicate that we are throttling.
#if ARCHITECTURE_IS_X86
		ETWThrottled();
#endif
		bSendMessage = false;
	}

	if (cl->IsFakeClient() && sv_stressbots && sv_stressbots->GetBool())
		bSendMessage = true;

	return bSendMessage;
}

static Detouring::Hook detour_CBaseServer_CheckTimeouts;
static void hook_CBaseServer_CheckTimeouts(CBaseServer* srv)
{
	if (srv->IsHLTV())
	{
		detour_CBaseServer_CheckTimeouts.GetTrampoline<Symbols::CBaseServer_CheckTimeouts>()(srv);
		return;
	}

	VPROF_BUDGET( "CBaseServer::CheckTimeouts", VPROF_BUDGETGROUP_OTHER_NETWORKING );
	// Don't timeout in _DEBUG builds
	int i;

#if !defined( _DEBUG )
	for (i=0 ; i< srv->GetClientCount() ; i++ )
	{
		IClient	*cl = srv->GetClient(i);

		if ( cl->IsFakeClient() || !cl->IsConnected() )
			continue;

		INetChannel *netchan = cl->GetNetChannel();

		if ( !netchan )
			continue;

		if ( netchan->IsTimedOut() )
		{
			if (Lua::PushHook("HolyLib:OnClientTimeout"))
			{
				Push_CBaseClient(g_Lua, (CBaseClient*)cl);
				if (g_Lua->CallFunctionProtected(2, 1, true))
				{
					float timeoutIncrease = (float)g_Lua->CheckNumberOpt(-1, 0);
					g_Lua->Pop(1);

					// The hook can Disconnect clients (this one included) - the local
					// would then dangle. hook_CNetChan_D2 nulls m_NetChannel on channel
					// destruction, so a re-fetch is authoritative.
					netchan = cl->GetNetChannel();
					if (!netchan)
						continue;

					if (timeoutIncrease > 0)
					{
						netchan->SetTimeout(netchan->GetTimeoutSeconds() + timeoutIncrease);
						continue;
					}
				}
			}
			cl->Disconnect( CLIENTNAME_TIMED_OUT, cl->GetClientName() );
		}
	}
#endif

	for (i=0 ; i< srv->GetClientCount() ; i++ )
	{
		IClient	*cl = srv->GetClient(i);

		if ( cl->IsFakeClient() || !cl->IsConnected() )
			continue;

		if ( cl->GetNetChannel() && cl->GetNetChannel()->IsOverflowed() )
		{
			if (Lua::PushHook("HolyLib:OnChannelOverflow"))
			{
				Push_CBaseClient(g_Lua, (CBaseClient*)cl);
				if (g_Lua->CallFunctionProtected(2, 1, true))
				{
					bool bCancel = g_Lua->GetBool(-1);
					g_Lua->Pop(1);
					if (bCancel)
						continue;
				}

				// The hook can Disconnect clients - don't double-drop.
				if (!cl->IsConnected())
					continue;
			}

			cl->Disconnect( "Client %d overflowed reliable channel.", i );
		}
	}

	/*
	 * Queue clients live in g_pQueueClients, not m_Clients - the loops above never
	 * reach them. Without this, a parked client whose socket died (crashed client,
	 * alt-F4 on the queue screen) is NEVER timed out: Think keeps pumping its
	 * reliable stream until it overflows, and the overflow-disconnect then fires
	 * from inside the send loop, at the worst possible moment. Reap them here with
	 * the same semantics; HolyLib:OnClientTimeout fires like above so Lua can
	 * extend queue waits (which never worked for parked clients before).
	 */
	std::vector<CGameClient*> pQueueSnapshot(g_pQueueClients); // Disconnect -> Lua may mutate the vector
	for (CBaseClient* cl : pQueueSnapshot)
	{
		if ( cl->IsFakeClient() || !cl->IsConnected() )
			continue;

		INetChannel *netchan = cl->GetNetChannel();
		if ( !netchan )
			continue;

		if ( netchan->IsTimedOut() )
		{
			if (Lua::PushHook("HolyLib:OnClientTimeout"))
			{
				Push_CBaseClient(g_Lua, cl);
				if (g_Lua->CallFunctionProtected(2, 1, true))
				{
					float timeoutIncrease = (float)g_Lua->CheckNumberOpt(-1, 0);
					g_Lua->Pop(1);

					/*
					 * The hook runs arbitrary Lua that can Disconnect this (or any)
					 * client - the pre-hook local then points at a freed channel and
					 * SetTimeout below dispatches through a stale vtable (this exact
					 * loop crashed live: crash_2026-07-10_21:07:07). This hook had
					 * never fired for parked clients before, so the queue-side Lua
					 * handlers were unexercised territory. hook_CNetChan_D2 nulls
					 * m_NetChannel on channel destruction, so a re-fetch is
					 * authoritative.
					 */
					netchan = cl->GetNetChannel();
					if (!netchan)
						continue;

					if (timeoutIncrease > 0)
					{
						netchan->SetTimeout(netchan->GetTimeoutSeconds() + timeoutIncrease);
						continue;
					}
				}
			}
			cl->Disconnect( CLIENTNAME_TIMED_OUT, cl->GetClientName() );
			continue;
		}

		if ( netchan->IsOverflowed() )
		{
			if (Lua::PushHook("HolyLib:OnChannelOverflow"))
			{
				Push_CBaseClient(g_Lua, cl);
				if (g_Lua->CallFunctionProtected(2, 1, true))
				{
					bool bCancel = g_Lua->GetBool(-1);
					g_Lua->Pop(1);
					if (bCancel)
						continue;
				}

				// The hook can Disconnect clients - don't double-drop.
				if (!cl->IsConnected())
					continue;
			}

			cl->Disconnect( "Client %d overflowed reliable channel.", ((CBaseClient*)cl)->m_nClientSlot );
		}
	}
}

class CExtendedNetMessage : public CNetMessage
{
public:
	INetMessageHandler *m_pMessageHandler;
};

static std::unordered_set<CGameClient*> g_PromotingQueueClients;
static bool g_bQueuePromotionSlotTableCorrupt = false;

static QueuePromotionResult QueuePromotionFailure(const char* reason, bool retryable = false, int newSlot = -1)
{
	return { false, retryable, newSlot, reason };
}

static QueuePromotionResult QueuePromotionSuccess(int newSlot)
{
	return { true, false, newSlot, nullptr };
}

static bool IsRegisteredQueueClient(CGameClient* candidate)
{
	if (!candidate)
		return false;

	for (CGameClient* queueClient : g_pQueueClients)
	{
		if (queueClient == candidate)
			return true;
	}

	return false;
}

static QueuePromotionResult ValidateQueuePromotionSource(CGameClient* origin)
{
	if (!Util::server || !Util::server->IsActive() || !gpGlobals)
		return QueuePromotionFailure("server_inactive");

	if (g_bClientLayoutMismatch)
		return QueuePromotionFailure("layout_mismatch");

	// Establish ownership by pointer identity before any source dereference.
	if (!IsRegisteredQueueClient(origin))
		return QueuePromotionFailure("not_queue_client");

	if (origin->GetServer() != Util::server || origin->GetServer()->IsHLTV() || origin->IsHLTV() || origin->IsFakeClient())
		return QueuePromotionFailure("not_queue_client");

	if (origin->m_nClientSlot < gpGlobals->maxClients ||
		origin->m_nClientSlot < 0 || origin->m_nClientSlot >= ABSOLUTE_PLAYER_LIMIT)
	{
		return QueuePromotionFailure("invalid_queue_slot");
	}

	if (!origin->IsConnected())
		return QueuePromotionFailure("source_not_connected");

	if (!origin->m_NetChannel)
		return QueuePromotionFailure("source_no_netchannel");

	// Lua blocks the requested PRESPAWN transition before the engine commits it,
	// so a legitimately parked client may still report CONNECTED or NEW here.
	if (origin->m_nSignonState < SIGNONSTATE_CONNECTED ||
		origin->m_nSignonState > SIGNONSTATE_PRESPAWN)
	{
		return QueuePromotionFailure("source_wrong_signon_state");
	}

	return { true, false, -1, nullptr };
}

static QueuePromotionResult LatchSlotTableCorruption(
	int index,
	CBaseClient* client,
	const char* detail,
	int newSlot = -1
)
{
	g_bQueuePromotionSlotTableCorrupt = true;
	if (client)
	{
		Warning(PROJECT_NAME " - gameserver: slot table corrupt at index %i: client=%p slot=%i entity=%i (%s); queue promotion disabled until full restart\n",
			index, (void*)client, client->m_nClientSlot, client->m_nEntityIndex, detail);
	} else {
		Warning(PROJECT_NAME " - gameserver: slot table corrupt at index %i: NULL client (%s); queue promotion disabled until full restart\n",
			index, detail);
	}

	return QueuePromotionFailure("slot_table_corrupt", false, newSlot);
}

static QueuePromotionResult ValidateRealClientSlotTable()
{
	if (g_bQueuePromotionSlotTableCorrupt)
		return QueuePromotionFailure("slot_table_corrupt");

	if (!Util::server || !Util::server->IsActive() || !gpGlobals)
		return QueuePromotionFailure("server_inactive");

	CBaseServer* server = (CBaseServer*)Util::server;
	if (server->m_Clients.Count() > gpGlobals->maxClients)
	{
		return LatchSlotTableCorruption(
			gpGlobals->maxClients,
			server->m_Clients[gpGlobals->maxClients],
			"m_Clients contains objects beyond the real-slot range"
		);
	}

	const int count = MIN(server->m_Clients.Count(), gpGlobals->maxClients);
	for (int i = 0; i < count; ++i)
	{
		CBaseClient* client = server->m_Clients[i];
		if (!client)
			return LatchSlotTableCorruption(i, nullptr, "missing real-slot object");

		if (client->m_nClientSlot != i)
			return LatchSlotTableCorruption(i, client, "m_nClientSlot != array index");

		if (client->m_nEntityIndex != i + 1)
			return LatchSlotTableCorruption(i, client, "m_nEntityIndex != array index + 1");

		if ((CBaseClient*)server->GetClient(i) != client)
			return LatchSlotTableCorruption(i, client, "IServer lookup disagrees with m_Clients");
	}

	return { true, false, -1, nullptr };
}

static int FindConnectedRealClientWithSteamID(CGameClient* origin)
{
	if (!origin || !Util::server || !gpGlobals || !origin->m_SteamID.IsValid())
		return -1;

	CBaseServer* server = (CBaseServer*)Util::server;
	const int count = MIN(server->m_Clients.Count(), gpGlobals->maxClients);
	for (int i = 0; i < count; ++i)
	{
		CBaseClient* realClient = server->m_Clients[i];
		if (realClient != origin && realClient->IsConnected() &&
			realClient->m_SteamID.IsValid() && realClient->m_SteamID == origin->m_SteamID)
		{
			return i;
		}
	}

	return -1;
}

static bool IsFreeRealClientTarget(CGameClient* target)
{
	if (!target || !Util::server || !gpGlobals)
		return false;

	CBaseServer* server = (CBaseServer*)Util::server;
	const int slot = target->m_nClientSlot;
	if (target->GetServer() != Util::server || target->GetServer()->IsHLTV() ||
		slot < 0 || slot >= gpGlobals->maxClients || slot >= server->m_Clients.Count())
	{
		return false;
	}

	if (server->m_Clients[slot] != target || target->m_nEntityIndex != slot + 1)
		return false;

	return target->m_nSignonState == SIGNONSTATE_NONE &&
		!target->IsConnected() &&
		target->m_NetChannel == nullptr &&
		!target->IsHLTV() &&
		!target->IsFakeClient();
}

class ScopedFreeClientLookupFlags
{
public:
	ScopedFreeClientLookupFlags() :
		m_OldNoQueueLookup(g_bNoQueueLookup),
		m_OldDontRunLua(g_bDontRunLuaInFreeClient)
	{
		g_bNoQueueLookup = true;
		g_bDontRunLuaInFreeClient = true;
	}

	~ScopedFreeClientLookupFlags()
	{
		g_bNoQueueLookup = m_OldNoQueueLookup;
		g_bDontRunLuaInFreeClient = m_OldDontRunLua;
	}

private:
	bool m_OldNoQueueLookup;
	bool m_OldDontRunLua;
};

static CGameClient* FindSafeFreeRealClientSlot(const char** reason)
{
	if (reason)
		*reason = nullptr;

	const QueuePromotionResult tableResult = ValidateRealClientSlotTable();
	if (!tableResult.ok)
	{
		if (reason)
			*reason = tableResult.reason;
		return nullptr;
	}

	CBaseServer* server = (CBaseServer*)Util::server;
	int count = MIN(server->m_Clients.Count(), gpGlobals->maxClients);
	for (int i = 0; i < count; ++i)
	{
		CGameClient* target = (CGameClient*)server->m_Clients[i];
		if (IsFreeRealClientTarget(target))
			return target;
	}

	if (server->m_Clients.Count() < gpGlobals->maxClients)
	{
		if (!detour_CBaseServer_GetFreeClient.IsEnabled())
		{
			if (reason)
				*reason = "get_free_client_unavailable";
			return nullptr;
		}

		// Ask the original engine implementation to allocate the next real-slot
		// object. The scoped flags are still set as defence in depth if an engine
		// branch re-enters GetFreeClient; every exit restores their prior values.
		netadrnew_s adr;
		adr.SetType(netadrtype_t::NA_IP);
		adr.SetIP(127, 0, 0, 1);
		adr.SetPort(count);

		CBaseClient* created = nullptr;
		{
			ScopedFreeClientLookupFlags guard;
			created = detour_CBaseServer_GetFreeClient.GetTrampoline<Symbols::CBaseServer_GetFreeClient>()(
				server,
				*((netadr_t*)&adr)
			);
		}

		const QueuePromotionResult postCreateTableResult = ValidateRealClientSlotTable();
		if (!postCreateTableResult.ok)
		{
			if (reason)
				*reason = postCreateTableResult.reason;
			return nullptr;
		}

		CGameClient* target = (CGameClient*)created;
		if (!target)
		{
			if (reason)
				*reason = "no_free_slot";
			return nullptr;
		}

		if (!IsFreeRealClientTarget(target))
		{
			Warning(PROJECT_NAME " - gameserver: target_not_free after real-slot allocation (target=%p slot=%i entity=%i signon=%i connected=%i channel=%p)\n",
				(void*)target,
				target->m_nClientSlot,
				target->m_nEntityIndex,
				target->m_nSignonState,
				target->IsConnected() ? 1 : 0,
				(void*)target->m_NetChannel);
			if (reason)
				*reason = "target_not_free";
			return nullptr;
		}

		return target;
	}

	if (reason)
		*reason = "no_free_slot";
	return nullptr;
}

struct QueueClientTransferState
{
	char name[MAX_PLAYER_NAME_LENGTH];
	char networkID[64];
	char friendsName[MAX_PLAYER_NAME_LENGTH];
	char guid[SIGNED_GUID_LEN + 1];
	char pendingNameChange[MAX_PLAYER_NAME_LENGTH];
	int userID;
	int clientChallenge;
	int signonState;
	int filesDownloaded;
	int signonTick;
	int stringTableAckTick;
	int deltaTick;
	int forceWaitForTick;
	uint32 friendsID;
	CRC32_t sendtableCRC;
	double nextMessageTime;
	double timeLastNameChange;
	float snapshotInterval;
	bool fakePlayer;
	bool reportFakeClient;
	bool receivedPacket;
	bool fullyAuthenticated;
	CSteamID steamID;
	CSteamID ownerSteamID;
	INetChannel* channel;
};

static QueueClientTransferState CaptureQueueClientTransferState(CGameClient* origin)
{
	QueueClientTransferState state = {};
	memcpy(state.name, origin->m_Name, sizeof(state.name));
	state.name[sizeof(state.name) - 1] = '\0';
	const char* networkID = origin->GetNetworkIDString();
	strncpy(state.networkID, networkID ? networkID : "", sizeof(state.networkID));
	state.networkID[sizeof(state.networkID) - 1] = '\0';
	memcpy(state.friendsName, origin->m_FriendsName, sizeof(state.friendsName));
	memcpy(state.guid, origin->m_GUID, sizeof(state.guid));
	memcpy(state.pendingNameChange, origin->m_szPendingNameChange, sizeof(state.pendingNameChange));
	state.userID = origin->m_UserID;
	state.clientChallenge = origin->m_clientChallenge;
	state.signonState = origin->m_nSignonState;
	state.filesDownloaded = origin->m_nFilesDownloaded;
	state.signonTick = origin->m_nSignonTick;
	state.stringTableAckTick = origin->m_nStringTableAckTick;
	state.deltaTick = origin->m_nDeltaTick;
	state.forceWaitForTick = origin->m_nForceWaitForTick;
	state.friendsID = origin->m_nFriendsID;
	state.sendtableCRC = origin->m_nSendtableCRC;
	state.nextMessageTime = origin->m_fNextMessageTime;
	state.timeLastNameChange = origin->m_fTimeLastNameChange;
	state.snapshotInterval = origin->m_fSnapshotInterval;
	state.fakePlayer = origin->m_bFakePlayer;
	state.reportFakeClient = origin->m_bReportFakeClient;
	state.receivedPacket = origin->m_bReceivedPacket;
	state.fullyAuthenticated = origin->m_bFullyAuthenticated;
	state.steamID = origin->m_SteamID;
	state.ownerSteamID = origin->m_OwnerSteamID;
	state.channel = origin->m_NetChannel;
	return state;
}

class ScopedQueuePromotion
{
public:
	explicit ScopedQueuePromotion(CGameClient* origin) : m_Origin(origin)
	{
		g_PromotingQueueClients.insert(origin);
	}

	~ScopedQueuePromotion()
	{
		g_PromotingQueueClients.erase(m_Origin);
	}

private:
	CGameClient* m_Origin;
};

static void DebugQueuePromotion(
	const QueuePromotionResult& result,
	CGameClient* origin,
	CGameClient* target,
	const QueueClientTransferState* state = nullptr
)
{
	if (!g_pGameServerModule.InDebug())
		return;

	const bool sourceKnown = IsRegisteredQueueClient(origin);
	Msg(PROJECT_NAME " - gameserver: queue promotion source=%p slot=%i entity=%i signon=%i userid=%i steamid=%s target=%p slot=%i entity=%i signon=%i status=%s reason=%s retryable=%i\n",
		(void*)origin,
		sourceKnown ? origin->m_nClientSlot : -1,
		sourceKnown ? origin->m_nEntityIndex : -1,
		state ? state->signonState : (sourceKnown ? origin->m_nSignonState : -1),
		state ? state->userID : (sourceKnown ? origin->m_UserID : -1),
		state ? state->networkID : (sourceKnown ? origin->GetNetworkIDString() : "unknown"),
		(void*)target,
		target ? target->m_nClientSlot : -1,
		target ? target->m_nEntityIndex : -1,
		target ? target->m_nSignonState : -1,
		result.ok ? "success" : "failure",
		result.reason ? result.reason : "none",
		result.retryable ? 1 : 0);
}

static void RetirePromotedQueueClient(CGameClient* origin)
{
	// The target owns the channel and all of its handlers before this function is
	// called. Nuke the source reference first so Clear cannot touch that channel.
	origin->m_NetChannel = nullptr;

	// Keep the alias while parked, then detach it at the one boundary where the
	// reusable source shell is about to be cleared.
	DetachParkedQueueClientEdict(origin);

	// A queue slot can alias an unrelated map edict. Never invoke Inactivate or
	// any game-DLL player-removal lifecycle for this reusable parked shell.
	origin->Clear();
}

static QueuePromotionResult CommitQueuePromotion(CGameClient* origin, CGameClient* target)
{
	QueuePromotionResult sourceResult = ValidateQueuePromotionSource(origin);
	if (!sourceResult.ok)
	{
		DebugQueuePromotion(sourceResult, origin, target);
		return sourceResult;
	}

	const int oldSlot = origin->m_nClientSlot;
	const int newSlot = target->m_nClientSlot;
	const QueueClientTransferState state = CaptureQueueClientTransferState(origin);
	ScopedQueuePromotion promotionGuard(origin);

	// Final phase-A checks immediately before the first write. Nothing above has
	// cleared either object or transferred the channel.
	if (!IsFreeRealClientTarget(target))
	{
		// No source or target field has been changed yet, so Lua may safely restore
		// this exact queue source at its original position.
		const QueuePromotionResult result = QueuePromotionFailure("target_not_free", true, newSlot);
		Warning(PROJECT_NAME " - gameserver: target_not_free after queue promotion selection (source=%p slot=%i target=%p slot=%i)\n",
			(void*)origin, oldSlot, (void*)target, newSlot);
		DebugQueuePromotion(result, origin, target, &state);
		return result;
	}

	if (origin->m_NetChannel != state.channel || !state.channel)
	{
		const QueuePromotionResult result = QueuePromotionFailure("source_no_netchannel");
		DebugQueuePromotion(result, origin, target, &state);
		return result;
	}

	// Phase B starts here. target is a verified empty real slot; invoking the
	// game-DLL-facing Inactivate path on it is unnecessary and unsafe.
	target->Clear();
	target->Connect(state.name, state.userID, state.channel, state.fakePlayer, state.clientChallenge);

	// Connect synchronously fires game events/Lua. The channel destructor hook
	// scrubs both clients if those callbacks disconnect it, so compare before any
	// channel dereference.
	if (!target->m_NetChannel || target->m_NetChannel != state.channel)
	{
		const QueuePromotionResult result = QueuePromotionFailure("channel_lost_during_promotion", false, newSlot);
		Warning(PROJECT_NAME " - gameserver: channel_lost_during_promotion after Connect (source=%p slot=%i target=%p slot=%i)\n",
			(void*)origin, oldSlot, (void*)target, newSlot);
		DebugQueuePromotion(result, origin, target, &state);
		return result;
	}

	// Connect has attached the saved channel to target. Rebind every handler
	// immediately, before any later post-commit validation can return: once the
	// target owns the channel, no live message may continue dispatching through
	// the queue source that will be cleared below.
	CNetChan* chan = (CNetChan*)target->m_NetChannel;
	chan->m_MessageHandler = (INetChannelHandler*)target;
	FOR_EACH_VEC(chan->m_NetMessages, i)
	{
		CExtendedNetMessage* msg = (CExtendedNetMessage*)chan->m_NetMessages[i];
		if (!msg)
			continue;

		msg->m_pMessageHandler = target;
		if (msg->GetType() == clc_CmdKeyValues)
		{
			Base_CmdKeyValues* keyVal = (Base_CmdKeyValues*)msg;
			if (keyVal->m_pKeyValues)
			{
				// Ownership cannot be proven after the handler move; retaining the
				// existing defensive null avoids a double free at the cost of the
				// same small leak as the legacy path.
				keyVal->m_pKeyValues = nullptr;
			}
		}
	}

	if (target->m_nClientSlot != newSlot || target->m_nEntityIndex != newSlot + 1)
	{
		const QueuePromotionResult result = LatchSlotTableCorruption(newSlot, target, "target mapping changed during Connect", newSlot);
		// Connect already attached the saved channel to target, so consume only
		// the parked shell. Never invoke its aliased-edict Inactivate path.
		RetirePromotedQueueClient(origin);
		DebugQueuePromotion(result, origin, target, &state);
		return result;
	}

	// Preserve the identity/authentication state that belongs to the connection.
	// Connect establishes the target object's slot-local resources; these fields
	// keep Steam auth, downloads, name-change policy and ownership attached to the
	// same human across the handoff.
	target->SetName(state.name);
	target->m_SteamID = state.steamID;
	target->m_nFriendsID = state.friendsID;
	memcpy(target->m_FriendsName, state.friendsName, sizeof(state.friendsName));
	memcpy(target->m_GUID, state.guid, sizeof(state.guid));
	target->m_nFilesDownloaded = state.filesDownloaded;
	target->m_nSendtableCRC = state.sendtableCRC;
	target->m_bReportFakeClient = state.reportFakeClient;
	target->m_bReceivedPacket = state.receivedPacket;
	target->m_bFullyAuthenticated = state.fullyAuthenticated;
	target->m_OwnerSteamID = state.ownerSteamID;
	target->m_fTimeLastNameChange = state.timeLastNameChange;
	memcpy(target->m_szPendingNameChange, state.pendingNameChange, sizeof(state.pendingNameChange));

	// Preserve the existing, production-proven signon cursor set. Reconnect below
	// restarts the client handshake, while these values keep its pre-reconnect
	// delta/string-table acknowledgement and send cadence coherent until that
	// transition is processed. Baselines/snapshots remain target-slot-local and
	// are intentionally not copied.
	target->m_nSignonState = state.signonState;
	target->m_nSignonTick = state.signonTick;
	target->m_nStringTableAckTick = state.stringTableAckTick;
	target->m_nDeltaTick = state.deltaTick;
	target->m_fNextMessageTime = state.nextMessageTime;
	target->m_fSnapshotInterval = state.snapshotInterval;
	target->m_nForceWaitForTick = state.forceWaitForTick;

	// The handoff is committed. A parked queue source may alias a map edict, so
	// never route it through Inactivate/game-DLL player cleanup. It remains in
	// g_pQueueClients as a disconnected object for GetFreeQueueClient to reuse.
	RetirePromotedQueueClient(origin);

	if (Lua::PushHook("HolyLib:OnPlayerChangedSlot"))
	{
		g_Lua->PushNumber(oldSlot);
		g_Lua->PushNumber(newSlot);
		g_Lua->CallFunctionProtected(3, 0, true);
	}

	if (!target->m_NetChannel || target->m_NetChannel != state.channel)
	{
		const QueuePromotionResult result = QueuePromotionFailure("channel_lost_during_promotion", false, newSlot);
		Warning(PROJECT_NAME " - gameserver: channel_lost_during_promotion after slot-change hook (source=%p slot=%i target=%p slot=%i)\n",
			(void*)origin, oldSlot, (void*)target, newSlot);
		DebugQueuePromotion(result, origin, target, &state);
		return result;
	}

	SVC_ServerInfo info;
	CBaseServer* server = (CBaseServer*)target->GetServer();
	server->FillServerInfo(info);
	info.m_nPlayerSlot = newSlot;
	if (!target->SendNetMsg(info, true))
	{
		const QueuePromotionResult result = QueuePromotionFailure("send_server_info_failed", false, newSlot);
		Warning(PROJECT_NAME " - gameserver: send_server_info_failed during queue promotion (source=%p slot=%i target=%p slot=%i)\n",
			(void*)origin, oldSlot, (void*)target, newSlot);
		DebugQueuePromotion(result, origin, target, &state);
		return result;
	}

	// SendNetMsg can synchronously overflow or tear down the channel. Do not
	// enter Reconnect with a channel pointer that the destructor sweep removed.
	if (!target->m_NetChannel || target->m_NetChannel != state.channel)
	{
		const QueuePromotionResult result = QueuePromotionFailure("channel_lost_during_promotion", false, newSlot);
		Warning(PROJECT_NAME " - gameserver: channel_lost_during_promotion after ServerInfo send (source=%p slot=%i target=%p slot=%i)\n",
			(void*)origin, oldSlot, (void*)target, newSlot);
		DebugQueuePromotion(result, origin, target, &state);
		return result;
	}

	target->Reconnect();
	if (!target->m_NetChannel || target->m_NetChannel != state.channel)
	{
		const QueuePromotionResult result = QueuePromotionFailure("channel_lost_during_promotion", false, newSlot);
		Warning(PROJECT_NAME " - gameserver: channel_lost_during_promotion after Reconnect (source=%p slot=%i target=%p slot=%i)\n",
			(void*)origin, oldSlot, (void*)target, newSlot);
		DebugQueuePromotion(result, origin, target, &state);
		return result;
	}

	const QueuePromotionResult result = QueuePromotionSuccess(newSlot);
	DebugQueuePromotion(result, origin, target, &state);
	return result;
}

static QueuePromotionResult PromoteQueueClientInternal(CGameClient* origin, CGameClient* requestedTarget)
{
	QueuePromotionResult result = ValidateQueuePromotionSource(origin);
	if (!result.ok)
	{
		if (result.reason && strcmp(result.reason, "layout_mismatch") == 0)
			Warning(PROJECT_NAME " - gameserver: layout_mismatch refused queue promotion\n");
		DebugQueuePromotion(result, origin, nullptr);
		return result;
	}

	if (g_PromotingQueueClients.find(origin) != g_PromotingQueueClients.end())
	{
		result = QueuePromotionFailure("promotion_reentrant");
		DebugQueuePromotion(result, origin, requestedTarget);
		return result;
	}

	// Validate the real slot table before trusting an identity match or its slot.
	result = ValidateRealClientSlotTable();
	if (!result.ok)
	{
		DebugQueuePromotion(result, origin, requestedTarget);
		return result;
	}

	// A reconnect can leave the same authenticated identity in a real slot and a
	// parked queue slot. This is final for the queue source: disconnect it here so
	// Lua cannot drop its business entry while leaving a ghost NetChannel behind.
	const int duplicateSlot = FindConnectedRealClientWithSteamID(origin);
	if (duplicateSlot >= 0)
	{
		result = QueuePromotionFailure("steamid_already_in_real_slot", false, duplicateSlot);
		DebugQueuePromotion(result, origin, nullptr);
		origin->Disconnect("Duplicate SteamID already active in real slot %i", duplicateSlot);
		return result;
	}

	CGameClient* target = requestedTarget;
	if (target)
	{
		if (!IsFreeRealClientTarget(target))
		{
			result = QueuePromotionFailure("target_not_free", false,
				target ? target->m_nClientSlot : -1);
			Warning(PROJECT_NAME " - gameserver: target_not_free for explicit queue promotion (source=%p target=%p)\n",
				(void*)origin, (void*)target);
			DebugQueuePromotion(result, origin, target);
			return result;
		}
	} else {
		const char* reason = nullptr;
		target = FindSafeFreeRealClientSlot(&reason);
		if (!target)
		{
			// These failures happen before CommitQueuePromotion performs its first
			// write. Preserve the selected queue source for a later capacity retry.
			const bool retryable = reason && (
				strcmp(reason, "no_free_slot") == 0 ||
				strcmp(reason, "target_not_free") == 0 ||
				strcmp(reason, "get_free_client_unavailable") == 0
			);
			result = QueuePromotionFailure(reason ? reason : "no_free_slot", retryable);
			if (result.reason && strcmp(result.reason, "target_not_free") == 0)
			{
				Warning(PROJECT_NAME " - gameserver: target_not_free after safe-slot allocation (source=%p slot=%i)\n",
					(void*)origin, origin->m_nClientSlot);
			}
			DebugQueuePromotion(result, origin, nullptr);
			return result;
		}
	}

	return CommitQueuePromotion(origin, target);
}

static QueuePromotionResult PromoteQueueClient(CGameClient* origin)
{
	return PromoteQueueClientInternal(origin, nullptr);
}

static QueuePromotionResult PromoteQueueClientIntoTarget(CGameClient* origin, CGameClient* target)
{
	return PromoteQueueClientInternal(origin, target);
}

static Detouring::Hook detour_CGameClient_SpawnPlayer;
static void hook_CGameClient_SpawnPlayer(CGameClient* client)
{
	// m_nClientSlot = player slot! (entIndex - 1)
	if (client->m_nClientSlot < gpGlobals->maxClients || gameserver_disablespawnsafety.GetBool())
	{
		detour_CGameClient_SpawnPlayer.GetTrampoline<Symbols::CGameClient_SpawnPlayer>()(client);
		return;
	}

	// Legacy callers retain their SpawnPlayer entry point, but queue slots now
	// share the same fail-closed implementation as PromoteFromQueue.
	PromoteQueueClient(client);
}

// Called by Util from CSteam3Server::NotifyClientDisconnect
void CGameServerModule::OnClientDisconnect(CBaseClient* pClient)
{
	// NCG: UAF guard (see IsKnownClient) — pClient->GetServer() below is a
	// virtual call, i.e. a jump through the vtable of possibly-freed memory
	// (`segfault at 0 ip 0`). Establish pointer liveness before ANY deref.
	// Membership normally implies GetServer()==Util::server, but keep the
	// original check for its fake/HLTV edge semantics.
	if (!IsKnownClient(pClient))
		return;

	if (pClient->GetServer() != Util::server) // Not our main server
		return;

	if (g_Lua)
	{
		if (Lua::PushHook("HolyLib:OnClientDisconnect"))
		{
			Push_CBaseClient(g_Lua, pClient);
			LuaUserData* pData = Get_CBaseClient_Data(g_Lua, -1, false);
			g_Lua->CallFunctionProtected(2, 0, true);
			if (pData)
				pData->ClearLuaTable(g_Lua);
		}

		// Our IsValid function checks for IsConnected, so technically we don't need to delete the userdata at all
		// But we still usually do because why not
		if (!gameserver_rawclients.GetBool())
			Delete_CBaseClient(g_Lua, pClient);
	}
}

static Detouring::Hook detour_CNetChan_SendDatagram;
static int hook_CNetChan_SendDatagram(CNetChan* chan, bf_write *datagram)
{
	int sequenceNr = detour_CNetChan_SendDatagram.GetTrampoline<Symbols::CNetChan_SendDatagram>()(chan, datagram);

	// NOTE: This code has to be here as moving it into it's own lua function breaks stuff?
	if (g_bFreeSubChannels)
	{
		// Just mark everything as freed >:D
		for (int i = 0; i<MAX_SUBCHANNELS; ++i)
		{
			CNetChan::subChannel_s * subchan = &chan->m_SubChannels[i];

			for (int j=0; j<MAX_STREAMS; ++j)
			{
				if (subchan->numFragments[j] == 0)
					continue;

				// Assert(m_WaitingList[j].Count() > 0);

				CNetChan::dataFragments_t * data = chan->m_WaitingList[j][0];

				// tell waiting list, that we received the acknowledge
				data->ackedFragments += subchan->numFragments[j];
				data->pendingFragments -= subchan->numFragments[j];
			}

			subchan->Free(); // mark subchannel as free again
		}
	}

	return sequenceNr; // return send seq nr
}

static Detouring::Hook detour_Filter_SendBan;
void hook_Filter_SendBan(const netadr_t& adr)
{
	if (sv_filter_nobanresponse.GetBool())
		return;

	detour_Filter_SendBan.GetTrampoline<Symbols::Filter_SendBan>()(adr);
}

void NET_RemoveNetChannel(INetChannel* chan, bool bDeleteNetChan)
{
	if (!func_NET_RemoveNetChannel)
		Error(PROJECT_NAME " - gameserver: Failed to load NET_RemoveNetChannel!\n");

	return func_NET_RemoveNetChannel(chan, bDeleteNetChan);
}

int NET_SendPacket(INetChannel *chan, int sock, const netadr_t &to, const unsigned char *data, int length, bf_write *pVoicePayload /* = nullptr */, bool bUseCompression /*=false*/)
{
	if (!func_NET_SendPacket)
		Error(PROJECT_NAME " - gameserver: Failed to load NET_SendPacket!\n");

	return func_NET_SendPacket(chan, sock, to, data, length, pVoicePayload, bUseCompression);
}

static Symbols::NET_SendStream func_NET_SendStream;
int NET_SendStream(int nSock, const char* buf, int len, int flags)
{
	if (!func_NET_SendStream)
		Error(PROJECT_NAME " - gameserver: Failed to load NET_SendStream!\n");

	return func_NET_SendStream(nSock, buf, len, flags);
}

static Symbols::NET_ReceiveStream func_NET_ReceiveStream;
int NET_ReceiveStream(int nSock, char* buf, int len, int flags)
{
	if (!func_NET_ReceiveStream)
		Error(PROJECT_NAME " - gameserver: Failed to load NET_ReceiveStream!\n");

	return func_NET_ReceiveStream(nSock, buf, len, flags);
}

static ConVar* host_timescale = nullptr;
static Detouring::Hook detour_NET_SetTime;
static void hook_NET_SetTime(double flRealtime) // We need this hook to keep net_time up to date
{
	detour_NET_SetTime.GetTrampoline<Symbols::NET_SetTime>()(flRealtime);

	static double s_last_realtime = 0;

	double frametime = flRealtime - s_last_realtime;
	s_last_realtime = flRealtime;

	if (frametime > 1.0f)
	{
		// if we have very long frame times because of loading stuff
		// don't apply that to net time to avoid unwanted timeouts
		frametime = 1.0f;
	}
	else if (frametime < 0.0f)
	{
		frametime = 0.0f;
	}

	// adjust network time so fakelag works with host_timescale
	net_time += frametime * (host_timescale ? host_timescale->GetFloat() : 1.0f);
}

static Detouring::Hook detour_CGameClient_ExecuteStringCommand;
static bool hook_CGameClient_ExecuteStringCommand(CGameClient* pClient, const char* pCmd)
{
	// Parked clients retain an aliased edict for engine bookkeeping. Never
	// forward it to the game DLL's ClientCommand(edict, ...) callback.
	if (IsParkedQueueClient(pClient))
		return false;

	if (Lua::PushHook("HolyLib:OnClientExecuteStringCommand"))
	{
		Push_CBaseClient(g_Lua, pClient);
		g_Lua->PushString(pCmd);
		if (g_Lua->CallFunctionProtected(3, 1, true))
		{
			bool bSkip = g_Lua->GetBool(-1);
			g_Lua->Pop(1);

			if (bSkip)
				return false;
		}
	}

	return detour_CGameClient_ExecuteStringCommand.GetTrampoline<Symbols::CGameClient_ExecuteStringCommand>()(pClient, pCmd);
}

#if SYSTEM_WINDOWS
DETOUR_THISCALL_START()
	DETOUR_THISCALL_ADDFUNC1(hook_CBaseServer_GetFreeClient, Base_GetFreeClient, CBaseServer*, netadr_t&);
	DETOUR_THISCALL_ADDFUNC1(hook_CBaseServer_CreateFakeClient, Base_CreateFakeClient, CBaseServer*, const char*);
	DETOUR_THISCALL_ADDFUNC1(hook_CBaseServer_UserInfoChanged, Base_UserInfoChanged, CBaseServer*, int);
	DETOUR_THISCALL_ADDFUNC1(hook_CServerPlugin_ClientSettingsChanged, Plugin_ClientSettingsChanged, void*, edict_t*);
	DETOUR_THISCALL_ADDRETFUNC1(hook_CSteam3Server_ClientFindFromSteamID, CBaseClient*, Steam_ClientFindFromSteamID, void*, CSteamID*);
	DETOUR_THISCALL_ADDFUNC1(hook_CGameServer_RemoveClientFromGame, Game_RemoveClientFromGame, CBaseServer*, CBaseClient*);
	DETOUR_THISCALL_ADDFUNC3(hook_CVEngineServer_GMOD_SendToClient, Engine_GMOD_SendToClient, IVEngineServer*, int, void*, int);
	DETOUR_THISCALL_ADDFUNC0(hook_CSteam3Server_SendUpdatedServerDetails, Steam_SendUpdatedServerDetails, void*);
	DETOUR_THISCALL_ADDFUNC0(hook_CBaseServer_CheckTimeouts, CheckTimeouts, CBaseServer*);
	DETOUR_THISCALL_ADDFUNC0(hook_CGameClient_SpawnPlayer, SpawnPlayer, CGameClient*);
	DETOUR_THISCALL_ADDRETFUNC2(hook_CBaseClient_SetSignonState, bool, SetSignonState, CBaseClient*, int, int);
	DETOUR_THISCALL_ADDRETFUNC2(hook_CGameClient_SetSignonState, bool, Game_SetSignonState, CGameClient*, int, int);
	DETOUR_THISCALL_ADDRETFUNC0(hook_CBaseServer_IsMultiplayer, bool, IsMultiplayer, CBaseServer*);
	DETOUR_THISCALL_ADDRETFUNC0(hook_GModDataPack_IsSingleplayer, bool, IsSingleplayer, void*);
	DETOUR_THISCALL_ADDRETFUNC0(hook_CBaseClient_ShouldSendMessages, bool, ShouldSendMessages, CGameClient*);
	DETOUR_THISCALL_ADDRETFUNC1(hook_CBaseServer_ProcessConnectionlessPacket, bool, ProcessConnectionlessPacket, IServer*, netpacket_s*);
	DETOUR_THISCALL_ADDRETFUNC1(hook_CNetChan_SendDatagram, int, SendDatagram, CNetChan*, bf_write*);
	DETOUR_THISCALL_ADDFUNC0(hook_CNetChan_D2, D2, CNetChan*);
	DETOUR_THISCALL_ADDRETFUNC1(hook_CGameClient_ExecuteStringCommand, int, ExecuteStringCommand, CGameClient*, const char*);
DETOUR_THISCALL_FINISH()
#endif

#include "tier0/icommandline.h"
void CGameServerModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	DETOUR_PREPARE_THISCALL();
	SourceSDK::FactoryLoader engine_loader("engine");

#if defined(SYSTEM_LINUX) && defined(ARCHITECTURE_X86_64)
	void* pHostBuildConVarUpdateMessage = Detour::GetFunction(engine_loader.GetModule(), Symbols::Host_BuildConVarUpdateMessageSym);
	ConCommandBase* pIteratorFallback = GetNullCVarIteratorFallback();
	CX64CVarIterator260709 pIterator(g_pCVar);
	void* pIteratorGet = pIterator.GetMethod(5);
	if (!pHostBuildConVarUpdateMessage || !pIterator.IsAvailable() || !pIteratorGet ||
		!pIteratorFallback)
	{
		g_bClientLayoutMismatch = true;
		Warning(PROJECT_NAME " - gameserver: could not verify the x86-64 ConVar iterator ABI - queue-client parking DISABLED!\n");
	} else {
		Detour::CreateAtAddress(
			&detour_CVarIterator_Get, "ICvar::IteratorInternal::Get",
			pIteratorGet, (void*)hook_CVarIterator_Get, m_pID
		);
		if (detour_CVarIterator_Get.IsEnabled())
		{
			g_bSafeCVarIteratorInstalled = true;
			Msg(PROJECT_NAME " - gameserver: installed null-safe x86-64 ConVar iterator for queue sign-on\n");
		} else {
			g_bClientLayoutMismatch = true;
			Warning(PROJECT_NAME " - gameserver: failed to install the x86-64 ConVar iterator guard - queue-client parking DISABLED!\n");
		}
	}
#endif

#if PLATFORM_64BITS
	/*
	 * Verify our compiled CBaseClient mirror against the engine's REAL layout
	 * before any queue-client machinery runs. CBaseClient::SetSignonState's very
	 * first instructions on the x86-64 branch are `push rbp; mov eax, [rdi+disp32]`
	 * (55 8B 87 xx xx xx xx) where disp32 IS the engine's m_nSignonState offset -
	 * the engine tells us the truth directly. A stale mirror (e.g. the removed
	 * avatar-data pad, +0x60 shift on build 260709) makes occupied slots scan as
	 * free and corrupts everything downstream, so on mismatch we disable queue
	 * parking instead of running with wrong offsets. Must run BEFORE the
	 * SetSignonState detour below patches this prologue.
	 *
	 * The CONNECTED branch in the same function also contains
	 * `mov byte ptr [rdi+disp32], 1` (C6 87 xx xx xx xx 01), where disp32 is
	 * m_bSendServerInfo. Verify that field independently: its neighbouring bools
	 * do not affect the aligned m_NetChannel/m_nSignonState offsets, so a
	 * one-field drift can pass the signon-state check while queue clients remain
	 * stuck at CONNECTED forever.
	 */
	{
		void* pSetSignonState = Detour::GetFunction(engine_loader.GetModule(), Symbols::CBaseClient_SetSignonStateSym);
		if (pSetSignonState)
		{
			const unsigned char* pBytes = (const unsigned char*)pSetSignonState;
			if (pBytes[0] == 0x55 && pBytes[1] == 0x8B && pBytes[2] == 0x87)
			{
				int32_t nEngineOffset = 0;
				memcpy(&nEngineOffset, pBytes + 3, sizeof(nEngineOffset));
				int32_t nMirrorOffset = (int32_t)(size_t)&(((CBaseClient*)0)->m_nSignonState);
				if (nEngineOffset != nMirrorOffset)
				{
					g_bClientLayoutMismatch = true;
					Warning(PROJECT_NAME " - gameserver: CBaseClient layout MISMATCH! engine m_nSignonState=0x%X, compiled mirror=0x%X\n", nEngineOffset, nMirrorOffset);
					Warning(PROJECT_NAME " - gameserver: queue-client parking DISABLED - update sourcesdk/baseclient.h for this engine build!\n");
				} else {
					Msg(PROJECT_NAME " - gameserver: verified CBaseClient layout (m_nSignonState @ 0x%X)\n", nEngineOffset);
				}

				int32_t nEngineServerInfoOffset = -1;
				int nServerInfoOffsetMatches = 0;
				for (size_t i = 7; i + 7 <= 0x60; ++i)
				{
					if (pBytes[i] == 0xC6 && pBytes[i + 1] == 0x87 && pBytes[i + 6] == 0x01)
					{
						memcpy(&nEngineServerInfoOffset, pBytes + i + 2, sizeof(nEngineServerInfoOffset));
						++nServerInfoOffsetMatches;
					}
				}

				int32_t nMirrorServerInfoOffset = (int32_t)(size_t)&(((CBaseClient*)0)->m_bSendServerInfo);
				if (nServerInfoOffsetMatches != 1 || nEngineServerInfoOffset != nMirrorServerInfoOffset)
				{
					g_bClientLayoutMismatch = true;
					Warning(PROJECT_NAME " - gameserver: CBaseClient layout MISMATCH! engine m_bSendServerInfo=0x%X (%d matches), compiled mirror=0x%X\n", nEngineServerInfoOffset, nServerInfoOffsetMatches, nMirrorServerInfoOffset);
					Warning(PROJECT_NAME " - gameserver: queue-client parking DISABLED - update sourcesdk/baseclient.h for this engine build!\n");
				} else {
					Msg(PROJECT_NAME " - gameserver: verified CBaseClient layout (m_bSendServerInfo @ 0x%X)\n", nEngineServerInfoOffset);
				}
			} else {
				g_bClientLayoutMismatch = true;
				Warning(PROJECT_NAME " - gameserver: could not verify CBaseClient layout (unexpected SetSignonState prologue) - re-verify field offsets against this engine build!\n");
				Warning(PROJECT_NAME " - gameserver: queue-client parking DISABLED until the layout can be verified!\n");
			}
		} else {
			g_bClientLayoutMismatch = true;
			Warning(PROJECT_NAME " - gameserver: could not resolve CBaseClient::SetSignonState - queue-client parking DISABLED!\n");
		}
	}
#endif

	Detour::Create(
		&detour_CBaseServer_GetFreeClient, "CBaseServer::GetFreeClient",
		engine_loader.GetModule(), Symbols::CBaseServer_GetFreeClientSym,
		(void*)DETOUR_THISCALL(hook_CBaseServer_GetFreeClient, Base_GetFreeClient), m_pID
	);

	Detour::Create(
		&detour_CBaseServer_CreateFakeClient, "CBaseServer::CreateFakeClient",
		engine_loader.GetModule(), Symbols::CBaseServer_CreateFakeClientSym,
		(void*)DETOUR_THISCALL(hook_CBaseServer_CreateFakeClient, Base_CreateFakeClient), m_pID
	);

	Detour::Create(
		&detour_CBaseServer_UserInfoChanged, "CBaseServer::UserInfoChanged",
		engine_loader.GetModule(), Symbols::CBaseServer_UserInfoChangedSym,
		(void*)DETOUR_THISCALL(hook_CBaseServer_UserInfoChanged, Base_UserInfoChanged), m_pID
	);

	Detour::Create(
		&detour_CGameServer_RemoveClientFromGame, "CGameServer::RemoveClientFromGame",
		engine_loader.GetModule(), Symbols::CGameServer_RemoveClientFromGameSym,
		(void*)DETOUR_THISCALL(hook_CGameServer_RemoveClientFromGame, Game_RemoveClientFromGame), m_pID
	);
	if (!detour_CGameServer_RemoveClientFromGame.IsEnabled())
	{
		g_bClientLayoutMismatch = true;
		Warning(PROJECT_NAME " - gameserver: failed to install the queue edict-removal guard - queue-client parking DISABLED!\n");
	}

	Detour::Create(
		&detour_CServerPlugin_ClientSettingsChanged, "CServerPlugin::ClientSettingsChanged",
		engine_loader.GetModule(), Symbols::CServerPlugin_ClientSettingsChangedSym,
		(void*)DETOUR_THISCALL(hook_CServerPlugin_ClientSettingsChanged, Plugin_ClientSettingsChanged), m_pID
	);

	Detour::Create(
		&detour_CSteam3Server_ClientFindFromSteamID, "CSteam3Server::ClientFindFromSteamID",
		engine_loader.GetModule(), Symbols::CSteam3Server_ClientFindFromSteamIDSym,
		(void*)DETOUR_THISCALL(hook_CSteam3Server_ClientFindFromSteamID, Steam_ClientFindFromSteamID), m_pID
	);

	Detour::Create(
		&detour_CVEngineServer_GMOD_SendToClient, "CVEngineServer::GMOD_SendToClient",
		engine_loader.GetModule(), Symbols::CVEngineServer_GMOD_SendToClientSym,
		(void*)DETOUR_THISCALL(hook_CVEngineServer_GMOD_SendToClient, Engine_GMOD_SendToClient), m_pID
	);

	Detour::Create(
		&detour_CSteam3Server_SendUpdatedServerDetails, "CSteam3Server::SendUpdatedServerDetails",
		engine_loader.GetModule(), Symbols::CSteam3Server_SendUpdatedServerDetailsSym,
		(void*)DETOUR_THISCALL(hook_CSteam3Server_SendUpdatedServerDetails, Steam_SendUpdatedServerDetails), m_pID
	);

	Detour::Create(
		&detour_CBaseClient_SetSignonState, "CBaseClient::SetSignonState",
		engine_loader.GetModule(), Symbols::CBaseClient_SetSignonStateSym,
		(void*)DETOUR_THISCALL(hook_CBaseClient_SetSignonState, SetSignonState), m_pID
	);
	if (!detour_CBaseClient_SetSignonState.IsEnabled())
	{
		g_bClientLayoutMismatch = true;
		Warning(PROJECT_NAME " - gameserver: failed to install CBaseClient::SetSignonState hook - queue-client parking DISABLED!\n");
	}

	Detour::Create(
		&detour_CGameClient_SetSignonState, "CGameClient::SetSignonState",
		engine_loader.GetModule(), Symbols::CGameClient_SetSignonStateSym,
		(void*)DETOUR_THISCALL(hook_CGameClient_SetSignonState, Game_SetSignonState), m_pID
	);
	if (!detour_CGameClient_SetSignonState.IsEnabled())
	{
		g_bClientLayoutMismatch = true;
		Warning(PROJECT_NAME " - gameserver: failed to install the pre-CheckConnect queue sign-on hook - queue-client parking DISABLED!\n");
	}

	// Previously x86-gated; the x64 signature resolves (verified unique on GMod
	// x86-64 build 260709, function base 0x61f30 = the exact frame in the
	// 2026-07-10 19:39 queue-servicing crash). Without this detour, x64 ran the
	// RAW engine ShouldSendMessages: no OnChannelOverflow hook, and the raw
	// m_NetChannel field access the hook was written to avoid.
	Detour::Create(
		&detour_CBaseClient_ShouldSendMessages, "CBaseClient::ShouldSendMessages",
		engine_loader.GetModule(), Symbols::CBaseClient_ShouldSendMessagesSym,
		(void*)DETOUR_THISCALL(hook_CBaseClient_ShouldSendMessages, ShouldSendMessages), m_pID
	);

	Detour::Create(
		&detour_CBaseServer_CheckTimeouts, "CBaseServer::CheckTimeouts",
		engine_loader.GetModule(), Symbols::CBaseServer_CheckTimeoutsSym,
		(void*)DETOUR_THISCALL(hook_CBaseServer_CheckTimeouts, CheckTimeouts), m_pID
	);

	Detour::Create(
		&detour_CGameClient_SpawnPlayer, "CGameClient::SpawnPlayer",
		engine_loader.GetModule(), Symbols::CGameClient_SpawnPlayerSym,
		(void*)DETOUR_THISCALL(hook_CGameClient_SpawnPlayer, SpawnPlayer), m_pID
	);
	if (!detour_CGameClient_SpawnPlayer.IsEnabled())
	{
		g_bClientLayoutMismatch = true;
		Warning(PROJECT_NAME " - gameserver: failed to install the queue SpawnPlayer relocation hook - queue-client parking DISABLED!\n");
	}

	Detour::Create(
		&detour_NET_SetTime, "NET_SetTime",
		engine_loader.GetModule(), Symbols::NET_SetTimeSym,
		(void*)hook_NET_SetTime, m_pID
	);

	Detour::Create(
		&detour_CGameClient_ExecuteStringCommand, "CGameClient::ExecuteStringCommand",
		engine_loader.GetModule(), Symbols::CGameClient_ExecuteStringCommandSym,
		(void*)DETOUR_THISCALL(hook_CGameClient_ExecuteStringCommand, ExecuteStringCommand), m_pID
	);
	if (!detour_CGameClient_ExecuteStringCommand.IsEnabled())
	{
		g_bClientLayoutMismatch = true;
		Warning(PROJECT_NAME " - gameserver: failed to install the queue ClientCommand guard - queue-client parking DISABLED!\n");
	}

	SourceSDK::FactoryLoader server_loader("server");
	if (!g_pModuleManager.IsMarkedAsBinaryModule()) // Loaded by require? Then we skip this.
	{
		Detour::Create(
			&detour_CBaseServer_IsMultiplayer, "CBaseServer::IsMultiplayer",
			engine_loader.GetModule(), Symbols::CBaseServer_IsMultiplayerSym,
			(void*)DETOUR_THISCALL(hook_CBaseServer_IsMultiplayer, IsMultiplayer), m_pID
		);

		Detour::Create(
			&detour_GModDataPack_IsSingleplayer, "GModDataPack::IsSingleplayer",
			server_loader.GetModule(), Symbols::GModDataPack_IsSingleplayerSym,
			(void*)DETOUR_THISCALL(hook_GModDataPack_IsSingleplayer, IsSingleplayer), m_pID
		);
	}

	Detour::Create(
		&detour_CBaseServer_ProcessConnectionlessPacket, "CBaseServer::ProcessConnectionlessPacket",

		engine_loader.GetModule(), Symbols::CBaseServer_ProcessConnectionlessPacketSym,
		(void*)DETOUR_THISCALL(hook_CBaseServer_ProcessConnectionlessPacket, ProcessConnectionlessPacket), m_pID
	);

	func_CBaseClient_OnRequestFullUpdate = (Symbols::CBaseClient_OnRequestFullUpdate)Detour::GetFunction(engine_loader.GetModule(), Symbols::CBaseClient_OnRequestFullUpdateSym);
	Detour::CheckFunction((void*)func_CBaseClient_OnRequestFullUpdate, "CBaseClient::OnRequestFullUpdate");

	/*
	 * CNetChan related stuff
	 */

	Detour::Create(
		&detour_CNetChan_D2, "CNetChan::~CNetChan",
		engine_loader.GetModule(), Symbols::CNetChan_D2Sym,
		(void*)DETOUR_THISCALL(hook_CNetChan_D2, D2), m_pID
	);

	func_NET_CreateNetChannel = (Symbols::NET_CreateNetChannel)Detour::GetFunction(engine_loader.GetModule(), Symbols::NET_CreateNetChannelSym);
	Detour::CheckFunction((void*)func_NET_CreateNetChannel, "NET_CreateNetChannel");

	func_NET_RemoveNetChannel = (Symbols::NET_RemoveNetChannel)Detour::GetFunction(engine_loader.GetModule(), Symbols::NET_RemoveNetChannelSym);
	Detour::CheckFunction((void*)func_NET_RemoveNetChannel, "NET_RemoveNetChannel");

	/*
	 * Everything below are networking related changes, when the next gmod update is out we should be able to remove most of it if rubat implements https://github.com/Facepunch/garrysmod-requests/issues/2632
	 */

	Detour::Create(
		&detour_CNetChan_SendDatagram, "CNetChan::SendDatagram",
		engine_loader.GetModule(), Symbols::CNetChan_SendDatagramSym,
		(void*)DETOUR_THISCALL(hook_CNetChan_SendDatagram, SendDatagram), m_pID
	);

	Detour::Create(
		&detour_Filter_SendBan, "Filter_SendBan",
		engine_loader.GetModule(), Symbols::Filter_SendBanSym,
		(void*)hook_Filter_SendBan, m_pID
	);

	func_NET_SendPacket = (Symbols::NET_SendPacket)Detour::GetFunction(engine_loader.GetModule(), Symbols::NET_SendPacketSym);
	Detour::CheckFunction((void*)func_NET_SendPacket, "NET_SendPacket");

	func_NET_SendStream = (Symbols::NET_SendStream)Detour::GetFunction(engine_loader.GetModule(), Symbols::NET_SendStreamSym);
	Detour::CheckFunction((void*)func_NET_SendStream, "NET_SendStream");

	func_NET_ReceiveStream = (Symbols::NET_ReceiveStream)Detour::GetFunction(engine_loader.GetModule(), Symbols::NET_ReceiveStreamSym);
	Detour::CheckFunction((void*)func_NET_ReceiveStream, "NET_ReceiveStream");

#if defined(ARCHITECTURE_X86) && defined(SYSTEM_LINUX)
	s_NetChannels = Detour::ResolveSymbol<CUtlVectorMT<CUtlVector<CNetChan*>>>(engine_loader, Symbols::s_NetChannelsSym);
#else
	s_NetChannels = Detour::ResolveSymbolWithOffset<CUtlVectorMT<CUtlVector<CNetChan*>>>(engine_loader.GetModule(), Symbols::s_NetChannelsSym);
#endif

	host_timescale = g_pCVar ? g_pCVar->FindVar("host_timescale") : nullptr;
}
