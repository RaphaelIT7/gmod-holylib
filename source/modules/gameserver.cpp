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

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

double		net_time;

class CGameServerModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "gameserver"; };
	virtual int Compatibility() { return LINUX32; };
	virtual bool SupportsMultipleLuaStates() { return true; };
};

static ConVar gameserver_disablespawnsafety("holylib_gameserver_disablespawnsafety", "0", 0, "If enabled, players can spawn on slots above 128 but this WILL cause stability and many other issues!");
static ConVar gameserver_connectionlesspackethook("holylib_gameserver_connectionlesspackethook", "1", 0, "If enabled, the HolyLib:ProcessConnectionlessPacket hook is active and will be called.");
static ConVar sv_filter_nobanresponse("sv_filter_nobanresponse", "0", 0, "If enabled, a blocked ip won't be informed that its even blocked.");

CGameServerModule g_pGameServerModule;
IModule* pGameServerModule = &g_pGameServerModule;

class SVC_CustomMessage : public CNetMessage
{
public:
	bool			ReadFromBuffer( bf_read &buffer ) { return true; };
	bool			WriteToBuffer( bf_write &buffer ) {
		if (m_iLength == -1)
			m_iLength = m_DataOut.GetNumBitsWritten();

		buffer.WriteUBitLong(GetType(), NETMSG_TYPE_BITS);
		return buffer.WriteBits(m_DataOut.GetData(), m_iLength);
	};
	const char		*ToString() const { return PROJECT_NAME ":CustomMessage"; };
	int				GetType() const { return m_iType; }
	const char		*GetName() const { return m_strName; }

	INetMessageHandler *m_pMessageHandler = NULL;
	bool Process() { Warning(PROJECT_NAME ": Tried to process this message? This should never happen!\n"); return true; };

	SVC_CustomMessage() { m_bReliable = false; }

	int	GetGroup() const { return INetChannelInfo::GENERIC; }

	int m_iType = 0;
	int m_iLength = -1;
	char m_strName[64] = "";
	bf_write m_DataOut;
};

PushReferenced_LuaClass(CBaseClient)
SpecialGet_LuaClass(CBaseClient, CHLTVClient, "CBaseClient")

Default__index(CBaseClient);
Default__newindex(CBaseClient);

LUA_FUNCTION_STATIC(CBaseClient_GetTable)
{
	LuaUserData* data = Get_CBaseClient_Data(LUA, 1, true);
	CBaseClient* pClient = (CBaseClient*)data->GetData();
	if (data->GetAdditionalData() != pClient->GetUserID())
	{
		data->SetAdditionalData(pClient->GetUserID());
		data->ClearLuaTable();
	}

	Util::ReferencePush(LUA, data->GetLuaTable()); // This should never crash so no safety checks.

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

LUA_FUNCTION_STATIC(CBaseClient_IsValid)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, false);
	
	LUA->PushBool(pClient != NULL && pClient->IsConnected());
	return 1;
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
	IGameEvent* pEvent = Get_IGameEvent(LUA, 2, true);

	pClient->FireGameEvent(pEvent);
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
		pClient->GetNetChannel()->Shutdown(NULL); // NULL = Send no disconnect message

	if (bNoEvent)
		BlockGameEvent("player_disconnect");

	pClient->Disconnect(strReason);
	
	if (bNoEvent)
		UnblockGameEvent("player_disconnect");

	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_SetRate)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	int nRate = LUA->CheckNumber(2);
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
	int nUpdateRate = LUA->CheckNumber(2);
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
	int iType = LUA->CheckNumber(2);
	const char* strName = LUA->CheckString(3);
	bf_write* bf = Get_bf_write(LUA, 4, true);

	if (!pClient)
		LUA->ThrowError("Failed to get IClient from player!");

	SVC_CustomMessage msg;
	msg.m_iType = iType;
	strcpy(msg.m_strName, strName);
	msg.m_DataOut.StartWriting(bf->GetData(), 0, 0, bf->GetMaxNumBits());
	msg.m_iLength = bf->GetNumBitsWritten();

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
	int nPlayerSlot = LUA->CheckNumber(2);

	LUA->PushBool(pClient->IsHearingClient(nPlayerSlot));
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_IsProximityHearingClient)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	int nPlayerSlot = LUA->CheckNumber(2);

	LUA->PushBool(pClient->IsProximityHearingClient(nPlayerSlot));
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_SetMaxRoutablePayloadSize)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	int nMaxRoutablePayloadSize = LUA->CheckNumber(2);

	pClient->SetMaxRoutablePayloadSize(nMaxRoutablePayloadSize);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_UpdateAcknowledgedFramecount)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	int nTick = LUA->CheckNumber(2);

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
	int iSignOnState = LUA->CheckNumber(2);
	int iSpawnCount = LUA->GetNumber(3);
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
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	bf_write* bf = Get_bf_write(LUA, 2, true);

	pClient->WriteGameSounds(*bf);
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

	pClient->SendSnapshot(NULL);
	return 0;
}*/

LUA_FUNCTION_STATIC(CBaseClient_SendServerInfo)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	pClient->SendServerInfo();
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
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);

	if (func_CBaseClient_OnRequestFullUpdate)
		func_CBaseClient_OnRequestFullUpdate(pClient);

	return 0;
}

void CBaseClient::SetSteamID( const CSteamID &steamID )
{
	m_SteamID = steamID;
}

LUA_FUNCTION_STATIC(CBaseClient_SetSteamID)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	const char* steamID64 = LUA->CheckString(2);
	uint64 steamID = strtoull(steamID64, NULL, 0);

	if (steamID == 0)
	{
		LUA->PushBool(false);
		return 1;
	}

	pClient->SetSteamID(CSteamID(steamID));
	LUA->PushBool(true);
	return 1;
}

/*
 * CNetChannel exposed things.
 * I should probably move it into a seperate class...
 */
LUA_FUNCTION_STATIC(CBaseClient_GetProcessingMessages)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushBool(pNetChannel->m_bProcessingMessages);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetClearedDuringProcessing)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushBool(pNetChannel->m_bClearedDuringProcessing);
	return 1;
}

// If anyone sees a point in having this function, open a issue and ask for it to be added.
/*LUA_FUNCTION_STATIC(CBaseClient_GetShouldDelete)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushBool(pNetChannel->m_bShouldDelete);
	return 1;
}*/

LUA_FUNCTION_STATIC(CBaseClient_GetOutSequenceNr)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_nOutSequenceNr);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetInSequenceNr)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_nInSequenceNr);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetOutSequenceNrAck)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_nOutSequenceNrAck);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetOutReliableState)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_nOutReliableState);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetInReliableState)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_nInReliableState);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetChokedPackets)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_nChokedPackets);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetStreamReliable)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	Push_bf_write(LUA, &pNetChannel->m_StreamReliable);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetStreamUnreliable)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	Push_bf_write(LUA, &pNetChannel->m_StreamUnreliable);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetStreamVoice)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	Push_bf_write(LUA, &pNetChannel->m_StreamVoice);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetStreamSocket)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_StreamSocket);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetMaxReliablePayloadSize)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_MaxReliablePayloadSize);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetLastReceived)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->last_received);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetConnectTime)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->connect_time);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetClearTime)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_fClearTime);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetTimeout)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_Timeout);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_SetTimeout)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	int seconds = LUA->CheckNumber(2);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	pNetChannel->SetTimeout(seconds);
	return 0;
}

static int g_pMaxFragments = -1;
static bool g_bFreeSubChannels = false;
LUA_FUNCTION_STATIC(CBaseClient_Transmit)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	bool bOnlyReliable = LUA->GetBool(2);
	int maxFragments = (int)LUA->CheckNumberOpt(3, -1);
	bool bFreeSubChannels = LUA->GetBool(4);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	g_pMaxFragments = maxFragments;
	g_bFreeSubChannels = bFreeSubChannels;
	LUA->PushBool(pNetChannel->Transmit(bOnlyReliable));
	g_bFreeSubChannels = false;
	g_pMaxFragments = -1;

	return 1;
}

/*LUA_FUNCTION_STATIC(CBaseClient_HasQueuedPackets)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushBool(pNetChannel->HasQueuedPackets());
	return 1;
}*/

LUA_FUNCTION_STATIC(CBaseClient_ProcessStream)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushBool(pNetChannel->ProcessStream());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_SetMaxBufferSize)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	bool bReliable = LUA->GetBool(2);
	int nBytes = LUA->CheckNumber(3);
	bool bVoice = LUA->GetBool(4);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	pNetChannel->SetMaxBufferSize(bReliable, nBytes, bVoice);
	return 0;
}

