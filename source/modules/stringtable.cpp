#include "module.h"
#include "util.h"
#include <GarrysMod/Lua/Interface.h>
#include "networkstringtabledefs.h"
#include "detours.h"
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

GarrysMod::Lua::ILuaObject* metatable;
#define INetworkStringTable_Type 100 // 100 shouldn't be used right? Also we got ILuaInterface::RegisterMetaTable in the next update :D
void Push_INetworkStringTable(INetworkStringTable* tbl)
{
	if ( !tbl )
	{
		g_Lua->PushNil();
		return;
	}

	g_Lua->PushUserType( tbl, INetworkStringTable_Type );
	metatable->Push();
	g_Lua->SetMetaTable(-2);
}

INetworkStringTable* Get_INetworkStringTable(int iStackPos)
{
	if (!g_Lua->IsType(iStackPos, INetworkStringTable_Type))
		return NULL;

	return g_Lua->GetUserType<INetworkStringTable>(iStackPos, INetworkStringTable_Type);
}

LUA_FUNCTION_STATIC(INetworkStringTable__tostring)
{
	INetworkStringTable* table = Get_INetworkStringTable(1);
	if (!table)
		LUA->ArgError(1, "INetworkStringTable");

	char szBuf[128] = {};
	V_snprintf(szBuf, sizeof(szBuf),"INetworkStringTable [%s]", table->GetTableName()); 
	LUA->PushString(szBuf);
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetTableName)
{
	INetworkStringTable* table = Get_INetworkStringTable(1);
	if (!table)
		LUA->ArgError(1, "INetworkStringTable");

	LUA->PushString(table->GetTableName());
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetTableId)
{
	INetworkStringTable* table = Get_INetworkStringTable(1);
	if (!table)
		LUA->ArgError(1, "INetworkStringTable");

	LUA->PushNumber(table->GetTableId());
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetNumStrings)
{
	INetworkStringTable* table = Get_INetworkStringTable(1);
	if (!table)
		LUA->ArgError(1, "INetworkStringTable");

	LUA->PushNumber(table->GetNumStrings());
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetMaxStrings)
{
	INetworkStringTable* table = Get_INetworkStringTable(1);
	if (!table)
		LUA->ArgError(1, "INetworkStringTable");

	LUA->PushNumber(table->GetMaxStrings());
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetEntryBits)
{
	INetworkStringTable* table = Get_INetworkStringTable(1);
	if (!table)
		LUA->ArgError(1, "INetworkStringTable");

	LUA->PushNumber(table->GetEntryBits());
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_SetTick)
{
	INetworkStringTable* table = Get_INetworkStringTable(1);
	if (!table)
		LUA->ArgError(1, "INetworkStringTable");

	int tick = LUA->CheckNumber(2);

	table->SetTick(tick);
	return 0;
}

LUA_FUNCTION_STATIC(INetworkStringTable_ChangedSinceTick)
{
	INetworkStringTable* table = Get_INetworkStringTable(1);
	if (!table)
		LUA->ArgError(1, "INetworkStringTable");

	int tick = LUA->CheckNumber(2);
	LUA->PushBool(table->ChangedSinceTick(tick));
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_AddString)
{
	INetworkStringTable* table = Get_INetworkStringTable(1);
	if (!table)
		LUA->ArgError(1, "INetworkStringTable");

	bool bIsServer = LUA->GetBool(2);
	const char* pStr = LUA->CheckString(3);
	//int length = g_Lua->CheckNumberOpt(4, -1);

	LUA->PushNumber(table->AddString(bIsServer, pStr));
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetString)
{
	INetworkStringTable* table = Get_INetworkStringTable(1);
	if (!table)
		LUA->ArgError(1, "INetworkStringTable");

	int idx = LUA->CheckNumber(2);

	LUA->PushString(table->GetString(idx));
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_FindStringIndex)
{
	INetworkStringTable* table = Get_INetworkStringTable(1);
	if (!table)
		LUA->ArgError(1, "INetworkStringTable");

	const char* pStr = LUA->CheckString(2);

	LUA->PushNumber(table->FindStringIndex(pStr));
	return 1;
}

LUA_FUNCTION_STATIC(stringtable_CreateStringTable)
{
	const char* name = LUA->CheckString(1);
	int maxentries = LUA->CheckNumber(2);
	int userdatafixedsize = g_Lua->CheckNumberOpt(3, 0);
	int userdatanetworkbits = g_Lua->CheckNumberOpt(4, 0);

	Push_INetworkStringTable(networkStringTableContainerServer->CreateStringTable(name, maxentries, userdatafixedsize, userdatanetworkbits));
	return 1;
}

LUA_FUNCTION_STATIC(stringtable_RemoveAllTables)
{
	networkStringTableContainerServer->RemoveAllTables();

	return 0;
}

LUA_FUNCTION_STATIC(stringtable_FindTable)
{
	const char* name = LUA->CheckString(1);

	INetworkStringTable* table = networkStringTableContainerServer->FindTable(name);
	if ( table )
	{
		Push_INetworkStringTable( table );
		return 1;
	}

	return 0;
}

