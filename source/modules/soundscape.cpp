#include "module.h"
#include "symbols.h"
#include "detours.h"
#include "LuaInterface.h"
#include "lua.h"

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