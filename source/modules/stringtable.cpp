#include <GarrysMod/Lua/Interface.h>
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "networkstringtabledefs.h"
#include <sourcesdk/networkstringtable.h>
#include <sourcesdk/server.h>
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
PushReferenced_LuaClass(INetworkStringTable, INetworkStringTable_TypeID)
Get_LuaClass(INetworkStringTable, INetworkStringTable_TypeID, "INetworkStringTable")

static Detouring::Hook detour_CNetworkStringTable_Deconstructor;
static void hook_CNetworkStringTable_Deconstructor(INetworkStringTable* tbl)
{
	auto it = g_pPushedINetworkStringTable.find(tbl);
	if (it != g_pPushedINetworkStringTable.end())
	{
		g_Lua->ReferencePush(it->second);
		g_Lua->SetUserType(-1, NULL);
		g_Lua->Pop(1);
		g_Lua->ReferenceFree(it->second);
		g_pPushedINetworkStringTable.erase(it);
	}
}

LUA_FUNCTION_STATIC(INetworkStringTable__tostring)
{
	INetworkStringTable* table = Get_INetworkStringTable(1, false);
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
	INetworkStringTable* table = Get_INetworkStringTable(1, true);

	LUA->PushString(table->GetTableName());
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetTableId)
{
	INetworkStringTable* table = Get_INetworkStringTable(1, true);

	LUA->PushNumber(table->GetTableId());
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetNumStrings)
{
	INetworkStringTable* table = Get_INetworkStringTable(1, true);

	LUA->PushNumber(table->GetNumStrings());
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetMaxStrings)
{
	INetworkStringTable* table = Get_INetworkStringTable(1, true);

	LUA->PushNumber(table->GetMaxStrings());
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetEntryBits)
{
	INetworkStringTable* table = Get_INetworkStringTable(1, true);

	LUA->PushNumber(table->GetEntryBits());
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_SetTick)
{
	INetworkStringTable* table = Get_INetworkStringTable(1, true);

	int tick = (int)LUA->CheckNumber(2);

	table->SetTick(tick);
	return 0;
}

LUA_FUNCTION_STATIC(INetworkStringTable_ChangedSinceTick)
{
	INetworkStringTable* table = Get_INetworkStringTable(1, true);

	int tick = (int)LUA->CheckNumber(2);
	LUA->PushBool(table->ChangedSinceTick(tick));
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_AddString)
{
	INetworkStringTable* table = Get_INetworkStringTable(1, true);

	bool bIsServer = LUA->GetBool(2);
	const char* pStr = LUA->CheckString(3);
	//int length = g_Lua->CheckNumberOpt(4, -1);

	LUA->PushNumber(table->AddString(bIsServer, pStr));
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetString)
{
	INetworkStringTable* table = Get_INetworkStringTable(1, true);

	int idx = (int)LUA->CheckNumber(2);

	LUA->PushString(table->GetString(idx));
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetAllStrings)
{
	INetworkStringTable* table = Get_INetworkStringTable(1, true);

	int idx = 0;
	g_Lua->PreCreateTable(table->GetMaxStrings(), 0);
	for (int i = 0; i < table->GetMaxStrings(); ++i)
	{
		const char* pStr = table->GetString(i);
		if (!pStr)
			continue;

		++idx;
		LUA->PushNumber(idx);
		LUA->PushString(pStr);
		LUA->RawSet(-3);
	}

	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_FindStringIndex)
{
	INetworkStringTable* table = Get_INetworkStringTable(1, true);

	const char* pStr = LUA->CheckString(2);

	LUA->PushNumber(table->FindStringIndex(pStr));
	return 1;
}

static Symbols::CNetworkStringTable_DeleteAllStrings func_CNetworkStringTable_DeleteAllStrings;
LUA_FUNCTION_STATIC(INetworkStringTable_DeleteAllStrings)
{
	INetworkStringTable* table = Get_INetworkStringTable(1, true);
	bool bNukePrecache = LUA->GetBool(2);

	if (!func_CNetworkStringTable_DeleteAllStrings)
		LUA->ThrowError("Failed to get CNetworkStringTable::DeleteAllStrings");

	func_CNetworkStringTable_DeleteAllStrings(table);

	if (!bNukePrecache)
		return 0;

	if (!Q_stricmp(MODEL_PRECACHE_TABLENAME, table->GetTableName()))
	{
		CGameServer* pServer = (CGameServer*)Util::server;
		for (int i = 0; i < MAX_MODELS; ++i)
		{
			CPrecacheItem item = pServer->model_precache[i];
			if (item.GetModel())
				item.SetModel(NULL);
		}
	} else if (!Q_stricmp(SOUND_PRECACHE_TABLENAME, table->GetTableName()))
	{
		CGameServer* pServer = (CGameServer*)Util::server;
		for (int i = 0; i < MAX_SOUNDS; ++i)
		{
			CPrecacheItem item = pServer->sound_precache[i];
			if (item.GetSound())
				item.SetSound(NULL);
		}
	} else if (!Q_stricmp(DECAL_PRECACHE_TABLENAME, table->GetTableName()))
	{
		CGameServer* pServer = (CGameServer*)Util::server;
		for (int i = 0; i < MAX_BASE_DECALS; ++i)
		{
			CPrecacheItem item = pServer->decal_precache[i];
			if (item.GetDecal())
				item.SetDecal(NULL);
		}
	} else if (!Q_stricmp(GENERIC_PRECACHE_TABLENAME, table->GetTableName()))
	{
		CGameServer* pServer = (CGameServer*)Util::server;
		for (int i = 0; i < MAX_GENERIC; ++i)
		{
			CPrecacheItem item = pServer->generic_precache[i];
			if (item.GetGeneric())
				item.SetGeneric(NULL);
		}
	}

	return 0;
}

