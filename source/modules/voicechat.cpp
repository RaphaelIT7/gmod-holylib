#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"
#include "Bootil/Bootil.h"
#include <netmessages.h>
#include "sourcesdk/baseclient.h"
#include "steam/isteamuser.h"

class CVoiceChatModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "voicechat"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; };
};

static ConVar voicechat_hooks("holylib_voicechat_hooks", "1", 0);

static CVoiceChatModule g_pVoiceChatModule;
IModule* pVoiceChatModule = &g_pVoiceChatModule;

struct VoiceData
{
	int iPlayerSlot = 0; // What if it's an invalid one ;D
	char pData[4096]; // Same as the engine
	int iLength = 0;
};

static int VoiceData_TypeID = -1;
void Push_VoiceData(VoiceData* buf)
{
	if (!buf)
	{
		g_Lua->PushNil();
		return;
	}

	g_Lua->PushUserType(buf, VoiceData_TypeID);
}

VoiceData* Get_VoiceData(int iStackPos, bool bError)
{
	if (bError)
	{
		if (!g_Lua->IsType(iStackPos, VoiceData_TypeID))
			g_Lua->ThrowError("Tried to use something that wasn't VoiceData!");

		VoiceData* pData = g_Lua->GetUserType<VoiceData>(iStackPos, VoiceData_TypeID);
		if (!pData)
			g_Lua->ThrowError("Tried to use a NULL VoiceData!");

		return pData;
	} else {
		if (!g_Lua->IsType(iStackPos, VoiceData_TypeID))
			return NULL;

		return g_Lua->GetUserType<VoiceData>(iStackPos, VoiceData_TypeID);
	}
}

LUA_FUNCTION_STATIC(VoiceData__tostring)
{
	VoiceData* pData = Get_VoiceData(1, false);
	if (!pData)
	{
		LUA->PushString("VoiceData [NULL]");
		return 1;
	}

	char szBuf[64] = {};
	V_snprintf(szBuf, sizeof(szBuf), "VoiceData [%i][%i]", pData->iPlayerSlot, pData->iLength);
	LUA->PushString(szBuf);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceData__index)
{
	if (!g_Lua->FindOnObjectsMetaTable(1, 2))
		LUA->PushNil();

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData__gc)
{
	VoiceData* pData = Get_VoiceData(1, false);
	if (pData)
	{
		LUA->SetUserType(1, NULL);
		delete pData;
	}

	return 0;
}

LUA_FUNCTION_STATIC(VoiceData_IsValid)
{
	VoiceData* pData = Get_VoiceData(1, false);

	LUA->PushBool(pData != nullptr);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetPlayerSlot)
{
	VoiceData* pData = Get_VoiceData(1, true);

	LUA->PushNumber(pData->iPlayerSlot);

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetLength)
{
	VoiceData* pData = Get_VoiceData(1, true);

	LUA->PushNumber(pData->iLength);

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetData)
{
	VoiceData* pData = Get_VoiceData(1, true);

	LUA->PushString(pData->pData, pData->iLength);

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetUncompressedData)
{
	VoiceData* pData = Get_VoiceData(1, true);

	if (!SteamUser())
		LUA->ThrowError("Failed to get SteamUser!\n");

	uint32 pDecompressedLength;
	char pDecompressed[20000];
	SteamUser()->DecompressVoice(
		pData->pData, pData->iLength,
		pDecompressed, sizeof(pDecompressed),
		&pDecompressedLength, 44100
	);

	LUA->PushString(pDecompressed, pDecompressedLength);

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_SetPlayerSlot)
{
	VoiceData* pData = Get_VoiceData(1, true);

	pData->iPlayerSlot = LUA->CheckNumber(2);

	return 0;
}

LUA_FUNCTION_STATIC(VoiceData_SetLength)
{
	VoiceData* pData = Get_VoiceData(1, true);

	pData->iLength = LUA->CheckNumber(2);
	if (pData->iLength > (int)sizeof(pData->pData))
		pData->iLength = sizeof(pData->pData);

	return 0;
}

LUA_FUNCTION_STATIC(VoiceData_SetData)
{
	VoiceData* pData = Get_VoiceData(1, true);

	const char* pStr = LUA->CheckString(2);
	int iLength = LUA->ObjLen(2);
	if (LUA->IsType(3, GarrysMod::Lua::Type::Number))
	{
		int iNewLength = LUA->GetNumber(3);
		iLength = MIN(iNewLength, iLength); // Don't allow one to go beyond the strength length
	}

	if (iLength > (int)sizeof(pData->pData))
		iLength = sizeof(pData->pData); // Don't allow one to copy too much.

	memcpy(pData->pData, pStr, iLength);

	return 0;
}

