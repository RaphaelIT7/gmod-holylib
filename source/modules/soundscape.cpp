#include "module.h"
#include "symbols.h"
#include "detours.h"
#include "LuaInterface.h"
#include "lua.h"

class CSoundscapeModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
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

void CSoundscapeModule::LuaInit(bool bServerInit)
{

}

void CSoundscapeModule::LuaShutdown()
{
	Util::NukeTable("httpserver");
}

void CSoundscapeModule::Shutdown()
{
}