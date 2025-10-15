#include "filesystem_base.h" // Has to be before symbols.h
#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CPASModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual const char* Name() { return "pas"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
	virtual bool SupportsMultipleLuaStates() { return true; };
};

CPASModule g_pPASModule;
IModule* pPASModule = &g_pPASModule;

inline bool TestPAS(Util::VisData* pVisData, const Vector& hearPos)
{
	return Util::engineserver->CheckOriginInPVS(hearPos, pVisData->cluster, sizeof(pVisData->cluster));
}

LUA_FUNCTION_STATIC(pas_TestPAS)
{
	Vector* orig;
	LUA->CheckType(1, GarrysMod::Lua::Type::Vector);
	if (LUA->IsType(1, GarrysMod::Lua::Type::Vector))
	{
		orig = Get_Vector(LUA, 1);
	} else {
		CBaseEntity* ent = Util::Get_Entity(LUA, 1, false);

		orig = (Vector*)&ent->GetAbsOrigin();
	}

	Util::VisData* pVisCluster = Util::CM_Vis(*orig, DVIS_PAS);

	LUA->CheckType(2, GarrysMod::Lua::Type::Vector);
	if (LUA->IsType(2, GarrysMod::Lua::Type::Vector))
	{
		LUA->PushBool(TestPAS(pVisCluster, *Get_Vector(LUA, 2)));
#if MODULE_EXISTS_ENTITYLIST
	} else if (Is_EntityList(LUA, 2)) {
		LUA->CreateTable();
		EntityList* entList = Get_EntityList(LUA, 2, true);
		for (auto& [pEnt, iReference]: entList->GetReferences())
		{
			entList->PushReference(pEnt, iReference);
			LUA->PushBool(TestPAS(pVisCluster, pEnt->GetAbsOrigin()));
			LUA->RawSet(-3);
		}
#endif
	} else {
		LUA->CheckType(2, GarrysMod::Lua::Type::Entity);
		CBaseEntity* ent = Util::Get_Entity(LUA, 2, false);

		LUA->PushBool(TestPAS(pVisCluster, ent->GetAbsOrigin()));
	}
	delete pVisCluster;

	return 1;
}

LUA_FUNCTION_STATIC(pas_CheckBoxInPAS)
{
	Vector* mins = Get_Vector(LUA, 1, true);
	Vector* maxs = Get_Vector(LUA, 2, true);
	Vector* orig = Get_Vector(LUA, 3, true);

	Util::VisData* pVisCluster = Util::CM_Vis(*orig, DVIS_PAS);

	LUA->PushBool(Util::engineserver->CheckBoxInPVS(*mins, *maxs, pVisCluster->cluster, sizeof(pVisCluster->cluster)));
	delete pVisCluster;

	return 1;
}

LUA_FUNCTION_STATIC(pas_FindInPAS)
{
	VPROF_BUDGET("pas.FindInPAS", VPROF_BUDGETGROUP_HOLYLIB);

	Vector* orig;
	if (LUA->IsType(1, GarrysMod::Lua::Type::Vector))
	{
		orig = Get_Vector(LUA, 1, true);
	} else {
		CBaseEntity* ent = Util::Get_Entity(LUA, 1, true);
		orig = (Vector*)&ent->GetAbsOrigin();
	}

	Util::VisData* pVisCluster = Util::CM_Vis(*orig, DVIS_PAS);

	LUA->PreCreateTable(MAX_EDICTS / 16, 0);
	int idx = 0;
#if MODULE_EXISTS_ENTITYLIST
	if (Util::pEntityList->IsEnabled())
	{
		EntityList& pGlobalEntityList = GetGlobalEntityList(LUA);
		for (auto& [pEnt, iReference] : pGlobalEntityList.GetReferences())
		{
			if (Util::engineserver->CheckOriginInPVS(pEnt->GetAbsOrigin(), pVisCluster->cluster, sizeof(pVisCluster->cluster)))
			{
				// Since it should be a bit more rare that ALL entities are pushed we don't directly loop thru the map itself to benefit from the vector's performance.
				pGlobalEntityList.PushReference(pEnt, iReference);
				Util::RawSetI(LUA, -2, ++idx);
			}
		}
		delete pVisCluster;
		return 1;
	}
#endif

	CBaseEntity* pEnt = Util::FirstEnt();
	while (pEnt != NULL)
	{
		if (Util::engineserver->CheckOriginInPVS(pEnt->GetAbsOrigin(), pVisCluster->cluster, sizeof(pVisCluster->cluster)))
		{
			Util::Push_Entity(LUA, pEnt);
			Util::RawSetI(LUA, -2, ++idx);
		}

		pEnt = Util::NextEnt(pEnt);
	}

	delete pVisCluster;
	return 1;
}

void CPASModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Util::StartTable(pLua);
		Util::AddFunc(pLua, pas_TestPAS, "TestPAS");
		Util::AddFunc(pLua, pas_CheckBoxInPAS, "CheckBoxInPAS");
		Util::AddFunc(pLua, pas_FindInPAS, "FindInPAS");
	Util::FinishTable(pLua, "pas");
}

void CPASModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "pas");
}