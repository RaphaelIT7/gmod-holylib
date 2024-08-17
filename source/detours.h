#pragma once

#include <GarrysMod/Lua/LuaInterface.h>
#include <GarrysMod/Symbol.hpp>
#include <detouring/hook.hpp>
#include <GarrysMod/FactoryLoader.hpp>
#include <vector>
#include "filesystem.h"
#include <steam/isteamuser.h>
#include "Platform.hpp"
#include <scanning/symbolfinder.hpp>

class CBaseEntity;
class CBasePlayer;
class IClient;
class IHandleEntity;
class CCheckTransmitInfo;
class CFileOpenInfo;
class CSearchPath;
class CSteam3Server;

/*
 * The symbols will have this order:
 * 0 - Linux 32x
 * 1 - Linux 64x
 * 2 - Windows 32x
 * 3 - Windows 64x
 */

namespace Symbols
{
	//---------------------------------------------------------------------------------
	// Purpose: All Base Symbols
	//---------------------------------------------------------------------------------
	typedef bool (*InitLuaClasses)(GarrysMod::Lua::ILuaInterface*);
	extern const std::vector<Symbol> InitLuaClassesSym;

	typedef bool (*CLuaInterface_Shutdown)(GarrysMod::Lua::ILuaInterface*);
	extern const std::vector<Symbol> CLuaInterface_ShutdownSym;

	typedef CBasePlayer* (*Get_Player)(int iStackPos, bool shouldError);
	extern const std::vector<Symbol> Get_PlayerSym;

	typedef void (*Push_Entity)(CBaseEntity*);
	extern const std::vector<Symbol> Push_EntitySym;

	typedef CBaseEntity* (*Get_Entity)(int iStackPos, bool shouldError);
	extern const std::vector<Symbol> Get_EntitySym;

	//---------------------------------------------------------------------------------
	// Purpose: holylib Symbols
	//---------------------------------------------------------------------------------
	typedef bool (*CServerGameDLL_ShouldHideServer)();
	extern const std::vector<Symbol> CServerGameDLL_ShouldHideServerSym;

	//---------------------------------------------------------------------------------
	// Purpose: gameevent Symbols
	//---------------------------------------------------------------------------------
	extern const std::vector<Symbol> s_GameSystemsSym;

	typedef bool (*CBaseClient_ProcessListenEvents)(void* client, void* msg);
	extern const std::vector<Symbol> CBaseClient_ProcessListenEventsSym;

	typedef bool (*CGameEventManager_AddListener)(void* manager, void* listener, void* descriptor, int);
	extern const std::vector<Symbol> CGameEventManager_AddListenerSym;

	//---------------------------------------------------------------------------------
	// Purpose: serverplugin Symbols
	//---------------------------------------------------------------------------------
	typedef void* (*CPlugin_Load)(void*, const char*);
	extern const std::vector<Symbol> CPlugin_LoadSym;

	//---------------------------------------------------------------------------------
	// Purpose: sourcetv Symbols
	//---------------------------------------------------------------------------------
	typedef void* (*CHLTVClient_ProcessGMod_ClientToServer)(IClient*, void*);
	extern const std::vector<Symbol> CHLTVClient_ProcessGMod_ClientToServerSym;

	typedef void (*CHLTVServer_CHLTVServer)(void*);
	extern const std::vector<Symbol> CHLTVServer_CHLTVServerSym;

	typedef void (*CHLTVServer_DestroyCHLTVServer)(void*);
	extern const std::vector<Symbol> CHLTVServer_DestroyCHLTVServerSym;

	typedef bool (*COM_IsValidPath)(const char*);
	extern const std::vector<Symbol> COM_IsValidPathSym;

	typedef void (*CHLTVDemoRecorder_StartRecording)(void*, const char*, bool);
	extern const std::vector<Symbol> CHLTVDemoRecorder_StartRecordingSym;

	typedef void (*CHLTVDemoRecorder_StopRecording)(void*);
	extern const std::vector<Symbol> CHLTVDemoRecorder_StopRecordingSym;

	typedef bool (*CHLTVClient_ExecuteStringCommand)(void*, const char*);
	extern const std::vector<Symbol> CHLTVClient_ExecuteStringCommandSym;

