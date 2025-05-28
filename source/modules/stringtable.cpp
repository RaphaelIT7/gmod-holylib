#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "networkstringtabledefs.h"
#include <sourcesdk/networkstringtable.h>
#include <sourcesdk/server.h>
#include <unordered_map>
#include "networkstringtableitem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CStringTableModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "stringtable"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
	virtual bool SupportsMultipleLuaStates() { return true; };
};

static CStringTableModule g_pStringTableFixModule;
IModule* pStringTableModule = &g_pStringTableFixModule;

#ifdef SYSTEM_WINDOWS // ISSUE: On Windows CNetworkStringTable::DeleteAllStrings doesn't exist as its inlined into CNetworkStringTable::ReadStringTable
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNetworkStringTableItem::CNetworkStringTableItem( void )
{
	m_pUserData = NULL;
	m_nUserDataLength = 0;
	m_nTickChanged = 0;

#ifndef SHARED_NET_STRING_TABLES
	m_nTickCreated = 0;
	m_pChangeList = NULL;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNetworkStringTableItem::~CNetworkStringTableItem( void )
{
#ifndef SHARED_NET_STRING_TABLES
	if ( m_pChangeList )
	{
		// free changelist and elements

		for ( int i=0; i < m_pChangeList->Count(); i++ )
		{
			itemchange_s item = m_pChangeList->Element( i );

			delete[] item.data;
		}

		delete m_pChangeList; // destructor calls Purge()

		m_pUserData = NULL;
	}
#endif
		
	delete[] m_pUserData;
}

/*
============
tmpstr512

rotates through a bunch of string buffers of 512 bytes each
============
*/
char *tmpstr512()
{
	static char	string[32][512];
	static int	curstring = 0;
	curstring = ( curstring + 1 ) & 31;
	return string[curstring];  
}

bool CNetworkStringTable_LessFunc( FileNameHandle_t const &a, FileNameHandle_t const &b )
{
	return a < b;
}

class CNetworkStringFilenameDict : public INetworkStringDict
{
public:
	CNetworkStringFilenameDict()
	{
		m_Items.SetLessFunc( CNetworkStringTable_LessFunc );
	}

	virtual ~CNetworkStringFilenameDict()
	{
		Purge();
	}

	unsigned int Count()
	{
		return m_Items.Count();
	}

	void Purge()
	{
		m_Items.Purge();
	}

	const char *String( int index )
	{
		char* pString = tmpstr512();
		g_pFullFileSystem->String( m_Items.Key( index ), pString, 512 );
		return pString;
	}

	bool IsValidIndex( int index )
	{
		return m_Items.IsValidIndex( index );
	}

	int Insert( const char *pString )
	{
		FileNameHandle_t fnHandle = g_pFullFileSystem->FindOrAddFileName( pString );
		return m_Items.Insert( fnHandle );
	}

	int Find( const char *pString )
	{
		FileNameHandle_t fnHandle = g_pFullFileSystem->FindFileName( pString );
		if ( !fnHandle )
			return m_Items.InvalidIndex();
		return m_Items.Find( fnHandle );
	}

	CNetworkStringTableItem	&Element( int index )
	{
		return m_Items.Element( index );
	}

	const CNetworkStringTableItem &Element( int index ) const
	{
		return m_Items.Element( index );
	}

private:
	CUtlMap< FileNameHandle_t, CNetworkStringTableItem > m_Items;
};

#if ARCHITECTURE_X86_64
class CNetworkStringDict : public INetworkStringDict
{
public:
	CNetworkStringDict( bool bUseDictionary ) : 
		m_bUseDictionary( bUseDictionary ), 
		m_Items( 0, 0, CTableItem::Less )
	{
	}

	virtual ~CNetworkStringDict() 
	{ 
	}

	unsigned int Count()
	{
		return -1;
	}

	void Purge()
	{
	}

	const char *String( int index )
	{
		return NULL;
	}

	bool IsValidIndex( int index )
	{
		return false;
	}

	int Insert( const char *pString )
	{
		return -1;
	}

	int Find( const char *pString )
	{
		return -1;
	}

	CNetworkStringTableItem	&Element( int index )
	{
		return m_Items.Element( index );
	}

	const CNetworkStringTableItem &Element( int index ) const
	{
		return m_Items.Element( index );
	}

	virtual void UpdateDictionary( int index )
	{
	}

	virtual int DictionaryIndex( int index )
	{
		return -1;
	}

public:
	bool	m_bUseDictionary;

	// We use this type of item to avoid having two copies of the strings in memory --
	//  either we have a dictionary slot and point to that, or we have a m_Name CUtlString that gets
	//  wiped between levels
	class CTableItem
	{
	public:
		static bool Less( const CTableItem &lhs, const CTableItem &rhs )
		{
			return lhs.m_StringHash < rhs.m_StringHash;
		}
	private:
		int						m_DictionaryIndex;
		CUtlString				m_Name;
		CRC32_t					m_StringHash;
	};
	CUtlMap< CTableItem, CNetworkStringTableItem > m_Items;
};
#else

//-----------------------------------------------------------------------------
// Implementation for general purpose strings
//-----------------------------------------------------------------------------
class CNetworkStringDict : public INetworkStringDict
{
public:
	CNetworkStringDict() 
	{
	}

	virtual ~CNetworkStringDict() 
	{ 
	}

	unsigned int Count()
	{
		return m_Lookup.Count();
	}

	void Purge()
	{
		m_Lookup.Purge();
	}

	const char *String( int index )
	{
		return m_Lookup.Key( index ).Get();
	}

	bool IsValidIndex( int index )
	{
		return m_Lookup.IsValidHandle( index );
	}

	int Insert( const char *pString )
	{
		return m_Lookup.Insert( pString );
	}

	int Find( const char *pString )
	{
		return pString ? m_Lookup.Find( pString ) : m_Lookup.InvalidHandle();
	}

	CNetworkStringTableItem	&Element( int index )
	{
		return m_Lookup.Element( index );
	}

	const CNetworkStringTableItem &Element( int index ) const
	{
		return m_Lookup.Element( index );
	}

private:
	CUtlStableHashtable< CUtlConstString, CNetworkStringTableItem, CaselessStringHashFunctor, UTLConstStringCaselessStringEqualFunctor<char> > m_Lookup;
};
#endif

void CNetworkStringTable::DeleteAllStrings( void )
{
	/*
		 == BUG ==
		 When the game shuts down our dll is unloaded and then the stringtables are deleted.
		 This means that when gmod tries to delete the stringtables it would try to call our vtable but we were already unloaded so it would crash.
		 So we need to copy over the original vtable and replace ours with GMod's vtable so that Gmod uses its functions safely.
	*/
	void* m_pItemsVTable = *(void**)m_pItems; // Save original vtable.
#if ARCHITECTURE_X86_64
	CNetworkStringDict* dict = (CNetworkStringDict*)m_pItems;
	bool bUseDictionary = dict->m_bUseDictionary;
#endif

	delete m_pItems;
	if ( m_bIsFilenames )
	{
		m_pItems = new CNetworkStringFilenameDict;
	}
	else
	{
#if ARCHITECTURE_X86_64
		m_pItems = new CNetworkStringDict(bUseDictionary);
#else
		m_pItems = new CNetworkStringDict;
#endif
	}
	*(void**)m_pItems = m_pItemsVTable; // Restore original vtable.

	if ( m_pItemsClientSide )
	{
#if ARCHITECTURE_X86_64
		CNetworkStringDict* clientDict = (CNetworkStringDict*)m_pItemsClientSide;
		bool bUseClientDictionary = clientDict->m_bUseDictionary;
#endif
		void* m_pItemsClientSideVTable = *(void**)m_pItemsClientSide; // Save original vtable.
		delete m_pItemsClientSide;
#if ARCHITECTURE_X86_64
		m_pItemsClientSide = new CNetworkStringDict(bUseClientDictionary);
#else
		m_pItemsClientSide = new CNetworkStringDict;
#endif
		m_pItemsClientSide->Insert( "___clientsideitemsplaceholder0___" ); // 0 slot can't be used
		m_pItemsClientSide->Insert( "___clientsideitemsplaceholder1___" ); // -1 can't be used since it looks like the "invalid" index from other string lookups
		*(void**)m_pItemsClientSide = m_pItemsClientSideVTable; // Restore original vtable.
	}
}
#endif

static CNetworkStringTableContainer* networkStringTableContainerServer = NULL;
void CStringTableModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	if (appfn[0])
	{
		networkStringTableContainerServer = (CNetworkStringTableContainer*)appfn[0](INTERFACENAME_NETWORKSTRINGTABLESERVER, NULL);
	} else {
		SourceSDK::FactoryLoader engine_loader("engine");
		networkStringTableContainerServer = engine_loader.GetInterface<CNetworkStringTableContainer>(INTERFACENAME_NETWORKSTRINGTABLESERVER);
	}

	Detour::CheckValue("get interface", "networkStringTableContainerServer", networkStringTableContainerServer != NULL);
}

