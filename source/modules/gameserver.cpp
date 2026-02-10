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

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CGameServerModule : public IModule
{
public:
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void InitDetour(bool bPreServer) override;
	void OnClientDisconnect(CBaseClient* pClient) override;
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

double net_time;
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

	INetMessageHandler *m_pMessageHandler = nullptr;
	bool Process() { Warning(PROJECT_NAME ": Tried to process this message? This should never happen!\n"); return true; };

	SVC_CustomMessage() { m_bReliable = false; }

	int	GetGroup() const { return INetChannelInfo::GENERIC; }

	int m_iType = 0;
	int m_iLength = -1;
	char m_strName[64] = "";
	bf_write m_DataOut;
};

PushReferenced_LuaClass(CBaseClient)
SpecialGet_LuaClass(CBaseClient, CHLTVClient, "CBaseClient", (gameserver_rawclients.GetBool() || pVar->IsConnected()))

Default__index(CBaseClient);
Default__newindex(CBaseClient);
Default__GetTable(CBaseClient);

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

	LUA->PushBool(pClient != nullptr && pClient->IsConnected());
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

	LUA->PushBool(pClient->IsHearingClient(nPlayerSlot));
	return 1;
}

LUA_FUNCTION_STATIC(CBaseClient_IsProximityHearingClient)
{
	CBaseClient* pClient = Get_CBaseClient(LUA, 1, true);
	int nPlayerSlot = (int)LUA->CheckNumber(2);

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
	Util::AddFunc(pLua, CBaseClient_GetRemoteFramerate, "GetRemoteFramerate");
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

	LUA->PushBool(pNetChannel != nullptr);
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

static std::unordered_set<ILuaNetMessageHandler*> g_pNetMessageHandlers;
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
		return 0;

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

	return 0;
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

LUA_FUNCTION_STATIC(gameserver_CreateNewClient)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	int nSlot = LUA->CheckNumber(1);
	CBaseServer* pServer = (CBaseServer*)Util::server;
	Push_CBaseClient(LUA, pServer->CreateNewClient(nSlot));
	return 1;
}

LUA_FUNCTION_STATIC(gameserver_GetFreeClient)
{
	if (!Util::server || !Util::server->IsActive())
		return 0;

	netadrnew_t adr;
	adr.SetFromString(LUA->CheckString(1), LUA->GetBool(2));

	CBaseServer* pServer = (CBaseServer*)Util::server;
	Push_CBaseClient(LUA, pServer->GetFreeClient(*((netadr_t*)&adr)));
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
		Util::AddFunc(pLua, gameserver_SendConnectionlessPacket, "SendConnectionlessPacket");
		Util::AddFunc(pLua, gameserver_CreateFakeClient, "CreateFakeClient");
		Util::AddFunc(pLua, gameserver_CreateNewClient, "CreateNewClient");
		Util::AddFunc(pLua, gameserver_GetFreeClient, "GetFreeClient");

		Util::AddFunc(pLua, gameserver_CreateNetChannel, "CreateNetChannel");
		Util::AddFunc(pLua, gameserver_RemoveNetChannel, "RemoveNetChannel");
		Util::AddFunc(pLua, gameserver_GetCreatedNetChannels, "GetCreatedNetChannels");

		Util::AddValue(pLua, NS_CLIENT, "NS_CLIENT");
		Util::AddValue(pLua, NS_SERVER, "NS_SERVER");
		Util::AddValue(pLua, NS_HLTV, "NS_HLTV");
	Util::FinishTable(pLua, "gameserver");
}

void CGameServerModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "gameserver");

	DeleteAll_CBaseClient(pLua);
	DeleteAll_CNetChan(pLua);
}

#define MAX_PLAYERS 128
static ConVar gameserver_maxplayers("holylib_gameserver_maxplayers", "255", 0, "Experimental - max client limit (above 255 cannot be networked, though may work if they remain purely as a CGameClient)", true, 1, true, 8192);
static Detouring::Hook detour_CBaseServer_GetFreeClient;
static CBaseClient* hook_CBaseServer_GetFreeClient(CBaseServer* _this, netadr_t& adr)
{
	CBaseClient* pClient = detour_CBaseServer_GetFreeClient.GetTrampoline<Symbols::CBaseServer_GetFreeClient>()(_this, adr);
	if (!pClient)
	{
		if (_this->GetClientCount() > gameserver_maxplayers.GetInt())
			return nullptr;

		pClient = _this->CreateNewClient(_this->GetClientCount());
		if (pClient)
			_this->m_Clients.AddToTail(pClient);
	}

	return pClient;
}

