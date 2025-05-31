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

class CGameServerModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "gameserver"; };
	virtual int Compatibility() { return LINUX32; };
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

int CBaseClient_TypeID = -1;
extern int CHLTVClient_TypeID;
PushReferenced_LuaClass(CBaseClient, CBaseClient_TypeID)
SpecialGet_LuaClass(CBaseClient, CBaseClient_TypeID, CHLTVClient_TypeID, "CBaseClient")

Default__index(CBaseClient);
Default__newindex(CBaseClient);

LUA_FUNCTION_STATIC(CBaseClient_GetTable)
{
	LuaUserData* data = Get_CBaseClient_Data(1, true);
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
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushNumber(pClient->GetPlayerSlot());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetUserID)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushNumber(pClient->GetUserID());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetName)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushString(pClient->GetClientName());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetSteamID)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushString(pClient->GetNetworkIDString());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_Reconnect)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	pClient->Reconnect();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_ClientPrint)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	pClient->ClientPrintf(LUA->CheckString(2));
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_IsValid)
{
	CBaseClient* pClient = Get_CBaseClient(1, false);
	
	LUA->PushBool(pClient != NULL && pClient->IsConnected());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_SendLua)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
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
	CBaseClient* pClient = Get_CBaseClient(1, true);
	IGameEvent* pEvent = Get_IGameEvent(2, true);

	pClient->FireGameEvent(pEvent);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_GetFriendsID)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushNumber(pClient->GetFriendsID());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetFriendsName)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushString(pClient->GetFriendsName());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetClientChallenge)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushNumber(pClient->GetClientChallenge());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_SetReportThisFakeClient)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	bool bReport = LUA->GetBool(2);

	pClient->SetReportThisFakeClient(bReport);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_ShouldReportThisFakeClient)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushNumber(pClient->ShouldReportThisFakeClient());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_Inactivate)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	pClient->Inactivate();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_Disconnect)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
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
	CBaseClient* pClient = Get_CBaseClient(1, true);
	int nRate = LUA->CheckNumber(2);
	bool bForce = LUA->GetBool(3);

	pClient->SetRate(nRate, bForce);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_GetRate)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushNumber(pClient->GetRate());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_SetUpdateRate)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	int nUpdateRate = LUA->CheckNumber(2);
	bool bForce = LUA->GetBool(3);

	pClient->SetUpdateRate(nUpdateRate, bForce);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_GetUpdateRate)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushNumber(pClient->GetUpdateRate());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_Clear)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	pClient->Clear();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_DemoRestart)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	pClient->DemoRestart();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_GetMaxAckTickCount)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushNumber(pClient->GetMaxAckTickCount());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_ExecuteStringCommand)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	const char* strCommand = LUA->CheckString(2);

	LUA->PushBool(pClient->ExecuteStringCommand(strCommand));
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_SendNetMsg)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	int iType = LUA->CheckNumber(2);
	const char* strName = LUA->CheckString(3);
	bf_write* bf = Get_bf_write(4, true);

	if (!pClient)
		LUA->ThrowError("Failed to get IClient from player!");

	SVC_CustomMessage msg;
	msg.m_iType = iType;
	strncpy(msg.m_strName, strName, sizeof(msg.m_strName));
	msg.m_DataOut.StartWriting(bf->GetData(), 0, 0, bf->GetMaxNumBits());
	msg.m_iLength = bf->GetNumBitsWritten();

	LUA->PushBool(pClient->SendNetMsg(msg));
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_IsConnected)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushBool(pClient->IsConnected());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_IsSpawned)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushBool(pClient->IsSpawned());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_IsActive)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushBool(pClient->IsActive());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetSignonState)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushNumber(pClient->m_nSignonState);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_IsFakeClient)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushBool(pClient->IsFakeClient());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_IsHLTV)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushBool(pClient->IsHLTV());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_IsHearingClient)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	int nPlayerSlot = LUA->CheckNumber(2);

	LUA->PushBool(pClient->IsHearingClient(nPlayerSlot));
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_IsProximityHearingClient)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	int nPlayerSlot = LUA->CheckNumber(2);

	LUA->PushBool(pClient->IsProximityHearingClient(nPlayerSlot));
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_SetMaxRoutablePayloadSize)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	int nMaxRoutablePayloadSize = LUA->CheckNumber(2);

	pClient->SetMaxRoutablePayloadSize(nMaxRoutablePayloadSize);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_UpdateAcknowledgedFramecount)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	int nTick = LUA->CheckNumber(2);

	LUA->PushBool(pClient->UpdateAcknowledgedFramecount(nTick));
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_ShouldSendMessages)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	LUA->PushBool(pClient->ShouldSendMessages());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_UpdateSendState)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	pClient->UpdateSendState();
	return 0;
}

