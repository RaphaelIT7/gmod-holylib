#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"

class CSourceTVLibModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn);
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour(bool bPreServer);
	virtual void Think(bool bSimulating);
	virtual void Shutdown();
	virtual const char* Name() { return "sourcetv"; };
	virtual void LoadConfig(KeyValues* config);

private:
	bool m_bAllowNetworking = false;
};

CSourceTVLibModule g_pSourceTVLibModule;
IModule* pSourceTVLibModule = &g_pSourceTVLibModule;

void CSourceTVLibModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
}

void CSourceTVLibModule::LoadConfig(KeyValues* config)
{
	//if (config->IsEmpty("sourcetv_allownetworking"))
	//	config->SetBool("sourcetv_allownetworking", true);

	//m_bAllowNetworking = config->GetBool("sourcetv_allownetworking", false);
}

void CSourceTVLibModule::LuaInit(bool bServerInit)
{
	if (!bServerInit)
	{

	}
}

void CSourceTVLibModule::LuaShutdown()
{
}

void CSourceTVLibModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }

}

void CSourceTVLibModule::Think(bool simulating)
{
}

void CSourceTVLibModule::Shutdown()
{
}