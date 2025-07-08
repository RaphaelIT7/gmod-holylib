#include "opus/opus_framedecoder.h"
#include "opus/steam_voice.h"
#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include <netmessages.h>
#include "sourcesdk/baseclient.h"
#include "steam/isteamclient.h"
#include <isteamutils.h>
#include "unordered_set"
#include "server.h"
#include "ivoiceserver.h"
#define private public // Try me.
#include "shareddefs.h"
#include "voice_gamemgr.h"
#undef private

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#include <recipientfilter.h>

class CVoiceChatModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax);
	virtual void LevelShutdown() OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual void LuaThink(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void PreLuaModuleLoaded(lua_State* L, const char* pFileName) OVERRIDE;
	virtual void PostLuaModuleLoaded(lua_State* L, const char* pFileName) OVERRIDE;
	virtual const char* Name() { return "voicechat"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32; };
	virtual bool SupportsMultipleLuaStates() { return true; };
};

static ConVar voicechat_hooks("holylib_voicechat_hooks", "1", 0);

static IThreadPool* pVoiceThreadPool = NULL;
static void OnVoiceThreadsChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	if (!pVoiceThreadPool)
		return;

	pVoiceThreadPool->ExecuteAll();
	pVoiceThreadPool->Stop();
	Util::StartThreadPool(pVoiceThreadPool, ((ConVar*)convar)->GetInt());
}

static ConVar voicechat_threads("holylib_voicechat_threads", "1", FCVAR_ARCHIVE, "The number of threads to use for voicechat.LoadVoiceStream and voicechat.SaveVoiceStream if you specify async", OnVoiceThreadsChange);

static CVoiceChatModule g_pVoiceChatModule;
IModule* pVoiceChatModule = &g_pVoiceChatModule;

struct VoiceData
{
	~VoiceData() {
		if (pData)
			delete[] pData;
	}

	inline void AllocData()
	{
		if (pData)
			delete[] pData;

		pData = new char[iLength]; // We won't need additional space right?
	}

	inline void SetData(const char* pNewData, int iNewLength)
	{
		iLength = iNewLength;
		AllocData();
		memcpy(pData, pNewData, iLength);
	}

	inline VoiceData* CreateCopy()
	{
		VoiceData* data = new VoiceData;
		data->bProximity = bProximity;
		data->iPlayerSlot = iPlayerSlot;
		data->SetData(pData, iLength);

		return data;
	}

	int iPlayerSlot = 0; // What if it's an invalid one ;D (It doesn't care.......)
	char* pData = NULL;
	int iLength = 0;
	bool bProximity = true;
};

Push_LuaClass(VoiceData)
Get_LuaClass(VoiceData, "VoiceData")

LUA_FUNCTION_STATIC(VoiceData__tostring)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, false);
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

Default__index(VoiceData);
Default__newindex(VoiceData);
Default__GetTable(VoiceData);
Default__gc(VoiceData,
	VoiceData* pVoiceData = (VoiceData*)pStoredData;
	if (pVoiceData)
		delete pVoiceData;
)

LUA_FUNCTION_STATIC(VoiceData_IsValid)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, false);

	LUA->PushBool(pData != nullptr);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetPlayerSlot)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	LUA->PushNumber(pData->iPlayerSlot);

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetLength)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	LUA->PushNumber(pData->iLength);

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetData)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	LUA->PushString(pData->pData, pData->iLength);

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetUncompressedData)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);
	int iSize = (int)LUA->CheckNumberOpt(2, 20000); // How many bytes to allocate for the decompressed version. 20000 is default

	ISteamUser* pSteamUser = Util::GetSteamUser();
	if (!pSteamUser)
		LUA->ThrowError("Failed to get SteamUser!\n");

	if (!pData->pData || pData->iLength == 0)
	{
		LUA->PushString("");
		return 1;
	}

	uint32 pDecompressedLength;
	char* pDecompressed = new char[iSize];
	pSteamUser->DecompressVoice(
		pData->pData, pData->iLength,
		pDecompressed, iSize,
		&pDecompressedLength, 44100
	);

	LUA->PushString(pDecompressed, pDecompressedLength);
	delete[] pDecompressed; // Lua will already have a copy of it.

	return 1;
}

static SteamOpus::Opus_FrameDecoder g_pOpusDecoder;
LUA_FUNCTION_STATIC(VoiceData_SetUncompressedData)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);
	const char* pUncompressedData = LUA->CheckString(2);
	int iSize = LUA->ObjLen(2);

	ISteamUser* pSteamUser = Util::GetSteamUser();
	if (!pSteamUser)
		LUA->ThrowError("Failed to get SteamUser!\n");

	if (!pData->pData || pData->iLength == 0)
	{
		LUA->PushString("");
		return 1;
	}

	char* pCompressed = new char[iSize];
	CBaseClient* pClient = Util::GetClientByIndex(pData->iPlayerSlot);
	uint64 steamID64 = 0;
	if (pClient)
	{
		steamID64 = pClient->GetNetworkID().steamid.ConvertToUint64(); // Crash any% speedrun
	}

	int pBytes = SteamVoice::CompressIntoBuffer(steamID64, &g_pOpusDecoder, pUncompressedData, iSize, pCompressed, 20000, 44100);
	if (pBytes != -1)
	{
		// Instead of calling SetData which copies it, we set it directly to avoid any additional copying.
		pData->pData = pCompressed;
		pData->iLength = pBytes;
		LUA->PushBool(true);
	} else {
		delete[] pCompressed;
		LUA->PushBool(false);
	}

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetProximity)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	LUA->PushBool(pData->bProximity);

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_SetPlayerSlot)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	pData->iPlayerSlot = (int)LUA->CheckNumber(2);

	return 0;
}