// Not doing FillUserInfo since it's useless

LUA_FUNCTION_STATIC(CBaseClient_UpdateUserSettings)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	pClient->UpdateUserSettings();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_SetSignonState) // At some point will replace HolyLib.SetSignOnState
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
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
	CBaseClient* pClient = Get_CBaseClient(1, true);
	bf_write* bf = Get_bf_write(2, true);

	pClient->WriteGameSounds(*bf);
	return 0;
}

/*LUA_FUNCTION_STATIC(CBaseClient_GetDeltaFrame)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	int nTick = LUA->CheckNumber(2);

	pClient->GetDeltaFrame(nTick);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_SendSnapshot)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	pClient->SendSnapshot(NULL);
	return 0;
}*/

LUA_FUNCTION_STATIC(CBaseClient_SendServerInfo)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	pClient->SendServerInfo();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_SendSignonData)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	pClient->SendSignonData();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_SpawnPlayer)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	pClient->SpawnPlayer();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_ActivatePlayer)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	pClient->ActivatePlayer();
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_SetName)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	const char* strName = LUA->CheckString(2);

	pClient->SetName(strName);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_SetUserCVar)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	const char* strName = LUA->CheckString(2);
	const char* strValue = LUA->CheckString(3);

	pClient->SetUserCVar(strName, strValue);
	return 0;
}

LUA_FUNCTION_STATIC(CBaseClient_FreeBaselines)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

	pClient->FreeBaselines();
	return 0;
}

static Symbols::CBaseClient_OnRequestFullUpdate func_CBaseClient_OnRequestFullUpdate;
LUA_FUNCTION_STATIC(CBaseClient_OnRequestFullUpdate)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);

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
	CBaseClient* pClient = Get_CBaseClient(1, true);
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
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushBool(pNetChannel->m_bProcessingMessages);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetClearedDuringProcessing)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushBool(pNetChannel->m_bClearedDuringProcessing);
	return 1;
}

// If anyone sees a point in having this function, open a issue and ask for it to be added.
/*LUA_FUNCTION_STATIC(CBaseClient_GetShouldDelete)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushBool(pNetChannel->m_bShouldDelete);
	return 1;
}*/

LUA_FUNCTION_STATIC(CBaseClient_GetOutSequenceNr)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_nOutSequenceNr);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetInSequenceNr)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_nInSequenceNr);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetOutSequenceNrAck)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_nOutSequenceNrAck);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetOutReliableState)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_nOutReliableState);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetInReliableState)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_nInReliableState);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetChokedPackets)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_nChokedPackets);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetStreamReliable)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	Push_bf_write(&pNetChannel->m_StreamReliable);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetStreamUnreliable)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	Push_bf_write(&pNetChannel->m_StreamUnreliable);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetStreamVoice)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	Push_bf_write(&pNetChannel->m_StreamVoice);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetStreamSocket)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_StreamSocket);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetMaxReliablePayloadSize)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_MaxReliablePayloadSize);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetLastReceived)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->last_received);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetConnectTime)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->connect_time);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetClearTime)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_fClearTime);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_GetTimeout)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->m_Timeout);
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_SetTimeout)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	int seconds = LUA->CheckNumber(2);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	pNetChannel->SetTimeout(seconds);
	return 0;
}

static bool g_bFreeSubChannels = false;
LUA_FUNCTION_STATIC(CBaseClient_Transmit)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	bool bOnlyReliable = LUA->GetBool(2);
	bool bFreeSubChannels = LUA->GetBool(3);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	g_bFreeSubChannels = bFreeSubChannels;
	LUA->PushBool(pNetChannel->Transmit(bOnlyReliable));
	g_bFreeSubChannels = false;

	return 1;
}

/*LUA_FUNCTION_STATIC(CBaseClient_HasQueuedPackets)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushBool(pNetChannel->HasQueuedPackets());
	return 1;
}*/

LUA_FUNCTION_STATIC(CBaseClient_ProcessStream)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushBool(pNetChannel->ProcessStream());
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_SetMaxBufferSize)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
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
	CBaseClient* pClient = Get_CBaseClient(1, true);
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
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushNumber(pNetChannel->GetMaxRoutablePayloadSize());
	return 1;
}