Detouring::Hook detour_SV_BroadcastVoiceData;
void hook_SV_BroadcastVoiceData(IClient* pClient, int nBytes, char* data, int64 xuid)
{
	if (g_pVoiceChatModule.InDebug())
		Msg("cl: %p\nbytes: %i\ndata: %p\n", pClient, nBytes, data);

	if (!voicechat_hooks.GetBool())
	{
		detour_SV_BroadcastVoiceData.GetTrampoline<Symbols::SV_BroadcastVoiceData>()(pClient, nBytes, data, xuid);
		return;
	}

	if (Lua::PushHook("HolyLib:PreProcessVoiceChat"))
	{
		VoiceData* pVoiceData = new VoiceData;
		pVoiceData->iLength = nBytes;
		memcpy(pVoiceData->pData, data, nBytes); // Is this safe? :^
		pVoiceData->iPlayerSlot = pClient->GetPlayerSlot();

		Util::Push_Entity((CBaseEntity*)Util::GetPlayerByClient((CBaseClient*)pClient));
		Push_VoiceData(pVoiceData);
		if (g_Lua->CallFunctionProtected(3, 1, true))
		{
			bool bHandled = g_Lua->GetBool(-1);
			g_Lua->Pop(1);
			if (bHandled)
				return;
		}
	}

	detour_SV_BroadcastVoiceData.GetTrampoline<Symbols::SV_BroadcastVoiceData>()(pClient, nBytes, data, xuid);
}

LUA_FUNCTION_STATIC(voicechat_SendEmptyData)
{
	CBasePlayer* pPlayer = Util::Get_Player(1, true); // Should error if given invalid player.
	CBaseClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!\n");

	SVC_VoiceData voiceData;
	voiceData.m_nFromClient = pClient->GetPlayerSlot();
	voiceData.m_nLength = 0;
	voiceData.m_DataOut = NULL; // Will possibly crash?
	voiceData.m_xuid = 0;

	pClient->SendNetMsg(voiceData);

	return 0;
}

LUA_FUNCTION_STATIC(voicechat_SendVoiceData)
{
	CBasePlayer* pPlayer = Util::Get_Player(1, true); // Should error if given invalid player.
	CBaseClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!\n");

	VoiceData* pData = Get_VoiceData(2, true);

	SVC_VoiceData voiceData;
	voiceData.m_nFromClient = pData->iPlayerSlot;
	voiceData.m_nLength = pData->iLength * 8; // In Bits...
	voiceData.m_DataOut = pData->pData;
	voiceData.m_xuid = 0;

	pClient->SendNetMsg(voiceData);

	return 0;
}

LUA_FUNCTION_STATIC(voicechat_ProcessVoiceData)
{
	CBasePlayer* pPlayer = Util::Get_Player(1, true); // Should error if given invalid player.
	CBaseClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!\n");

	VoiceData* pData = Get_VoiceData(2, true);

	if (!detour_SV_BroadcastVoiceData.IsValid())
		LUA->ThrowError("Missing valid detour for SV_BroadcastVoiceData!\n");

	detour_SV_BroadcastVoiceData.GetTrampoline<Symbols::SV_BroadcastVoiceData>()(
		pClient, pData->iLength, pData->pData, 0
	);

	return 0;
}

LUA_FUNCTION_STATIC(voicechat_CreateVoiceData)
{
	int iPlayerSlot = g_Lua->CheckNumberOpt(1, 0);
	const char* pStr = g_Lua->CheckStringOpt(2, NULL);
	int iLength = g_Lua->CheckNumberOpt(3, NULL);

	VoiceData* pData = new VoiceData;
	pData->iPlayerSlot = iPlayerSlot;

	if (pStr)
	{
		int iStrLength = LUA->ObjLen(2);
		if (iLength && iLength > iStrLength)
			iLength = iStrLength;

		if (!iLength)
			iLength = iStrLength;

		pData->iLength = iLength;
		memcpy(pData->pData, pStr, sizeof(pData->pData)); // Should we use V_strncpy? probably not
	}

	Push_VoiceData(pData);

	return 1;
}

void CVoiceChatModule::LuaInit(bool bServerInit)
{
	if (bServerInit) { return; }

	VoiceData_TypeID = g_Lua->CreateMetaTable("VoiceData");
		Util::AddFunc(VoiceData__tostring, "__tostring");
		Util::AddFunc(VoiceData__index, "__index");
		Util::AddFunc(VoiceData__gc, "__gc");
		Util::AddFunc(VoiceData_IsValid, "IsValid");
		Util::AddFunc(VoiceData_GetData, "GetData");
		Util::AddFunc(VoiceData_GetLength, "GetLength");
		Util::AddFunc(VoiceData_GetPlayerSlot, "GetPlayerSlot");
		Util::AddFunc(VoiceData_SetData, "SetData");
		Util::AddFunc(VoiceData_SetLength, "SetLength");
		Util::AddFunc(VoiceData_SetPlayerSlot, "SetPlayerSlot");
		Util::AddFunc(VoiceData_GetUncompressedData, "GetUncompressedData");
	g_Lua->Pop(1);

	Util::StartTable();
		Util::AddFunc(voicechat_SendEmptyData, "SendEmptyData");
		Util::AddFunc(voicechat_SendVoiceData, "SendVoiceData");
		Util::AddFunc(voicechat_ProcessVoiceData, "ProcessVoiceData");
		Util::AddFunc(voicechat_CreateVoiceData, "CreateVoiceData");
	Util::FinishTable("voicechat");
}

void CVoiceChatModule::LuaShutdown()
{
	Util::NukeTable("voicechat");
}

void CVoiceChatModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::ModuleLoader engine_loader("engine");
	Detour::Create(
		&detour_SV_BroadcastVoiceData, "SV_BroadcastVoiceData",
		engine_loader.GetModule(), Symbols::SV_BroadcastVoiceDataSym,
		(void*)hook_SV_BroadcastVoiceData, m_pID
	);
}