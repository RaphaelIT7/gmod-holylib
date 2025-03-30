#include "module.h"
#include "LuaInterface.h"
#include "lua.h"

class CTemplateModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual void Think(bool bSimulating) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual const char* Name() { return "template"; };
	virtual int Compatibility() { return LINUX32; };
};

CTemplateModule g_pTemplateModule;
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