	typedef void (*CHLTVClient_Deconstructor)(void*);
	extern const std::vector<Symbol> CHLTVClient_DeconstructorSym;

	extern const std::vector<Symbol> UsermessagesSym;

	//---------------------------------------------------------------------------------
	// Purpose: threadpoolfix Symbols
	//---------------------------------------------------------------------------------
	typedef int (*CThreadPool_ExecuteToPriority)(void* pool, void* idk, void* idk2);
	extern const std::vector<Symbol> CThreadPool_ExecuteToPrioritySym;

	//---------------------------------------------------------------------------------
	// Purpose: precachefix Symbols
	//---------------------------------------------------------------------------------
	typedef int (*CVEngineServer_PrecacheModel)(void* engine, const char* mdl, bool preload);
	extern const std::vector<Symbol> CVEngineServer_PrecacheModelSym;

	typedef int (*CVEngineServer_PrecacheGeneric)(void* engine, const char* mdl, bool preload);
	extern const std::vector<Symbol> CVEngineServer_PrecacheGenericSym;

	typedef int (*SV_FindOrAddModel)(const char* mdl, bool preload);
	extern const std::vector<Symbol> SV_FindOrAddModelSym;

	typedef int (*SV_FindOrAddGeneric)(const char* mdl, bool preload);
	extern const std::vector<Symbol> SV_FindOrAddGenericSym;

	//---------------------------------------------------------------------------------
	// Purpose: stringtable Symbols
	//---------------------------------------------------------------------------------
	typedef void (*CNetworkStringTable_DeleteAllStrings)(void* table);
	extern const std::vector<Symbol> CNetworkStringTable_DeleteAllStringsSym;

	typedef void (*CNetworkStringTable_Deconstructor)(void* table);
	extern const std::vector<Symbol> CNetworkStringTable_DeconstructorSym;

	//---------------------------------------------------------------------------------
	// Purpose: surffix Symbols
	//---------------------------------------------------------------------------------
	typedef int (*CGameMovement_TryPlayerMove)(void* gamemovement, Vector*, void*);
	extern const std::vector<Symbol> CGameMovement_TryPlayerMoveSym;

	typedef int (*CGameMovement_ClipVelocity)(void* gamemovement, Vector&, Vector&, Vector&, float);
	extern const std::vector<Symbol> CGameMovement_ClipVelocitySym;

	typedef void* (*CBaseEntity_GetGroundEntity)(void* ent);
	extern const std::vector<Symbol> CBaseEntity_GetGroundEntitySym;

	typedef bool (*CTraceFilterSimple_ShouldHitEntity)(void* tr, IHandleEntity*, int);
	extern const std::vector<Symbol> CTraceFilterSimple_ShouldHitEntitySym;

	typedef void* (*MoveHelperServer)();
	extern const std::vector<Symbol> MoveHelperServerSym;

	extern const std::vector<Symbol> g_pEntityListSym;

	//---------------------------------------------------------------------------------
	// Purpose: pvs Symbols
	//---------------------------------------------------------------------------------
	typedef int (*CGMOD_Player_SetupVisibility)(void* ent, unsigned char* pvs, int pvssize);
	extern const std::vector<Symbol> CGMOD_Player_SetupVisibilitySym;

	typedef void (*CServerGameEnts_CheckTransmit)(void* gameents, CCheckTransmitInfo*, const unsigned short*, int);
	extern const std::vector<Symbol> CServerGameEnts_CheckTransmitSym;

	//---------------------------------------------------------------------------------
	// Purpose: filesystem Symbols
	//---------------------------------------------------------------------------------
	typedef FileHandle_t* (*CBaseFileSystem_FindFileInSearchPath)(void* filesystem, CFileOpenInfo &);
	extern const std::vector<Symbol> CBaseFileSystem_FindFileInSearchPathSym;

	typedef bool (*CBaseFileSystem_IsDirectory)(void* filesystem, const char* pFileName, const char* pathID);
	extern const std::vector<Symbol> CBaseFileSystem_IsDirectorySym;

	typedef CSearchPath* (*CBaseFileSystem_FindSearchPathByStoreId)(void* filesystem, int);
	extern const std::vector<Symbol> CBaseFileSystem_FindSearchPathByStoreIdSym;

