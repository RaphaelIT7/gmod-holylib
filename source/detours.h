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
#include "sourcesdk/ivp_time.h"

class CBaseEntity;
class CBasePlayer;
class IClient;
class IHandleEntity;
class CCheckTransmitInfo;
class CFileOpenInfo;
class CSearchPath;
class CSteam3Server;
class IChangeFrameList;
class IGameEvent;

class IVP_Mindist;
class IVP_Real_Object;
class IVP_Synapse;
class IVP_Environment;
class IVP_Core;

struct ThreadPoolStartParams_t;

/*
 * The symbols will have this order:
 * 0 - Linux 32x
 * 1 - Linux 64x
 * 2 - Windows 32x
 * 3 - Windows 64x
 */

#if defined SYSTEM_WINDOWS
#if defined ARCHITECTURE_X86_64
#define GMCOMMON_CALLING_CONVENTION __fastcall
#else
#define GMCOMMON_CALLING_CONVENTION __thiscall
#endif
#else
#define GMCOMMON_CALLING_CONVENTION
#endif

/*
 * I need to figure out how to hook into shit without breaking it on windows.
 * Currently every single hook seems to break gmod and every call to a hook seems broken.
 * It's probably some shit with calling convention or so but I have no fking Idea how to solve that :<
 * If someone knows how I could fix this, please let me know.
 */

namespace Symbols
{
	//---------------------------------------------------------------------------------
	// Purpose: All Base Symbols
	//---------------------------------------------------------------------------------
	typedef bool (*InitLuaClasses)(GarrysMod::Lua::ILuaInterface*);
	extern const std::vector<Symbol> InitLuaClassesSym;

	typedef bool (GMCOMMON_CALLING_CONVENTION* CLuaInterface_Shutdown)(GarrysMod::Lua::ILuaInterface*);
	extern const std::vector<Symbol> CLuaInterface_ShutdownSym;

	typedef CBasePlayer* (*Get_Player)(int iStackPos, bool shouldError);
	extern const std::vector<Symbol> Get_PlayerSym;

	typedef void (*Push_Entity)(CBaseEntity* ent);
	extern const std::vector<Symbol> Push_EntitySym;

	typedef CBaseEntity* (*Get_Entity)(int iStackPos, bool shouldError);
	extern const std::vector<Symbol> Get_EntitySym;

	typedef void (*CBaseEntity_CalcAbsolutePosition)(void* ent);
	extern const std::vector<Symbol> CBaseEntity_CalcAbsolutePositionSym;

	typedef void (*CBaseAnimating_InvalidateBoneCache)(void* ent);
	extern const std::vector<Symbol> CBaseAnimating_InvalidateBoneCacheSym;

	typedef void (*CBaseEntity_PostConstructor)(void* ent, const char* szClassname);
	extern const std::vector<Symbol> CBaseEntity_PostConstructorSym;

	//---------------------------------------------------------------------------------
	// Purpose: holylib Symbols
	//---------------------------------------------------------------------------------
	typedef bool (GMCOMMON_CALLING_CONVENTION* CServerGameDLL_ShouldHideServer)();
	extern const std::vector<Symbol> CServerGameDLL_ShouldHideServerSym;
	
	typedef void (GMCOMMON_CALLING_CONVENTION* GetGModServerTags)(char* pDest, int iMaxSize, bool unknown);
	extern const std::vector<Symbol> GetGModServerTagsSym;

	//---------------------------------------------------------------------------------
	// Purpose: gameevent Symbols
	//---------------------------------------------------------------------------------
	typedef bool (GMCOMMON_CALLING_CONVENTION* CBaseClient_ProcessListenEvents)(void* client, void* msg);
	extern const std::vector<Symbol> CBaseClient_ProcessListenEventsSym;

	typedef bool (GMCOMMON_CALLING_CONVENTION* CGameEventManager_AddListener)(void* manager, void* listener, void* descriptor, int);
	extern const std::vector<Symbol> CGameEventManager_AddListenerSym;

	typedef IGameEvent* (GMCOMMON_CALLING_CONVENTION* CGameEventManager_CreateEvent)(void* manager, const char* name, bool bForce);
	extern const std::vector<Symbol> CGameEventManager_CreateEventSym;

