#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"
#include <sourcesdk/cmodel_private.h>
#include "player.h"
#include <cmodel_engine.h>
#include "zone.h"

class CPASModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
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

LUA_FUNCTION_STATIC(pas_TestPAS) // This is based off SV_DetermineMulticastRecipients
{
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
	int cluster = CM_LeafCluster(CM_PointLeafnum(*orig));
	CM_Vis(g_pCurrentPAS, sizeof(g_pCurrentPAS), cluster, DVIS_PAS);

	int clusterIndex = CM_LeafCluster(CM_PointLeafnum(*hearPos));
	int offset = clusterIndex >> 3;
	if (offset > (int)sizeof(g_pCurrentPAS))
	{
		Warning("invalid offset? cluster would read past end of data");
		LUA->PushBool(false);
		return 1;
	}

#pragma warning(disable:6385) // We won't be reading junk, so stop telling me that I do.
	LUA->PushBool(!(g_pCurrentPAS[offset] & (1 << (clusterIndex & 7)))); // What was broken in my version? I used mask instead of pvs and I didn't use the offset.
	return 1;
}

LUA_FUNCTION_STATIC(pas_CheckBoxInPAS) // This is based off SV_DetermineMulticastRecipients
{
	LUA->CheckType(1, GarrysMod::Lua::Type::Vector);
	LUA->CheckType(2, GarrysMod::Lua::Type::Vector);
	LUA->CheckType(3, GarrysMod::Lua::Type::Vector);

	Vector* mins = Get_Vector(1);
	Vector* maxs = Get_Vector(2);
	Vector* orig = Get_Vector(3);

	ResetPAS();
	int cluster = CM_LeafCluster(CM_PointLeafnum(*orig));
	CM_Vis(g_pCurrentPAS, sizeof(g_pCurrentPAS), cluster, DVIS_PAS);

	LUA->PushBool(Util::engineserver->CheckBoxInPVS(*mins, *maxs, g_pCurrentPAS, sizeof(g_pCurrentPAS)));
	return 1;
}

void CPASModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
#ifdef ARCHITECTURE_X86_64
	CollisionBSPData_LinkPhysics();
	Memory_Init();
#endif
}

void CPASModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	Util::StartTable();
		Util::AddFunc(pas_TestPAS, "TestPAS");
		Util::AddFunc(pas_CheckBoxInPAS, "CheckBoxInPAS");
	Util::FinishTable("pas");

#ifdef ARCHITECTURE_X86_64
	std::string pMapName = "maps/";
	pMapName.append(STRING(gpGlobals->mapname));
	pMapName.append(".bsp");

	unsigned int checksum;
	CM_LoadMap(pMapName.c_str(), false, &checksum);
	// This will eat more memory since we need to also load the map while gmod already did this
	// which I hate, but It's easier than trying to find any way to get g_BSPData since it's not exposed anywhere :/
#endif
}

void CPASModule::LuaShutdown()
{
	Util::NukeTable("pas");
}

extern CCollisionBSPData g_BSPData;
void CPASModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

#ifndef ARCHITECTURE_X86_64
	SourceSDK::FactoryLoader engine_loader("engine");
	CCollisionBSPData* gBSPData = Detour::ResolveSymbol<CCollisionBSPData>(engine_loader, Symbols::g_BSPDataSym);
	Detour::CheckValue("get class", "CCollisionBSPData", gBSPData != NULL);
	g_BSPData = *gBSPData;
#endif
}

void CPASModule::Shutdown()
{
#ifdef ARCHITECTURE_X86_64
	Memory_Shutdown();
#endif
}