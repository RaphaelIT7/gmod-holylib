#include "module.h"
#include "LuaInterface.h"
#include "lua.h"

class CTemplateModule : public IModule
{
public:
	void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) override;
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void InitDetour(bool bPreServer) override;
	void Think(bool bSimulating) override;
	void Shutdown() override;
	const char* Name() override { return "template"; };
	int Compatibility() override { return LINUX32; };
};

static CTemplateModule g_pTemplateModule;
IModule* pTemplateModule = &g_pTemplateModule;

void CTemplateModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
}

void CTemplateModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;
}

void CTemplateModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
}

void CTemplateModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

}

void CTemplateModule::Think(bool simulating)
{
}

void CTemplateModule::Shutdown()
{
}