// Added for CHLTVClient to inherit functions.
void Push_CBaseClientMeta()
{
	Util::AddFunc(CBaseClient__newindex, "__newindex");
	Util::AddFunc(CBaseClient__index, "__index");
	Util::AddFunc(CBaseClient_GetTable, "GetTable");

	Util::AddFunc(CBaseClient_GetPlayerSlot, "GetPlayerSlot");
	Util::AddFunc(CBaseClient_GetUserID, "GetUserID");
	Util::AddFunc(CBaseClient_GetName, "GetName");
	Util::AddFunc(CBaseClient_GetSteamID, "GetSteamID");
	Util::AddFunc(CBaseClient_Reconnect, "Reconnect");
	Util::AddFunc(CBaseClient_ClientPrint, "ClientPrint");
	Util::AddFunc(CBaseClient_IsValid, "IsValid");
	Util::AddFunc(CBaseClient_SendLua, "SendLua");
	Util::AddFunc(CBaseClient_FireGameEvent, "FireGameEvent");
	Util::AddFunc(CBaseClient_GetFriendsID, "GetFriendsID");
	Util::AddFunc(CBaseClient_GetFriendsName, "GetFriendsName");
	Util::AddFunc(CBaseClient_GetClientChallenge, "GetClientChallenge");
	Util::AddFunc(CBaseClient_SetReportThisFakeClient, "SetReportThisFakeClient");
	Util::AddFunc(CBaseClient_ShouldReportThisFakeClient, "ShouldReportThisFakeClient");
	Util::AddFunc(CBaseClient_Inactivate, "Inactivate");
	Util::AddFunc(CBaseClient_Disconnect, "Disconnect");
	Util::AddFunc(CBaseClient_SetRate, "SetRate");
	Util::AddFunc(CBaseClient_GetRate, "GetRate");
	Util::AddFunc(CBaseClient_SetUpdateRate, "SetUpdateRate");
	Util::AddFunc(CBaseClient_GetUpdateRate, "GetUpdateRate");
	Util::AddFunc(CBaseClient_Clear, "Clear");
	Util::AddFunc(CBaseClient_DemoRestart, "DemoRestart");
	Util::AddFunc(CBaseClient_GetMaxAckTickCount, "GetMaxAckTickCount");
	Util::AddFunc(CBaseClient_ExecuteStringCommand, "ExecuteStringCommand");
	Util::AddFunc(CBaseClient_SendNetMsg, "SendNetMsg");
	Util::AddFunc(CBaseClient_IsConnected, "IsConnected");
	Util::AddFunc(CBaseClient_IsSpawned, "IsSpawned");
	Util::AddFunc(CBaseClient_IsActive, "IsActive");
	Util::AddFunc(CBaseClient_GetSignonState, "GetSignonState");
	Util::AddFunc(CBaseClient_IsFakeClient, "IsFakeClient");
	Util::AddFunc(CBaseClient_IsHLTV, "IsHLTV");
	Util::AddFunc(CBaseClient_IsHearingClient, "IsHearingClient");
	Util::AddFunc(CBaseClient_IsProximityHearingClient, "IsProximityHearingClient");
	Util::AddFunc(CBaseClient_SetMaxRoutablePayloadSize, "SetMaxRoutablePayloadSize");
	Util::AddFunc(CBaseClient_UpdateAcknowledgedFramecount, "UpdateAcknowledgedFramecount");
	Util::AddFunc(CBaseClient_ShouldSendMessages, "ShouldSendMessages");
	Util::AddFunc(CBaseClient_UpdateSendState, "UpdateSendState");
	Util::AddFunc(CBaseClient_UpdateUserSettings, "UpdateUserSettings");
	Util::AddFunc(CBaseClient_SetSignonState, "SetSignonState");
	Util::AddFunc(CBaseClient_WriteGameSounds, "WriteGameSounds");
	Util::AddFunc(CBaseClient_SendServerInfo, "SendServerInfo");
	Util::AddFunc(CBaseClient_SendSignonData, "SendSignonData");
	Util::AddFunc(CBaseClient_SpawnPlayer, "SpawnPlayer");
	Util::AddFunc(CBaseClient_ActivatePlayer, "ActivatePlayer");
	Util::AddFunc(CBaseClient_SetName, "SetName");
	Util::AddFunc(CBaseClient_SetUserCVar, "SetUserCVar");
	Util::AddFunc(CBaseClient_FreeBaselines, "FreeBaselines");
	Util::AddFunc(CBaseClient_OnRequestFullUpdate, "OnRequestFullUpdate");
	Util::AddFunc(CBaseClient_SetSteamID, "SetSteamID");

	// CNetChan related functions
	Util::AddFunc(CBaseClient_GetProcessingMessages, "GetProcessingMessages");
	Util::AddFunc(CBaseClient_GetClearedDuringProcessing, "GetClearedDuringProcessing");
	//Util::AddFunc(CBaseClient_GetShouldDelete, "GetShouldDelete");
	Util::AddFunc(CBaseClient_GetOutSequenceNr, "GetOutSequenceNr");
	Util::AddFunc(CBaseClient_GetInSequenceNr, "GetInSequenceNr");
	Util::AddFunc(CBaseClient_GetOutSequenceNrAck, "GetOutSequenceNrAck");
	Util::AddFunc(CBaseClient_GetOutReliableState, "GetOutReliableState");
	Util::AddFunc(CBaseClient_GetInReliableState, "GetInReliableState");
	Util::AddFunc(CBaseClient_GetChokedPackets, "GetChokedPackets");
	Util::AddFunc(CBaseClient_GetStreamReliable, "GetStreamReliable");
	Util::AddFunc(CBaseClient_GetStreamUnreliable, "GetStreamUnreliable");
	Util::AddFunc(CBaseClient_GetStreamVoice, "GetStreamVoice");
	Util::AddFunc(CBaseClient_GetStreamSocket, "GetStreamSocket");
	Util::AddFunc(CBaseClient_GetMaxReliablePayloadSize, "GetMaxReliablePayloadSize");
	Util::AddFunc(CBaseClient_GetLastReceived, "GetLastReceived");
	Util::AddFunc(CBaseClient_GetConnectTime, "GetConnectTime");
	Util::AddFunc(CBaseClient_GetClearTime, "GetClearTime");
	Util::AddFunc(CBaseClient_GetTimeout, "GetTimeout");
	Util::AddFunc(CBaseClient_SetTimeout, "SetTimeout");
	Util::AddFunc(CBaseClient_Transmit, "Transmit");
	Util::AddFunc(CBaseClient_ProcessStream, "ProcessStream");
	//Util::AddFunc(CBaseClient_GetRegisteredMessages, "GetRegisteredMessages");
	Util::AddFunc(CBaseClient_SetMaxBufferSize, "SetMaxBufferSize");
	//Util::AddFunc(CBaseClient_HasQueuedPackets, "HasQueuedPackets");
	Util::AddFunc(CBaseClient_GetMaxRoutablePayloadSize, "GetMaxRoutablePayloadSize");
}