// Purely debug function, has no real use.
/*LUA_FUNCTION_STATIC(CBaseClient_GetRegisteredMessages)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

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
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->GetMaxRoutablePayloadSize());
	return 1;
}

// Added for CHLTVClient to inherit functions.
void Push_CBaseClientMeta(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::AddFunc(pLua, CBaseClient__newindex, "__newindex");
	Util::AddFunc(pLua, CBaseClient__index, "__index");
	Util::AddFunc(pLua, CBaseClient_GetTable, "GetTable");

	Util::AddFunc(pLua, CBaseClient_GetPlayerSlot, "GetPlayerSlot");
	Util::AddFunc(pLua, CBaseClient_GetUserID, "GetUserID");
	Util::AddFunc(pLua, CBaseClient_GetName, "GetName");
	Util::AddFunc(pLua, CBaseClient_GetSteamID, "GetSteamID");
	Util::AddFunc(pLua, CBaseClient_Reconnect, "Reconnect");
	Util::AddFunc(pLua, CBaseClient_ClientPrint, "ClientPrint");
	Util::AddFunc(pLua, CBaseClient_IsValid, "IsValid");
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
	Util::AddFunc(pLua, CBaseClient_SendSignonData, "SendSignonData");
	Util::AddFunc(pLua, CBaseClient_SpawnPlayer, "SpawnPlayer");
	Util::AddFunc(pLua, CBaseClient_ActivatePlayer, "ActivatePlayer");
	Util::AddFunc(pLua, CBaseClient_SetName, "SetName");
	Util::AddFunc(pLua, CBaseClient_SetUserCVar, "SetUserCVar");
	Util::AddFunc(pLua, CBaseClient_FreeBaselines, "FreeBaselines");
	Util::AddFunc(pLua, CBaseClient_OnRequestFullUpdate, "OnRequestFullUpdate");
	Util::AddFunc(pLua, CBaseClient_SetSteamID, "SetSteamID");

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
	Util::AddFunc(pLua, CBaseClient_Transmit, "Transmit");
	Util::AddFunc(pLua, CBaseClient_ProcessStream, "ProcessStream");
	//Util::AddFunc(pLua, CBaseClient_GetRegisteredMessages, "GetRegisteredMessages");
	Util::AddFunc(pLua, CBaseClient_SetMaxBufferSize, "SetMaxBufferSize");
	//Util::AddFunc(pLua, CBaseClient_HasQueuedPackets, "HasQueuedPackets");
	Util::AddFunc(pLua, CBaseClient_GetMaxRoutablePayloadSize, "GetMaxRoutablePayloadSize");
}

LUA_FUNCTION_STATIC(CGameClient__tostring)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, false);
	if (!pClient || !pClient->IsConnected())
	{
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

LUA_FUNCTION_STATIC(CNetChan_IsValid)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, false);
	
	LUA->PushBool(pNetChannel != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetAvgLoss)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	int flow = LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetAvgLoss(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetAvgChoke)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	int flow = LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetAvgChoke(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetAvgData)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	int flow = LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetAvgData(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetAvgLatency)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	int flow = LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetAvgLatency(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetAvgPackets)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	int flow = LUA->CheckNumber(2);

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
	int rate = LUA->CheckNumber(2);

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
	int flow = LUA->CheckNumber(2);

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
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	Push_bf_write(LUA, &pNetChannel->m_StreamReliable);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetStreamUnreliable)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	Push_bf_write(LUA, &pNetChannel->m_StreamUnreliable);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetStreamVoice)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	Push_bf_write(LUA, &pNetChannel->m_StreamVoice);
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
	int seconds = LUA->CheckNumber(2);

	pNetChannel->SetTimeout(seconds);
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_Transmit)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	bool bOnlyReliable = LUA->GetBool(2);
	int maxFragments = (int)LUA->CheckNumberOpt(3, -1);
	bool bFreeSubChannels = LUA->GetBool(4);

	g_pMaxFragments = maxFragments;
	g_bFreeSubChannels = bFreeSubChannels;
	LUA->PushBool(pNetChannel->Transmit(bOnlyReliable));
	g_bFreeSubChannels = false;
	g_pMaxFragments = -1;

	return 1;
}

/*LUA_FUNCTION_STATIC(CNetChan_HasQueuedPackets)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

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
	int nBytes = LUA->CheckNumber(3);
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
	const char* reason = LUA->CheckStringOpt(2, NULL);

	pNetChannel->Shutdown(reason);
	return 0;
}

static Detouring::Hook detour_CNetChan_D2;
void hook_CNetChan_D2(CNetChan* pNetChan)
{
	if (!ThreadInMainThread())
	{
		Warning(PROJECT_NAME ": CNetChan was deleted from another thread...\n");
		return;
	}

	Delete_CNetChan(g_Lua, pNetChan);

	detour_CNetChan_D2.GetTrampoline<Symbols::CNetChan_D2>()(pNetChan);
}

class NET_LuaNetChanMessage;
class ILuaNetMessageHandler : INetChannelHandler
{
public:
	ILuaNetMessageHandler(GarrysMod::Lua::ILuaInterface* pLua);
	virtual ~ILuaNetMessageHandler();

	virtual void ConnectionStart(INetChannel *chan);	// called first time network channel is established
	virtual void ConnectionClosing(const char *reason); // network channel is being closed by remote site
	virtual void ConnectionCrashed(const char *reason); // network error occured
	virtual void PacketStart(int incoming_sequence, int outgoing_acknowledged);	// called each time a new packet arrived
	virtual void PacketEnd(void); // all messages has been parsed
	virtual void FileRequested(const char *fileName, unsigned int transferID ); // other side request a file for download
	virtual void FileReceived(const char *fileName, unsigned int transferID ); // we received a file
	virtual void FileDenied(const char *fileName, unsigned int transferID );	// a file request was denied by other side
	virtual void FileSent(const char *fileName, unsigned int transferID );	// we sent a file
	virtual bool ShouldAcceptFile(const char *fileName, unsigned int transferID);

	virtual bool ProcessLuaNetChanMessage( [[maybe_unused]] NET_LuaNetChanMessage *msg );

public:
	CNetChan* chan = NULL;
	NET_LuaNetChanMessage* luaNetChanMessage = NULL;
	int messageCallbackFunction = -1;
	int connectionStartFunction = -1;
	int connectionClosingFunction = -1;
	int connectionCrashedFunction = -1;
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

	ILuaNetMessageHandler *m_pMessageHandler = NULL;
	bool Process() { return m_pMessageHandler->ProcessLuaNetChanMessage( this ); };

	NET_LuaNetChanMessage() { m_bReliable = true; }

	int	GetGroup() const { return INetChannelInfo::GENERIC; }

	int m_iLength = -1;
	bf_write m_DataOut;
	bf_read m_DataIn;
};

static std::unordered_set<ILuaNetMessageHandler*> g_pNetMessageHandlers;
ILuaNetMessageHandler::ILuaNetMessageHandler(GarrysMod::Lua::ILuaInterface* pLua)
{
	luaNetChanMessage = new NET_LuaNetChanMessage;
	luaNetChanMessage->m_pMessageHandler = this;
	g_pNetMessageHandlers.insert(this);
	m_pLua = pLua;
}

ILuaNetMessageHandler::~ILuaNetMessageHandler()
{
	if (luaNetChanMessage)
	{
		delete luaNetChanMessage;
		luaNetChanMessage = NULL;
	}

	g_pNetMessageHandlers.erase(this);

	if (!ThreadInMainThread())
	{
		Warning(PROJECT_NAME ": Tried to delete a ILuaNetMessageHandler from another thread! How could you! Now were leaking a reference...\n");
		return;
	}

	if (messageCallbackFunction != -1)
	{
		Util::ReferenceFree(m_pLua, messageCallbackFunction, "ILuaNetMessageHandler::~ILuaNetMessageHandler");
		messageCallbackFunction = -1;
	}

	if (connectionStartFunction != -1)
	{
		Util::ReferenceFree(m_pLua, connectionStartFunction, "ILuaNetMessageHandler::~ILuaNetMessageHandler");
		connectionStartFunction = -1;
	}

	if (connectionClosingFunction != -1)
	{
		Util::ReferenceFree(m_pLua, connectionClosingFunction, "ILuaNetMessageHandler::~ILuaNetMessageHandler");
		connectionClosingFunction = -1;
	}

	if (connectionCrashedFunction != -1)
	{
		Util::ReferenceFree(m_pLua, connectionCrashedFunction, "ILuaNetMessageHandler::~ILuaNetMessageHandler");
		connectionCrashedFunction = -1;
	}
}

void ILuaNetMessageHandler::ConnectionStart(INetChannel* chan)
{
	if (!ThreadInMainThread())
	{
		Warning(PROJECT_NAME ": Trying to call ConnectionStart outside the main thread!\n");
		return;
	}

	if (connectionStartFunction == -1) // We have no callback function set.
		return;

	m_pLua->ReferencePush(connectionStartFunction);
	Push_CNetChan(m_pLua, (CNetChan*)chan);
	m_pLua->CallFunctionProtected(1, 0, true);
}

void ILuaNetMessageHandler::ConnectionClosing(const char* reason)
{
	if (!ThreadInMainThread())
	{
		Warning(PROJECT_NAME ": Trying to call ConnectionStart outside the main thread!\n");
		return;
	}

	if (connectionClosingFunction == -1) // We have no callback function set.
		return;

	m_pLua->ReferencePush(connectionClosingFunction);
	Push_CNetChan(m_pLua, chan);
	m_pLua->PushString(reason);
	m_pLua->CallFunctionProtected(2, 0, true);
}

void ILuaNetMessageHandler::ConnectionCrashed(const char* reason)
{
	if (!ThreadInMainThread())
	{
		Warning(PROJECT_NAME ": Trying to call ConnectionStart outside the main thread!\n");
		return;
	}

	if (connectionCrashedFunction == -1) // We have no callback function set.
		return;

	m_pLua->ReferencePush(connectionCrashedFunction);
	Push_CNetChan(m_pLua, chan);
	m_pLua->PushString(reason);
	m_pLua->CallFunctionProtected(2, 0, true);
}

void ILuaNetMessageHandler::PacketStart(int incoming_sequence, int outgoing_acknowledged)
{
	//Msg("ILuaNetMessageHandler::PacketStart - %i | %i\n", incoming_sequence, outgoing_acknowledged);
}

void ILuaNetMessageHandler::PacketEnd()
{
	//Msg("ILuaNetMessageHandler::PacketEnd\n");
}

void ILuaNetMessageHandler::FileRequested(const char *fileName, unsigned int transferID)
{
	//Msg("ILuaNetMessageHandler::FileRequested - %s | %d\n", fileName, transferID);
}

void ILuaNetMessageHandler::FileReceived(const char *fileName, unsigned int transferID)
{
	//Msg("ILuaNetMessageHandler::FileReceived - %s | %d\n", fileName, transferID);
}

void ILuaNetMessageHandler::FileDenied(const char *fileName, unsigned int transferID)
{
	//Msg("ILuaNetMessageHandler::FileDenied - %s | %d\n", fileName, transferID);
}

void ILuaNetMessageHandler::FileSent(const char *fileName, unsigned int transferID)
{
	//Msg("ILuaNetMessageHandler::FileSent - %s | %d\n", fileName, transferID);
}

bool ILuaNetMessageHandler::ShouldAcceptFile(const char *fileName, unsigned int transferID)
{
	//Msg("ILuaNetMessageHandler::ShouldAcceptFile - %s | %d\n", fileName, transferID);
	return true;
}

bool ILuaNetMessageHandler::ProcessLuaNetChanMessage(NET_LuaNetChanMessage *msg)
{
	if (!ThreadInMainThread())
	{
		Warning(PROJECT_NAME ": Trying to process a lua net channel message outside the main thread!\n");
		return false;
	}

	if (messageCallbackFunction == -1) // We have no callback function set.
		return true;

	LuaUserData* pLuaData = Push_bf_read(m_pLua, &msg->m_DataIn);
	m_pLua->ReferencePush(messageCallbackFunction);

	Push_CNetChan(m_pLua, chan);
	m_pLua->Push(-3);
	m_pLua->PushNumber(msg->m_iLength);
	m_pLua->CallFunctionProtected(3, 0, true);
		
	if (pLuaData)
	{
		delete pLuaData;
	}
	m_pLua->SetUserType(-1, NULL);
	m_pLua->Pop(1);

	return true;
}

LUA_FUNCTION_STATIC(CNetChan_SendMessage)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	bf_write* bf = Get_bf_write(LUA, 2, true);
	bool bReliable = LUA->GetBool(3);

	NET_LuaNetChanMessage msg;
	msg.m_DataOut.StartWriting(bf->GetData(), 0, 0, bf->GetMaxNumBits());
	msg.m_iLength = bf->GetNumBitsWritten();

	LUA->PushBool(pNetChannel->SendNetMsg(msg, bReliable));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_SetMessageCallback)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	ILuaNetMessageHandler* pHandler = (ILuaNetMessageHandler*)pNetChannel->m_MessageHandler;
	LUA->CheckType(2, GarrysMod::Lua::Type::Function);
	
	if (!pHandler)
		return 0;

	if (pHandler->messageCallbackFunction != -1)
	{
		Util::ReferenceFree(LUA, pHandler->messageCallbackFunction, "CNetChan:SetCallback");
	}

	LUA->Push(2);
	pHandler->messageCallbackFunction = Util::ReferenceCreate(LUA, "CNetChan:SetCallback");
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_GetMessageCallback)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	ILuaNetMessageHandler* pHandler = (ILuaNetMessageHandler*)pNetChannel->m_MessageHandler;
	
	if (pHandler && pHandler->messageCallbackFunction != -1)
	{
		Util::ReferencePush(LUA, pHandler->messageCallbackFunction);
	} else {
		LUA->PushNil();
	}
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_SetConnectionStartCallback)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	ILuaNetMessageHandler* pHandler = (ILuaNetMessageHandler*)pNetChannel->m_MessageHandler;
	LUA->CheckType(2, GarrysMod::Lua::Type::Function);
	
	if (!pHandler)
		return 0;

	if (pHandler->connectionStartFunction != -1)
	{
		Util::ReferenceFree(LUA, pHandler->connectionStartFunction, "CNetChan:SetCallback");
	}

	LUA->Push(2);
	pHandler->connectionStartFunction = Util::ReferenceCreate(LUA, "CNetChan:SetCallback");
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_GetConnectionStartCallback)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	ILuaNetMessageHandler* pHandler = (ILuaNetMessageHandler*)pNetChannel->m_MessageHandler;
	
	if (pHandler && pHandler->connectionStartFunction != -1)
	{
		Util::ReferencePush(LUA, pHandler->connectionStartFunction);
	} else {
		LUA->PushNil();
	}
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_SetConnectionClosingCallback)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	ILuaNetMessageHandler* pHandler = (ILuaNetMessageHandler*)pNetChannel->m_MessageHandler;
	LUA->CheckType(2, GarrysMod::Lua::Type::Function);
	
	if (!pHandler)
		return 0;

	if (pHandler->connectionClosingFunction != -1)
	{
		Util::ReferenceFree(LUA, pHandler->connectionClosingFunction, "CNetChan:SetCallback");
	}

	LUA->Push(2);
	pHandler->connectionClosingFunction = Util::ReferenceCreate(LUA, "CNetChan:SetCallback");
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_GetConnectionClosingCallback)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	ILuaNetMessageHandler* pHandler = (ILuaNetMessageHandler*)pNetChannel->m_MessageHandler;
	
	if (pHandler && pHandler->connectionClosingFunction != -1)
	{
		Util::ReferencePush(LUA, pHandler->connectionClosingFunction);
	} else {
		LUA->PushNil();
	}
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_SetConnectionCrashedCallback)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	ILuaNetMessageHandler* pHandler = (ILuaNetMessageHandler*)pNetChannel->m_MessageHandler;
	LUA->CheckType(2, GarrysMod::Lua::Type::Function);
	
	if (!pHandler)
		return 0;

	if (pHandler->connectionCrashedFunction != -1)
	{
		Util::ReferenceFree(LUA, pHandler->connectionCrashedFunction, "CNetChan:SetCallback");
	}

	LUA->Push(2);
	pHandler->connectionCrashedFunction = Util::ReferenceCreate(LUA, "CNetChan:SetCallback");
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_GetConnectionCrashedCallback)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);
	ILuaNetMessageHandler* pHandler = (ILuaNetMessageHandler*)pNetChannel->m_MessageHandler;
	
	if (pHandler && pHandler->connectionCrashedFunction != -1)
	{
		Util::ReferencePush(LUA, pHandler->connectionCrashedFunction);
	} else {
		LUA->PushNil();
	}
	return 1;
}

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
		return 0;

	CBaseClient* pClient = (CBaseClient*)((IServer*)Util::server)->GetClient(iClientIndex);
	if (pClient && !pClient->IsConnected())
		pClient = NULL;

	Push_CBaseClient(LUA, pClient);

	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetClientCount)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushNumber(Util::server->GetClientCount());
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
			if (!pClient->IsConnected())
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

LUA_FUNCTION_STATIC(gameserver_GetName)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushString(Util::server->GetName());
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetMapName)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	LUA->PushString(Util::server->GetMapName());
	return 1;
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

	int nSlots = LUA->CheckNumber(1);

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

	int iType = LUA->CheckNumber(1);
	const char* strName = LUA->CheckString(2);
	bf_write* bf = Get_bf_write(LUA, 3, true);

	SVC_CustomMessage msg;
	msg.m_iType = iType;
	strcpy(msg.m_strName, strName);
	msg.m_DataOut.StartWriting(bf->GetData(), 0, 0, bf->GetMaxNumBits());
	msg.m_iLength = bf->GetNumBitsWritten();

	Util::server->BroadcastMessage(msg);
	return 0;
}

LUA_FUNCTION_STATIC(gameserver_CalculateCPUUsage)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	CBaseServer* pServer = (CBaseServer*)Util::server;
	pServer->CalculateCPUUsage();
	LUA->PushNumber(pServer->m_fCPUPercent);
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_ApproximateProcessMemoryUsage)
{
	LUA->PushNumber(ApproximateProcessMemoryUsage());
	return 1;
}

static Symbols::NET_SendPacket func_NET_SendPacket;
LUA_FUNCTION_STATIC(gameserver_SendConnectionlessPacket)
{
	bf_write* msg = Get_bf_write(LUA, 1, true);

	netadr_t adr;
	adr.SetFromString(LUA->CheckString(2), LUA->GetBool(3));

	if (!adr.IsValid())
	{
		LUA->PushNumber(-1);
		return 1;
	}

	CBaseServer* pServer = (CBaseServer*)Util::server;
	LUA->PushNumber(func_NET_SendPacket(NULL, pServer->m_Socket, adr, msg->GetData(), msg->GetNumBytesWritten(), NULL, false));
	return 1;
}

static Symbols::NET_CreateNetChannel func_NET_CreateNetChannel;
LUA_FUNCTION_STATIC(gameserver_CreateNetChannel)
{
	netadr_t adr;
	adr.SetFromString(LUA->CheckString(1), LUA->GetBool(2));

	if (!adr.IsValid())
	{
		Push_CNetChan(LUA, NULL);
		return 1;
	}

	ILuaNetMessageHandler* pHandler = new ILuaNetMessageHandler(LUA);

	CBaseServer* pServer = (CBaseServer*)Util::server;
	CNetChan* pNetChannel = (CNetChan*)func_NET_CreateNetChannel(pServer->m_Socket, &adr, adr.ToString(), (INetChannelHandler*)pHandler, true, 1);
	pNetChannel->RegisterMessage(pHandler->luaNetChanMessage);
	pHandler->chan = pNetChannel;

	Push_CNetChan(LUA, pNetChannel);
	return 1;
}

static Symbols::NET_RemoveNetChannel func_NET_RemoveNetChannel;
LUA_FUNCTION_STATIC(gameserver_RemoveNetChannel)
{
	CNetChan* pNetChannel = Get_CNetChan(LUA, 1, true);

	ILuaNetMessageHandler* pHandler = (ILuaNetMessageHandler*)pNetChannel->m_MessageHandler;
	func_NET_RemoveNetChannel(pNetChannel, true);
	LUA->SetUserType(1, NULL);

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
			Push_CNetChan(LUA, handler->chan);
			Util::RawSetI(LUA, -2, ++idx);
		}

	return 1;
}

extern CGlobalVars* gpGlobals;
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
		Util::AddFunc(pLua, CNetChan_IsValid, "IsValid");
		Util::AddFunc(pLua, CNetChan_GetTable, "GetTable");
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
		Util::AddFunc(pLua, CNetChan_Transmit, "Transmit");
		Util::AddFunc(pLua, CNetChan_ProcessStream, "ProcessStream");
		Util::AddFunc(pLua, CNetChan_SetMaxBufferSize, "SetMaxBufferSize");
		Util::AddFunc(pLua, CNetChan_GetMaxRoutablePayloadSize, "GetMaxRoutablePayloadSize");
		Util::AddFunc(pLua, CNetChan_SendMessage, "SendMessage");

		// Callbacks
		Util::AddFunc(pLua, CNetChan_SetMessageCallback, "SetMessageCallback");
		Util::AddFunc(pLua, CNetChan_GetMessageCallback, "GetMessageCallback");
		Util::AddFunc(pLua, CNetChan_SetConnectionStartCallback, "SetConnectionStartCallback");
		Util::AddFunc(pLua, CNetChan_GetConnectionStartCallback, "GetConnectionStartCallback");
		Util::AddFunc(pLua, CNetChan_SetConnectionClosingCallback, "SetConnectionClosingCallback");
		Util::AddFunc(pLua, CNetChan_GetConnectionClosingCallback, "GetConnectionClosingCallback");
		Util::AddFunc(pLua, CNetChan_SetConnectionCrashedCallback, "SetConnectionCrashedCallback");
		Util::AddFunc(pLua, CNetChan_GetConnectionCrashedCallback, "GetConnectionCrashedCallback");
	pLua->Pop(1);

	Util::StartTable(pLua);
		Util::AddFunc(pLua, gameserver_GetNumClients, "GetNumClients");
		Util::AddFunc(pLua, gameserver_GetNumProxies, "GetNumProxies");
		Util::AddFunc(pLua, gameserver_GetNumFakeClients, "GetNumFakeClients");
		Util::AddFunc(pLua, gameserver_GetMaxClients, "GetMaxClients");
		Util::AddFunc(pLua, gameserver_GetUDPPort, "GetUDPPort");
		Util::AddFunc(pLua, gameserver_GetClient, "GetClient");
		Util::AddFunc(pLua, gameserver_GetClientCount, "GetClientCount");
		Util::AddFunc(pLua, gameserver_GetAll, "GetAll");
		Util::AddFunc(pLua, gameserver_GetTime, "GetTime");
		Util::AddFunc(pLua, gameserver_GetTick, "GetTick");
		Util::AddFunc(pLua, gameserver_GetTickInterval, "GetTickInterval");
		Util::AddFunc(pLua, gameserver_GetName, "GetName");
		Util::AddFunc(pLua, gameserver_GetMapName, "GetMapName");
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
		Util::AddFunc(pLua, gameserver_CalculateCPUUsage, "CalculateCPUUsage");
		Util::AddFunc(pLua, gameserver_ApproximateProcessMemoryUsage, "ApproximateProcessMemoryUsage");
		Util::AddFunc(pLua, gameserver_SendConnectionlessPacket, "SendConnectionlessPacket");

		Util::AddFunc(pLua, gameserver_CreateNetChannel, "CreateNetChannel");
		Util::AddFunc(pLua, gameserver_RemoveNetChannel, "RemoveNetChannel");
		Util::AddFunc(pLua, gameserver_GetCreatedNetChannels, "GetCreatedNetChannels");
	Util::FinishTable(pLua, "gameserver");
}

void CGameServerModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "gameserver");

	DeleteAll_CBaseClient(pLua);
	DeleteAll_CNetChan(pLua);
}

static Detouring::Hook detour_CServerGameClients_GetPlayerLimit;
static void hook_CServerGameClients_GetPlayerLimit(void* funkyClass, int& minPlayers, int& maxPlayers, int& defaultMaxPlayers)
{
	minPlayers = 1;
	maxPlayers = 255; // Allows one to go up to 255 slots.
	defaultMaxPlayers = 255;
}

static Detouring::Hook detour_CBaseServer_ProcessConnectionlessPacket;
static bool hook_CBaseServer_ProcessConnectionlessPacket(void* server, netpacket_s* packet)
{
	if (!gameserver_connectionlesspackethook.GetBool())
	{
		return detour_CBaseServer_ProcessConnectionlessPacket.GetTrampoline<Symbols::CBaseServer_ProcessConnectionlessPacket>()(server, packet);
	}

	int originalPos = packet->message.GetNumBitsRead();
	if (Lua::PushHook("HolyLib:ProcessConnectionlessPacket"))
	{
		LuaUserData* pLuaData = Push_bf_read(g_Lua, &packet->message);
		g_Lua->Push(-3);
		g_Lua->Push(-3);
		g_Lua->Remove(-5);
		g_Lua->Remove(-4);
		g_Lua->Push(-3);

		bool bHandled = false;
		g_Lua->PushString(packet->from.ToString());
		if (g_Lua->CallFunctionProtected(3, 1, true))
		{
			bHandled = g_Lua->GetBool(-1);
			g_Lua->Pop(1);
		}

		if (pLuaData)
		{
			delete pLuaData;
		}
		g_Lua->SetUserType(-1, NULL);
		g_Lua->Pop(1);

		if (bHandled)
		{
			return true;
		}
	}

	packet->message.Seek(originalPos);
	return detour_CBaseServer_ProcessConnectionlessPacket.GetTrampoline<Symbols::CBaseServer_ProcessConnectionlessPacket>()(server, packet);
}

/*
 * ToDo: Ask Rubat if were allowed to modify SVC_ServerInfo
 *       I think it "could" be considered breaking gmod server operator rules.
 *       "Do not fake server information. This mostly means player count, but other data also applies."
 * 
 * Update: Rubat said it's fine.
 */