PushReferenced_LuaClass(INetworkStringTable)
Get_LuaClass(INetworkStringTable, "INetworkStringTable")

static Detouring::Hook detour_CNetworkStringTable_Deconstructor;
static void hook_CNetworkStringTable_Deconstructor(INetworkStringTable* tbl)
{
	Msg("Deconstructed StringTable %p\n", tbl);
	DeleteGlobal_INetworkStringTable(tbl);
	detour_CNetworkStringTable_Deconstructor.GetTrampoline<Symbols::CNetworkStringTable_Deconstructor>()(tbl);
}

LUA_FUNCTION_STATIC(INetworkStringTable__tostring)
{
	INetworkStringTable* table = Get_INetworkStringTable(LUA, 1, false);
	if (!table)
	{
		LUA->PushString("INetworkStringTable [NULL]");
		return 1;
	}

	const char* pTableName = table->GetTableName();
	if (!pTableName)
	{
		LUA->PushString("INetworkStringTable [NULL NAME]");
		return 1;
	}

	char szBuf[128] = {};
	V_snprintf(szBuf, sizeof(szBuf),"INetworkStringTable [%s]", pTableName); 
	LUA->PushString(szBuf);
	return 1;
}

Default__index(INetworkStringTable);
Default__newindex(INetworkStringTable);
Default__GetTable(INetworkStringTable);

