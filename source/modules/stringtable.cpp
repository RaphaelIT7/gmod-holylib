#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/InterfacePointers.hpp>
#include "detours.h"
#include "util.h"
#include "lua.h"
#include <vstdlib/jobthread.h>

class CStringTableModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* fn);
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour(bool bPreServer);
	virtual void Think(bool simulating);
	virtual void Shutdown();
	virtual const char* Name() { return "stringtable"; };
	virtual void LoadConfig(KeyValues* config) {};
};

CStringTableModule g_pStringTableFixModule;
IModule* pStringTableModule = &g_pStringTableFixModule;

void CStringTableModule::Init(CreateInterfaceFn* fn)
{
}

void CStringTableModule::LuaInit(bool bServerInit)
{
}

void CStringTableModule::LuaShutdown()
{
}

void CStringTableModule::InitDetour(bool bPreServer)
{
}

void CStringTableModule::Think(bool bSimulating)
{
}

void CStringTableModule::Shutdown()
{
}