#include <GarrysMod/Symbol.hpp>
#include "tier0/wchartypes.h"
#include "Platform.hpp"
#include "filesystem.h"
#include "inetchannel.h"
#include <vector>

#if PHYSENV_INCLUDEIVPFALLBACK
#include "sourcesdk/ivp_old/ivp_classes.h"
#endif

#if ARCHITECTURE_IS_X86_64
using ThreadId_t = uint64;
#else
using ThreadId_t = unsigned long;
#endif

using FileHandle_t = void*;

class Vector;
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
class CBaseHandle;
class IServer;
class IPhysicsObject;
class IPhysicsEnvironment;
class CBaseClient;
struct lua_State;
struct physrestoreparams_t;
class CPhysCollide;
struct objectparams_t;
class QAngle;
class CFrameSnapshot;
class CClientFrame;
class SendTable;
class CSendProxyRecipients;
class bf_read;
class bf_write;
class CBaseServer;
class PackedEntity;
class CEntityWriteInfo;
class ICvar;
class ConCommandBase;
class SVC_ServerInfo;
class INetChannel;
class CNetChan;
struct MD5Value_t;
struct dataFragments_s;
class CBaseClient;
class IVP_Cache_Ledge_Point;
class CVoiceGameMgr;
class ServerClass;
struct edict_t;
struct PackWork_t;
class CBaseViewModel;
class CBaseCombatCharacter;
class CHostState;
class CSkyCamera;
struct SteamServersConnected_t;
struct SteamServersDisconnected_t;
struct global_State;
struct GCtab;
struct audioparams_t;
struct ss_update_t;
class IConnectionlessPacketHandler;
class CCommand;

namespace GarrysMod::Lua
{
	class CLuaObject;
}

class CGameTrace;
using trace_t = CGameTrace;

namespace GarrysMod::Lua
{
	class ILuaInterface;
}

struct ThreadPoolStartParams_t;

#if defined SYSTEM_WINDOWS
#if defined ARCHITECTURE_X86_64
#define GMCOMMON_CALLING_CONVENTION __fastcall
#else
#define GMCOMMON_CALLING_CONVENTION __thiscall
#endif
#define CALLING_CONVENTION_FASTCALL __fastcall
#else
#define CALLING_CONVENTION_FASTCALL __attribute__((fastcall))
#define GMCOMMON_CALLING_CONVENTION
#endif