	//---------------------------------------------------------------------------------
	// Purpose: serverplugin Symbols
	//---------------------------------------------------------------------------------
	typedef void* (GMCOMMON_CALLING_CONVENTION* CPlugin_Load)(void*, const char*);
	extern const std::vector<Symbol> CPlugin_LoadSym;

	//---------------------------------------------------------------------------------
	// Purpose: sourcetv Symbols
	//---------------------------------------------------------------------------------
	typedef void* (GMCOMMON_CALLING_CONVENTION* CHLTVClient_ProcessGMod_ClientToServer)(IClient*, void*);
	extern const std::vector<Symbol> CHLTVClient_ProcessGMod_ClientToServerSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CHLTVServer_CHLTVServer)(void*);
	extern const std::vector<Symbol> CHLTVServer_CHLTVServerSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CHLTVServer_DestroyCHLTVServer)(void*);
	extern const std::vector<Symbol> CHLTVServer_DestroyCHLTVServerSym;

	typedef bool (GMCOMMON_CALLING_CONVENTION* COM_IsValidPath)(const char*);
	extern const std::vector<Symbol> COM_IsValidPathSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CHLTVDemoRecorder_StartRecording)(void*, const char*, bool);
	extern const std::vector<Symbol> CHLTVDemoRecorder_StartRecordingSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CHLTVDemoRecorder_StopRecording)(void*);
	extern const std::vector<Symbol> CHLTVDemoRecorder_StopRecordingSym;

	typedef bool (GMCOMMON_CALLING_CONVENTION* CHLTVClient_ExecuteStringCommand)(void*, const char*);
	extern const std::vector<Symbol> CHLTVClient_ExecuteStringCommandSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CHLTVClient_Deconstructor)(void*);
	extern const std::vector<Symbol> CHLTVClient_DeconstructorSym;

	extern const std::vector<Symbol> UsermessagesSym;

	//---------------------------------------------------------------------------------
	// Purpose: threadpoolfix Symbols
	//---------------------------------------------------------------------------------
	typedef int (GMCOMMON_CALLING_CONVENTION* CThreadPool_ExecuteToPriority)(void* pool, void* idk, void* idk2);
	extern const std::vector<Symbol> CThreadPool_ExecuteToPrioritySym;

	typedef bool (GMCOMMON_CALLING_CONVENTION* CThreadPool_Start)(void* pool, const ThreadPoolStartParams_t& params, const char* pszName);
	extern const std::vector<Symbol> CThreadPool_StartSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_InitAsync)(void* filesystem);
	extern const std::vector<Symbol> CBaseFileSystem_InitAsyncSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_ShutdownAsync)(void* filesystem);
	extern const std::vector<Symbol> CBaseFileSystem_ShutdownAsyncSym;

	//---------------------------------------------------------------------------------
	// Purpose: precachefix Symbols
	//---------------------------------------------------------------------------------
	typedef int (GMCOMMON_CALLING_CONVENTION* CVEngineServer_PrecacheModel)(void* engine, const char* mdl, bool preload);
	extern const std::vector<Symbol> CVEngineServer_PrecacheModelSym;

	typedef int (GMCOMMON_CALLING_CONVENTION* CVEngineServer_PrecacheGeneric)(void* engine, const char* mdl, bool preload);
	extern const std::vector<Symbol> CVEngineServer_PrecacheGenericSym;

	typedef int (*SV_FindOrAddModel)(const char* mdl, bool preload);
	extern const std::vector<Symbol> SV_FindOrAddModelSym;

	typedef int (*SV_FindOrAddGeneric)(const char* mdl, bool preload);
	extern const std::vector<Symbol> SV_FindOrAddGenericSym;

	//---------------------------------------------------------------------------------
	// Purpose: stringtable Symbols
	//---------------------------------------------------------------------------------
	typedef void (GMCOMMON_CALLING_CONVENTION* CNetworkStringTable_DeleteAllStrings)(void* table);
	extern const std::vector<Symbol> CNetworkStringTable_DeleteAllStringsSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CNetworkStringTable_Deconstructor)(void* table);
	extern const std::vector<Symbol> CNetworkStringTable_DeconstructorSym;

	//---------------------------------------------------------------------------------
	// Purpose: surffix Symbols
	//---------------------------------------------------------------------------------
	typedef int (GMCOMMON_CALLING_CONVENTION* CGameMovement_TryPlayerMove)(void* gamemovement, Vector*, void*);
	extern const std::vector<Symbol> CGameMovement_TryPlayerMoveSym;

	typedef int (GMCOMMON_CALLING_CONVENTION* CGameMovement_ClipVelocity)(void* gamemovement, Vector&, Vector&, Vector&, float);
	extern const std::vector<Symbol> CGameMovement_ClipVelocitySym;

	typedef void* (GMCOMMON_CALLING_CONVENTION* CBaseEntity_GetGroundEntity)(void* ent);
	extern const std::vector<Symbol> CBaseEntity_GetGroundEntitySym;

	typedef bool (GMCOMMON_CALLING_CONVENTION* CTraceFilterSimple_ShouldHitEntity)(void* tr, IHandleEntity*, int);
	extern const std::vector<Symbol> CTraceFilterSimple_ShouldHitEntitySym;

	typedef void* (*MoveHelperServer)();
	extern const std::vector<Symbol> MoveHelperServerSym;

	extern const std::vector<Symbol> g_pEntityListSym;

	//---------------------------------------------------------------------------------
	// Purpose: pvs Symbols
	//---------------------------------------------------------------------------------
	typedef int (GMCOMMON_CALLING_CONVENTION* CGMOD_Player_SetupVisibility)(void* ent, unsigned char* pvs, int pvssize);
	extern const std::vector<Symbol> CGMOD_Player_SetupVisibilitySym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CServerGameEnts_CheckTransmit)(void* gameents, CCheckTransmitInfo*, const unsigned short*, int);
	extern const std::vector<Symbol> CServerGameEnts_CheckTransmitSym;

	//---------------------------------------------------------------------------------
	// Purpose: filesystem Symbols
	//---------------------------------------------------------------------------------
	typedef FileHandle_t* (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_FindFileInSearchPath)(void* filesystem, CFileOpenInfo &);
	extern const std::vector<Symbol> CBaseFileSystem_FindFileInSearchPathSym;

	typedef bool (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_IsDirectory)(void* filesystem, const char* pFileName, const char* pathID);
	extern const std::vector<Symbol> CBaseFileSystem_IsDirectorySym;

	typedef CSearchPath* (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_FindSearchPathByStoreId)(void* filesystem, int);
	extern const std::vector<Symbol> CBaseFileSystem_FindSearchPathByStoreIdSym;

	typedef long (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_FastFileTime)(void* filesystem, const CSearchPath* path, const char* pFileName);
	extern const std::vector<Symbol> CBaseFileSystem_FastFileTimeSym;

	typedef bool (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_FixUpPath)(void* filesystem, const char *pFileName, char *pFixedUpFileName, int sizeFixedUpFileName);
	extern const std::vector<Symbol> CBaseFileSystem_FixUpPathSym;

	typedef FileHandle_t (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_OpenForRead)(void* filesystem, const char *pFileNameT, const char *pOptions, unsigned flags, const char *pathID, char **ppszResolvedFilename);
	extern std::vector<Symbol> CBaseFileSystem_OpenForReadSym;

	typedef long (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_GetFileTime)(void* filesystem, const char *pFileName, const char *pPathID);
	extern const std::vector<Symbol> CBaseFileSystem_GetFileTimeSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_AddSearchPath)(void* filesystem, const char *pPath, const char *pathID, SearchPathAdd_t addType);
	extern const std::vector<Symbol> CBaseFileSystem_AddSearchPathSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_AddVPKFile)(void* filesystem, const char *pPath, const char *pathID, SearchPathAdd_t addType);
	extern const std::vector<Symbol> CBaseFileSystem_AddVPKFileSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_RemoveAllMapSearchPaths)(void* filesystem);
	extern const std::vector<Symbol> CBaseFileSystem_RemoveAllMapSearchPathsSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_Close)(void* filesystem, FileHandle_t);
	extern const std::vector<Symbol> CBaseFileSystem_CloseSym;

	typedef const char* (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_CSearchPath_GetDebugString)(void* searchpath);
	extern const std::vector<Symbol> CBaseFileSystem_CSearchPath_GetDebugStringSym;


	//---------------------------------------------------------------------------------
	// Purpose: concommand Symbols
	//---------------------------------------------------------------------------------
	typedef bool (*ConCommand_IsBlocked)(const char* cmd);
	extern const std::vector<Symbol> ConCommand_IsBlockedSym;

	//---------------------------------------------------------------------------------
	// Purpose: vprof Symbols
	//---------------------------------------------------------------------------------
	typedef void* (GMCOMMON_CALLING_CONVENTION* CLuaGamemode_CallFinish)(void*, int);
	extern const std::vector<Symbol> CLuaGamemode_CallFinishSym;

	typedef void* (GMCOMMON_CALLING_CONVENTION* CLuaGamemode_CallWithArgs)(void*, int);
	extern const std::vector<Symbol> CLuaGamemode_CallWithArgsSym;

	typedef void* (GMCOMMON_CALLING_CONVENTION* CLuaGamemode_CallWithArgsStr)(void*, const char*);
	extern const std::vector<Symbol> CLuaGamemode_CallWithArgsStrSym;

	typedef void* (GMCOMMON_CALLING_CONVENTION* CLuaGamemode_CallStr)(void*, const char*);
	extern const std::vector<Symbol> CLuaGamemode_CallStrSym;

	typedef void* (GMCOMMON_CALLING_CONVENTION* CLuaGamemode_Call)(void*, int);
	extern const std::vector<Symbol> CLuaGamemode_CallSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CVProfile_OutputReport)(void*, int, const tchar*, int);
	extern const std::vector<Symbol> CVProfile_OutputReportSym;

	typedef void* (GMCOMMON_CALLING_CONVENTION* CScriptedEntity_StartFunctionStr)(void*, const char*);
	extern const std::vector<Symbol> CScriptedEntity_StartFunctionStrSym;

	typedef void* (GMCOMMON_CALLING_CONVENTION* CScriptedEntity_StartFunction)(void*, int);
	extern const std::vector<Symbol> CScriptedEntity_StartFunctionSym;

	typedef void* (GMCOMMON_CALLING_CONVENTION* CScriptedEntity_Call)(void*, int, int);
	extern const std::vector<Symbol> CScriptedEntity_CallSym;

	typedef void* (GMCOMMON_CALLING_CONVENTION* CScriptedEntity_CallFunctionStr)(void*, const char*);
	extern const std::vector<Symbol> CScriptedEntity_CallFunctionStrSym;

	typedef void* (GMCOMMON_CALLING_CONVENTION* CScriptedEntity_CallFunction)(void*, int);
	extern const std::vector<Symbol> CScriptedEntity_CallFunctionSym;

	typedef ThreadId_t (GMCOMMON_CALLING_CONVENTION* ThreadGetCurrentId_t)();
	extern const std::vector<Symbol> ThreadGetCurrentIdSym;

