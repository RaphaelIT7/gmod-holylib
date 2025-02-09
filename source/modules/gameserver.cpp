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

double		net_time;

class CGameServerModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "gameserver"; };
	virtual int Compatibility() { return LINUX32; };
};

CGameServerModule g_pGameServerModule;
IModule* pGameServerModule = &g_pGameServerModule;

class SVC_CustomMessage: public CNetMessage
{
public:
	bool			ReadFromBuffer( bf_read &buffer ) { return true; };
	bool			WriteToBuffer( bf_write &buffer ) {
		if (m_iLength == -1)
			m_iLength = m_DataOut.GetNumBitsWritten();

		buffer.WriteUBitLong(GetType(), NETMSG_TYPE_BITS);
		return buffer.WriteBits(m_DataOut.GetData(), m_iLength);
	};
	const char		*ToString() const { return "HolyLib:CustomMessage"; };
	int				GetType() const { return m_iType; }
	const char		*GetName() const { return m_strName;}

	INetMessageHandler *m_pMessageHandler = NULL;
	bool Process() { Warning("holylib: Tried to process this message? This should never happen!\n"); return true; };

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
	CBaseClient* pClient = (CBaseClient*)data->pData;
	if (data->pAdditionalData != pClient->GetUserID())
	{
		data->pAdditionalData = pClient->GetUserID();
		LUA->ReferenceFree(data->iTableReference);
		LUA->CreateTable();
		data->iTableReference = LUA->ReferenceCreate();
	}

	Util::ReferencePush(LUA, data->iTableReference); // This should never crash so no safety checks.

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

	pClient->Disconnect(strReason);
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
	strcpy(msg.m_strName, strName);
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

LUA_FUNCTION_STATIC(CBaseClient_Transmit)
{
	CBaseClient* pClient = Get_CBaseClient(1, true);
	bool bOnlyReliable = LUA->GetBool(2);
	CNetChan* pNetChannel = (CNetChan*)pClient->GetNetChannel();
	if (!pNetChannel)
		LUA->ThrowError("Failed to get a valid net channel");

	LUA->PushBool(pNetChannel->Transmit(bOnlyReliable));
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
	strcpy(msg.m_strName, strName);
	msg.m_DataOut.StartWriting(bf->GetData(), 0, 0, bf->GetMaxNumBits());
	msg.m_iLength = bf->GetNumBitsWritten();

	Util::server->BroadcastMessage(msg);
	return 0;
}

extern CGlobalVars* gpGlobals;
static ConVar* sv_stressbots;
void CGameServerModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	sv_stressbots = g_pCVar->FindVar("sv_stressbots");
	if (!sv_stressbots)
		Warning("holylib: Failed to find sv_stressbots convar!\n");

	CBaseClient_TypeID = g_Lua->CreateMetaTable("CGameClient");
		Push_CBaseClientMeta();

		Util::AddFunc(CGameClient__tostring, "__tostring");
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
	Util::FinishTable("gameserver");
}

void CGameServerModule::LuaShutdown()
{
}

static Detouring::Hook detour_CServerGameClients_GetPlayerLimit;
static void hook_CServerGameClients_GetPlayerLimit(void* funkyClass, int& minPlayers, int& maxPlayers, int& defaultMaxPlayers)
{
	minPlayers = 1;
	maxPlayers = 255; // Allows one to go up to 255 slots.
	defaultMaxPlayers = 255;
}

/*
 * ToDo: Ask Rubat if were allowed to modify SVC_ServerInfo
 *       I think it "could" be considered breaking gmod server operator rules.
 *       "Do not fake server information. This mostly means player count, but other data also applies."
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
		Push_CBaseClient(cl);
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
			Push_CBaseClient((CBaseClient*)cl);
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
				Push_CBaseClient((CBaseClient*)cl);
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
		Msg("holylib: Reassigned client to from slot %i to %i\n", origin->m_nClientSlot, target->m_nClientSlot);

	target->Inactivate();
	target->Clear();

	/*
	 * NOTE: This will fire the player_connect and player_connect_client gameevents.
	 */
	target->Connect( origin->m_Name, origin->m_UserID, origin->m_NetChannel, origin->m_bFakePlayer, origin->m_clientChallenge );

	/*
	 * Basic CBaseClient::Connect setup
	 */

	//target->m_ConVars = origin->m_ConVars;
	//target->m_bInitialConVarsSet = origin->m_bInitialConVarsSet;
	//target->m_UserID = origin->m_UserID;
	target->SetName( origin->m_Name ); // Required thingy
	//target->m_bFakePlayer = origin->m_bFakePlayer;
	//target->m_NetChannel = origin->m_NetChannel;
	//target->m_clientChallenge = origin->m_clientChallenge;
	target->m_nSignonState = origin->m_nSignonState;
	//target->edict = Util::engineserver->PEntityOfEntIndex( target->m_nEntityIndex );
	//target->m_PackInfo.m_pClientEnt = target->edict;
	//target->m_PackInfo.m_nPVSSize = sizeof( target->m_PackInfo.m_PVS );

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
	chan->m_MessageHandler = target;

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
	if (client->m_nClientSlot < MAX_PLAYERS)
	{
		detour_CGameClient_SpawnPlayer.GetTrampoline<Symbols::CGameClient_SpawnPlayer>()(client);
		return;
	}

	int nextFreeEntity = FindFreeClientSlot();
	if (nextFreeEntity > MAX_PLAYERS)
	{
		Warning("holylib: Failed to find a valid player slot to use! Stopping client spawn! (%i, %i, %i)\n", client->m_nClientSlot, client->GetUserID(), nextFreeEntity);
		return;
	}

	CGameClient* pClient = (CGameClient*)Util::GetClientByIndex(nextFreeEntity - 1);
	if (pClient->m_nSignonState != SIGNONSTATE_NONE)
	{
		// It really didn't like what we had planned.
		Warning("holylib: Client collision! fk. Client will be refused to spawn! (%i - %s, %i - %s)\n", pClient->m_nClientSlot, pClient->GetClientName(), client->m_nClientSlot, client->GetClientName());
		return;
	}

	MoveCGameClientIntoCGameClient(client, pClient);
	//detour_CGameClient_SpawnPlayer.GetTrampoline<Symbols::CGameClient_SpawnPlayer>()(pClient);
}

// Called by SourceTV from CSteam3Server::NotifyClientDisconnect
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

	func_CBaseClient_OnRequestFullUpdate = (Symbols::CBaseClient_OnRequestFullUpdate)Detour::GetFunction(engine_loader.GetModule(), Symbols::CBaseClient_OnRequestFullUpdateSym);
	Detour::CheckFunction((void*)func_CBaseClient_OnRequestFullUpdate, "CBaseClient::OnRequestFullUpdate");

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
}