// static MD5Value_t worldmapMD5;
static Detouring::Hook detour_CBaseServer_FillServerInfo;
static void hook_CBaseServer_FillServerInfo(void* srv, SVC_ServerInfo& info)
{
	detour_CBaseServer_FillServerInfo.GetTrampoline<Symbols::CBaseServer_FillServerInfo>()(srv, info);

	// Fixes a crash("UpdatePlayerName with bogus slot 129") when joining a server which has more than 128 slots / is over MAX_PLAYERS
	if ( info.m_nMaxClients > 128 )
		info.m_nMaxClients = 128;

	if ( info.m_nMaxClients <= 1 )
	{
		// Fixes clients denying the serverinfo on singleplayer games.
		info.m_nMaxClients = 2;

		// singleplayer games don't create a MD5, so we have to do it ourself.
		// V_memcpy( info.m_nMapMD5.bits, worldmapMD5.bits, MD5_DIGEST_LENGTH );
	}
}

static Detouring::Hook detour_CBaseClient_SetSignonState;
static bool hook_CBaseClient_SetSignonState(CBaseClient* cl, int state, int spawncount)
{
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

	return detour_CBaseClient_SetSignonState.GetTrampoline<Symbols::CBaseClient_SetSignonState>()(cl, state, spawncount);
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
static bool hook_CBaseClient_ShouldSendMessages(CBaseClient* cl)
{
	if ( !cl->IsConnected() )
		return false;

	// if the reliable message overflowed, drop the client
	if ( cl->m_NetChannel && cl->m_NetChannel->IsOverflowed() )
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
		}

		if (bKick)
		{
			cl->m_NetChannel->Reset();
			cl->Disconnect( "%s overflowed reliable buffer", cl->m_Name);
			return false;
		}
	}

	if ( cl->m_NetChannel )
		net_time = cl->m_NetChannel->GetTime(); // Required as we use net_time below.

	// check, if it's time to send the next packet
	bool bSendMessage = cl->m_fNextMessageTime <= net_time;
	if ( !bSendMessage && !cl->IsActive() )
	{
		// if we are in signon modem instantly reply if
		// we got a answer and have reliable data waiting
		if ( cl->m_bReceivedPacket && cl->m_NetChannel && cl->m_NetChannel->HasPendingReliableData() )
		{
			bSendMessage = true;
		}
	}

	if ( bSendMessage && cl->m_NetChannel && !cl->m_NetChannel->CanPacket() )
	{
		// we would like to send a message, but bandwidth isn't available yet
		// tell netchannel that we are choking a packet
		cl->m_NetChannel->SetChoked();	
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
	for (i=0 ; i< srv->m_Clients.Count() ; i++ )
	{
		IClient	*cl = srv->m_Clients[ i ];
		
		if ( cl->IsFakeClient() || !cl->IsConnected() )
			continue;

		INetChannel *netchan = cl->GetNetChannel();

		if ( !netchan )
			continue;

		if ( netchan->IsTimedOut() )
			cl->Disconnect( CLIENTNAME_TIMED_OUT, cl->GetClientName() );
	}
#endif

	for (i=0 ; i< srv->m_Clients.Count() ; i++ )
	{
		IClient	*cl = srv->m_Clients[ i ];
		
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
			}

			cl->Disconnect( "Client %d overflowed reliable channel.", i );
		}
	}
}

