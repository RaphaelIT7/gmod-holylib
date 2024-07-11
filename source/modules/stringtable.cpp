#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "networkstringtabledefs.h"
#include "detours.h"
#include "util.h"
#include "lua.h"

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

INetworkStringTableContainer* networkStringTableContainerServer = NULL;
void CStringTableModule::Init(CreateInterfaceFn* fn)
{
	networkStringTableContainerServer = (INetworkStringTableContainer*)fn[0](INTERFACENAME_NETWORKSTRINGTABLESERVER, NULL);
	Detour::CheckValue("get interface", "networkStringTableContainerServer", networkStringTableContainerServer != NULL);
}

LUA_FUNCTION_STATIC(stringtable_GetSize)
{
	const char* name = LUA->CheckString(1);

	INetworkStringTable* table = networkStringTableContainerServer->FindTable(name);
	if ( table )
	{
		LUA->PushNumber( table->GetNumStrings() );
		return 1;
	}

	return 0;
}

void CStringTableModule::LuaInit(bool bServerInit) // ToDo: Implement a INetworkStringTable class, a full table and call a hook when SV_CreateNetworkStringTables -> CreateNetworkStringTables is called.
{
	if ( !networkStringTableContainerServer )
		return;

	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		Util::StartTable();
			Util::AddFunc(stringtable_GetSize, "GetSize");
		Util::FinishTable("stringtable");
	g_Lua->Pop(1);
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