LUA_FUNCTION_STATIC(INetworkStringTable_SetMaxEntries)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(1, true);

	int maxEntries = (int)LUA->CheckNumber(2);
	int maxEntryBits = Q_log2(maxEntries);
	if ((1 << maxEntryBits) != maxEntries)
		LUA->ThrowError("String tables must be powers of two in size!");

	table->m_nMaxEntries = maxEntries;
	table->m_nEntryBits = maxEntryBits;

	return 0;
}

struct StringTableEntry
{
	char* pName = NULL;
	void* pUserData = NULL;
	int iUserDataLength = 0;
};

LUA_FUNCTION_STATIC(INetworkStringTable_DeleteString)
{
	INetworkStringTable* table = Get_INetworkStringTable(1, true);

	int strIndex = (int)LUA->CheckNumber(2);
	if (!table->GetString(strIndex))
	{
		LUA->PushBool(false);
		return 1;
	}

	if (!Q_stricmp(MODEL_PRECACHE_TABLENAME, table->GetTableName()))
	{
		CGameServer* pServer = (CGameServer*)Util::server;
		CPrecacheItem item = pServer->model_precache[strIndex];
		if (item.GetModel())
			item.SetModel(NULL);
	} else if (!Q_stricmp(SOUND_PRECACHE_TABLENAME, table->GetTableName()))
	{
		CGameServer* pServer = (CGameServer*)Util::server;
		CPrecacheItem item = pServer->sound_precache[strIndex];
		if (item.GetSound())
			item.SetSound(NULL);
	} else if (!Q_stricmp(DECAL_PRECACHE_TABLENAME, table->GetTableName()))
	{
		CGameServer* pServer = (CGameServer*)Util::server;
		CPrecacheItem item = pServer->decal_precache[strIndex];
		if (item.GetDecal())
			item.SetDecal(NULL);
	} else if (!Q_stricmp(GENERIC_PRECACHE_TABLENAME, table->GetTableName()))
	{
		CGameServer* pServer = (CGameServer*)Util::server;
		CPrecacheItem item = pServer->generic_precache[strIndex];
		if (item.GetGeneric())
			item.SetGeneric(NULL);
	}

	std::vector<StringTableEntry*> pElements;
	for (int i=0; i<table->GetNumStrings(); ++i)
	{
		if (i == strIndex)
			continue;

		const char* str = table->GetString(i);
		if (!str)
			continue;

		StringTableEntry* pEntry = new StringTableEntry;
		pEntry->pName = new char[strlen(str) + 1];
		strcpy(pEntry->pName, str);

		pEntry->pUserData = (void*)table->GetStringUserData(i, &pEntry->iUserDataLength);

		pElements.push_back(pEntry);
	}

	func_CNetworkStringTable_DeleteAllStrings(table); // If this deletes the UserData that we got, were gonna have a problem.

	for (StringTableEntry* pEntry : pElements)
	{
		table->AddString(true, pEntry->pName, pEntry->iUserDataLength, pEntry->pUserData);

		delete[] pEntry->pName; // Apparently the stringtable itself will make a copy of it and manage it? So Yeet our string.
		delete pEntry;
	}
	pElements.clear();

	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_IsValid)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(1, false);
	
	LUA->PushBool(table != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_SetStringUserData)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(1, true);
	int idx = (int)LUA->CheckNumber(2);
	const char* pUserData = LUA->GetString(3);
	int iLength = (int)g_Lua->CheckNumberOpt(4, NULL);

	if (idx >= table->GetNumStrings())
		return 0;

	if (!pUserData)
	{
		table->SetStringUserData(idx, 0, NULL);
		return 0;
	}

	table->SetStringUserData(idx, iLength ? iLength : LUA->ObjLen(3), pUserData);
	return 0;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetStringUserData)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(1, true);
	int idx = (int)LUA->CheckNumber(2);

	int iLength = 0;
	LUA->PushString((const char*)table->GetStringUserData(idx, &iLength));
	LUA->PushNumber(iLength);
	return 2;
}

