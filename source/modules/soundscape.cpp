#include "module.h"
#include "symbols.h"
#include "detours.h"
#include "LuaInterface.h"
#include "lua.h"
#include "player.h"

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
	virtual bool IsEnabledByDefault() { return false; };
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

	return (audioparams_t*)pAudio;
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

}

void CSoundscapeModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "soundscape");
}

void CSoundscapeModule::Shutdown()
{
}