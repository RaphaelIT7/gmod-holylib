#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"
#include "Bootil/Bootil.h"

class CVoiceChatModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual const char* Name() { return "voicechat"; };
	virtual int Compatibility() { return LINUX32; };
};

static CVoiceChatModule g_pVoiceChatModule;
IModule* pVoiceChatModule = &g_pVoiceChatModule;

void CVoiceChatModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
}

void CVoiceChatModule::LuaInit(bool bServerInit)
{
	if (bServerInit) { return; }

	if (Util::PushTable("voicechat"))
	{
	}
	Util::PopTable();
}

void CVoiceChatModule::LuaShutdown()
{
	Util::NukeTable("voicechat");
}

void CVoiceChatModule::Shutdown()
{

}

void CVoiceChatModule::InitDetour(bool bPreServer)
{

}