#include "module.h"
#include "util.h"
#include <GarrysMod/Lua/Interface.h>
#include "networkstringtabledefs.h"
#include "detours.h"
#include "lua.h"
#include <sourcesdk/networkstringtable.h>
#include <unordered_map>

class CStringTableModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "stringtable"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; };
};

static CStringTableModule g_pStringTableFixModule;
IModule* pStringTableModule = &g_pStringTableFixModule;

static CNetworkStringTableContainer* networkStringTableContainerServer = NULL;
void CStringTableModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	networkStringTableContainerServer = (CNetworkStringTableContainer*)appfn[0](INTERFACENAME_NETWORKSTRINGTABLESERVER, NULL);
	Detour::CheckValue("get interface", "networkStringTableContainerServer", networkStringTableContainerServer != NULL);
}

//static int registryIdx = 0;
static int INetworkStringTable_TypeID = -1;
static std::unordered_map<INetworkStringTable*, int> g_pPushedStringTables;
static void Push_INetworkStringTable(INetworkStringTable* tbl)
{
	if (!tbl)
	{
		g_Lua->PushNil();
		return;
	}

	auto it = g_pPushedStringTables.find(tbl);
	if (it != g_pPushedStringTables.end())
	{
		g_Lua->ReferencePush(it->second);
	} else {
		g_Lua->PushUserType(tbl, INetworkStringTable_TypeID);
		g_Lua->Push(-1);
		g_pPushedStringTables[tbl] = g_Lua->ReferenceCreate();
	}
}

static INetworkStringTable* Get_INetworkStringTable(int iStackPos)
{
	if (!g_Lua->IsType(iStackPos, INetworkStringTable_TypeID))
		return NULL;

	return g_Lua->GetUserType<INetworkStringTable>(iStackPos, INetworkStringTable_TypeID);
}

static Detouring::Hook detour_CNetworkStringTable_Deconstructor;
static void hook_CNetworkStringTable_Deconstructor(INetworkStringTable* tbl)
{
	auto it = g_pPushedStringTables.find(tbl);
	if (it != g_pPushedStringTables.end())
	{
		g_Lua->ReferencePush(it->second);
		g_Lua->SetUserType(-1, NULL);
		g_Lua->Pop(1);
		g_Lua->ReferenceFree(it->second);
		g_pPushedStringTables.erase(it);
	}
}

LUA_FUNCTION_STATIC(INetworkStringTable__tostring)
{
	INetworkStringTable* table = Get_INetworkStringTable(1);
	if (!table)
	{
		LUA->PushString("INetworkStringTable [NULL]");
		return 1;
	}

	char szBuf[128] = {};
	V_snprintf(szBuf, sizeof(szBuf),"INetworkStringTable [%s]", table->GetTableName()); 
	LUA->PushString(szBuf);
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable__index)
{
	if (!g_Lua->FindOnObjectsMetaTable(1, 2))
		LUA->PushNil();

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

static Symbols::CNetworkStringTable_DeleteAllStrings func_CNetworkStringTable_DeleteAllStrings;
LUA_FUNCTION_STATIC(INetworkStringTable_DeleteAllStrings)
{
	INetworkStringTable* table = Get_INetworkStringTable(1);
	if (!table)
		LUA->ArgError(1, "INetworkStringTable");

	if (!func_CNetworkStringTable_DeleteAllStrings)
		LUA->ThrowError("Failed to get CNetworkStringTable::DeleteAllStrings");

	func_CNetworkStringTable_DeleteAllStrings(table);

	return 0;
}

LUA_FUNCTION_STATIC(INetworkStringTable_SetMaxEntries)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(1);
	if (!table)
		LUA->ArgError(1, "INetworkStringTable");

	int maxEntries = LUA->CheckNumber(2);
	int maxEntryBits = Q_log2(maxEntries);
	if ((1 << maxEntryBits) != maxEntries)
		LUA->ThrowError("String tables must be powers of two in size!");

	table->m_nMaxEntries = maxEntries;
	table->m_nEntryBits = maxEntryBits;

	return 0;
}

LUA_FUNCTION_STATIC(INetworkStringTable_DeleteString)
{
	INetworkStringTable* table = Get_INetworkStringTable(1);
	if (!table)
		LUA->ArgError(1, "INetworkStringTable");

	int strIndex = LUA->CheckNumber(2);
	if (!table->GetString(strIndex))
	{
		LUA->PushBool(false);
		return 1;
	}

	std::vector<std::string> pElements;
	for (int i=0; i<table->GetNumStrings(); ++i)
	{
		if (i == strIndex)
			continue;

		const char* str = table->GetString(i);
		if (str)
			pElements.push_back(str);
		// ToDo: Save and restore userdata. Maybe I could also try later to not use a std::string above?
	}

	func_CNetworkStringTable_DeleteAllStrings(table); // If this deletes the UserData that we got, were gonna have a problem.

	for (std::string key : pElements)
	{
		table->AddString(true, key.c_str());
	}

	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_IsValid)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(1);
	
	LUA->PushBool(table != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_SetStringUserData)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(1);
	int idx = LUA->CheckNumber(2);
	const char* pUserData = LUA->CheckString(3);

	table->SetStringUserData(idx, LUA->ObjLen(3), pUserData);
	return 0;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetStringUserData)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(1);
	int idx = LUA->CheckNumber(2);

	LUA->PushString((const char*)table->GetStringUserData(idx, 0));
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
	if (!table)
		LUA->ArgError(1, "INetworkStringTable");

	bool bAllowClientSideAddString = LUA->GetBool(2);

	networkStringTableContainerServer->SetAllowClientSideAddString(table, bAllowClientSideAddString);

	return 0;
}