class CExtentedNetMessage : public CNetMessage
{
public:
	INetMessageHandler *m_pMessageHandler;
};

/*
 * Moving a entire CGameClient into another CGameClient to hopefully not make the engine too angry.
 * This is required to preserve the logic of m_nEntityIndex = m_nClientSlot + 1
 * We don't copy everything, like the baseline and such.
 * 
 * Current State: The Client's LocalPlayer is a NULL Entity.....
 */
static void MoveCGameClientIntoCGameClient(CGameClient* origin, CGameClient* target)
{
	if (g_pGameServerModule.InDebug())
		Msg(PROJECT_NAME ": Reassigned client to from slot %i to %i\n", origin->m_nClientSlot, target->m_nClientSlot);

	target->Inactivate();
	target->Clear();

	/*
	 * NOTE: This will fire the player_connect and player_connect_client gameevents.
	 * BUG: Their Name will have (1) at the beginning because of some funny engine behavior.
	 */
	target->Connect( origin->m_Name, origin->m_UserID, origin->m_NetChannel, origin->m_bFakePlayer, origin->m_clientChallenge );

	/*
	 * Basic CBaseClient::Connect setup
	 */

	//target->m_ConVars = origin->m_ConVars;
	//target->m_bInitialConVarsSet = origin->m_bInitialConVarsSet;
	//target->m_UserID = origin->m_UserID;
	//target->m_bFakePlayer = origin->m_bFakePlayer;
	//target->m_NetChannel = origin->m_NetChannel;
	//target->m_clientChallenge = origin->m_clientChallenge;
	//target->edict = Util::engineserver->PEntityOfEntIndex( target->m_nEntityIndex );
	//target->m_PackInfo.m_pClientEnt = target->edict;
	//target->m_PackInfo.m_nPVSSize = sizeof( target->m_PackInfo.m_PVS );

	target->SetName( origin->m_Name ); // Required thingy
	target->m_nSignonState = origin->m_nSignonState;

	/*
	 * Copy over other things
	 */

	//for (int i = 0; i < MAX_CUSTOM_FILES; ++i)
	//	target->m_nCustomFiles[i] = origin->m_nCustomFiles[i];

	target->m_SteamID = origin->m_SteamID;
	target->m_nFriendsID = origin->m_nFriendsID;
	target->m_nFilesDownloaded = origin->m_nFilesDownloaded;
	target->m_nSignonTick = origin->m_nSignonTick;
	target->m_nStringTableAckTick = origin->m_nStringTableAckTick;
	target->m_nDeltaTick = origin->m_nDeltaTick;
	target->m_nSendtableCRC = origin->m_nSendtableCRC;
	target->m_fNextMessageTime = origin->m_fNextMessageTime;
	target->m_fSnapshotInterval = origin->m_fSnapshotInterval;
	target->m_nForceWaitForTick = origin->m_nForceWaitForTick;
	target->m_bReportFakeClient = origin->m_bReportFakeClient;
	target->m_bReceivedPacket = origin->m_bReceivedPacket;
	target->m_bFullyAuthenticated = origin->m_bFullyAuthenticated;

	memcpy(target->m_FriendsName, origin->m_FriendsName, sizeof(origin->m_FriendsName));
	memcpy(target->m_GUID, origin->m_GUID, sizeof(origin->m_GUID));

	target->m_fTimeLastNameChange = origin->m_fTimeLastNameChange;
	target->m_bPlayerNameLocked = origin->m_bPlayerNameLocked;
	memcpy(target->m_szPendingNameChange, origin->m_szPendingNameChange, sizeof(origin->m_szPendingNameChange));

	/*
	 * Update CNetChan and CNetMessage's properly to not crash.
	 */

	CNetChan* chan = (CNetChan*)target->m_NetChannel;
	chan->m_MessageHandler = (INetChannelHandler*)target;

	FOR_EACH_VEC(chan->m_NetMessages, i)
	{
		CExtentedNetMessage* msg = (CExtentedNetMessage*)chan->m_NetMessages[i];
		msg->m_pMessageHandler = target;

		if (msg->GetType() == clc_CmdKeyValues)
		{
			Base_CmdKeyValues* keyVal = (Base_CmdKeyValues*)msg;
			if (keyVal->m_pKeyValues)
			{
				keyVal->m_pKeyValues = NULL; // Will leak memory but we can't safely delete it currently.
				// ToDo: Fix this small memory leak.
			}
		}
	}

	/*
	 * Nuke the origin client
	 */

	origin->m_NetChannel = NULL; // Nuke the net channel or else it might touch it.
	//origin->m_ConVars = NULL; // Same here
	origin->Inactivate();
	origin->Clear();

	if (Lua::PushHook("HolyLib:OnPlayerChangedSlot"))
	{
		g_Lua->PushNumber(origin->m_nClientSlot);
		g_Lua->PushNumber(target->m_nClientSlot);
		g_Lua->CallFunctionProtected(3, 0, true);
	}

	/*
	 * Update Client about it's playerSlot or else it will see the wrong entity as it's local player.
	 */

	SVC_ServerInfo info;
	CBaseServer* pServer = (CBaseServer*)target->GetServer();
	pServer->FillServerInfo(info);

	info.m_nPlayerSlot = target->m_nClientSlot;

	target->SendNetMsg(info, true);

	/*
	 * Reconnecting the client to let it go through the loading process again since it became unstable when we sent the ServerInfo.
	 */

	target->Reconnect();
}

