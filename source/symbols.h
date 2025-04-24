#include <GarrysMod/Symbol.hpp>
#include "sourcesdk/ivp_time.h"
#include <steam/isteamuser.h>
#include "tier0/wchartypes.h"
#include "Platform.hpp"
#include "filesystem.h"
#include "inetchannel.h"
#include <vector>

#if ARCHITECTURE_IS_X86_64
typedef uint64 ThreadId_t;
#else
typedef unsigned long ThreadId_t;
#endif

typedef void* FileHandle_t;

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

class	CGameTrace;
typedef	CGameTrace trace_t;

namespace GarrysMod::Lua
{
	class ILuaInterface;
}

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
  * 
  * Update: First, class hooks, don't have "this"/the first argument meaning you require a ClassProxy
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

	typedef void (*CBaseEntity_CalcAbsolutePosition)(void* ent);
	extern const std::vector<Symbol> CBaseEntity_CalcAbsolutePositionSym;

	typedef void (*CBaseAnimating_InvalidateBoneCache)(void* ent);
	extern const std::vector<Symbol> CBaseAnimating_InvalidateBoneCacheSym;

	typedef void (*CBaseEntity_PostConstructor)(void* ent, const char* szClassname);
	extern const std::vector<Symbol> CBaseEntity_PostConstructorSym;

	typedef void (*CCollisionProperty_MarkSurroundingBoundsDirty)(void* fancy_class);
	extern const std::vector<Symbol> CCollisionProperty_MarkSurroundingBoundsDirtySym;

	typedef void (*CBaseEntity_VPhysicsUpdate)(void* fancy_class, void* obj);
	extern const std::vector<Symbol> CBaseEntity_VPhysicsUpdateSym;

	typedef void (*CBaseEntity_GMOD_VPhysicsTest)(void* fancy_class, void* obj);
	extern const std::vector<Symbol> CBaseEntity_GMOD_VPhysicsTestSym;

	typedef void (*CBaseEntity_VPhysicsDestroyObject)(void* fancy_class);
	extern const std::vector<Symbol> CBaseEntity_VPhysicsDestroyObjectSym;

	typedef void (*lua_rawseti)(lua_State* L, int index, int i);
	extern const std::vector<Symbol> lua_rawsetiSym;

	typedef void (*lua_rawgeti)(lua_State* L, int index, int i);
	extern const std::vector<Symbol> lua_rawgetiSym;

	extern const std::vector<Symbol> CGetSym;
	extern const std::vector<Symbol> gEntListSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CSteam3Server_NotifyClientDisconnect)(void* server, CBaseClient* client);
	extern const std::vector<Symbol> CSteam3Server_NotifyClientDisconnectSym;

	typedef void (*SteamGameServer_Shutdown)();
	extern const std::vector<Symbol> SteamGameServer_ShutdownSym;

	//---------------------------------------------------------------------------------
	// Purpose: holylib Symbols
	//---------------------------------------------------------------------------------
	typedef bool (GMCOMMON_CALLING_CONVENTION* CServerGameDLL_ShouldHideServer)();
	extern const std::vector<Symbol> CServerGameDLL_ShouldHideServerSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* GetGModServerTags)(char* pDest, int iMaxSize, bool unknown);
	extern const std::vector<Symbol> GetGModServerTagsSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CFuncLadder_PlayerGotOn)(void* ladder, void* ply);
	extern const std::vector<Symbol> CFuncLadder_PlayerGotOnSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CFuncLadder_PlayerGotOff)(void* ladder, void* ply);
	extern const std::vector<Symbol> CFuncLadder_PlayerGotOffSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CHL2_Player_ExitLadder)(void* ply);
	extern const std::vector<Symbol> CHL2_Player_ExitLadderSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CBaseEntity_SetMoveType)(void* ent, int, int);
	extern const std::vector<Symbol> CBaseEntity_SetMoveTypeSym;

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

	typedef void (GMCOMMON_CALLING_CONVENTION* CHLTVDirector_StartNewShot)(void* director);
	extern const std::vector<Symbol> CHLTVDirector_StartNewShotSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CHLTVServer_BroadcastEvent)(IServer* server, IGameEvent* event);
	extern const std::vector<Symbol> CHLTVServer_BroadcastEventSym;

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
	typedef FileHandle_t* (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_FindFileInSearchPath)(void* filesystem, CFileOpenInfo&);
	extern const std::vector<Symbol> CBaseFileSystem_FindFileInSearchPathSym;

	typedef bool (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_IsDirectory)(void* filesystem, const char* pFileName, const char* pathID);
	extern const std::vector<Symbol> CBaseFileSystem_IsDirectorySym;

	typedef CSearchPath* (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_FindSearchPathByStoreId)(void* filesystem, int);
	extern const std::vector<Symbol> CBaseFileSystem_FindSearchPathByStoreIdSym;

	typedef long (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_FastFileTime)(void* filesystem, const CSearchPath* path, const char* pFileName);
	extern const std::vector<Symbol> CBaseFileSystem_FastFileTimeSym;

	typedef bool (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_FixUpPath)(void* filesystem, const char* pFileName, char* pFixedUpFileName, int sizeFixedUpFileName);
	extern const std::vector<Symbol> CBaseFileSystem_FixUpPathSym;

	typedef FileHandle_t(GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_OpenForRead)(void* filesystem, const char* pFileNameT, const char* pOptions, unsigned flags, const char* pathID, char** ppszResolvedFilename);
	extern std::vector<Symbol> CBaseFileSystem_OpenForReadSym;

	typedef long (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_GetFileTime)(void* filesystem, const char* pFileName, const char* pPathID);
	extern const std::vector<Symbol> CBaseFileSystem_GetFileTimeSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_AddSearchPath)(void* filesystem, const char* pPath, const char* pathID, SearchPathAdd_t addType);
	extern const std::vector<Symbol> CBaseFileSystem_AddSearchPathSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_AddVPKFile)(void* filesystem, const char* pPath, const char* pathID, SearchPathAdd_t addType);
	extern const std::vector<Symbol> CBaseFileSystem_AddVPKFileSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_RemoveAllMapSearchPaths)(void* filesystem);
	extern const std::vector<Symbol> CBaseFileSystem_RemoveAllMapSearchPathsSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_Close)(void* filesystem, FileHandle_t);
	extern const std::vector<Symbol> CBaseFileSystem_CloseSym;

	typedef const char* (GMCOMMON_CALLING_CONVENTION* CBaseFileSystem_CSearchPath_GetDebugString)(void* searchpath);
	extern const std::vector<Symbol> CBaseFileSystem_CSearchPath_GetDebugStringSym;

	extern const std::vector<Symbol> g_PathIDTableSym;

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

	typedef void* (GMCOMMON_CALLING_CONVENTION* lj_BC_FUNCC)(void* idk);
	extern const std::vector<Symbol> lj_BC_FUNCCSym;

