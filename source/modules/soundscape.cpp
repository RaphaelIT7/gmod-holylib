#include "module.h"
#include "symbols.h"
#include "detours.h"
#include "LuaInterface.h"
#include "lua.h"
#include "player.h"
#define private public
#include "soundscape_system.h"
#include "soundscape.h"
#undef private

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CSoundscapeModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual void ClientDisconnect(edict_t* pClient) OVERRIDE;
	virtual const char* Name() { return "soundscape"; };
	virtual int Compatibility() { return LINUX32; };
	virtual bool IsEnabledByDefault() { return true; };
};

static CSoundscapeModule g_pSoundscapeModule;
IModule* pSoundscapeModule = &g_pSoundscapeModule;

static DTVarByOffset m_Local_Offset("DT_LocalPlayerExclusive", "m_Local");
static DTVarByOffset m_Audio_Offset("DT_Local", "m_audio.localSound[0]");
static inline audioparams_t* GetAudioParams(void* pPlayer)
{
	void* pLocal = m_Local_Offset.GetPointer(pPlayer);
	if (!pLocal)
		return nullptr;

	void* pAudio = m_Audio_Offset.GetPointer(pLocal);
	if (!pAudio)
		return nullptr;

	// We need to shift it since audioparams_t has a vtable???
	// Do we need to shift it by one void* or two? Idk
	return (audioparams_t*)((char*)pAudio - sizeof(void*));
}

LUA_FUNCTION_STATIC(soundscape_GetActiveSoundScape)
{
	CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true);
	audioparams_t* pParams = GetAudioParams(pPlayer);
	if (!pParams) // Should normally never happen, though doesn't hurt to be sure
		LUA->ThrowError("Failed to get audioparams_t!");
	
	Util::Push_Entity(LUA, Util::GetCBaseEntityFromHandle(pParams->ent.m_Value));
	return 1;
}

LUA_FUNCTION_STATIC(soundscape_GetActiveSoundScapeIndex)
{
	CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true);
	audioparams_t* pParams = GetAudioParams(pPlayer);
	if (!pParams) // Should normally never happen, though doesn't hurt to be sure
		LUA->ThrowError("Failed to get audioparams_t!");
	
	LUA->PushNumber(pParams->soundscapeIndex);
	return 1;
}

LUA_FUNCTION_STATIC(soundscape_GetActivePositions)
{
	CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true);
	audioparams_t* pParams = GetAudioParams(pPlayer);
	if (!pParams) // Should normally never happen, though doesn't hurt to be sure
		LUA->ThrowError("Failed to get audioparams_t!");
	
	LUA->PreCreateTable(NUM_AUDIO_LOCAL_SOUNDS, 0);
		int nTableIndex = 0;
		int localBits = pParams->localBits.Get();
		for (int nIndex=0; nIndex<NUM_AUDIO_LOCAL_SOUNDS; ++nIndex)
		{
			if (!(localBits & (1 << nIndex))) // If a bit isn't set, its not used currently.
				continue;
			
			Push_CopyVector(LUA, new Vector(pParams->localSound[nIndex]));
			Util::RawSetI(LUA, -2, ++nTableIndex);
		}

	return 1;
}

static Symbols::CEnvSoundscape_WriteAudioParamsTo func_CEnvSoundscape_WriteAudioParamsTo = nullptr;
LUA_FUNCTION_STATIC(soundscape_SetActiveSoundscape)
{
	CBaseEntity* pPlayer = Util::Get_Player(LUA, 1, true);
	CBaseEntity* pSoundscape = Util::Get_Entity(LUA, 2, true);
	if (V_stricmp(pSoundscape->GetClassname(), "env_soundscape") != 0)
		LUA->ThrowError("Tried to give a entity which wasn't a env_soundscape!");

	audioparams_t* pParams = GetAudioParams(pPlayer);
	if (pParams && func_CEnvSoundscape_WriteAudioParamsTo)
		func_CEnvSoundscape_WriteAudioParamsTo(pSoundscape, *pParams);

	return 0;
}

static CBitVec<MAX_PLAYERS> pBlockEngineAction;
LUA_FUNCTION_STATIC(soundscape_BlockEngineChanges)
{
	CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true);
	bool bShouldBlock = LUA->GetBool(2);

	int iClient = pPlayer->edict()->m_EdictIndex-1;
	if (bShouldBlock) {
		pBlockEngineAction.Set(iClient);
	} else {
		pBlockEngineAction.Clear(iClient);
	}

	return 0;
}

void CSoundscapeModule::ClientDisconnect(edict_t* pClient)
{
	// We unset the bit so that the next client that gets this slot won't be affected by the previous player.
	pBlockEngineAction.Clear(pClient->m_EdictIndex-1);
}