LUA_FUNCTION_STATIC(CGameClient__tostring)
{
	CBaseClient* pClient = Get_CBaseClient(1, false);
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

int CNetChan_TypeID = -1;
PushReferenced_LuaClass(CNetChan, CNetChan_TypeID)
Get_LuaClass(CNetChan, CNetChan_TypeID, "CNetChan")

Default__index(CNetChan);
Default__newindex(CNetChan);
Default__GetTable(CNetChan);

LUA_FUNCTION_STATIC(CNetChan__tostring)
{
	CNetChan* pNetChannel = Get_CNetChan(1, false);
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
	CNetChan* pNetChannel = Get_CNetChan(1, false);
	
	LUA->PushBool(pNetChannel != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetAvgLoss)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
	int flow = LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetAvgLoss(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetAvgChoke)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
	int flow = LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetAvgChoke(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetAvgData)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
	int flow = LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetAvgData(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetAvgLatency)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
	int flow = LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetAvgLatency(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetAvgPackets)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
	int flow = LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetAvgPackets(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetChallengeNr)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->GetChallengeNr());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetAddress)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushString(pNetChannel->GetAddress());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetDataRate)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->GetDataRate());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetDropNumber)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->GetDropNumber());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_SetChoked)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	pNetChannel->SetChoked();
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_SetFileTransmissionMode)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
	bool bBackgroundMode = LUA->GetBool(2);

	pNetChannel->SetFileTransmissionMode(bBackgroundMode);
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_SetCompressionMode)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
	bool bCompression = LUA->GetBool(2);

	pNetChannel->SetCompressionMode(bCompression);
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_SetDataRate)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
	int rate = LUA->CheckNumber(2);

	pNetChannel->SetDataRate(rate);
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_GetTime)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->GetTime());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetTimeConnected)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->GetTimeConnected());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetTimeoutSeconds)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->GetTimeoutSeconds());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetTimeSinceLastReceived)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->GetTimeSinceLastReceived());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetTotalData)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
	int flow = LUA->CheckNumber(2);

	LUA->PushNumber(pNetChannel->GetTotalData(flow));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetBufferSize)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->GetBufferSize());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetProtocolVersion)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->GetProtocolVersion());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetName)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushString(pNetChannel->GetName());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetProcessingMessages)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushBool(pNetChannel->m_bProcessingMessages);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetClearedDuringProcessing)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushBool(pNetChannel->m_bClearedDuringProcessing);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetOutSequenceNr)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->m_nOutSequenceNr);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetInSequenceNr)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->m_nInSequenceNr);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetOutSequenceNrAck)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->m_nOutSequenceNrAck);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetOutReliableState)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->m_nOutReliableState);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetInReliableState)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->m_nInReliableState);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetChokedPackets)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->m_nChokedPackets);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetStreamReliable)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	Push_bf_write(&pNetChannel->m_StreamReliable);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetStreamUnreliable)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	Push_bf_write(&pNetChannel->m_StreamUnreliable);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetStreamVoice)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	Push_bf_write(&pNetChannel->m_StreamVoice);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetStreamSocket)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->m_StreamSocket);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetMaxReliablePayloadSize)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->m_MaxReliablePayloadSize);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetLastReceived)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->last_received);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetConnectTime)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->connect_time);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetClearTime)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->m_fClearTime);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_GetTimeout)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->m_Timeout);
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_SetTimeout)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
	int seconds = LUA->CheckNumber(2);

	pNetChannel->SetTimeout(seconds);
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_Transmit)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
	bool bOnlyReliable = LUA->GetBool(2);

	LUA->PushBool(pNetChannel->Transmit(bOnlyReliable));

	return 1;
}