LUA_FUNCTION_STATIC(VoiceData_SetLength)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	pData->iLength = (int)LUA->CheckNumber(2);

	return 0;
}

LUA_FUNCTION_STATIC(VoiceData_SetData)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	const char* pStr = LUA->CheckString(2);
	int iLength = LUA->ObjLen(2);
	if (LUA->IsType(3, GarrysMod::Lua::Type::Number))
	{
		int iNewLength = (int)LUA->GetNumber(3);
		iLength = MIN(iNewLength, iLength); // Don't allow one to go beyond the strength length
	}

	pData->SetData(pStr, iLength);

	return 0;
}

LUA_FUNCTION_STATIC(VoiceData_SetProximity)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	pData->bProximity = LUA->GetBool(2);

	return 0;
}

LUA_FUNCTION_STATIC(VoiceData_CreateCopy)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	Push_VoiceData(LUA, pData->CreateCopy());
	return 1;
}

struct VoiceStream {
	~VoiceStream()
	{
		for (auto& [_, val] : pVoiceData)
			delete val;

		pVoiceData.clear();
	}

	/*
	 * VoiceStream file structure:
	 * 
	 * 4 bytes / int - total count of VoiceData
	 * 
	 * each entry:
	 * 4 bytes / int - tick number
	 * 4 bytes / int - length of data
	 * (length) bytes / bytes - the data
	 */
	void Save(FileHandle_t fh)
	{
		// Create a copy so that the main thread can still party on it.
		std::unordered_map<int, VoiceData*> voiceDataEnties = pVoiceData;

		int count = (int)voiceDataEnties.size();
		g_pFullFileSystem->Write(&count, sizeof(int), fh); // First write the total number of voice data

		for (auto& [tickNumber, voiceData] : voiceDataEnties)
		{
			g_pFullFileSystem->Write(&tickNumber, sizeof(int), fh);

			int length = voiceData->iLength;
			char* data = voiceData->pData;

			g_pFullFileSystem->Write(&length, sizeof(int), fh);
			g_pFullFileSystem->Write(data, length, fh);
		}
	}

	static VoiceStream* Load(FileHandle_t fh)
	{
		VoiceStream* pStream = new VoiceStream;

		int count;
		g_pFullFileSystem->Read(&count, sizeof(int), fh);

		for (int i=0; i<count; ++i)
		{
			int tickNumber;
			g_pFullFileSystem->Read(&tickNumber, sizeof(int), fh);

			int length;
			g_pFullFileSystem->Read(&length, sizeof(int), fh);

			char* data = new char[length];
			g_pFullFileSystem->Read(data, length, fh);

			VoiceData* voiceData = new VoiceData;
			voiceData->iLength = length;
			voiceData->pData = data;

			pStream->SetIndex(tickNumber, voiceData);
		}

		return pStream;
	}

	/*
	 * If you push it to Lua, call ->CreateCopy() on the VoiceData,
	 * we CANT push the VoiceData we store as else the GC will do funnies & crash.
	 */
	inline VoiceData* GetIndex(int index)
	{
		auto it = pVoiceData.find(index);
		if (it == pVoiceData.end())
			return NULL;

		return it->second;
	}

	/*
	 * We assume that the given VoiceData was NEVER pushed to Lua.
	 */
	inline void SetIndex(int index, VoiceData* pData)
	{
		auto it = pVoiceData.find(index);
		if (it != pVoiceData.end())
		{
			delete it->second;
			pVoiceData.erase(it);
		}

		pVoiceData[index] = pData;
	}

	/*
	 * We create a copy of EVERY voiceData.
	 */
	inline void CreateLuaTable(GarrysMod::Lua::ILuaInterface* pLua)
	{
		pLua->PreCreateTable(0, pVoiceData.size());
			for (auto& [tickCount, voiceData] : pVoiceData)
			{
				Push_VoiceData(pLua, voiceData->CreateCopy());
				Util::RawSetI(pLua, -2, tickCount);
			}
	}

	inline int GetCount()
	{
		return (int)pVoiceData.size();
	}

private:
	// key = tickcount
	// value = VoiceData
	std::unordered_map<int, VoiceData*> pVoiceData;
};

Push_LuaClass(VoiceStream)
Get_LuaClass(VoiceStream, "VoiceStream")

LUA_FUNCTION_STATIC(VoiceStream__tostring)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, false);
	if (!pStream)
	{
		LUA->PushString("VoiceStream [NULL]");
		return 1;
	}

	char szBuf[64] = {};
	V_snprintf(szBuf, sizeof(szBuf), "VoiceStream [%i]", pStream->GetCount());
	LUA->PushString(szBuf);
	return 1;
}

Default__index(VoiceStream);
Default__newindex(VoiceStream);
Default__GetTable(VoiceStream);
Default__gc(VoiceStream,
	VoiceStream* pVoiceData = (VoiceStream*)pStoredData;
	if (pVoiceData)
		delete pVoiceData;
)