#ifdef SYSTEM_WINDOWS
	extern const std::vector<Symbol> Client_CLuaGamemode_CallFinishSym;
	extern const std::vector<Symbol> Client_CLuaGamemode_CallWithArgsSym;
	extern const std::vector<Symbol> Client_CLuaGamemode_CallSym;
	extern const std::vector<Symbol> Client_CScriptedEntity_StartFunctionStrSym;
	extern const std::vector<Symbol> Client_CScriptedEntity_StartFunctionSym;
	extern const std::vector<Symbol> Client_CScriptedEntity_CallSym;
	extern const std::vector<Symbol> Client_CScriptedEntity_CallFunctionStrSym;
	extern const std::vector<Symbol> Client_CScriptedEntity_CallFunctionSym;
#endif

	//---------------------------------------------------------------------------------
	// Purpose: networking Symbols
	//---------------------------------------------------------------------------------
	typedef IChangeFrameList* (*AllocChangeFrameList)(int, int);
	extern const std::vector<Symbol> AllocChangeFrameListSym;

	extern const std::vector<Symbol> g_FrameSnapshotManagerSym;

	//---------------------------------------------------------------------------------
	// Purpose: steamworks Symbols
	//---------------------------------------------------------------------------------
	typedef void (GMCOMMON_CALLING_CONVENTION* CSteam3Server_OnLoggedOff)(void*, SteamServersDisconnected_t*);
	extern const std::vector<Symbol> CSteam3Server_OnLoggedOffSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CSteam3Server_OnLogonSuccess)(void*, SteamServersConnected_t*);
	extern const std::vector<Symbol> CSteam3Server_OnLogonSuccessSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CSteam3Server_Shutdown)(void*);
	extern const std::vector<Symbol> CSteam3Server_ShutdownSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CSteam3Server_Activate)(void*, int);
	extern const std::vector<Symbol> CSteam3Server_ActivateSym;

	typedef CSteam3Server& (*Steam3ServerT)();
	extern const std::vector<Symbol> Steam3ServerSym;

	typedef void (*SV_InitGameServerSteam)();
	extern const std::vector<Symbol> SV_InitGameServerSteamSym;

	//---------------------------------------------------------------------------------
	// Purpose: pas Symbols
	//---------------------------------------------------------------------------------
	typedef const byte *(*CM_Vis)(byte *dest, int destlen, int cluster, int visType);
	extern const std::vector<Symbol> CM_VisSym;

	//---------------------------------------------------------------------------------
	// Purpose: voicechat Symbols
	//---------------------------------------------------------------------------------
	typedef void (*SV_BroadcastVoiceData)(IClient*, int nBytes, char* data, int64 xuid);
	extern const std::vector<Symbol> SV_BroadcastVoiceDataSym;

	//---------------------------------------------------------------------------------
	// Purpose: physenv Symbols
	//---------------------------------------------------------------------------------
	typedef void (*IVP_Mindist_do_impact)(void* mindist);
	extern const std::vector<Symbol> IVP_Mindist_do_impactSym;

	typedef void (*IVP_Event_Manager_Standard_simulate_time_events)(void* eventmanager, void* timemanager, void* environment, IVP_Time time);
	extern const std::vector<Symbol> IVP_Event_Manager_Standard_simulate_time_eventsSym;

	typedef void (*IVP_Mindist_simulate_time_event)(void* mindist, void* environment);
	extern const std::vector<Symbol> IVP_Mindist_simulate_time_eventSym;

	typedef void (*IVP_Mindist_update_exact_mindist_events)(void* mindist, IVP_BOOL, IVP_MINDIST_EVENT_HINT);
	extern const std::vector<Symbol> IVP_Mindist_update_exact_mindist_eventsSym;

	// Stuff for our do_impact replacement
	typedef void (*IVP_Mindist_D2)(void* mindist);
	extern const std::vector<Symbol> IVP_Mindist_D2Sym;

	extern const std::vector<Symbol> g_pCurrentMindistSym;
	extern const std::vector<Symbol> g_fDeferDeleteMindistSym;

	//---------------------------------------------------------------------------------
	// Purpose: net Symbols
	//---------------------------------------------------------------------------------
	extern const std::vector<Symbol> g_NetIncomingSym; // bf_read*
	extern const std::vector<Symbol> g_WriteSym; // bf_write*
	extern const std::vector<Symbol> g_StartedSym; // bool
}

//---------------------------------------------------------------------------------
// Purpose: Detour functions
//---------------------------------------------------------------------------------
namespace Detour
{
#define DETOUR_ISVALID( detour ) \
	detour .IsValid()

#define DETOUR_ISENABLED( detour ) \
	detour .IsEnabled()

#define DETOUR_ENABLE( detour ) \
	detour .Enable()

#define DETOUR_DISABLE( detour ) \
	detour .Disable()

// Below one is unused for now.
#define DETOUR_GETFUNC( detour, type ) \
	detour .GetTrampoline< type >()

	inline bool CheckValue(const char* msg, const char* name, bool ret)
	{
		if (!ret) {
			Msg("holylib: Failed to %s %s!\n", msg, name);
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
	extern void ReportLeak();
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

#ifdef SYSTEM_LINUX
#if ARCHITECTURE_IS_X86
#define DETOUR_SYMBOL_ID 0
#else
#define DETOUR_SYMBOL_ID 1
#endif
#else
#if ARCHITECTURE_IS_X86
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