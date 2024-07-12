#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/InterfacePointers.hpp>
#include "detours.h"
#include "util.h"
#include "lua.h"

class CPVSModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* fn);
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour(bool bPreServer);
	virtual void Think(bool simulating);
	virtual void Shutdown();
	virtual const char* Name() { return "pvs"; };
	virtual void LoadConfig(KeyValues* config) {};
};

CPVSModule g_pPVSModule;
IModule* pPVSModule = &g_pPVSModule;

void CPVSModule::Init(CreateInterfaceFn* fn)
{
}

void CPVSModule::LuaInit(bool bServerInit)
{
}

void CPVSModule::LuaShutdown()
{
}

void CPVSModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }
}

void CPVSModule::Think(bool bSimulating)
{
}

void CPVSModule::Shutdown()
{
	Detour::Remove(m_pID);
}