#define MAX_PLAYERS 128
static int FindFreeClientSlot()
{
	int nextFreeEntity = 255;
	int count = Util::server->GetClientCount();
	if (count > MAX_PLAYERS)
		count = MAX_PLAYERS;

	for (int iClientIndex=0; iClientIndex<count; ++iClientIndex)
	{
		CBaseClient* pClient = (CBaseClient*)Util::server->GetClient(iClientIndex);

		if (pClient->m_nSignonState != SIGNONSTATE_NONE)
			continue;

		if (pClient->m_nEntityIndex < nextFreeEntity)
			nextFreeEntity = pClient->m_nEntityIndex;
	}

	return nextFreeEntity;
}

static Detouring::Hook detour_CGameClient_SpawnPlayer;
void hook_CGameClient_SpawnPlayer(CGameClient* client)
{
	if (client->m_nClientSlot < MAX_PLAYERS && !gameserver_disablespawnsafety.GetBool())
	{
		detour_CGameClient_SpawnPlayer.GetTrampoline<Symbols::CGameClient_SpawnPlayer>()(client);
		return;
	}

	int nextFreeEntity = FindFreeClientSlot();
	if (nextFreeEntity > MAX_PLAYERS)
	{
		Warning(PROJECT_NAME ": Failed to find a valid player slot to use! Stopping client spawn! (%i, %i, %i)\n", client->m_nClientSlot, client->GetUserID(), nextFreeEntity);
		return;
	}

	CGameClient* pClient = (CGameClient*)Util::GetClientByIndex(nextFreeEntity - 1);
	if (pClient->m_nSignonState != SIGNONSTATE_NONE)
	{
		// It really didn't like what we had planned.
		Warning(PROJECT_NAME ": Client collision! fk. Client will be refused to spawn! (%i - %s, %i - %s)\n", pClient->m_nClientSlot, pClient->GetClientName(), client->m_nClientSlot, client->GetClientName());
		return;
	}

	MoveCGameClientIntoCGameClient(client, pClient);
	//detour_CGameClient_SpawnPlayer.GetTrampoline<Symbols::CGameClient_SpawnPlayer>()(pClient);
}