#if ARCHITECTURE_IS_X86_64
#define SIMPLETHREAD_RETURNVALUE long long unsigned
#else
#define SIMPLETHREAD_RETURNVALUE unsigned
#endif

 /*
  * I need to figure out how to hook into shit without breaking it on windows.
  * Currently every single hook seems to break gmod and every call to a hook seems broken.
  * It's probably some shit with calling convention or so but I have no fking Idea how to solve that :<
  * If someone knows how I could fix this, please let me know.
  * 
  * Update: First, class hooks, don't have "this"/the first argument meaning you require a ClassProxy
  */


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
	// Purpose: All Required Base Symbols
	//---------------------------------------------------------------------------------
	using InitLuaClasses = bool (*)(GarrysMod::Lua::ILuaInterface*);
	extern const std::vector<Symbol> InitLuaClassesSym;

	using CLuaInterface_Shutdown = bool (GMCOMMON_CALLING_CONVENTION*)(GarrysMod::Lua::ILuaInterface*);
	extern const std::vector<Symbol> CLuaInterface_ShutdownSym;

	using CBaseEntity_GetLuaEntity = GarrysMod::Lua::CLuaObject* (GMCOMMON_CALLING_CONVENTION*)(void* pEntity);
	extern const std::vector<Symbol> CBaseEntity_GetLuaEntitySym;
	// We use this one to directly push by reference to reduce overhead and improve performance.
	// This one is required on windows since else it tries the ILuaObject which is broken rn and idk why

	//---------------------------------------------------------------------------------
	// Purpose: All Optional Base Symbols
	//---------------------------------------------------------------------------------
	using CBaseEntity_CalcAbsolutePosition = void (GMCOMMON_CALLING_CONVENTION*)(void* ent);
	extern const std::vector<Symbol> CBaseEntity_CalcAbsolutePositionSym;

	using CBaseAnimating_InvalidateBoneCache = void (GMCOMMON_CALLING_CONVENTION*)(void* ent);
	extern const std::vector<Symbol> CBaseAnimating_InvalidateBoneCacheSym;

	using CBaseEntity_PostConstructor = void (GMCOMMON_CALLING_CONVENTION*)(void* ent, const char* szClassname);
	extern const std::vector<Symbol> CBaseEntity_PostConstructorSym;

	/*
		NOTE FOR LUAJIT SYMBOLS

		When we load any functions like lua_ or lj_ we NEED to also update the luajit module (luajit.cpp) to update the pointer when the module is enabled.
		This is because else we might call the loaded function from Gmod when in reality our luajit build has taken over which would cause a crash!
	*/

	using lua_rawseti = void (*)(lua_State* L, int index, int i);
	extern const Symbol lua_rawsetiSym;

	using lua_rawgeti = void (*)(lua_State* L, int index, int i);
	extern const Symbol lua_rawgetiSym;

	using lj_tab_new = GCtab* (*)(lua_State *L, uint32_t nArray, uint32_t nRec);
	extern const Symbol lj_tab_newSym;

	using lj_gc_barrierf = void (*)(global_State *g, void *o, void *v);
	extern const Symbol lj_gc_barrierfSym;

	using lj_tab_get = void* (*)(lua_State *L, void *t, void *key);
	extern const Symbol lj_tab_getSym;

	using lua_setfenv = int (*)(lua_State *L, int idx);
	extern const Symbol lua_setfenvSym;

	using lua_touserdata = void* (*)(lua_State *L, int idx);
	extern const Symbol lua_touserdataSym;

	using lua_type = int (*)(lua_State *L, int idx);
	extern const Symbol lua_typeSym;

	using luaL_checklstring = const char* (*)(lua_State *L, int idx, size_t* len);
	extern const Symbol luaL_checklstringSym;

	using lua_pcall = int (*)(lua_State *L, int nArgs, int nRets, int nErrorFunc);
	extern const Symbol lua_pcallSym;

	using lua_insert = void (*)(lua_State *L, int idx);
	extern const Symbol lua_insertSym;

	using lua_toboolean = int (*)(lua_State *L, int idx);
	extern const Symbol lua_tobooleanSym;

	extern const std::vector<Symbol> CGetSym;
	extern const std::vector<Symbol> gEntListSym;

	using CSteam3Server_NotifyClientDisconnect = void (GMCOMMON_CALLING_CONVENTION*)(void* server, CBaseClient* client);
	extern const std::vector<Symbol> CSteam3Server_NotifyClientDisconnectSym;

	using SteamGameServer_Shutdown = void (*)();
	extern const Symbol SteamGameServer_ShutdownSym;

	using GMOD_LoadBinaryModule = int (*)(lua_State* L, const char* pFileName);
	extern const Symbol GMOD_LoadBinaryModuleSym;

	using CM_Vis = const byte* (*)(byte* dest, int destlen, int cluster, int visType);
	extern const std::vector<Symbol> CM_VisSym;

	using CGameEventManager_CreateEvent = IGameEvent* (GMCOMMON_CALLING_CONVENTION*)(void* manager, const char* name, bool bForce);
	extern const std::vector<Symbol> CGameEventManager_CreateEventSym;

	//---------------------------------------------------------------------------------
	// Purpose: holylib Symbols
	//---------------------------------------------------------------------------------
	using GetGModServerTags = void (GMCOMMON_CALLING_CONVENTION*)(char* pDest, int iMaxSize, bool unknown);
	extern const std::vector<Symbol> GetGModServerTagsSym;

	using CFuncLadder_PlayerGotOn = void (GMCOMMON_CALLING_CONVENTION*)(void* ladder, void* ply);
	extern const std::vector<Symbol> CFuncLadder_PlayerGotOnSym;

	using CFuncLadder_PlayerGotOff = void (GMCOMMON_CALLING_CONVENTION*)(void* ladder, void* ply);
	extern const std::vector<Symbol> CFuncLadder_PlayerGotOffSym;

	using CHL2_Player_ExitLadder = void (GMCOMMON_CALLING_CONVENTION*)(void* ply);
	extern const std::vector<Symbol> CHL2_Player_ExitLadderSym;

	using CBaseEntity_SetMoveType = void (GMCOMMON_CALLING_CONVENTION*)(void* ent, int, int);
	extern const std::vector<Symbol> CBaseEntity_SetMoveTypeSym;

	using CHostState_State_ChangeLevelMP = void (GMCOMMON_CALLING_CONVENTION*)(const char* levelName, const char* LandmarkName);
	extern const std::vector<Symbol> CHostState_State_ChangeLevelMPSym;

	//---------------------------------------------------------------------------------
	// Purpose: gameevent Symbols
	//---------------------------------------------------------------------------------
	using CBaseClient_ProcessListenEvents = bool (GMCOMMON_CALLING_CONVENTION*)(void* client, void* msg);
	extern const std::vector<Symbol> CBaseClient_ProcessListenEventsSym;

	using CGameEventManager_AddListener = bool (GMCOMMON_CALLING_CONVENTION*)(void* manager, void* listener, void* descriptor, int);
	extern const std::vector<Symbol> CGameEventManager_AddListenerSym;

	//---------------------------------------------------------------------------------
	// Purpose: serverplugin Symbols
	//---------------------------------------------------------------------------------
	using CPlugin_Load = void* (GMCOMMON_CALLING_CONVENTION*)(void*, const char*);
	extern const std::vector<Symbol> CPlugin_LoadSym;

	//---------------------------------------------------------------------------------
	// Purpose: sourcetv Symbols
	//---------------------------------------------------------------------------------
	using CHLTVClient_ProcessGMod_ClientToServer = void* (GMCOMMON_CALLING_CONVENTION*)(IClient*, void*);
	extern const std::vector<Symbol> CHLTVClient_ProcessGMod_ClientToServerSym;

	using CHLTVServer_CHLTVServer = void (GMCOMMON_CALLING_CONVENTION*)(void*);
	extern const std::vector<Symbol> CHLTVServer_CHLTVServerSym;

	using CHLTVServer_DestroyCHLTVServer = void (GMCOMMON_CALLING_CONVENTION*)(void*);
	extern const std::vector<Symbol> CHLTVServer_DestroyCHLTVServerSym;

	using COM_IsValidPath = bool (GMCOMMON_CALLING_CONVENTION*)(const char*);
	extern const std::vector<Symbol> COM_IsValidPathSym;

	using CHLTVDemoRecorder_StartRecording = void (GMCOMMON_CALLING_CONVENTION*)(void*, const char*, bool);
	extern const std::vector<Symbol> CHLTVDemoRecorder_StartRecordingSym;

	using CHLTVDemoRecorder_StopRecording = void (GMCOMMON_CALLING_CONVENTION*)(void*);
	extern const std::vector<Symbol> CHLTVDemoRecorder_StopRecordingSym;

	using CHLTVClient_ExecuteStringCommand = bool (GMCOMMON_CALLING_CONVENTION*)(void*, const char*);
	extern const std::vector<Symbol> CHLTVClient_ExecuteStringCommandSym;

	using CHLTVClient_Deconstructor = void (GMCOMMON_CALLING_CONVENTION*)(void*);
	extern const std::vector<Symbol> CHLTVClient_DeconstructorSym;

	using CHLTVDirector_StartNewShot = void (GMCOMMON_CALLING_CONVENTION*)(void* director);
	extern const std::vector<Symbol> CHLTVDirector_StartNewShotSym;

	using CHLTVServer_BroadcastEvent = void (GMCOMMON_CALLING_CONVENTION*)(IServer* server, IGameEvent* event);
	extern const std::vector<Symbol> CHLTVServer_BroadcastEventSym;

	extern const std::vector<Symbol> UsermessagesSym;

	//---------------------------------------------------------------------------------
	// Purpose: threadpoolfix Symbols
	//---------------------------------------------------------------------------------

	// None as all fixed we had were implemented into gmod.

	//---------------------------------------------------------------------------------
	// Purpose: precachefix Symbols
	//---------------------------------------------------------------------------------
	using CVEngineServer_PrecacheModel = int (GMCOMMON_CALLING_CONVENTION*)(void* engine, const char* mdl, bool preload);
	extern const std::vector<Symbol> CVEngineServer_PrecacheModelSym;

	using CVEngineServer_PrecacheGeneric = int (GMCOMMON_CALLING_CONVENTION*)(void* engine, const char* mdl, bool preload);
	extern const std::vector<Symbol> CVEngineServer_PrecacheGenericSym;

	using SV_FindOrAddModel = int (*)(const char* mdl, bool preload);
	extern const std::vector<Symbol> SV_FindOrAddModelSym;

	using SV_FindOrAddGeneric = int (*)(const char* mdl, bool preload);
	extern const std::vector<Symbol> SV_FindOrAddGenericSym;

	//---------------------------------------------------------------------------------
	// Purpose: stringtable Symbols
	//---------------------------------------------------------------------------------
	using CNetworkStringTable_DeleteAllStrings = void (GMCOMMON_CALLING_CONVENTION*)(void* table);
	extern const std::vector<Symbol> CNetworkStringTable_DeleteAllStringsSym;

	using CNetworkStringTable_Deconstructor = void (GMCOMMON_CALLING_CONVENTION*)(void* table);
	extern const std::vector<Symbol> CNetworkStringTable_DeconstructorSym;

	//---------------------------------------------------------------------------------
	// Purpose: surffix Symbols
	//---------------------------------------------------------------------------------
	using CGameMovement_TryPlayerMove = int (GMCOMMON_CALLING_CONVENTION*)(void* gamemovement, Vector*, void*);
	extern const std::vector<Symbol> CGameMovement_TryPlayerMoveSym;

	using CGameMovement_ClipVelocity = int (GMCOMMON_CALLING_CONVENTION*)(void* gamemovement, Vector&, Vector&, Vector&, float);
	extern const std::vector<Symbol> CGameMovement_ClipVelocitySym;

	using CTraceFilterSimple_ShouldHitEntity = bool (GMCOMMON_CALLING_CONVENTION*)(void* tr, IHandleEntity*, int);
	extern const std::vector<Symbol> CTraceFilterSimple_ShouldHitEntitySym;

	using MoveHelperServer = void* (*)();
	extern const std::vector<Symbol> MoveHelperServerSym;

	//---------------------------------------------------------------------------------
	// Purpose: pvs Symbols
	//---------------------------------------------------------------------------------
	using CGMOD_Player_SetupVisibility = int (GMCOMMON_CALLING_CONVENTION*)(void* ent, unsigned char* pvs, int pvssize);
	extern const std::vector<Symbol> CGMOD_Player_SetupVisibilitySym;

	using CServerGameEnts_CheckTransmit = void (GMCOMMON_CALLING_CONVENTION*)(void* gameents, CCheckTransmitInfo*, const unsigned short*, int);
	extern const std::vector<Symbol> CServerGameEnts_CheckTransmitSym;

	//---------------------------------------------------------------------------------
	// Purpose: filesystem Symbols
	//---------------------------------------------------------------------------------
	using CBaseFileSystem_FindFileInSearchPath = FileHandle_t* (GMCOMMON_CALLING_CONVENTION*)(void* filesystem, CFileOpenInfo&);
	extern const std::vector<Symbol> CBaseFileSystem_FindFileInSearchPathSym;

	using CBaseFileSystem_IsDirectory = bool (GMCOMMON_CALLING_CONVENTION*)(void* filesystem, const char* pFileName, const char* pathID);
	extern const std::vector<Symbol> CBaseFileSystem_IsDirectorySym;

	using CBaseFileSystem_FindSearchPathByStoreId = CSearchPath* (GMCOMMON_CALLING_CONVENTION*)(void* filesystem, int);
	extern const std::vector<Symbol> CBaseFileSystem_FindSearchPathByStoreIdSym;

	using CBaseFileSystem_FastFileTime = long (GMCOMMON_CALLING_CONVENTION*)(void* filesystem, const CSearchPath* path, const char* pFileName);
	extern const std::vector<Symbol> CBaseFileSystem_FastFileTimeSym;

	using CBaseFileSystem_FixUpPath = bool (GMCOMMON_CALLING_CONVENTION*)(void* filesystem, const char* pFileName, char* pFixedUpFileName, int sizeFixedUpFileName);
	extern const std::vector<Symbol> CBaseFileSystem_FixUpPathSym;

	using CBaseFileSystem_OpenForRead = FileHandle_t(GMCOMMON_CALLING_CONVENTION*)(void* filesystem, const char* pFileNameT, const char* pOptions, unsigned flags, const char* pathID, char** ppszResolvedFilename);
	extern std::vector<Symbol> CBaseFileSystem_OpenForReadSym;

	using CBaseFileSystem_GetFileTime = long (GMCOMMON_CALLING_CONVENTION*)(void* filesystem, const char* pFileName, const char* pPathID);
	extern const std::vector<Symbol> CBaseFileSystem_GetFileTimeSym;

	using CBaseFileSystem_AddSearchPath = void (GMCOMMON_CALLING_CONVENTION*)(void* filesystem, const char* pPath, const char* pathID, SearchPathAdd_t addType);
	extern const std::vector<Symbol> CBaseFileSystem_AddSearchPathSym;

	using CBaseFileSystem_AddVPKFile = void (GMCOMMON_CALLING_CONVENTION*)(void* filesystem, const char* pPath, const char* pathID, SearchPathAdd_t addType);
	extern const std::vector<Symbol> CBaseFileSystem_AddVPKFileSym;

	using CBaseFileSystem_Close = void (GMCOMMON_CALLING_CONVENTION*)(void* filesystem, FileHandle_t);
	extern const std::vector<Symbol> CBaseFileSystem_CloseSym;

	using CBaseFileSystem_CSearchPath_GetDebugString = const char* (GMCOMMON_CALLING_CONVENTION*)(void* searchpath);
	extern const std::vector<Symbol> CBaseFileSystem_CSearchPath_GetDebugStringSym;

	extern const std::vector<Symbol> g_PathIDTableSym;

	//---------------------------------------------------------------------------------
	// Purpose: concommand Symbols
	//---------------------------------------------------------------------------------
	using ConCommand_IsBlocked = const char* (*)(const char* cmd);
	extern const std::vector<Symbol> ConCommand_IsBlockedSym;

	//---------------------------------------------------------------------------------
	// Purpose: vprof Symbols
	//---------------------------------------------------------------------------------
	using CLuaGamemode_CallFinish = void* (GMCOMMON_CALLING_CONVENTION*)(void*, int);
	extern const std::vector<Symbol> CLuaGamemode_CallFinishSym;

	using CLuaGamemode_CallWithArgs = void* (GMCOMMON_CALLING_CONVENTION*)(void*, int);
	extern const std::vector<Symbol> CLuaGamemode_CallWithArgsSym;

	using CLuaGamemode_CallWithArgsStr = void* (GMCOMMON_CALLING_CONVENTION*)(void*, const char*);
	extern const std::vector<Symbol> CLuaGamemode_CallWithArgsStrSym;

	using CLuaGamemode_CallStr = void* (GMCOMMON_CALLING_CONVENTION*)(void*, const char*);
	extern const std::vector<Symbol> CLuaGamemode_CallStrSym;

	using CLuaGamemode_Call = void* (GMCOMMON_CALLING_CONVENTION*)(void*, int);
	extern const std::vector<Symbol> CLuaGamemode_CallSym;

	using CVProfile_OutputReport = void (GMCOMMON_CALLING_CONVENTION*)(void*, int, const tchar*, int);
	extern const std::vector<Symbol> CVProfile_OutputReportSym;

	using CScriptedEntity_StartFunctionStr = void* (GMCOMMON_CALLING_CONVENTION*)(void*, const char*);
	extern const std::vector<Symbol> CScriptedEntity_StartFunctionStrSym;

	using CScriptedEntity_StartFunction = void* (GMCOMMON_CALLING_CONVENTION*)(void*, int);
	extern const std::vector<Symbol> CScriptedEntity_StartFunctionSym;

	using CScriptedEntity_Call = void* (GMCOMMON_CALLING_CONVENTION*)(void*, int, int);
	extern const std::vector<Symbol> CScriptedEntity_CallSym;

	using CScriptedEntity_CallFunctionStr = void* (GMCOMMON_CALLING_CONVENTION*)(void*, const char*);
	extern const std::vector<Symbol> CScriptedEntity_CallFunctionStrSym;

	using CScriptedEntity_CallFunction = void* (GMCOMMON_CALLING_CONVENTION*)(void*, int);
	extern const std::vector<Symbol> CScriptedEntity_CallFunctionSym;

	using lj_BC_FUNCC = void* (GMCOMMON_CALLING_CONVENTION*)(void* idk);
	extern const std::vector<Symbol> lj_BC_FUNCCSym;

	//---------------------------------------------------------------------------------
	// Purpose: networking Symbols
	//---------------------------------------------------------------------------------
	using AllocChangeFrameList = IChangeFrameList* (*)(int, int);
	extern const std::vector<Symbol> AllocChangeFrameListSym;

	using SendTable_CullPropsFromProxies = int (*)(const SendTable*, const int*, int, int, const CSendProxyRecipients*, int, const CSendProxyRecipients*, int, int*, int);
	extern const std::vector<Symbol> SendTable_CullPropsFromProxiesSym;

	using SendTable_WritePropList = void (*)(const SendTable *, const void*, int, bf_write*, const int, const int*, int);
	extern const std::vector<Symbol> SendTable_WritePropListSym;

	using CBaseServer_WriteDeltaEntities = void (*)(CBaseServer* pServer, CBaseClient *client, CClientFrame *to, CClientFrame *from, bf_write &pBuf);
	extern const std::vector<Symbol> CBaseServer_WriteDeltaEntitiesSym;

	using SendTable_CalcDelta = int (*)(const SendTable*, const void*, const int, const void*, const int, int*, int, const int);
	extern const std::vector<Symbol> SendTable_CalcDeltaSym;

	using SV_DetermineUpdateType = void (__cdecl*)(CEntityWriteInfo& u);
	extern const std::vector<Symbol> SV_DetermineUpdateTypeSym;

	using PackedEntity_GetPropsChangedAfterTick = int (GMCOMMON_CALLING_CONVENTION*)(PackedEntity*, int tick, int* iOutProps, int nMaxOutProps);
	extern const std::vector<Symbol> PackedEntity_GetPropsChangedAfterTickSym;

	using CGameServer_SendClientMessages = void (GMCOMMON_CALLING_CONVENTION*)(CBaseServer* pServer, bool sendSnapshots);
	extern const std::vector<Symbol> CGameServer_SendClientMessagesSym;

	using CBaseEntity_GMOD_SetShouldPreventTransmitToPlayer = void (GMCOMMON_CALLING_CONVENTION*)(CBaseEntity* pEnt, CBasePlayer* pPly, bool bPreventTransmit);
	extern const std::vector<Symbol> CBaseEntity_GMOD_SetShouldPreventTransmitToPlayerSym;

	using CBaseEntity_GMOD_ShouldPreventTransmitToPlayer = bool (GMCOMMON_CALLING_CONVENTION*)(CBaseEntity* pEnt, CBasePlayer* pPly);
	extern const std::vector<Symbol> CBaseEntity_GMOD_ShouldPreventTransmitToPlayerSym;

	extern const std::vector<Symbol> g_FrameSnapshotManagerSym;
	extern const std::vector<Symbol> g_PropTypeFnsSym;
	extern const std::vector<Symbol> g_BSPDataSym;

	using PackWork_t_Process = void (*)(PackWork_t& pWork);
	extern const std::vector<Symbol> PackWork_t_ProcessSym;

	using SV_PackEntity = void (*)(int edictIdx, edict_t* edict, ServerClass* pServerClass, CFrameSnapshot *pSnapshot );
	extern const std::vector<Symbol> SV_PackEntitySym;

	using InvalidateSharedEdictChangeInfos = void (*)();
	extern const std::vector<Symbol> InvalidateSharedEdictChangeInfosSym;
	extern const std::vector<Symbol> PackEntities_NormalSym;

	using CGMOD_Player_CreateViewModel = void (GMCOMMON_CALLING_CONVENTION*)(CBasePlayer* pPlayer, int viewmodelindex);
	extern const std::vector<Symbol> CGMOD_Player_CreateViewModelSym;

	using CBaseCombatCharacter_SetTransmit = void (GMCOMMON_CALLING_CONVENTION*)(CBaseCombatCharacter* pCharacter, CCheckTransmitInfo *pInfo, bool bAlways);
	extern const std::vector<Symbol> CBaseCombatCharacter_SetTransmitSym;
	extern const std::vector<Symbol> CBaseAnimating_SetTransmitSym;

	//---------------------------------------------------------------------------------
	// Purpose: steamworks Symbols
	//---------------------------------------------------------------------------------
	using CSteam3Server_OnLoggedOff = void (GMCOMMON_CALLING_CONVENTION*)(void*, SteamServersDisconnected_t*);
	extern const std::vector<Symbol> CSteam3Server_OnLoggedOffSym;

	using CSteam3Server_OnLogonSuccess = void (GMCOMMON_CALLING_CONVENTION*)(void*, SteamServersConnected_t*);
	extern const std::vector<Symbol> CSteam3Server_OnLogonSuccessSym;

	using CSteam3Server_Shutdown = void (GMCOMMON_CALLING_CONVENTION*)(void*);
	extern const std::vector<Symbol> CSteam3Server_ShutdownSym;

	using CSteam3Server_Activate = void (GMCOMMON_CALLING_CONVENTION*)(void*, int);
	extern const std::vector<Symbol> CSteam3Server_ActivateSym;

	using CSteam3Server_NotifyClientConnect = bool (GMCOMMON_CALLING_CONVENTION*)(void*, CBaseClient* client, uint32 unUserID, netadr_t& adr, const void *pvCookie, uint32 ucbCookie);
	extern const std::vector<Symbol> CSteam3Server_NotifyClientConnectSym;

	using CSteam3Server_SendUpdatedServerDetails = void (GMCOMMON_CALLING_CONVENTION*)(void*);
	extern const std::vector<Symbol> CSteam3Server_SendUpdatedServerDetailsSym;

	using CSteam3Server_CheckForDuplicateSteamID = bool (GMCOMMON_CALLING_CONVENTION*)(void*, void*);
	extern const std::vector<Symbol> CSteam3Server_CheckForDuplicateSteamIDSym;

	using Steam3ServerT = CSteam3Server& (*)();
	extern const std::vector<Symbol> Steam3ServerSym;

	using SV_InitGameServerSteam = void (*)();
	extern const std::vector<Symbol> SV_InitGameServerSteamSym;

	//---------------------------------------------------------------------------------
	// Purpose: voicechat Symbols
	//---------------------------------------------------------------------------------
	using SV_BroadcastVoiceData = void (*)(IClient*, int nBytes, char* data, int64 xuid);
	extern const std::vector<Symbol> SV_BroadcastVoiceDataSym;

	// These below are obsolete soon.
	using CVoiceGameMgr_Update = void (GMCOMMON_CALLING_CONVENTION*)(CVoiceGameMgr*, double frametime);
	extern const std::vector<Symbol> CVoiceGameMgr_UpdateSym;

	extern const std::vector<Symbol> g_PlayerModEnableSym;
	extern const std::vector<Symbol> g_BanMasksSym;
	extern const std::vector<Symbol> g_SentGameRulesMasksSym;
	extern const std::vector<Symbol> g_SentBanMasksSym;
	extern const std::vector<Symbol> g_bWantModEnableSym;

	//---------------------------------------------------------------------------------
	// Purpose: physenv Symbols
	//---------------------------------------------------------------------------------
