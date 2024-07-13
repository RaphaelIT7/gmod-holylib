#pragma once

#include <GarrysMod/Lua/LuaInterface.h>
#include <GarrysMod/Symbol.hpp>
#include <detouring/hook.hpp>
#include <scanning/symbolfinder.hpp>
#include <vector>

class CBaseEntity;
class CBasePlayer;
class IClient;
class IHandleEntity;
class CCheckTransmitInfo;

namespace Symbols
{
	//---------------------------------------------------------------------------------
	// Purpose: All Base Symbols
	//---------------------------------------------------------------------------------
	typedef bool (*InitLuaClasses)(GarrysMod::Lua::ILuaInterface*);
	const Symbol InitLuaClassesSym = Symbol::FromName("_Z14InitLuaClassesP13ILuaInterface");

	typedef CBasePlayer* (*Get_Player)(int, bool);
	const Symbol Get_PlayerSym = Symbol::FromName("_Z10Get_Playerib");

	typedef void (*Push_Entity)(CBaseEntity*);
	const Symbol Push_EntitySym = Symbol::FromName("_Z11Push_EntityP11CBaseEntity");

	//---------------------------------------------------------------------------------
	// Purpose: holylib Symbols
	//---------------------------------------------------------------------------------
	typedef bool (*CServerGameDLL_ShouldHideServer)();
	const Symbol CServerGameDLL_ShouldHideServerSym = Symbol::FromName("_ZN14CServerGameDLL16ShouldHideServerEv");

	//---------------------------------------------------------------------------------
	// Purpose: gameevent Symbols
	//---------------------------------------------------------------------------------
	const Symbol s_GameSystemsSym = Symbol::FromName("_ZL13s_GameSystems");

	//---------------------------------------------------------------------------------
	// Purpose: serverplugin Symbols
	//---------------------------------------------------------------------------------
	typedef void* (*CPlugin_Load)(void*, const char*);
	const Symbol CPlugin_LoadSym = Symbol::FromName("_ZN7CPlugin4LoadEPKc");

	//---------------------------------------------------------------------------------
	// Purpose: sourcetv Symbols
	//---------------------------------------------------------------------------------
	typedef void* (*CGameClient_ProcessGMod_ClientToServer)(IClient*, void*);
	const Symbol CGameClient_ProcessGMod_ClientToServerSym = Symbol::FromName("_ZN11CGameClient26ProcessGMod_ClientToServerEP23CLC_GMod_ClientToServer");

	//---------------------------------------------------------------------------------
	// Purpose: threadpoolfix Symbols
	//---------------------------------------------------------------------------------
	typedef int (*CThreadPool_ExecuteToPriority)(void* pool, void* idk, void* idk2);
	const Symbol CThreadPool_ExecuteToPrioritySym = Symbol::FromName("_ZN11CThreadPool17ExecuteToPriorityE13JobPriority_tPFbP4CJobE");

	//---------------------------------------------------------------------------------
	// Purpose: precachefix Symbols
	//---------------------------------------------------------------------------------
	typedef int (*CVEngineServer_PrecacheModel)(void* engine, const char* mdl, bool preload);
	const Symbol CVEngineServer_PrecacheModelSym = Symbol::FromName("_ZN14CVEngineServer13PrecacheModelEPKcb");

	typedef int (*CVEngineServer_PrecacheGeneric)(void* engine, const char* mdl, bool preload);
	const Symbol CVEngineServer_PrecacheGenericSym = Symbol::FromName("_ZN14CVEngineServer15PrecacheGenericEPKcb");

	typedef int (*SV_FindOrAddModel)(const char* mdl, bool preload);
	const Symbol SV_FindOrAddModelSym = Symbol::FromName("_Z17SV_FindOrAddModelPKcb");

	typedef int (*SV_FindOrAddGeneric)(const char* mdl, bool preload);
	const Symbol SV_FindOrAddGenericSym = Symbol::FromName("_Z19SV_FindOrAddGenericPKcb");

	//---------------------------------------------------------------------------------
	// Purpose: stringtable Symbols
	//---------------------------------------------------------------------------------
	typedef int (*CServerGameDLL_CreateNetworkStringTables)(void* servergamedll);
	const Symbol CServerGameDLL_CreateNetworkStringTablesSym = Symbol::FromName("_ZN14CServerGameDLL25CreateNetworkStringTablesEv");

	//---------------------------------------------------------------------------------
	// Purpose: surffix Symbols
	//---------------------------------------------------------------------------------
	typedef int (*CGameMovement_TryPlayerMove)(void* gamemovement, Vector*, void*);
	const Symbol CGameMovement_TryPlayerMoveSym = Symbol::FromName("_ZN13CGameMovement13TryPlayerMoveEP6VectorP10CGameTrace");

	typedef int (*CGameMovement_ClipVelocity)(void* gamemovement, Vector&, Vector&, Vector&, float);
	const Symbol CGameMovement_ClipVelocitySym = Symbol::FromName("_ZN13CGameMovement12ClipVelocityER6VectorS1_S1_f");

	typedef void* (*CBaseEntity_GetGroundEntity)(void* ent);
	const Symbol CBaseEntity_GetGroundEntitySym = Symbol::FromName("_ZN11CBaseEntity15GetGroundEntityEv");

	typedef bool (*CTraceFilterSimple_ShouldHitEntity)(IHandleEntity*, int);
	const Symbol CTraceFilterSimple_ShouldHitEntitySym = Symbol::FromName("_ZN18CTraceFilterSimple15ShouldHitEntityEP13IHandleEntityi");

	typedef void* (*MoveHelperServer)();
	const Symbol MoveHelperServerSym = Symbol::FromName("_Z16MoveHelperServerv");

	const Symbol g_pEntityListSym = Symbol::FromName("g_pEntityList");

	//---------------------------------------------------------------------------------
	// Purpose: pvs Symbols
	//---------------------------------------------------------------------------------
	typedef int (*CGMOD_Player_SetupVisibility)(void* ent, unsigned char* pvs, int pvssize);
	const Symbol CGMOD_Player_SetupVisibilitySym = Symbol::FromName("_ZN12CGMOD_Player15SetupVisibilityEP11CBaseEntityPhi");

	typedef void (*CServerGameEnts_CheckTransmit)(CCheckTransmitInfo*, const unsigned short*, int);
	const Symbol CServerGameEnts_CheckTransmitSym = Symbol::FromName("_ZN15CServerGameEnts13CheckTransmitEP18CCheckTransmitInfoPKti");
}

//---------------------------------------------------------------------------------
// Purpose: Detour functions
//---------------------------------------------------------------------------------
namespace Detour
{
	inline bool CheckValue(const char* msg, const char* name, bool ret)
	{
		if (!ret) {
			Msg("[holylib] Failed to %s %s!\n", msg, name);
			return false;
		}

		return true;
	}

	inline bool CheckValue(const char* name, bool ret)
	{
		return CheckValue("get function", name, ret);
	}

	template<class T>
	bool CheckFunction(T func, const char* name)
	{
		return CheckValue("get function", name, func != nullptr);
	}

	extern void* GetFunction(void* module, Symbol symbol);
	extern void Create(Detouring::Hook* hook, const char* name, void* module, Symbol symbol, void* func, unsigned int category);
	extern void Remove(unsigned int category); // 0 = All
	extern unsigned int g_pCurrentCategory;
}