LUA_FUNCTION_STATIC(VoiceStream_IsValid)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, false);

	LUA->PushBool(pStream != nullptr);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceStream_GetData)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);

	pStream->CreateLuaTable(LUA);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceStream_SetData)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);
	LUA->CheckType(2, GarrysMod::Lua::Type::Table);

	LUA->Push(2);
	LUA->PushNil();
	while (LUA->Next(-2))
	{
		// We could remove this, but that would mean that the key could NEVER be 0
		if (!LUA->IsType(-2, GarrysMod::Lua::Type::Number))
		{
			LUA->Pop(1);
			continue;
		}

		int tick = (int)LUA->GetNumber(-2); // key
		VoiceData* data = Get_VoiceData(LUA, -1, false); // value

		if (data)
		{
			pStream->SetIndex(tick, data->CreateCopy());
		}

		LUA->Pop(1);
	}
	LUA->Pop(1);
	return 0;
}

LUA_FUNCTION_STATIC(VoiceStream_GetCount)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);

	LUA->PushNumber(pStream->GetCount());
	return 1;
}

LUA_FUNCTION_STATIC(VoiceStream_GetIndex)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);
	int index = (int)LUA->CheckNumber(2);

	VoiceData* data = pStream->GetIndex(index);
	Push_VoiceData(LUA, data ? data->CreateCopy() : NULL);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceStream_SetIndex)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);
	int index = (int)LUA->CheckNumber(2);
	VoiceData* pData = Get_VoiceData(LUA, 3, true);

	pStream->SetIndex(index, pData->CreateCopy());
	return 0;
}

static CPlayerBitVec* g_PlayerModEnable;
static CPlayerBitVec* g_BanMasks;
static CPlayerBitVec* g_SentGameRulesMasks;
static CPlayerBitVec* g_SentBanMasks;
static CPlayerBitVec* g_bWantModEnable;
static double g_fLastPlayerTalked[ABSOLUTE_PLAYER_LIMIT] = {0};
static double g_fLastPlayerUpdated[ABSOLUTE_PLAYER_LIMIT] = {0};
static bool g_bIsPlayerTalking[ABSOLUTE_PLAYER_LIMIT] = {0};
static CVoiceGameMgr* g_pManager = NULL;
static ConVar voicechat_updateinterval("holylib_voicechat_updateinterval", "0.1", FCVAR_ARCHIVE, "How often we call PlayerCanHearPlayersVoice for the actively talking players. This interval is unique to each player");
static ConVar voicechat_managerupdateinterval("holylib_voicechat_managerupdateinterval", "0.1", FCVAR_ARCHIVE, "How often we loop through all players to check their voice states. We still check the player's interval to reduce calls if they already have been updated in the last x(your defined interval) seconds.");
static ConVar voicechat_stopdelay("holylib_voicechat_stopdelay", "1", FCVAR_ARCHIVE, "How many seconds before a player is marked as stopped talking");
static void UpdatePlayerTalkingState(CBasePlayer* pPlayer, bool bIsTalking = false)
{
	if (!g_pManager) // Skip if we have no manager.
		return;

	int iClient = pPlayer->edict()->m_EdictIndex-1;
	double fTime = Util::engineserver->Time();
	if (bIsTalking)
	{
		g_fLastPlayerTalked[iClient] = fTime;
	} else {
		// We update anyways, just to ensure that the code won't break, but we won't call the lua hook since we know their not talking and don't need it.
		if (g_bIsPlayerTalking[iClient] && (g_fLastPlayerTalked[iClient] + voicechat_stopdelay.GetFloat()) > fTime) // They are talking, and we have no reason to update so just skip it.
		{
			if (g_pVoiceChatModule.InDebug() == 2)
			{
				Msg("Skipping voice player update since their not talking! (%i, %s, %f, %f)\n", iClient, g_bIsPlayerTalking[iClient] ? "true" : "false", fTime, voicechat_stopdelay.GetFloat());
			}

			return;
		}
	}

	if ((g_fLastPlayerUpdated[iClient] + voicechat_updateinterval.GetFloat()) > fTime)
	{
		if (g_pVoiceChatModule.InDebug() == 2)
		{
			Msg("Skipping voice player update! (%i, %s, %f, %f, %f)\n", iClient, bIsTalking ? "true" : "false", g_fLastPlayerUpdated[iClient], fTime, voicechat_updateinterval.GetFloat());
		}

		return;
	}

	if (g_pVoiceChatModule.InDebug() == 2)
	{
		Msg("Doing voice player update! (%i, %s, %f, %f, %f)\n", iClient, bIsTalking ? "true" : "false", g_fLastPlayerUpdated[iClient], fTime, voicechat_updateinterval.GetFloat());
	}

	CSingleUserRecipientFilter user( pPlayer );

	// Request the state of their "VModEnable" cvar.
	if((*g_bWantModEnable)[iClient])
	{
		UserMessageBegin( user, "RequestState" );
		MessageEnd();
		// Since this is reliable, only send it once
		(*g_bWantModEnable)[iClient] = false;
	}

	ConVarRef sv_alltalk("sv_alltalk");
	bool bAllTalk = !!sv_alltalk.GetInt();

	CPlayerBitVec gameRulesMask;
	CPlayerBitVec ProximityMask;
	bool bProximity = false;
	if(bIsTalking && (*g_PlayerModEnable)[iClient] )
	{
		// Build a mask of who they can hear based on the game rules.
		for(int iOtherClient=0; iOtherClient < g_pManager->m_nMaxPlayers; iOtherClient++)
		{
			CBaseEntity *pEnt = Util::GetCBaseEntityFromEdict(Util::engineserver->PEntityOfEntIndex(iOtherClient + 1));
			if(pEnt && pEnt->IsPlayer() && 
				(bAllTalk || g_pManager->m_pHelper->CanPlayerHearPlayer((CBasePlayer*)pEnt, pPlayer, bProximity )) )
			{
				gameRulesMask[iOtherClient] = true;
				ProximityMask[iOtherClient] = bProximity;
			}
		}
	}

	// If this is different from what the client has, send an update. 
	if((gameRulesMask != g_SentGameRulesMasks[iClient] || 
		g_BanMasks[iClient] != g_SentBanMasks[iClient]))
	{
		g_SentGameRulesMasks[iClient] = gameRulesMask;
		g_SentBanMasks[iClient] = g_BanMasks[iClient];

		UserMessageBegin( user, "VoiceMask" );
			int dw;
			for(dw=0; dw < VOICE_MAX_PLAYERS_DW; dw++)
			{
				WRITE_LONG(gameRulesMask.GetDWord(dw));
				WRITE_LONG(g_BanMasks[iClient].GetDWord(dw));
			}
			WRITE_BYTE( !!(*g_PlayerModEnable)[iClient] );
		MessageEnd();
	}

	// Tell the engine.
	for(int iOtherClient=0; iOtherClient < g_pManager->m_nMaxPlayers; iOtherClient++)
	{
		bool bCanHear = gameRulesMask[iOtherClient] && !g_BanMasks[iClient][iOtherClient];
		g_pVoiceServer->SetClientListening( iOtherClient+1, iClient+1, bCanHear );

		if ( bCanHear )
		{
			g_pVoiceServer->SetClientProximity( iOtherClient+1, iClient+1, !!ProximityMask[iOtherClient] );
		}
	}

	g_fLastPlayerUpdated[iClient] = fTime;

	if ((g_fLastPlayerTalked[iClient] + voicechat_stopdelay.GetFloat()) > fTime)
	{
		g_bIsPlayerTalking[iClient] = true;
	} else {
		g_bIsPlayerTalking[iClient] = bIsTalking;
	}

	if (g_pVoiceChatModule.InDebug() == 2)
	{
		Msg("Updated voice player! (%i, %s, %f, %f)\n", iClient, bIsTalking ? "true" : "false", fTime, (g_fLastPlayerTalked[iClient] + voicechat_stopdelay.GetFloat()));
	}
}

