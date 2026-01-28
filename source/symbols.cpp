#include "symbols.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const Symbol NULL_SIGNATURE = Symbol::FromSignature("");

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
	// If any symbol is required by the core they belong here.
	// Also, if many modules require the same function like with CBaseEntity::CalcAbsolutePosition we also move them to the core & do the stuff in util.cpp
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> InitLuaClassesSym = { // Find caller of CLuaClass::InitClasses -> "Error initializing class %s\n"
		Symbol::FromName("_Z14InitLuaClassesP13ILuaInterface"),
		Symbol::FromSignature("\x48\x8B\x05****\x48\x85\xC0**\x8B\x50\x10\x85\xD2**\x55\x48\x89\xE5\x53\x31\xDB******\x48\x8B*\x48\x8B\x3C\xD8\xE8\xD4"), // 48 8B 05 ?? ?? ?? ?? 48 85 C0 ?? ?? 8B 50 10 85 D2 ?? ?? 55 48 89 E5 53 31 DB ?? ?? ?? ?? ?? ?? 48 8B ?? 48 8B 3C D8 E8 D4
		Symbol::FromSignature("\x55\x8B\xEC*****\x85\xC0**\x8B\x00\x56\x8B\x30\x3B\xF0"), // 55 8B EC ?? ?? ?? ?? ?? 85 C0 ?? ?? 8B 00 56 8B 30 3B F0
		Symbol::FromSignature("\x40\x57\x48\x83\xEC\x20\x48******\x48\x8B\xF9\x48\x85\xC0"), // 40 57 48 83 EC 20 48 ?? ?? ?? ?? ?? ?? 48 8B F9 48 85 C0
	};

	const std::vector<Symbol> CLuaInterface_ShutdownSym = {
		Symbol::FromName("_ZN13CLuaInterface8ShutdownEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x54\x49\x89\xFC\x53\x4D\x8D"), // 55 48 89 E5 41 55 41 54 49 89 FC 53 4D 8D
		Symbol::FromSignature("\x53\x56\x57\x8B\xF9\x8B\x4F\x20"), // 53 56 57 8B F9 8B 4F 20
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x57\x48\x83\xEC\x20\x48\x8B\x41\x38"), // 48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B 41 38
	};

	const std::vector<Symbol> UsermessagesSym = { // "UserMessageBegin"
		Symbol::FromName("_ZL14g_UserMessages"),
		Symbol::FromSignature("\x55\x48\x8D*****\xBA\x01\x00\x00\x00\x48\x89\xE5\x53\x48\x83\xEC\x08\x2A\x2A\x2A\x2A"), // 55 48 8D ?? ?? ?? ?? ?? BA 01 00 00 00 48 89 E5 53 48 83 EC 08 2A 2A 2A 2A
		Symbol::FromSignature("\x2A\x2A\x2A\x2A\xE8****\x8B\xD0\x83\xFA\xFF**\xFF\x75\x0C"), // ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B D0 83 FA FF ?? ?? FF 75 0C
		Symbol::FromSignature("\x2A\x2A\x2A\x2A\xE8****\x8B\xF8\x83\xF8\xFF**\x48\x8B\xD3\x48************\x48"), // ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B F8 83 F8 FF ?? ?? 48 8B D3 48 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 48
	};

	const std::vector<Symbol> CBaseEntity_CalcAbsolutePositionSym = { // 64x = "%s[%i]:SetPos( %f %f %f ): Ignoring unreasonable position." -> Entity__SetPos -> CBaseEntity::SetAbsOrigin -> CBaseEntity::CalcAbsolutePosition
		Symbol::FromName("_ZN11CBaseEntity20CalcAbsolutePositionEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x83\xEC\x60\x8B\x87\xB0\x01\x00\x00"), // 55 48 89 E5 41 56 41 55 41 54 53 48 89 FB 48 83 EC 60 8B 87 B0 01 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x60\x56\x8B\xF1\x8B\x8E\x00\x01\x00\x00\x8B\xC1\xC1\xE8\x0B\xA8\x01******\x53\x57"), // 55 8B EC 83 EC 60 56 8B F1 8B 8E 00 01 00 00 8B C1 C1 E8 0B A8 01 ?? ?? ?? ?? ?? ?? 53 57
		Symbol::FromSignature("\x4C\x8B\xDC\x53\x48\x81\xEC\xA0\x00\x00\x00\x48******\x48\x33\xC4\x48\x89\x84\x24\x80\x00\x00\x00\x48\x8B\xD9\x8B\x89\x88\x01\x00\x00\x8B\xC1\xC1\xE8\x0B"), // 4C 8B DC 53 48 81 EC A0 00 00 00 48 ?? ?? ?? ?? ?? ?? 48 33 C4 48 89 84 24 80 00 00 00 48 8B D9 8B 89 88 01 00 00 8B C1 C1 E8 0B
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
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x45\x08\x56\x8B\xF1\x85\xC0**\x50\xE8****\x8B\x8E\x00\x01\x00\x00"), // 55 8B EC 8B 45 08 56 8B F1 85 C0 ?? ?? 50 E8 ?? ?? ?? ?? 8B 8E 00 01 00 00
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x30\x48\x8B\xD9\x48\x85\xD2"), // 40 53 48 83 EC 30 48 8B D9 48 85 D2
	};

	const Symbol lua_rawsetiSym = Symbol::FromName("lua_rawseti");

	const Symbol lua_rawgetiSym = Symbol::FromName("lua_rawgeti");

	const Symbol lj_tab_newSym = Symbol::FromName("lj_tab_new");

	const Symbol lj_gc_barrierfSym = Symbol::FromName("lj_gc_barrierf");

	const Symbol lj_tab_getSym = Symbol::FromName("lj_tab_get");

	const Symbol lua_setfenvSym = Symbol::FromName("lua_setfenv");

	const Symbol lua_touserdataSym = Symbol::FromName("lua_touserdata");

	const Symbol lua_typeSym = Symbol::FromName("lua_type");

	const Symbol lua_gcSym = Symbol::FromName("lua_gc");

	const Symbol lua_setallocfSym = Symbol::FromName("lua_setallocf");

	const Symbol luaL_checklstringSym = Symbol::FromName("luaL_checklstring");

	const Symbol lua_pcallSym = Symbol::FromName("lua_pcall");

	const Symbol lua_insertSym = Symbol::FromName("lua_insert");

	const Symbol lua_tobooleanSym = Symbol::FromName("lua_toboolean");

	const std::vector<Symbol> CGetSym = { // 64x ToDo
		Symbol::FromName("get"),
	};

	const std::vector<Symbol> gEntListSym = { // Search for "Master was null or not a master!\n"
		Symbol::FromName("gEntList"),
		Symbol::FromSignature("\x55\x48\x8D\x3D\x2A\x2A\x2A\x2A\x48\x89\xE5\x53\x48\x83\xEC\x08\xC6\x05\x2A\x2A\x43\x01\x00", 0x111),
		Symbol::FromSignature("****\x6A\x00\x6A\x00\x6A\x00\x6A\x00\x57\x6A\x00\xE8****\x8B\xD8\x85\xDB"), // ?? ?? ?? ?? 6A 00 6A 00 6A 00 6A 00 57 6A 00 E8 ?? ?? ?? ?? 8B D8 85 DB
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x40\x48\x8B\xFA\x48\x85\xC9", 0x1C),
	};

	const std::vector<Symbol> CSteam3Server_NotifyClientDisconnectSym = { // 64x = Search for "Dropped %s (%llu) from server (%s)\n"
		Symbol::FromName("_ZN13CSteam3Server22NotifyClientDisconnectEP11CBaseClient"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x53\x48\x89\xF3\x48\x83\xEC\x20\x48\x85\xF6"), // 55 48 89 E5 41 54 53 48 89 F3 48 83 EC 20 48 85 F6
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x20\x53\x56\x8B\x75\x08"),
		Symbol::FromSignature("\x48\x85\xD2\x0F\x84\x2A\x2A\x2A\x2A\x53\x57\x48\x83\xEC\x58"),
	};

	const Symbol SteamGameServer_ShutdownSym = Symbol::FromName("SteamGameServer_Shutdown"); // Same symbol for all versions.

	const Symbol GMOD_LoadBinaryModuleSym = Symbol::FromName("GMOD_LoadBinaryModule"); // Same symbol for all versions.

	const std::vector<Symbol> CM_VisSym = { // CM_Vis:
		Symbol::FromName("_Z6CM_VisPhiii"),
		Symbol::FromSignature("\x55\x83\xF9\x02\x48\x89\xE5\x41\x54\x53\x48\x89\xFB\x89\xD7\x89\xCA"), // 55 83 F9 02 48 89 E5 41 54 53 48 89 FB 89 D7 89 CA
		Symbol::FromSignature("\x55\x8B\xEC\x51\x8B\x4D\x14"),
		Symbol::FromSignature("\x40\x55\x48\x83\xEC\x30\x49\x63\xC1"),
	};

	const std::vector<Symbol> CBaseEntity_GetLuaEntitySym = {//Search for 'm_LuaEntity != ENTITY!'
		Symbol::FromName("_ZN11CBaseEntity12GetLuaEntityEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x83\xEC\x28\x4C\x8B\x25\x2A\x2A\x2A\x2A\x49\x83\x3C\x24\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 28 4C 8B 25 ? ? ? ? 49 83 3C 24 00
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x18*******\x57\x8B\xF9"), // 55 8B EC 83 EC 18 ?? ?? ?? ?? ?? ?? ?? 57 8B F9
		Symbol::FromSignature("\x40\x57\x48\x83\xEC\x40********\x48\x8B\xF9"), // 40 57 48 83 EC 40 ?? ?? ?? ?? ?? ?? ?? ?? 48 8B F9
	};

	const std::vector<Symbol> CGameEventManager_CreateEventSym = {// Search for "CreateEvent: event '%s' not registered."
		Symbol::FromName("_ZN17CGameEventManager11CreateEventEPKcb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x54\x53\x48\x89\xF3\x48\x83\xEC\x08\x48\x85\xF6**\x80\x3E\x00"), // 55 48 89 E5 41 55 41 54 53 48 89 F3 48 83 EC 08 48 85 F6 ?? ?? 80 3E 00
		Symbol::FromSignature("\x55\x8B\xEC\x56\x8B\x75\x08\x57\x85\xF6\x74\x2A\x80\x3E\x00"),
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x57\x41\x56\x41\x57\x48\x83\xEC\x20\x45\x0F\xB6\xF8"),	
	};

	//---------------------------------------------------------------------------------
	// Purpose: holylib Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> GetGModServerTagsSym = { // "gmws:"
		Symbol::FromName("_Z17GetGModServerTagsPcjb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x4C\x8D\xAD\x30\xFF\xFF\xFF"), // 55 48 89 E5 41 57 41 56 41 55 4C 8D AD 30 FF FF FF
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x80\x00\x00\x00\x8B*****\x53\xC7\x45\x84"), // 55 8B EC 81 EC 80 00 00 00 8B ?? ?? ?? ?? ?? 53 C7 45 84
		Symbol::FromSignature("\x40\x55\x56\x57\x41\x55\x41\x56\x41\x57\x48\x8D\x6C\x24\xD1"), // 40 55 56 57 41 55 41 56 41 57 48 8D 6C 24 D1
	};

	const std::vector<Symbol> CFuncLadder_PlayerGotOnSym = { // "reserved_spot" (big one?) -> Find function -> Find Caller
		Symbol::FromName("_ZN11CFuncLadder11PlayerGotOnEP11CBasePlayer"),
		Symbol::FromSignature("\x55\x48\x89\xF2\x0F\x57\xC0\x48\x89\xE5"), // 55 48 89 F2 0F 57 C0 48 89 E5
	};

	const std::vector<Symbol> CFuncLadder_PlayerGotOffSym = { // "reserved_spot" (big one?) -> Find function -> Find Caller
		Symbol::FromName("_ZN11CFuncLadder12PlayerGotOffEP11CBasePlayer"),
		Symbol::FromSignature("\x55\x48\x89\xF8\x48\x89\xF2\x48\x8D\xBF"), // 55 48 89 F8 48 89 F2 48 8D BF
	};

	const std::vector<Symbol> CHL2_Player_ExitLadderSym = {
		Symbol::FromName("_ZN11CHL2_Player10ExitLadderEv"),
		NULL_SIGNATURE, // ToDo
	};

	const std::vector<Symbol> CBaseEntity_SetMoveTypeSym = {// Search for %s[%i]: Changing collision rules within then you have CBaseEntity__CollisionRulesChanged and the 2nd xref it's this one
		Symbol::FromName("_ZN11CBaseEntity11SetMoveTypeE10MoveType_t13MoveCollide_t"),
		NULL_SIGNATURE, // Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x83\xEC\x10\x0F\xB6\x87\xF6\x01\x00\x00"), // 55 48 89 E5 41 56 41 55 41 54 53 48 89 FB 48 83 EC 10 0F B6 87 F6 01 00 00
	};

	const std::vector<Symbol> CHostState_State_ChangeLevelMPSym = {
		Symbol::FromName("_Z23HostState_ChangeLevelMPPKcS0_"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x49\x89\xFC\x53\x48\x89\xF3*****\x48\x89\xC7"), // 55 48 89 E5 41 54 49 89 FC 53 48 89 F3 ?? ?? ?? ?? ?? 48 89 C7
		Symbol::FromSignature("\x55\x8B\xEC*****\x8B\xC8*****\x68\x00\x01\x00\x00\xFF\x75\x08"), // 55 8B EC ?? ?? ?? ?? ?? 8B C8 ?? ?? ?? ?? ?? 68 00 01 00 00 FF 75 08
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x8B\xFA\x48\x8B\xD9*****\x48\x8B\xC8"), // 48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 ?? ?? ?? ?? ?? 48 8B C8
	};

	const std::vector<Symbol> CLuaInterface_RunStringExSym = { // @%s%s
		Symbol::FromName("_ZN13CLuaInterface11RunStringExEPKcS1_S1_bbbb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x49\x89\xF5\x41\x54\x53\x48\x89\xFB\x48\x81\xEC\x58\x01\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 49 89 F5 41 54 53 48 89 FB 48 81 EC 58 01 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x55\x10\x81\xEC\x4C\x01\x00\x00"), // 55 8B EC 8B 55 10 81 EC 4C 01 00 00
		Symbol::FromSignature("\x40\x55\x53\x56\x57\x41\x54\x41\x56\x41\x57\x48\x8D\xAC\x24\x50\xFF\xFF\xFF"), // 40 55 53 56 57 41 54 41 56 41 57 48 8D AC 24 50 FF FF FF
	};

	//---------------------------------------------------------------------------------
	// Purpose: gameevent Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CBaseClient_ProcessListenEventsSym = { // ProcessListenEvents
		Symbol::FromName("_ZN11CBaseClient19ProcessListenEventsEP16CLC_ListenEvents"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x49\x89\xF4\x53\x48\x8B\x1D****\x8B\x93\x0C\x10\x00\x00\x85\xD2\x41\x0F\x95\xC5******\x48"), // 55 48 89 E5 41 56 41 55 41 54 49 89 F4 53 48 8B 1D ?? ?? ?? ?? 8B 93 0C 10 00 00 85 D2 41 0F 95 C5 ?? ?? ?? ?? ?? ?? 48
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x57\x8D\x79\xF8\x8B*****\x57\x8B\x01"), // (ToDo: Verify!) 55 8B EC 53 56 57 8D 79 F8 8B ?? ?? ?? ?? ?? 57 8B 01
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
		// No windows since windows doesn't have/support serverplugins at all
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
	
	// None since all fixes were implemented into gmod

	//---------------------------------------------------------------------------------
	// Purpose: precachefix Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CVEngineServer_PrecacheModelSym = {
		Symbol::FromName("_ZN14CVEngineServer13PrecacheModelEPKcb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x44\x0F\xB6\xE2\x53\x48\x89\xF3\x44\x89\xE6\x48\x89\xDF"), // 55 48 89 E5 41 54 44 0F B6 E2 53 48 89 F3 44 89 E6 48 89
		Symbol::FromSignature("\x55\x8B\xEC\x56\x8B\x75\x08\x57\x8B\x7D\x0C\x57\x56"), // 55 8B EC 56 8B 75 08 57 8B 7D 0C 57 56
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x8B\xDA\x41\x0F\xB6\xF8"), // 48 89 5c 24 08 57 48 83 EC 20 48 8B DA 41 0F B6 F8
	};

	const std::vector<Symbol> CVEngineServer_PrecacheGenericSym = {
		Symbol::FromName("_ZN14CVEngineServer15PrecacheGenericEPKcb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xF3\x48\x83\xEC\x08\x0F\xB6\xF2\x48\x89\xDF"), // 55 48 89 E5 53 48 89 F3 48 83 EC 08 0F B6 F2 48 89 DF
		Symbol::FromSignature("\x55\x8B\xEC\xFF\x75\x0C\xFF\x08*****\x83\xC4\x08\x85\xC0\x79\x12"), // 55 8B EC FF 75 0C FF 75 08 ?? ?? ?? ?? ?? 83 C4 08 85 C0 79 12
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x20\x48\x8B\xDA\x41\x0F\xB6\xD0\x48\x8B\xCB"), // 40 53 48 83 EC 20 48 8B DA 41 0F B6 D0 48 8B CB
	};

	const std::vector<Symbol> SV_FindOrAddModelSym = {
		Symbol::FromName("_Z17SV_FindOrAddModelPKcb"),
		Symbol::FromSignature("\x55\x40\x80\xFE\x01\x48\x89\xFE\x48\x89\xE5\x48\x8B\x3D****\x19\xD2\x5D\x83\xE2\xFE\x31\xC9\x83\xC2\x03"), // 55 40 80 FE 01 48 89 FE 48 89 E5 48 8B 3D ?? ?? ?? ?? 19 D2 5D 83 E2 FE 31 C9 83 C2 03
		Symbol::FromSignature("\x55\x8B\xEC\x0F\xB6\x45\x0C*****\x6A\x00"), // 55 8B EC 0F B6 45 0C ?? ?? ?? ?? ?? 6A 00
		Symbol::FromSignature("\x44\x0F\xB6\xC2\x45\x33\xC9\x48\x8B\xD1"), // 44 0f B6 C2 45 33 C9 48 8B D1
	};

	const std::vector<Symbol> SV_FindOrAddGenericSym = {
		Symbol::FromName("_Z19SV_FindOrAddGenericPKcb"),
		Symbol::FromSignature("\x55\x40\x80\xFE\x01\x48\x89\xFE\x48\x89\xE5\x48\x8B\x3D\xC6***\x19\xD2"), // 55 40 80 FE 01 48 89 FE 48 89 E5 48 8B 3D C6 ?? ?? ?? 19 D2
		Symbol::FromSignature("\x55\x8B\xEC\x51******\x85\xC9\x75\x07"), // 55 8B EC 51 ?? ?? ?? ?? ?? ?? 85 C9 75 07
		Symbol::FromSignature("\x48\x89\x5C\x24\x18\x56\x48\x83\xEC\x30\x48******\x48"), // 48 89 5C 24 18 56 48 83 EC 30 48 8B F1 0F B6 DA 48 ?? ?? ?? ?? ?? ?? 48
	};

	//---------------------------------------------------------------------------------
	// Purpose: stringtable Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CNetworkStringTable_DeleteAllStringsSym = { // Error reading string table %s\n - CNetworkStringTableContainer::ReadStringTables -> Find the CNetworkStringTable::ReadStringTable call -> Find the CNetworkStringTable::DeleteAllStrings call
		Symbol::FromName("_ZN19CNetworkStringTable16DeleteAllStringsEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x08\x48\x8B\x7F\x50"), // 55 48 89 E5 53 48 89 FB 48 83 EC 08 48 8B 7F 50
		// No windows since there we use a completely different and unholy setup since Rubat never bothered for https://github.com/Facepunch/garrysmod-requests/issues/2766 :sob:
		// Also, on windows this function is inlined / you cannot even get a symbol since it does not exist, thats why we got such a screwed up workaround.
	};

	const std::vector<Symbol> CNetworkStringTable_DeconstructorSym = { // "Table %s\n" - Brings you to CNetworkStringTable::Dump
		Symbol::FromName("_ZN19CNetworkStringTableD0Ev"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x08\xE8\x8F\xFF\xFF\xFF\x48\x83\xC4\x08\x48\x89\xDF\x5B\x5D\xE9**\xEE\xFF"), // 55 48 89 E5 53 48 89 FB 48 83 EC 08 E8 8F FF FF FF 48 83 C4 08 48 89 DF 5B 5D E9 ?? ?? EE FF
		Symbol::FromSignature("\x55\x8B\xEC\x56\x8B\xF1\xFF\x76\x08"),
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x8D\x05\x2A\x2A\x2A\x2A\x48\x8B\xD9\x48\x89\x01\x8B\xFA\x48\x8B\x49\x10"),
		// Optional though not really required, tho it still would be good to have tbh as it acts as safety against invalid pointers for lua userdata
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

	const std::vector<Symbol> CTraceFilterSimple_ShouldHitEntitySym = { // Find '%3.2f: NextBotGroundLocomotion::GetBot()->OnLandOnGround' to get NextBotGroundLocomotion__OnLandOnGround then it should be above
		Symbol::FromName("_ZN18CTraceFilterSimple15ShouldHitEntityEP13IHandleEntityi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x49\x89\xFE\x41\x55\x41\x89\xD5\x41\x54\x53"), //55 48 89 E5 41 56 49 89 FE 41 55 41 89 D5 41 54 53
	};

	const std::vector<Symbol> MoveHelperServerSym = {// Find 'CBasePlayer::PhysicsSimulate' and then you'll get CBasePlayer__PhysicsSimulate and then it's into it
		Symbol::FromName("_Z16MoveHelperServerv"),
		Symbol::FromSignature("\x80\x3D\x29\x2A\x2A\x01\x00"), // 80 3D 29 ?? ?? 01 00
		Symbol::FromSignature("\xA1****\xA8\x01**\x83\xC8\x01\xC7*********\x68\x00\x30\x00\x00"), // A1 ?? ?? ?? ?? A8 01 ?? ?? 83 C8 01 C7 ?? ?? ?? ?? ?? ?? ?? ?? ?? 68 00 30 00 00
	};

	//---------------------------------------------------------------------------------
	// Purpose: pvs Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CGMOD_Player_SetupVisibilitySym = {//Find 'predicted_' then search for CGMOD_Player::CreateViewModel and once you got it, find the function bellow it in the xref VTable and you got it
		Symbol::FromName("_ZN12CGMOD_Player15SetupVisibilityEP11CBaseEntityPhi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x49\x89\xFD\x41\x54\x49\x89\xF4\x53\x48\x83\xEC\x08\xE8\x2A\x2A\x2A\x2A\x48\x8B\x1D\x2A\x2A\x2A\x2A"), // 55 48 89 E5 41 55 49 89 FD 41 54 49 89 F4 53 48 83 EC 08 E8 ? ? ? ? 48 8B 1D ? ? ? ?
		Symbol::FromSignature("\x55\x8B\xEC\x56\xFF\x75\x10\x8B\xF1\xFF\x75\x0C\xFF\x75\x08*****\x8B*****\x85\xC9**\x68\x94\x00\x00\x00"), // 55 8B EC 56 FF 75 10 8B F1 FF 75 0C FF 75 08 ?? ?? ?? ?? ?? 8B ?? ?? ?? ?? ?? 85 C9 ?? ?? 68 94 00 00 00
	};

	const std::vector<Symbol> CServerGameEnts_CheckTransmitSym = { // Find 'cl_updaterate' and you'll find CServerGameClients__ClientSettingsChanged then search for xref to get vtable and it will be above
		Symbol::FromName("_ZN15CServerGameEnts13CheckTransmitEP18CCheckTransmitInfoPKti"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x49\x89\xF6\x31\xF6"), //55 48 89 E5 41 57 41 56 49 89 F6 31 F6
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x20\x8B*****\x6A*\x8B\x01\xFF"), // 55 8B EC 83 EC 20 8B ?? ?? ?? ?? ?? 6A ?? 8B 01 FF
	};

	//---------------------------------------------------------------------------------
	// Purpose: filesystem Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CBaseFileSystem_FindFileInSearchPathSym = { // Search for "%s%c%s" and find the function that use it only 1 time
		Symbol::FromName("_ZN15CBaseFileSystem20FindFileInSearchPathER13CFileOpenInfo"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xFF\x41\x56\x41\x55\x41\x54\x53\x48\x89\xF3"), // 55 48 89 E5 41 57 49 89 FF 41 56 41 55 41 54 53 48 89 F3
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x38\x03\x00\x00\x53\x56\x57"), // 55 8B EC 81 EC 38 03 00 00 53 56 57
		Symbol::FromSignature("\x40\x53\x56\x57\x48\x81\xEC\xA0\x03\x00\x00"), // 40 53 56 57 48 81 EC A0 03 00 00
	};

	const std::vector<Symbol> CBaseFileSystem_IsDirectorySym = { // Search for "Unable to rename %s to %s!\n" to get CBaseFileSystem::RenameFile then xref and get the vtable and 2 bellow
		Symbol::FromName("_ZN15CBaseFileSystem11IsDirectoryEPKcS1_"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x4C\x8D\xAD\x38***\x41\x54"), // 55 48 89 E5 41 57 41 56 41 55 4C 8D AD 38 ?? ?? ?? 41 54
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x90\x04\x00\x00\x8D"), // 55 8B EC 81 EC 90 04 00 00 8D
		Symbol::FromSignature("\x40\x55\x53\x56\x57\x41\x57\x48\x8D\xAC\x24\x90\xFB\xFF\xFF"), // 40 55 53 56 57 41 57 48 8D AC 24 90 FB FF FF
	};

	const std::vector<Symbol> CBaseFileSystem_FindSearchPathByStoreIdSym = {
		// Find 'CBaseFileSystem::GetFileTime' then you have CBaseFileSystem__GetPathTime, go to xref and find in the vtable 3 function bellow, you'll get CBaseFileSystem__RegisterFileWhitelist which use CFileTracker2__GetFilesToUnloadForWhitelistChange which use it
		Symbol::FromName("_ZN15CBaseFileSystem23FindSearchPathByStoreIdEi"),
		Symbol::FromSignature("\x55\x0F\xB7\x87\x88\x00\x00\x00"), //55 0F B7 87 88 00 00 00
		NULL_SIGNATURE,
		// FUCK! The compiler inlined it into CFileTracker2::GetFilesToUnloadForWhitelistChange so we cannot access it!
		// This is like the most important function that we use
	};

	const std::vector<Symbol> CBaseFileSystem_FastFileTimeSym = {// Find 'CBaseFileSystem::GetFileTime' then you have CBaseFileSystem__GetPathTime and it's there
		Symbol::FromName("_ZN15CBaseFileSystem12FastFileTimeEPKNS_11CSearchPathEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x49\x89\xFD\x41\x54\x49\x89\xD4"), //55 48 89 E5 41 57 41 56 41 55 49 89 FD 41 54 49 89 D4
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x40\x01\x00\x00\x53\x56\x8B\x75\x08"), // 55 8B EC 81 EC 40 01 00 00 53 56 8B 75 08
		Symbol::FromSignature("\x40\x53\x56\x57\x48\x81\xEC\xA0\x01\x00\x00"), // 40 53 56 57 48 81 EC A0 01 00 00
	};

	const std::vector<Symbol> CBaseFileSystem_FixUpPathSym = {//Find 'BASE_PATH'
		Symbol::FromName("_ZN15CBaseFileSystem9FixUpPathEPKcPci"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x89\xD3\x48\x63\xD1"), //55 48 89 E5 41 56 41 55 41 54 49 89 FC 53 48 89 D3 48 63 D1
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\xFF\x75\x10"), // 55 8B EC 53 56 FF 75 10
		Symbol::FromSignature("\x48\x89\x5C\x24\x18\x56\x48\x83\xEC\x30"), // 48 89 5C 24 18 56 48 83 EC 30
	};

	std::vector<Symbol> CBaseFileSystem_OpenForReadSym = { // [Failed]
		Symbol::FromName("_ZN15CBaseFileSystem11OpenForReadEPKcS1_jS1_PPc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xD7\x41\x56\x4D\x89\xC6\x41\x55\x4D\x89\xCD\x41\x54"), // 55 48 89 E5 41 57 49 89 D7 41 56 4D 89 C6 41 55 4D 89 CD 41 54
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\xB8\x03\x00\x00\x53\x56\x57"), // 55 8B EC 81 EC B8 03 00 00 53 56 57
		Symbol::FromSignature("\x48\x89\x5C\x24\x20\x55\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\xAC\x24\x70\xFC\xFF\xFF"), // 48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 70 FC FF FF
	};

	const std::vector<Symbol> CBaseFileSystem_GetFileTimeSym = {
		Symbol::FromName("_ZN15CBaseFileSystem11GetFileTimeEPKcS1_"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x49\x89\xFE\x41\x55\x41\x54\x53\x48\x89\xD3"), // 55 48 89 E5 41 57 41 56 49 89 FE 41 55 41 54 53 48 89 D3
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x70\x03\x00\x00\x53\x8B\xD9"), // 55 8B EC 81 EC 70 03 00 00 53 8B D9 - "CBaseFileSystem::GetFileTime"
		Symbol::FromSignature("\x4C\x8B\xDC\x55\x56"), // 4C 8B DC 55 56
	};

	const std::vector<Symbol> CBaseFileSystem_AddSearchPathSym = {
		Symbol::FromName("_ZN15CBaseFileSystem13AddSearchPathEPKcS1_j"),
		Symbol::FromSignature("\x55\x45\x31\xC9\x41\xB8\x01\x00\x00\x00"), // 55 45 31 C9 41 B8 01 00 00 00
		// On Windows AddSearchPath_Internal is merged into this one, you can search for ".bsp" or go down 8th in the vtable
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x14\x01\x00\x00\x53\x8B\xD9\x56"), // 55 8B EC 81 EC 14 01 00 00 53 8B D9 56
		Symbol::FromSignature("\x48\x83\xEC\x38\xC6\x44\x24\x28\x00"), // 48 83 EC 38 C6 44 24 28 00
	};

	const std::vector<Symbol> CBaseFileSystem_AddVPKFileSym = { //Search for 'DUPLICATE: [%s]\n'
		Symbol::FromName("_ZN15CBaseFileSystem10AddVPKFileEPKcS1_j"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x49\x89\xD6\x41\x55\x41\x54\x53\x48\x89\xFB"), // 55 48 89 E5 41 57 41 56 49 89 D6 41 55 41 54 53 48 89 FB
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x20\x03\x00\x00\x53\x56\x57"), // 55 8B EC 81 EC 20 03 00 00 53 56 57
		Symbol::FromSignature("\x40\x55\x56\x57\x41\x56\x41\x57\x48\x81\xEC\xB0\x03\x00\x00"), // 40 55 56 57 41 56 41 57 48 81 EC B0 03 00 00
	};

	const std::vector<Symbol> CBaseFileSystem_CloseSym = { //Search for 'CBaseFileSystem::Close'
		Symbol::FromName("_ZN15CBaseFileSystem5CloseEPv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x49\x89\xFE\x41\x55\x41\x54\x49\x89\xF4\x53\x48\x83\xEC\x08\x48\x8B\x1D\x2A\x2A\x2A\x2A\x8B\x8B\x0C\x10\x00\x00\x85\xC9\x41\x0F\x95\xC5\x0F\x85\x2A\x2A\x2A\x2A\x4D\x85\xE4\x0F\x84\x2A\x2A\x2A\x2A\x49\x8B\x04\x24"), // 55 48 89 E5 41 57 41 56 49 89 FE 41 55 41 54 49 89 F4 53 48 83 EC 08 48 8B 1D ? ? ? ? 8B 8B 0C 10 00 00 85 C9 41 0F 95 C5 0F 85 ? ? ? ? 4D 85 E4 0F 84 ? ? ? ? 49 8B 04 24
		Symbol::FromSignature("\x55\x8B\xEC\x56\x57\x8B\xF9\x8B*****\x8B\xB1\x0C\x10\x00\x00\x85\xF6************************\x8B\x4D\x08\x85\xC9**\x68****\x8D**\x6A*\x50\xE8****\x83****\x8B\x01\x6A\x01"), // 55 8B EC 56 57 8B F9 8B ?? ?? ?? ?? ?? 8B B1 0C 10 00 00 85 F6 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 8B 4D 08 85 C9 ?? ?? 68 ?? ?? ?? ?? 8D ?? ?? 6A ?? 50 E8 ?? ?? ?? ?? 83 ?? ?? ?? ?? 8B 01 6A 01
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x30\x48\x8B\xF1\x48\x8B\xDA\x48\x8B\x0D\x2A\x2A\x2A\x2A\x8B\xB9\x0C\x10\x00\x00\x85\xFF\x74\x2A\xC7\x44\x24\x28\x04\x00\x00\x00\x4C\x8D\x0D\x2A\x2A\x2A\x2A\x45\x33\xC0\xC6\x44\x24\x20\x00\x48\x8D\x15\x2A\x2A\x2A\x2A\xFF\x15\x2A\x2A\x2A\x00"), // 48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 48 8B F1 48 8B DA 48 8B 0D ? ? ? ? 8B B9 0C 10 00 00 85 FF 74 ? C7 44 24 28 04 00 00 00 4C 8D 0D ? ? ? ? 45 33 C0 C6 44 24 20 00 48 8D 15 ? ? ? ? FF 15 ?? ?? ?? 00
	};

	const std::vector<Symbol> CBaseFileSystem_CSearchPath_GetDebugStringSym = {
		Symbol::FromName("_ZNK15CBaseFileSystem11CSearchPath14GetDebugStringEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x83\xEC\x18\x48\x8B\x47\x28"), //55 48 89 E5 53 48 83 EC 18 48 8B 47 28
	};

	const std::vector<Symbol> g_PathIDTableSym = {
		Symbol::FromName("g_PathIDTable"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x81\xEC\x58\x01\x00\x00", 0x8E), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 89 FB 48 81 EC 58 01 00 00
		Symbol::FromSignature("****\x50\xE8****\xB9******\x0F\xB7\x45\xFE"), // ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? B9 ?? ?? ?? ?? ?? ?? 0F B7 45 FE
		Symbol::FromSignature("\x48\x89\x5C\x24\x10\x57\x48\x83\xEC\x20\xB8\xFF\xFF\x00\x00\x48\x8B\xD9", 0x67), // 48 89 5C 24 10 57 48 83 EC 20 B8 FF FF 00 00 48 8B D9
	};

	//---------------------------------------------------------------------------------
	// Purpose: concommand Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> ConCommand_IsBlockedSym = { // "crosshair_setup" && "hud_fastswitch" && "retry" in the same sub_
		Symbol::FromName("_Z20ConCommand_IsBlockedPKc"),
		Symbol::FromSignature("\x48\x8B\x05****\x55\x48\x89\xE5\x41\x54\x49\x89\xFC\x53\x48\x8B*\x80\x78\x70\x00"), // 48 8B 05 ?? ?? ?? ?? 55 48 89 E5 41 54 49 89 FC 53 48 8B ?? 80 78 70 00
		Symbol::FromSignature("\x55\x8B\xEC\xA1****\x57\x8B\x7D\x08"), // 55 8B EC A1 ?? ?? ?? ?? 57 8B 7D 08
		Symbol::FromSignature("\x40\x57\x48\x83\xEC\x20\x48******\x48\x8B\xF9\x80\xB8\x90\x00\x00\x00\x00"), // 40 57 48 83 EC 20 48 ?? ?? ?? ?? ?? ?? 48 8B F9 80 B8 90 00 00 00 00
	};

	//---------------------------------------------------------------------------------
	// Purpose: vprof Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CLuaGamemode_CallFinishSym = {//Search for "CLuaGamemode::CallFinish"
		Symbol::FromName("_ZN12CLuaGamemode10CallFinishEi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x41\x89\xF4\x53\x48\x8B\x1D****\x8B\x93\x0C\x10\x00\x00"), // 55 48 89 E5 41 56 41 55 41 54 41 89 F4 53 48 8B 1D ?? ?? ?? ?? 8B 93 0C 10 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x0D****\x53\x56\x8B\xB1\x0C\x10\x00\x00\x85\xF6**\x6A\x04\x6A\x00\x68****\x6A\x00\x68**********\x8B\x0D****\x8B\x45\x08"), //  55 8B EC 8B 0D ?? ?? ?? ?? 53 56 8B B1 0C 10 00 00 85 F6 ?? ?? 6A 04 6A 00 68 ?? ?? ?? ?? 6A 00 68 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 45 08
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x30\x48\x8B\x0D\x2A\x2A\x2A\x2A\x8D\x5A\x02"), //40 53 48 83 EC 30 48 8B 0D ? ? ? ? 8D 5A 02
	};

	const std::vector<Symbol> CLuaGamemode_CallWithArgsSym = { // int version - Look at the difference in the call to [GM:CallWithArgs - !ThreadInMainThread]
		Symbol::FromName("_ZN12CLuaGamemode12CallWithArgsEi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x89\xF7\x41\x56\x41\x55\x49\x89\xFD\x41\x54\x53\x48\x83\xEC\x18"), // 55 48 89 E5 41 57 41 89 F7 41 56 41 55 49 89 FD 41 54 53 48 83 EC 18
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x57\x8B\xF9\x8B\x0D****\x8B\xB1\x0C\x10\x00\x00\x85\xF6**\x6A\x04\x6A\x00\x68****\x6A\x00\x68**********\xFF\x15****\x84\xC0**\x8B\x0D****\xFF\x75\x08\x8B\x01\xFF\x90\xEC\x01\x00\x00\x50\x68****\xFF\x15****\x8B\x07\x8B\xCF\x8B\x80\x8C\x00\x00\x00\xFF\xD0\x84\xC0**\x8B\x47\x3C\x8D\x4F\x3C"), //  55 8B EC 53 56 57 8B F9 8B 0D ?? ?? ?? ?? 8B B1 0C 10 00 00 85 F6 ?? ?? 6A 04 6A 00 68 ?? ?? ?? ?? 6A 00 68 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 84 C0 ?? ?? 8B 0D ?? ?? ?? ?? FF 75 08 8B 01 FF 90 EC 01 00 00 50 68 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B 07 8B CF 8B 80 8C 00 00 00 FF D0 84 C0 ?? ?? 8B 47 3C 8D 4F 3C
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x30\x48\x89\x6C\x24\x40\x48\x8B\xD9\x48\x8B\x0D\x2A\x2A\x2A\x2A\x8B\xEA\x48\x89\x74\x24\x48\x8B\xB1\x0C\x10\x00\x00\x85\xF6\x74\x2A\xC7\x44\x24\x28\x04\x00\x00\x00\x4C\x8D\x0D\x2A\x2A\x2A\x2A\x45\x33\xC0\xC6\x44\x24\x20\x00\x48\x8D\x15\x2A\x2A\x2A\x2A\xFF\x15\x26\x23\x75\x00"), //  0 53 48 83 EC 30 48 89 6C 24 40 48 8B D9 48 8B 0D ? ? ? ? 8B EA 48 89 74 24 48 8B B1 0C 10 00 00 85 F6 74 ? C7 44 24 28 04 00 00 00 4C 8D 0D ? ? ? ? 45 33 C0 C6 44 24 20 00 48 8D 15 ? ? ? ? FF 15 26 23 75 00
	};

	const std::vector<Symbol> CLuaGamemode_CallWithArgsStrSym = { 
		Symbol::FromName("_ZN12CLuaGamemode12CallWithArgsEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xF7\x41\x56\x41\x55\x49\x89\xFD\x41\x54\x53\x48\x83\xEC\x18*******\x8B\x93\x0C\x10\x00\x00\x85\xD2\x41\x0F\x95\xC6"), // 55 48 89 E5 41 57 49 89 F7 41 56 41 55 49 89 FD 41 54 53 48 83 EC 18 ?? ?? ?? ?? ?? ?? ?? 8B 93 0C 10 00 00 85 D2 41 0F 95 C6
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x30\x48\x89\x6C\x24\x40\x48\x8B\xD9\x48\x8B\x0D\x2A\x2A\x2A\x2A\x48\x8B\xEA\x48\x89\x74\x24\x48\x8B\xB1\x0C\x10\x00\x00\x85\xF6\x74\x2A\xC7\x44\x24\x28\x04\x00\x00\x00\x4C\x8D\x0D\x2A\x2A\x2A\x2A\x45\x33\xC0\xC6\x44\x24\x20\x00\x48\x8D\x15\x2A\x2A\x2A\x2A\xFF\x15\x25\x22\x75\x00"), //40 53 48 83 EC 30 48 89 6C 24 40 48 8B D9 48 8B 0D ? ? ? ? 48 8B EA 48 89 74 24 48 8B B1 0C 10 00 00 85 F6 74 ? C7 44 24 28 04 00 00 00 4C 8D 0D ? ? ? ? 45 33 C0 C6 44 24 20 00 48 8D 15 ? ? ? ? FF 15 25 22 75 00
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x30\x48\x89\x6C\x24\x40\x48\x8B\xD9\x48\x8B\x0D\x2A\x2A\x2A\x2A\x48\x8B\xEA\x48\x89\x74\x24\x48\x8B\xB1\x0C\x10\x00\x00\x85\xF6\x74\x2A\xC7\x44\x24\x28\x04\x00\x00\x00\x4C\x8D\x0D\x2A\x2A\x2A\x2A\x45\x33\xC0\xC6\x44\x24\x20\x00\x48\x8D\x15\x2A\x2A\x2A\x2A\xFF\x15\x25\x22\x75\x00"), //40 53 48 83 EC 30 48 89 6C 24 40 48 8B D9 48 8B 0D ? ? ? ? 48 8B EA 48 89 74 24 48 8B B1 0C 10 00 00 85 F6 74 ? C7 44 24 28 04 00 00 00 4C 8D 0D ? ? ? ? 45 33 C0 C6 44 24 20 00 48 8D 15 ? ? ? ? FF 15 25 22 75 00
	};

	const std::vector<Symbol> CLuaGamemode_CallStrSym = { // const char* version - Look at the difference in the call to [GM:CallWithArgs - !ThreadInMainThread] Also search for "CLuaGamemode::" not CLuaGamemode::Call on 64x
		Symbol::FromName("_ZN12CLuaGamemode4CallEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x49\x89\xF6\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x83\xEC\x08*******\x8B\x93\x0C\x10\x00\x00\x85\xD2\x41\x0F\x95\xC5******\xE8"), // 55 48 89 E5 41 57 41 56 49 89 F6 41 55 41 54 49 89 FC 53 48 83 EC 08 ?? ?? ?? ?? ?? ?? ?? 8B 93 0C 10 00 00 85 D2 41 0F 95 C5 ?? ?? ?? ?? ?? ?? E8
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x30\x48\x89\x6C\x24\x40\x48\x8B\xD9\x48\x8B\x0D\x2A\x2A\x2A\x2A\x48\x8B\xEA\x48\x89\x74\x24\x48\x8B\xB1\x0C\x10\x00\x00\x85\xF6\x74\x2A\xC7\x44\x24\x28\x04\x00\x00\x00\x4C\x8D\x0D\x2A\x2A\x2A\x2A\x45\x33\xC0\xC6\x44\x24\x20\x00\x48\x8D\x15\x2A\x2A\x2A\x2A\xFF\x15\xD5\x24\x75\x00"), //40 53 48 83 EC 30 48 89 6C 24 40 48 8B D9 48 8B 0D ? ? ? ? 48 8B EA 48 89 74 24 48 8B B1 0C 10 00 00 85 F6 74 ? C7 44 24 28 04 00 00 00 4C 8D 0D ? ? ? ? 45 33 C0 C6 44 24 20 00 48 8D 15 ? ? ? ? FF 15 D5 24 75 00
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x30\x48\x89\x6C\x24\x40\x48\x8B\xD9\x48\x8B\x0D\x2A\x2A\x2A\x2A\x48\x8B\xEA\x48\x89\x74\x24\x48\x8B\xB1\x0C\x10\x00\x00\x85\xF6\x74\x2A\xC7\x44\x24\x28\x04\x00\x00\x00\x4C\x8D\x0D\x2A\x2A\x2A\x2A\x45\x33\xC0\xC6\x44\x24\x20\x00\x48\x8D\x15\x2A\x2A\x2A\x2A\xFF\x15\xD5\x24\x75\x00"), //40 53 48 83 EC 30 48 89 6C 24 40 48 8B D9 48 8B 0D ? ? ? ? 48 8B EA 48 89 74 24 48 8B B1 0C 10 00 00 85 F6 74 ? C7 44 24 28 04 00 00 00 4C 8D 0D ? ? ? ? 45 33 C0 C6 44 24 20 00 48 8D 15 ? ? ? ? FF 15 D5 24 75 00
	};

	const std::vector<Symbol> CLuaGamemode_CallSym = {
		Symbol::FromName("_ZN12CLuaGamemode4CallEi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x89\xF6\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x83\xEC\x08"), // 55 48 89 E5 41 57 41 56 41 89 F6 41 55 41 54 49 89 FC 53 48 83 EC 08
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x57\x8B\xF9\x8B\x0D****\x8B\xB1\x0C\x10\x00\x00\x85\xF6**\x6A\x04\x6A\x00\x68****\x6A\x00\x68**********\xFF\x15****\x84\xC0**\x8B\x0D****\xFF\x75\x08\x8B\x01\xFF\x90\xEC\x01\x00\x00\x50\x68****\xFF\x15****\x8B\x07\x8B\xCF\x8B\x80\x8C\x00\x00\x00\xFF\xD0\x84\xC0**\x8B\x47\x3C\x8D\x5F\x3C"), //  55 8B EC 53 56 57 8B F9 8B 0D ?? ?? ?? ?? 8B B1 0C 10 00 00 85 F6 ?? ?? 6A 04 6A 00 68 ?? ?? ?? ?? 6A 00 68 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 84 C0 ?? ?? 8B 0D ?? ?? ?? ?? FF 75 08 8B 01 FF 90 EC 01 00 00 50 68 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B 07 8B CF 8B 80 8C 00 00 00 FF D0 84 C0 ?? ?? 8B 47 3C 8D 5F 3C
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x30\x48\x89\x6C\x24\x40\x48\x8B\xD9\x48\x8B\x0D\x2A\x2A\x2A\x2A\x8B\xEA\x48\x89\x74\x24\x48\x8B\xB1\x0C\x10\x00\x00\x85\xF6\x74\x2A\xC7\x44\x24\x28\x04\x00\x00\x00\x4C\x8D\x0D\x2A\x2A\x2A\x2A\x45\x33\xC0\xC6\x44\x24\x20\x00\x48\x8D\x15\x2A\x2A\x2A\x2A\xFF\x15\xE6\x25\x75\x00"), //40 53 48 83 EC 30 48 89 6C 24 40 48 8B D9 48 8B 0D ? ? ? ? 8B EA 48 89 74 24 48 8B B1 0C 10 00 00 85 F6 74 ? C7 44 24 28 04 00 00 00 4C 8D 0D ? ? ? ? 45 33 C0 C6 44 24 20 00 48 8D 15 ? ? ? ? FF 15 E6 25 75 00
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
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x8B\xFA\x48\x8B\xD9\xFF\x15\xAA\x33\x74\x00"), // 48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 FF 15 AA 33 74 00
	};

	const std::vector<Symbol> CScriptedEntity_StartFunctionSym = { // int version - hasTraced
		Symbol::FromName("_ZN15CScriptedEntity13StartFunctionEi"), 
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x89\xF6\x41\x55\x41\x54\x49\x89\xFC\x53\xE8****"), // 55 48 89 E5 41 56 41 89 F6 41 55 41 54 49 89 FC 53 E8 ?? ?? ?? ??
		Symbol::FromSignature("\x55\x8B\xEC\x56\x8B\xF1\xFF\x15****\x84\xC0**\x80\x7E\x04\x00**\x8B\x4E\x0C\x8B\x01\xFF\x90\x84\x03\x00\x00\x8B\xC8\x8B\x10\xFF\x92\x84\x00\x00\x00\x8B\x0D****\x6A\x00"), //  55 8B EC 56 8B F1 FF 15 ?? ?? ?? ?? 84 C0 ?? ?? 80 7E 04 00 ?? ?? 8B 4E 0C 8B 01 FF 90 84 03 00 00 8B C8 8B 10 FF 92 84 00 00 00 8B 0D ?? ?? ?? ?? 6A 00
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x8B\xFA\x48\x8B\xD9\xFF\x15\x7B\x34\x74\x00"), // 48 89 5C 24 08 57 48 83 EC 20 8B FA 48 8B D9 FF 15 7B 34 74 00
	};

	const std::vector<Symbol> CScriptedEntity_CallSym = { // SENT:AcceptInput
		Symbol::FromName("_ZN15CScriptedEntity4CallEii"),
		Symbol::FromSignature("\x48\x8B*****\x55\x83\xC6\x01\x85\xD2\x48\x89\xE5"), // 48 8B ?? ?? ?? ?? ?? 55 83 C6 01 85 D2 48 89 E5
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x0D****\x8B\x55\x0C\x56\x8B\x75\x08\x8B\x01\x46\x85\xD2"), //  55 8B EC 8B 0D ?? ?? ?? ?? 8B 55 0C 56 8B 75 08 8B 01 46 85 D2
		Symbol::FromSignature("\x48\x83\xEC\x28\x48\x8B\x0D\x2A\x2A\x2A\x2A\xFF\xC2"), // 48 83 EC 28 48 8B 0D ? ? ? ? FF C2
	};

	const std::vector<Symbol> CScriptedEntity_CallFunctionStrSym = { // const char* version - SetupDataTables
		Symbol::FromName("_ZN15CScriptedEntity12CallFunctionEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x83\xEC\x38\x0F\xB6\x47\x08\x84\xC0"), // 55 48 89 E5 41 55 41 54 53 48 89 FB 48 83 EC 38 0F B6 47 08 84 C0
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x14\x56\x8B\xF1\x80\x7E\x04\x00**\x32\xC0\x5E\x8B\xE5\x5D\xC2\x04\x00"), // 55 8B EC 83 EC 14 56 8B F1 80 7E 04 00 ?? ?? 32 C0 5E 8B E5 5D C2 04 00
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x40\x80\x79\x08\x00"), // 48 89 5C 24 08 57 48 83 EC 40 80 79 08 00
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
	// NOTE: This is just optimizations, having CGMOD_Player_CreateViewModelSym on Windows would be good but all the other things are probably best for Linux only.
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> AllocChangeFrameListSym = { // I'm still surprised I managed to get this one :^
		Symbol::FromName("_Z20AllocChangeFrameListii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x54\x41\x89\xFC\xBF\x28"), // 55 48 89 E5 41 55 41 54 41 89 FC BF 28
		Symbol::FromSignature("\x55\x8B\xEC\x56\x57\x6A\x18"),
		Symbol::FromSignature("\x48\x89\x74\x24\x18\x48\x89\x7C\x24\x20\x41\x56\x48\x83\xEC\x20\x48\x89\x6C\x24\x38"),
		//Search for "SV_PackEntity: SnagChangeFrameList mi"
	};

	const std::vector<Symbol> SendTable_CullPropsFromProxiesSym = { // CPropCullStack::AddProp - m_pOutProps overflowed
		Symbol::FromName("_Z30SendTable_CullPropsFromProxiesPK9SendTablePKiiiPK20CSendProxyRecipientsiS6_iPii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xFF\x41\x56\x41\x89\xCE"), // 55 48 89 E5 41 57 49 89 FF 41 56 41 89 CE
		Symbol::FromSignature("\x55\x8B\xEC\xB8\x48\x41\x00\x00"),
		Symbol::FromSignature("\x48\x89\x5C\x24\x10\x48\x89\x74\x24\x18\x48\x89\x7C\x24\x20\x55\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\xAC\x24\x50\xBE\xFF\xFF"),
	};

	const std::vector<Symbol> SendTable_WritePropListSym = { // SendTable_WritePropList->inputBuffer
		Symbol::FromName("_Z23SendTable_WritePropListPK9SendTablePKviP8bf_writeiPKii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x81\xEC\x88\x00\x00\x00\x48\x89\xBD\x60\xFF\xFF\xFF"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 81 EC 88 00 00 00 48 89 BD 60 FF FF FF
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x40\x83\x7D\x20\x00"),
		Symbol::FromSignature("\x4C\x89\x4C\x24\x20\x48\x89\x4C\x24\x08\x55"),
	};

	const std::vector<Symbol> CBaseServer_WriteDeltaEntitiesSym = { // CBaseServer::WriteDeltaEntities
		Symbol::FromName("_ZN11CBaseServer18WriteDeltaEntitiesEP11CBaseClientP12CClientFrameS3_R8bf_write"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xCC\x53\x48\x89\xFB"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 CC 53 48 89 FB
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x84\x04\x00\x00"),
		Symbol::FromSignature("\x40\x55\x53\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\xAC\x24\xF8\xFB\xFF\xFF"),
	};

	const std::vector<Symbol> SendTable_CalcDeltaSym = {//Search for "SendTable_CalcDelta/toBits"
		Symbol::FromName("_Z19SendTable_CalcDeltaPK9SendTablePKviS3_iPiii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xCF\x41\x56\x49\x89\xF6"), // 55 48 89 E5 41 57 49 89 CF 41 56 49 89 F6
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x4C\x80\x3D\x2A\x2A\x2A\x2A\x00"),
		Symbol::FromSignature("\x48\x89\x5C\x24\x18\x55\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\x6C\x24\xF9"),
	};

	const std::vector<Symbol> SV_DetermineUpdateTypeSym = { // Search for xref of SendTable_CalcDelta
		Symbol::FromName("_ZL22SV_DetermineUpdateTypeR16CEntityWriteInfo"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x81\xEC\xA8\x80\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 89 FB 48 81 EC A8 80 00 00
		Symbol::FromSignature("\x55\x8B\xEC\xB8\x08\x40\x00\x00\xE8\x2A\x2A\x2A\x2A\x53\x56\x57\x8B\x7D\x08"),
		Symbol::FromSignature("\x40\x53\xB8\x60\x40\x00\x00"),
	};

	const std::vector<Symbol> PackedEntity_GetPropsChangedAfterTickSym = { //Search into SV_DetermineUpdateTypeSym
		Symbol::FromName("_ZN12PackedEntity24GetPropsChangedAfterTickEiPii"),
		Symbol::FromSignature("\x55\x48\x8B\x7F\x50\x48\x89\xE5\x48\x85\xFF"), // 55 48 8B 7F 50 48 89 E5 48 85 FF
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x49\x30"),
		Symbol::FromSignature("\x48\x8B\x49\x50\x48\x85\xC9\x74\x2A\x48\x8B\x01\x48\xFF\x60\x18"),
	};

	const std::vector<Symbol> CGameServer_SendClientMessagesSym = { //Search for "SendClientMessages"
		Symbol::FromName("_ZN11CGameServer18SendClientMessagesEb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x89\xF7\x41\x56\x41\x55\x49\x89\xFD\x41\x54\x53\x48\x81\xEC\x48\x08\x00\x00"), // 55 48 89 E5 41 57 41 89 F7 41 56 41 55 49 89 FD 41 54 53 48 81 EC 48 08 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x1C\x04\x00\x00\x53\x56\x57"),
		Symbol::FromSignature("\x4C\x8B\xDC\x48\x81\xEC\xA8\x08\x00\x00"),
	};

	const std::vector<Symbol> CBaseEntity_GMOD_SetShouldPreventTransmitToPlayerSym = { //Find CBaseEntity::GetLuaEntity with "m_LuaEntity != ENTITY!"
		Symbol::FromName("_ZN11CBaseEntity37GMOD_SetShouldPreventTransmitToPlayerEP11CBasePlayerb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x89\xD7\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53"), // 55 48 89 E5 41 57 41 89 D7 41 56 41 55 41 54 49 89 FC 53
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x8B\x75\x08\x57\x8B\xF9\x85\xF6\x74\x2A\x8B\x06\x8B\xCE\xFF\x50\x08\x8B\x00"),
		Symbol::FromSignature("\x40\x53\x56\x57\x41\x55\x41\x56\x48\x83\xEC\x20"),
	};

	const std::vector<Symbol> CBaseEntity_GMOD_ShouldPreventTransmitToPlayerSym = { //Find CBaseEntity::GetLuaEntity with "m_LuaEntity != ENTITY!"
		Symbol::FromName("_ZN11CBaseEntity34GMOD_ShouldPreventTransmitToPlayerEP11CBasePlayer"),
		Symbol::FromSignature("\x55\x31\xC0\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x18\x66\x83\xBF\xE6\x14\x00\x00\x00"), // 55 31 C0 48 89 E5 53 48 89 FB 48 83 EC 18 66 83 BF E6 14 00 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x56\x8B\xF1\x66\x83\xBE\x3E\x13\x00\x00\x00"),
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x20\x66\x83\xB9\xBE\x14\x00\x00\x00"),
	};

	const std::vector<Symbol> g_FrameSnapshotManagerSym = {
		Symbol::FromName("framesnapshotmanager"),
		Symbol::FromSignature("\x48\x8B\x2A\x2A\x2A\x2A\x2A\x48\x8B\x38\x48\x8B\x07*\x50\x10\x48\x8D\x43\x15"), // 48 8B ?? ?? ?? ?? ?? 48 8B 38 48 8B 07 ?? 50 10 48 8D 43 15 || "framesnapshotmanager->LevelChanged()" || "sv.Clear()"
		Symbol::FromSignature("\x2A\x2A\x2A\x2A\x83\xC4\x04\x8B\x01\xFF\x50\x04\x6A\x40"), // ?? ?? ?? ?? 83 C4 04 8B 01 FF 50 04 6A 40 || "framesnapshotmanager"
		Symbol::FromSignature("\x40\x55\x53\x56\x57\x41\x55\x41\x56\x48\x8D\xAC\x24\x58\xFD\xFF\xFF", 0x3F6)
	};

	const std::vector<Symbol> g_PropTypeFnsSym = {
		Symbol::FromName("g_PropTypeFns"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x49\x89\xF5\x41\x54\x53\x48\x83\xEC\x08\x4C\x8B\x67\x20\xE8\x2A\x2A\x2A\x2A\x41\x8B\x55\x1C", 0x51), // 55 48 89 E5 41 57 41 56 41 55 49 89 F5 41 54 53 48 83 EC 08 4C 8B 67 20 E8 ? ? ? ? 41 8B 55 1C
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x4D\x08\x83\xEC\x28", 0x10B), //55 8B EC 8B 4D 08 83 EC 28
		Symbol::FromSignature("\x40\x55\x53\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\xAC\x24\x88\xFB\xFF\xFF", 0x733), //40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 88 FB FF FF
	};

	const std::vector<Symbol> g_BSPDataSym = { // Search CM_Vis 
		Symbol::FromName("g_BSPData"),
		Symbol::FromSignature("\x55\x83\xF9\x02", 0x1F), // 55 83 F9 02
		Symbol::FromSignature("\x55\x8B\xEC\x51\x8B\x4D\x14", 0xB3), // 55 83 F9 02
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x30\x0F\xB6\x1D\x2A\x2A\x2A\x2A", 0xD), // Search for "gmod_uncache_test" and find CM_FreeMap
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
		Symbol::FromSignature("\x55\x8B\xEC\xB8\x2A\xC4\x00\x00"), // 55 8B EC B8 ?? C4 00 00
		Symbol::FromSignature("\x40\x53\x55\x56\x57\x41\x54\x41\x55\x41\x57\xB8\xE0\xC4\x00\x00")
	};

	const std::vector<Symbol> InvalidateSharedEdictChangeInfosSym = {//Search for "NUM_FOR_EDICTINFO: bad pointer" then you get CBaseEdict::GetChangeAccessor and search for the xref
		Symbol::FromName("_Z32InvalidateSharedEdictChangeInfosv"),
		Symbol::FromSignature("\x0F\xB7\x05\x2A\x2A\x2A\x2A\x66\x83\xF8\xFF\x74\x2A"), // 0F B7 05 ? ? ? ? 66 83 F8 FF 74 ?
		Symbol::FromSignature("\x66*****\xB9\xFF\xFF\x00\x00\x66\x3B\xC1**\x57\x33\xFF"), // 66 ?? ?? ?? ?? ?? B9 FF FF 00 00 66 3B C1 ?? ?? 57 33 FF
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x20\x0F\xB7\x05\x2A\x2A\x2A\x2A")
	};

	const std::vector<Symbol> PackEntities_NormalSym = { //Search for 'PackEntities_Normal: Invalid edict ind
		Symbol::FromName("_Z19PackEntities_NormaliPP11CGameClientP14CFrameSnapshot"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x81\xEC\xC8\xC4\x03\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 81 EC C8 C4 03 00
		Symbol::FromSignature("\x55\x8B\xEC\xB8\x2C\x80\x01\x00"), // 55 8B EC B8 2C 80 01 00
		Symbol::FromSignature("\x40\x55\x53\x57\x48\x8D\xAC\x24\x60\x00\xFD\xFF")
	};

	const std::vector<Symbol> CGMOD_Player_CreateViewModelSym = { //Search for "CGMOD_Player::PostThink" to find CGMOD_Player__PostThink then xref and find vtable, go upward 12 times
		Symbol::FromName("_ZN12CGMOD_Player15CreateViewModelEi"),
		Symbol::FromSignature("\x55\xBA\x01\x00\x00\x00\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x4C\x63\xE6\x53\x44\x89\xE6\x48\x89\xFB\x48\x83\xEC\x18\xE8\xAC\xBD\x9C\xFF\x48\x85\xC0\x74\x17\x48\x83\xC4\x18\x5B\x41\x5C\x41\x5D\x41\x5E\x41\x5F\x5D\xC3\x0F\x1F\x84\x00\x00\x00\x00\x00\x48\x8D\x3D\xD7\xFB\x24\x00\xBE\xFF\xFF\xFF\xFF\xE8\x7F\xCC\x95\xFF\x48\x85\xC0\x49\x89\xC5\x74\xD0\xF6\x83\xB1\x01\x00\x00\x08\x0F\x85\xA2\x00\x00\x00\x48\x8D\xB3\x54\x03\x00\x00\x4C\x89\xEF\xE8\x4B\x43\x85\xFF\x48\x89\xDE\x4C\x89\xEF\xE8\x30\xC9\x88\xFF\x44\x89\xE6\x4C\x89\xEF\xE8\xE5\xCA\x88\xFF\x4C\x89\xEF\xE8\x7D\x9E\xAA\xFF\x31\xD2\x48\x89\xDE\x4C\x89\xEF\xE8\x40\xE4\x86\xFF\x49\x8B\x45\x00\x4C\x89\xEF\xFF\x50\x18\x41\x83\xFC\x02\x8B\x00\x89\x45\xC0\x0F\x87\x70\xFF\xFF\xFF\x4E\x8D\xAC\xA3\x10\x28\x00\x00\xBA\x04\x00\x00\x00\x48\x8D\x75\xC0\x4C\x89\xEF\xE8\x0E\xD8\x69\xFF\x85\xC0\x0F\x84\x4F\xFF\xFF\xFF\x80\xBB\x98\x00\x00\x00\x00\x75\x32\x4C\x8B\x73\x40\x4D\x85\xF6\x74\x08\x41\x8B\x06\xF6\xC4\x01\x74\x2A\x8B\x45\xC0\x42\x89\x84\xA3\x10\x28\x00\x00\xE9\x25\xFF\xFF\xFF\x0F\x1F\x40\x00\x48\x89\xDF\xE8\x50\x27\x85\xFF\xE9\x51\xFF\xFF\xFF\x80\x8B\xA0\x00\x00\x00\x01\xEB\xD6\x83\xC8\x01\x4C\x89\xF7\x49\x29\xDD\x41\x89\x06\x45\x89\xEF\xE8\x4E\x26\x92\xFF\x48\x8B\x15\x87\x2C\x93\x00\x48\x8B\x0A\x0F\xB7\x39\x66\x39\x78\x02\x74\x47\x0F\xB7\x89\xA2\x0F\x00\x00\x66\x83\xF9\x64\x0F\x84\x95\x00\x00\x00\x66\x89\x08\x48\x8B\x0A\x66\x83\x81\xA2\x0F\x00\x00\x01\x0F\xB7\x09\x66\x89\x48\x02\x0F\xB7\x00\x48\x8D\x0C\x80\x48\x8B\x02\xBA\x01\x00\x00\x00\x48\x8D\x04\xC8\x66\x44\x89\x68\x02\x66\x89\x50\x28\xE9\x68\xFF\xFF\xFF\x0F\xB7\x10\x48\x8D\x14\x92\x48\x8D\x14\xD1\x48\x8D\x7A\x02\x0F\xB7\x77\x26\x66\x85\xF6\x74\x36\x66\x44\x3B\x6A\x02\x0F\x84\x45\xFF\xFF\xFF\x8D\x4E\xFF\x48\x83\xC2\x04\x0F\xB7\xC9\x48\x8D\x0C\x4A\xEB\x10\x90\x48\x83\xC2\x02\x66\x44\x3B\x7A\xFE\x0F\x84\x25\xFF\xFF\xFF\x48\x39\xCA\x75\xEC\x66\x83\xFE\x13\x74\x11\x8D\x46\x01\x66\x89\x47\x26\x66\x44\x89\x2C\x77\xE9\x09\xFF\xFF\xFF\x66\xC7\x40\x02\x00\x00\x41\x81\x0E\x00\x01\x00\x00\xE9\xF7\xFE\xFF\xFF"), // 55 BA 01 00 00 00 48 89 E5 41 57 41 56 41 55 41 54 4C 63 E6 53 44 89 E6 48 89 FB 48 83 EC 18 E8 AC BD 9C FF 48 85 C0 74 17 48 83 C4 18 5B 41 5C 41 5D 41 5E 41 5F 5D C3 0F 1F 84 00 00 00 00 00 48 8D 3D D7 FB 24 00 BE FF FF FF FF E8 7F CC 95 FF 48 85 C0 49 89 C5 74 D0 F6 83 B1 01 00 00 08 0F 85 A2 00 00 00 48 8D B3 54 03 00 00 4C 89 EF E8 4B 43 85 FF 48 89 DE 4C 89 EF E8 30 C9 88 FF 44 89 E6 4C 89 EF E8 E5 CA 88 FF 4C 89 EF E8 7D 9E AA FF 31 D2 48 89 DE 4C 89 EF E8 40 E4 86 FF 49 8B 45 00 4C 89 EF FF 50 18 41 83 FC 02 8B 00 89 45 C0 0F 87 70 FF FF FF 4E 8D AC A3 10 28 00 00 BA 04 00 00 00 48 8D 75 C0 4C 89 EF E8 0E D8 69 FF 85 C0 0F 84 4F FF FF FF 80 BB 98 00 00 00 00 75 32 4C 8B 73 40 4D 85 F6 74 08 41 8B 06 F6 C4 01 74 2A 8B 45 C0 42 89 84 A3 10 28 00 00 E9 25 FF FF FF 0F 1F 40 00 48 89 DF E8 50 27 85 FF E9 51 FF FF FF 80 8B A0 00 00 00 01 EB D6 83 C8 01 4C 89 F7 49 29 DD 41 89 06 45 89 EF E8 4E 26 92 FF 48 8B 15 87 2C 93 00 48 8B 0A 0F B7 39 66 39 78 02 74 47 0F B7 89 A2 0F 00 00 66 83 F9 64 0F 84 95 00 00 00 66 89 08 48 8B 0A 66 83 81 A2 0F 00 00 01 0F B7 09 66 89 48 02 0F B7 00 48 8D 0C 80 48 8B 02 BA 01 00 00 00 48 8D 04 C8 66 44 89 68 02 66 89 50 28 E9 68 FF FF FF 0F B7 10 48 8D 14 92 48 8D 14 D1 48 8D 7A 02 0F B7 77 26 66 85 F6 74 36 66 44 3B 6A 02 0F 84 45 FF FF FF 8D 4E FF 48 83 C2 04 0F B7 C9 48 8D 0C 4A EB 10 90 48 83 C2 02 66 44 3B 7A FE 0F 84 25 FF FF FF 48 39 CA 75 EC 66 83 FE 13 74 11 8D 46 01 66 89 47 26 66 44 89 2C 77 E9 09 FF FF FF 66 C7 40 02 00 00 41 81 0E 00 01 00 00 E9 F7 FE FF FF"),
		Symbol::FromSignature("\x55\x8B\xEC\x53\x8B\x5D\x08\x56\x6A\x01"),
		Symbol::FromSignature("\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x48\x63\xF2"),
	};

	const std::vector<Symbol> CBaseCombatCharacter_SetTransmitSym = {//Search for "models/manhack.mdl" and find CNPC_Manhack__Spawn, then vtable above
		Symbol::FromName("_ZN20CBaseCombatCharacter11SetTransmitEP18CCheckTransmitInfob"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x49\x89\xF5\x41\x54\x49\x89\xFC\x53\x48\x83\xEC\x08\x48\x8B\x47\x40"), // 55 48 89 E5 41 57 41 56 41 55 49 89 F5 41 54 49 89 FC 53 48 83 EC 08 48 8B 47 40
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x57\x8B\xF9\x8B\x47\x18\x85\xC0\x74\x2A\x0F\xBF\x48\x06\xEB\x2A\x33\xC9\x8B\x5D\x08\x8B\xF1\xC1\xEE\x05\x83\xE1\x1F\xB8\x01\x00\x00\x00\xD3\xE0\x8B\x93\x08\x20\x00\x00\x85\x04\xB2\x0F\x85\x2A\x2A\x2A\x2A\x8B\x75\x0C\x8B\xCF\x56\x53\xE8\x2A\x2A\x2A\x2A\x8B\x03"),
		Symbol::FromSignature("\x48\x89\x5C\x24\x10\x48\x89\x6C\x24\x18\x56\x48\x83\xEC\x20\x48\x8B\x41\x30\x41\x0F\xB6\xE8\x48\x8B\xF2\x48\x8B\xD9\x48\x85\xC0\x74\x2A\x0F\xBF\x48\x06"),
	};

	const std::vector<Symbol> CBaseAnimating_SetTransmitSym = {//Find "Setting CBaseAnimating to non-studio model %s  (type:%i)" to get CBaseAnimating__SetModel then find the last xref and take 4 function upper
		Symbol::FromName("_ZN14CBaseAnimating11SetTransmitEP18CCheckTransmitInfob"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x49\x89\xF4\x53\x48\x89\xFB\x48\x8B\x47\x40\x48\x85\xC0\x0F\x84\x2A\x2A\x2A\x2A\x0F\xB7\x48\x06\x0F\xBF\xC1\xC1\xE8\x05\x89\xC0\x48\x8D\x34\x85\x2A\x2A\x2A\x2A\xB8\x01\x00\x00\x00\xD3\xE0\x49\x8B\x8C\x24\x10\x20\x00\x00\x85\x04\x31\x74\x2A\x5B\x41\x5C\x41\x5D\x41\x5E\x5D\xC3\x90\x44\x0F\xB6\xEA\x4C\x89\xE6\x48\x89\xDF\x44\x89\xEA\xE8\x2A\x2A\x2A\x2A\x8B\x83\x3C\x16\x00\x00"), // 55 48 89 E5 41 56 41 55 41 54 49 89 F4 53 48 89 FB 48 8B 47 40 48 85 C0 0F 84 ? ? ? ? 0F B7 48 06 0F BF C1 C1 E8 05 89 C0 48 8D 34 85 ? ? ? ? B8 01 00 00 00 D3 E0 49 8B 8C 24 10 20 00 00 85 04 31 74 ? 5B 41 5C 41 5D 41 5E 5D C3 90 44 0F B6 EA 4C 89 E6 48 89 DF 44 89 EA E8 ? ? ? ? 8B 83 3C 16 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x57\x8B\xF9\x8B\x47\x18\x85\xC0**\x0F\xBF\x48\x06**\x33\xC9\x8B\x5D\x08\x8B\xF1\xC1\xEE\x05\x83\xE1\x1F\xB8\x01\x00\x00\x00\xD3\xE0\x8B\x93\x08\x20\x00\x00\x85\x04\xB2**\x8B\x75\x0C\x8B\xCF\x56\x53\xE8\x2A\x2A\x2A\x2A\x8B\x97\x64\x14\x00\x00"), //  55 8B EC 53 56 57 8B F9 8B 47 18 85 C0 ?? ?? 0F BF 48 06 ?? ?? 33 C9 8B 5D 08 8B F1 C1 EE 05 83 E1 1F B8 01 00 00 00 D3 E0 8B 93 08 20 00 00 85 04 B2 ?? ?? 8B 75 0C 8B CF 56 53 E8 C0 BC 02 00 8B 97 64 14 00 00 83 FA FF ?? ?? A1 E0 09 9B 10 8B CA 81 E1 FF 3F 00 00 C1 EA 0E 03 C9
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x48\x8B\x41\x30\x41\x0F\xB6\xF0\x48\x8B\xFA\x48\x8B\xD9\x48\x85\xC0\x74\x2A\x44\x0F\xBF\x48\x06\xEB\x2A\x45\x33\xC9\x48\x8B\x87\x10\x20\x00\x00\x41\x8B\xD1\x41\x83\xE1\x1F\x48\xC1\xEA\x05\x8B\x04\x90\x44\x0F\xA3\xC8\x0F\x82\x2A\x2A\x2A\x2A\x48\x8B\xD7\xE8\x2A\x2A\x2A\x2A\x8B\x93\x14\x16\x00\x00"), //  48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B 41 30 41 0F B6 F0 48 8B FA 48 8B D9 48 85 C0 74 ? 44 0F BF 48 06 EB ? 45 33 C9 48 8B 87 10 20 00 00 41 8B D1 41 83 E1 1F 48 C1 EA 05 8B 04 90 44 0F A3 C8 0F 82 ? ? ? ? 48 8B D7 E8 ? ? ? ? 8B 93 14 16 00 00
	};

	//---------------------------------------------------------------------------------
	// Purpose: steamworks Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CSteam3Server_OnLoggedOffSym = { // Search for "GSL token expired"
		Symbol::FromName("_ZN13CSteam3Server11OnLoggedOffEP26SteamServersDisconnected_t"),
		Symbol::FromSignature("\x83\xBF\x30****\x0F\x84\x7C***\x55\x48\x89\xE5"), // 83 BF 30 ?? ?? ?? ?? 0F 84 7C ?? ?? ?? 55 48 89 E5
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x4D\x0C\x83\xEC\x48"), // 55 8B EC 8B 4D 0C 83 EC 48
		Symbol::FromSignature("\x48\x89\x5C\x24\x18\x57\x48\x81\xEC\x90\x00\x00\x00"), // 48 89 5C 24 18 57 48 81 EC 90 00 00 00
	};

	const std::vector<Symbol> CSteam3Server_OnLogonSuccessSym = { //Search for ""Connection to Steam servers successful."
		Symbol::FromName("_ZN13CSteam3Server14OnLogonSuccessEP23SteamServersConnected_t"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x28\x64\x48\x8B"), // 55 48 89 E5 53 48 89 FB 48 83 EC 28 64 48 8B
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x1C\x56\x8B\xF1\x8B\x4E\x04"), // 55 8B EC 83 EC 1C 56 8B F1 8B 4E 04
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x60\x48\x8B\x05\x2A\x2A\x2A\x2A\x48\x33\xC4\x48\x89\x44\x24\x50\x48\x8B\xD9"), // 40 53 48 83 EC 60 48 8B 05 ? ? ? ? 48 33 C4 48 89 44 24 50 48 8B D9
	};

	const std::vector<Symbol> CSteam3Server_ShutdownSym = {//Look into CSteam3Server_Activate
		Symbol::FromName("_ZN13CSteam3Server8ShutdownEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x08\x48\x83\x7F**\x74\x5A"), // 55 48 89 E5 53 48 89 FB 48 83 EC 08 48 83 7F ?? ?? 74 5A
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x08\x56\x8B\xF1\x83\x7E\x04\x00"), // 55 8B EC 83 EC 08 56 8B F1 83 7E 04 00
		//Inline in win64 unfortunately
	};

	const std::vector<Symbol> CSteam3Server_ActivateSym = { //Search for "-steamport"
		Symbol::FromName("_ZN13CSteam3Server8ActivateENS_11EServerTypeE"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x41\x89\xF4\x53\x48\x89\xFB\x48\x81\xEC"), // 55 48 89 E5 41 57 41 56 41 55 41 54 41 89 F4 53 48 89 FB 48 81 EC
		Symbol::FromSignature("\x55\x8B\xEC\xA1\x2A\x2A\x2A\x2A\x81\xEC\x24\x01\x00\x00"), // 55 8B EC A1 ? ? ? ? 81 EC 24 01 00 00
		Symbol::FromSignature("\x40\x55\x53\x56\x48\x8D\x6C\x24\x80"), // 40 55 53 56 48 8D 6C 24 80
	};

	const std::vector<Symbol> Steam3ServerSym = { // 64x = Found with "Warning: sv_steamgroup is not applicabl" and then find the unk_[xyz], search for it's usage and find a small function.
		Symbol::FromName("_Z12Steam3Serverv"),
		Symbol::FromSignature("\x55\x48\x8D\x05****\x48\x89\xE5\x5D\xC3\x90\x66\x90\x55\x31\xC0\xB9\x08"), // 55 48 8D 05 ?? ?? ?? ?? 48 89 E5 5D C3 90 66 90 55 31 C0 B9 08
		Symbol::FromName("_Z12Steam3Serverv"),
		Symbol::FromName("_Z12Steam3Serverv"),
	};

	const std::vector<Symbol> SV_InitGameServerSteamSym = {
		Symbol::FromName("_Z22SV_InitGameServerSteamv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x83\xEC\x08\x48\x8B\x1D********\x00\x00\x01"), // 55 48 89 E5 53 48 83 EC 08 48 8B 1D ?? ?? ?? ?? ?? ?? ?? ?? 00 00 01
		//Inline in win unfortunately
	};

	const std::vector<Symbol> CSteam3Server_NotifyClientConnectSym = { // 64x = "S3: Client"
		Symbol::FromName("_ZN13CSteam3Server19NotifyClientConnectEP11CBaseClientjR8netadr_sPKvj"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x81\xEC\xB8\x00\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 FC 53 48 81 EC B8 00 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x48\x56\x8B\xF1"), // 55 8B EC 83 EC 48 56 8B F1
		Symbol::FromSignature("\x40\x55\x57\x41\x54\x41\x56"), // 40 55 57 41 54 41 56
	};

	const std::vector<Symbol> CSteam3Server_SendUpdatedServerDetailsSym = { // 64x = "steamblocking:%d"
		Symbol::FromName("_ZN13CSteam3Server24SendUpdatedServerDetailsEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x83\xEC\x68\x64\x48\x8B\x04\x25\x28"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 FC 53 48 83 EC 68 64 48 8B 04 25 28
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x48\x8B\xC1\x89\x45\xE4"), // 55 8B EC 83 EC 48 8B C1 89 45 E4
		Symbol::FromSignature("\x4C\x8B\xDC\x56\x48\x81\xEC\x90\x00\x00\x00"), // 4C 8B DC 56 48 81 EC 90 00 00 00
	};

	const std::vector<Symbol> CSteam3Server_CheckForDuplicateSteamIDSym = { // 64x = "STEAM UserID"
		Symbol::FromName("_ZN13CSteam3Server24CheckForDuplicateSteamIDEPK11CBaseClient"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x83\xEC\x68\x48\x89\x75\x80"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 68 48 89 75 80
		//Inline in win unfortunately
	};

	//---------------------------------------------------------------------------------
	// Purpose: voicechat Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> SV_BroadcastVoiceDataSym = { // Sending voice from: %s
		Symbol::FromName("_Z21SV_BroadcastVoiceDataP7IClientiPcx"),
		Symbol::FromSignature("\x55\x48******\x48\x89\xE5\x41\x57\x41\x56\x41\x89\xF6\x41\x55\x49\x89\xFD\x41\x54"), // 55 48 ?? ?? ?? ?? ?? ?? 48 89 E5 41 57 41 56 41 89 F6 41 55 49 89 FD 41 54
		Symbol::FromSignature("\x55\x8B\xEC\xA1****\x83\xEC\x50\x8B\x50\x48"), // 55 8B EC A1 ?? ?? ?? ?? 83 EC 50 8B 50 48
		Symbol::FromSignature("\x48\x89\x5C\x24\x20\x56\x57\x41\x56\x48******\x8B\xF2\x4C\x8B\xF1"), // 48 89 5C 24 20 56 57 41 56 48 ?? ?? ?? ?? ?? ?? 8B F2 4C 8B F1
	};

	const std::vector<Symbol> CVoiceGameMgrHelper_CanPlayerHearPlayerSym = { // Good luck.
		Symbol::FromName("_ZN19CVoiceGameMgrHelper19CanPlayerHearPlayerEP11CBasePlayerS1_Rb"),
		// Needs all other platforms.
	};

	//---------------------------------------------------------------------------------
	// Purpose: physenv Symbols
	// ToDo: Get the Linux64 and Windows32 symbols.
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> IVP_Mindist_do_impactSym = { // Search for '%s in contact with %s, crash. dist = %d', then do xref to 'IVP Failed at %s %d\n' and find the xref bellow
		Symbol::FromName("_ZN11IVP_Mindist9do_impactEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x49\x89\xFD\x41\x54\x53\x4C\x8B\x67\x40")//,55 48 89 E5 41 56 41 55 49 89 FD 41 54 53 4C 8B 67 40
	};

	const std::vector<Symbol> IVP_Event_Manager_Standard_simulate_time_eventsSym = { // NOTE: is virtual
		Symbol::FromName("_ZN26IVP_Event_Manager_Standard20simulate_time_eventsEP16IVP_Time_ManagerP15IVP_Environment8IVP_Time"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xF7\x41\x56\x45\x31\xF6")//55 48 89 E5 41 57 49 89 F7 41 56 45 31 F6
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

	const std::vector<Symbol> GMod_Util_IsPhysicsObjectValidSym = { // PhysObject [%s][Entity %d] or "Tried to use a NULL physics object!""
		Symbol::FromName("_ZN4GMod4Util20IsPhysicsObjectValidEP14IPhysicsObject"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x8D\x75\xEC"), // 55 48 89 E5 53 48 89 FB 48 8D 75 EC
		Symbol::FromSignature("\x55\x8B\xEC\x51\x8B*****\x8D\x55\xFC\xC7\x45\xFC\x00\x00\x00\x00"), // 55 8B EC 51 8B ?? ?? ?? ?? ?? 8D 55 FC C7 45 FC 00 00 00 00
	};

	const std::vector<Symbol> CPhysicsHook_FrameUpdatePostEntityThinkSym = { // "CPhysicsHook::FrameUpdatePostEntityThink" - VPROF Call
		Symbol::FromName("_ZN12CPhysicsHook26FrameUpdatePostEntityThinkEv"),
	};

	const std::vector<Symbol> CCollisionEvent_FrameUpdateSym = {
		Symbol::FromName("_ZN15CCollisionEvent11FrameUpdateEv"),
		NULL_SIGNATURE,
	};

	const std::vector<Symbol> CPhysicsEnvironment_DestroyObjectSym = {//Search for 'error deleting physics object\n'
		Symbol::FromName("_ZN19CPhysicsEnvironment13DestroyObjectEP14IPhysicsObject"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x53\x48\x83\xEC\x10\x48\x85\xF6"),//55 48 89 E5 41 54 53 48 83 EC 10 48 85 F6
		// On Windows we will use the vtable instead to detour it.... maybe we should do the same on linux?
	};

	const std::vector<Symbol> CPhysicsEnvironment_RestoreSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment7RestoreERK19physrestoreparams_t"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x45\x31\xE4\x53\x48\x89\xF3"),//55 48 89 E5 41 57 41 56 41 55 41 54 45 31 E4 53 48 89 F3
	};

	const std::vector<Symbol> CPhysicsEnvironment_TransferObjectSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment14TransferObjectEP14IPhysicsObjectP19IPhysicsEnvironment"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x81\xEC\xD8\x00\x00\x00"),//55 48 89 E5 41 57 41 56 41 55 41 54 49 89 FC 53 48 81 EC D8 00 00 00
	};

	const std::vector<Symbol> CPhysicsEnvironment_CreateSphereObjectSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment18CreateSphereObjectEfiRK6VectorRK6QAngleP14objectparams_tb"),
		Symbol::FromSignature("\x55\x45\x0F\xB6\xC9\x48\x89\xE5\x53"),//55 45 0F B6 C9 48 89 E5 53
	};

	const std::vector<Symbol> CPhysicsEnvironment_UnserializeObjectFromBufferSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment27UnserializeObjectFromBufferEPvPhjb"),
		Symbol::FromSignature("\x55\x45\x0F\xB6\xC0"),//55 45 0F B6 C0
	};

	const std::vector<Symbol> CPhysicsEnvironment_CreatePolyObjectStaticSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment22CreatePolyObjectStaticEPK12CPhysCollideiRK6VectorRK6QAngleP14objectparams_t"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x28\xC7\x04\x24\x01\x00\x00\x00"),//55 48 89 E5 53 48 89 FB 48 83 EC 28 C7 04 24 01 00 00 00
	};

	const std::vector<Symbol> CPhysicsEnvironment_CreatePolyObjectSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment16CreatePolyObjectEPK12CPhysCollideiRK6VectorRK6QAngleP14objectparams_t"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x28\xC7\x04\x24\x00\x00\x00\x00"),//55 48 89 E5 53 48 89 FB 48 83 EC 28 C7 04 24 00 00 00 00
	};

	const std::vector<Symbol> CPhysicsEnvironment_D2Sym = {
		Symbol::FromName("_ZN19CPhysicsEnvironmentD2Ev"),
		Symbol::FromSignature("\x55\x48\x8D\x05\x2A\x2A\x2A\x2A\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x89\x07\x48\x8B\x87\xF0\x00\x00\x00"),//55 48 8D 05 ? ? ? ? 48 89 E5 41 56 41 55 41 54 53 48 89 FB 48 89 07 48 8B 87 F0 00 00 00
	};

	const std::vector<Symbol> CPhysicsEnvironment_C2Sym = {
		Symbol::FromName("_ZN19CPhysicsEnvironmentC2Ev"),
		Symbol::FromSignature("\x55\x48\x8D\x05\x2A\x2A\x2A\x2A\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x83\xC4\x80"),//55 48 8D 05 ? ? ? ? 48 89 E5 41 56 41 55 41 54 53 48 89 FB 48 83 C4 80
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

	const std::vector<Symbol> CCollisionProperty_MarkSurroundingBoundsDirtySym = { //Search for 'Clamping SetLocalVelocity(%f,%f,%f) on %s' to get CBaseEntity::SetLocalVelocity then into it find CBaseEntity::InvalidatePhysicsRecursive
		Symbol::FromName("_ZN18CCollisionProperty24MarkPartitionHandleDirtyEv"),
		Symbol::FromSignature("\x55\x48\x8B\x57\x08\x48\x89\xE5\x48\x8B\x42\x40"), //55 48 8B 57 08 48 89 E5 48 8B 42 40
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
		// This is not worth for Windows at all, so lets not even try as its just a small performance benefit when actively using vprof
	};

	//---------------------------------------------------------------------------------
	// Purpose: cvar Symbols
	// NOTE: Gmod has merged this optimization, though ours is still like 100x faster??? Nice
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
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x45\x10\xC7\x00\x80\x00\x00\x00"), // 55 8B EC 8B 45 10 C7 00 80 00 00 00
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
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x45\x89\xCF\x41\x56\x49\x89\xCE", 0x190), // 55 48 89 E5 41 57 45 89 CF 41 56 49 89 CE
	};

	const std::vector<Symbol> NET_SetTimeSym = {
		Symbol::FromName("_Z11NET_SetTimed"),
		Symbol::FromSignature("\x55\x66\x0F\x28\xC8"), // 55 66 0F 28 C8
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

	const std::vector<Symbol> Bootil_File_ChangeMonitor_HasChangesSym = {
		Symbol::FromName("_ZN6Bootil4File13ChangeMonitor10HasChangesEv"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: Soundscape Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CEnvSoundscape_UpdateForPlayerSym = { // "CSoundscapeSystem::Init" -> Find vtable of CSoundscapeSystem and from there find it
		Symbol::FromName("_ZN14CEnvSoundscape15UpdateForPlayerER11ss_update_t"),
		// Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x89\xF3\x48******\x80"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 FC 53 48 89 F3 48 ?? ?? ?? ?? ?? ?? 80
		// I feel like this symbol is wrong :/
	};

	const std::vector<Symbol> CEnvSoundscape_WriteAudioParamsToSym = {
		Symbol::FromName("_ZN14CEnvSoundscape18WriteAudioParamsToER13audioparams_t"),
	};

	const std::vector<Symbol> g_SoundscapeSystemSym = {
		Symbol::FromName("g_SoundscapeSystem"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: networkthreading Symbols
	// NOTE: Tbh this really has no purpose on Windows / really is useful only on Linux Dedicated Servers so let's only support Linux 32x/64x
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> NET_ProcessSocketSym = {
		Symbol::FromName("_Z17NET_ProcessSocketiP28IConnectionlessPacketHandler"),
	};

	const std::vector<Symbol> NET_GetPacketSym = {
		Symbol::FromName("_Z13NET_GetPacketiPh"),
	};

	const std::vector<Symbol> Filter_ShouldDiscardSym = {
		Symbol::FromName("_Z20Filter_ShouldDiscardRK8netadr_s"),
	};

	const std::vector<Symbol> NET_FindNetChannelSym = {
		Symbol::FromName("_Z18NET_FindNetChanneliR8netadr_s"),
	};

	// Since we don't have s_NetChannels we gotta do this shit.
	// Would probably be useful to ask for it to be exposed but I don't want to wait 2+ years
	const std::vector<Symbol> CNetChan_ConstructorSym = {
		Symbol::FromName("_ZN8CNetChanC2Ev"),
	};

	const std::vector<Symbol> CNetChan_DeconstructorSym = {
		Symbol::FromName("_ZN8CNetChanD2Ev"),
	};

	// Yay commands :3
	// Becomes unessesary with https://github.com/Facepunch/garrysmod-requests/issues/2974
	const std::vector<Symbol> writeipSym = {
		Symbol::FromName("_ZL7writeipRK8CCommand"),
	};

	const std::vector<Symbol> listipSym = {
		Symbol::FromName("_ZL6listipRK8CCommand"),
	};

	const std::vector<Symbol> removeipSym = {
		Symbol::FromName("_ZL8removeipRK8CCommand"),
	};

	const std::vector<Symbol> Filter_Add_fSym = {
		Symbol::FromName("_ZL12Filter_Add_fRK8CCommand"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: networkingreplacement Symbols
	// NOTE: Way to complex for windows
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> SV_EnsureInstanceBaselineSym = {
		Symbol::FromName("_Z25SV_EnsureInstanceBaselineP11ServerClassiPKvi"),
	};

	const std::vector<Symbol> SendTable_EncodeSym = {
		Symbol::FromName("_Z16SendTable_EncodePK9SendTablePKvP8bf_writeiP10CUtlMemoryI20CSendProxyRecipientsiEb"),
	};

	const std::vector<Symbol> CFrameSnapshotManager_GetPreviouslySentPacketSym = {
		Symbol::FromName("_ZN21CFrameSnapshotManager23GetPreviouslySentPacketEii"),
	};

	const std::vector<Symbol> CFrameSnapshotManager_UsePreviouslySentPacketSym = {
		Symbol::FromName("_ZN21CFrameSnapshotManager23UsePreviouslySentPacketEP14CFrameSnapshotii"),
	};

	const std::vector<Symbol> CFrameSnapshotManager_CreatePackedEntitySym = {
		Symbol::FromName("_ZN21CFrameSnapshotManager18CreatePackedEntityEP14CFrameSnapshoti"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: nw2 Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> GMODTable_EncodeSym = {
		Symbol::FromName("_Z16GMODTable_EncodePKhP8DVariantPK8SendPropP8bf_writei"),
	};
}