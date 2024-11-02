#include "LuaInterface.h"
#include "detours.h"
#include "symbols.h"
#include "module.h"
#include "lua.h"
#include "player.h"

class CEntListModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void OnEdictFreed(const edict_t* pEdict) OVERRIDE;
	virtual const char* Name() { return "entitylist"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
};

CEntListModule g_pEntListModule;
IModule* pEntListModule = &g_pEntListModule;

static int EntityList_TypeID = -1;
static std::vector<EntityList*> pEntityLists;
Push_LuaClass(EntityList, EntityList_TypeID)
Get_LuaClass(EntityList, EntityList_TypeID, "EntityList")

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
	V_snprintf(szBuf, sizeof(szBuf), "EntityList [%i]", pData->pEntities.size());
	LUA->PushString(szBuf);
	return 1;
}

LUA_FUNCTION_STATIC(EntityList__gc)
{
	EntityList* pData = Get_EntityList(1, false);
	if (pData)
	{
		pData->pEntities.clear();
		Vector_RemoveElement(pEntityLists, pData)
	}

	return 0;
}

LUA_FUNCTION_STATIC(EntityList__index)
{
	if (!LUA->FindOnObjectsMetaTable(1, 2))
		LUA->PushNil();

	return 1;
}

LUA_FUNCTION_STATIC(EntityList_GetTable)
{
	EntityList* data = Get_EntityList(1, true);

	LUA->PreCreateTable(data->pEntities.size(), 0);
		int idx = 0;
		for (auto& pHandle : data->pEntities)
		{
			CBaseEntity* ent = pHandle;
			if (!ent)
				continue;

			++idx;
			LUA->PushNumber(idx);
			Util::Push_Entity(ent);
			LUA->SetTable(-3);

			if (g_pEntListModule.InDebug())
				Msg("holylib: NetworkSerialNumber_: %i, Index: %i\n", idx, (int)ent->edict()->m_EdictIndex);
		}
	return 1;
}

LUA_FUNCTION_STATIC(EntityList_SetTable)
{
	EntityList* data = Get_EntityList(1, true);
	data->pEdictHash.clear();
	data->pEntities.clear();

	LUA->Push(2);
	LUA->PushNil();
	while (LUA->Next(-2))
	{
		CBaseEntity* ent = Util::Get_Entity(-1, true);
		if (!ent) // I don't trust it yet.
		{
			LUA->Pop(1);
			continue;
		}

		edict_t* edict = ent->edict();
		if (!edict) // Not a networkable entity?
		{
			LUA->Pop(1);
			continue;
		}

		short edictIndex = ent->edict()->m_EdictIndex;
		data->pEntities.push_back(ent);
		data->pEdictHash[edictIndex] = ent;

		LUA->Pop(1);
	}
	LUA->Pop(1);
	return 0;
}

LUA_FUNCTION_STATIC(EntityList_AddTable)
{
	EntityList* data = Get_EntityList(1, true);

	LUA->Push(2);
	LUA->PushNil();
	while (LUA->Next(-2))
	{
		CBaseEntity* ent = Util::Get_Entity(-1, true);
		if (!ent) // I don't trust it yet.
		{
			LUA->Pop(1);
			continue;
		}

		short edictIndex = ent->edict()->m_EdictIndex;
		auto it = data->pEdictHash.find(edictIndex);
		if (it != data->pEdictHash.end())
		{
			LUA->Pop(1);
			continue;
		}

		data->pEntities.push_back(ent);
		data->pEdictHash[edictIndex] = ent;

		LUA->Pop(1);
	}
	LUA->Pop(1);
	return 0;
}

LUA_FUNCTION_STATIC(EntityList_RemoveTable)
{
	EntityList* data = Get_EntityList(1, true);

	LUA->Push(2);
	LUA->PushNil();
	while (LUA->Next(-2))
	{
		CBaseEntity* ent = Util::Get_Entity(-1, true);
		if (!ent) // I don't trust it yet.
		{
			LUA->Pop(1);
			continue;
		}

		short edictIndex = ent->edict()->m_EdictIndex;
		auto it = data->pEdictHash.find(edictIndex);
		if (it == data->pEdictHash.end())
		{
			LUA->Pop(1);
			continue;
		}

		Vector_RemoveElement(data->pEntities, ent)
		data->pEdictHash.erase(it);

		LUA->Pop(1);
	}
	LUA->Pop(1);
	return 0;
}

LUA_FUNCTION_STATIC(EntityList_Add)
{
	EntityList* data = Get_EntityList(1, true);
	CBaseEntity* ent = Util::Get_Entity(2, true);
	edict_t* edict = ent->edict();

	if (!edict)
		return 0;

	short edictIndex = edict->m_EdictIndex;
	auto it = data->pEdictHash.find(edictIndex);
	if (it == data->pEdictHash.end())
		return 0;

	data->pEntities.push_back(ent);
	data->pEdictHash[edictIndex] = ent;

	return 0;
}

LUA_FUNCTION_STATIC(EntityList_Remove)
{
	EntityList* data = Get_EntityList(1, true);
	CBaseEntity* ent = Util::Get_Entity(2, true);
	edict_t* edict = ent->edict();

	if (!edict)
		return 0;

	short edictIndex = edict->m_EdictIndex;
	auto it = data->pEdictHash.find(edictIndex);
	if (it == data->pEdictHash.end())
		return 0;

	Vector_RemoveElement(data->pEntities, ent)
	data->pEdictHash.erase(edictIndex);

	return 0;
}

LUA_FUNCTION_STATIC(EntitiyList_Create)
{
	EntityList* pList = new EntityList;
	pEntityLists.push_back(pList);
	Push_EntityList(pList);
	return 1;
}

void CEntListModule::OnEdictFreed(const edict_t* edict)
{
	if (g_pEntListModule.InDebug())
		Msg("holylib: Removing edict (%i)\n", edict->m_EdictIndex);

	short serialNumber = edict->m_EdictIndex;
	for (EntityList* pList : pEntityLists)
	{
		auto it = pList->pEdictHash.find(serialNumber);
		if (it == pList->pEdictHash.end())
			continue;

		Vector_RemoveElement(pList->pEntities, it->second)
		pList->pEdictHash.erase(it);
	}
}

void CEntListModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	EntityList_TypeID = g_Lua->CreateMetaTable("EntityList");
		Util::AddFunc(EntityList__tostring, "__tostring");
		Util::AddFunc(EntityList__index, "__index");
		Util::AddFunc(EntityList__gc, "__gc");
		Util::AddFunc(EntityList_GetTable, "GetTable");
		Util::AddFunc(EntityList_SetTable, "SetTable");
		Util::AddFunc(EntityList_AddTable, "AddTable");
		Util::AddFunc(EntityList_RemoveTable, "RemoveTable");
		Util::AddFunc(EntityList_Add, "Add");
		Util::AddFunc(EntityList_Remove, "Remove");
	g_Lua->Pop(1);

	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		Util::AddFunc(EntitiyList_Create, "CreateEntityList");
	g_Lua->Pop(1);
}

void CEntListModule::LuaShutdown()
{
	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		Util::RemoveField("CreateEntityList");
	g_Lua->Pop(1);
}