LUA_FUNCTION_STATIC(INetworkStringTable_GetTableName)
{
	INetworkStringTable* table = Get_INetworkStringTable(LUA, 1, true);

	LUA->PushString(table->GetTableName());
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetTableId)
{
	INetworkStringTable* table = Get_INetworkStringTable(LUA, 1, true);

	LUA->PushNumber(table->GetTableId());
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetNumStrings)
{
	INetworkStringTable* table = Get_INetworkStringTable(LUA, 1, true);

	LUA->PushNumber(table->GetNumStrings());
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetMaxStrings)
{
	INetworkStringTable* table = Get_INetworkStringTable(LUA, 1, true);

	LUA->PushNumber(table->GetMaxStrings());
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetEntryBits)
{
	INetworkStringTable* table = Get_INetworkStringTable(LUA, 1, true);

	LUA->PushNumber(table->GetEntryBits());
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_SetTick)
{
	INetworkStringTable* table = Get_INetworkStringTable(LUA, 1, true);

	int tick = (int)LUA->CheckNumber(2);

	table->SetTick(tick);
	return 0;
}

LUA_FUNCTION_STATIC(INetworkStringTable_ChangedSinceTick)
{
	INetworkStringTable* table = Get_INetworkStringTable(LUA, 1, true);

	int tick = (int)LUA->CheckNumber(2);
	LUA->PushBool(table->ChangedSinceTick(tick));
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_AddString)
{
	INetworkStringTable* table = Get_INetworkStringTable(LUA, 1, true);

	const char* pStr = LUA->CheckString(2);
	bool bIsServer = true;
	if (LUA->IsType(3, GarrysMod::Lua::Type::Bool))
		bIsServer = LUA->GetBool(3);
	//int length = LUA->CheckNumberOpt(4, -1);

	LUA->PushNumber(table->AddString(bIsServer, pStr));
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetString)
{
	INetworkStringTable* table = Get_INetworkStringTable(LUA, 1, true);

	int idx = (int)LUA->CheckNumber(2);

	LUA->PushString(table->GetString(idx));
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetAllStrings)
{
	INetworkStringTable* table = Get_INetworkStringTable(LUA, 1, true);

	int idx = 0;
	LUA->PreCreateTable(table->GetMaxStrings(), 0);
	for (int i = 0; i < table->GetMaxStrings(); ++i)
	{
		const char* pStr = table->GetString(i);
		if (!pStr)
			continue;

		LUA->PushString(pStr);
		Util::RawSetI(LUA, -2, ++idx);
	}

	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_FindStringIndex)
{
	INetworkStringTable* table = Get_INetworkStringTable(LUA, 1, true);

	const char* pStr = LUA->CheckString(2);

	LUA->PushNumber(table->FindStringIndex(pStr));
	return 1;
}