#if PHYSENV_INCLUDEIVPFALLBACK
	using IVP_Mindist_do_impact = void (GMCOMMON_CALLING_CONVENTION*)(void* mindist);
	extern const std::vector<Symbol> IVP_Mindist_do_impactSym;

	using IVP_Event_Manager_Standard_simulate_time_events = void (GMCOMMON_CALLING_CONVENTION*)(void* eventmanager, void* timemanager, void* environment, GMODSDK::IVP_Time time);
	extern const std::vector<Symbol> IVP_Event_Manager_Standard_simulate_time_eventsSym;

	using IVP_Mindist_simulate_time_event = void (GMCOMMON_CALLING_CONVENTION*)(void* mindist, void* environment);
	extern const std::vector<Symbol> IVP_Mindist_simulate_time_eventSym;

	using IVP_Mindist_update_exact_mindist_events = void (GMCOMMON_CALLING_CONVENTION*)(void* mindist, GMODSDK::IVP_BOOL, GMODSDK::IVP_MINDIST_EVENT_HINT);
	extern const std::vector<Symbol> IVP_Mindist_update_exact_mindist_eventsSym;

	using CPhysicsEnvironment_DestroyObject = void (GMCOMMON_CALLING_CONVENTION*)(IPhysicsEnvironment*, IPhysicsObject*);
	extern const std::vector<Symbol> CPhysicsEnvironment_DestroyObjectSym;

	using CPhysicsEnvironment_Restore = bool (GMCOMMON_CALLING_CONVENTION*)(IPhysicsEnvironment*, physrestoreparams_t const&);
	extern const std::vector<Symbol> CPhysicsEnvironment_RestoreSym;

	using CPhysicsEnvironment_TransferObject = bool (GMCOMMON_CALLING_CONVENTION*)(IPhysicsEnvironment*, IPhysicsObject*, IPhysicsEnvironment*);
	extern const std::vector<Symbol> CPhysicsEnvironment_TransferObjectSym;

	using CPhysicsEnvironment_CreateSphereObject = IPhysicsObject* (GMCOMMON_CALLING_CONVENTION*)(IPhysicsEnvironment*, float radius, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t *pParams, bool isStatic);
	extern const std::vector<Symbol> CPhysicsEnvironment_CreateSphereObjectSym;

	using CPhysicsEnvironment_UnserializeObjectFromBuffer = IPhysicsObject* (GMCOMMON_CALLING_CONVENTION*)(IPhysicsEnvironment*, void *pGameData, unsigned char *pBuffer, unsigned int bufferSize, bool enableCollisions);
	extern const std::vector<Symbol> CPhysicsEnvironment_UnserializeObjectFromBufferSym;

	using CPhysicsEnvironment_CreatePolyObjectStatic = IPhysicsObject* (GMCOMMON_CALLING_CONVENTION*)(IPhysicsEnvironment*, const CPhysCollide *pCollisionModel, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t *pParams);
	extern const std::vector<Symbol> CPhysicsEnvironment_CreatePolyObjectStaticSym;

	using CPhysicsEnvironment_CreatePolyObject = IPhysicsObject* (GMCOMMON_CALLING_CONVENTION*)(IPhysicsEnvironment*, const CPhysCollide *pCollisionModel, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t *pParams);
	extern const std::vector<Symbol> CPhysicsEnvironment_CreatePolyObjectSym;

	using CPhysicsEnvironment_D2 = void (GMCOMMON_CALLING_CONVENTION*)(IPhysicsEnvironment*); // Deconstructor
	extern const std::vector<Symbol> CPhysicsEnvironment_D2Sym;

	using CPhysicsEnvironment_C2 = void (GMCOMMON_CALLING_CONVENTION*)(IPhysicsEnvironment*); // Constructor
	extern const std::vector<Symbol> CPhysicsEnvironment_C2Sym;

	using IVP_Mindist_Manager_recheck_ov_element = void (GMCOMMON_CALLING_CONVENTION*)(void* mindistManager, void* physObj); // Crash fix.
	extern const std::vector<Symbol> IVP_Mindist_Manager_recheck_ov_elementSym;

	// Stuff for our do_impact replacement
	using IVP_Mindist_D2 = void (*)(void* mindist);
	extern const std::vector<Symbol> IVP_Mindist_D2Sym;

	extern const std::vector<Symbol> g_pCurrentMindistSym;
	extern const std::vector<Symbol> g_fDeferDeleteMindistSym;
