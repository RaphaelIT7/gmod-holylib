#include "detours.h"

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

	const std::vector<Symbol> Get_PlayerSym = {
		Symbol::FromName("_Z10Get_Playerib"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x41\x89\xF4\x53\x40\x0F\xB6\xF6\xE8\xED\xF3"), // 55 48 89 E5 41 54 41 89 F4 53 40 0F B6 F6 E8 ED F3
	};

	const std::vector<Symbol> Push_EntitySym = {
		Symbol::FromName("_Z11Push_EntityP11CBaseEntity"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x53\x48\x83\xEC\x20\x48\x8B\x1D"), // 55 48 89 E5 41 54 53 48 83 EC 20 48 8B 1D
	};

	const std::vector<Symbol> Get_EntitySym = {
		Symbol::FromName("_Z10Get_Entityib"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x89\xF5\x41\x54\x41\x89\xFC\x53"), // 55 48 89 E5 41 55 41 89 F5 41 54 41 89 FC 53
	};

	const std::vector<Symbol> g_pEntityListSym = { // 64x = ents.GetAll
		Symbol::FromName("g_pEntityList"),
	};

	const std::vector<Symbol> UsermessagesSym = {
		Symbol::FromName("_ZL14g_UserMessages"),
		Symbol::FromSignature("\x55\x48\x8D*****\xBA\x01\x00\x00\x00\x48\x89\xE5\x53\x48\x83\xEC\x08\x2A\x2A\x2A\x2A"), // 55 48 8D ?? ?? ?? ?? ?? BA 01 00 00 00 48 89 E5 53 48 83 EC 08 2A 2A 2A 2A
	};

	const std::vector<Symbol> CBaseEntity_CalcAbsolutePositionSym = { // Can't find a 64x symbol yet....
		Symbol::FromName("_ZN11CBaseEntity20CalcAbsolutePositionEv"),
	};

	const std::vector<Symbol> CBaseAnimating_InvalidateBoneCacheSym = {
		Symbol::FromName("_ZN14CBaseAnimating19InvalidateBoneCacheEv"),
		Symbol::FromSignature(""), // 
		// How to hopefully find it(Still a pain): Search for "%5.2f : %s : %s : %5.3f\n" -> CBaseAnimating::StudioFrameAdvance() -> StudioFrameAdvanceInternal() -> Studio_InvalidateBoneCacheIfNotMatching() -> Find CBaseAnimating::InvalidateBoneCache by checking which function calls it with -1.0f
	};

	const std::vector<Symbol> CBaseEntity_PostConstructorSym = { // ToDo: 64x
		Symbol::FromName("_ZN11CBaseEntity15PostConstructorEPKc"),
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

	const std::vector<Symbol> CGameEventManager_CreateEventSym = {
		Symbol::FromName("_ZN17CGameEventManager11CreateEventEPKcb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x54\x53\x48\x89\xF3\x48\x83\xEC\x08\x48\x85\xF6**\x80\x3E\x00"), // 55 48 89 E5 41 55 41 54 53 48 89 F3 48 83 EC 08 48 85 F6 ?? ?? 80 3E 00
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
	const std::vector<Symbol> CNetworkStringTable_DeleteAllStringsSym = {
		Symbol::FromName("_ZN19CNetworkStringTable16DeleteAllStringsEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x08\x48\x8B\x7F\x50"), // 55 48 89 E5 53 48 89 FB 48 83 EC 08 48 8B 7F 50
	};

	const std::vector<Symbol> CNetworkStringTable_DeconstructorSym = {
		Symbol::FromName("_ZN19CNetworkStringTableD0Ev"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x08\xE8\x8F\xFF\xFF\xFF\x48\x83\xC4\x08\x48\x89\xDF\x5B\x5D\xE9**\xEE\xFF"), // 55 48 89 E5 53 48 89 FB 48 83 EC 08 E8 8F FF FF FF 48 83 C4 08 48 89 DF 5B 5D E9 ?? ?? EE FF
	};

	//---------------------------------------------------------------------------------
	// Purpose: surffix Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CGameMovement_TryPlayerMoveSym = {
		Symbol::FromName("_ZN13CGameMovement13TryPlayerMoveEP6VectorP10CGameTrace"),
	};

	const std::vector<Symbol> CGameMovement_ClipVelocitySym = {
		Symbol::FromName("_ZN13CGameMovement12ClipVelocityER6VectorS1_S1_f"),
	};

	const std::vector<Symbol> CBaseEntity_GetGroundEntitySym = {
		Symbol::FromName("_ZN11CBaseEntity15GetGroundEntityEv"),
	};

	const std::vector<Symbol> CTraceFilterSimple_ShouldHitEntitySym = {
		Symbol::FromName("_ZN18CTraceFilterSimple15ShouldHitEntityEP13IHandleEntityi"),
	};

	const std::vector<Symbol> MoveHelperServerSym = {
		Symbol::FromName("_Z16MoveHelperServerv"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: pvs Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CGMOD_Player_SetupVisibilitySym = {
		Symbol::FromName("_ZN12CGMOD_Player15SetupVisibilityEP11CBaseEntityPhi"),
	};

	const std::vector<Symbol> CServerGameEnts_CheckTransmitSym = {
		Symbol::FromName("_ZN15CServerGameEnts13CheckTransmitEP18CCheckTransmitInfoPKti"),
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
		Symbol::FromName("_ZN15CBaseFileSystem23FindSearchPathByStoreIdEi"),
	};

	const std::vector<Symbol> CBaseFileSystem_FastFileTimeSym = {
		Symbol::FromName("_ZN15CBaseFileSystem12FastFileTimeEPKNS_11CSearchPathEPKc"),
	};

	const std::vector<Symbol> CBaseFileSystem_FixUpPathSym = {
		Symbol::FromName("_ZN15CBaseFileSystem9FixUpPathEPKcPci"),
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
	};

	const std::vector<Symbol> CBaseFileSystem_AddVPKFileSym = {
		Symbol::FromName("_ZN15CBaseFileSystem10AddVPKFileEPKcS1_j"),
	};

	const std::vector<Symbol> CBaseFileSystem_RemoveAllMapSearchPathsSym = {
		Symbol::FromName("_ZN15CBaseFileSystem23RemoveAllMapSearchPathsEv"),
	};

	const std::vector<Symbol> CBaseFileSystem_CloseSym = {
		Symbol::FromName("_ZN15CBaseFileSystem5CloseEPv"),
	};

	const std::vector<Symbol> CBaseFileSystem_CSearchPath_GetDebugStringSym = {
		Symbol::FromName("_ZNK15CBaseFileSystem11CSearchPath14GetDebugStringEv"),
	};


	//---------------------------------------------------------------------------------
	// Purpose: concommand Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> ConCommand_IsBlockedSym = {
		Symbol::FromName("_Z20ConCommand_IsBlockedPKc"),
		Symbol::FromSignature("\x48\x8B\x05****\x55\x48\x89\xE5\x41\x54\x49\x89\xFC\x53\x48\x8B*\x80\x78\x70\x00"), // 48 8B 05 ?? ?? ?? ?? 55 48 89 E5 41 54 49 89 FC 53 48 8B ?? 80 78 70 00
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

	const std::vector<Symbol> ThreadGetCurrentIdSym = {
		NULL_SIGNATURE,
		Symbol::FromName("ThreadGetCurrentId"), // Trying to fix 64x Vprof
	};

#ifdef SYSTEM_WINDOWS
	const std::vector<Symbol> Client_CLuaGamemode_CallFinishSym = {
		NULL_SIGNATURE,
		NULL_SIGNATURE,
		Symbol::FromSignature(""), 
	};

	const std::vector<Symbol> Client_CLuaGamemode_CallWithArgsSym = {
		NULL_SIGNATURE,
		NULL_SIGNATURE,
		Symbol::FromSignature(""), 
	};

	const std::vector<Symbol> Client_CLuaGamemode_CallSym = {
		NULL_SIGNATURE,
		NULL_SIGNATURE,
		Symbol::FromSignature(""), 
	};

	const std::vector<Symbol> Client_CScriptedEntity_StartFunctionStrSym = { // const char* version - GetSoundInterests
		NULL_SIGNATURE,
		NULL_SIGNATURE,
		Symbol::FromSignature(""), 
	};

	const std::vector<Symbol> Client_CScriptedEntity_StartFunctionSym = { // int version - SENT:AcceptInput
		NULL_SIGNATURE,
		NULL_SIGNATURE,
		Symbol::FromSignature(""), 
	};

	const std::vector<Symbol> Client_CScriptedEntity_CallSym = { // SENT:AcceptInput
		NULL_SIGNATURE,
		NULL_SIGNATURE,
		Symbol::FromSignature(""), 
	};

	const std::vector<Symbol> Client_CScriptedEntity_CallFunctionStrSym = { // const char* version - SetupDataTables
		NULL_SIGNATURE,
		NULL_SIGNATURE,
		Symbol::FromSignature(""), 
	};

	const std::vector<Symbol> Client_CScriptedEntity_CallFunctionSym = { // int version. - Found no good identifyer to find it. Guessed it.
		NULL_SIGNATURE,
		NULL_SIGNATURE,
		Symbol::FromSignature(""), 
	};
#endif

	//---------------------------------------------------------------------------------
	// Purpose: networking Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> AllocChangeFrameListSym = {
		Symbol::FromName("_Z20AllocChangeFrameListii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x54\x41\x89\xFC\xBF\x28"), // 55 48 89 E5 41 55 41 54 41 89 FC BF 28
	};

	const std::vector<Symbol> g_FrameSnapshotManagerSym = {
		Symbol::FromName("framesnapshotmanager"),
		Symbol::FromSignature("\x48\x8B\x2A\x2A\x2A\x2A\x2A\x48\x8B\x38\x48\x8B\x07*\x50\x10\x48\x8D\x43\x15"), // 48 8B ?? ?? ?? ?? ?? 48 8B 38 48 8B 07 ?? 50 10 48 8D 43 15 || "framesnapshotmanager->LevelChanged()" || "sv.Clear()"
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

	//---------------------------------------------------------------------------------
	// Purpose: pas Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CM_VisSym = { // CM_Vis:
		Symbol::FromName("_Z6CM_VisPhiii"),
		Symbol::FromSignature("\x55\x83\xF9\x02\x48\x89\xE5\x41\x54\x53\x48\x89\xFB\x89\xD7\x89\xCA"), // 55 83 F9 02 48 89 E5 41 54 53 48 89 FB 89 D7 89 CA
	};

	//---------------------------------------------------------------------------------
	// Purpose: pas Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> SV_BroadcastVoiceDataSym = { // Sending voice from: %s
		Symbol::FromName("_Z21SV_BroadcastVoiceDataP7IClientiPcx"),
		Symbol::FromSignature("\x55\x48******\x48\x89\xE5\x41\x57\x41\x56\x41\x89\xF6\x41\x55\x49\x89\xFD\x41\x54"), // 55 48 ?? ?? ?? ?? ?? ?? 48 89 E5 41 57 41 56 41 89 F6 41 55 49 89 FD 41 54
	};

	//---------------------------------------------------------------------------------
	// Purpose: physenv Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> IVP_Mindist_do_impactSym = {
		Symbol::FromName("_ZN11IVP_Mindist9do_impactEv"),
	};

	const std::vector<Symbol> IVP_Event_Manager_Standard_simulate_time_eventsSym = {
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
}