static Symbols::CNetworkStringTable_DeleteAllStrings func_CNetworkStringTable_DeleteAllStrings;
LUA_FUNCTION_STATIC(INetworkStringTable_DeleteAllStrings)
{
	INetworkStringTable* table = Get_INetworkStringTable(LUA, 1, true);
	bool bNukePrecache = LUA->GetBool(2);

#ifndef SYSTEM_WINDOWS
	if (!func_CNetworkStringTable_DeleteAllStrings)
		LUA->ThrowError("Failed to get CNetworkStringTable::DeleteAllStrings");

	func_CNetworkStringTable_DeleteAllStrings(table);
#else
	CNetworkStringTable* pTable = (CNetworkStringTable*)table;
	pTable->DeleteAllStrings();
#endif

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
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(LUA, 1, true);

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
	INetworkStringTable* table = Get_INetworkStringTable(LUA, 1, true);

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

#ifdef SYSTEM_WINDOWS
	CNetworkStringTable* pTable = (CNetworkStringTable*)table;
	pTable->DeleteAllStrings();
#else
	if (!func_CNetworkStringTable_DeleteAllStrings)
		LUA->ThrowError("Failed to get CNetworkStringTable::DeleteAllStrings");

	func_CNetworkStringTable_DeleteAllStrings(table); // If this deletes the UserData that we got, were gonna have a problem.
#endif

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
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(LUA, 1, false);
	
	LUA->PushBool(table != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_SetStringUserData)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(LUA, 1, true);
	int idx = (int)LUA->CheckNumber(2);
	const char* pUserData = LUA->GetString(3);
	int iLength = (int)LUA->CheckNumberOpt(4, NULL);

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
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(LUA, 1, true);
	int idx = (int)LUA->CheckNumber(2);

	int iLength = 0;
	LUA->PushString((const char*)table->GetStringUserData(idx, &iLength));
	LUA->PushNumber(iLength);
	return 2;
}

LUA_FUNCTION_STATIC(INetworkStringTable_SetNumberUserData)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(LUA, 1, true);
	int idx = (int)LUA->CheckNumber(2);
	int pUserData = (int)LUA->CheckNumber(3);

	if (idx >= table->GetNumStrings())
		return 0;

	table->SetStringUserData(idx, sizeof(int), &pUserData);
	return 0;
}

LUA_FUNCTION_STATIC(INetworkStringTable_GetNumberUserData)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(LUA, 1, true);
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
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(LUA, 1, true);
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
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(LUA, 1, true);
	int idx = (int)LUA->CheckNumber(2);

	int dataLen; 
	const void *pData = table->GetStringUserData(idx, &dataLen);
	if (pData && dataLen == sizeof(CPrecacheUserData))
		LUA->PushNumber(((const CPrecacheUserData*)table->GetStringUserData(idx, 0))->flags);
	else
		LUA->PushNil();

	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_IsClientSideAddStringAllowed)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(LUA, 1, true);

	LUA->PushBool(table->m_bAllowClientSideAddString);
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_IsLocked)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(LUA, 1, true);

	LUA->PushBool(table->m_bLocked);
	return 1;
}