static CSoundscapeSystem* g_pSoundscapeSystem = nullptr;
LUA_FUNCTION_STATIC(soundscape_GetAll)
{
	if (!g_pSoundscapeSystem)
		LUA->ThrowError("Failed to load g_pSoundscapeSystem!");

	LUA->CreateTable();

	for (auto nIndex=g_pSoundscapeSystem->m_soundscapes.First(); nIndex != g_pSoundscapeSystem->m_soundscapes.InvalidIndex(); nIndex = g_pSoundscapeSystem->m_soundscapes.Next(nIndex))
	{
		LUA->PushString(g_pSoundscapeSystem->m_soundscapes.GetStringForKey(nIndex));
		LUA->PushNumber(g_pSoundscapeSystem->m_soundscapes.GetIDForKey(nIndex));
		
		LUA->RawSet(-3);
	}

	return 1;
}

static bool IsValidStringIndex(int nIndex)
{
	if (!g_pSoundscapeSystem)
		return false;

	return nIndex >= 0 && nIndex < g_pSoundscapeSystem->m_soundscapeCount;
}

LUA_FUNCTION_STATIC(soundscape_GetNameByIndex)
{
	if (!g_pSoundscapeSystem)
		LUA->ThrowError("Failed to load g_pSoundscapeSystem!");

	int nIndex = LUA->CheckNumber(1);
	if (!IsValidStringIndex(nIndex))
	{
		LUA->PushNil();
		return 1;
	}

	const char* pName = g_pSoundscapeSystem->m_soundscapes.GetStringText(nIndex);
	if (!pName)
	{
		LUA->PushNil();
		return 1;
	}

	LUA->PushString(pName);
	return 1;
}

LUA_FUNCTION_STATIC(soundscape_GetIndexByName)
{
	if (!g_pSoundscapeSystem)
		LUA->ThrowError("Failed to load g_pSoundscapeSystem!");

	const char* pName = LUA->CheckString(1);

	int nIndex = g_pSoundscapeSystem->m_soundscapes.GetStringID(pName);
	if (IsValidStringIndex(nIndex))
		LUA->PushNumber(nIndex);
	else
		LUA->PushNil();

	return 1;
}

LUA_FUNCTION_STATIC(soundscape_GetAllEntities)
{
	if (!g_pSoundscapeSystem)
		LUA->ThrowError("Failed to load g_pSoundscapeSystem!");

	int nCount = g_pSoundscapeSystem->m_soundscapeEntities.Count();
	LUA->PreCreateTable(nCount, 0);

	int nLuaIndex = 0;
	for(int entityIndex = 0; entityIndex < nCount; ++entityIndex)
	{
		Util::Push_Entity(LUA, g_pSoundscapeSystem->m_soundscapeEntities[entityIndex]);
		Util::RawSetI(LUA, -2, ++nLuaIndex);
	}

	return 1;
}

static bool g_bCallUpdateHook = false;
static int nLastUpdateTick = -1;
static CBitVec<MAX_PLAYERS> pHandledPlayers;
static ss_update_t* pCurrentUpdate = nullptr;
static Detouring::Hook detour_CEnvSoundscape_UpdateForPlayer;
static void hook_CEnvSoundscape_UpdateForPlayer(CBaseEntity* pSoundScape, ss_update_t& update)
{
	int nTick = gpGlobals->tickcount;
	if (nTick != nLastUpdateTick)
		pHandledPlayers.ClearAll();

	int iClient = update.pPlayer->edict()->m_EdictIndex-1;
	
	// Player got handled by Lua already, so we can skip
	if (pHandledPlayers.IsBitSet(iClient))
		return;

	// Can update.pPlayer even be NULL? Probably no
	if (g_bCallUpdateHook && update.pPlayer && Lua::PushHook("HolyLib:OnSoundScapeUpdateForPlayer"))
	{
		Util::Push_Entity(g_Lua, pSoundScape);
		Util::Push_Entity(g_Lua, update.pPlayer);

		pCurrentUpdate = &update;
		if (g_Lua->CallFunctionProtected(3, 1, true))
		{
			bool bCancel = g_Lua->GetBool(-1);
			g_Lua->Pop(1);

			if (bCancel)
			{
				audioparams_t* pParams = GetAudioParams(update.pPlayer);
				if (pParams && func_CEnvSoundscape_WriteAudioParamsTo)
					func_CEnvSoundscape_WriteAudioParamsTo(update.pCurrentSoundscape, *pParams);

				// We canceled, so we now, we don't want anything to mess with the player further, so we'll block further actions for this tick.
				pHandledPlayers.Set(iClient);
				pCurrentUpdate = nullptr;
				return;
			}
		}
		pCurrentUpdate = nullptr;
	}

	detour_CEnvSoundscape_UpdateForPlayer.GetTrampoline<Symbols::CEnvSoundscape_UpdateForPlayer>()(pSoundScape, update);
}

