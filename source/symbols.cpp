#include "symbols.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static Symbol NULL_SIGNATURE = Symbol::FromSignature("");

namespace Symbols
{
	//---------------------------------------------------------------------------------
	// Purpose: All Base Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> InitLuaClassesSym = {
		Symbol::FromName("_Z14InitLuaClassesP13ILuaInterface"),
		Symbol::FromSignature("\x48\x8B\x05****\x48\x85\xC0**\x8B\x50\x10\x85\xD2**\x55\x48\x89\xE5\x53\x31\xDB******\x48\x8B*\x48\x8B\x3C\xD8\xE8\xD4"), // 48 8B 05 ?? ?? ?? ?? 48 85 C0 ?? ?? 8B 50 10 85 D2 ?? ?? 55 48 89 E5 53 31 DB ?? ?? ?? ?? ?? ?? 48 8B ?? 48 8B 3C D8 E8 D4
	};

	const std::vector<Symbol> CLuaInterface_ShutdownSym = {
		Symbol::FromName("_ZN13CLuaInterface8ShutdownEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x54\x49\x89\xFC\x53\x4D\x8D"), // 55 48 89 E5 41 55 41 54 49 89 FC 53 4D 8D
	};

	const std::vector<Symbol> UsermessagesSym = {
		Symbol::FromName("_ZL14g_UserMessages"),
		Symbol::FromSignature("\x55\x48\x8D*****\xBA\x01\x00\x00\x00\x48\x89\xE5\x53\x48\x83\xEC\x08\x2A\x2A\x2A\x2A"), // 55 48 8D ?? ?? ?? ?? ?? BA 01 00 00 00 48 89 E5 53 48 83 EC 08 2A 2A 2A 2A
		Symbol::FromSignature("\x2A\x2A\x2A\x2A\xE8****\x8B\xD0\x83\xFA\xFF**\xFF\x75\x0C"), // UserMessageBegin - ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B D0 83 FA FF ?? ?? FF 75 0C
	};

	const std::vector<Symbol> CBaseEntity_CalcAbsolutePositionSym = { // 64x = "%s[%i]:SetPos( %f %f %f ): Ignoring unreasonable position." -> Entity__SetPos -> CBaseEntity::SetAbsOrigin -> CBaseEntity::CalcAbsolutePosition
		Symbol::FromName("_ZN11CBaseEntity20CalcAbsolutePositionEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x83\xEC\x60\x8B\x87\xB0\x01\x00\x00"), // 55 48 89 E5 41 56 41 55 41 54 53 48 89 FB 48 83 EC 60 8B 87 B0 01 00 00
	};

	const std::vector<Symbol> CBaseAnimating_InvalidateBoneCacheSym = {
		Symbol::FromName("_ZN14CBaseAnimating19InvalidateBoneCacheEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x48\x8B\xBF\x58\x16\x00\x00"), // 55 48 89 E5 48 8B BF 58 16 00 00
		// How to hopefully find it(Still a pain): Search for "%5.2f : %s : %s : %5.3f\n" -> CBaseAnimating::StudioFrameAdvance() -> StudioFrameAdvanceInternal() -> Studio_InvalidateBoneCacheIfNotMatching() -> Find CBaseAnimating::InvalidateBoneCache by checking which function calls it with -1.0f
		// Else: Search for 'aim_yaw' -> CNPC_SecurityCamera__UpdateFacing -> CBaseAnimating::InvalidateBoneCache
	};

	const std::vector<Symbol> CBaseEntity_PostConstructorSym = { // Search for 'Setting CBaseEntity to non-brush model %s' then search for the vtable and it will be 2 functions bellow
		Symbol::FromName("_ZN11CBaseEntity15PostConstructorEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x18\x48\x85\xF6\x74\x2A\xE8\x2A\x2A\x2A\x2A"), // 55 48 89 E5 53 48 89 FB 48 83 EC 18 48 85 F6 74 ? E8 ? ? ? ?
	};

	const std::vector<Symbol> CCollisionProperty_MarkSurroundingBoundsDirtySym = { //Search for 'Clamping SetLocalVelocity(%f,%f,%f) on %s' to get CBaseEntity::SetLocalVelocity then into it find CBaseEntity::InvalidatePhysicsRecursive
		Symbol::FromName("_ZN18CCollisionProperty24MarkPartitionHandleDirtyEv"),
		Symbol::FromSignature("\x55\x48\x8B\x57\x08\x48\x89\xE5\x48\x8B\x42\x40"), //55 48 8B 57 08 48 89 E5 48 8B 42 40
	};

	const Symbol lua_rawsetiSym = Symbol::FromName("lua_rawseti");

	const Symbol lua_rawgetiSym = Symbol::FromName("lua_rawgeti");

	const Symbol lj_tab_newSym = Symbol::FromName("lj_tab_new");

	const Symbol lj_gc_barrierfSym = Symbol::FromName("lj_gc_barrierf");

	const Symbol lj_tab_getSym = Symbol::FromName("lj_tab_get");

	const Symbol lua_setfenvSym = Symbol::FromName("lua_setfenv");

	const Symbol lua_touserdataSym = Symbol::FromName("lua_touserdata");

	const Symbol lua_typeSym = Symbol::FromName("lua_type");

	const Symbol luaL_checklstringSym = Symbol::FromName("luaL_checklstring");

	const Symbol lua_pcallSym = Symbol::FromName("lua_pcall");

	const Symbol lua_insertSym = Symbol::FromName("lua_insert");

	const Symbol lua_tobooleanSym = Symbol::FromName("lua_toboolean");

	const std::vector<Symbol> CGetSym = { // 64x ToDo
		Symbol::FromName("get"),
	};

	const std::vector<Symbol> gEntListSym = { // 64x = ents.GetAll
		Symbol::FromName("gEntList"),
		Symbol::FromSignature("\x55\x48\x8D\x3D\x2A\x2A\x2A\x2A\x48\x89\xE5\x53\x48\x83\xEC\x08\xC6\x05\xC9\x83\x43\x01\x00", 0x111),
	};

	const std::vector<Symbol> CSteam3Server_NotifyClientDisconnectSym = { // 64x = Search for "S3" and then go through every function upwards till you find one that somewhat matches the ASM of the 32x version.
		Symbol::FromName("_ZN13CSteam3Server22NotifyClientDisconnectEP11CBaseClient"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x53\x48\x89\xF3\x48\x83\xEC\x20\x48\x85\xF6"), // 55 48 89 E5 41 54 53 48 89 F3 48 83 EC 20 48 85 F6
	};

	const Symbol SteamGameServer_ShutdownSym = Symbol::FromName("SteamGameServer_Shutdown"); // Same symbol for all versions.

	const Symbol GMOD_LoadBinaryModuleSym = Symbol::FromName("GMOD_LoadBinaryModule"); // Same symbol for all versions.

	const std::vector<Symbol> CM_VisSym = { // CM_Vis:
		Symbol::FromName("_Z6CM_VisPhiii"),
		Symbol::FromSignature("\x55\x83\xF9\x02\x48\x89\xE5\x41\x54\x53\x48\x89\xFB\x89\xD7\x89\xCA"), // 55 83 F9 02 48 89 E5 41 54 53 48 89 FB 89 D7 89 CA
	};

	const std::vector<Symbol> CBaseEntity_GetLuaEntitySym = {
		Symbol::FromName("_ZN11CBaseEntity12GetLuaEntityEv"),
	};

	const std::vector<Symbol> CGameEventManager_CreateEventSym = {
		Symbol::FromName("_ZN17CGameEventManager11CreateEventEPKcb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x54\x53\x48\x89\xF3\x48\x83\xEC\x08\x48\x85\xF6**\x80\x3E\x00"), // 55 48 89 E5 41 55 41 54 53 48 89 F3 48 83 EC 08 48 85 F6 ?? ?? 80 3E 00
	};

	//---------------------------------------------------------------------------------
	// Purpose: holylib Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CServerGameDLL_ShouldHideServerSym = {
		Symbol::FromName("_ZN14CServerGameDLL16ShouldHideServerEv"),
		Symbol::FromSignature("\x48\x8B\x3D\xC1\x9B\x18\x01\x55\x48\x89\xE5"), // 48 8B 3D C1 9B 18 01 55 48 89 E5
	};

	const std::vector<Symbol> GetGModServerTagsSym = {
		Symbol::FromName("_Z17GetGModServerTagsPcjb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x4C\x8D\xAD\x30\xFF\xFF\xFF"), // 55 48 89 E5 41 57 41 56 41 55 4C 8D AD 30 FF FF FF
	};

	const std::vector<Symbol> CFuncLadder_PlayerGotOnSym = { // "reserved_spot" -> Find function -> Find Caller
		Symbol::FromName("_ZN11CFuncLadder11PlayerGotOnEP11CBasePlayer"),
		Symbol::FromSignature("\x55\x48\x89\xF2\x0F\x57\xC0\x48\x89\xE5"), // 55 48 89 F2 0F 57 C0 48 89 E5
	};

	const std::vector<Symbol> CFuncLadder_PlayerGotOffSym = { // "reserved_spot" -> Find function -> Find Caller
		Symbol::FromName("_ZN11CFuncLadder12PlayerGotOffEP11CBasePlayer"),
		Symbol::FromSignature("\x55\x48\x89\xF8\x48\x89\xF2\x48\x8D\xBF"), // 55 48 89 F8 48 89 F2 48 8D BF
	};

	const std::vector<Symbol> CHL2_Player_ExitLadderSym = {
		Symbol::FromName("_ZN11CHL2_Player10ExitLadderEv"),
		NULL_SIGNATURE, // ToDo
	};

	const std::vector<Symbol> CBaseEntity_SetMoveTypeSym = {
		Symbol::FromName("_ZN11CBaseEntity11SetMoveTypeE10MoveType_t13MoveCollide_t"),
		NULL_SIGNATURE, // ToDo
	};

	const std::vector<Symbol> CHostState_State_ChangeLevelMPSym = {
		Symbol::FromName("_Z23HostState_ChangeLevelMPPKcS0_"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x49\x89\xFC\x53\x48\x89\xF3*****\x48\x89\xC7"), // 55 48 89 E5 41 54 49 89 FC 53 48 89 F3 ?? ?? ?? ?? ?? 48 89 C7
		Symbol::FromSignature("\x55\x8B\xEC*****\x8B\xC8*****\x68\x00\x01\x00\x00\xFF\x75\x08"), // 55 8B EC ?? ?? ?? ?? ?? 8B C8 ?? ?? ?? ?? ?? 68 00 01 00 00 FF 75 08
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x8B\xFA\x48\x8B\xD9*****\x48\x8B\xC8"), // 48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 ?? ?? ?? ?? ?? 48 8B C8
	};

	//---------------------------------------------------------------------------------
	// Purpose: gameevent Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CBaseClient_ProcessListenEventsSym = {
		Symbol::FromName("_ZN11CBaseClient19ProcessListenEventsEP16CLC_ListenEvents"),
		// FUCK. The symbol for 64x is broken or I got the WRONG ONE. WHY DOES CHLTVClient have the same SHIT and SAME vprof name :(
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x49\x89\xF4\x53\x48\x8B\x1D****\x8B\x93\x0C\x10\x00\x00\x85\xD2\x41\x0F\x95\xC5******\x41\x0F\xB6\x74\x24\x20\x49"), // 55 48 89 E5 41 56 41 55 41 54 49 89 F4 53 48 8B 1D ?? ?? ?? ?? 8B 93 0C 10 00 00 85 D2 41 0F 95 C5 ?? ?? ?? ?? ?? ?? 41 0F B6 74 24 20 49
	};

	const std::vector<Symbol> CGameEventManager_AddListenerSym = { // Fk this. No 64x
		Symbol::FromName("_ZN17CGameEventManager11AddListenerEPvP20CGameEventDescriptori"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: serverplugin Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CPlugin_LoadSym = {
		Symbol::FromName("_ZN7CPlugin4LoadEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x49\x89\xFE\x41\x55\x41\x54\x49\x89\xF4\xBE\x2E"), // 55 48 89 E5 41 56 49 89 FE 41 55 41 54 49 89 F4 BE 2E
	};

	//---------------------------------------------------------------------------------
	// Purpose: sourcetv Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CHLTVClient_ProcessGMod_ClientToServerSym = {
		Symbol::FromName("_ZN11CHLTVClient26ProcessGMod_ClientToServerEP23CLC_GMod_ClientToServer"),
	};

	const std::vector<Symbol> CHLTVServer_CHLTVServerSym = {
		Symbol::FromName("_ZN11CHLTVServerC2Ev"),
	};

	const std::vector<Symbol> CHLTVServer_DestroyCHLTVServerSym = {
		Symbol::FromName("_ZN11CHLTVServerD2Ev"),
	};

	const std::vector<Symbol> COM_IsValidPathSym = {
		Symbol::FromName("_Z15COM_IsValidPathPKc"),
	};

	const std::vector<Symbol> CHLTVDemoRecorder_StartRecordingSym = {
		Symbol::FromName("_ZN17CHLTVDemoRecorder14StartRecordingEPKcb"),
	};

	const std::vector<Symbol> CHLTVDemoRecorder_StopRecordingSym = {
		Symbol::FromName("_ZN17CHLTVDemoRecorder13StopRecordingEv"),
	};

	const std::vector<Symbol> CHLTVClient_ExecuteStringCommandSym = {
		Symbol::FromName("_ZN11CHLTVClient20ExecuteStringCommandEPKc"),
	};

	const std::vector<Symbol> CHLTVClient_DeconstructorSym = {
		Symbol::FromName("_ZN11CHLTVClientD1Ev"),
	};

	const std::vector<Symbol> CHLTVDirector_StartNewShotSym = {
		Symbol::FromName("_ZN13CHLTVDirector12StartNewShotEv"),
	};

	const std::vector<Symbol> CHLTVServer_BroadcastEventSym = {
		Symbol::FromName("_ZN11CHLTVServer14BroadcastEventEP10IGameEvent"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: threadpoolfix Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CThreadPool_ExecuteToPrioritySym = {
		Symbol::FromName("_ZN11CThreadPool17ExecuteToPriorityE13JobPriority_tPFbP4CJobE"),
	};

	const std::vector<Symbol> CThreadPool_StartSym = { // Defaulting to limit of 3 worker threads
		NULL_SIGNATURE,
		Symbol::FromSignature("\x55\x48\x89\xF1\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54"), // 55 48 89 F1 48 89 E5 41 57 41 56 41 55 41 54
	};

	const std::vector<Symbol> CBaseFileSystem_InitAsyncSym = { // Async I/O disabled from command line\n
		NULL_SIGNATURE,
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x81\xEC\x28\x01\x00\x00"), // 55 48 89 E5 53 48 89 FB 48 81 EC 28 01 00 00
	};

	const std::vector<Symbol> CBaseFileSystem_ShutdownAsyncSym = {
		NULL_SIGNATURE,
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x08\x48\x83\xBF\x48\x01\x00\x00\x00**\x48\x8B\x07"), // 55 48 89 E5 53 48 89 FB 48 83 EC 08 48 83 BF 48 01 00 00 00 ?? ?? 48 8B 07
	};

	//---------------------------------------------------------------------------------
	// Purpose: precachefix Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CVEngineServer_PrecacheModelSym = {
		Symbol::FromName("_ZN14CVEngineServer13PrecacheModelEPKcb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x44\x0F\xB6\xE2\x53\x48\x89\xF3\x44\x89\xE6\x48\x89\xDF"), // 55 48 89 E5 41 54 44 0F B6 E2 53 48 89 F3 44 89 E6 48 89
	};

	const std::vector<Symbol> CVEngineServer_PrecacheGenericSym = {
		Symbol::FromName("_ZN14CVEngineServer15PrecacheGenericEPKcb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xF3\x48\x83\xEC\x08\x0F\xB6\xF2\x48\x89\xDF"), // 55 48 89 E5 53 48 89 F3 48 83 EC 08 0F B6 F2 48 89 DF
	};

	const std::vector<Symbol> SV_FindOrAddModelSym = {
		Symbol::FromName("_Z17SV_FindOrAddModelPKcb"),
		Symbol::FromSignature("\x55\x40\x80\xFE\x01\x48\x89\xFE\x48\x89\xE5\x48\x8B\x3D****\x19\xD2\x5D\x83\xE2\xFE\x31\xC9\x83\xC2\x03"), // 55 40 80 FE 01 48 89 FE 48 89 E5 48 8B 3D ?? ?? ?? ?? 19 D2 5D 83 E2 FE 31 C9 83 C2 03
	};

	const std::vector<Symbol> SV_FindOrAddGenericSym = {
		Symbol::FromName("_Z19SV_FindOrAddGenericPKcb"),
		Symbol::FromSignature("\x55\x40\x80\xFE\x01\x48\x89\xFE\x48\x89\xE5\x48\x8B\x3D\xC6***\x19\xD2"), // 55 40 80 FE 01 48 89 FE 48 89 E5 48 8B 3D C6 ?? ?? ?? 19 D2
	};

	//---------------------------------------------------------------------------------
	// Purpose: stringtable Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CNetworkStringTable_DeleteAllStringsSym = { // Error reading string table %s\n - CNetworkStringTableContainer::ReadStringTables -> Fidn the CNetworkStringTable::ReadStringTable call -> Find the CNetworkStringTable::DeleteAllStrings call
		Symbol::FromName("_ZN19CNetworkStringTable16DeleteAllStringsEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x08\x48\x8B\x7F\x50"), // 55 48 89 E5 53 48 89 FB 48 83 EC 08 48 8B 7F 50
	};

	const std::vector<Symbol> CNetworkStringTable_DeconstructorSym = { // Table %s\n - Brings you to CNetworkStringTable::Dump
		Symbol::FromName("_ZN19CNetworkStringTableD0Ev"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x08\xE8\x8F\xFF\xFF\xFF\x48\x83\xC4\x08\x48\x89\xDF\x5B\x5D\xE9**\xEE\xFF"), // 55 48 89 E5 53 48 89 FB 48 83 EC 08 E8 8F FF FF FF 48 83 C4 08 48 89 DF 5B 5D E9 ?? ?? EE FF
	};

	//---------------------------------------------------------------------------------
	// Purpose: surffix Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CGameMovement_TryPlayerMoveSym = { // Find 'Player.Swim' then find the CGameMovement__CheckJumpButton and find the xref and go 3 bellow
		Symbol::FromName("_ZN13CGameMovement13TryPlayerMoveEP6VectorP10CGameTrace"),
		Symbol::FromSignature("\x55\x0F\x57\xD2\x48\x89\xE5\x41\x57\x41\x56\x48\x8D\x85\xB0\xFE\xFF\xFF"), //55 0F 57 D2 48 89 E5 41 57 41 56 48 8D 85 B0 FE FF FF
	};

	const std::vector<Symbol> CGameMovement_ClipVelocitySym = { // Find it into CGameMovement_TryPlayerMoveSym
		Symbol::FromName("_ZN13CGameMovement12ClipVelocityER6VectorS1_S1_f"),
		Symbol::FromSignature("\x55\x0F\x57\xDB\x31\xC0\xF3\x0F\x10\x52\x08"), //55 0F 57 DB 31 C0 F3 0F 10 52 08
	};

	const std::vector<Symbol> CBaseEntity_GetGroundEntitySym = { // Find 'Trace using: %s\n' and then it's bellow
		Symbol::FromName("_ZN11CBaseEntity15GetGroundEntityEv"),
		Symbol::FromSignature("\x48\x8B\x05\x2A\x2A\x2A\x2A\x55\x48\x89\xE5\x48\x8B\x08\x8B\x87\xD0\x02\x00\x00"), //48 8B 05 ? ? ? ? 55 48 89 E5 48 8B 08 8B 87 D0 02 00 00
	};

	const std::vector<Symbol> CTraceFilterSimple_ShouldHitEntitySym = { // Find '%3.2f: NextBotGroundLocomotion::GetBot()->OnLandOnGround' to get NextBotGroundLocomotion__OnLandOnGround then it should be above
		Symbol::FromName("_ZN18CTraceFilterSimple15ShouldHitEntityEP13IHandleEntityi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x49\x89\xFE\x41\x55\x41\x89\xD5\x41\x54\x53"), //55 48 89 E5 41 56 49 89 FE 41 55 41 89 D5 41 54 53
	};

	const std::vector<Symbol> MoveHelperServerSym = {// Find 'CBasePlayer::PhysicsSimulate' and then you'll get CBasePlayer__PhysicsSimulate and then it's into it
		Symbol::FromName("_Z16MoveHelperServerv"),
		Symbol::FromSignature("\x80\x3D\x29\xF6\x18\x01\x00"), // 80 3D 29 F6 18 01 00
	};

	//---------------------------------------------------------------------------------
	// Purpose: pvs Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CGMOD_Player_SetupVisibilitySym = {//Find 'predicted_' then search for CGMOD_Player::CreateViewModel and once you got it, find the function bellow it in the xref VTable and you got it
		Symbol::FromName("_ZN12CGMOD_Player15SetupVisibilityEP11CBaseEntityPhi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x49\x89\xFD\x41\x54\x49\x89\xF4\x53\x48\x83\xEC\x08\xE8\x2A\x2A\x2A\x2A\x48\x8B\x1D\x2A\x2A\x2A\x2A"), // 55 48 89 E5 41 55 49 89 FD 41 54 49 89 F4 53 48 83 EC 08 E8 ? ? ? ? 48 8B 1D ? ? ? ?
	};

	const std::vector<Symbol> CServerGameEnts_CheckTransmitSym = { // Find 'cl_updaterate' and you'll find CServerGameClients__ClientSettingsChanged then search for xref to get vtable and it will be above
		Symbol::FromName("_ZN15CServerGameEnts13CheckTransmitEP18CCheckTransmitInfoPKti"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x49\x89\xF6\x31\xF6"), //55 48 89 E5 41 57 41 56 49 89 F6 31 F6
	};

	//---------------------------------------------------------------------------------
	// Purpose: filesystem Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CBaseFileSystem_FindFileInSearchPathSym = {
		Symbol::FromName("_ZN15CBaseFileSystem20FindFileInSearchPathER13CFileOpenInfo"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xFF\x41\x56\x41\x55\x41\x54\x53\x48\x89\xF3"), // 55 48 89 E5 41 57 49 89 FF 41 56 41 55 41 54 53 48 89 F3
	};

	const std::vector<Symbol> CBaseFileSystem_IsDirectorySym = {
		Symbol::FromName("_ZN15CBaseFileSystem11IsDirectoryEPKcS1_"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x4C\x8D\xAD\x38***\x41\x54"), // 55 48 89 E5 41 57 41 56 41 55 4C 8D AD 38 ?? ?? ?? 41 54
	};

	const std::vector<Symbol> CBaseFileSystem_FindSearchPathByStoreIdSym = {
		// Find 'CBaseFileSystem::GetFileTime' then you have CBaseFileSystem__GetPathTime, go to xref and find in the vtable 3 function bellow, you'll get CBaseFileSystem__RegisterFileWhitelist which use CFileTracker2__GetFilesToUnloadForWhitelistChange which use it
		Symbol::FromName("_ZN15CBaseFileSystem23FindSearchPathByStoreIdEi"),
		Symbol::FromSignature("\x55\x0F\xB7\x87\x88\x00\x00\x00"), //55 0F B7 87 88 00 00 00
	};

	const std::vector<Symbol> CBaseFileSystem_FastFileTimeSym = {// Find 'CBaseFileSystem::GetFileTime' then you have CBaseFileSystem__GetPathTime and it's there
		Symbol::FromName("_ZN15CBaseFileSystem12FastFileTimeEPKNS_11CSearchPathEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x49\x89\xFD\x41\x54\x49\x89\xD4"), //55 48 89 E5 41 57 41 56 41 55 49 89 FD 41 54 49 89 D4
	};

	const std::vector<Symbol> CBaseFileSystem_FixUpPathSym = {//Find 'BASE_PATH'
		Symbol::FromName("_ZN15CBaseFileSystem9FixUpPathEPKcPci"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x49\x89\xFD\x41\x54\x49\x89\xD4"), //55 48 89 E5 41 57 41 56 41 55 49 89 FD 41 54 49 89 D4
	};

	std::vector<Symbol> CBaseFileSystem_OpenForReadSym = {
		Symbol::FromName("_ZN15CBaseFileSystem11OpenForReadEPKcS1_jS1_PPc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xD7\x41\x56\x4D\x89\xC6\x41\x55\x4D\x89\xCD\x41\x54"), // 55 48 89 E5 41 57 49 89 D7 41 56 4D 89 C6 41 55 4D 89 CD 41 54
	};

	const std::vector<Symbol> CBaseFileSystem_GetFileTimeSym = {
		Symbol::FromName("_ZN15CBaseFileSystem11GetFileTimeEPKcS1_"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x49\x89\xFE\x41\x55\x41\x54\x53\x48\x89\xD3"), // 55 48 89 E5 41 57 41 56 49 89 FE 41 55 41 54 53 48 89 D3
	};

	const std::vector<Symbol> CBaseFileSystem_AddSearchPathSym = {
		Symbol::FromName("_ZN15CBaseFileSystem13AddSearchPathEPKcS1_j"),
		Symbol::FromSignature("\x55\x45\x31\xC9\x41\xB8\x01\x00\x00\x00"), // 55 45 31 C9 41 B8 01 00 00 00
	};

	const std::vector<Symbol> CBaseFileSystem_AddVPKFileSym = {//Search for 'DUPLICATE: [%s]\n'
		Symbol::FromName("_ZN15CBaseFileSystem10AddVPKFileEPKcS1_j"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x49\x89\xD6\x41\x55\x41\x54\x53\x48\x89\xFB"), // 55 48 89 E5 41 57 41 56 49 89 D6 41 55 41 54 53 48 89 FB
	};

	const std::vector<Symbol> CBaseFileSystem_RemoveAllMapSearchPathsSym = {
		Symbol::FromName("_ZN15CBaseFileSystem23RemoveAllMapSearchPathsEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x49\x89\xD6\x41\x55\x41\x54\x53\x48\x89\xFB"), // 55 48 89 E5 41 57 41 56 49 89 D6 41 55 41 54 53 48 89 FB
	};

	const std::vector<Symbol> CBaseFileSystem_CloseSym = {
		Symbol::FromName("_ZN15CBaseFileSystem5CloseEPv"),
		Symbol::FromSignature("\x55\xBE\x00\x00\x00\x80\x48\x89\xE5"), // 55 BE 00 00 00 80 48 89 E5
	};

	const std::vector<Symbol> CBaseFileSystem_CSearchPath_GetDebugStringSym = {
		Symbol::FromName("_ZNK15CBaseFileSystem11CSearchPath14GetDebugStringEv"),
	};

	const std::vector<Symbol> g_PathIDTableSym = {
		Symbol::FromName("g_PathIDTable"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: concommand Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> ConCommand_IsBlockedSym = {
		Symbol::FromName("_Z20ConCommand_IsBlockedPKc"),
		Symbol::FromSignature("\x48\x8B\x05****\x55\x48\x89\xE5\x41\x54\x49\x89\xFC\x53\x48\x8B*\x80\x78\x70\x00"), // 48 8B 05 ?? ?? ?? ?? 55 48 89 E5 41 54 49 89 FC 53 48 8B ?? 80 78 70 00
		Symbol::FromSignature("\x55\x8B\xEC\xA1****\x57\x8B\x7D\x08"), // 55 8B EC A1 ?? ?? ?? ?? 57 8B 7D 08
	};

	//---------------------------------------------------------------------------------
	// Purpose: vprof Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CLuaGamemode_CallFinishSym = {
		Symbol::FromName("_ZN12CLuaGamemode10CallFinishEi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x41\x89\xF4\x53\x48\x8B\x1D****\x8B\x93\x0C\x10\x00\x00"), // 55 48 89 E5 41 56 41 55 41 54 41 89 F4 53 48 8B 1D ?? ?? ?? ?? 8B 93 0C 10 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x0D****\x53\x56\x8B\xB1\x0C\x10\x00\x00\x85\xF6**\x6A\x04\x6A\x00\x68****\x6A\x00\x68**********\x8B\x0D****\x8B\x45\x08"), //  55 8B EC 8B 0D ?? ?? ?? ?? 53 56 8B B1 0C 10 00 00 85 F6 ?? ?? 6A 04 6A 00 68 ?? ?? ?? ?? 6A 00 68 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 45 08
	};

	const std::vector<Symbol> CLuaGamemode_CallWithArgsSym = { // int version - Look at the difference in the call to [GM:CallWithArgs - !ThreadInMainThread]
		Symbol::FromName("_ZN12CLuaGamemode12CallWithArgsEi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x89\xF7\x41\x56\x41\x55\x49\x89\xFD\x41\x54\x53\x48\x83\xEC\x18"), // 55 48 89 E5 41 57 41 89 F7 41 56 41 55 49 89 FD 41 54 53 48 83 EC 18
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x57\x8B\xF9\x8B\x0D****\x8B\xB1\x0C\x10\x00\x00\x85\xF6**\x6A\x04\x6A\x00\x68****\x6A\x00\x68**********\xFF\x15****\x84\xC0**\x8B\x0D****\xFF\x75\x08\x8B\x01\xFF\x90\xEC\x01\x00\x00\x50\x68****\xFF\x15****\x8B\x07\x8B\xCF\x8B\x80\x8C\x00\x00\x00\xFF\xD0\x84\xC0**\x8B\x47\x3C\x8D\x4F\x3C"), //  55 8B EC 53 56 57 8B F9 8B 0D ?? ?? ?? ?? 8B B1 0C 10 00 00 85 F6 ?? ?? 6A 04 6A 00 68 ?? ?? ?? ?? 6A 00 68 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 84 C0 ?? ?? 8B 0D ?? ?? ?? ?? FF 75 08 8B 01 FF 90 EC 01 00 00 50 68 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B 07 8B CF 8B 80 8C 00 00 00 FF D0 84 C0 ?? ?? 8B 47 3C 8D 4F 3C
	};

	const std::vector<Symbol> CLuaGamemode_CallWithArgsStrSym = {
		Symbol::FromName("_ZN12CLuaGamemode12CallWithArgsEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xF7\x41\x56\x41\x55\x49\x89\xFD\x41\x54\x53\x48\x83\xEC\x18*******\x8B\x93\x0C\x10\x00\x00\x85\xD2\x41\x0F\x95\xC6"), // 55 48 89 E5 41 57 49 89 F7 41 56 41 55 49 89 FD 41 54 53 48 83 EC 18 ?? ?? ?? ?? ?? ?? ?? 8B 93 0C 10 00 00 85 D2 41 0F 95 C6
	};

	const std::vector<Symbol> CLuaGamemode_CallStrSym = { // const char* version - Look at the difference in the call to [GM:CallWithArgs - !ThreadInMainThread] Also search for "CLuaGamemode::" not CLuaGamemode::Call on 64x
		Symbol::FromName("_ZN12CLuaGamemode4CallEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x49\x89\xF6\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x83\xEC\x08*******\x8B\x93\x0C\x10\x00\x00\x85\xD2\x41\x0F\x95\xC5******\xE8"), // 55 48 89 E5 41 57 41 56 49 89 F6 41 55 41 54 49 89 FC 53 48 83 EC 08 ?? ?? ?? ?? ?? ?? ?? 8B 93 0C 10 00 00 85 D2 41 0F 95 C5 ?? ?? ?? ?? ?? ?? E8
	};

	const std::vector<Symbol> CLuaGamemode_CallSym = {
		Symbol::FromName("_ZN12CLuaGamemode4CallEi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x89\xF6\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x83\xEC\x08"), // 55 48 89 E5 41 57 41 56 41 89 F6 41 55 41 54 49 89 FC 53 48 83 EC 08
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x57\x8B\xF9\x8B\x0D****\x8B\xB1\x0C\x10\x00\x00\x85\xF6**\x6A\x04\x6A\x00\x68****\x6A\x00\x68**********\xFF\x15****\x84\xC0**\x8B\x0D****\xFF\x75\x08\x8B\x01\xFF\x90\xEC\x01\x00\x00\x50\x68****\xFF\x15****\x8B\x07\x8B\xCF\x8B\x80\x8C\x00\x00\x00\xFF\xD0\x84\xC0**\x8B\x47\x3C\x8D\x5F\x3C"), //  55 8B EC 53 56 57 8B F9 8B 0D ?? ?? ?? ?? 8B B1 0C 10 00 00 85 F6 ?? ?? 6A 04 6A 00 68 ?? ?? ?? ?? 6A 00 68 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 84 C0 ?? ?? 8B 0D ?? ?? ?? ?? FF 75 08 8B 01 FF 90 EC 01 00 00 50 68 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B 07 8B CF 8B 80 8C 00 00 00 FF D0 84 C0 ?? ?? 8B 47 3C 8D 5F 3C
	};

	const std::vector<Symbol> CVProfile_OutputReportSym = {
		Symbol::FromName("_ZN9CVProfile12OutputReportEiPKci"),
		Symbol::FromName("_ZN9CVProfile12OutputReportEiPKci"),
		Symbol::FromName("?OutputReport@CVProfile@@QAEXHPBDH@Z"),
	};

	const std::vector<Symbol> CScriptedEntity_StartFunctionStrSym = { // const char* version - GetSoundInterests
		Symbol::FromName("_ZN15CScriptedEntity13StartFunctionEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x49\x89\xF6\x41\x55\x41\x54\x49\x89\xFC\x53\xE8****\x84\xC0"), // 55 48 89 E5 41 56 49 89 F6 41 55 41 54 49 89 FC 53 E8 ?? ?? ?? ?? 84 C0
		Symbol::FromSignature("\x55\x8B\xEC\x56\x8B\xF1\xFF\x15****\x84\xC0**\x80\x7E\x04\x00**\x8B\x4E\x0C\x8B\x01\xFF\x90\x84\x03\x00\x00\x8B\xC8\x8B\x10\xFF\x92\x84\x00\x00\x00\x8B\x0D****\xFF\x75\x08"), //  55 8B EC 56 8B F1 FF 15 ?? ?? ?? ?? 84 C0 ?? ?? 80 7E 04 00 ?? ?? 8B 4E 0C 8B 01 FF 90 84 03 00 00 8B C8 8B 10 FF 92 84 00 00 00 8B 0D ?? ?? ?? ?? FF 75 08
	};

	const std::vector<Symbol> CScriptedEntity_StartFunctionSym = { // int version - SENT:AcceptInput
		Symbol::FromName("_ZN15CScriptedEntity13StartFunctionEi"), 
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x89\xF6\x41\x55\x41\x54\x49\x89\xFC\x53\xE8****"), // 55 48 89 E5 41 56 41 89 F6 41 55 41 54 49 89 FC 53 E8 ?? ?? ?? ??
		Symbol::FromSignature("\x55\x8B\xEC\x56\x8B\xF1\xFF\x15****\x84\xC0**\x80\x7E\x04\x00**\x8B\x4E\x0C\x8B\x01\xFF\x90\x84\x03\x00\x00\x8B\xC8\x8B\x10\xFF\x92\x84\x00\x00\x00\x8B\x0D****\x6A\x00"), //  55 8B EC 56 8B F1 FF 15 ?? ?? ?? ?? 84 C0 ?? ?? 80 7E 04 00 ?? ?? 8B 4E 0C 8B 01 FF 90 84 03 00 00 8B C8 8B 10 FF 92 84 00 00 00 8B 0D ?? ?? ?? ?? 6A 00
	};

	const std::vector<Symbol> CScriptedEntity_CallSym = { // SENT:AcceptInput
		Symbol::FromName("_ZN15CScriptedEntity4CallEii"),
		Symbol::FromSignature("\x48\x8B*****\x55\x83\xC6\x01\x85\xD2\x48\x89\xE5"), // 48 8B ?? ?? ?? ?? ?? 55 83 C6 01 85 D2 48 89 E5
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x0D****\x8B\x55\x0C\x56\x8B\x75\x08\x8B\x01\x46\x85\xD2"), //  55 8B EC 8B 0D ?? ?? ?? ?? 8B 55 0C 56 8B 75 08 8B 01 46 85 D2
	};

	const std::vector<Symbol> CScriptedEntity_CallFunctionStrSym = { // const char* version - SetupDataTables
		Symbol::FromName("_ZN15CScriptedEntity12CallFunctionEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x83\xEC\x38\x0F\xB6\x47\x08\x84\xC0"), // 55 48 89 E5 41 55 41 54 53 48 89 FB 48 83 EC 38 0F B6 47 08 84 C0
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x14\x56\x8B\xF1\x80\x7E\x04\x00**\x32\xC0\x5E\x8B\xE5\x5D\xC2\x04\x00"), // 55 8B EC 83 EC 14 56 8B F1 80 7E 04 00 ?? ?? 32 C0 5E 8B E5 5D C2 04 00
	};

	const std::vector<Symbol> CScriptedEntity_CallFunctionSym = { // int version. - Found no good identifyer to find it. Guess it by checking if a function has a similar signature
		Symbol::FromName("_ZN15CScriptedEntity12CallFunctionEi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x53\x48\x89\xFB\x48\x83\xEC\x10\x80\x7F\x08\x00"), //  55 48 89 E5 41 54 53 48 89 FB 48 83 EC 10 80 7F 08 00
		Symbol::FromSignature("\x55\x8B\xEC\x56\x8B\xF1\x80\x7E\x04\x00**\x8B\x4E\x0C\x8B"), // 55 8B EC 56 8B F1 80 7E 04 00 ?? ?? 8B 4E 0C 8B
	};

	const std::vector<Symbol> lj_BC_FUNCCSym = {
		Symbol::FromSignature("\x8B\x6A\xF8\x8B\x7D\x14\x8B\x6C\x24\x30\x8D\x44\xC2\xF8\x89\x55\x10\x8D\x88\xA0\x00\x00\x00\x3B\x4D\x18\x89\x45\x14\x89\x2C\x24"), // 8B 6A F8 8B 7D 14 8B 6C 24 30 8D 44 C2 F8 89 55 10 8D 88 A0 00 00 00 3B 4D 18 89 45 14 89 2C 24
	};

	//---------------------------------------------------------------------------------
	// Purpose: networking Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> AllocChangeFrameListSym = { // I'm still suprised I managed to get this one :^
		Symbol::FromName("_Z20AllocChangeFrameListii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x54\x41\x89\xFC\xBF\x28"), // 55 48 89 E5 41 55 41 54 41 89 FC BF 28
	};

	const std::vector<Symbol> SendTable_CullPropsFromProxiesSym = { // CPropCullStack::AddProp - m_pOutProps overflowed
		Symbol::FromName("_Z30SendTable_CullPropsFromProxiesPK9SendTablePKiiiPK20CSendProxyRecipientsiS6_iPii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xFF\x41\x56\x41\x89\xCE"), // 55 48 89 E5 41 57 49 89 FF 41 56 41 89 CE
	};

	const std::vector<Symbol> SendTable_WritePropListSym = { // SendTable_WritePropList->inputBuffer
		Symbol::FromName("_Z23SendTable_WritePropListPK9SendTablePKviP8bf_writeiPKii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x81\xEC\x88\x00\x00\x00\x48\x89\xBD\x60\xFF\xFF\xFF"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 81 EC 88 00 00 00 48 89 BD 60 FF FF FF
	};

	const std::vector<Symbol> CBaseServer_WriteDeltaEntitiesSym = { // CBaseServer::WriteDeltaEntities
		Symbol::FromName("_ZN11CBaseServer18WriteDeltaEntitiesEP11CBaseClientP12CClientFrameS3_R8bf_write"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xCC\x53\x48\x89\xFB"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 CC 53 48 89 FB
	};

	const std::vector<Symbol> SendTable_CalcDeltaSym = {
		Symbol::FromName("_Z19SendTable_CalcDeltaPK9SendTablePKviS3_iPiii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xCF\x41\x56\x49\x89\xF6"), // 55 48 89 E5 41 57 49 89 CF 41 56 49 89 F6
	};

	const std::vector<Symbol> SV_DetermineUpdateTypeSym = { // CBaseServer::WriteDeltaEntities
		Symbol::FromName("_ZL22SV_DetermineUpdateTypeR16CEntityWriteInfo"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x81\xEC\xA8\x80\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 89 FB 48 81 EC A8 80 00 00
	};

	const std::vector<Symbol> PackedEntity_GetPropsChangedAfterTickSym = { //
		Symbol::FromName("_ZN12PackedEntity24GetPropsChangedAfterTickEiPii"),
		Symbol::FromSignature("\x55\x48\x8B\x7F\x50\x48\x89\xE5\x48\x85\xFF"), // 55 48 8B 7F 50 48 89 E5 48 85 FF
	};

	const std::vector<Symbol> CGameServer_SendClientMessagesSym = { //
		Symbol::FromName("_ZN11CGameServer18SendClientMessagesEb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x89\xF7\x41\x56\x41\x55\x49\x89\xFD\x41\x54\x53\x48\x81\xEC\x48\x08\x00\x00"), // 55 48 89 E5 41 57 41 89 F7 41 56 41 55 49 89 FD 41 54 53 48 81 EC 48 08 00 00
	};

	const std::vector<Symbol> CBaseEntity_GMOD_SetShouldPreventTransmitToPlayerSym = { //Find CBaseEntity::GetLuaEntity with "m_LuaEntity != ENTITY!"
		Symbol::FromName("_ZN11CBaseEntity37GMOD_SetShouldPreventTransmitToPlayerEP11CBasePlayerb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x89\xD7\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53"), // 55 48 89 E5 41 57 41 89 D7 41 56 41 55 41 54 49 89 FC 53
	};

	const std::vector<Symbol> CBaseEntity_GMOD_ShouldPreventTransmitToPlayerSym = { //Find CBaseEntity::GetLuaEntity with "m_LuaEntity != ENTITY!"
		Symbol::FromName("_ZN11CBaseEntity34GMOD_ShouldPreventTransmitToPlayerEP11CBasePlayer"),
		Symbol::FromSignature("\x55\x31\xC0\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x18\x66\x83\xBF\xE6\x14\x00\x00\x00"), // 55 31 C0 48 89 E5 53 48 89 FB 48 83 EC 18 66 83 BF E6 14 00 00 00
	};

	const std::vector<Symbol> g_FrameSnapshotManagerSym = {
		Symbol::FromName("framesnapshotmanager"),
		Symbol::FromSignature("\x48\x8B\x2A\x2A\x2A\x2A\x2A\x48\x8B\x38\x48\x8B\x07*\x50\x10\x48\x8D\x43\x15"), // 48 8B ?? ?? ?? ?? ?? 48 8B 38 48 8B 07 ?? 50 10 48 8D 43 15 || "framesnapshotmanager->LevelChanged()" || "sv.Clear()"
		Symbol::FromSignature("\x2A\x2A\x2A\x2A\x83\xC4\x04\x8B\x01\xFF\x50\x04\x6A\x40"), // ?? ?? ?? ?? 83 C4 04 8B 01 FF 50 04 6A 40 || "framesnapshotmanager"
	};

	const std::vector<Symbol> g_PropTypeFnsSym = {
		Symbol::FromName("g_PropTypeFns"),
		Symbol::FromSignature(""), // ToDo
	};

	const std::vector<Symbol> g_BSPDataSym = { // 64x won't happen until I figure out how to get g_BSPData. Rubat pls expose it
		Symbol::FromName("g_BSPData"),
	};

	// Only used on Linux
	const std::vector<Symbol> PackWork_t_ProcessSym = {
		Symbol::FromName("_ZN10PackWork_t7ProcessERS_"),
		Symbol::FromSignature("\x55\x48\x89\xFA\x48\x8B\x4F\x10"), // 55 48 89 FA 48 8B 4F 10
	};

	// Only used on Windows
	const std::vector<Symbol> SV_PackEntitySym = {
		NULL_SIGNATURE,
		NULL_SIGNATURE,
		Symbol::FromSignature("\x55\x8B\xEC\xB8\x58\xC4\x00\x00"), // 55 8B EC B8 58 C4 00 00
	};

	const std::vector<Symbol> InvalidateSharedEdictChangeInfosSym = {//Search for "NUM_FOR_EDICTINFO: bad pointer" then you get CBaseEdict::GetChangeAccessor and search for the xref
		Symbol::FromName("_Z32InvalidateSharedEdictChangeInfosv"),
		Symbol::FromSignature("\x0F\xB7\x05\x2A\x2A\x2A\x2A\x66\x83\xF8\xFF\x74\x2A"), // 0F B7 05 ? ? ? ? 66 83 F8 FF 74 ?
		Symbol::FromSignature("\x66*****\xB9\xFF\xFF\x00\x00\x66\x3B\xC1**\x57\x33\xFF"), // 66 ?? ?? ?? ?? ?? B9 FF FF 00 00 66 3B C1 ?? ?? 57 33 FF
	};

	const std::vector<Symbol> PackEntities_NormalSym = { //Search for 'SV_PackEntity: SendTable_Encode returned false (ent %d).
		Symbol::FromName("_Z19PackEntities_NormaliPP11CGameClientP14CFrameSnapshot"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x81\xEC\xC8\xC4\x03\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 81 EC C8 C4 03 00
		Symbol::FromSignature("\x55\x8B\xEC\xB8\x2C\x80\x01\x00"), // 55 8B EC B8 2C 80 01 00
	};

	const std::vector<Symbol> CGMOD_Player_CreateViewModelSym = {
		Symbol::FromName("_ZN12CGMOD_Player15CreateViewModelEi"),
	};

	const std::vector<Symbol> CBaseCombatCharacter_SetTransmitSym = {//Search for 1st "CBaseAnimating::SetTransmit" xref
		Symbol::FromName("_ZN20CBaseCombatCharacter11SetTransmitEP18CCheckTransmitInfob"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x49\x89\xF5\x41\x54\x49\x89\xFC\x53\x48\x83\xEC\x08\x48\x8B\x47\x40"), // 55 48 89 E5 41 57 41 56 41 55 49 89 F5 41 54 49 89 FC 53 48 83 EC 08 48 8B 47 40
	};

	const std::vector<Symbol> CBaseAnimating_SetTransmitSym = {//Find "Setting CBaseAnimating to non-studio model %s  (type:%i)" to get CBaseAnimating__SetModel then find the last xref and take 4 function upper
		Symbol::FromName("_ZN14CBaseAnimating11SetTransmitEP18CCheckTransmitInfob"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x89\xFB\x48\x81\xEC\x20\x01\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 FC 53 48 89 FB 48 81 EC 20 01 00 00
	};

	const std::vector<Symbol> GetCurrentSkyCameraSym = {
		Symbol::FromName("_Z19GetCurrentSkyCamerav"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: steamworks Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CSteam3Server_OnLoggedOffSym = {
		Symbol::FromName("_ZN13CSteam3Server11OnLoggedOffEP26SteamServersDisconnected_t"),
		Symbol::FromSignature("\x83\xBF\x30****\x0F\x84\x7C***\x55\x48\x89\xE5"), // 83 BF 30 ?? ?? ?? ?? 0F 84 7C ?? ?? ?? 55 48 89 E5
	};

	const std::vector<Symbol> CSteam3Server_OnLogonSuccessSym = {
		Symbol::FromName("_ZN13CSteam3Server14OnLogonSuccessEP23SteamServersConnected_t"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x28\x64\x48\x8B"), // 55 48 89 E5 53 48 89 FB 48 83 EC 28 64 48 8B
	};

	const std::vector<Symbol> CSteam3Server_ShutdownSym = {
		Symbol::FromName("_ZN13CSteam3Server8ShutdownEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x08\x48\x83\x7F**\x74\x5A"), // 55 48 89 E5 53 48 89 FB 48 83 EC 08 48 83 7F ?? ?? 74 5A
	};

	const std::vector<Symbol> CSteam3Server_ActivateSym = {
		Symbol::FromName("_ZN13CSteam3Server8ActivateENS_11EServerTypeE"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x41\x89\xF4\x53\x48\x89\xFB\x48\x81\xEC") // 55 48 89 E5 41 57 41 56 41 55 41 54 41 89 F4 53 48 89 FB 48 81 EC
	};

	const std::vector<Symbol> Steam3ServerSym = { // 64x = Found with "Warning: sv_steamgroup is not applicabl" and then find the unk_[xyz], search for it's usage and find a small function.
		Symbol::FromName("_Z12Steam3Serverv"),
		Symbol::FromSignature("\x55\x48\x8D\x05****\x48\x89\xE5\x5D\xC3\x90\x66\x90\x55\x31\xC0\xB9\x08"), // 55 48 8D 05 ?? ?? ?? ?? 48 89 E5 5D C3 90 66 90 55 31 C0 B9 08
	};

	const std::vector<Symbol> SV_InitGameServerSteamSym = {
		Symbol::FromName("_Z22SV_InitGameServerSteamv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x83\xEC\x08\x48\x8B\x1D********\x00\x00\x01"), // 55 48 89 E5 53 48 83 EC 08 48 8B 1D ?? ?? ?? ?? ?? ?? ?? ?? 00 00 01
	};

	const std::vector<Symbol> CSteam3Server_NotifyClientConnectSym = { // 64x = "S3: Client"
		Symbol::FromName("_ZN13CSteam3Server19NotifyClientConnectEP11CBaseClientjR8netadr_sPKvj"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x81\xEC\xB8\x00\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 FC 53 48 81 EC B8 00 00 00
	};

	const std::vector<Symbol> CSteam3Server_SendUpdatedServerDetailsSym = { // 64x = "steamblocking:%d"
		Symbol::FromName("_ZN13CSteam3Server24SendUpdatedServerDetailsEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x83\xEC\x68\x64\x48\x8B\x04\x25\x28"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 FC 53 48 83 EC 68 64 48 8B 04 25 28
	};

	const std::vector<Symbol> CSteam3Server_CheckForDuplicateSteamIDSym = { // 64x = "STEAM UserID"
		Symbol::FromName("_ZN13CSteam3Server24CheckForDuplicateSteamIDEPK11CBaseClient"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x83\xEC\x68\x48\x89\x75\x80"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 68 48 89 75 80
	};

	//---------------------------------------------------------------------------------
	// Purpose: voicechat Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> SV_BroadcastVoiceDataSym = { // Sending voice from: %s
		Symbol::FromName("_Z21SV_BroadcastVoiceDataP7IClientiPcx"),
		Symbol::FromSignature("\x55\x48******\x48\x89\xE5\x41\x57\x41\x56\x41\x89\xF6\x41\x55\x49\x89\xFD\x41\x54"), // 55 48 ?? ?? ?? ?? ?? ?? 48 89 E5 41 57 41 56 41 89 F6 41 55 49 89 FD 41 54
		Symbol::FromSignature("\x55\x8B\xEC\xA1****\x83\xEC\x50\x8B\x50\x48"), // 55 8B EC A1 ?? ?? ?? ?? 83 EC 50 8B 50 48
	};

	const std::vector<Symbol> CVoiceGameMgr_UpdateSym = { // VoiceMask
		Symbol::FromName("_ZN13CVoiceGameMgr6UpdateEd"),
		NULL_SIGNATURE,
		Symbol::FromSignature("\x55\x8B\xEC\xDD\x45\x08\x56\x8B\xF1\xDC\x46\x10\xDD\x56\x10"), // 55 8B EC DD 45 08 56 8B F1 DC 46 10 DD 56 10
	};

	const std::vector<Symbol> g_PlayerModEnableSym = {
		Symbol::FromName("g_PlayerModEnable"),
		NULL_SIGNATURE,
		Symbol::FromSignature("\x2A\x2A\x2A\x2A\x89\x5D\xFC\x2A\x2A\x2A\x2A\x2A\x83\xC4\x04"), // CVoiceGameMgr::ClientCommand: VModEnable - ?? ?? ?? ?? 89 5D FC ?? ?? ?? ?? ?? 83 C4 04
	};

	const std::vector<Symbol> g_BanMasksSym = {
		Symbol::FromName("g_BanMasks"),
		NULL_SIGNATURE,
		Symbol::FromSignature("\x2A\x2A\x2A\x2A\x8B\x4D\x08\x46\x8B\x07"), // CVoiceGameMgr::ClientCommand: vban - ?? ?? ?? ?? 8B 4D 08 46 8B 07
	};

	const std::vector<Symbol> g_SentGameRulesMasksSym = {
		Symbol::FromName("g_SentGameRulesMasks"),
		NULL_SIGNATURE,
		Symbol::FromSignature("\x2A\x2A\x2A\x2A\x89\x81\x2A\x2A\x2A\x2A\x8B\x81\x2A\x2A\x2A\x2A\x89\x81\x2A\x2A\x2A\x2A\x8B\x81\x2A\x2A\x2A\x2A\x89\x81\x2A\x2A\x2A\x2A\x8D\x45\xA0"), // Guess - ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 81 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 81 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8D 45 A0
	};

	const std::vector<Symbol> g_SentBanMasksSym = {
		Symbol::FromName("g_SentBanMasks"),
		NULL_SIGNATURE,
		Symbol::FromSignature("\x2A\x2A\x2A\x2A\x89\x81\x2A\x2A\x2A\x2A\x8B\x81\x2A\x2A\x2A\x2A\x89\x81\x2A\x2A\x2A\x2A\x8D\x45\xA0"), // Guess - ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 81 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8D 45 A0
	};

	const std::vector<Symbol> g_bWantModEnableSym = {
		Symbol::FromName("g_bWantModEnable"),
		NULL_SIGNATURE,
		Symbol::FromSignature("\x2A\x2A\x2A\x2A\x89\x81\x2A\x2A\x2A\x2A\x8D\x45\xA0"), // Guess - ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8D 45 A0
	};

	//---------------------------------------------------------------------------------
	// Purpose: physenv Symbols
	// ToDo: Get the Linux64 and Windows32 symbols.
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> IVP_Mindist_do_impactSym = { // NOTE: is virtual
		Symbol::FromName("_ZN11IVP_Mindist9do_impactEv"),
		Symbol::FromSignature(""),
	};

	const std::vector<Symbol> IVP_Event_Manager_Standard_simulate_time_eventsSym = { // NOTE: is virtual
		Symbol::FromName("_ZN26IVP_Event_Manager_Standard20simulate_time_eventsEP16IVP_Time_ManagerP15IVP_Environment8IVP_Time"),
	};

	const std::vector<Symbol> IVP_Mindist_simulate_time_eventSym = {
		Symbol::FromName("_ZN11IVP_Mindist19simulate_time_eventEP15IVP_Environment"),
	};

	const std::vector<Symbol> IVP_Mindist_update_exact_mindist_eventsSym = {
		Symbol::FromName("_ZN11IVP_Mindist27update_exact_mindist_eventsE8IVP_BOOL22IVP_MINDIST_EVENT_HINT"),
	};

	const std::vector<Symbol> IVP_Mindist_D2Sym = {
		Symbol::FromName("_ZN11IVP_MindistD2Ev"),
	};

	const std::vector<Symbol> g_pCurrentMindistSym = {
		Symbol::FromName("g_pCurrentMindist"),
	};

	const std::vector<Symbol> g_fDeferDeleteMindistSym = {
		Symbol::FromName("g_fDeferDeleteMindist"),
	};

	const std::vector<Symbol> GMod_Util_IsPhysicsObjectValidSym = { // PhysObject [%s][Entity %d]
		Symbol::FromName("_ZN4GMod4Util20IsPhysicsObjectValidEP14IPhysicsObject"),
		Symbol::FromSignature(""),
		Symbol::FromSignature("\x55\x8B\xEC\x51\x8B*****\x8D\x55\xFC\xC7\x45\xFC\x00\x00\x00\x00"), // 55 8B EC 51 8B ?? ?? ?? ?? ?? 8D 55 FC C7 45 FC 00 00 00 00
	};

	const std::vector<Symbol> PhysFrameSym = { // "Reset physics clock\n"
		Symbol::FromName("_ZL9PhysFramef"),
		Symbol::FromSignature(""),
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x1C\x83******\x53\x56\x57"), // 55 8B EC 83 EC 1C 83 ?? ?? ?? ?? ?? ?? 53 56 57
	};

	const std::vector<Symbol> CPhysicsEnvironment_DestroyObjectSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment13DestroyObjectEP14IPhysicsObject"),
		Symbol::FromSignature(""),
		// On Windows we will use the vtable instead to detour it.... maybe we should do the same on linux?
	};

	const std::vector<Symbol> CPhysicsEnvironment_RestoreSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment7RestoreERK19physrestoreparams_t"),
	};

	const std::vector<Symbol> CPhysicsEnvironment_TransferObjectSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment14TransferObjectEP14IPhysicsObjectP19IPhysicsEnvironment"),
	};

	const std::vector<Symbol> CPhysicsEnvironment_CreateSphereObjectSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment18CreateSphereObjectEfiRK6VectorRK6QAngleP14objectparams_tb"),
	};

	const std::vector<Symbol> CPhysicsEnvironment_UnserializeObjectFromBufferSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment27UnserializeObjectFromBufferEPvPhjb"),
	};

	const std::vector<Symbol> CPhysicsEnvironment_CreatePolyObjectStaticSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment22CreatePolyObjectStaticEPK12CPhysCollideiRK6VectorRK6QAngleP14objectparams_t"),
	};

	const std::vector<Symbol> CPhysicsEnvironment_CreatePolyObjectSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment16CreatePolyObjectEPK12CPhysCollideiRK6VectorRK6QAngleP14objectparams_t"),
	};

	const std::vector<Symbol> CPhysicsEnvironment_D2Sym = {
		Symbol::FromName("_ZN19CPhysicsEnvironmentD2Ev"),
	};

	const std::vector<Symbol> CPhysicsEnvironment_C2Sym = {
		Symbol::FromName("_ZN19CPhysicsEnvironmentC2Ev"),
	};

	const std::vector<Symbol> IVP_Mindist_Manager_recheck_ov_elementSym = {
		Symbol::FromName("_ZN19IVP_Mindist_Manager18recheck_ov_elementEP15IVP_Real_Object"),
	};

	const std::vector<Symbol> IVP_Mindist_Minimize_Solver_p_minimize_PPSym = {
		Symbol::FromName("_ZN27IVP_Mindist_Minimize_Solver13p_minimize_PPEPK16IVP_Compact_EdgeS2_P21IVP_Cache_Ledge_PointS4_"),
	};

	const std::vector<Symbol> IVP_Mindist_Base_get_objectsSym = {
		Symbol::FromName("_ZN16IVP_Mindist_Base11get_objectsEPP15IVP_Real_Object"),
	};

	const std::vector<Symbol> IVP_OV_Element_add_oo_collisionSym = {
		Symbol::FromName("_ZN14IVP_OV_Element16add_oo_collisionEP13IVP_Collision"),
	};

	const std::vector<Symbol> IVP_OV_Element_remove_oo_collisionSym = {
		Symbol::FromName("_ZN14IVP_OV_Element19remove_oo_collisionEP13IVP_Collision"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: net Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> g_NetIncomingSym = {
		Symbol::FromName("g_NetIncoming"),
	};

	const std::vector<Symbol> g_WriteSym = {
		Symbol::FromName("_ZL7g_Write"),
	};

	const std::vector<Symbol> g_StartedSym = {
		Symbol::FromName("_ZL9g_Started"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: luajit Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CLuaInterface_GetTypeSym = { // 64x = CBaseLuaInterface::GetType (If https://github.com/Facepunch/garrysmod-requests/issues/2578 is done update this)
		Symbol::FromName("_ZN13CLuaInterface7GetTypeEi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x89\xF6\x41\x55\x41\x54\x49\x89\xFC"), // 55 48 89 E5 41 57 41 56 41 89 F6 41 55 41 54 49 89 FC
	};

	//---------------------------------------------------------------------------------
	// Purpose: cvar Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CCvar_RegisterConCommandSym = {
		Symbol::FromName("_ZN5CCvar18RegisterConCommandEP14ConCommandBase"),
	};

	const std::vector<Symbol> CCvar_UnregisterConCommandSym = {
		Symbol::FromName("_ZN5CCvar20UnregisterConCommandEP14ConCommandBase"),
	};

	const std::vector<Symbol> CCvar_UnregisterConCommandsSym = {
		Symbol::FromName("_ZN5CCvar21UnregisterConCommandsEi"),
	};

	const std::vector<Symbol> CCvar_FindCommandBaseConstSym = {
		Symbol::FromName("_ZNK5CCvar15FindCommandBaseEPKc"),
	};

	const std::vector<Symbol> CCvar_FindCommandBaseSym = {
		Symbol::FromName("_ZN5CCvar15FindCommandBaseEPKc"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: gameserver Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CServerGameClients_GetPlayerLimitSym = {
		Symbol::FromName("_ZNK18CServerGameClients15GetPlayerLimitsERiS0_S0_"),
		Symbol::FromSignature("\x55\xC7\x01\x80\x00\x00\x00"), // 55 C7 01 80 00 00 00
	};

	const std::vector<Symbol> CBaseServer_FillServerInfoSym = {
		Symbol::FromName("_ZN11CBaseServer14FillServerInfoER14SVC_ServerInfo"),
		Symbol::FromSignature("\x55\xBA\x04\x01\x00\x00\x48\x89\xE5\x41\x54\x49\x89\xF4\x53\x48\x8D\x35\x2A\x2A\x2A\x2A"), // 55 BA 04 01 00 00 48 89 E5 41 54 49 89 F4 53 48 8D 35 ? ? ? ?
	};

	const std::vector<Symbol> CHLTVServer_FillServerInfoSym = {
		Symbol::FromName("_ZN11CHLTVServer14FillServerInfoER14SVC_ServerInfo"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x49\x89\xFC\x53\x48\x8D\x7F\x08"), // 55 48 89 E5 41 54 49 89 FC 53 48 8D 7F 08
	};

	const std::vector<Symbol> CBaseClient_SetSignonStateSym = {
		Symbol::FromName("_ZN11CBaseClient14SetSignonStateEii"),
		Symbol::FromSignature("\x55\x8B\x87\xA8\x01\x00\x00"), // 55 8B 87 A8 01 00 00
	};

	const std::vector<Symbol> CBaseServer_IsMultiplayerSym = {
		Symbol::FromName("_ZNK11CBaseServer13IsMultiplayerEv"),
		Symbol::FromSignature("\x55\x83\xBF\x34\x01\x00\x00\x01"), // 55 83 BF 34 01 00 00 01
	};

	const std::vector<Symbol> GModDataPack_IsSingleplayerSym = {
		Symbol::FromName("_ZN12GModDataPack14IsSingleplayerEv"),
		Symbol::FromSignature("\x48\x8B\x05\x2A\x2A\x2A\x2A\x55\x48\x89\xE5\x5D\x48\x8B\x00\x83\x78\x14\x01"), // 48 8B 05 ? ? ? ? 55 48 89 E5 5D 48 8B 00 83 78 14 01
	};

	const std::vector<Symbol> CBaseClient_ShouldSendMessagesSym = {//Search for '%s overflowed reliable buffer (%i bytes, %s in, %s out)'
		Symbol::FromName("_ZN11CBaseClient18ShouldSendMessagesEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x83\xEC\x18\x48\x8B\x07"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 89 FB 48 83 EC 18 48 8B 07
	};

	const std::vector<Symbol> CBaseServer_CheckTimeoutsSym = {//Search for 'CBaseServer::CheckTimeouts'
		Symbol::FromName("_ZN11CBaseServer13CheckTimeoutsEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x83\xEC\x18\x48\x8B\x05\x2A\x2A\x2A\x2A"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 FC 53 48 83 EC 18 48 8B 05 ? ? ? ?
	};

	const std::vector<Symbol> CBaseClient_OnRequestFullUpdateSym = {//Search for 'CBaseClient::OnRequestFullUpdate'
		Symbol::FromName("_ZN11CBaseClient19OnRequestFullUpdateEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x83\xEC\x18\x4C\x8B\x25\x2A\x2A\x2A\x2A"), //55 48 89 E5 41 57 41 56 41 55 41 54 53 48 89 FB 48 83 EC 18 4C 8B 25 ? ? ? ?
	};

	const std::vector<Symbol> CGameClient_SpawnPlayerSym = {
		Symbol::FromName("_ZN11CGameClient11SpawnPlayerEv"),
		Symbol::FromSignature("\x55\x31\xC0\x48\x89\xE5\x53\x48\x89\xFB\x48\x8D\x3D\x2A\x2A\x2A\x2A\x48\x83\xEC\x78"), // 55 31 C0 48 89 E5 53 48 89 FB 48 8D 3D ? ? ? ? 48 83 EC 78
	};

	const std::vector<Symbol> CBaseServer_ProcessConnectionlessPacketSym = {
		Symbol::FromName("_ZN11CBaseServer27ProcessConnectionlessPacketEP11netpacket_s"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x49\x89\xFE\x41\x55\x41\x54\x53\x48\x89\xF3\x48\x81\xEC\xC8\x0A\x00\x00"), // 55 48 89 E5 41 57 41 56 49 89 FE 41 55 41 54 53 48 89 F3 48 81 EC C8 0A 00 00
	};

	const std::vector<Symbol> NET_SendPacketSym = {
		Symbol::FromName("_Z14NET_SendPacketP11INetChanneliRK8netadr_sPKhiP8bf_writeb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x49\x89\xD5\x41\x54\x4D\x89\xCC"), // 55 48 89 E5 41 57 41 56 41 55 49 89 D5 41 54 4D 89 CC
	};

	const std::vector<Symbol> CNetChan_SendDatagramSym = {
		Symbol::FromName("_ZN8CNetChan12SendDatagramEP8bf_write"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xF7\x41\x56\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x81\xEC\xE8\x00\x04\x00"), // 55 48 89 E5 41 57 49 89 F7 41 56 41 55 41 54 53 48 89 FB 48 81 EC E8 00 04 00
	};

	const std::vector<Symbol> CNetChan_D2Sym = {
		Symbol::FromName("_ZN8CNetChanD2Ev"),
		Symbol::FromSignature("\x55\x48\x8D\x05\x2A\x2A\x2A\x2A\x48\x89\xE5\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x83\xEC\x08"), // 55 48 8D 05 ? ? ? ? 48 89 E5 41 55 41 54 49 89 FC 53 48 83 EC 08
	};

	const std::vector<Symbol> NET_CreateNetChannelSym = {
		Symbol::FromName("_Z20NET_CreateNetChanneliP8netadr_sPKcP18INetChannelHandlerbi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x45\x89\xCF\x41\x56\x49\x89\xCE"), // 55 48 89 E5 41 57 45 89 CF 41 56 49 89 CE
	};

	const std::vector<Symbol> NET_RemoveNetChannelSym = {
		Symbol::FromName("_Z20NET_RemoveNetChannelP11INetChannelb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x83\xEC\x08\x48\x85\xFF\x0F\x84\x2A\x2A\x2A\x2A\x4C\x8D\x25\x2A\x2A\x2A\x2A"), // 55 48 89 E5 41 55 41 54 53 48 89 FB 48 83 EC 08 48 85 FF 0F 84 ? ? ? ? 4C 8D 25 ? ? ? ?
	};

	const std::vector<Symbol> Filter_SendBanSym = {
		Symbol::FromName("_Z14Filter_SendBanRK8netadr_s"),
		Symbol::FromSignature("\x55\x48\x89\xFE\xB9\x6C\x00\x00\x00"), // 55 48 89 FE B9 6C 00 00 00
	};

	const std::vector<Symbol> NET_SendStreamSym = {
		Symbol::FromName("_Z14NET_SendStreamiPKcii"),
		Symbol::FromSignature("\x55\x48\x63\xD2\x48\x89\xE5\x53\x48\x83\xEC\x08\xE8\x2A\x2A\x2A\x2A\x48\x89\xC3"), // 55 48 63 D2 48 89 E5 53 48 83 EC 08 E8 ? ? ? ? 48 89 C3
	};

	const std::vector<Symbol> NET_ReceiveStreamSym = {
		Symbol::FromName("_Z17NET_ReceiveStreamiPcii"),
		Symbol::FromSignature("\x55\x48\x63\xD2\x48\x89\xE5\x53\x48\x83\xEC\x08\xE8\x2A\x2A\x2A\x2A\x83\xF8\xFF"), // 55 48 63 D2 48 89 E5 53 48 83 EC 08 E8 ? ? ? ? 83 F8 FF
	};

	const std::vector<Symbol> s_NetChannelsSym = {
		Symbol::FromName("_ZL13s_NetChannels"),
	};
  
	const std::vector<Symbol> NET_SetTimeSym = {
		Symbol::FromName("_Z11NET_SetTimed"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: AutoRefresh Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> GarrysMod_AutoRefresh_HandleChange_LuaSym = {
		Symbol::FromName("_ZN9GarrysMod11AutoRefresh16HandleChange_LuaERKSsS2_S2_"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x49\x89\xD6\x41\x55\x49\x89\xFD\x48\x89\xD7\x41\x54"),
	};

	const std::vector<Symbol> GarrysMod_AutoRefresh_InitSym = {
		Symbol::FromName("_ZN9GarrysMod11AutoRefresh4InitEv"),
	};

	const std::vector<Symbol> GarrysMod_AutoRefresh_CycleSym = {
		Symbol::FromName("_ZN9GarrysMod11AutoRefresh5CycleEv"),
	};

	const std::vector<Symbol> Bootil_File_ChangeMonitor_CheckForChangesSym = {
		Symbol::FromName("_ZN6Bootil4File13ChangeMonitor10HasChangesEv"),
	};

	const std::vector<Symbol> Bootil_File_ChangeMonitor_HasChangedSym = {
		Symbol::FromName("_ZN6Bootil4File13ChangeMonitor10HasChangesEv"),
	};
}