// Called by Util from CSteam3Server::NotifyClientDisconnect
void GameServer_OnClientDisconnect(CBaseClient* pClient)
{
	if (pClient->GetServer() != Util::server)
		return;

	if (g_Lua && Lua::PushHook("HolyLib:OnClientDisconnect"))
	{
		Push_CBaseClient(g_Lua, pClient);
		g_Lua->CallFunctionProtected(2, 0, true);
	}


	Delete_CBaseClient(g_Lua, pClient);
}

inline unsigned short BufferToShortChecksum( const void *pvData, size_t nLength )
{
	CRC32_t crc = CRC32_ProcessSingleBuffer( pvData, nLength );

	unsigned short lowpart = ( crc & 0xffff );
	unsigned short highpart = ( ( crc >> 16 ) & 0xffff );

	return (unsigned short)( lowpart ^ highpart );
}

#define FLIPBIT(v,b) if (v&b) v &= ~b; else v |= b;

static Detouring::Hook detour_CNetChan_UpdateSubChannels;
void hook_CNetChan_UpdateSubChannels(CNetChan* chan)
{
	if (g_pMaxFragments <= 0)
	{
		detour_CNetChan_UpdateSubChannels.GetTrampoline<Symbols::CNetChan_UpdateSubChannels>()(chan);
		return;
	}

	// first check if there is a free subchannel
	CNetChan::subChannel_s * freeSubChan = chan->GetFreeSubChannel();

	if ( freeSubChan == NULL )
		return; //all subchannels in use right now

	int nSendMaxFragments = MIN(g_pMaxFragments, 7);

	bool bSendData = false;

	for ( int i = 0; i < MAX_STREAMS; i++ )
	{
		if ( chan->m_WaitingList[i].Count() <= 0 )
			continue;

		CNetChan::dataFragments_s *data = chan->m_WaitingList[i][0]; // get head

		if ( data->asTCP )
			continue;

		int nSentFragments = data->ackedFragments + data->pendingFragments;

		Assert( nSentFragments <= data->numFragments );

		if ( nSentFragments == data->numFragments )
			continue; // all fragments already send

		// how many fragments can we send ?

		int numFragments = MIN( nSendMaxFragments, data->numFragments - nSentFragments );

		// if we are in file background transmission mode, just send one fragment per packet
		//if ( i == FRAG_FILE_STREAM && chan->m_bFileBackgroundTranmission )
		//	numFragments = MIN( 1, numFragments );

		// copy fragment data into subchannel

		freeSubChan->startFraggment[i] = nSentFragments;
		freeSubChan->numFragments[i] = numFragments;
		
		data->pendingFragments += numFragments;

		bSendData = true;

		nSendMaxFragments -= numFragments;

		if ( nSendMaxFragments <= 0 )
			break;
	}

	if ( bSendData )
	{
		// flip channel bit 
		int bit = 1<<freeSubChan->index;

		FLIPBIT(chan->m_nOutReliableState, bit);

		freeSubChan->state = SUBCHANNEL_TOSEND;
		freeSubChan->sendSeqNr = 0;
	}
}

static ConVar* vcr_verbose;
static ConVar* net_maxroutable;
static ConVar* net_compresspackets_minsize;
static ConVar* net_showudp;
static ConVar* net_compresspackets;
static ConVar* net_maxcleartime;
static Symbols::CNetChan_CreateFragmentsFromBuffer func_CNetChan_CreateFragmentsFromBuffer;
static Symbols::CNetChan_FlowNewPacket func_CNetChan_FlowNewPacket;
static Symbols::CNetChan_FlowUpdate func_CNetChan_FlowUpdate;
static Symbols::CNetChan_SendSubChannelData func_CNetChan_SendSubChannelData;
static Detouring::Hook detour_CNetChan_SendDatagram;
int hook_CNetChan_SendDatagram(CNetChan* chan, bf_write *datagram)
{
	ALIGN4 byte		send_buf[ NET_MAX_MESSAGE ] ALIGN4_POST;

#if !defined(NO_VCR) && ARCHITECTURE_IS_X86
	if ( vcr_verbose->GetInt() && datagram && datagram->GetNumBytesWritten() > 0 )
		VCRGenericValueVerify( "datagram", datagram->GetBasePointer(), datagram->GetNumBytesWritten()-1 );
#endif
	
	// first increase out sequence number
	
	// check, if fake client, then fake send also
	if ( chan->remote_address.GetType() == NA_NULL )	
	{
		// this is a demo channel, fake sending all data
		chan->m_fClearTime = 0.0;		// no bandwidth delay
		chan->m_nChokedPackets = 0;	// Reset choke state
		chan->m_StreamReliable.Reset();		// clear current reliable buffer
		chan->m_StreamUnreliable.Reset();		// clear current unrelaible buffer
		chan->m_nOutSequenceNr++;
		return chan->m_nOutSequenceNr-1;
	}

	// process all new and pending reliable data, return true if reliable data should
	// been send with this packet

	if ( chan->m_StreamReliable.IsOverflowed() )
	{
		ConMsg ("%s:send reliable stream overflow\n", chan->remote_address.ToString());
		return 0;
	}
	else if ( chan->m_StreamReliable.GetNumBitsWritten() > 0 )
	{
		func_CNetChan_CreateFragmentsFromBuffer(chan, &chan->m_StreamReliable, FRAG_NORMAL_STREAM);
		chan->m_StreamReliable.Reset();
	}

	bf_write send( "CNetChan_TransmitBits->send", send_buf, sizeof(send_buf) );

	// Prepare the packet header
	// build packet flags
	unsigned char flags = 0;

	// start writing packet

	send.WriteLong ( chan->m_nOutSequenceNr );
	send.WriteLong ( chan->m_nInSequenceNr );

	bf_write flagsPos = send; // remember flags byte position

	send.WriteByte ( 0 ); // write correct flags value later
	//if ( chan->ShouldChecksumPackets() )
	{
		send.WriteShort( 0 );  // write correct checksum later
		Assert( !(send.GetNumBitsWritten() % 8 ) );
	}

	// Note, this only matters on the PC
	int nCheckSumStart = send.GetNumBytesWritten();

	send.WriteByte ( chan->m_nInReliableState );

	if ( chan->m_nChokedPackets > 0 )
	{
		flags |= PACKET_FLAG_CHOKED;
		send.WriteByte ( chan->m_nChokedPackets & 0xFF );	// send number of choked packets
	}

	// always append a challenge number
	flags |= PACKET_FLAG_CHALLENGE ;

	// append the challenge number itself right on the end
	send.WriteLong( chan->m_ChallengeNr );

	if ( func_CNetChan_SendSubChannelData(chan, send) )
	{
		flags |= PACKET_FLAG_RELIABLE;
	}

	// Is there room for given datagram data. the datagram data 
	// is somewhat more important than the normal unreliable data
	// this is done to allow some kind of snapshot behavior
	// weather all data in datagram is transmitted or none.
	if ( datagram )
	{
		if( datagram->GetNumBitsWritten() < send.GetNumBitsLeft() )
		{
			send.WriteBits( datagram->GetData(), datagram->GetNumBitsWritten() );
		}
		else
		{
			ConDMsg("CNetChan::SendDatagram:  data would overfow, ignoring\n");
		}
	}

	// Is there room for the unreliable payload?
	if ( chan->m_StreamUnreliable.GetNumBitsWritten() < send.GetNumBitsLeft() )
	{
		send.WriteBits(chan->m_StreamUnreliable.GetData(), chan->m_StreamUnreliable.GetNumBitsWritten() );
	}
	else
	{
		ConDMsg("CNetChan::SendDatagram:  Unreliable would overfow, ignoring\n");
	}

	chan->m_StreamUnreliable.Reset();	// clear unreliable data buffer

	// On the PC the voice data is in the main packet
	if ( !IsX360() && 
		chan->m_StreamVoice.GetNumBitsWritten() > 0 && chan->m_StreamVoice.GetNumBitsWritten() < send.GetNumBitsLeft() )
	{
		send.WriteBits(chan->m_StreamVoice.GetData(), chan->m_StreamVoice.GetNumBitsWritten() );
		chan->m_StreamVoice.Reset();
	}

	int nMinRoutablePayload = MIN_ROUTABLE_PAYLOAD;

#if defined( _DEBUG ) || defined( MIN_ROUTABLE_TESTING )
	if ( m_Socket == NS_SERVER )
	{
		nMinRoutablePayload = net_minroutable.GetInt();
	}
#endif

	// Deal with packets that are too small for some networks
	while ( send.GetNumBytesWritten() < nMinRoutablePayload )		
	{
		// Go ahead and pad some bits as long as needed
		send.WriteUBitLong( net_NOP, NETMSG_TYPE_BITS );
	}

	// Make sure we have enough bits to read a final net_NOP opcode before compressing 
	int nRemainingBits = send.GetNumBitsWritten() % 8;
	if ( nRemainingBits > 0 &&  nRemainingBits <= (8-NETMSG_TYPE_BITS) )
	{
		send.WriteUBitLong( net_NOP, NETMSG_TYPE_BITS );
	}

	// if ( IsX360() )
	{
		// Now round up to byte boundary
		nRemainingBits = send.GetNumBitsWritten() % 8;
		if ( nRemainingBits > 0 )
		{
			int nPadBits = 8 - nRemainingBits;

			flags |= ENCODE_PAD_BITS( nPadBits );
	
			// Pad with ones
			if ( nPadBits > 0 )
			{
				unsigned int unOnes = GetBitForBitnum( nPadBits ) - 1;
				send.WriteUBitLong( unOnes, nPadBits );
			}
		}
	}


	// FIXME:  This isn't actually correct since compression might make the main payload usage a bit smaller
	bool bSendVoice = IsX360() && ( chan->m_StreamVoice.GetNumBitsWritten() > 0 && chan->m_StreamVoice.GetNumBitsWritten() < send.GetNumBitsLeft() );
		
	bool bCompress = false;
	if ( net_compresspackets->GetBool() )
	{
		if ( send.GetNumBytesWritten() >= net_compresspackets_minsize->GetInt() )
		{
			bCompress = true;
		}
	}

	// write correct flags value and the checksum
	flagsPos.WriteByte( flags ); 

	// Compute checksum (must be aligned to a byte boundary!!)
	//if ( chan->ShouldChecksumPackets() ) if MultiPlayer -> always true
	{
		const void *pvData = send.GetData() + nCheckSumStart;
		Assert( !(send.GetNumBitsWritten() % 8 ) );
		int nCheckSumBytes = send.GetNumBytesWritten() - nCheckSumStart;
		unsigned short usCheckSum = BufferToShortChecksum( pvData, nCheckSumBytes );
		flagsPos.WriteUBitLong( usCheckSum, 16 );
	}

	// Send the datagram
	int	bytesSent = func_NET_SendPacket( chan, chan->m_Socket, chan->remote_address, send.GetData(), send.GetNumBytesWritten(), bSendVoice ? &chan->m_StreamVoice : 0, bCompress );

	if ( bSendVoice || !IsX360() )
	{
		chan->m_StreamVoice.Reset();
	}

	if ( net_showudp->GetInt() && net_showudp->GetInt() != 2 )
	{
		int mask = 63;
		char comp[ 64 ] = { 0 };
		if ( net_compresspackets->GetBool() && 
			bytesSent && 
			( bytesSent < send.GetNumBytesWritten() ) )
		{
			Q_snprintf( comp, sizeof( comp ), " compression=%5u [%5.2f %%]", bytesSent, 100.0f * float( bytesSent ) / float( send.GetNumBytesWritten() ) );
		}
	
		ConMsg ("UDP -> %12.12s: sz=%5i seq=%5i ack=%5i rel=%1i ch=%1i tm=%f rt=%f%s\n"
			, chan->GetName()
			, send.GetNumBytesWritten()
			, ( chan->m_nOutSequenceNr ) & mask
			, chan->m_nInSequenceNr & mask
			, (flags & PACKET_FLAG_RELIABLE) ? 1 : 0
			, flags & PACKET_FLAG_CHALLENGE ? 1 : 0
			, (float)net_time
			, (float)Plat_FloatTime()
			, comp );
	}

	// update stats

	int nTotalSize = bytesSent + UDP_HEADER_SIZE;

	func_CNetChan_FlowNewPacket(chan, FLOW_OUTGOING, chan->m_nOutSequenceNr, chan->m_nInSequenceNr, chan->m_nChokedPackets, 0, nTotalSize);

	func_CNetChan_FlowUpdate(chan, FLOW_OUTGOING, nTotalSize);
	
	if ( chan->m_fClearTime < net_time )
	{
		chan->m_fClearTime = net_time;
	}

	// calculate net_time when channel will be ready for next packet (throttling)
	// TODO:  This doesn't exactly match size sent when packet is a "split" packet (actual bytes sent is higher, etc.)
	/*double fAddTime = (float)nTotalSize / (float)chan->m_Rate;

	chan->m_fClearTime += fAddTime;

	if ( net_maxcleartime->GetFloat() > 0.0f )
	{
		double m_flLatestClearTime = net_time + net_maxcleartime->GetFloat();
		if ( chan->m_fClearTime > m_flLatestClearTime )
		{
			chan->m_fClearTime = m_flLatestClearTime;
		}
	}*/

	if ( net_maxcleartime->GetFloat() > 0.0f )
	{
		double m_flLatestClearTime = net_time + net_maxcleartime->GetFloat();
		if ( chan->m_fClearTime > m_flLatestClearTime )
		{
			chan->m_fClearTime = m_flLatestClearTime;
		}
	}
	
	chan->m_nChokedPackets = 0;
	chan->m_nOutSequenceNr++;

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

				Assert(m_WaitingList[j].Count() > 0);

				CNetChan::dataFragments_t * data = chan->m_WaitingList[j][0];

				// tell waiting list, that we received the acknowledge
				data->ackedFragments += subchan->numFragments[j]; 
				data->pendingFragments -= subchan->numFragments[j];
			}

			subchan->Free(); // mark subchannel as free again
		}
	}

	return chan->m_nOutSequenceNr-1; // return send seq nr
}