#endif

	using GMod_Util_IsPhysicsObjectValid = bool (*)(IPhysicsObject* obj);
	extern const std::vector<Symbol> GMod_Util_IsPhysicsObjectValidSym;

	using CPhysicsHook_FrameUpdatePostEntityThink = void (GMCOMMON_CALLING_CONVENTION*)(void* CPhysicsHook);
	extern const std::vector<Symbol> CPhysicsHook_FrameUpdatePostEntityThinkSym;

	using CCollisionEvent_FrameUpdate = void (GMCOMMON_CALLING_CONVENTION*)(void* pCollisionEvent);
	extern const std::vector<Symbol> CCollisionEvent_FrameUpdateSym;

	using CCollisionProperty_MarkSurroundingBoundsDirty = void (GMCOMMON_CALLING_CONVENTION*)(void* fancy_class);
	extern const std::vector<Symbol> CCollisionProperty_MarkSurroundingBoundsDirtySym;

#if PHYSENV_INCLUDEIVPFALLBACK
	using IVP_Mindist_Minimize_Solver_p_minimize_PP = GMODSDK::IVP_MRC_TYPE (*)(void* mindistMinimizeSolver, const GMODSDK::IVP_Compact_Edge *A, const GMODSDK::IVP_Compact_Edge *B, IVP_Cache_Ledge_Point *m_cache_A, IVP_Cache_Ledge_Point *m_cache_B);
	extern const std::vector<Symbol> IVP_Mindist_Minimize_Solver_p_minimize_PPSym;

	using IVP_Mindist_Base_get_objects = void (GMCOMMON_CALLING_CONVENTION*)(void* mindist, GMODSDK::IVP_Real_Object**);
	extern const std::vector<Symbol> IVP_Mindist_Base_get_objectsSym;

	using IVP_OV_Element_add_oo_collision = void (GMCOMMON_CALLING_CONVENTION*)(void* ovElement, GMODSDK::IVP_Collision* connector);
	extern const std::vector<Symbol> IVP_OV_Element_add_oo_collisionSym;

	using IVP_OV_Element_remove_oo_collision = void (GMCOMMON_CALLING_CONVENTION*)(void* ovElement, GMODSDK::IVP_Collision* connector);
	extern const std::vector<Symbol> IVP_OV_Element_remove_oo_collisionSym;
