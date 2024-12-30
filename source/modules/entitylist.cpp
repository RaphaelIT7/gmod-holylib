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
EntityList g_pGlobalEntityList; // NOTE: This needs to be after pEntityLists? (funny Constructor Behavior apparently)

EntityList::EntityList()
{
	pEntityLists.push_back(this);
}

EntityList::~EntityList()
{
	Clear();
	Vector_RemoveElement(pEntityLists, this)
}

void EntityList::Clear()
{
	pEntities.clear();
	if (g_Lua)
		for (auto& [_, iReference] : pEntReferences)
			g_Lua->ReferenceFree(iReference);
	
	pEntReferences.clear();
}

void EntityList::CreateReference(CBaseEntity* pEntity)
{
	if (!g_Lua)
		Error("holylib: missing g_Lua!\n");

	Util::Push_Entity(pEntity);
	pEntReferences[pEntity] = g_Lua->ReferenceCreate();
}

void EntityList::FreeEntity(CBaseEntity* pEntity)
{
	auto it = pEntReferences.find(pEntity);
	if (it != pEntReferences.end())
	{
		if (IsValidReference(it->second))
			if (g_Lua)
				g_Lua->ReferenceFree(it->second);

		Vector_RemoveElement(pEntities, pEntity);
		pEntReferences.erase(it);
	}
}

bool Is_EntityList(int iStackPos)
{
	return g_Lua->IsType(iStackPos, EntityList_TypeID);
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
	V_snprintf(szBuf, sizeof(szBuf), "EntityList [%i]", (int)pData->pEntities.size());
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

	LUA->PreCreateTable(pData->pEntities.size(), 0);
		int idx = 0;
		for (auto& [pEnt, iReference] : pData->pEntReferences)
		{
			if (!pData->IsValidReference(iReference))
				pData->CreateReference(pEnt);

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

		pData->CreateReference(pEntity);
		pData->pEntities.push_back(pEntity);

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

		auto it = pData->pEntReferences.find(pEntity);
		if (it != pData->pEntReferences.end())
		{
			LUA->Pop(1);
			continue;
		}

		pData->CreateReference(pEntity);
		pData->pEntities.push_back(pEntity);

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

	pData->CreateReference(pEntity);
	pData->pEntities.push_back(pEntity);

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
	EntityList* pList = new EntityList;
	Push_EntityList(pList);
	return 1;
}

LUA_FUNCTION_STATIC(GetGlobalEntityList)
{
	LUA->PreCreateTable(g_pGlobalEntityList.pEntities.size(), 0);
		int idx = 0;
		for (auto& [pEnt, iReference] : g_pGlobalEntityList.pEntReferences)
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
	g_pGlobalEntityList.pEntReferences[pEntity] = -1;
	g_pGlobalEntityList.pEntities.push_back(pEntity);

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
}

void CEntListModule::LuaShutdown()
{
	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		Util::RemoveField("CreateEntityList");
		Util::RemoveField("GetGlobalEntityList");
	g_Lua->Pop(1);
	g_pGlobalEntityList.Clear();
}