static Detouring::Hook detour_CVoiceGameMgr_Update;
#if SYSTEM_WINDOWS && ARCHITECTURE_X86
static void __fastcall hook_CVoiceGameMgr_Update(double frametime)
{
	__asm {
		mov g_pManager, eax;
	}
#else
static void hook_CVoiceGameMgr_Update(CVoiceGameMgr* pManager, double frametime)
{
	g_pManager = pManager;
#endif
	g_pManager->m_UpdateInterval += frametime;
	if(g_pManager->m_UpdateInterval < voicechat_managerupdateinterval.GetFloat())
		return;

	if (g_pVoiceChatModule.InDebug() == 3)
	{
		Msg("Doing voice manager update!\n");
	}

	g_pManager->m_UpdateInterval = 0;
	for(int iClient=0; iClient < g_pManager->m_nMaxPlayers; iClient++)
	{
		CBaseEntity *pEnt = Util::GetCBaseEntityFromEdict(Util::engineserver->PEntityOfEntIndex(iClient + 1));
		if(!pEnt || !pEnt->IsPlayer())
			continue;

		CBasePlayer *pPlayer = (CBasePlayer*)pEnt;

		UpdatePlayerTalkingState(pPlayer, false);
	}
}

void CVoiceChatModule::ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
	for (int i = 0; i < ABSOLUTE_PLAYER_LIMIT; ++i)
	{
		g_fLastPlayerTalked[i] = 0.0;
		g_fLastPlayerUpdated[i] = 0.0;
		g_bIsPlayerTalking[i] = false;
	}
}

void CVoiceChatModule::LevelShutdown()
{
	for (int i = 0; i < ABSOLUTE_PLAYER_LIMIT; ++i)
	{
		g_fLastPlayerTalked[i] = 0.0;
		g_fLastPlayerUpdated[i] = 0.0;
		g_bIsPlayerTalking[i] = false;
	}
	g_pManager = NULL;
}