#endif

	//---------------------------------------------------------------------------------
	// Purpose: net Symbols
	//---------------------------------------------------------------------------------
	extern const std::vector<Symbol> g_NetIncomingSym; // bf_read*
	extern const std::vector<Symbol> g_WriteSym; // bf_write*
	extern const std::vector<Symbol> g_StartedSym; // bool

	//---------------------------------------------------------------------------------
	// Purpose: luajit Symbols
	//---------------------------------------------------------------------------------
	using CLuaInterface_GetType = int (GMCOMMON_CALLING_CONVENTION*)(GarrysMod::Lua::ILuaInterface* pLua);
	extern const std::vector<Symbol> CLuaInterface_GetTypeSym;

	//---------------------------------------------------------------------------------
	// Purpose: gameserver Symbols
	//---------------------------------------------------------------------------------
	using CServerGameClients_GetPlayerLimit = void (GMCOMMON_CALLING_CONVENTION*)(void*, int&, int&, int&);
	extern const std::vector<Symbol> CServerGameClients_GetPlayerLimitSym;

	using CBaseServer_FillServerInfo = void (GMCOMMON_CALLING_CONVENTION*)(void*, SVC_ServerInfo&);
	extern const std::vector<Symbol> CBaseServer_FillServerInfoSym;

	using CHLTVServer_FillServerInfo = void (GMCOMMON_CALLING_CONVENTION*)(void*, SVC_ServerInfo&);
	extern const std::vector<Symbol> CHLTVServer_FillServerInfoSym;

	using CBaseClient_SetSignonState = bool (GMCOMMON_CALLING_CONVENTION*)(void* client, int state, int spawncount);
	extern const std::vector<Symbol> CBaseClient_SetSignonStateSym;

	using CBaseServer_IsMultiplayer = bool (GMCOMMON_CALLING_CONVENTION*)(void* srv);
	extern const std::vector<Symbol> CBaseServer_IsMultiplayerSym;

	using GModDataPack_IsSingleplayer = bool (GMCOMMON_CALLING_CONVENTION*)(void* datapack);
	extern const std::vector<Symbol> GModDataPack_IsSingleplayerSym;

	using CBaseClient_ShouldSendMessages = bool (GMCOMMON_CALLING_CONVENTION*)(void* client);
	extern const std::vector<Symbol> CBaseClient_ShouldSendMessagesSym;

	using CBaseServer_CheckTimeouts = void (GMCOMMON_CALLING_CONVENTION*)(void* server);
	extern const std::vector<Symbol> CBaseServer_CheckTimeoutsSym;

	using CBaseClient_OnRequestFullUpdate = void (GMCOMMON_CALLING_CONVENTION*)(void* client);
	extern const std::vector<Symbol> CBaseClient_OnRequestFullUpdateSym;

	using CGameClient_SpawnPlayer = void (GMCOMMON_CALLING_CONVENTION*)(void* client);
	extern const std::vector<Symbol> CGameClient_SpawnPlayerSym;

	using CBaseServer_ProcessConnectionlessPacket = bool (GMCOMMON_CALLING_CONVENTION*)(void* server, netpacket_s* packet);
	extern const std::vector<Symbol> CBaseServer_ProcessConnectionlessPacketSym;

	using NET_SendPacket = int (*)(INetChannel *chan, int sock, const netadr_t &to, const unsigned char *data, int length, bf_write *pVoicePayload /* = nullptr */, bool bUseCompression /*=false*/);
	extern const std::vector<Symbol> NET_SendPacketSym;

	using CNetChan_SendDatagram = int (GMCOMMON_CALLING_CONVENTION*)(CNetChan* chan, bf_write *datagram);
	extern const std::vector<Symbol> CNetChan_SendDatagramSym;

	using CNetChan_D2 = void (GMCOMMON_CALLING_CONVENTION*)(CNetChan* chan);
	extern const std::vector<Symbol> CNetChan_D2Sym;

	using NET_CreateNetChannel = INetChannel* (*)(int socket, netadr_t* adr, const char* name, INetChannelHandler* handler, bool bForceNewCHannel, int nProtocolVersion);
	extern const std::vector<Symbol> NET_CreateNetChannelSym;

	using NET_RemoveNetChannel = void (*)(INetChannel* chan, bool bDeleteNetChan);
	extern const std::vector<Symbol> NET_RemoveNetChannelSym;

	using Filter_SendBan = void (*)(const netadr_t& adr);
	extern const std::vector<Symbol> Filter_SendBanSym;

	using NET_SendStream = int (*)(int nSock, const char * buf, int len, int flags);
	extern const std::vector<Symbol> NET_SendStreamSym;

	using NET_ReceiveStream = int (*)(int nSock, char * buf, int len, int flags);
	extern const std::vector<Symbol> NET_ReceiveStreamSym;

	using NET_SetTime = void (*)(double flRealtime);
	extern const std::vector<Symbol> NET_SetTimeSym;

	extern const std::vector<Symbol> s_NetChannelsSym;

	//---------------------------------------------------------------------------------
	// Purpose: cvar Symbols
	//---------------------------------------------------------------------------------
	using CCvar_RegisterConCommand = void (*)(ICvar*, ConCommandBase*);
	extern const std::vector<Symbol> CCvar_RegisterConCommandSym;

	using CCvar_UnregisterConCommand = void (*)(ICvar*, ConCommandBase*);
	extern const std::vector<Symbol> CCvar_UnregisterConCommandSym;

	using CCvar_UnregisterConCommands = void (*)(ICvar*, int);
	extern const std::vector<Symbol> CCvar_UnregisterConCommandsSym;

	using CCvar_FindCommandBaseConst = const ConCommandBase* (GMCOMMON_CALLING_CONVENTION*)(ICvar*, const char* name);
	extern const std::vector<Symbol> CCvar_FindCommandBaseConstSym;

	using CCvar_FindCommandBase = ConCommandBase* (GMCOMMON_CALLING_CONVENTION*)(ICvar*, const char* name);
	extern const std::vector<Symbol> CCvar_FindCommandBaseSym;

	//---------------------------------------------------------------------------------
	// Purpose: AutoRefresh Symbols
	//---------------------------------------------------------------------------------
	using GarrysMod_AutoRefresh_HandleChange_Lua = bool (*)(const std::string* fileRelPath, const std::string* fileName, const std::string* fileExt);
	extern const std::vector<Symbol> GarrysMod_AutoRefresh_HandleChange_LuaSym;

	using GarrysMod_AutoRefresh_Init = void (*)();
	extern const std::vector<Symbol> GarrysMod_AutoRefresh_InitSym;

	using GarrysMod_AutoRefresh_Cycle = void (*)();
	extern const std::vector<Symbol> GarrysMod_AutoRefresh_CycleSym;

	using Bootil_File_ChangeMonitor_CheckForChanges = void (GMCOMMON_CALLING_CONVENTION*)(void* changeMonitor);
	extern const std::vector<Symbol> Bootil_File_ChangeMonitor_CheckForChangesSym;

	using Bootil_File_ChangeMonitor_HasChanges = bool (GMCOMMON_CALLING_CONVENTION*)(void* changeMonitor);
	extern const std::vector<Symbol> Bootil_File_ChangeMonitor_HasChangesSym;

	//---------------------------------------------------------------------------------
	// Purpose: soundscape Symbols
	//---------------------------------------------------------------------------------
	using CEnvSoundscape_UpdateForPlayer = bool (GMCOMMON_CALLING_CONVENTION*)(CBaseEntity* pSoundScape, ss_update_t& update);
	extern const std::vector<Symbol> CEnvSoundscape_UpdateForPlayerSym;

	using CEnvSoundscape_WriteAudioParamsTo = bool (GMCOMMON_CALLING_CONVENTION*)(CBaseEntity* pSoundScape, audioparams_t& update);
	extern const std::vector<Symbol> CEnvSoundscape_WriteAudioParamsToSym;

	extern const std::vector<Symbol> g_SoundscapeSystemSym;

	//---------------------------------------------------------------------------------
	// Purpose: networkthreading Symbols
	//---------------------------------------------------------------------------------
	using NET_ProcessSocket = void (*)(int sock, IConnectionlessPacketHandler *handler);
	extern const std::vector<Symbol> NET_ProcessSocketSym;

	using NET_GetPacket = netpacket_s* (*)(int sock, byte *scratch);
	extern const std::vector<Symbol> NET_GetPacketSym;
	
	using Filter_ShouldDiscard = bool (*)(const netadr_t& adr);
	extern const std::vector<Symbol> Filter_ShouldDiscardSym;

	// Already defined by gameserver
	// using  = void (*Filter_SendBan)(const netadr_t& adr);
	// extern const std::vector<Symbol> Filter_SendBanSym;

	using NET_FindNetChannel = CNetChan* (*)(int socket, netadr_t &adr);
	extern const std::vector<Symbol> NET_FindNetChannelSym;

	using CNetChan_Constructor = void (GMCOMMON_CALLING_CONVENTION*)(CNetChan* pChanenl);
	extern const std::vector<Symbol> CNetChan_ConstructorSym;

	using NET_RemoveNetChannel = void (*)(INetChannel* pChannel, bool bShouldRemove);
	extern const std::vector<Symbol> NET_RemoveNetChannelSym;

	// Now come some commands that party on g_IPFilters as we need to make it thread safe to avoid crashes :3
	using writeip = void (*)(const CCommand* pCommand);
	extern const std::vector<Symbol> writeipSym;

	using listip = void (*)(const CCommand* pCommand);
	extern const std::vector<Symbol> listipSym;

	using removeip = void (*)(const CCommand* pCommand);
	extern const std::vector<Symbol> removeipSym;

	using Filter_Add_f = void (*)(const CCommand* pCommand);
	extern const std::vector<Symbol> Filter_Add_fSym;

	//---------------------------------------------------------------------------------
	// Purpose: networkingreplacement Symbols
	//---------------------------------------------------------------------------------
	using SV_EnsureInstanceBaseline = void (*)(ServerClass *pServerClass, int iEdict, const void *pData, intp nBytes);
	extern const std::vector<Symbol> SV_EnsureInstanceBaselineSym;

	using SendTable_Encode = bool (*)(const SendTable *pTable, const void *pStruct, bf_write *pOut, int objectID, void *pRecipients, bool bNonZeroOnly);
	extern const std::vector<Symbol> SendTable_EncodeSym;

	using CFrameSnapshotManager_GetPreviouslySentPacket = PackedEntity* (*)(void* framesnapshotmanager, int iEntity, int iSerialNumber);
	extern const std::vector<Symbol> CFrameSnapshotManager_GetPreviouslySentPacketSym;

	using CFrameSnapshotManager_UsePreviouslySentPacket = bool (*)(void* framesnapshotmanager, CFrameSnapshot* pSnapshot, int entity, int entSerialNumber);
	extern const std::vector<Symbol> CFrameSnapshotManager_UsePreviouslySentPacketSym;

	using CFrameSnapshotManager_CreatePackedEntity = PackedEntity* (*)(void* framesnapshotmanager, CFrameSnapshot* pSnapshot, int entity);
	extern const std::vector<Symbol> CFrameSnapshotManager_CreatePackedEntitySym;
}