static ConVar* net_showfragments;
static ConVar* net_maxpacketdrop;
static ConVar* net_showdrop;
static Symbols::CNetChan_CheckWaitingList func_CNetChan_CheckWaitingList;
static Detouring::Hook detour_CNetChan_ProcessPacketHeader;
int hook_CNetChan_ProcessPacketHeader(CNetChan* chan, netpacket_t* packet)
{
	// get sequence numbers		
	int sequence	= packet->message.ReadLong();
	int sequence_ack= packet->message.ReadLong();
	int flags		= packet->message.ReadByte();

	//if ( ShouldChecksumPackets() )
	{
		unsigned short usCheckSum = (unsigned short)packet->message.ReadUBitLong( 16 );

		// Checksum applies to rest of packet
		Assert( !( packet->message.GetNumBitsRead() % 8 ) );
		int nOffset = packet->message.GetNumBitsRead() >> 3;
		int nCheckSumBytes = packet->message.TotalBytesAvailable() - nOffset;
	
		const void *pvData = packet->message.GetBasePointer() + nOffset;
		unsigned short usDataCheckSum = BufferToShortChecksum( pvData, nCheckSumBytes );
	
		if ( usDataCheckSum != usCheckSum )
		{
			ConMsg ("%s:corrupted packet %i at %i\n"
				, chan->remote_address.ToString ()
				, sequence
				, chan->m_nInSequenceNr);
			return -1;
		}
	}

	int relState	= packet->message.ReadByte();	// reliable state of 8 subchannels
	int nChoked		= 0;	// read later if choked flag is set
	int i,j;

	if ( flags & PACKET_FLAG_CHOKED )
		nChoked = packet->message.ReadByte(); 

	if ( flags & PACKET_FLAG_CHALLENGE )
	{
		unsigned int nChallenge = packet->message.ReadLong();
		if ( nChallenge != chan->m_ChallengeNr )
			return -1;
		// challenge was good, latch we saw a good one
		chan->m_bStreamContainsChallenge = true;
	}
	else if ( chan->m_bStreamContainsChallenge )
		return -1; // what, no challenge in this packet but we got them before?

	// discard stale or duplicated packets
	if (sequence <= chan->m_nInSequenceNr )
	{
		if ( net_showdrop->GetInt() )
		{
			if ( sequence == chan->m_nInSequenceNr )
			{
				ConMsg ("%s:duplicate packet %i at %i\n"
					, chan->remote_address.ToString ()
					, sequence
					, chan->m_nInSequenceNr);
			}
			else
			{
				ConMsg ("%s:out of order packet %i at %i\n"
					, chan->remote_address.ToString ()
					, sequence
					, chan->m_nInSequenceNr);
			}
		}
		
		return -1;
	}

//
// dropped packets don't keep the message from being used
//
	chan->m_PacketDrop = sequence - (chan->m_nInSequenceNr + nChoked + 1);

	if ( chan->m_PacketDrop > 0 )
	{
		if ( net_showdrop->GetInt() )
		{
			ConMsg ("%s:Dropped %i packets at %i\n"
			,chan->remote_address.ToString(), chan->m_PacketDrop, sequence );
		}
	}

	if ( net_maxpacketdrop->GetInt() > 0 && chan->m_PacketDrop > net_maxpacketdrop->GetInt() )
	{
		if ( net_showdrop->GetInt() )
		{
			ConMsg ("%s:Too many dropped packets (%i) at %i\n"
				,chan->remote_address.ToString(), chan->m_PacketDrop, sequence );
		}
		return -1;
	}

	for ( i = 0; i<MAX_SUBCHANNELS; i++ )
	{
		int bitmask = (1<<i);

		// data of channel i has been acknowledged
		CNetChan::subChannel_s * subchan = &chan->m_SubChannels[i];

		Assert( subchan->index == i);

		if ( (chan->m_nOutReliableState & bitmask) == (relState & bitmask) || i >= MAX_SUBCHANNELS )
		{
			if ( subchan->state == SUBCHANNEL_DIRTY )
			{
				// subchannel was marked dirty during changelevel, waiting list is already cleared
				subchan->Free();
			}
			else if ( subchan->sendSeqNr > sequence_ack )
			{
				// Tell this to someone that would actually care >:(
				DevMsg ("%s:reliable state invalid (%i).\n"	,chan->remote_address.ToString(), i );
				// Assert( 0 );
				return -1;
			}
			else if ( subchan->state == SUBCHANNEL_WAITING )
			{
				for ( j=0; j<MAX_STREAMS; j++ )
				{
					if ( subchan->numFragments[j] == 0 )
						continue;

					Assert( m_WaitingList[j].Count() > 0 );
					
					CNetChan::dataFragments_t* data = chan->m_WaitingList[j][0];

					// tell waiting list, that we received the acknowledge
					data->ackedFragments += subchan->numFragments[j]; 
					data->pendingFragments -= subchan->numFragments[j];
				}

				subchan->Free(); // mark subchannel as free again
			}
		}
		else // subchannel doesn't match
		{
			if ( subchan->sendSeqNr <= sequence_ack )
			{
				Assert( subchan->state != SUBCHANNEL_FREE );

				if ( subchan->state == SUBCHANNEL_WAITING )
				{
					if ( net_showfragments->GetBool() )
					{	
						ConMsg("Resending subchan %i: start %i, num %i\n", subchan->index, subchan->startFraggment[0], subchan->numFragments[0] );
					}

					subchan->state = SUBCHANNEL_TOSEND; // schedule for resend
				}
				else if ( subchan->state == SUBCHANNEL_DIRTY )
				{
					// remote host lost dirty channel data, flip bit back
					int bit = 1<<subchan->index; // flip bit back since data was send yet
			
					FLIPBIT(chan->m_nOutReliableState, bit);

					subchan->Free(); 
				}
			}
		}
	}

	chan->m_nInSequenceNr = sequence;
	chan->m_nOutSequenceNrAck = sequence_ack;
#if ARCHITECTURE_IS_X86
	ETWReadPacket( packet->from.ToString(), packet->wiresize, chan->m_nInSequenceNr, chan->m_nOutSequenceNr );
#endif

	// Update waiting list status
	
	for(i=0; i<MAX_STREAMS;i++)
		func_CNetChan_CheckWaitingList(chan, i); 

	// Update data flow stats (use wiresize (compressed))
	func_CNetChan_FlowNewPacket( chan, FLOW_INCOMING, chan->m_nInSequenceNr, chan->m_nOutSequenceNrAck, nChoked, chan->m_PacketDrop, packet->wiresize + UDP_HEADER_SIZE );

	return flags;
}