LUA_FUNCTION_STATIC(soundscape_SetCurrentDistance)
{
	if (!pCurrentUpdate)
		LUA->ThrowError("Tried to use this function outside of a Soundscape hook call!");

	// If one lets the Hook continue/doesn't cancel from Lua, then this can influence which other soundscape might get picked
	pCurrentUpdate->currentDistance = LUA->CheckNumber(1);
	pCurrentUpdate->bInRange = LUA->GetBool(2);
	return 0;
}

// This basically is the only one realisticly useful
// SetCurrentDistance & SetCurrentPlayerPosition are just to influence the soundscape selection
LUA_FUNCTION_STATIC(soundscape_SetCurrentSoundscape)
{
	if (!pCurrentUpdate)
		LUA->ThrowError("Tried to use this function outside of a Soundscape hook call!");

	CBaseEntity* pEntity = Util::Get_Entity(LUA, 1, true);
	if (V_stricmp(pEntity->GetClassname(), "env_soundscape") != 0)
		LUA->ThrowError("Tried to give a entity which wasn't a env_soundscape!");

	pCurrentUpdate->pCurrentSoundscape = (CEnvSoundscape*)pEntity;
	return 0;
}

LUA_FUNCTION_STATIC(soundscape_SetCurrentPlayerPosition)
{
	if (!pCurrentUpdate)
		LUA->ThrowError("Tried to use this function outside of a Soundscape hook call!");

	pCurrentUpdate->playerPosition = *Get_Vector(LUA, 1, true);
	return 0;
}

LUA_FUNCTION_STATIC(soundscape_EnableUpdateHook)
{
	g_bCallUpdateHook = LUA->GetBool(1);

	return 0;
}

void CSoundscapeModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
}

void CSoundscapeModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::FactoryLoader server_loader("server");
	Detour::Create(
		&detour_CEnvSoundscape_UpdateForPlayer, "CEnvSoundscape::UpdateForPlayer",
		server_loader.GetModule(), Symbols::CEnvSoundscape_UpdateForPlayerSym,
		(void*)hook_CEnvSoundscape_UpdateForPlayer, m_pID
	);

	g_pSoundscapeSystem = Detour::ResolveSymbol<CSoundscapeSystem>(server_loader, Symbols::g_SoundscapeSystemSym);
	Detour::CheckValue("get class", "g_SoundscapeSystem", g_pSoundscapeSystem != nullptr);

	func_CEnvSoundscape_WriteAudioParamsTo = (Symbols::CEnvSoundscape_WriteAudioParamsTo)Detour::GetFunction(server_loader.GetModule(), Symbols::CEnvSoundscape_WriteAudioParamsToSym);
	Detour::CheckFunction((void*)func_CEnvSoundscape_WriteAudioParamsTo, "CEnvSoundscape::WriteAudioParamsTo");
}

void CSoundscapeModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (pLua == g_Lua)
	{
		g_bCallUpdateHook = false; // Resetting it on changelevel & such
	}

	Util::StartTable(pLua);
		Util::AddFunc(pLua, soundscape_GetActiveSoundScape, "GetActiveSoundScape");
		Util::AddFunc(pLua, soundscape_GetActiveSoundScapeIndex, "GetActiveSoundScapeIndex");
		Util::AddFunc(pLua, soundscape_GetActivePositions, "GetActivePositions");
		Util::AddFunc(pLua, soundscape_SetActiveSoundscape, "SetActiveSoundscape");
		Util::AddFunc(pLua, soundscape_BlockEngineChanges, "BlockEngineChanges");

		Util::AddFunc(pLua, soundscape_GetAll, "GetAll");
		Util::AddFunc(pLua, soundscape_GetNameByIndex, "GetNameByIndex");
		Util::AddFunc(pLua, soundscape_GetIndexByName, "GetIndexByName");
		Util::AddFunc(pLua, soundscape_GetAllEntities, "GetAllEntities");

		Util::AddFunc(pLua, soundscape_SetCurrentDistance, "SetCurrentDistance");
		Util::AddFunc(pLua, soundscape_SetCurrentSoundscape, "SetCurrentSoundscape");
		Util::AddFunc(pLua, soundscape_SetCurrentPlayerPosition, "SetCurrentPlayerPosition");

		Util::AddFunc(pLua, soundscape_EnableUpdateHook, "EnableUpdateHook");
	Util::FinishTable(pLua, "soundscape");
}

void CSoundscapeModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "soundscape");
}

void CSoundscapeModule::Shutdown()
{
}