/*LUA_FUNCTION_STATIC(CNetChan_HasQueuedPackets)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushBool(pNetChannel->HasQueuedPackets());
	return 1;
}*/

LUA_FUNCTION_STATIC(CNetChan_ProcessStream)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushBool(pNetChannel->ProcessStream());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_SetMaxBufferSize)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
	bool bReliable = LUA->GetBool(2);
	int nBytes = LUA->CheckNumber(3);
	bool bVoice = LUA->GetBool(4);

	pNetChannel->SetMaxBufferSize(bReliable, nBytes, bVoice);
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_GetMaxRoutablePayloadSize)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

	LUA->PushNumber(pNetChannel->GetMaxRoutablePayloadSize());
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_Shutdown)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
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

	Delete_CNetChan(pNetChan);

	detour_CNetChan_D2.GetTrampoline<Symbols::CNetChan_D2>()(pNetChan);
}

class NET_LuaNetChanMessage;
class ILuaNetMessageHandler : INetChannelHandler
{
public:
	ILuaNetMessageHandler();
	virtual ~ILuaNetMessageHandler( void );

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

ILuaNetMessageHandler::ILuaNetMessageHandler()
{
	luaNetChanMessage = new NET_LuaNetChanMessage;
	luaNetChanMessage->m_pMessageHandler = this;
}

ILuaNetMessageHandler::~ILuaNetMessageHandler()
{
	if (luaNetChanMessage)
	{
		delete luaNetChanMessage;
		luaNetChanMessage = NULL;
	}

	if (!ThreadInMainThread())
	{
		Warning(PROJECT_NAME ": Tried to delete a ILuaNetMessageHandler from another thread! How could you! Now were leaking a reference...\n");
		return;
	}

	if (messageCallbackFunction != -1)
	{
		Util::ReferenceFree(messageCallbackFunction, "ILuaNetMessageHandler::~ILuaNetMessageHandler");
		messageCallbackFunction = -1;
	}

	if (connectionStartFunction != -1)
	{
		Util::ReferenceFree(connectionStartFunction, "ILuaNetMessageHandler::~ILuaNetMessageHandler");
		connectionStartFunction = -1;
	}

	if (connectionClosingFunction != -1)
	{
		Util::ReferenceFree(connectionClosingFunction, "ILuaNetMessageHandler::~ILuaNetMessageHandler");
		connectionClosingFunction = -1;
	}

	if (connectionCrashedFunction != -1)
	{
		Util::ReferenceFree(connectionCrashedFunction, "ILuaNetMessageHandler::~ILuaNetMessageHandler");
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

	g_Lua->ReferencePush(connectionStartFunction);
	Push_CNetChan((CNetChan*)chan);
	g_Lua->CallFunctionProtected(1, 0, true);
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

	g_Lua->ReferencePush(connectionClosingFunction);
	Push_CNetChan(chan);
	g_Lua->PushString(reason);
	g_Lua->CallFunctionProtected(2, 0, true);
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

	g_Lua->ReferencePush(connectionCrashedFunction);
	Push_CNetChan(chan);
	g_Lua->PushString(reason);
	g_Lua->CallFunctionProtected(2, 0, true);
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

	LuaUserData* pLuaData = Push_bf_read(&msg->m_DataIn);
	g_Lua->ReferencePush(messageCallbackFunction);

	Push_CNetChan(chan);
	g_Lua->Push(-3);
	g_Lua->PushNumber(msg->m_iLength);
	g_Lua->CallFunctionProtected(3, 0, true);
		
	if (pLuaData)
	{
		delete pLuaData;
	}
	g_Lua->SetUserType(-1, NULL);
	g_Lua->Pop(1);

	return true;
}

LUA_FUNCTION_STATIC(CNetChan_SendMessage)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
	bf_write* bf = Get_bf_write(2, true);
	bool bReliable = LUA->GetBool(3);

	NET_LuaNetChanMessage msg;
	msg.m_DataOut.StartWriting(bf->GetData(), 0, 0, bf->GetMaxNumBits());
	msg.m_iLength = bf->GetNumBitsWritten();

	LUA->PushBool(pNetChannel->SendNetMsg(msg, bReliable));
	return 1;
}

LUA_FUNCTION_STATIC(CNetChan_SetMessageCallback)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
	ILuaNetMessageHandler* pHandler = (ILuaNetMessageHandler*)pNetChannel->m_MessageHandler;
	LUA->CheckType(2, GarrysMod::Lua::Type::Function);
	
