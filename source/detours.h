#pragma once

#include <GarrysMod/Lua/LuaInterface.h>
#include <GarrysMod/Symbol.hpp>
#include <detouring/hook.hpp>
#include <scanning/symbolfinder.hpp>
#include <vector>
#include "filesystem.h"

class CBaseEntity;
class CBasePlayer;
class IClient;
class IHandleEntity;
class CCheckTransmitInfo;
class CFileOpenInfo;
class CSearchPath;

namespace Symbols
{
	//---------------------------------------------------------------------------------
	// Purpose: All Base Symbols
	//---------------------------------------------------------------------------------
	typedef bool (*InitLuaClasses)(GarrysMod::Lua::ILuaInterface*);
	const Symbol InitLuaClassesSym = Symbol::FromName("_Z14InitLuaClassesP13ILuaInterface");

	typedef bool (*CLuaInterface_Shutdown)(GarrysMod::Lua::ILuaInterface*);
	const Symbol CLuaInterface_ShutdownSym = Symbol::FromName("_ZN13CLuaInterface8ShutdownEv");

	typedef CBasePlayer* (*Get_Player)(int, bool);
	const Symbol Get_PlayerSym = Symbol::FromName("_Z10Get_Playerib");

	typedef void (*Push_Entity)(CBaseEntity*);
	const Symbol Push_EntitySym = Symbol::FromName("_Z11Push_EntityP11CBaseEntity");

	typedef CBaseEntity* (*Get_Entity)(int, bool);
	const Symbol Get_EntitySym = Symbol::FromName("_Z10Get_Entityib");

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

	const Symbol hltvSym = Symbol::FromName("hltv");

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

	typedef void (*CServerGameEnts_CheckTransmit)(void* gameents, CCheckTransmitInfo*, const unsigned short*, int);
	const Symbol CServerGameEnts_CheckTransmitSym = Symbol::FromName("_ZN15CServerGameEnts13CheckTransmitEP18CCheckTransmitInfoPKti");

	//---------------------------------------------------------------------------------
	// Purpose: filesystem Symbols
	//---------------------------------------------------------------------------------
	typedef FileHandle_t* (*CBaseFileSystem_FindFileInSearchPath)(void* filesystem, CFileOpenInfo &);
	const Symbol CBaseFileSystem_FindFileInSearchPathSym = Symbol::FromName("_ZN15CBaseFileSystem20FindFileInSearchPathER13CFileOpenInfo");

	typedef bool (*CBaseFileSystem_IsDirectory)(void* filesystem, const char* pFileName, const char* pathID);
	const Symbol CBaseFileSystem_IsDirectorySym = Symbol::FromName("_ZN15CBaseFileSystem11IsDirectoryEPKcS1_");

	typedef CSearchPath* (*CBaseFileSystem_FindSearchPathByStoreId)(void* filesystem, int);
	const Symbol CBaseFileSystem_FindSearchPathByStoreIdSym = Symbol::FromName("_ZN15CBaseFileSystem23FindSearchPathByStoreIdEi");

	typedef long (*CBaseFileSystem_FastFileTime)(void* filesystem, const CSearchPath* path, const char* pFileName);
	const Symbol CBaseFileSystem_FastFileTimeSym = Symbol::FromName("_ZN15CBaseFileSystem12FastFileTimeEPKNS_11CSearchPathEPKc");

	typedef bool (*CBaseFileSystem_FixUpPath)(void* filesystem, const char *pFileName, char *pFixedUpFileName, int sizeFixedUpFileName);
	const Symbol CBaseFileSystem_FixUpPathSym = Symbol::FromName("_ZN15CBaseFileSystem9FixUpPathEPKcPci");

	typedef FileHandle_t (*CBaseFileSystem_OpenForRead)(void* filesystem, const char *pFileNameT, const char *pOptions, unsigned flags, const char *pathID, char **ppszResolvedFilename);
	const Symbol CBaseFileSystem_OpenForReadSym = Symbol::FromName("_ZN15CBaseFileSystem11OpenForReadEPKcS1_jS1_PPc");

	typedef long (*CBaseFileSystem_GetFileTime)(void* filesystem, const char *pFileName, const char *pPathID);
	const Symbol CBaseFileSystem_GetFileTimeSym = Symbol::FromName("_ZN15CBaseFileSystem11GetFileTimeEPKcS1_");

	typedef void (*CBaseFileSystem_AddSearchPath)(void* filesystem, const char *pPath, const char *pathID, SearchPathAdd_t addType);
	const Symbol CBaseFileSystem_AddSearchPathSym = Symbol::FromName("_ZN15CBaseFileSystem13AddSearchPathEPKcS1_j");

	typedef void (*CBaseFileSystem_AddVPKFile)(void* filesystem, const char *pPath, const char *pathID, SearchPathAdd_t addType);
	const Symbol CBaseFileSystem_AddVPKFileSym = Symbol::FromName("_ZN15CBaseFileSystem10AddVPKFileEPKcS1_j");

	typedef void (*CBaseFileSystem_RemoveAllMapSearchPaths)(void* filesystem);
	const Symbol CBaseFileSystem_RemoveAllMapSearchPathsSym = Symbol::FromName("_ZN15CBaseFileSystem23RemoveAllMapSearchPathsEv");

	typedef void (*Addon_FileSystem_MarkChanged)(void* filesystem);
	const Symbol Addon_FileSystem_MarkChangedSym = Symbol::FromName("_ZN5Addon10FileSystem11MarkChangedEv");

	const Symbol g_PathIDTableSym = Symbol::FromName("g_PathIDTable");

	//---------------------------------------------------------------------------------
	// Purpose: concommand Symbols
	//---------------------------------------------------------------------------------
	typedef bool (*ConCommand_IsBlocked)(const char* cmd);
	const Symbol ConCommand_IsBlockedSym = Symbol::FromName("_Z20ConCommand_IsBlockedPKc");

	//---------------------------------------------------------------------------------
	// Purpose: vprof Symbols
	//---------------------------------------------------------------------------------
	typedef void* (*CLuaGamemode_CallFinish)(void*, int);
	const Symbol CLuaGamemode_CallFinishSym = Symbol::FromName("_ZN12CLuaGamemode10CallFinishEi");

	typedef void* (*CLuaGamemode_CallWithArgs)(void*, int);
	const Symbol CLuaGamemode_CallWithArgsSym = Symbol::FromName("_ZN12CLuaGamemode12CallWithArgsEi");

	typedef void* (*CLuaGamemode_Call)(void*, int);
	const Symbol CLuaGamemode_CallSym = Symbol::FromName("_ZN12CLuaGamemode4CallEi");

	typedef void (*CVProfile_OutputReport)(void*, int, const tchar*, int);
	const Symbol CVProfile_OutputReportSym = Symbol::FromName("_ZN9CVProfile12OutputReportEiPKci");
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