static Detouring::Hook detour_CBaseServer_CreateFakeClient;
static CBaseClient* hook_CBaseServer_CreateFakeClient(CBaseServer* _this, const char* pName)
{
	netadr_t adr;
	CBaseClient* pClient = detour_CBaseServer_GetFreeClient.GetTrampoline<Symbols::CBaseServer_GetFreeClient>()(_this, adr);
	if (!pClient || pClient->m_nClientSlot >= MAX_PLAYERS)
		return nullptr;

	return detour_CBaseServer_CreateFakeClient.GetTrampoline<Symbols::CBaseServer_CreateFakeClient>()(_this, pName);
}

static Detouring::Hook detour_CBaseServer_UserInfoChanged;
static void hook_CBaseServer_UserInfoChanged(CBaseServer* _this, int nClientIndex)
{
	if (nClientIndex >= MAX_PLAYERS)
		return;

	detour_CBaseServer_UserInfoChanged.GetTrampoline<Symbols::CBaseServer_UserInfoChanged>()(_this, nClientIndex);
}

static Detouring::Hook detour_CGameServer_RemoveClientFromGame;
static void hook_CGameServer_RemoveClientFromGame(CBaseServer* _this, CBaseClient* pClient)
{
	if (pClient->m_nClientSlot >= MAX_PLAYERS)
		return;

	detour_CGameServer_RemoveClientFromGame.GetTrampoline<Symbols::CGameServer_RemoveClientFromGame>()(_this, pClient);
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
	pServer->m_nMaxclients = gameserver_maxplayers.GetInt();

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
			}

			cl->Disconnect( "Client %d overflowed reliable channel.", i );
		}
	}
}

class CExtendedNetMessage : public CNetMessage
{
public:
	INetMessageHandler *m_pMessageHandler;
};