static Detouring::Hook detour_SV_BroadcastVoiceData;
static void hook_SV_BroadcastVoiceData(IClient* pClient, int nBytes, char* data, int64 xuid)
{
	VPROF_BUDGET("HolyLib - SV_BroadcastVoiceData", VPROF_BUDGETGROUP_HOLYLIB);

	if (g_pVoiceChatModule.InDebug() == 1)
		Msg("cl: %p\nbytes: %i\ndata: %p\n", pClient, nBytes, data);

	UpdatePlayerTalkingState(Util::GetPlayerByClient((CBaseClient*)pClient), true);

	if (!voicechat_hooks.GetBool())
	{
		detour_SV_BroadcastVoiceData.GetTrampoline<Symbols::SV_BroadcastVoiceData>()(pClient, nBytes, data, xuid);
		return;
	}

	if (Lua::PushHook("HolyLib:PreProcessVoiceChat"))
	{
		VoiceData* pVoiceData = new VoiceData;
		pVoiceData->SetData(data, nBytes);
		pVoiceData->iPlayerSlot = pClient->GetPlayerSlot();
		LuaUserData* pLuaData = Push_VoiceData(g_Lua, pVoiceData);
		// Stack: -3 = hook.Run(function) | -2 = hook name(string) | -1 = voicedata(userdata)
		
		g_Lua->Push(-3);
		// Stack: -4 = hook.Run(function) | -3 = hook name(string) | -2 = voicedata(userdata) | -1 = hook.Run(function)
		
		g_Lua->Push(-3);
		// Stack: -5 = hook.Run(function) | -4 = hook name(string) | -3 = voicedata(userdata) | -2 = hook.Run(function) | -1 = hook name(string)
		
		g_Lua->Remove(-5);
		// Stack: -4 = hook name(string) | -3 = voicedata(userdata) | -2 = hook.Run(function) | -1 = hook name(string)

		g_Lua->Remove(-4);
		// Stack: -3 = voicedata(userdata) | -2 = hook.Run(function) | -1 = hook name(string)

		Util::Push_Entity(g_Lua, (CBaseEntity*)Util::GetPlayerByClient((CBaseClient*)pClient));
		// Stack: -4 = voicedata(userdata) | -3 = hook.Run(function) | -2 = hook name(string) | -1 = entity(userdata)

		g_Lua->Push(-4);
		// Stack: -5 = voicedata(userdata) | -4 = hook.Run(function) | -3 = hook name(string) | -2 = entity(userdata) | -1 = voicedata(userdata)

		bool bHandled = false;
		if (g_Lua->CallFunctionProtected(3, 1, true))
		{
			bHandled = g_Lua->GetBool(-1);
			g_Lua->Pop(1);
		}

		// Stack: -1 = voicedata(userdata)

		if (pLuaData)
		{
			delete pLuaData;
		}

		delete pVoiceData;
		g_Lua->SetUserType(-1, NULL);
		g_Lua->Pop(1); // The voice data is still there, so now finally remove it.

		if (bHandled)
			return;
	}

	detour_SV_BroadcastVoiceData.GetTrampoline<Symbols::SV_BroadcastVoiceData>()(pClient, nBytes, data, xuid);
}

LUA_FUNCTION_STATIC(voicechat_SendEmptyData)
{
	CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true); // Should error if given invalid player.
	CBaseClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!\n");

	SVC_VoiceData voiceData;
	voiceData.m_nFromClient = (int)LUA->CheckNumberOpt(2, pClient->GetPlayerSlot());
	voiceData.m_nLength = 0;
	voiceData.m_DataOut = NULL; // Will possibly crash?
	voiceData.m_xuid = 0;

	pClient->SendNetMsg(voiceData);

	return 0;
}

LUA_FUNCTION_STATIC(voicechat_SendVoiceData)
{
	CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true); // Should error if given invalid player.
	IClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!\n");

	VoiceData* pData = Get_VoiceData(LUA, 2, true);

	SVC_VoiceData voiceData;
	voiceData.m_nFromClient = pData->iPlayerSlot;
	voiceData.m_nLength = pData->iLength * 8; // In Bits...
	voiceData.m_DataOut = pData->pData;
	voiceData.m_bProximity = pData->bProximity;
	voiceData.m_xuid = 0;

	pClient->SendNetMsg(voiceData);

	return 0;
}

LUA_FUNCTION_STATIC(voicechat_BroadcastVoiceData)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	SVC_VoiceData voiceData;
	voiceData.m_nFromClient = pData->iPlayerSlot;
	voiceData.m_nLength = pData->iLength * 8; // In Bits...
	voiceData.m_DataOut = pData->pData;
	voiceData.m_bProximity = pData->bProximity;
	voiceData.m_xuid = 0;

	if (LUA->IsType(2, GarrysMod::Lua::Type::Table))
	{
		LUA->Push(1);
		LUA->PushNil();
		while (LUA->Next(-2))
		{
			CBasePlayer* pPlayer = Util::Get_Player(LUA, -1, true);
			CBaseClient* pClient = Util::GetClientByPlayer(pPlayer);
			if (!pClient)
				LUA->ThrowError("Failed to get CBaseClient!\n");

			pClient->SendNetMsg(voiceData);

			LUA->Pop(1);
		}
		LUA->Pop(1);
	} else {
		for(CBaseClient* pClient : Util::GetClients())
			pClient->SendNetMsg(voiceData);
	}

	return 0;
}

LUA_FUNCTION_STATIC(voicechat_ProcessVoiceData)
{
	CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true); // Should error if given invalid player.
	CBaseClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!\n");

	VoiceData* pData = Get_VoiceData(LUA, 2, true);

	if (!DETOUR_ISVALID(detour_SV_BroadcastVoiceData))
		LUA->ThrowError("Missing valid detour for SV_BroadcastVoiceData!\n");

	UpdatePlayerTalkingState(pPlayer, true);
	detour_SV_BroadcastVoiceData.GetTrampoline<Symbols::SV_BroadcastVoiceData>()(
		pClient, pData->iLength, pData->pData, 0
	);

	return 0;
}

LUA_FUNCTION_STATIC(voicechat_CreateVoiceData)
{
	int iPlayerSlot = (int)LUA->CheckNumberOpt(1, 0);
	const char* pStr = LUA->CheckStringOpt(2, NULL);
	int iLength = (int)LUA->CheckNumberOpt(3, 0);

	VoiceData* pData = new VoiceData;
	pData->iPlayerSlot = iPlayerSlot;

	if (pStr)
	{
		int iStrLength = LUA->ObjLen(2);
		if (iLength && iLength > iStrLength)
			iLength = iStrLength;

		if (!iLength)
			iLength = iStrLength;

		pData->SetData(pStr, iLength);
	}

	Push_VoiceData(LUA, pData);

	return 1;
}

