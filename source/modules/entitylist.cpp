#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "player.h"
#include "unordered_set"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CEntListModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void OnEntityCreated(CBaseEntity* pEntity) OVERRIDE;
	virtual void OnEntityDeleted(CBaseEntity* pEntity) OVERRIDE;
	virtual const char* Name() { return "entitylist"; };
	virtual int Compatibility() { return LINUX32; };
};

CEntListModule g_pEntListModule;
IModule* pEntListModule = &g_pEntListModule;

static int EntityList_TypeID = -1;
Push_LuaClass(EntityList, EntityList_TypeID)
Get_LuaClass(EntityList, EntityList_TypeID, "EntityList")

static std::unordered_set<EntityList*> pEntityLists;
EntityList g_pGlobalEntityList; // NOTE: This needs to be after pEntityLists? (funny Constructor Behavior apparently)

EntityList::EntityList()
{
	pEntityLists.insert(this);
}

EntityList::~EntityList()
{
	Invalidate();
	pEntityLists.erase(this);
}

void EntityList::Clear()
{
	m_pEntities.clear();
	for (auto& [_, iReference] : m_pEntReferences)
	{
		if (IsValidReference(iReference))
		{
			Util::ReferenceFree(iReference, "EntityList::Clear");
		}
	}
	
	m_pEntReferences.clear();
}

void EntityList::CreateReference(CBaseEntity* pEntity)
{
	auto it = m_pEntReferences.find(pEntity);
	if (it != m_pEntReferences.end())
	{
		if (IsValidReference(it->second)) // We initally set it to -1
		{
			Warning(PROJECT_NAME ": entitylist is leaking references! Report this!\n");
			Util::ReferenceFree(m_pEntReferences[pEntity], "EntityList::CreateReference - Leak");
		}
	}

	Util::Push_Entity(pEntity);
	m_pEntReferences[pEntity] = Util::ReferenceCreate("EntityList::CreateReference");
}

void EntityList::FreeEntity(CBaseEntity* pEntity)
{
	auto it = m_pEntReferences.find(pEntity);
	if (it != m_pEntReferences.end())
	{
		if (IsValidReference(it->second))
			Util::ReferenceFree(it->second, "EntityList::FreeEntity");

		Vector_RemoveElement(m_pEntities, pEntity);
		m_pEntReferences.erase(it);
	}
}

bool Is_EntityList(int iStackPos)
{
	if (g_Lua)
	{
		return g_Lua->IsType(iStackPos, EntityList_TypeID);
	}

	return false;
}

LUA_FUNCTION_STATIC(EntityList__tostring)
{
	EntityList* pData = Get_EntityList(1, false);
	if (!pData)
	{
		LUA->PushString("EntityList [NULL]");
		return 1;
	}

	char szBuf[64] = {};
	V_snprintf(szBuf, sizeof(szBuf), "EntityList [%i]", (int)pData->GetEntities().size());
	LUA->PushString(szBuf);
	return 1;
}

Default__index(EntityList);
Default__newindex(EntityList);
Default__GetTable(EntityList);
Default__gc(EntityList,
	EntityList* pList = (EntityList*)pData->GetData();
	if (pList != &g_pGlobalEntityList)
		delete pList;
)

