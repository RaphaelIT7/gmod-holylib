#include "module.h"
#include "symbols.h"
#include "detours.h"
#include "LuaInterface.h"
#include "lua.h"
#include "player.h"
#include "eiface.h"
#include "public/iholyutil.h"
extern IHolyUtil* g_pHolyUtil;

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

LUA_FUNCTION_STATIC(soundscape_StopSoundscape)
{
    CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true);
    if (!pPlayer)
        LUA->ThrowError("Invalid player!");

    IVEngineServer* pEngine = g_pHolyUtil->GetEngineServer();
    if (!pEngine)
        LUA->ThrowError("Failed to get IVEngineServer from HolyLib!");

    edict_t* pEdict = pPlayer->edict();
    if (!pEdict)
        LUA->ThrowError("Invalid player edict!");

    pEngine->ClientCommand(pEdict, "playsoundscape nothing\n");

    return 0;
}

void CSoundscapeModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
}

void CSoundscapeModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

}

void CSoundscapeModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	Util::StartTable(pLua);
		Util::AddFunc(pLua, soundscape_GetActiveSoundScape, "GetActiveSoundScape");
		Util::AddFunc(pLua, soundscape_GetActiveSoundScapeIndex, "GetActiveSoundScapeIndex");
		Util::AddFunc(pLua, soundscape_GetActivePositions, "GetActivePositions");
		Util::AddFunc(pLua, soundscape_StopSoundscape, "StopSoundscape");
	Util::FinishTable(pLua, "soundscape");
}

void CSoundscapeModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "soundscape");
}

void CSoundscapeModule::Shutdown()
{
}