LUA_FUNCTION_STATIC(stringtable_GetTable)
{
	int stringTable = LUA->CheckNumber(1);

	INetworkStringTable* table = networkStringTableContainerServer->GetTable(stringTable);
	if (table)
	{
		Push_INetworkStringTable(table);
		return 1;
	}

	return 0;
}

LUA_FUNCTION_STATIC(stringtable_GetNumTables)
{
	LUA->PushNumber(networkStringTableContainerServer->GetNumTables());
	return 1;
}

LUA_FUNCTION_STATIC(stringtable_CreateStringTableEx)
{
	const char* name = LUA->CheckString(1);
	int maxentries = LUA->CheckNumber(2);
	int userdatafixedsize = g_Lua->CheckNumberOpt(3, 0);
	int userdatanetworkbits = g_Lua->CheckNumberOpt(4, 0);
	bool bIsFilenames = g_Lua->GetBool(5);

	Push_INetworkStringTable(networkStringTableContainerServer->CreateStringTableEx(name, maxentries, userdatafixedsize, userdatanetworkbits, bIsFilenames));
	return 1;
}

LUA_FUNCTION_STATIC(stringtable_SetAllowClientSideAddString)
{
	INetworkStringTable* table = Get_INetworkStringTable(1);
	bool bAllowClientSideAddString = LUA->GetBool(2);

	networkStringTableContainerServer->SetAllowClientSideAddString(table, bAllowClientSideAddString);

	return 0;
}

Detouring::Hook detour_CServerGameDLL_CreateNetworkStringTables;
void hook_CServerGameDLL_CreateNetworkStringTables(void* servergamedll)
{
	detour_CServerGameDLL_CreateNetworkStringTables.GetTrampoline<Symbols::CServerGameDLL_CreateNetworkStringTables>()(servergamedll);

	if (Lua::PushHook("HolyLib:OnStringtableCreation")) // Use this hook to create / modify the stringtables.
	{
		g_Lua->CallFunctionProtected(1, 0, true);
	}
}

void CStringTableModule::LuaInit(bool bServerInit) // ToDo: Implement a INetworkStringTable class, a full table and call a hook when SV_CreateNetworkStringTables -> CreateNetworkStringTables is called.
{
	if (!networkStringTableContainerServer)
		return;

	if ( metatable )
		g_Lua->DestroyObject(metatable);

	metatable = g_Lua->CreateObject();
	g_Lua->CreateMetaTableType("INetworkStringTable", INetworkStringTable_Type);
		Util::AddFunc(INetworkStringTable_GetTableName, "GetTableName");
		Util::AddFunc(INetworkStringTable_GetTableId, "GetTableId");
		Util::AddFunc(INetworkStringTable_GetNumStrings, "GetNumStrings");
		Util::AddFunc(INetworkStringTable_GetMaxStrings, "GetMaxStrings");
		Util::AddFunc(INetworkStringTable_GetEntryBits, "GetEntryBits");
		Util::AddFunc(INetworkStringTable_SetTick, "SetTick");
		Util::AddFunc(INetworkStringTable_ChangedSinceTick, "ChangedSinceTick");
		Util::AddFunc(INetworkStringTable_AddString, "AddString");
		Util::AddFunc(INetworkStringTable_GetString, "GetString");
		Util::AddFunc(INetworkStringTable_FindStringIndex, "FindStringIndex");
		metatable->SetFromStack(-1);
	g_Lua->Pop(1);

	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		Util::StartTable();
			Util::AddFunc(stringtable_CreateStringTable, "CreateStringTable");
			Util::AddFunc(stringtable_RemoveAllTables, "RemoveAllTables");
			Util::AddFunc(stringtable_FindTable, "FindTable");
			Util::AddFunc(stringtable_GetTable, "GetTable");
			Util::AddFunc(stringtable_GetNumTables, "GetNumTables");
			Util::AddFunc(stringtable_CreateStringTableEx, "CreateStringTableEx");
			Util::AddFunc(stringtable_SetAllowClientSideAddString, "SetAllowClientSideAddString");

			g_Lua->PushNumber(INVALID_STRING_INDEX);
			g_Lua->SetField(-2, "INVALID_STRING_INDEX");
		Util::FinishTable("stringtable");
	g_Lua->Pop(1);
}

void CStringTableModule::LuaShutdown()
{
	if ( metatable )
		g_Lua->DestroyObject(metatable);
}

void CStringTableModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }

	SourceSDK::ModuleLoader server_loader("server");
	Detour::Create(
		&detour_CServerGameDLL_CreateNetworkStringTables, "CServerGameDLL::CreateNetworkStringTables",
		server_loader.GetModule(), Symbols::CServerGameDLL_CreateNetworkStringTablesSym,
		(void*)hook_CServerGameDLL_CreateNetworkStringTables, m_pID
	);
}

void CStringTableModule::Think(bool bSimulating)
{
}

void CStringTableModule::Shutdown()
{
	Detour::Remove(m_pID);
}