	typedef long (*CBaseFileSystem_FastFileTime)(void* filesystem, const CSearchPath* path, const char* pFileName);
	extern const std::vector<Symbol> CBaseFileSystem_FastFileTimeSym;

	typedef bool (*CBaseFileSystem_FixUpPath)(void* filesystem, const char *pFileName, char *pFixedUpFileName, int sizeFixedUpFileName);
	extern const std::vector<Symbol> CBaseFileSystem_FixUpPathSym;

	typedef FileHandle_t (*CBaseFileSystem_OpenForRead)(void* filesystem, const char *pFileNameT, const char *pOptions, unsigned flags, const char *pathID, char **ppszResolvedFilename);
	extern std::vector<Symbol> CBaseFileSystem_OpenForReadSym;

	typedef long (*CBaseFileSystem_GetFileTime)(void* filesystem, const char *pFileName, const char *pPathID);
	extern const std::vector<Symbol> CBaseFileSystem_GetFileTimeSym;

	typedef void (*CBaseFileSystem_AddSearchPath)(void* filesystem, const char *pPath, const char *pathID, SearchPathAdd_t addType);
	extern const std::vector<Symbol> CBaseFileSystem_AddSearchPathSym;

	typedef void (*CBaseFileSystem_AddVPKFile)(void* filesystem, const char *pPath, const char *pathID, SearchPathAdd_t addType);
	extern const std::vector<Symbol> CBaseFileSystem_AddVPKFileSym;

	typedef void (*CBaseFileSystem_RemoveAllMapSearchPaths)(void* filesystem);
	extern const std::vector<Symbol> CBaseFileSystem_RemoveAllMapSearchPathsSym;

	typedef void (*CBaseFileSystem_Close)(void* filesystem, FileHandle_t);
	extern const std::vector<Symbol> CBaseFileSystem_CloseSym;

	typedef const char* (*CBaseFileSystem_CSearchPath_GetDebugString)(void* searchpath);
	extern const std::vector<Symbol> CBaseFileSystem_CSearchPath_GetDebugStringSym;


	//---------------------------------------------------------------------------------
	// Purpose: concommand Symbols
	//---------------------------------------------------------------------------------
	typedef bool (*ConCommand_IsBlocked)(const char* cmd);
	extern const std::vector<Symbol> ConCommand_IsBlockedSym;

	//---------------------------------------------------------------------------------
	// Purpose: vprof Symbols
	//---------------------------------------------------------------------------------
	//typedef void* (*CLuaGamemode_CallFinish)(void*, int);
	//extern const std::vector<Symbol> CLuaGamemode_CallFinishSym;

	typedef void* (*CLuaGamemode_CallWithArgs)(void*, int);
	extern const std::vector<Symbol> CLuaGamemode_CallWithArgsSym;

	typedef void* (*CLuaGamemode_Call)(void*, int);
	extern const std::vector<Symbol> CLuaGamemode_CallSym;

	typedef void (*CVProfile_OutputReport)(void*, int, const tchar*, int);
	extern const std::vector<Symbol> CVProfile_OutputReportSym;

	typedef void* (*CScriptedEntity_StartFunction1)(void*, const char*);
	extern const std::vector<Symbol> CScriptedEntity_StartFunction1Sym;

	typedef void* (*CScriptedEntity_StartFunction2)(void*, int);
	extern const std::vector<Symbol> CScriptedEntity_StartFunction2Sym;

	typedef void* (*CScriptedEntity_Call)(void*, int, int);
	extern const std::vector<Symbol> CScriptedEntity_CallSym;

	typedef void* (*CScriptedEntity_CallFunction1)(void*, const char*);
	extern const std::vector<Symbol> CScriptedEntity_CallFunction1Sym;

	typedef void* (*CScriptedEntity_CallFunction2)(void*, int);
	extern const std::vector<Symbol> CScriptedEntity_CallFunction2Sym;

	//---------------------------------------------------------------------------------
	// Purpose: networking Symbols
	//---------------------------------------------------------------------------------
	typedef void* (*AllocChangeFrameList)(int, int);
	extern const std::vector<Symbol> AllocChangeFrameListSym;