LUA_FUNCTION_STATIC(INetworkStringTable_SetNumberUserData)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(1, true);
	int idx = (int)LUA->CheckNumber(2);
	int pUserData = (int)LUA->CheckNumber(3);

	if (idx >= table->GetNumStrings())
		return 0;

	table->SetStringUserData(idx, sizeof(int), &pUserData);
	return 0;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetNumberUserData)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(1, true);
	int idx = (int)LUA->CheckNumber(2);

	int dataLen; 
	const void *pData = table->GetStringUserData(idx, &dataLen);
	if (pData && dataLen == sizeof(int))
		LUA->PushNumber(*((const int*)table->GetStringUserData(idx, 0)));
	else
		LUA->PushNil();

	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_SetPrecacheUserData)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(1, true);
	int idx = (int)LUA->CheckNumber(2);
	int pFlags = (int)LUA->CheckNumber(3);

	if (idx >= table->GetNumStrings())
		return 0;

	CPrecacheUserData p;
	p.flags = pFlags;

	table->SetStringUserData(idx, sizeof(p), &p);
	return 0;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetPrecacheUserData)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(1, true);
	int idx = (int)LUA->CheckNumber(2);

	int dataLen; 
	const void *pData = table->GetStringUserData(idx, &dataLen);
	if (pData && dataLen == sizeof(CPrecacheUserData))
		LUA->PushNumber(((const CPrecacheUserData*)table->GetStringUserData(idx, 0))->flags);
	else
		LUA->PushNil();

	return 1;
}

LUA_FUNCTION_STATIC(stringtable_CreateStringTable)
{
	const char* name = LUA->CheckString(1);
	int maxentries = (int)LUA->CheckNumber(2);
	int userdatafixedsize = (int)g_Lua->CheckNumberOpt(3, 0);
	int userdatanetworkbits = (int)g_Lua->CheckNumberOpt(4, 0);

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
	if (table)
	{
		Push_INetworkStringTable(table);
		return 1;
	}

	return 0;
}

LUA_FUNCTION_STATIC(stringtable_GetTable)
{
	int stringTable = (int)LUA->CheckNumber(1);

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
	int maxentries = (int)LUA->CheckNumber(2);
	int userdatafixedsize = (int)g_Lua->CheckNumberOpt(3, 0);
	int userdatanetworkbits = (int)g_Lua->CheckNumberOpt(4, 0);
	bool bIsFilenames = g_Lua->GetBool(5);

	Push_INetworkStringTable(networkStringTableContainerServer->CreateStringTableEx(name, maxentries, userdatafixedsize, userdatanetworkbits, bIsFilenames));
	return 1;
}

LUA_FUNCTION_STATIC(stringtable_SetAllowClientSideAddString)
{
	INetworkStringTable* table = Get_INetworkStringTable(1, true);
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
	INetworkStringTable* table = Get_INetworkStringTable(1, true);

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
			Util::AddFunc(INetworkStringTable_GetAllStrings, "GetAllStrings");
			Util::AddFunc(INetworkStringTable_FindStringIndex, "FindStringIndex");
			Util::AddFunc(INetworkStringTable_DeleteAllStrings, "DeleteAllStrings");
			Util::AddFunc(INetworkStringTable_SetMaxEntries, "SetMaxEntries");
			Util::AddFunc(INetworkStringTable_DeleteString, "DeleteString");
			Util::AddFunc(INetworkStringTable_IsValid, "IsValid");
			Util::AddFunc(INetworkStringTable_SetStringUserData, "SetStringUserData");
			Util::AddFunc(INetworkStringTable_GetStringUserData, "GetStringUserData");
			Util::AddFunc(INetworkStringTable_SetNumberUserData, "SetNumberUserData");
			Util::AddFunc(INetworkStringTable_GetNumberUserData, "GetNumberUserData");
			Util::AddFunc(INetworkStringTable_SetPrecacheUserData, "SetPrecacheUserData");
			Util::AddFunc(INetworkStringTable_GetPrecacheUserData, "GetPrecacheUserData");
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
	Util::NukeTable("stringtable");
	g_pPushedINetworkStringTable.clear();
}

void CStringTableModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::ModuleLoader engine_loader("engine");
	Detour::Create(
		&detour_CNetworkStringTable_Deconstructor, "CNetworkStringTable::~CNetworkStringTable",
		engine_loader.GetModule(), Symbols::CNetworkStringTable_DeconstructorSym,
		(void*)hook_CNetworkStringTable_Deconstructor, m_pID
	);

	func_CNetworkStringTable_DeleteAllStrings = (Symbols::CNetworkStringTable_DeleteAllStrings)Detour::GetFunction(engine_loader.GetModule(), Symbols::CNetworkStringTable_DeleteAllStringsSym);
	Detour::CheckFunction((void*)func_CNetworkStringTable_DeleteAllStrings, "CNetworkStringTable::DeleteAllStrings");
}