	if (!pHandler)
		return 0;

	if (pHandler->messageCallbackFunction != -1)
	{
		Util::ReferenceFree(pHandler->messageCallbackFunction, "CNetChan:SetCallback");
	}

	LUA->Push(2);
	pHandler->messageCallbackFunction = Util::ReferenceCreate("CNetChan:SetCallback");
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_GetMessageCallback)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
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
	CNetChan* pNetChannel = Get_CNetChan(1, true);
	ILuaNetMessageHandler* pHandler = (ILuaNetMessageHandler*)pNetChannel->m_MessageHandler;
	LUA->CheckType(2, GarrysMod::Lua::Type::Function);
	
	if (!pHandler)
		return 0;

	if (pHandler->connectionStartFunction != -1)
	{
		Util::ReferenceFree(pHandler->connectionStartFunction, "CNetChan:SetCallback");
	}

	LUA->Push(2);
	pHandler->connectionStartFunction = Util::ReferenceCreate("CNetChan:SetCallback");
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_GetConnectionStartCallback)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
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
	CNetChan* pNetChannel = Get_CNetChan(1, true);
	ILuaNetMessageHandler* pHandler = (ILuaNetMessageHandler*)pNetChannel->m_MessageHandler;
	LUA->CheckType(2, GarrysMod::Lua::Type::Function);
	
	if (!pHandler)
		return 0;

	if (pHandler->connectionClosingFunction != -1)
	{
		Util::ReferenceFree(pHandler->connectionClosingFunction, "CNetChan:SetCallback");
	}

	LUA->Push(2);
	pHandler->connectionClosingFunction = Util::ReferenceCreate("CNetChan:SetCallback");
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_GetConnectionClosingCallback)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
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
	CNetChan* pNetChannel = Get_CNetChan(1, true);
	ILuaNetMessageHandler* pHandler = (ILuaNetMessageHandler*)pNetChannel->m_MessageHandler;
	LUA->CheckType(2, GarrysMod::Lua::Type::Function);
	
	if (!pHandler)
		return 0;

	if (pHandler->connectionCrashedFunction != -1)
	{
		Util::ReferenceFree(pHandler->connectionCrashedFunction, "CNetChan:SetCallback");
	}

	LUA->Push(2);
	pHandler->connectionCrashedFunction = Util::ReferenceCreate("CNetChan:SetCallback");
	return 0;
}

LUA_FUNCTION_STATIC(CNetChan_GetConnectionCrashedCallback)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);
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

	Push_CBaseClient(pClient);

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

			Push_CBaseClient(pClient);
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
	bf_write* bf = Get_bf_write(3, true);

	SVC_CustomMessage msg;
	msg.m_iType = iType;
	strncpy(msg.m_strName, strName, sizeof(msg.m_strName));
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
	bf_write* msg = Get_bf_write(1, true);

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
		Push_CNetChan(NULL);
		return 1;
	}

	ILuaNetMessageHandler* pHandler = new ILuaNetMessageHandler;

	CBaseServer* pServer = (CBaseServer*)Util::server;
	CNetChan* pNetChannel = (CNetChan*)func_NET_CreateNetChannel(pServer->m_Socket, &adr, adr.ToString(), (INetChannelHandler*)pHandler, true, 1);
	pNetChannel->RegisterMessage(pHandler->luaNetChanMessage);
	pHandler->chan = pNetChannel;

	Push_CNetChan(pNetChannel);
	return 1;
}