LUA_FUNCTION_STATIC(INetworkStringTable_SetLocked)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(LUA, 1, true);
	table->m_bLocked = LUA->GetBool(2);

	return 0;
}

LUA_FUNCTION_STATIC(INetworkStringTable_IsFilenames)
{
	CNetworkStringTable* table = (CNetworkStringTable*)Get_INetworkStringTable(LUA, 1, true);

	LUA->PushBool(table->m_bIsFilenames);
	return 1;
}

static inline void UpdateCGameServerStringTables(INetworkStringTable* pTable)
{
	CGameServer* pServer = (CGameServer*)Util::server;
	if (pServer && pTable)
	{
		if (!Q_stricmp(MODEL_PRECACHE_TABLENAME, pTable->GetTableName()) && !pServer->m_pModelPrecacheTable)
		{
			pServer->m_pModelPrecacheTable = pTable;
		} else if (!Q_stricmp(SOUND_PRECACHE_TABLENAME, pTable->GetTableName()) && !pServer->m_pSoundPrecacheTable)
		{
			pServer->m_pSoundPrecacheTable = pTable;
		} else if (!Q_stricmp(DECAL_PRECACHE_TABLENAME, pTable->GetTableName()) && !pServer->m_pDecalPrecacheTable)
		{
			pServer->m_pDecalPrecacheTable = pTable;
		} else if (!Q_stricmp(GENERIC_PRECACHE_TABLENAME, pTable->GetTableName()) && !pServer->m_pGenericPrecacheTable)
		{
			pServer->m_pGenericPrecacheTable = pTable;
		} else if (!Q_stricmp("DynamicModels", pTable->GetTableName()) && !pServer->m_pDynamicModelsTable)
		{
			pServer->m_pDynamicModelsTable = pTable;
		}
		// Additional CBaseServer tables
		else if (!Q_stricmp(INSTANCE_BASELINE_TABLENAME, pTable->GetTableName()) && !pServer->m_pInstanceBaselineTable)
		{
			pServer->m_pInstanceBaselineTable = pTable;
		}
		 else if (!Q_stricmp(LIGHT_STYLES_TABLENAME, pTable->GetTableName()) && !pServer->m_pLightStyleTable)
		{
			pServer->m_pLightStyleTable = pTable;
		}
		else if (!Q_stricmp(LIGHT_STYLES_TABLENAME, pTable->GetTableName()) && !pServer->m_pUserInfoTable)
		{
			pServer->m_pUserInfoTable = pTable;
		}
		else if (!Q_stricmp(SERVER_STARTUP_DATA_TABLENAME, pTable->GetTableName()) && !pServer->m_pServerStartupTable)
		{
			pServer->m_pServerStartupTable = pTable;
		}
		else if (!Q_stricmp("downloadables", pTable->GetTableName()) && !pServer->m_pDownloadableFileTable)
		{
			pServer->m_pDownloadableFileTable = pTable;
		}
	}
}

LUA_FUNCTION_STATIC(stringtable_CreateStringTable)
{
	const char* name = LUA->CheckString(1);
	int maxentries = (int)LUA->CheckNumber(2);
	int userdatafixedsize = (int)LUA->CheckNumberOpt(3, 0);
	int userdatanetworkbits = (int)LUA->CheckNumberOpt(4, 0);

	if (networkStringTableContainerServer->FindTable(name))
	{
		std::string err = "Tried to create a already existing stringtable! (";
		err.append(name);
		err.append(")");
		LUA->ThrowError(err.c_str());
	}

	if ((1 << Q_log2(maxentries)) != maxentries)
	{
		std::string err = "String tables must be powers of two in size!, ";
		err.append(std::to_string(maxentries));
		err.append(" is not a power of 2\n");
		LUA->ThrowError(err.c_str());
	}

	if (!networkStringTableContainerServer->m_bAllowCreation)
	{
		std::string err = "Tried to create string table '";
		err.append(std::to_string(maxentries));
		err.append("' at wrong time\n");
		LUA->ThrowError(err.c_str());
	}

	INetworkStringTable* pTable = networkStringTableContainerServer->CreateStringTable(name, maxentries, userdatafixedsize, userdatanetworkbits);
	Push_INetworkStringTable(LUA, pTable);
	UpdateCGameServerStringTables(pTable);

	return 1;
}