//#ifdef SYSTEM_WINDOWS
	extern const std::vector<Symbol> Client_CLuaGamemode_CallFinishSym;
	extern const std::vector<Symbol> Client_CLuaGamemode_CallWithArgsSym;
	extern const std::vector<Symbol> Client_CLuaGamemode_CallSym;
	extern const std::vector<Symbol> Client_CScriptedEntity_StartFunctionStrSym;
	extern const std::vector<Symbol> Client_CScriptedEntity_StartFunctionSym;
	extern const std::vector<Symbol> Client_CScriptedEntity_CallSym;
	extern const std::vector<Symbol> Client_CScriptedEntity_CallFunctionStrSym;
	extern const std::vector<Symbol> Client_CScriptedEntity_CallFunctionSym;
//#endif

	//---------------------------------------------------------------------------------
	// Purpose: networking Symbols
	//---------------------------------------------------------------------------------
	typedef IChangeFrameList* (*AllocChangeFrameList)(int, int);
	extern const std::vector<Symbol> AllocChangeFrameListSym;

	typedef int (*SendTable_CullPropsFromProxies)(const SendTable*, const int*, int, int, const CSendProxyRecipients*, int, const CSendProxyRecipients*, int, int*, int);
	extern const std::vector<Symbol> SendTable_CullPropsFromProxiesSym;

	typedef void (*SendTable_WritePropList)(const SendTable *, const void*, int, bf_write*, const int, const int*, int);
	extern const std::vector<Symbol> SendTable_WritePropListSym;

	typedef void (*CBaseServer_WriteDeltaEntities)(CBaseServer* pServer, CBaseClient *client, CClientFrame *to, CClientFrame *from, bf_write &pBuf);
	extern const std::vector<Symbol> CBaseServer_WriteDeltaEntitiesSym;

	typedef int (*SendTable_CalcDelta)(const SendTable*, const void*, const int, const void*, const int, int*, int, const int);
	extern const std::vector<Symbol> SendTable_CalcDeltaSym;

	typedef void (__cdecl* SV_DetermineUpdateType)(CEntityWriteInfo& u);
	extern const std::vector<Symbol> SV_DetermineUpdateTypeSym;

	typedef int (*PackedEntity_GetPropsChangedAfterTick)(PackedEntity*, int tick, int* iOutProps, int nMaxOutProps);
	extern const std::vector<Symbol> PackedEntity_GetPropsChangedAfterTickSym;

	typedef void (*CGameServer_SendClientMessages)(CBaseServer* pServer, bool sendSnapshots);
	extern const std::vector<Symbol> CGameServer_SendClientMessagesSym;

	typedef void (*CBaseEntity_GMOD_SetShouldPreventTransmitToPlayer)(CBaseEntity* pEnt, CBasePlayer* pPly, bool bPreventTransmit);
	extern const std::vector<Symbol> CBaseEntity_GMOD_SetShouldPreventTransmitToPlayerSym;

	typedef bool (*CBaseEntity_GMOD_ShouldPreventTransmitToPlayer)(CBaseEntity* pEnt, CBasePlayer* pPly);
	extern const std::vector<Symbol> CBaseEntity_GMOD_ShouldPreventTransmitToPlayerSym;

	extern const std::vector<Symbol> g_FrameSnapshotManagerSym;
	extern const std::vector<Symbol> g_PropTypeFnsSym;
	extern const std::vector<Symbol> g_BSPDataSym;

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

	typedef bool (GMCOMMON_CALLING_CONVENTION* CSteam3Server_NotifyClientConnect)(void*, CBaseClient* client, uint32 unUserID, netadr_t& adr, const void *pvCookie, uint32 ucbCookie);
	extern const std::vector<Symbol> CSteam3Server_NotifyClientConnectSym;

	typedef void (GMCOMMON_CALLING_CONVENTION* CSteam3Server_SendUpdatedServerDetails)(void*);
	extern const std::vector<Symbol> CSteam3Server_SendUpdatedServerDetailsSym;

	typedef bool (GMCOMMON_CALLING_CONVENTION* CSteam3Server_CheckForDuplicateSteamID)(void*, void*);
	extern const std::vector<Symbol> CSteam3Server_CheckForDuplicateSteamIDSym;

	typedef CSteam3Server& (*Steam3ServerT)();
	extern const std::vector<Symbol> Steam3ServerSym;

	typedef void (*SV_InitGameServerSteam)();
	extern const std::vector<Symbol> SV_InitGameServerSteamSym;

	typedef void* (*CGet_SteamUGC)(void*);
	extern const std::vector<Symbol> CGet_SteamUGCSym;

	//---------------------------------------------------------------------------------
	// Purpose: pas Symbols
	//---------------------------------------------------------------------------------
	typedef const byte* (*CM_Vis)(byte* dest, int destlen, int cluster, int visType);
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

	typedef void (*CPhysicsEnvironment_DestroyObject)(IPhysicsEnvironment*, IPhysicsObject*);
	extern const std::vector<Symbol> CPhysicsEnvironment_DestroyObjectSym;

	typedef bool (*CPhysicsEnvironment_Restore)(IPhysicsEnvironment*, physrestoreparams_t const&);
	extern const std::vector<Symbol> CPhysicsEnvironment_RestoreSym;

	typedef bool (*CPhysicsEnvironment_TransferObject)(IPhysicsEnvironment*, IPhysicsObject*, IPhysicsEnvironment*);
	extern const std::vector<Symbol> CPhysicsEnvironment_TransferObjectSym;

	typedef IPhysicsObject* (*CPhysicsEnvironment_CreateSphereObject)(IPhysicsEnvironment*, float radius, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t *pParams, bool isStatic);
	extern const std::vector<Symbol> CPhysicsEnvironment_CreateSphereObjectSym;

	typedef IPhysicsObject* (*CPhysicsEnvironment_UnserializeObjectFromBuffer)(IPhysicsEnvironment*, void *pGameData, unsigned char *pBuffer, unsigned int bufferSize, bool enableCollisions);
	extern const std::vector<Symbol> CPhysicsEnvironment_UnserializeObjectFromBufferSym;

	typedef IPhysicsObject* (*CPhysicsEnvironment_CreatePolyObjectStatic)(IPhysicsEnvironment*, const CPhysCollide *pCollisionModel, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t *pParams);
	extern const std::vector<Symbol> CPhysicsEnvironment_CreatePolyObjectStaticSym;

	typedef IPhysicsObject* (*CPhysicsEnvironment_CreatePolyObject)(IPhysicsEnvironment*, const CPhysCollide *pCollisionModel, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t *pParams);
	extern const std::vector<Symbol> CPhysicsEnvironment_CreatePolyObjectSym;

	typedef void (*CPhysicsEnvironment_D2)(IPhysicsEnvironment*); // Deconstructor
	extern const std::vector<Symbol> CPhysicsEnvironment_D2Sym;

	typedef void (*CPhysicsEnvironment_C2)(IPhysicsEnvironment*); // Constructor
	extern const std::vector<Symbol> CPhysicsEnvironment_C2Sym;

	// Stuff for our do_impact replacement
	typedef void (*IVP_Mindist_D2)(void* mindist);
	extern const std::vector<Symbol> IVP_Mindist_D2Sym;

	extern const std::vector<Symbol> g_pCurrentMindistSym;
	extern const std::vector<Symbol> g_fDeferDeleteMindistSym;

	typedef bool (*GMod_Util_IsPhysicsObjectValid)(IPhysicsObject* obj);
	extern const std::vector<Symbol> GMod_Util_IsPhysicsObjectValidSym;

	typedef void (*PhysFrame)(float deltaTime);
	extern const std::vector<Symbol> PhysFrameSym;

	//---------------------------------------------------------------------------------
	// Purpose: net Symbols
	//---------------------------------------------------------------------------------
	extern const std::vector<Symbol> g_NetIncomingSym; // bf_read*
	extern const std::vector<Symbol> g_WriteSym; // bf_write*
	extern const std::vector<Symbol> g_StartedSym; // bool

	//---------------------------------------------------------------------------------
	// Purpose: luajit Symbols
	//---------------------------------------------------------------------------------
	typedef int (*CLuaInterface_GetType)(GarrysMod::Lua::ILuaInterface* pLua);
	extern const std::vector<Symbol> CLuaInterface_GetTypeSym;

	//---------------------------------------------------------------------------------
	// Purpose: gameserver Symbols
	//---------------------------------------------------------------------------------
	typedef void (*CServerGameClients_GetPlayerLimit)(void*, int&, int&, int&);
	extern const std::vector<Symbol> CServerGameClients_GetPlayerLimitSym;

	typedef void (*CBaseServer_FillServerInfo)(void*, SVC_ServerInfo&);
	extern const std::vector<Symbol> CBaseServer_FillServerInfoSym;

	typedef bool (*CBaseClient_SetSignonState)(void* client, int state, int spawncount);
	extern const std::vector<Symbol> CBaseClient_SetSignonStateSym;

	typedef bool (*CBaseServer_IsMultiplayer)(void* srv);
	extern const std::vector<Symbol> CBaseServer_IsMultiplayerSym;

	typedef bool (*GModDataPack_IsSingleplayer)(void* datapack);
	extern const std::vector<Symbol> GModDataPack_IsSingleplayerSym;

	typedef bool (*CBaseClient_ShouldSendMessages)(void* client);
	extern const std::vector<Symbol> CBaseClient_ShouldSendMessagesSym;

	typedef void (*CBaseServer_CheckTimeouts)(void* server);
	extern const std::vector<Symbol> CBaseServer_CheckTimeoutsSym;

	typedef void (*CBaseClient_OnRequestFullUpdate)(void* client);
	extern const std::vector<Symbol> CBaseClient_OnRequestFullUpdateSym;

	typedef void (*CGameClient_SpawnPlayer)(void* client);
	extern const std::vector<Symbol> CGameClient_SpawnPlayerSym;

	typedef bool (*CBaseServer_ProcessConnectionlessPacket)(void* server, netpacket_s* packet);
	extern const std::vector<Symbol> CBaseServer_ProcessConnectionlessPacketSym;

	typedef int (*NET_SendPacket)(INetChannel *chan, int sock, const netadr_t &to, const unsigned char *data, int length, bf_write *pVoicePayload /* = NULL */, bool bUseCompression /*=false*/);
	extern const std::vector<Symbol> NET_SendPacketSym;

	typedef bool (*CNetChan_CreateFragmentsFromBuffer)(CNetChan* channel, bf_write *buffer, int stream);
	extern const std::vector<Symbol> CNetChan_CreateFragmentsFromBufferSym;

	typedef bool (*CNetChan_SendSubChannelData)(CNetChan* channel, bf_write &buf);
	extern const std::vector<Symbol> CNetChan_SendSubChannelDataSym;

	typedef void (*CNetChan_FlowNewPacket)(CNetChan* channel, int flow, int seqnr, int acknr, int nChoked, int nDropped, int nSize);
	extern const std::vector<Symbol> CNetChan_FlowNewPacketSym;

	typedef void (*CNetChan_FlowUpdate)(CNetChan* channel, int flow, int addbytes);
	extern const std::vector<Symbol> CNetChan_FlowUpdateSym;

	typedef int (*CNetChan_SendDatagram)(CNetChan* chan, bf_write *datagram);
	extern const std::vector<Symbol> CNetChan_SendDatagramSym;

	typedef void (*CNetChan_UpdateSubChannels)(CNetChan* channel);
	extern const std::vector<Symbol> CNetChan_UpdateSubChannelsSym;

	typedef void (*CNetChan_CheckWaitingList)(CNetChan* channel, int nList);
	extern const std::vector<Symbol> CNetChan_CheckWaitingListSym;

	typedef int (*CNetChan_ProcessPacketHeader)(CNetChan* chan, netpacket_t* packet);
	extern const std::vector<Symbol> CNetChan_ProcessPacketHeaderSym;

	typedef void (*CNetChan_D2)(CNetChan* chan);
	extern const std::vector<Symbol> CNetChan_D2Sym;

	typedef INetChannel* (*NET_CreateNetChannel)(int socket, netadr_t* adr, const char* name, INetChannelHandler* handler, bool bForceNewCHannel, int nProtocolVersion);
	extern const std::vector<Symbol> NET_CreateNetChannelSym;

	typedef void (*NET_RemoveNetChannel)(INetChannel* chan, bool bDeleteNetChan);
	extern const std::vector<Symbol> NET_RemoveNetChannelSym;

	typedef void (*Filter_SendBan)(const netadr_t& adr);
	extern const std::vector<Symbol> Filter_SendBanSym;

	//---------------------------------------------------------------------------------
	// Purpose: cvar Symbols
	//---------------------------------------------------------------------------------
	typedef void (*CCvar_RegisterConCommand)(ICvar*, ConCommandBase*);
	extern const std::vector<Symbol> CCvar_RegisterConCommandSym;

	typedef void (*CCvar_UnregisterConCommand)(ICvar*, ConCommandBase*);
	extern const std::vector<Symbol> CCvar_UnregisterConCommandSym;

	typedef void (*CCvar_UnregisterConCommands)(ICvar*, int);
	extern const std::vector<Symbol> CCvar_UnregisterConCommandsSym;

	typedef const ConCommandBase* (*CCvar_FindCommandBaseConst)(ICvar*, const char* name);
	extern const std::vector<Symbol> CCvar_FindCommandBaseConstSym;

	typedef ConCommandBase* (*CCvar_FindCommandBase)(ICvar*, const char* name);
	extern const std::vector<Symbol> CCvar_FindCommandBaseSym;

	//---------------------------------------------------------------------------------
	// Purpose: lagcompensation Symbols
	//---------------------------------------------------------------------------------
	typedef void (*UTIL_TraceEntity1)(CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, unsigned int mask, const IHandleEntity *pIgnore, int nCollisionGroup, trace_t *ptr);
	extern const std::vector<Symbol> UTIL_TraceEntity1Sym;

	typedef void (*UTIL_TraceEntity1)(CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, unsigned int mask, const IHandleEntity *pIgnore, int nCollisionGroup, trace_t *ptr);
	extern const std::vector<Symbol> UTIL_TraceEntity1Sym;

	typedef void (*UTIL_TraceEntity2)(CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, unsigned int mask, trace_t *ptr);
	extern const std::vector<Symbol> UTIL_TraceEntity2Sym;
}