	//---------------------------------------------------------------------------------
	// Purpose: steamworks Symbols
	//---------------------------------------------------------------------------------
	typedef void (*CSteam3Server_OnLoggedOff)(void*, SteamServersDisconnected_t*);
	extern const std::vector<Symbol> CSteam3Server_OnLoggedOffSym;

	typedef void (*CSteam3Server_OnLogonSuccess)(void*, SteamServersConnected_t*);
	extern const std::vector<Symbol> CSteam3Server_OnLogonSuccessSym;

	typedef void (*CSteam3Server_Shutdown)(void*);
	extern const std::vector<Symbol> CSteam3Server_ShutdownSym;

	typedef void (*CSteam3Server_Activate)(void*, int);
	extern const std::vector<Symbol> CSteam3Server_ActivateSym;

	typedef CSteam3Server& (*Steam3ServerT)();
	extern const std::vector<Symbol> Steam3ServerSym;

	//---------------------------------------------------------------------------------
	// Purpose: pas Symbols
	//---------------------------------------------------------------------------------
	extern const std::vector<Symbol> g_BSPDataSym;
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

	inline bool CheckFunction(void* func, const char* name)
	{
		return CheckValue("get function", name, func != nullptr);
	}

	extern void* GetFunction(void* module, Symbol symbol);
	extern void Create(Detouring::Hook* hook, const char* name, void* module, Symbol symbol, void* func, unsigned int category);
	extern void Remove(unsigned int category); // 0 = All
	extern unsigned int g_pCurrentCategory;

	extern SymbolFinder symfinder;
	template<class T>
	inline T* ResolveSymbol(
		SourceSDK::FactoryLoader& loader, const Symbol& symbol
	)
	{
		if (symbol.type == Symbol::Type::None)
			return nullptr;

	#if defined SYSTEM_WINDOWS
		auto iface = reinterpret_cast<T**>(symfinder.Resolve(
			loader.GetModule(), symbol.name.c_str(), symbol.length
		));
		return iface != nullptr ? *iface : nullptr;
	#elif defined SYSTEM_POSIX
		return reinterpret_cast<T*>(symfinder.Resolve(
			loader.GetModule(), symbol.name.c_str(), symbol.length
		));
	#endif
	}

#ifdef SYSTEM_IS_LINUX
#ifndef ARCHITECTURE_X86_64
#define DETOUR_SYMBOL_ID 0
#else
#define DETOUR_SYMBOL_ID 1
#endif
#else
#ifndef ARCHITECTURE_X86_64
#define DETOUR_SYMBOL_ID 2
#else
#define DETOUR_SYMBOL_ID 3
#endif
#endif

	template<class T>
	inline T* ResolveSymbol(
		SourceSDK::FactoryLoader& loader, const std::vector<Symbol>& symbols
	)
	{
	#if DETOUR_SYMBOL_ID != 0
		if ((symbols.size()-1) < DETOUR_SYMBOL_ID)
			return NULL;
	#endif

	#if defined SYSTEM_WINDOWS
		auto iface = reinterpret_cast<T**>(symfinder.Resolve(
			loader.GetModule(), symbols[DETOUR_SYMBOL_ID].name.c_str(), symbols[DETOUR_SYMBOL_ID].length
		));
		return iface != nullptr ? *iface : nullptr;
	#elif defined SYSTEM_POSIX
		return reinterpret_cast<T*>(symfinder.Resolve(
			loader.GetModule(), symbols[DETOUR_SYMBOL_ID].name.c_str(), symbols[DETOUR_SYMBOL_ID].length
		));
	#endif
	}

	inline void* GetFunction(void* module, std::vector<Symbol> symbols)
	{
#if DETOUR_SYMBOL_ID != 0
		if ((symbols.size()-1) < DETOUR_SYMBOL_ID)
			return NULL;
#endif

		return GetFunction(module, symbols[DETOUR_SYMBOL_ID]);
	}

	inline void Create(Detouring::Hook* hook, const char* name, void* module, std::vector<Symbol> symbols, void* func, unsigned int category)
	{
#if DETOUR_SYMBOL_ID != 0
		if ((symbols.size()-1) < DETOUR_SYMBOL_ID)
		{
			CheckFunction(nullptr, name);
			return;
		}
#endif

		Create(hook, name, module, symbols[DETOUR_SYMBOL_ID], func, category);
	}
}