LUA_FUNCTION_STATIC(EntityList_IsValid)
{
	LUA->PushBool(Get_EntityList(1, false) != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(EntityList_GetEntities)
{
	EntityList* pData = Get_EntityList(1, true);

	LUA->PreCreateTable(pData->GetEntities().size(), 0);
		int idx = 0;
		for (auto& [pEnt, iReference] : pData->GetReferences())
		{
			pData->EnsureReference(pEnt, iReference);

			Util::ReferencePush(LUA, iReference);
			Util::RawSetI(LUA, -2, ++idx);
		}
	return 1;
}

LUA_FUNCTION_STATIC(EntityList_SetEntities)
{
	EntityList* pData = Get_EntityList(1, true);
	LUA->CheckType(2, GarrysMod::Lua::Type::Table);
	pData->Clear();

	LUA->Push(2);
	LUA->PushNil();
	while (LUA->Next(-2))
	{
		CBaseEntity* pEntity = Util::Get_Entity(-1, true);

		pData->AddEntity(pEntity);
		pData->CreateReference(pEntity);

		LUA->Pop(1);
	}
	LUA->Pop(1);
	return 0;
}

LUA_FUNCTION_STATIC(EntityList_AddEntities)
{
	EntityList* pData = Get_EntityList(1, true);
	LUA->CheckType(2, GarrysMod::Lua::Type::Table);

	LUA->Push(2);
	LUA->PushNil();
	while (LUA->Next(-2))
	{
		CBaseEntity* pEntity = Util::Get_Entity(-1, true);

		auto it = pData->GetReferences().find(pEntity);
		if (it != pData->GetReferences().end())
		{
			LUA->Pop(1);
			continue;
		}

		pData->AddEntity(pEntity, true);

		LUA->Pop(1);
	}
	LUA->Pop(1);
	return 0;
}

LUA_FUNCTION_STATIC(EntityList_RemoveEntities)
{
	EntityList* pData = Get_EntityList(1, true);
	LUA->CheckType(2, GarrysMod::Lua::Type::Table);

	LUA->Push(2);
	LUA->PushNil();
	while (LUA->Next(-2))
	{
		CBaseEntity* pEntity = Util::Get_Entity(-1, true);
		pData->FreeEntity(pEntity);
		LUA->Pop(1);
	}
	LUA->Pop(1);

	return 0;
}

LUA_FUNCTION_STATIC(EntityList_AddEntity)
{
	EntityList* pData = Get_EntityList(1, true);
	CBaseEntity* pEntity = Util::Get_Entity(2, true);

	pData->AddEntity(pEntity, true);

	return 0;
}

LUA_FUNCTION_STATIC(EntityList_RemoveEntity)
{
	EntityList* pData = Get_EntityList(1, true);
	CBaseEntity* pEntity = Util::Get_Entity(2, true);

	pData->FreeEntity(pEntity);

	return 0;
}

LUA_FUNCTION_STATIC(CreateEntityList)
{
	EntityList* pList = new EntityList;
	Push_EntityList(pList);
	return 1;
}

LUA_FUNCTION_STATIC(GetGlobalEntityList)
{
	LUA->PreCreateTable(g_pGlobalEntityList.GetEntities().size(), 0);
		int idx = 0;
		for (auto& [pEnt, iReference] : g_pGlobalEntityList.GetReferences())
		{
			if (!g_pGlobalEntityList.IsValidReference(iReference))
				g_pGlobalEntityList.CreateReference(pEnt);

			Util::ReferencePush(LUA, iReference);
			Util::RawSetI(LUA, -2, ++idx);
		}

	return 1;
}

void CEntListModule::OnEntityDeleted(CBaseEntity* pEntity)
{
	if (g_pEntListModule.InDebug())
		Msg("Deleted Entity: %p (%i)\n", pEntity, (int)pEntityLists.size());

	for (EntityList* pList : pEntityLists)
	{
		pList->FreeEntity(pEntity);

		if (g_pEntListModule.InDebug())
			Msg("Deleted Entity inside %p\n", pList);
	}
}

void CEntListModule::OnEntityCreated(CBaseEntity* pEntity)
{
	g_pGlobalEntityList.FreeEntity(pEntity);

	//Util::Push_Entity(pEntity); // BUG: The Engine hates us for this. "CREATING ENTITY - ALREADY HAS A LUA TABLE! AND IT SHOULDN'T"
	g_pGlobalEntityList.AddEntity(pEntity);

	if (g_pEntListModule.InDebug())
		Msg("Created Entity %p (%p, %i)\n", pEntity, &g_pGlobalEntityList, (int)pEntityLists.size());
}

void CEntListModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	EntityList_TypeID = g_Lua->CreateMetaTable("EntityList");
		Util::AddFunc(EntityList__tostring, "__tostring");
		Util::AddFunc(EntityList__index, "__index");
		Util::AddFunc(EntityList__newindex, "__newindex");
		Util::AddFunc(EntityList__gc, "__gc");
		Util::AddFunc(EntityList_GetTable, "GetTable");
		Util::AddFunc(EntityList_IsValid, "IsValid");
		Util::AddFunc(EntityList_GetEntities, "GetEntities");
		Util::AddFunc(EntityList_SetEntities, "SetEntities");
		Util::AddFunc(EntityList_AddEntities, "AddEntities");
		Util::AddFunc(EntityList_RemoveEntities, "RemoveEntities");
		Util::AddFunc(EntityList_AddEntity, "AddEntity");
		Util::AddFunc(EntityList_RemoveEntity, "RemoveEntity");
	g_Lua->Pop(1);

	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		Util::AddFunc(CreateEntityList, "CreateEntityList");
		Util::AddFunc(GetGlobalEntityList, "GetGlobalEntityList");
	g_Lua->Pop(1);
}

void CEntListModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		Util::RemoveField("CreateEntityList");
		Util::RemoveField("GetGlobalEntityList");
	g_Lua->Pop(1);

	g_pGlobalEntityList.Invalidate();
}