LUA_FUNCTION_STATIC(stringtable_RemoveAllTables)
{
	networkStringTableContainerServer->RemoveAllTables();
	DeleteAll_INetworkStringTable(LUA); // We don't need this since we hooked into the deconstructor. Update: I'm a idiot. We need it to delete the LuaUserData that else would enter a invalid state.
	// ToDo: Nuke the stored stringtables inside CGameServer.
	// Also, when we create a stringtable like modelprecache, we should update the pointer inside the CGameServer.

	CGameServer* pServer = (CGameServer*)Util::server;
	if (pServer)
	{
		pServer->m_pModelPrecacheTable = NULL;
		pServer->m_pSoundPrecacheTable = NULL;
		pServer->m_pGenericPrecacheTable = NULL;
		pServer->m_pDecalPrecacheTable = NULL;
		pServer->m_pDynamicModelsTable = NULL;

		// CBaseServer tables.
		pServer->m_pInstanceBaselineTable = NULL;
		pServer->m_pLightStyleTable = NULL;
		pServer->m_pUserInfoTable = NULL;
		pServer->m_pServerStartupTable = NULL;
		pServer->m_pDownloadableFileTable = NULL;
	}

	//Error("GG\n"); // Until we do exactly what we described above, this function will 100% cause a crash.

	return 0;
}