LUA_FUNCTION_STATIC(voicechat_IsHearingClient)
{
	CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true);
	IClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!\n");

	CBasePlayer* pTargetPlayer = Util::Get_Player(LUA, 2, true);
	IClient* pTargetClient = Util::GetClientByPlayer(pTargetPlayer);
	if (!pTargetClient)
		LUA->ThrowError("Failed to get CBaseClient for target player!\n");

	LUA->PushBool(pClient->IsHearingClient(pTargetClient->GetPlayerSlot()));

	return 1;
}

LUA_FUNCTION_STATIC(voicechat_IsProximityHearingClient)
{
	CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true);
	IClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!\n");

	CBasePlayer* pTargetPlayer = Util::Get_Player(LUA, 2, true);
	IClient* pTargetClient = Util::GetClientByPlayer(pTargetPlayer);
	if (!pTargetClient)
		LUA->ThrowError("Failed to get CBaseClient for target player!\n");

	LUA->PushBool(pClient->IsProximityHearingClient(pTargetClient->GetPlayerSlot()));

	return 1;
}

LUA_FUNCTION_STATIC(voicechat_CreateVoiceStream)
{
	Push_VoiceStream(LUA, new VoiceStream);
	return 1;
}

enum VoiceStreamTaskStatus {
	VoiceStreamTaskStatus_FAILED_FILE_NOT_FOUND = -2,
	VoiceStreamTaskStatus_FAILED_INVALID_TYPE = -1,
	VoiceStreamTaskStatus_NONE = 0,
	VoiceStreamTaskStatus_DONE = 1
};

enum VoiceStreamTaskType {
	VoiceStreamTask_NONE,
	VoiceStreamTask_SAVE,
	VoiceStreamTask_LOAD
};

struct VoiceStreamTask {
	~VoiceStreamTask()
	{
		if (iReference != -1)
		{
			pLua->ReferenceFree(iReference);
			iReference = -1;
		}

		if (iCallback != -1)
		{
			pLua->ReferenceFree(iCallback);
			iCallback = -1;
		}
	}

	char pFileName[MAX_PATH];
	char pGamePath[MAX_PATH];

	VoiceStreamTaskType iType = VoiceStreamTask_NONE;
	VoiceStreamTaskStatus iStatus = VoiceStreamTaskStatus_NONE;

	VoiceStream* pStream = NULL;
	int iReference = -1; // A reference to the pStream to stop the GC from kicking in.
	int iCallback = -1;
	GarrysMod::Lua::ILuaInterface* pLua = NULL;
};

class LuaVoiceModuleData : public Lua::ModuleData
{
public:
	std::unordered_set<VoiceStreamTask*> pVoiceStreamTasks;
};

static void VoiceStreamJob(VoiceStreamTask*& task)
{
	switch(task->iType)
	{
		case VoiceStreamTask_LOAD:
		{
			FileHandle_t fh = g_pFullFileSystem->Open(task->pFileName, "rb", task->pGamePath);
			if (fh)
			{
				task->pStream = VoiceStream::Load(fh);
				g_pFullFileSystem->Close(fh);
			} else {
				task->iStatus = VoiceStreamTaskStatus_FAILED_FILE_NOT_FOUND;
			}
			break;
		}
		case VoiceStreamTask_SAVE:
		{
			FileHandle_t fh = g_pFullFileSystem->Open(task->pFileName, "wb", task->pGamePath);
			if (fh)
			{
				task->pStream->Save(fh);
				g_pFullFileSystem->Close(fh);
			} else {
				task->iStatus = VoiceStreamTaskStatus_FAILED_FILE_NOT_FOUND;
			}
			break;
		}
		default:
		{
			Warning("holylib - VoiceChat(VoiceStreamJob): Managed to get a job without a type. How.\n");
			task->iStatus = VoiceStreamTaskStatus_FAILED_INVALID_TYPE;
			return;
		}
	}

	if (task->iStatus == VoiceStreamTaskStatus_NONE) // Wasn't set already? then just set it to done.
	{
		task->iStatus = VoiceStreamTaskStatus_DONE;
	}
}

LUA_FUNCTION_STATIC(voicechat_LoadVoiceStream)
{
	LuaVoiceModuleData* pData = (LuaVoiceModuleData*)Lua::GetLuaData(LUA)->GetModuleData(g_pVoiceChatModule.m_pID);

	const char* pFileName = LUA->CheckString(1);
	const char* pGamePath = LUA->CheckStringOpt(2, "DATA");
	bool bAsync = LUA->GetBool(3);
	if (bAsync)
	{
		LUA->CheckType(4, GarrysMod::Lua::Type::Function);
	}

	VoiceStreamTask* task = new VoiceStreamTask;
	V_strncpy(task->pFileName, pFileName, sizeof(task->pFileName));
	V_strncpy(task->pGamePath, pGamePath, sizeof(task->pGamePath));
	task->iType = VoiceStreamTask_LOAD;
	task->pLua = LUA;

	if (bAsync)
	{
		LUA->Push(4);
		task->iCallback = Util::ReferenceCreate(LUA, "voicechat.LoadVoiceStream - callback");
		pData->pVoiceStreamTasks.insert(task);
		pVoiceThreadPool->QueueCall(&VoiceStreamJob, task);
		return 0;
	} else {
		VoiceStreamJob(task);
		Push_VoiceStream(LUA, task->pStream);
		LUA->PushNumber((int)task->iStatus);
		delete task;
		return 2;
	}
}