static Symbols::NET_RemoveNetChannel func_NET_RemoveNetChannel;
LUA_FUNCTION_STATIC(gameserver_RemoveNetChannel)
{
	CNetChan* pNetChannel = Get_CNetChan(1, true);

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
	LUA->PreCreateTable(g_pPushedCNetChan.size(), 0);
		int idx = 0;
		for (auto& [chan, userdata] : g_pPushedCNetChan)
		{
			if (userdata->Push())
			{
				Util::RawSetI(LUA, -2, ++idx);
			}
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

	CBaseClient_TypeID = g_Lua->CreateMetaTable("CGameClient");
		Push_CBaseClientMeta();

		Util::AddFunc(CGameClient__tostring, "__tostring");
	g_Lua->Pop(1);

	CNetChan_TypeID = g_Lua->CreateMetaTable("CNetChan");
		Util::AddFunc(CNetChan__tostring, "__tostring");
		Util::AddFunc(CNetChan__index, "__index");
		Util::AddFunc(CNetChan__newindex, "__newindex");
		Util::AddFunc(CNetChan_IsValid, "IsValid");
		Util::AddFunc(CNetChan_GetTable, "GetTable");
		Util::AddFunc(CNetChan_GetAvgLoss, "GetAvgLoss");
		Util::AddFunc(CNetChan_GetAvgChoke, "GetAvgChoke");
		Util::AddFunc(CNetChan_GetAvgData, "GetAvgData");
		Util::AddFunc(CNetChan_GetAvgLatency, "GetAvgLatency");
		Util::AddFunc(CNetChan_GetAvgPackets, "GetAvgPackets");
		Util::AddFunc(CNetChan_GetChallengeNr, "GetChallengeNr");
		Util::AddFunc(CNetChan_GetAddress, "GetAddress");
		Util::AddFunc(CNetChan_GetDataRate, "GetDataRate");
		Util::AddFunc(CNetChan_GetDropNumber, "GetDropNumber");
		Util::AddFunc(CNetChan_SetChoked, "SetChoked");
		Util::AddFunc(CNetChan_SetFileTransmissionMode, "SetFileTransmissionMode");
		Util::AddFunc(CNetChan_SetCompressionMode, "SetCompressionMode");
		Util::AddFunc(CNetChan_SetDataRate, "SetDataRate");
		Util::AddFunc(CNetChan_SetTimeout, "SetTimeout");
		Util::AddFunc(CNetChan_GetTime, "GetTime");
		Util::AddFunc(CNetChan_GetTimeConnected, "GetTimeConnected");
		Util::AddFunc(CNetChan_GetTimeoutSeconds, "GetTimeoutSeconds");
		Util::AddFunc(CNetChan_GetTimeSinceLastReceived, "GetTimeSinceLastReceived");
		Util::AddFunc(CNetChan_GetTotalData, "GetTotalData");
		Util::AddFunc(CNetChan_GetBufferSize, "GetBufferSize");
		Util::AddFunc(CNetChan_GetProtocolVersion, "GetProtocolVersion");
		Util::AddFunc(CNetChan_GetName, "GetName");
		Util::AddFunc(CNetChan_GetProcessingMessages, "GetProcessingMessages");
		Util::AddFunc(CNetChan_GetClearedDuringProcessing, "GetClearedDuringProcessing");
		Util::AddFunc(CNetChan_GetOutSequenceNr, "GetOutSequenceNr");
		Util::AddFunc(CNetChan_GetInSequenceNr, "GetInSequenceNr");
		Util::AddFunc(CNetChan_GetOutSequenceNrAck, "GetOutSequenceNrAck");
		Util::AddFunc(CNetChan_GetOutReliableState, "GetOutReliableState");
		Util::AddFunc(CNetChan_GetInReliableState, "GetInReliableState");
		Util::AddFunc(CNetChan_GetChokedPackets, "GetChokedPackets");
		Util::AddFunc(CNetChan_GetStreamReliable, "GetStreamReliable");
		Util::AddFunc(CNetChan_GetStreamUnreliable, "GetStreamUnreliable");
		Util::AddFunc(CNetChan_GetStreamVoice, "GetStreamVoice");
		Util::AddFunc(CNetChan_GetStreamSocket, "GetStreamSocket");
		Util::AddFunc(CNetChan_GetMaxReliablePayloadSize, "GetMaxReliablePayloadSize");
		Util::AddFunc(CNetChan_GetLastReceived, "GetLastReceived");
		Util::AddFunc(CNetChan_GetConnectTime, "GetConnectTime");
		Util::AddFunc(CNetChan_GetClearTime, "GetClearTime");
		Util::AddFunc(CNetChan_GetTimeout, "GetTimeout");
		Util::AddFunc(CNetChan_SetTimeout, "SetTimeout");
		Util::AddFunc(CNetChan_Transmit, "Transmit");
		Util::AddFunc(CNetChan_ProcessStream, "ProcessStream");
		Util::AddFunc(CNetChan_SetMaxBufferSize, "SetMaxBufferSize");
		Util::AddFunc(CNetChan_GetMaxRoutablePayloadSize, "GetMaxRoutablePayloadSize");
		Util::AddFunc(CNetChan_SendMessage, "SendMessage");

		// Callbacks
		Util::AddFunc(CNetChan_SetMessageCallback, "SetMessageCallback");
		Util::AddFunc(CNetChan_GetMessageCallback, "GetMessageCallback");
		Util::AddFunc(CNetChan_SetConnectionStartCallback, "SetConnectionStartCallback");
		Util::AddFunc(CNetChan_GetConnectionStartCallback, "GetConnectionStartCallback");
		Util::AddFunc(CNetChan_SetConnectionClosingCallback, "SetConnectionClosingCallback");
		Util::AddFunc(CNetChan_GetConnectionClosingCallback, "GetConnectionClosingCallback");
		Util::AddFunc(CNetChan_SetConnectionCrashedCallback, "SetConnectionCrashedCallback");
		Util::AddFunc(CNetChan_GetConnectionCrashedCallback, "GetConnectionCrashedCallback");
	g_Lua->Pop(1);

	Util::StartTable();
		Util::AddFunc(gameserver_GetNumClients, "GetNumClients");
		Util::AddFunc(gameserver_GetNumProxies, "GetNumProxies");
		Util::AddFunc(gameserver_GetNumFakeClients, "GetNumFakeClients");
		Util::AddFunc(gameserver_GetMaxClients, "GetMaxClients");
		Util::AddFunc(gameserver_GetUDPPort, "GetUDPPort");
		Util::AddFunc(gameserver_GetClient, "GetClient");
		Util::AddFunc(gameserver_GetClientCount, "GetClientCount");
		Util::AddFunc(gameserver_GetAll, "GetAll");
		Util::AddFunc(gameserver_GetTime, "GetTime");
		Util::AddFunc(gameserver_GetTick, "GetTick");
		Util::AddFunc(gameserver_GetTickInterval, "GetTickInterval");
		Util::AddFunc(gameserver_GetName, "GetName");
		Util::AddFunc(gameserver_GetMapName, "GetMapName");
		Util::AddFunc(gameserver_GetSpawnCount, "GetSpawnCount");
		Util::AddFunc(gameserver_GetNumClasses, "GetNumClasses");
		Util::AddFunc(gameserver_GetClassBits, "GetClassBits");
		Util::AddFunc(gameserver_IsActive, "IsActive");
		Util::AddFunc(gameserver_IsLoading, "IsLoading");
		Util::AddFunc(gameserver_IsDedicated, "IsDedicated");
		Util::AddFunc(gameserver_IsPaused, "IsPaused");
		Util::AddFunc(gameserver_IsMultiplayer, "IsMultiplayer");
		Util::AddFunc(gameserver_IsPausable, "IsPausable");
		Util::AddFunc(gameserver_IsHLTV, "IsHLTV");
		Util::AddFunc(gameserver_GetPassword, "GetPassword");
		Util::AddFunc(gameserver_SetMaxClients, "SetMaxClients");
		Util::AddFunc(gameserver_SetPaused, "SetPaused");
		Util::AddFunc(gameserver_SetPassword, "SetPassword");
		Util::AddFunc(gameserver_BroadcastMessage, "BroadcastMessage");
		Util::AddFunc(gameserver_CalculateCPUUsage, "CalculateCPUUsage");
		Util::AddFunc(gameserver_ApproximateProcessMemoryUsage, "ApproximateProcessMemoryUsage");
		Util::AddFunc(gameserver_SendConnectionlessPacket, "SendConnectionlessPacket");

		Util::AddFunc(gameserver_CreateNetChannel, "CreateNetChannel");
		Util::AddFunc(gameserver_RemoveNetChannel, "RemoveNetChannel");
		Util::AddFunc(gameserver_GetCreatedNetChannels, "GetCreatedNetChannels");
	Util::FinishTable("gameserver");
}

void CGameServerModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable("gameserver");

	DeleteAll_CBaseClient();
	DeleteAll_CNetChan();
}

// Called by Util from CSteam3Server::NotifyClientDisconnect
void GameServer_OnClientDisconnect(CBaseClient* pClient)
{
	if (pClient->GetServer() != Util::server)
		return;

	if (g_Lua && Lua::PushHook("HolyLib:OnClientDisconnect"))
	{
		Push_CBaseClient(pClient);
		g_Lua->CallFunctionProtected(2, 0, true);
	}

	auto it = g_pPushedCBaseClient.find(pClient);
	if (it != g_pPushedCBaseClient.end())
	{
		Delete_CBaseClient(it->first);
	}
}

void CGameServerModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::FactoryLoader engine_loader("engine");

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

	func_NET_SendPacket = (Symbols::NET_SendPacket)Detour::GetFunction(engine_loader.GetModule(), Symbols::NET_SendPacketSym);
	Detour::CheckFunction((void*)func_NET_SendPacket, "NET_SendPacket");
}