static Detouring::Hook detour_Filter_SendBan;
void hook_Filter_SendBan(const netadr_t& adr)
{
	if (sv_filter_nobanresponse.GetBool())
		return;

	detour_Filter_SendBan.GetTrampoline<Symbols::Filter_SendBan>()(adr);
}

void CGameServerModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::FactoryLoader engine_loader("engine");
	Detour::Create(
		&detour_CBaseServer_FillServerInfo, "CBaseServer::FillServerInfo",
		engine_loader.GetModule(), Symbols::CBaseServer_FillServerInfoSym,
		(void*)hook_CBaseServer_FillServerInfo, m_pID
	);

	Detour::Create(
		&detour_CBaseClient_SetSignonState, "CBaseClient::SetSignonState",
		engine_loader.GetModule(), Symbols::CBaseClient_SetSignonStateSym,
		(void*)hook_CBaseClient_SetSignonState, m_pID
	);

	Detour::Create(
		&detour_CBaseClient_ShouldSendMessages, "CBaseClient::ShouldSendMessages",
		engine_loader.GetModule(), Symbols::CBaseClient_ShouldSendMessagesSym,
		(void*)hook_CBaseClient_ShouldSendMessages, m_pID
	);

	Detour::Create(
		&detour_CBaseServer_CheckTimeouts, "CBaseServer::CheckTimeouts",
		engine_loader.GetModule(), Symbols::CBaseServer_CheckTimeoutsSym,
		(void*)hook_CBaseServer_CheckTimeouts, m_pID
	);

	Detour::Create(
		&detour_CGameClient_SpawnPlayer, "CGameClient::SpawnPlayer",
		engine_loader.GetModule(), Symbols::CGameClient_SpawnPlayerSym,
		(void*)hook_CGameClient_SpawnPlayer, m_pID
	);

	SourceSDK::FactoryLoader server_loader("server");
	if (!g_pModuleManager.IsMarkedAsBinaryModule()) // Loaded by require? Then we skip this.
	{
		Detour::Create(
			&detour_CBaseServer_IsMultiplayer, "CBaseServer::IsMultiplayer",
			engine_loader.GetModule(), Symbols::CBaseServer_IsMultiplayerSym,
			(void*)hook_CBaseServer_IsMultiplayer, m_pID
		);

		Detour::Create(
			&detour_GModDataPack_IsSingleplayer, "GModDataPack::IsSingleplayer",
			server_loader.GetModule(), Symbols::GModDataPack_IsSingleplayerSym,
			(void*)hook_GModDataPack_IsSingleplayer, m_pID
		);
	}

	Detour::Create(
		&detour_CServerGameClients_GetPlayerLimit, "CServerGameClients::GetPlayerLimit",
		server_loader.GetModule(), Symbols::CServerGameClients_GetPlayerLimitSym,
		(void*)hook_CServerGameClients_GetPlayerLimit, m_pID
	);

	Detour::Create(
		&detour_CBaseServer_ProcessConnectionlessPacket, "CBaseServer::ProcessConnectionlessPacket",
		engine_loader.GetModule(), Symbols::CBaseServer_ProcessConnectionlessPacketSym,
		(void*)hook_CBaseServer_ProcessConnectionlessPacket, m_pID
	);

	func_CBaseClient_OnRequestFullUpdate = (Symbols::CBaseClient_OnRequestFullUpdate)Detour::GetFunction(engine_loader.GetModule(), Symbols::CBaseClient_OnRequestFullUpdateSym);
	Detour::CheckFunction((void*)func_CBaseClient_OnRequestFullUpdate, "CBaseClient::OnRequestFullUpdate");

	/*
	 * CNetChan related stuff
	 */

	Detour::Create(
		&detour_CNetChan_D2, "CNetChan::~CNetChan",
		engine_loader.GetModule(), Symbols::CNetChan_D2Sym,
		(void*)hook_CNetChan_D2, m_pID
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
		(void*)hook_CNetChan_SendDatagram, m_pID
	);

	Detour::Create(
		&detour_CNetChan_ProcessPacketHeader, "CNetChan::ProcessPacketHeader",
		engine_loader.GetModule(), Symbols::CNetChan_ProcessPacketHeaderSym,
		(void*)hook_CNetChan_ProcessPacketHeader, m_pID
	);

	Detour::Create(
		&detour_Filter_SendBan, "Filter_SendBan",
		engine_loader.GetModule(), Symbols::Filter_SendBanSym,
		(void*)hook_Filter_SendBan, m_pID
	);

	func_NET_SendPacket = (Symbols::NET_SendPacket)Detour::GetFunction(engine_loader.GetModule(), Symbols::NET_SendPacketSym);
	Detour::CheckFunction((void*)func_NET_SendPacket, "NET_SendPacket");

	func_CNetChan_CreateFragmentsFromBuffer = (Symbols::CNetChan_CreateFragmentsFromBuffer)Detour::GetFunction(engine_loader.GetModule(), Symbols::CNetChan_CreateFragmentsFromBufferSym);
	Detour::CheckFunction((void*)func_CNetChan_CreateFragmentsFromBuffer, "CNetChan::CreateFragmentsFromBuffer");

	func_CNetChan_FlowNewPacket = (Symbols::CNetChan_FlowNewPacket)Detour::GetFunction(engine_loader.GetModule(), Symbols::CNetChan_FlowNewPacketSym);
	Detour::CheckFunction((void*)func_CNetChan_FlowNewPacket, "CNetChan::FlowNewPacket");

	func_CNetChan_FlowUpdate = (Symbols::CNetChan_FlowUpdate)Detour::GetFunction(engine_loader.GetModule(), Symbols::CNetChan_FlowUpdateSym);
	Detour::CheckFunction((void*)func_CNetChan_FlowUpdate, "CNetChan::FlowUpdate");

	func_CNetChan_SendSubChannelData = (Symbols::CNetChan_SendSubChannelData)Detour::GetFunction(engine_loader.GetModule(), Symbols::CNetChan_SendSubChannelDataSym);
	Detour::CheckFunction((void*)func_CNetChan_SendSubChannelData, "CNetChan::SendSubChannelData");

	func_CNetChan_CheckWaitingList = (Symbols::CNetChan_CheckWaitingList)Detour::GetFunction(engine_loader.GetModule(), Symbols::CNetChan_CheckWaitingListSym);
	Detour::CheckFunction((void*)func_CNetChan_CheckWaitingList, "CNetChan::CheckWaitingList");

	vcr_verbose = g_pCVar->FindVar("vcr_verbose");
	net_maxroutable = g_pCVar->FindVar("net_maxroutable");
	net_compresspackets_minsize = g_pCVar->FindVar("net_compresspackets_minsize");
	net_showudp = g_pCVar->FindVar("net_showudp");
	net_compresspackets = g_pCVar->FindVar("net_compresspackets");
	net_maxcleartime = g_pCVar->FindVar("net_maxcleartime");
	net_showfragments = g_pCVar->FindVar("net_showfragments");
	net_maxpacketdrop = g_pCVar->FindVar("net_maxpacketdrop");
	net_showdrop = g_pCVar->FindVar("net_showdrop");
}