#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "player.h"
#include "unordered_set"

class CEntListModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void OnEntityCreated(CBaseEntity* pEntity) OVERRIDE;
	virtual void OnEntityDeleted(CBaseEntity* pEntity) OVERRIDE;
	virtual const char* Name() { return "entitylist"; };
	virtual int Compatibility() { return LINUX32; };
};

CEntListModule g_pEntListModule;
IModule* pEntListModule = &g_pEntListModule;

static int EntityList_TypeID = -1;
PushReferenced_LuaClass(EntityList, EntityList_TypeID)
Get_LuaClass(EntityList, EntityList_TypeID, "EntityList")

static std::vector<EntityList*> pEntityLists;
EntityList g_pGlobalEntityList(NULL); // NOTE: This needs to be after pEntityLists? (funny Constructor Behavior apparently)

EntityList::EntityList(GarrysMod::Lua::ILuaInterface* pLua)
{
	m_pLua = pLua;
	pEntityLists.push_back(this);
}

EntityList::~EntityList()
{
	Invalidate();
	Vector_RemoveElement(pEntityLists, this)
}

void EntityList::Clear()
{
	m_pEntities.clear();
	if (m_pLua)
		for (auto& [_, iReference] : m_pEntReferences)
			m_pLua->ReferenceFree(iReference);
	
	m_pEntReferences.clear();
}

void EntityList::CreateReference(CBaseEntity* pEntity)
{
	if (!m_pLua)
		Error("holylib: missing pLua!\n");

	Util::Push_Entity(pEntity);
	m_pEntReferences[pEntity] = m_pLua->ReferenceCreate();
}

void EntityList::FreeEntity(CBaseEntity* pEntity)
{
	auto it = m_pEntReferences.find(pEntity);
	if (it != m_pEntReferences.end())
	{
		if (IsValidReference(it->second) && m_pLua)
			m_pLua->ReferenceFree(it->second);

		Vector_RemoveElement(m_pEntities, pEntity);
		m_pEntReferences.erase(it);
	}
}

bool Is_EntityList(int iStackPos)
{
	if (g_Lua)
		return g_Lua->IsType(iStackPos, EntityList_TypeID);

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

LUA_FUNCTION_STATIC(EntityList__gc)
{
	EntityList* pData = Get_EntityList(1, false);
	if (pData && pData != &g_pGlobalEntityList)
	{
		Delete_EntityList(pData);
		delete pData;
	}

	return 0;
}

LUA_FUNCTION_STATIC(EntityList__index)
{
	if (LUA->FindOnObjectsMetaTable(1, 2))
		return 1;

	LUA->Pop(1);
	Util::ReferencePush(g_pPushedEntityList[Get_EntityList(1, true)]->iTableReference);
	if (!LUA->FindObjectOnTable(-1, 2))
		LUA->PushNil();

	LUA->Remove(-2);

	return 1;
}

LUA_FUNCTION_STATIC(EntityList__newindex)
{
	Util::ReferencePush(g_pPushedEntityList[Get_EntityList(1, true)]->iTableReference);
	LUA->Push(2);
	LUA->Push(3);
	LUA->RawSet(-3);
	LUA->Pop(1);

	return 0;
}

LUA_FUNCTION_STATIC(EntityList_GetLuaTable)
{
	Util::ReferencePush(g_pPushedEntityList[Get_EntityList(1, true)]->iTableReference);

	return 1;
}

LUA_FUNCTION_STATIC(EntityList_IsValid)
{
	LUA->PushBool(Get_EntityList(1, false) != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(EntityList_GetTable)
{
	EntityList* pData = Get_EntityList(1, true);

	LUA->PreCreateTable(pData->GetEntities().size(), 0);
		int idx = 0;
		for (auto& [pEnt, iReference] : pData->GetReferences())
		{
			pData->EnsureReference(pEnt, iReference);

			Util::ReferencePush(iReference);
			Util::RawSetI(-2, ++idx);
		}
	return 1;
}

LUA_FUNCTION_STATIC(EntityList_SetTable)
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

LUA_FUNCTION_STATIC(EntityList_AddTable)
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

LUA_FUNCTION_STATIC(EntityList_RemoveTable)
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

LUA_FUNCTION_STATIC(EntityList_Add)
{
	EntityList* pData = Get_EntityList(1, true);
	CBaseEntity* pEntity = Util::Get_Entity(2, true);

	pData->AddEntity(pEntity, true);

	return 0;
}

LUA_FUNCTION_STATIC(EntityList_Remove)
{
	EntityList* pData = Get_EntityList(1, true);
	CBaseEntity* pEntity = Util::Get_Entity(2, true);

	pData->FreeEntity(pEntity);

	return 0;
}

LUA_FUNCTION_STATIC(CreateEntityList)
{
	EntityList* pList = new EntityList(LUA);
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

			Util::ReferencePush(iReference);
			Util::RawSetI(-2, ++idx);
		}

	return 1;
}

void CEntListModule::OnEntityDeleted(CBaseEntity* pEntity)
{
	if (g_pEntListModule.InDebug())
		Msg("Deleted Entity: %p (%i)\n", pEntity, pEntityLists.size());

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
		Msg("Created Entity %p (%p, %i)\n", pEntity, &g_pGlobalEntityList, pEntityLists.size());
}

void CEntListModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	EntityList_TypeID = g_Lua->CreateMetaTable("EntityList");
		Util::AddFunc(EntityList__tostring, "__tostring");
		Util::AddFunc(EntityList__index, "__index");
		Util::AddFunc(EntityList__newindex, "__newindex");
		Util::AddFunc(EntityList__gc, "__gc");
		Util::AddFunc(EntityList_GetLuaTable, "GetLuaTable");
		Util::AddFunc(EntityList_IsValid, "IsValid");
		Util::AddFunc(EntityList_GetTable, "GetTable");
		Util::AddFunc(EntityList_SetTable, "SetTable");
		Util::AddFunc(EntityList_AddTable, "AddTable");
		Util::AddFunc(EntityList_RemoveTable, "RemoveTable");
		Util::AddFunc(EntityList_Add, "Add");
		Util::AddFunc(EntityList_Remove, "Remove");
	g_Lua->Pop(1);

	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		Util::AddFunc(CreateEntityList, "CreateEntityList");
		Util::AddFunc(GetGlobalEntityList, "GetGlobalEntityList");
	g_Lua->Pop(1);

	g_pGlobalEntityList.SetLua(g_Lua);
}

void CEntListModule::LuaShutdown()
{
	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		Util::RemoveField("CreateEntityList");
		Util::RemoveField("GetGlobalEntityList");
	g_Lua->Pop(1);
	g_pGlobalEntityList.Invalidate();
}