/*
 * Moving a entire CGameClient into another CGameClient to hopefully not make the engine too angry.
 * This is required to preserve the logic of m_nEntityIndex = m_nClientSlot + 1
 * We don't copy everything, like the baseline and such.
 *
 * Current State: It works
 * Previous Bugs:
 * - The Client's LocalPlayer is a NULL Entity.....
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
	target->m_OwnerSteamID = origin->m_OwnerSteamID;

	memcpy(target->m_FriendsName, origin->m_FriendsName, sizeof(origin->m_FriendsName));
	memcpy(target->m_GUID, origin->m_GUID, sizeof(origin->m_GUID));

	target->m_fTimeLastNameChange = origin->m_fTimeLastNameChange;
	memcpy(target->m_szPendingNameChange, origin->m_szPendingNameChange, sizeof(origin->m_szPendingNameChange));

	/*
	 * Update CNetChan and CNetMessage's properly to not crash.
	 */

	CNetChan* chan = (CNetChan*)target->m_NetChannel;
	chan->m_MessageHandler = (INetChannelHandler*)target;

	FOR_EACH_VEC(chan->m_NetMessages, i)
	{
		CExtendedNetMessage* msg = (CExtendedNetMessage*)chan->m_NetMessages[i];
		msg->m_pMessageHandler = target;

		if (msg->GetType() == clc_CmdKeyValues)
		{
			Base_CmdKeyValues* keyVal = (Base_CmdKeyValues*)msg;
			if (keyVal->m_pKeyValues)
			{
				keyVal->m_pKeyValues = nullptr; // Will leak memory but we can't safely delete it currently.
				// ToDo: Fix this small memory leak.
			}
		}
	}

	/*
	 * Nuke the origin client
	 */

	origin->m_NetChannel = nullptr; // Nuke the net channel or else it might touch it.
	//origin->m_ConVars = nullptr; // Same here
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
static void hook_CGameClient_SpawnPlayer(CGameClient* client)
{
	// m_nClientSlot = player slot! (entIndex - 1)
	if (client->m_nClientSlot < MAX_PLAYERS || gameserver_disablespawnsafety.GetBool())
	{
		detour_CGameClient_SpawnPlayer.GetTrampoline<Symbols::CGameClient_SpawnPlayer>()(client);
		return;
	}

	// ent index! can be 128!
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
void CGameServerModule::OnClientDisconnect(CBaseClient* pClient)
{
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

// Hotfix until GMod has it fixed - https://github.com/Facepunch/garrysmod-issues/issues/6722
static Detouring::Hook detour_CVoiceGameMgr_ClientConnected;
void hook_CVoiceGameMgr_ClientConnected(void* _this, edict_t* pEdict)
{
	if (!pEdict || (pEdict->m_EdictIndex-1) >= MAX_PLAYERS)
		return;

	detour_CVoiceGameMgr_ClientConnected.GetTrampoline<Symbols::CVoiceGameMgr_ClientConnected>()(_this, pEdict);
}

#if SYSTEM_WINDOWS
DETOUR_THISCALL_START()
	DETOUR_THISCALL_ADDFUNC1(hook_CBaseServer_GetFreeClient, Base_GetFreeClient, CBaseServer*, netadr_t&);
	DETOUR_THISCALL_ADDFUNC1(hook_CBaseServer_CreateFakeClient, Base_CreateFakeClient, CBaseServer*, const char*);
	DETOUR_THISCALL_ADDFUNC1(hook_CBaseServer_UserInfoChanged, Base_UserInfoChanged, CBaseServer*, int);
	DETOUR_THISCALL_ADDFUNC1(hook_CGameServer_RemoveClientFromGame, Game_RemoveClientFromGame, CBaseServer*, CBaseClient*);
	DETOUR_THISCALL_ADDFUNC0(hook_CSteam3Server_SendUpdatedServerDetails, Steam_SendUpdatedServerDetails, void*);
	DETOUR_THISCALL_ADDFUNC0(hook_CBaseServer_CheckTimeouts, CheckTimeouts, CBaseServer*);
	DETOUR_THISCALL_ADDFUNC0(hook_CGameClient_SpawnPlayer, SpawnPlayer, CGameClient*);
	DETOUR_THISCALL_ADDRETFUNC2(hook_CBaseClient_SetSignonState, bool, SetSignonState, CBaseClient*, int, int);
	DETOUR_THISCALL_ADDRETFUNC0(hook_CBaseServer_IsMultiplayer, bool, IsMultiplayer, CBaseServer*);
	DETOUR_THISCALL_ADDRETFUNC0(hook_GModDataPack_IsSingleplayer, bool, IsSingleplayer, void*);
	DETOUR_THISCALL_ADDRETFUNC0(hook_CBaseClient_ShouldSendMessages, bool, ShouldSendMessages, CBaseClient*);
	DETOUR_THISCALL_ADDRETFUNC1(hook_CBaseServer_ProcessConnectionlessPacket, bool, ProcessConnectionlessPacket, IServer*, netpacket_s*);
	DETOUR_THISCALL_ADDRETFUNC1(hook_CNetChan_SendDatagram, int, SendDatagram, CNetChan*, bf_write*);
	DETOUR_THISCALL_ADDFUNC0(hook_CNetChan_D2, D2, CNetChan*);
DETOUR_THISCALL_FINISH()
#endif

void CGameServerModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	DETOUR_PREPARE_THISCALL();
	SourceSDK::FactoryLoader engine_loader("engine");
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

#if ARCHITECTURE_IS_X86
	Detour::Create(
		&detour_CBaseClient_ShouldSendMessages, "CBaseClient::ShouldSendMessages",
		engine_loader.GetModule(), Symbols::CBaseClient_ShouldSendMessagesSym,
		(void*)DETOUR_THISCALL(hook_CBaseClient_ShouldSendMessages, ShouldSendMessages), m_pID
	);
#endif

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

	Detour::Create(
		&detour_NET_SetTime, "NET_SetTime",
		engine_loader.GetModule(), Symbols::NET_SetTimeSym,
		(void*)hook_NET_SetTime, m_pID
	);

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

	Detour::Create(
		&detour_CVoiceGameMgr_ClientConnected, "CVoiceGameMgr::ClientConnected",
		server_loader.GetModule(), Symbols::CVoiceGameMgr_ClientConnectedSym,
		(void*)hook_CVoiceGameMgr_ClientConnected, m_pID
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