LUA_FUNCTION_STATIC(voicechat_SaveVoiceStream)
{
	LuaVoiceModuleData* pData = (LuaVoiceModuleData*)Lua::GetLuaData(LUA)->GetModuleData(g_pVoiceChatModule.m_pID);

	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);
	const char* pFileName = LUA->CheckString(2);
	const char* pGamePath = LUA->CheckStringOpt(3, "DATA");
	bool bAsync = LUA->GetBool(4);
	if (bAsync)
	{
		LUA->CheckType(5, GarrysMod::Lua::Type::Function);
	}

	VoiceStreamTask* task = new VoiceStreamTask;
	V_strncpy(task->pFileName, pFileName, sizeof(task->pFileName));
	V_strncpy(task->pGamePath, pGamePath, sizeof(task->pGamePath));
	task->iType = VoiceStreamTask_SAVE;
	
	LUA->Push(1);
	task->iReference = Util::ReferenceCreate(LUA, "voicechat.SaveVoiceStream - VoiceStream");
	task->pStream = pStream;
	task->pLua = LUA;

	if (bAsync)
	{
		LUA->Push(5);
		task->iCallback = Util::ReferenceCreate(LUA, "voicechat.SaveVoiceStream - callback");
		pData->pVoiceStreamTasks.insert(task);
		pVoiceThreadPool->QueueCall(&VoiceStreamJob, task);
		return 0;
	} else {
		VoiceStreamJob(task);
		LUA->PushNumber((int)task->iStatus);
		delete task;
		return 1;
	}
}

LUA_FUNCTION_STATIC(voicechat_IsPlayerTalking)
{
	int iClient = -1;
	if (LUA->IsType(1, GarrysMod::Lua::Type::Number))
	{
		
	} else {
		CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true);
		iClient = pPlayer->edict()->m_EdictIndex-1;
	}

	if (iClient == -1 || iClient > ABSOLUTE_PLAYER_LIMIT)
		LUA->ThrowError("Failed to get a valid Client index!");

	LUA->PushBool(g_bIsPlayerTalking[iClient]);
	return 1;
}

LUA_FUNCTION_STATIC(voicechat_LastPlayerTalked)
{
	int iClient = -1;
	if (LUA->IsType(1, GarrysMod::Lua::Type::Number))
	{
		
	} else {
		CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true);
		iClient = pPlayer->edict()->m_EdictIndex-1;
	}

	if (iClient == -1 || iClient > ABSOLUTE_PLAYER_LIMIT)
		LUA->ThrowError("Failed to get a valid Client index!");

	LUA->PushNumber(g_fLastPlayerTalked[iClient]);
	return 1;
}

void CVoiceChatModule::LuaThink(GarrysMod::Lua::ILuaInterface* pLua)
{
	LuaVoiceModuleData* pData = (LuaVoiceModuleData*)Lua::GetLuaData(pLua)->GetModuleData(m_pID);

	if (pData->pVoiceStreamTasks.size() <= 0)
		return;

	for (auto it = pData->pVoiceStreamTasks.begin(); it != pData->pVoiceStreamTasks.end(); )
	{
		VoiceStreamTask* pTask = *it;
		if (pTask->iStatus == VoiceStreamTaskStatus_NONE)
		{
			it++;
			continue;
		}

		if (pTask->iCallback == -1)
		{
			Warning("holylib - VoiceChat(Think): somehow managed to get a task without a callback!\n");
			if (pTask->pStream != NULL && pTask->iType != VoiceStreamTask_SAVE)
				delete pTask->pStream;

			delete pTask;
			it = pData->pVoiceStreamTasks.erase(it);
			continue;
		}

		pLua->ReferencePush(pTask->iCallback);
		Push_VoiceStream(pLua, pTask->pStream); // Lua GC will take care of deleting.
		pLua->PushBool(pTask->iStatus == VoiceStreamTaskStatus_DONE);

		pLua->CallFunctionProtected(2, 0, true);
		
		delete pTask;
		it = pData->pVoiceStreamTasks.erase(it);
	}
}

void CVoiceChatModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Lua::GetLuaData(pLua)->SetModuleData(m_pID, new LuaVoiceModuleData);

	Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::VoiceData, pLua->CreateMetaTable("VoiceData"));
		Util::AddFunc(pLua, VoiceData__tostring, "__tostring");
		Util::AddFunc(pLua, VoiceData__index, "__index");
		Util::AddFunc(pLua, VoiceData__newindex, "__newindex");
		Util::AddFunc(pLua, VoiceData__gc, "__gc");
		Util::AddFunc(pLua, VoiceData_GetTable, "GetTable");
		Util::AddFunc(pLua, VoiceData_IsValid, "IsValid");
		Util::AddFunc(pLua, VoiceData_GetData, "GetData");
		Util::AddFunc(pLua, VoiceData_GetLength, "GetLength");
		Util::AddFunc(pLua, VoiceData_GetPlayerSlot, "GetPlayerSlot");
		Util::AddFunc(pLua, VoiceData_SetData, "SetData");
		Util::AddFunc(pLua, VoiceData_SetLength, "SetLength");
		Util::AddFunc(pLua, VoiceData_SetPlayerSlot, "SetPlayerSlot");
		Util::AddFunc(pLua, VoiceData_GetUncompressedData, "GetUncompressedData");
		Util::AddFunc(pLua, VoiceData_SetUncompressedData, "SetUncompressedData");
		Util::AddFunc(pLua, VoiceData_GetProximity, "GetProximity");
		Util::AddFunc(pLua, VoiceData_SetProximity, "SetProximity");
		Util::AddFunc(pLua, VoiceData_CreateCopy, "CreateCopy");
	pLua->Pop(1);

	Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::VoiceStream, pLua->CreateMetaTable("VoiceStream"));
		Util::AddFunc(pLua, VoiceStream__tostring, "__tostring");
		Util::AddFunc(pLua, VoiceStream__index, "__index");
		Util::AddFunc(pLua, VoiceStream__newindex, "__newindex");
		Util::AddFunc(pLua, VoiceStream__gc, "__gc");
		Util::AddFunc(pLua, VoiceStream_GetTable, "GetTable");
		Util::AddFunc(pLua, VoiceStream_IsValid, "IsValid");
		Util::AddFunc(pLua, VoiceStream_GetData, "GetData");
		Util::AddFunc(pLua, VoiceStream_SetData, "SetData");
		Util::AddFunc(pLua, VoiceStream_GetCount, "GetCount");
		Util::AddFunc(pLua, VoiceStream_GetIndex, "GetIndex");
		Util::AddFunc(pLua, VoiceStream_SetIndex, "SetIndex");
	pLua->Pop(1);

	Util::StartTable(pLua);
		Util::AddFunc(pLua, voicechat_SendEmptyData, "SendEmptyData");
		Util::AddFunc(pLua, voicechat_SendVoiceData, "SendVoiceData");
		Util::AddFunc(pLua, voicechat_BroadcastVoiceData, "BroadcastVoiceData");
		Util::AddFunc(pLua, voicechat_ProcessVoiceData, "ProcessVoiceData");
		Util::AddFunc(pLua, voicechat_CreateVoiceData, "CreateVoiceData");
		Util::AddFunc(pLua, voicechat_IsHearingClient, "IsHearingClient");
		Util::AddFunc(pLua, voicechat_IsProximityHearingClient, "IsProximityHearingClient");
		Util::AddFunc(pLua, voicechat_CreateVoiceStream, "CreateVoiceStream");
		Util::AddFunc(pLua, voicechat_LoadVoiceStream, "LoadVoiceStream");
		Util::AddFunc(pLua, voicechat_SaveVoiceStream, "SaveVoiceStream");
		Util::AddFunc(pLua, voicechat_IsPlayerTalking, "IsPlayerTalking");
		Util::AddFunc(pLua, voicechat_LastPlayerTalked, "LastPlayerTalked");
	Util::FinishTable(pLua, "voicechat");
}

void CVoiceChatModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "voicechat");
}

IVoiceServer* g_pVoiceServer = NULL;
void CVoiceChatModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	if (appfn[0])
	{
		g_pVoiceServer = (IVoiceServer*)appfn[0](INTERFACEVERSION_VOICESERVER, NULL);
	} else {
		SourceSDK::FactoryLoader engine_loader("engine");
		g_pVoiceServer = engine_loader.GetInterface<IVoiceServer>(INTERFACEVERSION_VOICESERVER);
	}

	Detour::CheckValue("get interface", "g_pVoiceServer", g_pVoiceServer != NULL);
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

	SourceSDK::FactoryLoader server_loader("server");
	Detour::Create(
		&detour_CVoiceGameMgr_Update, "CVoiceGameMgr::Update",
		server_loader.GetModule(), Symbols::CVoiceGameMgr_UpdateSym,
		(void*)hook_CVoiceGameMgr_Update, m_pID
	);

	g_PlayerModEnable = Detour::ResolveSymbol<CPlayerBitVec>(server_loader, Symbols::g_PlayerModEnableSym);
	Detour::CheckValue("get class", "g_PlayerModEnable", g_PlayerModEnable != NULL);

	g_BanMasks = Detour::ResolveSymbol<CPlayerBitVec>(server_loader, Symbols::g_BanMasksSym);
	Detour::CheckValue("get class", "g_BanMasks", g_BanMasks != NULL);

	g_SentGameRulesMasks = Detour::ResolveSymbol<CPlayerBitVec>(server_loader, Symbols::g_SentGameRulesMasksSym);
	Detour::CheckValue("get class", "g_SentGameRulesMasks", g_SentGameRulesMasks != NULL);

	g_SentBanMasks = Detour::ResolveSymbol<CPlayerBitVec>(server_loader, Symbols::g_SentBanMasksSym);
	Detour::CheckValue("get class", "g_SentBanMasks", g_SentBanMasks != NULL);

	g_bWantModEnable = Detour::ResolveSymbol<CPlayerBitVec>(server_loader, Symbols::g_bWantModEnableSym);
	Detour::CheckValue("get class", "g_bWantModEnable", g_bWantModEnable != NULL);
}

void CVoiceChatModule::PreLuaModuleLoaded(lua_State* L, const char* pFileName)
{
	std::string_view strFileName = pFileName;
	if (strFileName.find("voicebox") !=std::string::npos)
	{
		Msg(PROJECT_NAME " - voicechat: Removing SV_BroadcastVoiceData hook before voicebox is loaded\n");
		detour_SV_BroadcastVoiceData.Disable();
		detour_SV_BroadcastVoiceData.Destroy();
	}
}

void CVoiceChatModule::PostLuaModuleLoaded(lua_State* L, const char* pFileName)
{
	std::string_view strFileName = pFileName;
	if (strFileName.find("voicebox") !=std::string::npos)
	{
		Msg(PROJECT_NAME " - voicechat: Recreating SV_BroadcastVoiceData hook after voicebox was loaded\n");
		SourceSDK::ModuleLoader engine_loader("engine");
		Detour::Create(
			&detour_SV_BroadcastVoiceData, "SV_BroadcastVoiceData",
			engine_loader.GetModule(), Symbols::SV_BroadcastVoiceDataSym,
			(void*)hook_SV_BroadcastVoiceData, m_pID
		);
	}
}