LUA_FUNCTION_STATIC(stringtable_IsCreationAllowed)
{
	LUA->PushBool(networkStringTableContainerServer->m_bAllowCreation);

	return 1;
}

LUA_FUNCTION_STATIC(stringtable_IsLocked)
{
	LUA->PushBool(networkStringTableContainerServer->m_bLocked);

	return 1;
}

LUA_FUNCTION_STATIC(stringtable_AllowCreation)
{
	networkStringTableContainerServer->m_bAllowCreation = LUA->GetBool(1);

	return 0;
}

LUA_FUNCTION_STATIC(stringtable_RemoveTable)
{
	INetworkStringTable* table = Get_INetworkStringTable(1);
	if (!table)
		LUA->ArgError(1, "INetworkStringTable");

	networkStringTableContainerServer->m_Tables.Remove(table->GetTableId());
	delete table;

	return 0;
}

void CStringTableModule::LuaInit(bool bServerInit) // ToDo: Implement a INetworkStringTable class, a full table and call a hook when SV_CreateNetworkStringTables -> CreateNetworkStringTables is called.
{
	if (!networkStringTableContainerServer)
		return;

	if (!bServerInit)
	{
		/*g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_REG);
			g_Lua->CreateTable();
			g_Lua->SetField(-2, "inetworkstringtable_objects");
		g_Lua->Pop(1);*/

		INetworkStringTable_TypeID = g_Lua->CreateMetaTable("INetworkStringTable");
			Util::AddFunc(INetworkStringTable__tostring, "__tostring");
			Util::AddFunc(INetworkStringTable__index, "__index");
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
			Util::AddFunc(INetworkStringTable_DeleteAllStrings, "DeleteAllStrings");
			Util::AddFunc(INetworkStringTable_SetMaxEntries, "SetMaxEntries");
			Util::AddFunc(INetworkStringTable_DeleteString, "DeleteString");
			Util::AddFunc(INetworkStringTable_IsValid, "IsValid");
			Util::AddFunc(INetworkStringTable_SetStringUserData, "SetStringUserData");
			Util::AddFunc(INetworkStringTable_GetStringUserData, "GetStringUserData");
		g_Lua->Pop(1);

		if (g_Lua->PushMetaTable(INetworkStringTable_TypeID))
		{
			g_Lua->Pop(1);
		} else {
			Warning("HolyLib: g_Lua->PushMetaTable fails to push INetworkStringTable!\n");
		}

		Util::StartTable();
			Util::AddFunc(stringtable_CreateStringTable, "CreateStringTable");
			Util::AddFunc(stringtable_RemoveAllTables, "RemoveAllTables"); // ToDo: Invalidate all pushed stringtables.
			Util::AddFunc(stringtable_FindTable, "FindTable");
			Util::AddFunc(stringtable_GetTable, "GetTable");
			Util::AddFunc(stringtable_GetNumTables, "GetNumTables");
			Util::AddFunc(stringtable_CreateStringTableEx, "CreateStringTableEx");
			Util::AddFunc(stringtable_SetAllowClientSideAddString, "SetAllowClientSideAddString");
			Util::AddFunc(stringtable_IsCreationAllowed, "IsCreationAllowed");
			Util::AddFunc(stringtable_IsLocked, "IsLocked");
			Util::AddFunc(stringtable_AllowCreation, "AllowCreation");
			Util::AddFunc(stringtable_RemoveTable, "RemoveTable");

			g_Lua->PushNumber(INVALID_STRING_INDEX);
			g_Lua->SetField(-2, "INVALID_STRING_INDEX");
		Util::FinishTable("stringtable");
	} else {
		if (Lua::PushHook("HolyLib:OnStringTableCreation")) // Use this hook to create / modify the stringtables.
		{
			networkStringTableContainerServer->m_bAllowCreation = true; // Will this work? We'll see.
			g_Lua->CallFunctionProtected(1, 0, true);
			networkStringTableContainerServer->m_bAllowCreation = false;
		}
	}
}

void CStringTableModule::LuaShutdown() // ToDo: Can we remove the metatable?
{
	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		g_Lua->PushNil();
		g_Lua->SetField(-2, "stringtable");
	g_Lua->Pop(1);
	g_pPushedStringTables.clear();
}

void CStringTableModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }

	SourceSDK::ModuleLoader engine_loader("engine");
	Detour::Create(
		&detour_CNetworkStringTable_Deconstructor, "CNetworkStringTable::~CNetworkStringTable",
		engine_loader.GetModule(), Symbols::CNetworkStringTable_DeconstructorSym,
		(void*)hook_CNetworkStringTable_Deconstructor, m_pID
	);

	func_CNetworkStringTable_DeleteAllStrings = (Symbols::CNetworkStringTable_DeleteAllStrings)Detour::GetFunction(engine_loader.GetModule(), Symbols::CNetworkStringTable_DeleteAllStringsSym);
	Detour::CheckFunction((void*)func_CNetworkStringTable_DeleteAllStrings, "CNetworkStringTable::DeleteAllStrings");
}