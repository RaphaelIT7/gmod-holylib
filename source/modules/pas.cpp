#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"
#include <sourcesdk/cmodel_private.h>
#include "player.h"

class CPASModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "pas"; };
	virtual int Compatibility() { return LINUX32; };
};

extern Vector* Get_Vector(int iStackPos);

CPASModule g_pPASModule;
IModule* pPASModule = &g_pPASModule;

byte g_pCurrentPAS[MAX_MAP_LEAFS / 8];
inline void ResetPAS()
{
	Q_memset(g_pCurrentPAS, 0, sizeof(g_pCurrentPAS));
}

Symbols::CM_Vis func_CM_Vis = NULL;
LUA_FUNCTION_STATIC(pas_TestPAS) // This is based off SV_DetermineMulticastRecipients
{
	if (!func_CM_Vis)
		LUA->ThrowError("Failed to load CM_Vis!");

	Vector* orig;
	LUA->CheckType(1, GarrysMod::Lua::Type::Vector);
	//if (LUA->IsType(1, GarrysMod::Lua::Type::Vector))
	//{
	orig = Get_Vector(1);
	/*} else {
		LUA->CheckType(1, GarrysMod::Lua::Type::Entity);
		CBaseEntity* ent = Util::Get_Entity(1, false);
		if (!ent)
			LUA->ArgError(1, "Tried to use a NULL entity");

		orig = (Vector*)&ent->GetAbsOrigin(); // ToDo: This currently breaks the compile.
	}*/

	Vector* hearPos;
	LUA->CheckType(2, GarrysMod::Lua::Type::Vector);
	//if (LUA->IsType(2, GarrysMod::Lua::Type::Vector))
	//{
	hearPos = Get_Vector(2);
	/*} else {
		LUA->CheckType(2, GarrysMod::Lua::Type::Entity);
		CBaseEntity* ent = Util::Get_Entity(2, false);
		if (!ent)
			LUA->ArgError(2, "Tried to use a NULL entity");

		hearPos = (Vector*)&ent->GetAbsOrigin();
	}*/

	ResetPAS();
	func_CM_Vis(g_pCurrentPAS, sizeof(g_pCurrentPAS), engine->GetClusterForOrigin(*hearPos), DVIS_PAS);

	LUA->PushBool(Util::engineserver->CheckOriginInPVS(*hearPos, g_pCurrentPAS, sizeof(g_pCurrentPAS)));
	return 1;
}

LUA_FUNCTION_STATIC(pas_CheckBoxInPAS) // This is based off SV_DetermineMulticastRecipients
{
	if (!func_CM_Vis)
		LUA->ThrowError("Failed to load CM_Vis!");

	LUA->CheckType(1, GarrysMod::Lua::Type::Vector);
	LUA->CheckType(2, GarrysMod::Lua::Type::Vector);
	LUA->CheckType(3, GarrysMod::Lua::Type::Vector);

	Vector* mins = Get_Vector(1);
	Vector* maxs = Get_Vector(2);
	Vector* orig = Get_Vector(3);

	ResetPAS();
	func_CM_Vis(g_pCurrentPAS, sizeof(g_pCurrentPAS), engine->GetClusterForOrigin(*orig), DVIS_PAS);

	LUA->PushBool(Util::engineserver->CheckBoxInPVS(*mins, *maxs, g_pCurrentPAS, sizeof(g_pCurrentPAS)));
	return 1;
}

LUA_FUNCTION_STATIC(pas_FindInPAS)
{
	if (!func_CM_Vis)
		LUA->ThrowError("Failed to load CM_Vis!");

	Vector* orig;
	LUA->CheckType(1, GarrysMod::Lua::Type::Vector);
	if (LUA->IsType(1, GarrysMod::Lua::Type::Vector))
	{
		orig = Get_Vector(1);
	}
	else {
		LUA->CheckType(1, GarrysMod::Lua::Type::Entity);
		CBaseEntity* ent = Util::Get_Entity(1, false);
		if (!ent)
			LUA->ArgError(1, "Tried to use a NULL entity");

		orig = (Vector*)&ent->GetAbsOrigin(); // ToDo: This currently breaks the compile.
	}

	ResetPAS();
	func_CM_Vis(g_pCurrentPAS, sizeof(g_pCurrentPAS), engine->GetClusterForOrigin(*orig), DVIS_PAS);

	LUA->CreateTable();
	int idx = 0;
	CBaseEntity* pEnt = Util::entitylist->FirstEnt();
	while (pEnt != NULL)
	{
		int clusterIndex = engine->GetClusterForOrigin(pEnt->GetAbsOrigin());
		int offset = clusterIndex >> 3;
		if (offset > (int)sizeof(g_pCurrentPAS))
		{
			Warning("invalid offset? cluster would read past end of data");
			break;
		}

		if (!(g_pCurrentPAS[offset] & (1 << (clusterIndex & 7))))
		{
			++idx;
			LUA->PushNumber(idx);
			Util::Push_Entity(pEnt);
			LUA->SetTable(-3);
		}

		pEnt = Util::entitylist->NextEnt(pEnt);
	}

	return 1;
}

void CPASModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	Util::StartTable();
		Util::AddFunc(pas_TestPAS, "TestPAS");
		Util::AddFunc(pas_CheckBoxInPAS, "CheckBoxInPAS");
		Util::AddFunc(pas_FindInPAS, "FindInPAS");
	Util::FinishTable("pas");
}

void CPASModule::LuaShutdown()
{
	Util::NukeTable("pas");
}

void CPASModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::FactoryLoader engine_loader("engine");
	func_CM_Vis = (Symbols::CM_Vis)Detour::GetFunction(engine_loader.GetModule(), Symbols::CM_VisSym);
	Detour::CheckFunction((void*)func_CM_Vis, "CM_Vis");
}