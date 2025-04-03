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

Push_LuaClass(EntityList)
Get_LuaClass(EntityList, "EntityList")

static std::unordered_set<EntityList*> pEntityLists;

class LuaEntityModuleData : public Lua::ModuleData
{
public:
	EntityList pGlobalEntityList;
};

static inline LuaEntityModuleData* GetLuaData(GarrysMod::Lua::ILuaInterface* pLua)
{
	if (!pLua)
		return NULL;

	return (LuaEntityModuleData*)Lua::GetLuaData(pLua)->GetModuleData(g_pEntListModule.m_pID);
}

EntityList& GetGlobalEntityList(GarrysMod::Lua::ILuaInterface* pLua)
{
	auto pData = GetLuaData(pLua);
	
	return pData->pGlobalEntityList;
}

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
			Util::ReferenceFree(m_pLua, iReference, "EntityList::Clear");
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
			Util::ReferenceFree(m_pLua, m_pEntReferences[pEntity], "EntityList::CreateReference - Leak");
		}
	}

	Util::Push_Entity(m_pLua, pEntity);
	m_pEntReferences[pEntity] = Util::ReferenceCreate(m_pLua, "EntityList::CreateReference");
}

void EntityList::FreeEntity(CBaseEntity* pEntity)
{
	auto it = m_pEntReferences.find(pEntity);
	if (it != m_pEntReferences.end())
	{
		if (IsValidReference(it->second))
			Util::ReferenceFree(m_pLua, it->second, "EntityList::FreeEntity");

		Vector_RemoveElement(m_pEntities, pEntity);
		m_pEntReferences.erase(it);
	}
}

bool Is_EntityList(GarrysMod::Lua::ILuaInterface* pLua, int iStackPos)
{
	if (pLua)
	{
		return pLua->IsType(iStackPos, Lua::GetLuaData(pLua)->GetMetaTable(Lua::LuaTypes::EntityList));
	}

	return false;
}

LUA_FUNCTION_STATIC(EntityList__tostring)
{
	EntityList* pData = Get_EntityList(LUA, 1, false);
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
	if (pList)
		delete pList;
)

LUA_FUNCTION_STATIC(EntityList_IsValid)
{
	LUA->PushBool(Get_EntityList(LUA, 1, false) != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(EntityList_GetEntities)
{
	EntityList* pData = Get_EntityList(LUA, 1, true);

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
	EntityList* pData = Get_EntityList(LUA, 1, true);
	LUA->CheckType(2, GarrysMod::Lua::Type::Table);
	pData->Clear();

	LUA->Push(2);
	LUA->PushNil();
	while (LUA->Next(-2))
	{
		CBaseEntity* pEntity = Util::Get_Entity(LUA, -1, true);

		pData->AddEntity(pEntity);
		pData->CreateReference(pEntity);

		LUA->Pop(1);
	}
	LUA->Pop(1);
	return 0;
}

LUA_FUNCTION_STATIC(EntityList_AddEntities)
{
	EntityList* pData = Get_EntityList(LUA, 1, true);
	LUA->CheckType(2, GarrysMod::Lua::Type::Table);

	LUA->Push(2);
	LUA->PushNil();
	while (LUA->Next(-2))
	{
		CBaseEntity* pEntity = Util::Get_Entity(LUA, -1, true);

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
	EntityList* pData = Get_EntityList(LUA, 1, true);
	LUA->CheckType(2, GarrysMod::Lua::Type::Table);

	LUA->Push(2);
	LUA->PushNil();
	while (LUA->Next(-2))
	{
		CBaseEntity* pEntity = Util::Get_Entity(LUA, -1, true);
		pData->FreeEntity(pEntity);
		LUA->Pop(1);
	}
	LUA->Pop(1);

	return 0;
}

LUA_FUNCTION_STATIC(EntityList_AddEntity)
{
	EntityList* pData = Get_EntityList(LUA, 1, true);
	CBaseEntity* pEntity = Util::Get_Entity(LUA, 2, true);

	pData->AddEntity(pEntity, true);

	return 0;
}

LUA_FUNCTION_STATIC(EntityList_RemoveEntity)
{
	EntityList* pData = Get_EntityList(LUA, 1, true);
	CBaseEntity* pEntity = Util::Get_Entity(LUA, 2, true);

	pData->FreeEntity(pEntity);

	return 0;
}

LUA_FUNCTION_STATIC(CreateEntityList)
{
	EntityList* pList = new EntityList();
	pList->SetLua(LUA);
	Push_EntityList(LUA, pList);
	return 1;
}

LUA_FUNCTION_STATIC(GetGlobalEntityList)
{
	EntityList& pGlobalEntityList = GetGlobalEntityList(LUA);
	LUA->PreCreateTable(pGlobalEntityList.GetEntities().size(), 0);
		int idx = 0;
		for (auto& [pEnt, iReference] : pGlobalEntityList.GetReferences())
		{
			if (!pGlobalEntityList.IsValidReference(iReference))
				pGlobalEntityList.CreateReference(pEnt);

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
	EntityList& pGlobalEntityList = GetGlobalEntityList(g_Lua);
	pGlobalEntityList.FreeEntity(pEntity);

	//Util::Push_Entity(pEntity); // BUG: The Engine hates us for this. "CREATING ENTITY - ALREADY HAS A LUA TABLE! AND IT SHOULDN'T"
	pGlobalEntityList.AddEntity(pEntity);

	if (g_pEntListModule.InDebug())
		Msg("Created Entity %p (%p, %i)\n", pEntity, &pGlobalEntityList, (int)pEntityLists.size());
}

void CEntListModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Lua::GetLuaData(pLua)->SetModuleData(m_pID, new LuaEntityModuleData);

	Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::EntityList, pLua->CreateMetaTable("EntityList"));
		Util::AddFunc(pLua, EntityList__tostring, "__tostring");
		Util::AddFunc(pLua, EntityList__index, "__index");
		Util::AddFunc(pLua, EntityList__newindex, "__newindex");
		Util::AddFunc(pLua, EntityList__gc, "__gc");
		Util::AddFunc(pLua, EntityList_GetTable, "GetTable");
		Util::AddFunc(pLua, EntityList_IsValid, "IsValid");
		Util::AddFunc(pLua, EntityList_GetEntities, "GetEntities");
		Util::AddFunc(pLua, EntityList_SetEntities, "SetEntities");
		Util::AddFunc(pLua, EntityList_AddEntities, "AddEntities");
		Util::AddFunc(pLua, EntityList_RemoveEntities, "RemoveEntities");
		Util::AddFunc(pLua, EntityList_AddEntity, "AddEntity");
		Util::AddFunc(pLua, EntityList_RemoveEntity, "RemoveEntity");
	pLua->Pop(1);

	pLua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		Util::AddFunc(pLua, CreateEntityList, "CreateEntityList");
		Util::AddFunc(pLua, GetGlobalEntityList, "GetGlobalEntityList");
	pLua->Pop(1);
}

void CEntListModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	pLua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		Util::RemoveField(pLua, "CreateEntityList");
		Util::RemoveField(pLua, "GetGlobalEntityList");
	pLua->Pop(1);

	EntityList& pGlobalEntityList = GetGlobalEntityList(pLua);
	pGlobalEntityList.Invalidate();
}