LUA_FUNCTION_STATIC(stringtable_FindTable)
{
	const char* name = LUA->CheckString(1);

	INetworkStringTable* table = networkStringTableContainerServer->FindTable(name);
	if (table)
	{
		Push_INetworkStringTable(LUA, table);
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
		Push_INetworkStringTable(LUA, table);
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
	int userdatafixedsize = (int)LUA->CheckNumberOpt(3, 0);
	int userdatanetworkbits = (int)LUA->CheckNumberOpt(4, 0);
	bool bIsFilenames = LUA->GetBool(5);

	if (networkStringTableContainerServer->FindTable(name))
	{
		std::string err = "Tried to create a already existing stringtable! (";
		err.append(name);
		err.append(")");
		LUA->ThrowError(err.c_str());
	}

	if ((1 << Q_log2(maxentries)) != maxentries)
	{
		std::string err = "String tables must be powers of two in size!, ";
		err.append(std::to_string(maxentries));
		err.append(" is not a power of 2\n");
		LUA->ThrowError(err.c_str());
	}

	if (!networkStringTableContainerServer->m_bAllowCreation)
	{
		std::string err = "Tried to create string table '";
		err.append(std::to_string(maxentries));
		err.append("' at wrong time\n");
		LUA->ThrowError(err.c_str());
	}

	INetworkStringTable* pTable = networkStringTableContainerServer->CreateStringTableEx(name, maxentries, userdatafixedsize, userdatanetworkbits, bIsFilenames);
	Push_INetworkStringTable(LUA, pTable);
	UpdateCGameServerStringTables(pTable);
	
	return 1;
}

LUA_FUNCTION_STATIC(stringtable_SetAllowClientSideAddString)
{
	INetworkStringTable* table = Get_INetworkStringTable(LUA, 1, true);
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

LUA_FUNCTION_STATIC(stringtable_SetLocked)
{
	networkStringTableContainerServer->m_bLocked = LUA->GetBool(1);

	return 0;
}

LUA_FUNCTION_STATIC(stringtable_AllowCreation)
{
	networkStringTableContainerServer->m_bAllowCreation = LUA->GetBool(1);

	return 0;
}

LUA_FUNCTION_STATIC(stringtable_RemoveTable)
{
	INetworkStringTable* pTable = Get_INetworkStringTable(LUA, 1, true);

	networkStringTableContainerServer->m_Tables.FastRemove(pTable->GetTableId());
	//DeleteGlobal_INetworkStringTable(pTable); // We don't need this since we hooked into the deconstructor.
	Delete_INetworkStringTable(LUA, pTable); // Delete our Lua pointer.

	CGameServer* pServer = (CGameServer*)Util::server;
	if (pServer)
	{
		if (pServer->m_pModelPrecacheTable == pTable)
		{
			pServer->m_pModelPrecacheTable = NULL;
		} else if (pServer->m_pSoundPrecacheTable == pTable)
		{
			pServer->m_pSoundPrecacheTable = NULL;
		} else if (pServer->m_pDecalPrecacheTable == pTable)
		{
			pServer->m_pDecalPrecacheTable = NULL;
		} else if (pServer->m_pGenericPrecacheTable == pTable)
		{
			pServer->m_pGenericPrecacheTable = NULL;
		} else if (pServer->m_pDynamicModelsTable == pTable)
		{
			pServer->m_pDynamicModelsTable = NULL;
		}
		// Additional CBaseServer tables
		else if (pServer->m_pInstanceBaselineTable == pTable)
		{
			pServer->m_pInstanceBaselineTable = NULL;
		}
		else if (pServer->m_pLightStyleTable == pTable)
		{
			pServer->m_pLightStyleTable = NULL;
		}
		else if (pServer->m_pUserInfoTable == pTable)
		{
			pServer->m_pUserInfoTable = NULL;
		}
		else if (pServer->m_pServerStartupTable == pTable)
		{
			pServer->m_pServerStartupTable = NULL;
		}
		else if (pServer->m_pDownloadableFileTable == pTable)
		{
			pServer->m_pDownloadableFileTable = NULL;
		}
	}
	delete pTable;

	return 0;
}

void CStringTableModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) // ToDo: Implement a INetworkStringTable class, a full table and call a hook when SV_CreateNetworkStringTables -> CreateNetworkStringTables is called.
{
	if (!networkStringTableContainerServer)
		return;

	if (!bServerInit)
	{
		Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::INetworkStringTable, pLua->CreateMetaTable("INetworkStringTable"));
			Util::AddFunc(pLua, INetworkStringTable__tostring, "__tostring");
			Util::AddFunc(pLua, INetworkStringTable__newindex, "__newindex");
			Util::AddFunc(pLua, INetworkStringTable__index, "__index");
			Util::AddFunc(pLua, INetworkStringTable_GetTable, "GetTable");

			Util::AddFunc(pLua, INetworkStringTable_GetTableName, "GetTableName");
			Util::AddFunc(pLua, INetworkStringTable_GetTableId, "GetTableId");
			Util::AddFunc(pLua, INetworkStringTable_GetNumStrings, "GetNumStrings");
			Util::AddFunc(pLua, INetworkStringTable_GetMaxStrings, "GetMaxStrings");
			Util::AddFunc(pLua, INetworkStringTable_GetEntryBits, "GetEntryBits");
			Util::AddFunc(pLua, INetworkStringTable_SetTick, "SetTick");
			Util::AddFunc(pLua, INetworkStringTable_ChangedSinceTick, "ChangedSinceTick");
			Util::AddFunc(pLua, INetworkStringTable_AddString, "AddString");
			Util::AddFunc(pLua, INetworkStringTable_GetString, "GetString");
			Util::AddFunc(pLua, INetworkStringTable_GetAllStrings, "GetAllStrings");
			Util::AddFunc(pLua, INetworkStringTable_FindStringIndex, "FindStringIndex");
			Util::AddFunc(pLua, INetworkStringTable_DeleteAllStrings, "DeleteAllStrings");
			Util::AddFunc(pLua, INetworkStringTable_SetMaxEntries, "SetMaxEntries");
			Util::AddFunc(pLua, INetworkStringTable_DeleteString, "DeleteString");
			Util::AddFunc(pLua, INetworkStringTable_IsValid, "IsValid");
			Util::AddFunc(pLua, INetworkStringTable_SetStringUserData, "SetStringUserData");
			Util::AddFunc(pLua, INetworkStringTable_GetStringUserData, "GetStringUserData");
			Util::AddFunc(pLua, INetworkStringTable_SetNumberUserData, "SetNumberUserData");
			Util::AddFunc(pLua, INetworkStringTable_GetNumberUserData, "GetNumberUserData");
			Util::AddFunc(pLua, INetworkStringTable_SetPrecacheUserData, "SetPrecacheUserData");
			Util::AddFunc(pLua, INetworkStringTable_GetPrecacheUserData, "GetPrecacheUserData");
			Util::AddFunc(pLua, INetworkStringTable_IsClientSideAddStringAllowed, "IsClientSideAddStringAllowed");
			Util::AddFunc(pLua, stringtable_SetAllowClientSideAddString, "SetAllowClientSideAddString"); // Alias to stringtable.SetAllowClientSideAddString to have all Set/Is/Get functions in one place
			Util::AddFunc(pLua, INetworkStringTable_IsLocked, "IsLocked");
			Util::AddFunc(pLua, INetworkStringTable_SetLocked, "SetLocked");
			Util::AddFunc(pLua, INetworkStringTable_IsFilenames, "IsFilenames");
		pLua->Pop(1);

		if (pLua->PushMetaTable(Lua::GetLuaData(pLua)->GetMetaTable(Lua::LuaTypes::INetworkStringTable)))
		{
			pLua->Pop(1);
		} else {
			Warning(PROJECT_NAME ": pLua->PushMetaTable fails to push INetworkStringTable!\n");
		}

		Util::StartTable(pLua);
			Util::AddFunc(pLua, stringtable_CreateStringTable, "CreateStringTable");
			Util::AddFunc(pLua, stringtable_RemoveAllTables, "RemoveAllTables");
			Util::AddFunc(pLua, stringtable_FindTable, "FindTable");
			Util::AddFunc(pLua, stringtable_GetTable, "GetTable");
			Util::AddFunc(pLua, stringtable_GetNumTables, "GetNumTables");
			Util::AddFunc(pLua, stringtable_CreateStringTableEx, "CreateStringTableEx");
			Util::AddFunc(pLua, stringtable_SetAllowClientSideAddString, "SetAllowClientSideAddString");
			Util::AddFunc(pLua, stringtable_IsCreationAllowed, "IsCreationAllowed");
			Util::AddFunc(pLua, stringtable_IsLocked, "IsLocked");
			Util::AddFunc(pLua, stringtable_SetLocked, "SetLocked");
			Util::AddFunc(pLua, stringtable_AllowCreation, "AllowCreation");
			Util::AddFunc(pLua, stringtable_RemoveTable, "RemoveTable");

			Util::AddValue(pLua, INVALID_STRING_INDEX, "INVALID_STRING_INDEX");
		Util::FinishTable(pLua, "stringtable");
	} else {
		if (Lua::PushHook("HolyLib:OnStringTableCreation", pLua)) // Use this hook to create / modify the stringtables.
		{
			networkStringTableContainerServer->m_bAllowCreation = true; // Will this work? We'll see.
			pLua->CallFunctionProtected(1, 0, true);
			networkStringTableContainerServer->m_bAllowCreation = false;
		}
	}
}

void CStringTableModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) // ToDo: Can we remove the metatable?
{
	Util::NukeTable(pLua, "stringtable");

	DeleteAll_INetworkStringTable(pLua);
}

void CStringTableModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	// Note: This hook exists for safety and if everything goes well we shouldn't even require it.
	SourceSDK::ModuleLoader engine_loader("engine");
	Detour::Create(
		&detour_CNetworkStringTable_Deconstructor, "CNetworkStringTable::~CNetworkStringTable",
		engine_loader.GetModule(), Symbols::CNetworkStringTable_DeconstructorSym,
		(void*)hook_CNetworkStringTable_Deconstructor, m_pID
	);

	func_CNetworkStringTable_DeleteAllStrings = (Symbols::CNetworkStringTable_DeleteAllStrings)Detour::GetFunction(engine_loader.GetModule(), Symbols::CNetworkStringTable_DeleteAllStringsSym);
	Detour::CheckFunction((void*)func_CNetworkStringTable_DeleteAllStrings, "CNetworkStringTable::DeleteAllStrings");
}