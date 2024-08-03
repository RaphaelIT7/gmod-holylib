#pragma once

#include <GarrysMod/Lua/LuaInterface.h>
#include <GarrysMod/Symbol.hpp>
#include <detouring/hook.hpp>
#include <scanning/symbolfinder.hpp>
#include <GarrysMod/FactoryLoader.hpp>
#include <vector>
#include "filesystem.h"
#include <steam/isteamuser.h>
#include "Platform.hpp"

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
	const std::vector<Symbol> InitLuaClassesSym = {
		Symbol::FromName("_Z14InitLuaClassesP13ILuaInterface"),
		Symbol::FromSignature("\x48\x8B\x05\x59\x8B\xF9\x00"), // 48 8B 05 59 8B F9 00
	};

	typedef bool (*CLuaInterface_Shutdown)(GarrysMod::Lua::ILuaInterface*);
	const std::vector<Symbol> CLuaInterface_ShutdownSym = {
		Symbol::FromName("_ZN13CLuaInterface8ShutdownEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x54\x49\x89\xFC\x53\x4D\x8D"), // 55 48 89 E5 41 55 41 54 49 89 FC 53 4D 8D
	};

	typedef CBasePlayer* (*Get_Player)(int iStackPos, bool shouldError);
	const std::vector<Symbol> Get_PlayerSym = {
		Symbol::FromName("_Z10Get_Playerib"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x41\x89\xF4\x53\x40\x0F\xB6\xF6\xE8\xED\xF3"), // 55 48 89 E5 41 54 41 89 F4 53 40 0F B6 F6 E8 ED F3
	};

	typedef void (*Push_Entity)(CBaseEntity*);
	const std::vector<Symbol> Push_EntitySym = {
		Symbol::FromName("_Z11Push_EntityP11CBaseEntity"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x53\x48\x83\xEC\x20\x48\x8B\x1D"), // 55 48 89 E5 41 54 53 48 83 EC 20 48 8B 1D
	};

	typedef CBaseEntity* (*Get_Entity)(int iStackPos, bool shouldError);
	const std::vector<Symbol> Get_EntitySym = {
		Symbol::FromName("_Z10Get_Entityib"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x89\xF5\x41\x54\x41\x89\xFC\x53"), // 55 48 89 E5 41 55 41 89 F5 41 54 41 89 FC 53
	};

	//---------------------------------------------------------------------------------
	// Purpose: holylib Symbols
	//---------------------------------------------------------------------------------
	typedef bool (*CServerGameDLL_ShouldHideServer)();
	const std::vector<Symbol> CServerGameDLL_ShouldHideServerSym = {
		Symbol::FromName("_ZN14CServerGameDLL16ShouldHideServerEv"),
		Symbol::FromSignature("\x48\x8B\x3D\xC1\x9B\x18\x01\x55\x48\x89\xE5"), // 48 8B 3D C1 9B 18 01 55 48 89 E5
	};

	//---------------------------------------------------------------------------------
	// Purpose: gameevent Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> s_GameSystemsSym = {
		Symbol::FromName("_ZL13s_GameSystems"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: serverplugin Symbols
	//---------------------------------------------------------------------------------
	typedef void* (*CPlugin_Load)(void*, const char*);
	const std::vector<Symbol> CPlugin_LoadSym = {
		Symbol::FromName("_ZN7CPlugin4LoadEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x49\x89\xFE\x41\x55\x41\x54\x89\xF4\xBE\x2E"), // 55 48 89 E5 41 56 49 89 FE 41 55 41 54 49 89 F4 BE 2E
	};

	//---------------------------------------------------------------------------------
	// Purpose: sourcetv Symbols
	//---------------------------------------------------------------------------------
	typedef void* (*CHLTVClient_ProcessGMod_ClientToServer)(IClient*, void*);
	const std::vector<Symbol> CHLTVClient_ProcessGMod_ClientToServerSym = {
		Symbol::FromName("_ZN11CHLTVClient26ProcessGMod_ClientToServerEP23CLC_GMod_ClientToServer"),
	};

	typedef void (*CHLTVServer_CHLTVServer)(void*);
	const std::vector<Symbol> CHLTVServer_CHLTVServerSym = {
		Symbol::FromName("_ZN11CHLTVServerC2Ev"),
	};

	typedef void (*CHLTVServer_DestroyCHLTVServer)(void*);
	const std::vector<Symbol> CHLTVServer_DestroyCHLTVServerSym = {
		Symbol::FromName("_ZN11CHLTVServerD2Ev"),
	};

	typedef bool (*COM_IsValidPath)(const char*);
	const std::vector<Symbol> COM_IsValidPathSym = {
		Symbol::FromName("_Z15COM_IsValidPathPKc"),
	};

	typedef void (*CHLTVDemoRecorder_StartRecording)(void*, const char*, bool);
	const std::vector<Symbol> CHLTVDemoRecorder_StartRecordingSym = {
		Symbol::FromName("_ZN17CHLTVDemoRecorder14StartRecordingEPKcb"),
	};

	typedef void (*CHLTVDemoRecorder_StopRecording)(void*);
	const std::vector<Symbol> CHLTVDemoRecorder_StopRecordingSym = {
		Symbol::FromName("_ZN17CHLTVDemoRecorder13StopRecordingEv"),
	};

	typedef bool (*CHLTVClient_ExecuteStringCommand)(void*, const char*);
	const std::vector<Symbol> CHLTVClient_ExecuteStringCommandSym = {
		Symbol::FromName("_ZN11CHLTVClient20ExecuteStringCommandEPKc"),
	};

	typedef void (*CHLTVClient_Deconstructor)(void*);
	const std::vector<Symbol> CHLTVClient_DeconstructorSym = {
		Symbol::FromName("_ZN11CHLTVClientD1Ev"),
	};

	const std::vector<Symbol> UsermessagesSym = {
		Symbol::FromName("usermessages"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: threadpoolfix Symbols
	//---------------------------------------------------------------------------------
	typedef int (*CThreadPool_ExecuteToPriority)(void* pool, void* idk, void* idk2);
	const std::vector<Symbol> CThreadPool_ExecuteToPrioritySym = {
		Symbol::FromName("_ZN11CThreadPool17ExecuteToPriorityE13JobPriority_tPFbP4CJobE"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: precachefix Symbols
	//---------------------------------------------------------------------------------
	typedef int (*CVEngineServer_PrecacheModel)(void* engine, const char* mdl, bool preload);
	const std::vector<Symbol> CVEngineServer_PrecacheModelSym = {
		Symbol::FromName("_ZN14CVEngineServer13PrecacheModelEPKcb"),
	};

	typedef int (*CVEngineServer_PrecacheGeneric)(void* engine, const char* mdl, bool preload);
	const std::vector<Symbol> CVEngineServer_PrecacheGenericSym = {
		Symbol::FromName("_ZN14CVEngineServer15PrecacheGenericEPKcb"),
	};

	typedef int (*SV_FindOrAddModel)(const char* mdl, bool preload);
	const std::vector<Symbol> SV_FindOrAddModelSym = {
		Symbol::FromName("_Z17SV_FindOrAddModelPKcb"),
	};

	typedef int (*SV_FindOrAddGeneric)(const char* mdl, bool preload);
	const std::vector<Symbol> SV_FindOrAddGenericSym = {
		Symbol::FromName("_Z19SV_FindOrAddGenericPKcb"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: stringtable Symbols
	//---------------------------------------------------------------------------------
	typedef void (*CNetworkStringTable_DeleteAllStrings)(void* table);
	const std::vector<Symbol> CNetworkStringTable_DeleteAllStringsSym = {
		Symbol::FromName("_ZN19CNetworkStringTable16DeleteAllStringsEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x08\x48\x8B\x7F\x50"), // 55 48 89 E5 53 48 89 FB 48 83 EC 08 48 8B 7F 50
	};

	typedef void (*CNetworkStringTable_Deconstructor)(void* table);
	const std::vector<Symbol> CNetworkStringTable_DeconstructorSym = {
		Symbol::FromName("_ZN19CNetworkStringTableD0Ev"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x08\xE8\x8F\xFF\xFF\xFF\x48\x83\xC4\x08\x48\x89\xDF\x5B\x5D\xE9\x71\x96\xEE\xFF"), // 55 48 89 E5 53 48 89 FB 48 83 EC 08 E8 8F FF FF FF 48 83 C4 08 48 89 DF 5B 5D E9 71 96 EE FF
	};

	//---------------------------------------------------------------------------------
	// Purpose: surffix Symbols
	//---------------------------------------------------------------------------------
	typedef int (*CGameMovement_TryPlayerMove)(void* gamemovement, Vector*, void*);
	const std::vector<Symbol> CGameMovement_TryPlayerMoveSym = {
		Symbol::FromName("_ZN13CGameMovement13TryPlayerMoveEP6VectorP10CGameTrace"),
	};

	typedef int (*CGameMovement_ClipVelocity)(void* gamemovement, Vector&, Vector&, Vector&, float);
	const std::vector<Symbol> CGameMovement_ClipVelocitySym = {
		Symbol::FromName("_ZN13CGameMovement12ClipVelocityER6VectorS1_S1_f"),
	};

	typedef void* (*CBaseEntity_GetGroundEntity)(void* ent);
	const std::vector<Symbol> CBaseEntity_GetGroundEntitySym = {
		Symbol::FromName("_ZN11CBaseEntity15GetGroundEntityEv"),
	};

	typedef bool (*CTraceFilterSimple_ShouldHitEntity)(IHandleEntity*, int);
	const std::vector<Symbol> CTraceFilterSimple_ShouldHitEntitySym = {
		Symbol::FromName("_ZN18CTraceFilterSimple15ShouldHitEntityEP13IHandleEntityi"),
	};

	typedef void* (*MoveHelperServer)();
	const std::vector<Symbol> MoveHelperServerSym = {
		Symbol::FromName("_Z16MoveHelperServerv"),
	};

	const std::vector<Symbol> g_pEntityListSym = {
		Symbol::FromName("g_pEntityList"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: pvs Symbols
	//---------------------------------------------------------------------------------
	typedef int (*CGMOD_Player_SetupVisibility)(void* ent, unsigned char* pvs, int pvssize);
	const std::vector<Symbol> CGMOD_Player_SetupVisibilitySym = {
		Symbol::FromName("_ZN12CGMOD_Player15SetupVisibilityEP11CBaseEntityPhi"),
	};

	typedef void (*CServerGameEnts_CheckTransmit)(void* gameents, CCheckTransmitInfo*, const unsigned short*, int);
	const std::vector<Symbol> CServerGameEnts_CheckTransmitSym = {
		Symbol::FromName("_ZN15CServerGameEnts13CheckTransmitEP18CCheckTransmitInfoPKti"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: filesystem Symbols
	//---------------------------------------------------------------------------------
	typedef FileHandle_t* (*CBaseFileSystem_FindFileInSearchPath)(void* filesystem, CFileOpenInfo &);
	const std::vector<Symbol> CBaseFileSystem_FindFileInSearchPathSym = {
		Symbol::FromName("_ZN15CBaseFileSystem20FindFileInSearchPathER13CFileOpenInfo"),
	};

	typedef bool (*CBaseFileSystem_IsDirectory)(void* filesystem, const char* pFileName, const char* pathID);
	const std::vector<Symbol> CBaseFileSystem_IsDirectorySym = {
		Symbol::FromName("_ZN15CBaseFileSystem11IsDirectoryEPKcS1_"),
	};

	typedef CSearchPath* (*CBaseFileSystem_FindSearchPathByStoreId)(void* filesystem, int);
	const std::vector<Symbol> CBaseFileSystem_FindSearchPathByStoreIdSym = {
		Symbol::FromName("_ZN15CBaseFileSystem23FindSearchPathByStoreIdEi"),
	};

	typedef long (*CBaseFileSystem_FastFileTime)(void* filesystem, const CSearchPath* path, const char* pFileName);
	const std::vector<Symbol> CBaseFileSystem_FastFileTimeSym = {
		Symbol::FromName("_ZN15CBaseFileSystem12FastFileTimeEPKNS_11CSearchPathEPKc"),
	};

	typedef bool (*CBaseFileSystem_FixUpPath)(void* filesystem, const char *pFileName, char *pFixedUpFileName, int sizeFixedUpFileName);
	const std::vector<Symbol> CBaseFileSystem_FixUpPathSym = {
		Symbol::FromName("_ZN15CBaseFileSystem9FixUpPathEPKcPci"),
	};

	typedef FileHandle_t (*CBaseFileSystem_OpenForRead)(void* filesystem, const char *pFileNameT, const char *pOptions, unsigned flags, const char *pathID, char **ppszResolvedFilename);
	const std::vector<Symbol> CBaseFileSystem_OpenForReadSym = {
		Symbol::FromName("_ZN15CBaseFileSystem11OpenForReadEPKcS1_jS1_PPc"),
	};

	typedef long (*CBaseFileSystem_GetFileTime)(void* filesystem, const char *pFileName, const char *pPathID);
	const std::vector<Symbol> CBaseFileSystem_GetFileTimeSym = {
		Symbol::FromName("_ZN15CBaseFileSystem11GetFileTimeEPKcS1_"),
	};

	typedef void (*CBaseFileSystem_AddSearchPath)(void* filesystem, const char *pPath, const char *pathID, SearchPathAdd_t addType);
	const std::vector<Symbol> CBaseFileSystem_AddSearchPathSym = {
		Symbol::FromName("_ZN15CBaseFileSystem13AddSearchPathEPKcS1_j"),
	};

	typedef void (*CBaseFileSystem_AddVPKFile)(void* filesystem, const char *pPath, const char *pathID, SearchPathAdd_t addType);
	const std::vector<Symbol> CBaseFileSystem_AddVPKFileSym = {
		Symbol::FromName("_ZN15CBaseFileSystem10AddVPKFileEPKcS1_j"),
	};

	typedef void (*CBaseFileSystem_RemoveAllMapSearchPaths)(void* filesystem);
	const std::vector<Symbol> CBaseFileSystem_RemoveAllMapSearchPathsSym = {
		Symbol::FromName("_ZN15CBaseFileSystem23RemoveAllMapSearchPathsEv"),
	};

	const std::vector<Symbol> g_PathIDTableSym = {
		Symbol::FromName("g_PathIDTable"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: concommand Symbols
	//---------------------------------------------------------------------------------
	typedef bool (*ConCommand_IsBlocked)(const char* cmd);
	const std::vector<Symbol> ConCommand_IsBlockedSym = {
		Symbol::FromName("_Z20ConCommand_IsBlockedPKc"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: vprof Symbols
	//---------------------------------------------------------------------------------
	typedef void* (*CLuaGamemode_CallFinish)(void*, int);
	const std::vector<Symbol> CLuaGamemode_CallFinishSym = {
		Symbol::FromName("_ZN12CLuaGamemode10CallFinishEi"),
	};

	typedef void* (*CLuaGamemode_CallWithArgs)(void*, int);
	const std::vector<Symbol> CLuaGamemode_CallWithArgsSym = {
		Symbol::FromName("_ZN12CLuaGamemode12CallWithArgsEi"),
	};

	typedef void* (*CLuaGamemode_Call)(void*, int);
	const std::vector<Symbol> CLuaGamemode_CallSym = {
		Symbol::FromName("_ZN12CLuaGamemode4CallEi"),
	};

	typedef void (*CVProfile_OutputReport)(void*, int, const tchar*, int);
	const std::vector<Symbol> CVProfile_OutputReportSym = {
		Symbol::FromName("_ZN9CVProfile12OutputReportEiPKci"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: networking Symbols
	//---------------------------------------------------------------------------------
	typedef void* (*AllocChangeFrameList)(int, int);
	const std::vector<Symbol> AllocChangeFrameListSym = {
		Symbol::FromName("_Z20AllocChangeFrameListii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x54\x41\x89\xFC\xBF\x28"), // 55 48 89 E5 41 55 41 54 41 89 FC BF 28
	};

	//---------------------------------------------------------------------------------
	// Purpose: steamworks Symbols
	//---------------------------------------------------------------------------------
	typedef void (*CSteam3Server_OnLoggedOff)(void*, SteamServersDisconnected_t*);
	const std::vector<Symbol> CSteam3Server_OnLoggedOffSym = {
		Symbol::FromName("_ZN13CSteam3Server11OnLoggedOffEP26SteamServersDisconnected_t"),
		Symbol::FromSignature("\x83\xBF\x30****\x0F\x84\x7C***\x55\x48\x89\xE5"), // 83 BF 30 ?? ?? ?? ?? 0F 84 7C ?? ?? ?? 55 48 89 E5
	};

	typedef void (*CSteam3Server_OnLogonSuccess)(void*, SteamServersConnected_t*);
	const std::vector<Symbol> CSteam3Server_OnLogonSuccessSym = {
		Symbol::FromName("_ZN13CSteam3Server14OnLogonSuccessEP23SteamServersConnected_t"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x28\x64\x48\x8B"), // 55 48 89 E5 53 48 89 FB 48 83 EC 28 64 48 8B
	};

	typedef void (*CSteam3Server_Shutdown)(void*);
	const std::vector<Symbol> CSteam3Server_ShutdownSym = {
		Symbol::FromName("_ZN13CSteam3Server8ShutdownEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x08\x48\x83\x7F**\x74\x5A"), // 55 48 89 E5 53 48 89 FB 48 83 EC 08 48 83 7F ?? ?? 74 5A
	};

	typedef void (*CSteam3Server_Activate)(void*, int);
	const std::vector<Symbol> CSteam3Server_ActivateSym = {
		Symbol::FromName("_ZN13CSteam3Server8ActivateENS_11EServerTypeE"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x41\x89\xF4\x53\x48\x89\xFB\x48\x81\xEC") // 55 48 89 E5 41 57 41 56 41 55 41 54 41 89 F4 53 48 89 FB 48 81 EC
	};

	typedef CSteam3Server& (*Steam3ServerT)();
	const std::vector<Symbol> Steam3ServerSym = {
		Symbol::FromName("_Z12Steam3Serverv"),
		Symbol::FromSignature("\x55\x48\x8D\x05\x18\x8B\x4C\x00\x48\x89\xE5\x5D\xC3"), // 55 48 8D 05 18 8B 4C 00 48 89 E5 5D C3
	};

	//---------------------------------------------------------------------------------
	// Purpose: pas Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> g_BSPDataSym = {
		Symbol::FromName("g_BSPData"),
	};
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

	static SymbolFinder symbol_finder;
	template<class T>
	static inline T* ResolveSymbol(
		SourceSDK::FactoryLoader& loader, const Symbol& symbol
	)
	{
		if (symbol.type == Symbol::Type::None)
			return nullptr;

	#if defined SYSTEM_WINDOWS
		auto iface = reinterpret_cast<T**>(symbol_finder.Resolve(
			loader.GetModule(), symbol.name.c_str(), symbol.length
		));
		return iface != nullptr ? *iface : nullptr;
	#elif defined SYSTEM_POSIX
		return reinterpret_cast<T*>(symbol_finder.Resolve(
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
	static inline T* ResolveSymbol(
		SourceSDK::FactoryLoader& loader, const std::vector<Symbol>& symbols
	)
	{
		if ((symbol.size()-1) < DETOUR_SYMBOL_ID)
			return NULL;

		return ResolveSymbol<T>(loader, symbols[DETOUR_SYMBOL_ID]);
	}

	inline void* GetFunction(void* module, std::vector<Symbol> symbol)
	{
		if ((symbol.size()-1) < DETOUR_SYMBOL_ID)
			return NULL;

		return GetFunction(module, symbol[DETOUR_SYMBOL_ID]);
	}

	inline void Create(Detouring::Hook* hook, const char* name, void* module, std::vector<Symbol> symbol, void* func, unsigned int category)
	{
		if ((symbol.size()-1) < DETOUR_SYMBOL_ID)
		{
			CheckFunction(nullptr, name);
			return;
		}

		Create(hook, name, module, symbol[DETOUR_SYMBOL_ID], func, category);
	}
}