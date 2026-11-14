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
 * 4 - Windows Dedicated 32x (falls back to Windows 32x if non existent)
 * 5 - Windows Dedicated 64x (falls back to Windows 64x if non existent)
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
		Symbol::FromSignature("\x48\x8B\x05****\x48\x85\xC0\x74\x64\x8B\x50\x10\x85\xD2\x7E\x5D\x55"), // 48 8B 05 ?? ?? ?? ?? 48 85 C0 74 64 8B 50 10 85 D2 7E 5D 55
		Symbol::FromSignature("\x55\x8B\xEC\x51*****\x85\xC0******\x33\xC9"), // 55 8B EC 51 ?? ?? ?? ?? ?? 85 C0 ?? ?? ?? ?? ?? ?? 33 C9
		Symbol::FromSignature("\x40\x57\x48\x83\xEC\x20\x48******\x48\x8B\xF9\x48\x85\xC0"), // 40 57 48 83 EC 20 48 ?? ?? ?? ?? ?? ?? 48 8B F9 48 85 C0
	};

	const std::vector<Symbol> CLuaInterface_ShutdownSym = {
		Symbol::FromName("_ZN13CLuaInterface8ShutdownEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x4C\x8D\x35****\x41\x54\x49\x89\xFC"), // 55 48 89 E5 41 56 41 55 4C 8D 35 ?? ?? ?? ?? 41 54 49 89 FC
		Symbol::FromSignature("\x53\x56\x57\x8B\xF9\x8B\x4F\x20"), // 53 56 57 8B F9 8B 4F 20
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x57\x48\x83\xEC\x20\x48\x8B\x41\x38"), // 48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B 41 38
	};

	const std::vector<Symbol> UsermessagesSym = { // "UserMessageBegin"
		Symbol::FromName("_ZL14g_UserMessages"),
		Symbol::FromSignature("\x48\x8D\x1D****\x48\x83\xEC\x08\x48\x89\xDF\xE8****\x48\x83\xC4\x08\x48\x89\xDE\x5B\x48\x8D\x15****\x5D\x48\x8D\x3D****\xE9****\x66\x2E\x0F\x1F\x84\x00\x00\x00\x00\x00"), // 48 8D 1D ?? ?? ?? ?? 48 83 EC 08 48 89 DF E8 ?? ?? ?? ?? 48 83 C4 08 48 89 DE 5B 48 8D 15 ?? ?? ?? ?? 5D 48 8D 3D ?? ?? ?? ?? E9 ?? ?? ?? ?? 66 2E 0F 1F 84 00 00 00 00 00
		Symbol::FromSignature("\x2A\x2A\x2A\x2A\xE8****\x8B\xD0\x83\xFA\xFF**\xFF\x75\x0C"), // ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B D0 83 FA FF ?? ?? FF 75 0C
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x8B\xF9\x48\x8B\xDA\x48\x8B\x0D****\xE8****", 0x10), // 48 89 5C 24 08 57 48 83 EC 20 48 8B F9 48 8B DA 48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ??
	};

	const std::vector<Symbol> CBaseEntity_CalcAbsolutePositionSym = { // 64x = "%s[%i]:SetPos( %f %f %f ): Ignoring unreasonable position." -> Entity__SetPos -> CBaseEntity::SetAbsOrigin -> CBaseEntity::CalcAbsolutePosition
		Symbol::FromName("_ZN11CBaseEntity20CalcAbsolutePositionEv"),
		Symbol::FromSignature("\x8B\x87\xB8\x01\x00\x00\xF6\xC4\x08\x75\x05\xC3\x0F\x1F\x40\x00"), // 8B 87 B8 01 00 00 F6 C4 08 75 05 C3 0F 1F 40 00
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x60\x56\x8B\xF1\x8B\x8E\x0C\x01\x00\x00\x8B\xC1\xC1\xE8\x0B\xA8\x01\x0F\x84****"),
		Symbol::FromSignature("\x4C\x8B\xDC\x53\x48\x81\xEC\x90\x00\x00\x00\x48\x8B\xD9\x8B\x89\x90\x01\x00\x00"), // 4C 8B DC 53 48 81 EC 90 00 00 00 48 8B D9 8B 89 90 01 00 00
	};

	const std::vector<Symbol> CBaseAnimating_InvalidateBoneCacheSym = {
		Symbol::FromName("_ZN14CBaseAnimating19InvalidateBoneCacheEv"),
		Symbol::FromSignature("\x48\x8B\xBF\x70\x16\x00\x00\xE9****\x0F\x1F\x40\x00\x83\xFE\x03\x76\x0B"), // 48 8B BF 70 16 00 00 E9 ?? ?? ?? ?? 0F 1F 40 00 83 FE 03 76 0B
		// How to hopefully find it(Still a pain): Search for "%5.2f : %s : %s : %5.3f\n" -> CBaseAnimating::StudioFrameAdvance() -> StudioFrameAdvanceInternal() -> Studio_InvalidateBoneCacheIfNotMatching() -> Find CBaseAnimating::InvalidateBoneCache by checking which function calls it with -1.0f
		// Else: Search for 'aim_yaw' -> CNPC_SecurityCamera__UpdateFacing -> CBaseAnimating::InvalidateBoneCache
	};

	const std::vector<Symbol> CBaseEntity_PostConstructorSym = { // Search for 'Setting CBaseEntity to non-brush model %s' then search for the vtable and it will be 2 functions bellow
		Symbol::FromName("_ZN11CBaseEntity15PostConstructorEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x18\x48\x85\xF6\x74\x05"), // 55 48 89 E5 53 48 89 FB 48 83 EC 18 48 85 F6 74 05
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x45\x08\x56\x8B\xF1\x85\xC0\x74\x06\x50"),
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x30\x48\x8B\xD9\x48\x85\xD2"), // 40 53 48 83 EC 30 48 8B D9 48 85 D2
	};

	const Symbol lua_rawsetiSym = Symbol::FromName("lua_rawseti");

	const Symbol lua_rawgetiSym = Symbol::FromName("lua_rawgeti");

	const Symbol lj_tab_newSym = Symbol::FromName("lj_tab_new");

	const Symbol lj_gc_barrierfSym = Symbol::FromName("lj_gc_barrierf");

	const Symbol lj_tab_getSym = Symbol::FromName("lj_tab_get");

	const Symbol lua_setfenvSym = Symbol::FromName("lua_setfenv");

	const Symbol lua_typeSym = Symbol::FromName("lua_type");

	const Symbol lua_gcSym = Symbol::FromName("lua_gc");

	const Symbol lua_setallocfSym = Symbol::FromName("lua_setallocf");

	const Symbol luaL_checklstringSym = Symbol::FromName("luaL_checklstring");

	const Symbol lua_callSym = Symbol::FromName("lua_call");

	const Symbol lua_pcallSym = Symbol::FromName("lua_pcall");

	const Symbol lua_pushvfstringSym = Symbol::FromName("lua_pushvfstring");

	const Symbol lua_errorSym = Symbol::FromName("lua_error");

	const std::vector<Symbol> CGetSym = { // 64x ToDo
		Symbol::FromName("get"),
	};

	const std::vector<Symbol> gEntListSym = { // Search for "Master was null or not a master!\n"
		Symbol::FromName("gEntList"),
		Symbol::FromSignature("\x48\x8D\x1D****\x49\x89\x45\x10\x4D\x8D\x65\x08\x45\x31\xFF"), // 48 8D 1D ?? ?? ?? ?? 49 89 45 10 4D 8D 65 08 45 31 FF
		Symbol::FromSignature("****\x6A\x00\x50\x6A\x00\xE8****\x8B\xF0\x85\xF6\x74\x23\x8B\x16"),
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x40\x48\x8B\xFA\x48\x85\xC9", 0x1C),
	};

	const std::vector<Symbol> svSym = { // Annoying stuff
		Symbol::FromName("sv"),
		Symbol::FromSignature("\x48\x8D\x05****\x0F\xB6\x80\x68\x01\x00\x00\x84\xC0\x75\x0B"), // 48 8D 05 ?? ?? ?? ?? 0F B6 80 68 01 00 00 84 C0 75 0B
		Symbol::FromSignature("\x56\x57\xB9****\xE8****\xB9****\xC7\x05********", 0x2),
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x57\x48\x83\xEC\x20\x48\x8D\x0D****\xE8****\x33\xED\x48\x8D\x05****\x48\x8D\x0D****\x48\x89\x05****", 0x14),
	};

	const std::vector<Symbol> CSteam3Server_NotifyClientDisconnectSym = { // 64x = Search for "Dropped %s (%llu) from server (%s)\n"
		Symbol::FromName("_ZN13CSteam3Server22NotifyClientDisconnectEP11CBaseClient"),
		Symbol::FromSignature("\x48\x85\xF6\x0F\x84\x2F\x01\x00\x00\x55\x48\x89\xE5\x41\x55\x4C\x8D\x2D****"), // 48 85 F6 0F 84 2F 01 00 00 55 48 89 E5 41 55 4C 8D 2D ?? ?? ?? ??
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x20\x56\x8B\x75\x08\x57\x8B\xF9\x85\xF6"),
		Symbol::FromSignature("\x48\x85\xD2\x0F\x84****\x48\x89\x5C\x24\x18\x57"), // 48 85 D2 0F 84 ?? ?? ?? ?? 48 89 5C 24 18 57
	};

	const Symbol SteamGameServer_ShutdownSym = Symbol::FromName("SteamGameServer_Shutdown"); // Same symbol for all versions.

	const Symbol GMOD_LoadBinaryModuleSym = Symbol::FromName("GMOD_LoadBinaryModule"); // Same symbol for all versions.

	const std::vector<Symbol> CM_VisSym = { // CM_Vis:
		Symbol::FromName("_Z6CM_VisPhiii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x83\xEC\x08\x48\x85\xFF\x74\x5A\x83\xF9\x02"), // 55 48 89 E5 53 48 83 EC 08 48 85 FF 74 5A 83 F9 02
		Symbol::FromSignature("\x55\x8B\xEC\x51\x8B\x4D\x14"),
		Symbol::FromSignature("\x40\x55\x48\x83\xEC\x30\x49\x63\xC1"),
	};

	const std::vector<Symbol> CBaseEntity_GetLuaEntitySym = {//Search for 'm_LuaEntity != ENTITY!'
		Symbol::FromName("_ZN11CBaseEntity12GetLuaEntityEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x83\xEC\x28\x4C\x8D\x25****"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 28 4C 8D 25 ?? ?? ?? ??
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x18\x83\x3D****\x00\x57\x8B\xF9\x74\x18"),
		Symbol::FromSignature("\x40\x57\x48\x83\xEC\x40\x48\x83\x3D****\x00"), // 40 57 48 83 EC 40 48 83 3D ?? ?? ?? ?? 00
	};

	const std::vector<Symbol> CGameEventManager_CreateEventSym = {// Search for "CreateEvent: event '%s' not registered."
		Symbol::FromName("_ZN17CGameEventManager11CreateEventEPKcb"),
		Symbol::FromSignature("\x55\x48\x85\xF6\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x53\x0F\x84\xAD\x00\x00\x00"), // 55 48 85 F6 48 89 E5 41 56 41 55 41 54 53 0F 84 AD 00 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x56\x8B\x75\x08\x57\x85\xF6\x74\x2A\x80\x3E\x00"),
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x57\x48\x83\xEC\x20\x41\x0F\xB6\xE8"),
	};

	const std::vector<Symbol> Sys_Error_InternalSym = { // Engine error: %s\n
		Symbol::FromName("_Z18Sys_Error_InternalbPKcPc"),
		Symbol::FromSignature("\x55\x48\x89\xD1\x48\x89\xF2\xBE\x00\x04\x00\x00\x48\x89\xE5\x41\x54"), // 55 48 89 D1 48 89 F2 BE 00 04 00 00 48 89 E5 41 54
		Symbol::FromSignature("\x55\x8B\xEC**\x68****\x68****\x64*****\x50\x64"), // 55 8B EC ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 64 ?? ?? ?? ?? ?? 50 64
		Symbol::FromSignature("\x40\x53\x48\x81\xEC\x20\x04\x00\x00\x0F\xB6\xD9\x4D\x8B\xC8"), // 40 53 48 81 EC 20 04 00 00 0F B6 D9 4D 8B C8
	};

	//---------------------------------------------------------------------------------
	// Purpose: holylib Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> GetGModServerTagsSym = { // "gmws:"
		Symbol::FromName("_Z17GetGModServerTagsPcjb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x48\x8D\x85\xB0\xFE\xFF\xFF\x41\x56\x41\x55\x41\x54"), // 55 48 89 E5 41 57 48 8D 85 B0 FE FF FF 41 56 41 55 41 54
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x80\x00\x00\x00\x8B\x0D\x2A\x2A\x2A\x2A"), // 55 8B EC 81 EC 80 00 00 00 8B 0D ? ? ? ?
		Symbol::FromSignature("\x44\x88\x44\x24\x18\x48\x89\x54\x24\x10\x48\x89\x4C\x24\x08"), // 44 88 44 24 18 48 89 54 24 10 48 89 4C 24 08
	};

	const std::vector<Symbol> CFuncLadder_PlayerGotOnSym = { // "reserved_spot" (big one?) -> Find function -> Find Caller
		Symbol::FromName("_ZN11CFuncLadder11PlayerGotOnEP11CBasePlayer"),
		Symbol::FromSignature("\x55\x48\x89\xF2\x66\x0F\xEF\xC0\x48\x89\xE5\x41\x57\x41\x56\x41\x55"), // 55 48 89 F2 66 0F EF C0 48 89 E5 41 57 41 56 41 55
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x40\x48\x8B\xD9\x48\x8B\xFA\x4C\x8B\xC2"), // 48 89 5C 24 08 57 48 83 EC 40 48 8B D9 48 8B FA 4C 8B C2
	};

	const std::vector<Symbol> CFuncLadder_PlayerGotOffSym = { // "reserved_spot" (big one?) -> Find function -> Find Caller
		Symbol::FromName("_ZN11CFuncLadder12PlayerGotOffEP11CBasePlayer"),
		Symbol::FromSignature("\x48\x8D\x87\xF8\x15\x00\x00\x48\x89\xF2\x48\x89\xFE\x66\x0F\xEF\xC0"), // 48 8D 87 F8 15 00 00 48 89 F2 48 89 FE 66 0F EF C0
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x8B\xC1\x4C\x8B\xC2\x48\x8B\xD0\x48\x81\xC1\xB0\x15\x00\x00"), // 48 8B C1 4C 8B C2 48 8B D0 48 81 C1 B0 15 00 00
	};

	const std::vector<Symbol> CHL2_Player_ExitLadderSym = {
		Symbol::FromName("_ZN11CHL2_Player10ExitLadderEv"),
		Symbol::FromSignature("\x80\xBF\xFE\x01\x00\x00\x09\x74\x07\xC3\x66\x0F\x1F\x44\x00\x00"), // 80 BF FE 01 00 00 09 74 07 C3 66 0F 1F 44 00 00
		NULL_SIGNATURE,
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x20\x80\xB9\xD6\x01\x00\x00\x09\x48\x8B\xD9"), // 40 53 48 83 EC 20 80 B9 D6 01 00 00 09 48 8B D9
	};

	const std::vector<Symbol> CBaseEntity_SetMoveTypeSym = {// Search for %s[%i]: Changing collision rules within then you have CBaseEntity__CollisionRulesChanged and the 2nd xref it's this one
		Symbol::FromName("_ZN11CBaseEntity11SetMoveTypeE10MoveType_t13MoveCollide_t"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x89\xD5\x41\x54\x53\x48\x89\xFB"), // 55 48 89 E5 41 56 41 55 41 89 D5 41 54 53 48 89 FB
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x89\x5C\x24\x10\x48\x89\x6C\x24\x18\x48\x89\x74\x24\x20\x57\x48\x83\xEC\x20\x0F\xB6\x81\xD6\x01\x00\x00"), // 48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 48 83 EC 20 0F B6 81 D6 01 00 00
	};

	const std::vector<Symbol> HostState_ChangeLevelMPSym = { // Search for "changelevel" & see the function it calls at the end
		Symbol::FromName("_Z23HostState_ChangeLevelMPPKcS0_"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x49\x89\xFC\x53\x48\x89\xF3\xE8****\x48\x89\xC7"), // 55 48 89 E5 41 54 49 89 FC 53 48 89 F3 E8 ?? ?? ?? ?? 48 89 C7
		Symbol::FromSignature("\x55\x8B\xEC*****\x8B\xC8*****\x68\x00\x01\x00\x00\xFF\x75\x08"), // 55 8B EC ?? ?? ?? ?? ?? 8B C8 ?? ?? ?? ?? ?? 68 00 01 00 00 FF 75 08
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x8B\xFA\x48\x8B\xD9*****\x48\x8B\xC8"), // 48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 ?? ?? ?? ?? ?? 48 8B C8
	};

	const std::vector<Symbol> CLuaInterface_RunStringExSym = { // @%s%s
		Symbol::FromName("_ZN13CLuaInterface11RunStringExEPKcS1_S1_bbbb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xF4\x53\x48\x89\xFB\x48\x81\xEC\x78\x01\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 F4 53 48 89 FB 48 81 EC 78 01 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x55\x10\x81\xEC\x4C\x01\x00\x00"), // 55 8B EC 8B 55 10 81 EC 4C 01 00 00
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x48\x89\x7C\x24\x18\x55\x41\x56"), // 48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 55 41 56
	};

	//---------------------------------------------------------------------------------
	// Purpose: gameevent Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CBaseClient_ProcessListenEventsSym = { // ProcessListenEvents
		Symbol::FromName("_ZN11CBaseClient19ProcessListenEventsEP16CLC_ListenEvents"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xFF\x41\x56\x41\x55\x41\xBD\x01\x00\x00\x00"), // 55 48 89 E5 41 57 49 89 FF 41 56 41 55 41 BD 01 00 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x57\x8D\x79\xF8\x8B*****\x57\x8B\x01"), // (ToDo: Verify!) 55 8B EC 53 56 57 8D 79 F8 8B ?? ?? ?? ?? ?? 57 8B 01
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x48\x8D\x79\xF0"), // 48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8D 79 F0
	};

	const std::vector<Symbol> CGameEventManager_AddListenerSym = { // Fk this. No 64x
		Symbol::FromName("_ZN17CGameEventManager11AddListenerEPvP20CGameEventDescriptori"),
		Symbol::FromSignature("\x48\x85\xF6\x74\x1B\x48\x85\xD2\x74\x16\x55\x48\x89\xE5\xE8****\xB8\x01\x00\x00\x00"), // 48 85 F6 74 1B 48 85 D2 74 16 55 48 89 E5 E8 ?? ?? ?? ?? B8 01 00 00 00
	};

	//---------------------------------------------------------------------------------
	// Purpose: serverplugin Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CPlugin_LoadSym = {
		Symbol::FromName("_ZN7CPlugin4LoadEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xF4\xBE\x2E\x00\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 F4 BE 2E 00 00 00
		// No windows since windows doesn't have/support serverplugins at all
	};

	//---------------------------------------------------------------------------------
	// Purpose: sourcetv Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CHLTVClient_ProcessGMod_ClientToServerSym = {
		Symbol::FromName("_ZN11CHLTVClient26ProcessGMod_ClientToServerEP23CLC_GMod_ClientToServer"),
		Symbol::FromSignature("\xB8\x01\x00\x00\x00\xC3\x66\x2E\x0F\x1F\x84\x00\x00\x00\x00\x00\x48\x8B\x07"), // B8 01 00 00 00 C3 66 2E 0F 1F 84 00 00 00 00 00 48 8B 07
	};

	const std::vector<Symbol> CHLTVServer_CHLTVServerSym = {
		Symbol::FromName("_ZN11CHLTVServerC2Ev"),
		Symbol::FromSignature("\x55\x48\x8D\x05****\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53"), // 55 48 8D 05 ?? ?? ?? ?? 48 89 E5 41 57 41 56 41 55 41 54 53
	};

	const std::vector<Symbol> CHLTVServer_DestroyCHLTVServerSym = {
		Symbol::FromName("_ZN11CHLTVServerD2Ev"),
		Symbol::FromSignature("\x55\x48\x8D\x05****\xF3\x0F\x7E\x05****\x48\x89\xE5\x41\x56\x66\x48\x0F\x6E\xC8"), // 55 48 8D 05 ?? ?? ?? ?? F3 0F 7E 05 ?? ?? ?? ?? 48 89 E5 41 56 66 48 0F 6E C8
	};

	const std::vector<Symbol> COM_IsValidPathSym = {
		Symbol::FromName("_Z15COM_IsValidPathPKc"),
		Symbol::FromSignature("\x55\x48\x85\xFF\x48\x89\xE5\x41\x54\x53\x74\x44\x0F\xB6\x07\x48\x89\xFB\x3C\x2F\x0F\x94\xC2\x3C\x5C"), // 55 48 85 FF 48 89 E5 41 54 53 74 44 0F B6 07 48 89 FB 3C 2F 0F 94 C2 3C 5C | not unique, the first match is the target
	};

	const std::vector<Symbol> CHLTVDemoRecorder_StartRecordingSym = {
		Symbol::FromName("_ZN17CHLTVDemoRecorder14StartRecordingEPKcb"),
		Symbol::FromSignature("\x55\x48\x8D\x15****\x48\x89\xE5\x41\x55\x41\x54\x49\x89\xF4\x53\x48\x89\xFB\x48\x81\xEC\x18\x01\x00\x00"), // 55 48 8D 15 ?? ?? ?? ?? 48 89 E5 41 55 41 54 49 89 F4 53 48 89 FB 48 81 EC 18 01 00 00
	};

	const std::vector<Symbol> CHLTVDemoRecorder_StopRecordingSym = {
		Symbol::FromName("_ZN17CHLTVDemoRecorder13StopRecordingEv"),
		Symbol::FromSignature("\x80\xBF\x50\x05\x00\x00\x00\x74\x07\xE9****\x66\x90\xC3\x90\x66\x66\x2E\x0F\x1F\x84\x00\x00\x00\x00\x00"), // 80 BF 50 05 00 00 00 74 07 E9 ?? ?? ?? ?? 66 90 C3 90 66 66 2E 0F 1F 84 00 00 00 00 00
	};

	const std::vector<Symbol> CHLTVClient_ExecuteStringCommandSym = {
		Symbol::FromName("_ZN11CHLTVClient20ExecuteStringCommandEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x89\xF3\x48\x81\xEC\x48\x07\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 FC 53 48 89 F3 48 81 EC 48 07 00 00
	};

	const std::vector<Symbol> CHLTVClient_DeconstructorSym = {
		Symbol::FromName("_ZN11CHLTVClientD1Ev"),
		Symbol::FromSignature("\xF3\x0F\x7E\x05****\x48\x8D\x05****\x66\x48\x0F\x6E\xC8\x48\x05\x58\x01\x00\x00"), // F3 0F 7E 05 ?? ?? ?? ?? 48 8D 05 ?? ?? ?? ?? 66 48 0F 6E C8 48 05 58 01 00 00
	};

	const std::vector<Symbol> CHLTVDirector_StartNewShotSym = {
		Symbol::FromName("_ZN13CHLTVDirector12StartNewShotEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x83\xEC\x18\x4C\x8D\x2D****\xF2\x0F\x10\x05****"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 89 FB 48 83 EC 18 4C 8D 2D ?? ?? ?? ?? F2 0F 10 05 ?? ?? ?? ??
	};

	const std::vector<Symbol> CHLTVServer_BroadcastEventSym = {
		Symbol::FromName("_ZN11CHLTVServer14BroadcastEventEP10IGameEvent"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x4C\x8D\xAD\xB8\xFB\xFF\xFF\x41\x54"), // 55 48 89 E5 41 56 41 55 4C 8D AD B8 FB FF FF 41 54
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
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x49\x89\xF5\x41\x54\x44\x0F\xB6\xE2\x4C\x89\xEF"), // 55 48 89 E5 41 55 49 89 F5 41 54 44 0F B6 E2 4C 89 EF
		Symbol::FromSignature("\x55\x8B\xEC\x56\x8B\x75\x08\x57\x8B\x7D\x0C\x57\x56"), // 55 8B EC 56 8B 75 08 57 8B 7D 0C 57 56
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x8B\xDA\x41\x0F\xB6\xF8"), // 48 89 5c 24 08 57 48 83 EC 20 48 8B DA 41 0F B6 F8
	};

	const std::vector<Symbol> CVEngineServer_PrecacheGenericSym = {
		Symbol::FromName("_ZN14CVEngineServer15PrecacheGenericEPKcb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xF3\x0F\xB6\xF2\x48\x83\xEC\x08\x48\x89\xDF"), // 55 48 89 E5 53 48 89 F3 0F B6 F2 48 83 EC 08 48 89 DF
		Symbol::FromSignature("\x55\x8B\xEC\xFF\x75\x0C\xFF\x75\x08*****\x83\xC4\x08\x85\xC0\x79\x12"), // 55 8B EC FF 75 0C FF 75 08 ?? ?? ?? ?? ?? 83 C4 08 85 C0 79 12
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x20\x48\x8B\xDA\x41\x0F\xB6\xD0\x48\x8B\xCB"), // 40 53 48 83 EC 20 48 8B DA 41 0F B6 D0 48 8B CB
	};

	const std::vector<Symbol> SV_FindOrAddModelSym = {
		Symbol::FromName("_Z17SV_FindOrAddModelPKcb"),
		Symbol::FromSignature("\x40\x80\xFE\x01\x48\x89\xFE\x48\x8D\x3D****\x19\xD2\x31\xC9\x83\xE2\xFE"), // 40 80 FE 01 48 89 FE 48 8D 3D ?? ?? ?? ?? 19 D2 31 C9 83 E2 FE
		Symbol::FromSignature("\x55\x8B\xEC\x0F\xB6\x45\x0C*****\x6A\x00"), // 55 8B EC 0F B6 45 0C ?? ?? ?? ?? ?? 6A 00
		Symbol::FromSignature("\x44\x0F\xB6\xC2\x45\x33\xC9\x48\x8B\xD1"), // 44 0f B6 C2 45 33 C9 48 8B D1
	};

	const std::vector<Symbol> SV_FindOrAddGenericSym = {
		Symbol::FromName("_Z19SV_FindOrAddGenericPKcb"),
		Symbol::FromSignature("\x40\x80\xFE\x01\x48\x89\xFE\x48\x8D\x3D****\x19\xD2\x83\xE2\xFE\x83\xC2\x03\xE9****\x90\x0F\x1F\x40\x00\x55\x48\x89\xE5\x41\x54\x53\x48\x63\xDE\x85\xDB\x78\x22"), // 40 80 FE 01 48 89 FE 48 8D 3D ?? ?? ?? ?? 19 D2 83 E2 FE 83 C2 03 E9 ?? ?? ?? ?? 90 0F 1F 40 00 55 48 89 E5 41 54 53 48 63 DE 85 DB 78 22
		Symbol::FromSignature("\x55\x8B\xEC\x51\x8B\x0D****\x85\xC9\x75\x07\x83\xC8\xFF\x8B\xE5\x5D\xC3\x8B\x01\x56\x57\x8B\x7D\x08\x6A\x00\x6A\xFF\x57\x6A\x01\xFF\x50\x20\x8B\xF0\x81\xFE\xFF\xFF\x00\x00\x74\x08\x81\xFE\x00\x02\x00\x00\x72\x09\x5F\x83\xC8\xFF\x5E\x8B\xE5\x5D\xC3\x57\xE8****\x8B\xC8\xE8****\x8B\x0D****\x6A\x00\x56\x8B\x01\xFF\x50\x2C\x8A\x55\x0C\x02\xD2\xFE\xC2\x85\xC0\x75\x0A\x8A\x4D\xFF\x80\xE1\xFC\x0A\xCA\xEB\x09\x8A\x00\x8A\xCA\x80\xE1\x03\x0A\xC8\x88\x4D\xFF\x8D\x45\xFF\x8B\x0D****\x50\x6A\x01\x56\x8B\x11\xFF\x52\x28\x57\x8D\x0C\xF5****\xE8****\x5F\x8B\xC6\x5E\x8B\xE5\x5D\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\x55\x8B\xEC\x0F\xB6\x45\x0C"),
		Symbol::FromSignature("\x48\x89\x5C\x24\x20\x55\x48\x83\xEC\x30\x48\x8B\xE9\x0F\xB6\xDA\x48\x8B\x0D****\x48\x85\xC9\x75\x0E\x8D\x41\xFF\x48\x8B\x5C\x24\x58\x48\x83\xC4\x30\x5D\xC3\x48\x8B\x01\x41\xB9\xFF\xFF\xFF\xFF\x4C\x8B\xC5\x48\x89\x74\x24\x40\xB2\x01\x48\xC7\x44\x24\x20\x00\x00\x00\x00\xFF\x50\x40\x48\x63\xF0\x81\xFE\xFF\xFF\x00\x00\x75\x15\x48\x8B\x74\x24\x40\xB8\xFF\xFF\xFF\xFF\x48\x8B\x5C\x24\x58\x48\x83\xC4\x30\x5D\xC3\x48\x89\x7C\x24\x50\x48\x8B\xFE\x48\x81\xFE\x00\x02\x00\x00\x72\x07\xB8\xFF\xFF\xFF\xFF\xEB\x77\xE8****\x48\x8B\xC8\x48\x8B\xD5\xE8****\x48\x8B\x0D****\x45\x33\xC0\x8B\xD6\x48\x8B\x01\xFF\x50\x58\x02\xDB\x48\x8B\xC8\xFE\xC3\x48\x85\xC0\x75\x0B\x0F\xB6\x44\x24\x48\x24\xFC\x0A\xC3\xEB\x07\x0F\xB6\xC3\x24\x03\x0A\x01\x48\x8B\x0D****\x4C\x8D\x4C\x24\x48\x88\x44\x24\x48\x41\xB8\x01\x00\x00\x00\x8B\xD6\x4C\x8B\x11\x41\xFF\x52\x50\x48\xC1\xE7\x04\x48\x8D\x0D****\x48\x03\xCF\x48\x8B\xD5\xE8****\x8B\xC6\x48\x8B\x7C\x24\x50\x48\x8B\x74\x24\x40\x48\x8B\x5C\x24\x58\x48\x83\xC4\x30\x5D\xC3\xCC\xCC\xCC\x44\x0F\xB6\xC2"),
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
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x08\xE8****\x48\x89\xDF\x48\x8B\x5D\xF8\xBE\x60\x00\x00\x00"), // 55 48 89 E5 53 48 89 FB 48 83 EC 08 E8 ?? ?? ?? ?? 48 89 DF 48 8B 5D F8 BE 60 00 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x56\x8B\xF1\xFF\x76\x08"),
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x45\x33\xD2\x48\x8D\x05****"), // 48 89 5C 24 08 57 48 83 EC 20 45 33 D2 48 8D 05 ?? ?? ?? ??
		// Optional though not really required, tho it still would be good to have tbh as it acts as safety against invalid pointers for lua userdata
	};

	//---------------------------------------------------------------------------------
	// Purpose: surffix Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CGameMovement_TryPlayerMoveSym = { // Find 'Player.Swim' then find the CGameMovement__CheckJumpButton and find the xref and go 3 bellow
		Symbol::FromName("_ZN13CGameMovement13TryPlayerMoveEP6VectorP10CGameTrace"),
		Symbol::FromSignature("\x55\x66\x0F\xEF\xC9\x48\x89\xE5\x41\x57\x41\x56\x45\x31\xF6\x41\x55"), // 55 66 0F EF C9 48 89 E5 41 57 41 56 45 31 F6 41 55
		NULL_SIGNATURE,
		Symbol::FromSignature("\x4C\x8B\xDC\x49\x89\x5B\x10\x55\x56\x57\x41\x54\x41\x55"), // 4C 8B DC 49 89 5B 10 55 56 57 41 54 41 55
	};

	const std::vector<Symbol> CGameMovement_ClipVelocitySym = { // Find it into CGameMovement_TryPlayerMoveSym
		Symbol::FromName("_ZN13CGameMovement12ClipVelocityER6VectorS1_S1_f"),
		Symbol::FromSignature("\x48\x89\xD0\x0F\x28\xD0\x66\x0F\xEF\xC0\x48\x89\xCA\xF3\x0F\x10\x48\x08"), // 48 89 D0 0F 28 D0 66 0F EF C0 48 89 CA F3 0F 10 48 08
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x83\xEC\x48\xF3\x41\x0F\x10\x58\x08\x0F\x57\xED\x0F\x2F\xDD"), // 48 83 EC 48 F3 41 0F 10 58 08 0F 57 ED 0F 2F DD
	};

	const std::vector<Symbol> CTraceFilterSimple_ShouldHitEntitySym = { // Find '%3.2f: NextBotGroundLocomotion::GetBot()->OnLandOnGround' to get NextBotGroundLocomotion__OnLandOnGround then it should be above
		Symbol::FromName("_ZN18CTraceFilterSimple15ShouldHitEntityEP13IHandleEntityi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x49\x89\xFD\x41\x54\x41\x89\xD4"), // 55 48 89 E5 41 56 41 55 49 89 FD 41 54 41 89 D4
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x48\x89\x7C\x24\x20\x41\x56\x48\x83\xEC\x20\x48\x8B\xE9\x41\x8B\xF8"), // 48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20 48 8B E9 41 8B F8
	};

	const std::vector<Symbol> MoveHelperServerSym = {// Find 'CBasePlayer::PhysicsSimulate' and then you'll get CBasePlayer__PhysicsSimulate and then it's into it
		Symbol::FromName("_Z16MoveHelperServerv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x53\x0F\xB6\x05****\x48\x8D\x1D****\x84\xC0\x74\x0F\x48\x89\xD8\x5B\x41\x5C\x5D\xC3\x0F\x1F\x80\x00\x00\x00\x00\x4C\x8D\x25****\x48\x8D\x1D****\x4C\x89\xE7\xE8****\x85\xC0\x74\xD7\x48\x8D\x05****"), // 55 48 89 E5 41 54 53 0F B6 05 ?? ?? ?? ?? 48 8D 1D ?? ?? ?? ?? 84 C0 74 0F 48 89 D8 5B 41 5C 5D C3 0F 1F 80 00 00 00 00 4C 8D 25 ?? ?? ?? ?? 48 8D 1D ?? ?? ?? ?? 4C 89 E7 E8 ?? ?? ?? ?? 85 C0 74 D7 48 8D 05 ?? ?? ?? ??
		Symbol::FromSignature("\xA1****\xA8\x01**\x83\xC8\x01\xC7*********\x68\x00\x30\x00\x00"), // A1 ?? ?? ?? ?? A8 01 ?? ?? 83 C8 01 C7 ?? ?? ?? ?? ?? ?? ?? ?? ?? 68 00 30 00 00
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x20\x65\x48\x8B\x04\x25****\x8B\x0D****\xBA\x04\x00\x00\x00\x48\x8B\x0C\xC8"), // 40 53 48 83 EC 20 65 48 8B 04 25 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? BA 04 00 00 00 48 8B 0C C8
	};

	//---------------------------------------------------------------------------------
	// Purpose: pvs Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CGMOD_Player_SetupVisibilitySym = {//Find 'predicted_' then search for CGMOD_Player::CreateViewModel and once you got it, find the function bellow it in the xref VTable and you got it
		Symbol::FromName("_ZN12CGMOD_Player15SetupVisibilityEP11CBaseEntityPhi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x89\xF3\x48\x83\xEC\x08\xE8****\x4C\x8D\x2D****"), // 55 48 89 E5 41 55 41 54 49 89 FC 53 48 89 F3 48 83 EC 08 E8 ?? ?? ?? ?? 4C 8D 2D ?? ?? ?? ??
		Symbol::FromSignature("\x55\x8B\xEC\x56\xFF\x75\x10\x8B\xF1\xFF\x75\x0C\xFF\x75\x08\xE8****\x8B\x0D****"),
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x8B\xDA\x48\x8B\xF9\xE8****\x48\x8B\x0D****"), // 48 89 5C 24 08 57 48 83 EC 20 48 8B DA 48 8B F9 E8 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ??
	};

	const std::vector<Symbol> CServerGameEnts_CheckTransmitSym = { // Find 'cl_updaterate' and you'll find CServerGameClients__ClientSettingsChanged then search for xref to get vtable and it will be above
		Symbol::FromName("_ZN15CServerGameEnts13CheckTransmitEP18CCheckTransmitInfoPKti"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xD7\x41\x56\x41\x55\x41\x54\x4C\x63\xE1\x53\x48\x83\xEC\x48"), // 55 48 89 E5 41 57 49 89 D7 41 56 41 55 41 54 4C 63 E1 53 48 83 EC 48
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x20\x8B\x0D****\x57\x8B\xB9\x0C\x10\x00\x00"),
		Symbol::FromSignature("\x4C\x89\x44\x24\x18\x55\x56\x57\x41\x56\x41\x57\x48\x83\xEC\x60"), // 4C 89 44 24 18 55 56 57 41 56 41 57 48 83 EC 60
	};

	//---------------------------------------------------------------------------------
	// Purpose: filesystem Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CBaseFileSystem_IsDirectorySym = { // Search for "Unable to rename %s to %s!\n" to get CBaseFileSystem::RenameFile then xref and get the vtable and 2 bellow
		Symbol::FromName("_ZN15CBaseFileSystem11IsDirectoryEPKcS1_"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x81\xEC\xB8\x05\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 FC 53 48 81 EC B8 05 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x90\x04\x00\x00"), // 55 8B EC 81 EC 90 04 00 00
		Symbol::FromSignature("\x40\x55\x53\x56\x57\x41\x57\x48\x8D\xAC\x24\x90\xFB\xFF\xFF"), // 40 55 53 56 57 41 57 48 8D AC 24 90 FB FF FF
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x9C\x04\x00\x00") // 55 8B EC 81 EC 9C 04 00 00
	};

	const std::vector<Symbol> CBaseFileSystem_FastFileTimeSym = {// Find 'CBaseFileSystem::GetPathTime' then you have CBaseFileSystem__GetPathTime and it's there
		Symbol::FromName("_ZN15CBaseFileSystem12FastFileTimeEPKNS_11CSearchPathEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x49\x89\xFD\x41\x54\x49\x89\xD4\x53\x48\x89\xF3\x48\x81\xEC\xD0\x02\x00\x00"), // 55 48 89 E5 41 56 41 55 49 89 FD 41 54 49 89 D4 53 48 89 F3 48 81 EC D0 02 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x40\x01\x00\x00\x53\x56"), // 55 8B EC 81 EC 40 01 00 00 53 56
		Symbol::FromSignature("\x40\x53\x56\x57\x48\x81\xEC\xA0\x01\x00\x00"), // 40 53 56 57 48 81 EC A0 01 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x4C\x01\x00\x00\x53\x56") // 55 8B EC 81 EC 4C 01 00 00 53 56
	};

	const std::vector<Symbol> CBaseFileSystem_FixUpPathSym = {//Find 'BASE_PATH'
		Symbol::FromName("_ZN15CBaseFileSystem9FixUpPathEPKcPci"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x89\xD3"), // 55 48 89 E5 41 56 41 55 41 54 49 89 FC 53 48 89 D3
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\xFF\x75\x10"), // 55 8B EC 53 56 FF 75 10
		Symbol::FromSignature("\x48\x89\x5C\x24\x18\x56\x48\x83\xEC\x30"), // 48 89 5C 24 18 56 48 83 EC 30
	};

	std::vector<Symbol> CBaseFileSystem_OpenForReadSym = { // [Failed]
		Symbol::FromName("_ZN15CBaseFileSystem11OpenForReadEPKcS1_jS1_PPc"),
		Symbol::FromSignature("\x55\x49\x89\xF2\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x4D\x89\xCD"), // 55 49 89 F2 48 89 E5 41 57 41 56 41 55 4D 89 CD
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\xB8\x03\x00\x00\x53\x56\x57"), // 55 8B EC 81 EC B8 03 00 00 53 56 57
		Symbol::FromSignature("\x48\x89\x5C\x24\x20\x55\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\xAC\x24\x70\xFC\xFF\xFF"), // 48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 70 FC FF FF
	};

	const std::vector<Symbol> CBaseFileSystem_GetFileTimeSym = {
		Symbol::FromName("_ZN15CBaseFileSystem11GetFileTimeEPKcS1_"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xD4\x53\x48\x89\xFB"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 D4 53 48 89 FB
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x70\x03\x00\x00\x53\x8B\xD9"), // 55 8B EC 81 EC 70 03 00 00 53 8B D9 - "CBaseFileSystem::GetFileTime"
		Symbol::FromSignature("\x4C\x8B\xDC\x55\x56"), // 4C 8B DC 55 56
	};

	const std::vector<Symbol> CBaseFileSystem_CSearchPath_GetDebugStringSym = {
		Symbol::FromName("_ZNK15CBaseFileSystem11CSearchPath14GetDebugStringEv"),
		Symbol::FromSignature("\x48\x8B\x47\x20\x48\x85\xC0\x74\x0F\x48\x8D\x78\x30\xE9****\x66\x0F\x1F\x44\x00\x00"), // 48 8B 47 20 48 85 C0 74 0F 48 8D 78 30 E9 ?? ?? ?? ?? 66 0F 1F 44 00 00
	};

	const std::vector<Symbol> CBaseFileSystem_HandleOpenRegularFileSym = {
		Symbol::FromName("_ZN15CBaseFileSystem21HandleOpenRegularFileER13CFileOpenInfob"),
		Symbol::FromSignature("\x55\x31\xD2\x48\x89\xE5\x41\x56\x41\x55\x4C\x8D\x6E\x48\x41\x54"), // 55 31 D2 48 89 E5 41 56 41 55 4C 8D 6E 48 41 54
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x0C\x53\x56\x8B\xF1\x57\x8B\x7D\x08"), // 55 8B EC 83 EC 0C 53 56 8B F1 57 8B 7D 08
	};

	const std::vector<Symbol> CBaseFileSystem_Trace_FOpenSym = {
		Symbol::FromName("_ZN15CBaseFileSystem11Trace_FOpenEPKcS1_jPx"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x4D\x89\xC7\x41\x56\x4C\x8D\xB5\xC0\xFE\xFF\xFF"), // 55 48 89 E5 41 57 4D 89 C7 41 56 4C 8D B5 C0 FE FF FF
	};

	const std::vector<Symbol> CBaseFileSystem_NewSearchPathSym = {
		Symbol::FromName("_ZN15CBaseFileSystem13NewSearchPathEj"),
		Symbol::FromSignature("\x55\x89\xF2\xB8\x01\x00\x00\x00\x48\x89\xE5\x41\x56\x83\xE2\x01"), // 55 89 F2 B8 01 00 00 00 48 89 E5 41 56 83 E2 01
	};

	const std::vector<Symbol> CBaseFileSystem_AddSearchPathInternalSym = {
		Symbol::FromName("_ZN15CBaseFileSystem21AddSearchPathInternalEPKcS1_jbb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x49\x89\xF6\xBE\x00\x00\x00\x80"), // 55 48 89 E5 41 57 41 56 49 89 F6 BE 00 00 00 80
	};

	const std::vector<Symbol> CBaseFileSystem_CreateDirHierarchySym = {
		Symbol::FromName("_ZN15CBaseFileSystem18CreateDirHierarchyEPKcS1_"),
		Symbol::FromSignature("\x55\xB9\x04\x01\x00\x00\x48\x89\xE5\x41\x55\x41\x54\x53\x48\x89\xFB"), // 55 B9 04 01 00 00 48 89 E5 41 55 41 54 53 48 89 FB
	};

	const std::vector<Symbol> CBaseFileSystem_OpenForWriteSym = {
		Symbol::FromName("_ZN15CBaseFileSystem12OpenForWriteEPKcS1_S1_"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x49\x89\xD6\x41\x55\x48\x8D\x95\x90\xFD\xFF\xFF"), // 55 48 89 E5 41 57 41 56 49 89 D6 41 55 48 8D 95 90 FD FF FF
	};

	const std::vector<Symbol> CBaseFileSystem_RenameFileSym = {
		Symbol::FromName("_ZN15CBaseFileSystem10RenameFileEPKcS1_S1_"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x81\xEC\x98\x06\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 89 FB 48 81 EC 98 06 00 00
	};

	const std::vector<Symbol> CBaseFileSystem_GetWritePathSym = {
		Symbol::FromName("_ZN15CBaseFileSystem12GetWritePathEPKcS1_"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x49\x89\xF5\x41\x54\x49\x89\xFC\x53\x48\x83\xEC\x18"), // 55 48 89 E5 41 55 49 89 F5 41 54 49 89 FC 53 48 83 EC 18
	};

	const std::vector<Symbol> CFileHandle_ConstructorSym = {
		Symbol::FromName("_ZN11CFileHandleC1EP15CBaseFileSystem"),
		Symbol::FromSignature("\x48\x8D\x05****\xC7\x47\x18\xFF\xFF\xFF\xFF\x48\x89\x07\xC7\x47\x78\x61\x48\x46\x43"), // 48 8D 05 ?? ?? ?? ?? C7 47 18 FF FF FF FF 48 89 07 C7 47 78 61 48 46 43
	};

	const std::vector<Symbol> Addon_FileSystem_GetFileEntrySym = {
		Symbol::FromName("_ZN5Addon10FileSystem12GetFileEntryEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x4C\x8D\x6D\xC0\x41\x54\x53\x48\x83\xEC\x38"), // 55 48 89 E5 41 57 41 56 41 55 4C 8D 6D C0 41 54 53 48 83 EC 38
	};

	const std::vector<Symbol> Addon_FileSystem_ResolveFileSym = {
		Symbol::FromName("_ZN5Addon10FileSystem11ResolveFileB5cxx11EPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x4C\x8D\x7D\xC0\x41\x55\x41\x54\x53"), // 55 48 89 E5 41 57 41 56 4C 8D 7D C0 41 55 41 54 53
	};

	const std::vector<Symbol> Addon_FileSystem_GetFileSizeSym = {
		Symbol::FromName("_ZN5Addon10FileSystem11GetFileSizeERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x89\xF3\x48\x83\xEC\x28"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 FC 53 48 89 F3 48 83 EC 28
	};

	const std::vector<Symbol> Addon_FileSystem_IsDirectorySym = {
		Symbol::FromName("_ZN5Addon10FileSystem11IsDirectoryENSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x89\xF3\x48\x83\xEC\x08\x4C\x8D\x2D****"), // 55 48 89 E5 41 55 41 54 49 89 FC 53 48 89 F3 48 83 EC 08 4C 8D 2D ?? ?? ?? ??
	};

	const std::vector<Symbol> Addon_FileHandle_SizeSym = {
		Symbol::FromName("_ZN5Addon10FileHandle4SizeEv"),
		Symbol::FromSignature("\x8B\x47\x08\xC3\x66\x66\x2E\x0F\x1F\x84\x00\x00\x00\x00\x00\x90"), // 8B 47 08 C3 66 66 2E 0F 1F 84 00 00 00 00 00 90
	};

	const std::vector<Symbol> CFileSystem_Stdio_FS_FindFirstFileSym = {
		Symbol::FromName("_ZN17CFileSystem_Stdio16FS_FindFirstFileEPKcP9FIND_DATA"),
		Symbol::FromSignature("\x55\xB9\x04\x01\x00\x00\x48\x89\xE5\x41\x54\x4C\x8D\xA5\xE0\xFE\xFF\xFF"), // 55 B9 04 01 00 00 48 89 E5 41 54 4C 8D A5 E0 FE FF FF
	};

	const std::vector<Symbol> CFileSystem_Stdio_FS_FindNextFileSym = {
		Symbol::FromName("_ZN17CFileSystem_Stdio15FS_FindNextFileEPvP9FIND_DATA"),
		Symbol::FromSignature("\x48\x89\xF7\x48\x89\xD6\x48\x83\xFF\xFF\x74\x0C\xE9****\x0F\x1F\x80\x00\x00\x00\x00"), // 48 89 F7 48 89 D6 48 83 FF FF 74 0C E9 ?? ?? ?? ?? 0F 1F 80 00 00 00 00
	};

	const std::vector<Symbol> CFileSystem_Stdio_FS_FindCloseSym = {
		Symbol::FromName("_ZN17CFileSystem_Stdio12FS_FindCloseEPv"),
		Symbol::FromSignature("\x48\x89\xF7\xE9****\x0F\x1F\x84\x00\x00\x00\x00\x00\x48\x8B\x7F\x08"), // 48 89 F7 E9 ?? ?? ?? ?? 0F 1F 84 00 00 00 00 00 48 8B 7F 08
	};

	const std::vector<Symbol> CFileSystem_Stdio_FS_statSym = {
		Symbol::FromName("_ZN17CFileSystem_Stdio7FS_statEPKcP4statPb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x49\x89\xD4\x53\x48\x81\xEC\x20\x02\x00\x00"), // 55 48 89 E5 41 56 41 55 41 54 49 89 D4 53 48 81 EC 20 02 00 00
	};

	const std::vector<Symbol> CPackedStore_DirectoryEntryExistsSym = {
		Symbol::FromName("_ZN12CPackedStore20DirectoryEntryExistsEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x81\xEC\x28\x01\x00\x00"), // 55 48 89 E5 41 55 41 54 49 89 FC 53 48 81 EC 28 01 00 00
	};

	const std::vector<Symbol> g_PathIDTableSym = {
		Symbol::FromName("g_PathIDTable"),
		Symbol::FromSignature("\x4C\x8D\x2D****\x4D\x8D\xB5\x80\x00\x00\x00\x4D\x8D\xBD\x98\x00\x00\x00"), // 4C 8D 2D ?? ?? ?? ?? 4D 8D B5 80 00 00 00 4D 8D BD 98 00 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x08\x01\x00\x00\x53\x56\x57", 0x4d), // 55 8B EC 81 EC 08 01 00 00 53 56 57
		Symbol::FromSignature("\x48\x89\x5C\x24\x10\x57\x48\x83\xEC\x20\xB8\xFF\xFF\x00\x00\x48\x8B\xD9", 0x67), // 48 89 5C 24 10 57 48 83 EC 20 B8 FF FF 00 00 48 8B D9
	};

	//---------------------------------------------------------------------------------
	// Purpose: concommand Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> ConCommand_IsBlockedSym = { // "crosshair_setup" && "hud_fastswitch" && "retry" in the same sub_
		Symbol::FromName("_Z20ConCommand_IsBlockedPKc"),
		Symbol::FromSignature("\x48\x8D\x05****\x55\x48\x89\xE5\x41\x54\x49\x89\xFC\x53\x48\x8B\x00"), // 48 8D 05 ?? ?? ?? ?? 55 48 89 E5 41 54 49 89 FC 53 48 8B 00
		Symbol::FromSignature("\x55\x8B\xEC\xA1****\x57\x8B\x7D\x08"), // 55 8B EC A1 ?? ?? ?? ?? 57 8B 7D 08
		Symbol::FromSignature("\x40\x57\x48\x83\xEC\x20\x48\x8B\x05****\x48\x8B\xF9\x83\x78\x1C\x01"), // 40 57 48 83 EC 20 48 8B 05 ?? ?? ?? ?? 48 8B F9 83 78 1C 01
	};

	//---------------------------------------------------------------------------------
	// Purpose: vprof Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CLuaGamemode_CallFinishSym = {//Search for "CLuaGamemode::CallFinish"
		Symbol::FromName("_ZN12CLuaGamemode10CallFinishEi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x4C\x8B\x25****\x53\x89\xF3\x45\x8B\xAC\x24\x0C\x10\x00\x00\x45\x85\xED\x0F\x85\x9B\x00\x00\x00\x48\x8D\x05****\x8D\x73\x02\x48\x8B\x38\x48\x8B\x07\xFF\x90\x10\x02\x00\x00"), // 55 48 89 E5 41 56 41 55 41 54 4C 8B 25 ?? ?? ?? ?? 53 89 F3 45 8B AC 24 0C 10 00 00 45 85 ED 0F 85 9B 00 00 00 48 8D 05 ?? ?? ?? ?? 8D 73 02 48 8B 38 48 8B 07 FF 90 10 02 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x0D****\x56\x8B\x75\x08\x83\xC6\x02\x83\xB9\x0C\x10\x00\x00\x00\x74\x3E\x53\x6A\x04\x6A\x00\x68****\x6A\x00\x68****\xFF\x15****\x8B\x0D****\x56\x8B\x01\x8B\x80\x08\x01\x00\x00"),
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x30\x48\x8B\x0D****\x8D\x5A\x02\x83\xB9****\x00\x74*\xC7\x44\x24\x28\x04\x00\x00\x00\x4C\x8D\x0D****\x45\x33\xC0\xC6\x44\x24\x20\x00\x48\x8D\x15****\xFF\x15****\x48\x8B\x0D****\x8B\xD3\x48\x8B\x01\xFF\x90\x10\x02\x00\x00"), // 40 53 48 83 EC 30 48 8B 0D ?? ?? ?? ?? 8D 5A 02 83 B9 ?? ?? ?? ?? 00 74 ?? C7 44 24 28 04 00 00 00 4C 8D 0D ?? ?? ?? ?? 45 33 C0 C6 44 24 20 00 48 8D 15 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? 8B D3 48 8B 01 FF 90 10 02 00 00
	};

	const std::vector<Symbol> CLuaGamemode_CallWithArgsSym = { // int version - Look at the difference in the call to [GM:CallWithArgs - !ThreadInMainThread]
		Symbol::FromName("_ZN12CLuaGamemode12CallWithArgsEi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x41\x89\xF4\x53\x48\x89\xFB\x48\x83\xEC\x18\x4C\x8B\x35****"), // 55 48 89 E5 41 57 41 56 41 55 41 54 41 89 F4 53 48 89 FB 48 83 EC 18 4C 8B 35 ?? ?? ?? ??
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x57\x8B\xF9\x8B\x0D\x2A\x2A\x2A\x2A\x8B\xB1\x0C\x10\x00\x00\x85\xF6\x74\x2A\x6A\x04\x6A\x00\x68\x2A\x2A\x2A\x2A\x6A\x00\x68\x2A\x2A\x2A\x2A\xFF\x15\x2A\x2A\x2A\x2A\xFF\x15\x2A\x2A\x2A\x2A\x84\xC0\x75\x2A\x8B\x0D\x2A\x2A\x2A\x2A\xFF\x75\x08\x8B\x01\xFF\x90\xEC\x01\x00\x00\x50\x68\x2A\x2A\x2A\x2A\xFF\x15\x2A\x2A\x2A\x2A\x8B\x07\x8B\xCF\x8B\x80\x8C\x00\x00\x00\xFF\xD0\x84\xC0\x74\x2A\x8B\x47\x3C\x8D\x4F\x3C\x8B\x80\x98\x00\x00\x00\xFF\xD0\x84\xC0\x75\x2A\x85\xF6\x74\x2A\x8B\x0D\x2A\x2A\x2A\x2A\xFF\x15\x2A\x2A\x2A\x2A\x5F\x5E\x32\xC0\x5B\x5D\xC2\x04\x00\x8B\x47\x3C\x8D\x4F\x3C\xFF\x90\x84\x00\x00\x00\x8B\x0D\x2A\x2A\x2A\x2A\xFF\x75\x08\x8B\x01\xFF\x90\xE8\x01\x00\x00\x8B\x07\x8B\xCF\xFF\x90\x84\x00\x00\x00\x85\xF6"), //55 8B EC 53 56 57 8B F9 8B 0D ? ? ? ? 8B B1 0C 10 00 00 85 F6 74 ? 6A 04 6A 00 68 ? ? ? ? 6A 00 68 ? ? ? ? FF 15 ? ? ? ? FF 15 ? ? ? ? 84 C0 75 ? 8B 0D ? ? ? ? FF 75 08 8B 01 FF 90 EC 01 00 00 50 68 ? ? ? ? FF 15 ? ? ? ? 8B 07 8B CF 8B 80 8C 00 00 00 FF D0 84 C0 74 ? 8B 47 3C 8D 4F 3C 8B 80 98 00 00 00 FF D0 84 C0 75 ? 85 F6 74 ? 8B 0D ? ? ? ? FF 15 ? ? ? ? 5F 5E 32 C0 5B 5D C2 04 00 8B 47 3C 8D 4F 3C FF 90 84 00 00 00 8B 0D ? ? ? ? FF 75 08 8B 01 FF 90 E8 01 00 00 8B 07 8B CF FF 90 84 00 00 00 85 F6
		Symbol::FromSignature("\x48\x89\x5C\x24\x10\x48\x89\x6C\x24\x18\x57\x48\x83\xEC\x30\x48\x8B\xF9\x8B\xEA\x48\x8B\x0D****\x8B\x99****\x85\xDB\x74*\xC7\x44\x24\x28\x04\x00\x00\x00\x4C\x8D\x0D****\x45\x33\xC0\xC6\x44\x24\x20\x00\x48\x8D\x15****\xFF\x15****\xFF\x15****\x84\xC0\x75*\x48\x8B\x0D****\x8B\xD5\x48\x8B\x01\xFF\x90\xD8\x03\x00\x00\x48\x8B\xD0\x48\x8D\x0D****\xFF\x15****\xCC\x48\x8B\x07\x48\x8B\xCF\x48\x89\x74\x24\x40\xFF\x90\x18\x01\x00\x00\x84\xC0\x74*\x48\x8B\x47\x48\x48\x8D\x4F\x48\xFF\x90\x30\x01\x00\x00\x84\xC0\x75*\x85\xDB\x74*\x48\x8B\x0D****\xFF\x15****\x32\xC0\xEB*\x48\x8B\x47\x48\x48\x8D\x4F\x48\xFF\x90\x08\x01\x00\x00\x48\x8B\x0D****\x8B\xD5\x48\x8B\x01\xFF\x90\xD0\x03\x00\x00\x48\x8B\x07\x48\x8B\xCF\xFF\x90\x08\x01\x00\x00\x85\xDB"), // 48 89 5C 24 10 48 89 6C 24 18 57 48 83 EC 30 48 8B F9 8B EA 48 8B 0D ?? ?? ?? ?? 8B 99 ?? ?? ?? ?? 85 DB 74 ?? C7 44 24 28 04 00 00 00 4C 8D 0D ?? ?? ?? ?? 45 33 C0 C6 44 24 20 00 48 8D 15 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 84 C0 75 ?? 48 8B 0D ?? ?? ?? ?? 8B D5 48 8B 01 FF 90 D8 03 00 00 48 8B D0 48 8D 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? CC 48 8B 07 48 8B CF 48 89 74 24 40 FF 90 18 01 00 00 84 C0 74 ?? 48 8B 47 48 48 8D 4F 48 FF 90 30 01 00 00 84 C0 75 ?? 85 DB 74 ?? 48 8B 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 32 C0 EB ?? 48 8B 47 48 48 8D 4F 48 FF 90 08 01 00 00 48 8B 0D ?? ?? ?? ?? 8B D5 48 8B 01 FF 90 D0 03 00 00 48 8B 07 48 8B CF FF 90 08 01 00 00 85 DB
	};

	const std::vector<Symbol> CLuaGamemode_CallWithArgsStrSym = { 
		Symbol::FromName("_ZN12CLuaGamemode12CallWithArgsEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xF4\x53\x48\x89\xFB\x48\x83\xEC\x18\x4C\x8B\x35****"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 F4 53 48 89 FB 48 83 EC 18 4C 8B 35 ?? ?? ?? ??
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x57\x8B\xF9\x8B\x0D\x2A\x2A\x2A\x2A\x8B\xB1\x0C\x10\x00\x00\x85\xF6\x74\x2A\x6A\x04\x6A\x00\x68\x2A\x2A\x2A\x2A\x6A\x00\x68\x2A\x2A\x2A\x2A\xFF\x15\x2A\x2A\x2A\x2A\xFF\x15\x2A\x2A\x2A\x2A\x84\xC0\x75\x2A\xFF\x75\x08\x68\x2A\x2A\x2A\x2A\xFF\x15\x2A\x2A\x2A\x2A\x8B\x07\x8B\xCF\x8B\x80\x8C\x00\x00\x00\xFF\xD0\x84\xC0\x74\x2A\x8B\x47\x3C\x8D\x4F\x3C\x8B\x80\x98\x00\x00\x00\xFF\xD0\x84\xC0\x75\x2A\x85\xF6\x74\x2A\x8B\x0D\x2A\x2A\x2A\x2A\xFF\x15\x2A\x2A\x2A\x2A\x5F\x5E\x32\xC0\x5B\x5D\xC2\x04\x00\x8B\x47\x3C\x8D\x4F\x3C\xFF\x90\x84\x00\x00\x00\x8B\x0D\x2A\x2A\x2A\x2A\x6A\x00\xFF\x75\x08\x8B\x01\xFF\x50\x74\x8B\x07\x8B\xCF\xFF\x90\x84\x00\x00\x00\x85\xF6"), //55 8B EC 53 56 57 8B F9 8B 0D ? ? ? ? 8B B1 0C 10 00 00 85 F6 74 ? 6A 04 6A 00 68 ? ? ? ? 6A 00 68 ? ? ? ? FF 15 ? ? ? ? FF 15 ? ? ? ? 84 C0 75 ? FF 75 08 68 ? ? ? ? FF 15 ? ? ? ? 8B 07 8B CF 8B 80 8C 00 00 00 FF D0 84 C0 74 ? 8B 47 3C 8D 4F 3C 8B 80 98 00 00 00 FF D0 84 C0 75 ? 85 F6 74 ? 8B 0D ? ? ? ? FF 15 ? ? ? ? 5F 5E 32 C0 5B 5D C2 04 00 8B 47 3C 8D 4F 3C FF 90 84 00 00 00 8B 0D ? ? ? ? 6A 00 FF 75 08 8B 01 FF 50 74 8B 07 8B CF FF 90 84 00 00 00 85 F6
		Symbol::FromSignature("\x48\x89\x5C\x24\x10\x48\x89\x6C\x24\x18\x57\x48\x83\xEC\x30\x48\x8B\xF9\x48\x8B\xEA\x48\x8B\x0D****\x8B\x99****\x85\xDB\x74*\xC7\x44\x24\x28\x04\x00\x00\x00\x4C\x8D\x0D****\x45\x33\xC0\xC6\x44\x24\x20\x00\x48\x8D\x15****\xFF\x15****\xFF\x15****\x84\xC0\x75*\x48\x8B\xD5\x48\x8D\x0D****\xFF\x15****\xCC\x48\x8B\x07\x48\x8B\xCF\x48\x89\x74\x24\x40\xFF\x90\x18\x01\x00\x00\x84\xC0\x74*\x48\x8B\x47\x48\x48\x8D\x4F\x48\xFF\x90\x30\x01\x00\x00\x84\xC0\x75*\x85\xDB\x74*\x48\x8B\x0D****\xFF\x15****\x32\xC0\xEB*\x48\x8B\x47\x48\x48\x8D\x4F\x48\xFF\x90\x08\x01\x00\x00\x48\x8B\x0D****\x45\x33\xC0\x48\x8B\xD5\x48\x8B\x01\xFF\x90\xE8\x00\x00\x00\x48\x8B\x07\x48\x8B\xCF\xFF\x90\x08\x01\x00\x00\x85\xDB"), // 48 89 5C 24 10 48 89 6C 24 18 57 48 83 EC 30 48 8B F9 48 8B EA 48 8B 0D ?? ?? ?? ?? 8B 99 ?? ?? ?? ?? 85 DB 74 ?? C7 44 24 28 04 00 00 00 4C 8D 0D ?? ?? ?? ?? 45 33 C0 C6 44 24 20 00 48 8D 15 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 84 C0 75 ?? 48 8B D5 48 8D 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? CC 48 8B 07 48 8B CF 48 89 74 24 40 FF 90 18 01 00 00 84 C0 74 ?? 48 8B 47 48 48 8D 4F 48 FF 90 30 01 00 00 84 C0 75 ?? 85 DB 74 ?? 48 8B 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 32 C0 EB ?? 48 8B 47 48 48 8D 4F 48 FF 90 08 01 00 00 48 8B 0D ?? ?? ?? ?? 45 33 C0 48 8B D5 48 8B 01 FF 90 E8 00 00 00 48 8B 07 48 8B CF FF 90 08 01 00 00 85 DB
	};

	const std::vector<Symbol> CLuaGamemode_CallStrSym = { // const char* version - Look at the difference in the call to [GM:CallWithArgs - !ThreadInMainThread] Also search for "CLuaGamemode::" not CLuaGamemode::Call on 64x
		Symbol::FromName("_ZN12CLuaGamemode4CallEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xF4\x53\x48\x89\xFB\x48\x83\xEC\x08\x4C\x8B\x2D****"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 F4 53 48 89 FB 48 83 EC 08 4C 8B 2D ?? ?? ?? ??
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x57\x8B\xF9\x8B\x0D\x2A\x2A\x2A\x2A\x8B\xB1\x0C\x10\x00\x00\x85\xF6\x74\x2A\x6A\x04\x6A\x00\x68\x2A\x2A\x2A\x2A\x6A\x00\x68\x2A\x2A\x2A\x2A\xFF\x15\x2A\x2A\x2A\x2A\xFF\x15\x2A\x2A\x2A\x2A\x84\xC0\x75\x2A\xFF\x75\x08\x68\x2A\x2A\x2A\x2A\xFF\x15\x2A\x2A\x2A\x2A\x8B\x07\x8B\xCF\x8B\x80\x8C\x00\x00\x00\xFF\xD0\x84\xC0\x74\x2A\x8B\x47\x3C\x8D\x4F\x3C\x8B\x80\x98\x00\x00\x00\xFF\xD0\x84\xC0\x75\x2A\x85\xF6\x74\x2A\x8B\x0D\x2A\x2A\x2A\x2A\xFF\x15\x2A\x2A\x2A\x2A\x5F\x5E\x32\xC0\x5B\x5D\xC2\x04\x00\x8B\x47\x3C\x8D\x4F\x3C\xFF\x90\x84\x00\x00\x00\x8B\x0D\x2A\x2A\x2A\x2A\x6A\x00\xFF\x75\x08\x8B\x01\xFF\x50\x74\x8B\x07\x8B\xCF\xFF\x90\x84\x00\x00\x00\x8B\x0D\x2A\x2A\x2A\x2A"), //55 8B EC 53 56 57 8B F9 8B 0D ? ? ? ? 8B B1 0C 10 00 00 85 F6 74 ? 6A 04 6A 00 68 ? ? ? ? 6A 00 68 ? ? ? ? FF 15 ? ? ? ? FF 15 ? ? ? ? 84 C0 75 ? FF 75 08 68 ? ? ? ? FF 15 ? ? ? ? 8B 07 8B CF 8B 80 8C 00 00 00 FF D0 84 C0 74 ? 8B 47 3C 8D 4F 3C 8B 80 98 00 00 00 FF D0 84 C0 75 ? 85 F6 74 ? 8B 0D ? ? ? ? FF 15 ? ? ? ? 5F 5E 32 C0 5B 5D C2 04 00 8B 47 3C 8D 4F 3C FF 90 84 00 00 00 8B 0D ? ? ? ? 6A 00 FF 75 08 8B 01 FF 50 74 8B 07 8B CF FF 90 84 00 00 00 8B 0D ? ? ? ?
		Symbol::FromSignature("\x48\x89\x5C\x24\x10\x48\x89\x6C\x24\x18\x57\x48\x83\xEC\x30\x48\x8B\xF9\x48\x8B\xEA\x48\x8B\x0D****\x8B\x99****\x85\xDB\x74*\xC7\x44\x24\x28\x04\x00\x00\x00\x4C\x8D\x0D****\x45\x33\xC0\xC6\x44\x24\x20\x00\x48\x8D\x15****\xFF\x15****\xFF\x15****\x84\xC0\x75*\x48\x8B\xD5\x48\x8D\x0D****\xFF\x15****\xCC\x48\x8B\x07\x48\x8B\xCF\x48\x89\x74\x24\x40\xFF\x90\x18\x01\x00\x00\x84\xC0\x74*\x48\x8B\x47\x48\x48\x8D\x4F\x48\xFF\x90\x30\x01\x00\x00\x84\xC0\x75*\x85\xDB\x74*\x48\x8B\x0D****\xFF\x15****\x32\xC0\xEB*\x48\x8B\x47\x48\x48\x8D\x4F\x48\xFF\x90\x08\x01\x00\x00\x48\x8B\x0D****\x45\x33\xC0\x48\x8B\xD5\x48\x8B\x01\xFF\x90\xE8\x00\x00\x00\x48\x8B\x07\x48\x8B\xCF\xFF\x90\x08\x01\x00\x00\x48\x8B\x0D****"), // 48 89 5C 24 10 48 89 6C 24 18 57 48 83 EC 30 48 8B F9 48 8B EA 48 8B 0D ?? ?? ?? ?? 8B 99 ?? ?? ?? ?? 85 DB 74 ?? C7 44 24 28 04 00 00 00 4C 8D 0D ?? ?? ?? ?? 45 33 C0 C6 44 24 20 00 48 8D 15 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 84 C0 75 ?? 48 8B D5 48 8D 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? CC 48 8B 07 48 8B CF 48 89 74 24 40 FF 90 18 01 00 00 84 C0 74 ?? 48 8B 47 48 48 8D 4F 48 FF 90 30 01 00 00 84 C0 75 ?? 85 DB 74 ?? 48 8B 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 32 C0 EB ?? 48 8B 47 48 48 8D 4F 48 FF 90 08 01 00 00 48 8B 0D ?? ?? ?? ?? 45 33 C0 48 8B D5 48 8B 01 FF 90 E8 00 00 00 48 8B 07 48 8B CF FF 90 08 01 00 00 48 8B 0D ?? ?? ?? ??
	};

	const std::vector<Symbol> CLuaGamemode_CallSym = {
		Symbol::FromName("_ZN12CLuaGamemode4CallEi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x41\x89\xF4\x53\x48\x89\xFB\x48\x83\xEC\x08"), // 55 48 89 E5 41 57 41 56 41 55 41 54 41 89 F4 53 48 89 FB 48 83 EC 08
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x57\x8B\xF9\x8B\x0D\x2A\x2A\x2A\x2A\x8B\xB1\x0C\x10\x00\x00\x85\xF6\x74\x2A\x6A\x04\x6A\x00\x68\x2A\x2A\x2A\x2A\x6A\x00\x68\x2A\x2A\x2A\x2A\xFF\x15\x2A\x2A\x2A\x2A\xFF\x15\x2A\x2A\x2A\x2A\x84\xC0\x75\x2A\x8B\x0D\x2A\x2A\x2A\x2A\xFF\x75\x08\x8B\x01\xFF\x90\xEC\x01\x00\x00\x50\x68\x2A\x2A\x2A\x2A\xFF\x15\x2A\x2A\x2A\x2A\x8B\x07\x8B\xCF\x8B\x80\x8C\x00\x00\x00\xFF\xD0\x84\xC0\x74\x2A\x8B\x47\x3C\x8D\x4F\x3C\x8B\x80\x98\x00\x00\x00\xFF\xD0\x84\xC0\x75\x2A\x85\xF6\x74\x2A\x8B\x0D\x2A\x2A\x2A\x2A\xFF\x15\x2A\x2A\x2A\x2A\x5F\x5E\x32\xC0\x5B\x5D\xC2\x04\x00\x8B\x47\x3C\x8D\x4F\x3C\xFF\x90\x84\x00\x00\x00\x8B\x0D\x2A\x2A\x2A\x2A\xFF\x75\x08\x8B\x01\xFF\x90\xE8\x01\x00\x00\x8B\x07\x8B\xCF\xFF\x90\x84\x00\x00\x00\x8B\x0D\x2A\x2A\x2A\x2A"), //55 8B EC 53 56 57 8B F9 8B 0D ? ? ? ? 8B B1 0C 10 00 00 85 F6 74 ? 6A 04 6A 00 68 ? ? ? ? 6A 00 68 ? ? ? ? FF 15 ? ? ? ? FF 15 ? ? ? ? 84 C0 75 ? 8B 0D ? ? ? ? FF 75 08 8B 01 FF 90 EC 01 00 00 50 68 ? ? ? ? FF 15 ? ? ? ? 8B 07 8B CF 8B 80 8C 00 00 00 FF D0 84 C0 74 ? 8B 47 3C 8D 4F 3C 8B 80 98 00 00 00 FF D0 84 C0 75 ? 85 F6 74 ? 8B 0D ? ? ? ? FF 15 ? ? ? ? 5F 5E 32 C0 5B 5D C2 04 00 8B 47 3C 8D 4F 3C FF 90 84 00 00 00 8B 0D ? ? ? ? FF 75 08 8B 01 FF 90 E8 01 00 00 8B 07 8B CF FF 90 84 00 00 00 8B 0D ? ? ? ?
		Symbol::FromSignature("\x48\x89\x5C\x24\x10\x48\x89\x6C\x24\x18\x57\x48\x83\xEC\x30\x48\x8B\xF9\x8B\xEA\x48\x8B\x0D****\x8B\x99****\x85\xDB\x74*\xC7\x44\x24\x28\x04\x00\x00\x00\x4C\x8D\x0D****\x45\x33\xC0\xC6\x44\x24\x20\x00\x48\x8D\x15****\xFF\x15****\xFF\x15****\x84\xC0\x75*\x48\x8B\x0D****\x8B\xD5\x48\x8B\x01\xFF\x90\xD8\x03\x00\x00\x48\x8B\xD0\x48\x8D\x0D****\xFF\x15****\xCC\x48\x8B\x07\x48\x8B\xCF\x48\x89\x74\x24\x40\xFF\x90\x18\x01\x00\x00\x84\xC0\x74*\x48\x8B\x47\x48\x48\x8D\x4F\x48\xFF\x90\x30\x01\x00\x00\x84\xC0\x75*\x85\xDB\x74*\x48\x8B\x0D****\xFF\x15****\x32\xC0\xEB*\x48\x8B\x47\x48\x48\x8D\x4F\x48\xFF\x90\x08\x01\x00\x00\x48\x8B\x0D****\x8B\xD5\x48\x8B\x01\xFF\x90\xD0\x03\x00\x00\x48\x8B\x07\x48\x8B\xCF\xFF\x90\x08\x01\x00\x00\x48\x8B\x0D****"), // 48 89 5C 24 10 48 89 6C 24 18 57 48 83 EC 30 48 8B F9 8B EA 48 8B 0D ?? ?? ?? ?? 8B 99 ?? ?? ?? ?? 85 DB 74 ?? C7 44 24 28 04 00 00 00 4C 8D 0D ?? ?? ?? ?? 45 33 C0 C6 44 24 20 00 48 8D 15 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 84 C0 75 ?? 48 8B 0D ?? ?? ?? ?? 8B D5 48 8B 01 FF 90 D8 03 00 00 48 8B D0 48 8D 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? CC 48 8B 07 48 8B CF 48 89 74 24 40 FF 90 18 01 00 00 84 C0 74 ?? 48 8B 47 48 48 8D 4F 48 FF 90 30 01 00 00 84 C0 75 ?? 85 DB 74 ?? 48 8B 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 32 C0 EB ?? 48 8B 47 48 48 8D 4F 48 FF 90 08 01 00 00 48 8B 0D ?? ?? ?? ?? 8B D5 48 8B 01 FF 90 D0 03 00 00 48 8B 07 48 8B CF FF 90 08 01 00 00 48 8B 0D ?? ?? ?? ??
	};

	const std::vector<Symbol> CVProfile_OutputReportSym = {
		Symbol::FromName("_ZN9CVProfile12OutputReportEiPKci"),
		Symbol::FromName("_ZN9CVProfile12OutputReportEiPKci"),
		Symbol::FromName("?OutputReport@CVProfile@@QAEXHPBDH@Z"),
		Symbol::FromName("?OutputReport@CVProfile@@QEAAXHPEBDH@Z")
	};

	const std::vector<Symbol> CScriptedEntity_StartFunctionStrSym = { // const char* version - GetSoundInterests
		Symbol::FromName("_ZN15CScriptedEntity13StartFunctionEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x49\x89\xF4\x53\x48\x89\xFB\xE8****\x84\xC0\x74\x7B"), // 55 48 89 E5 41 56 41 55 41 54 49 89 F4 53 48 89 FB E8 ?? ?? ?? ?? 84 C0 74 7B
		Symbol::FromSignature("\x55\x8B\xEC\x56\x8B\xF1\xFF\x15\x2A\x2A\x2A\x2A\x84\xC0\x74\x2A\x80\x7E\x04\x00\x74\x2A\x8B\x4E\x0C\x8B\x01\xFF\x90\x90\x03\x00\x00\x8B\xC8\x8B\x10\xFF\x92\x84\x00\x00\x00\x8B\x0D\x2A\x2A\x2A\x2A\x6A\x00"), // 55 8B EC 56 8B F1 FF 15 ? ? ? ? 84 C0 74 ? 80 7E 04 00 74 ? 8B 4E 0C 8B 01 FF 90 90 03 00 00 8B C8 8B 10 FF 92 84 00 00 00 8B 0D ? ? ? ? 6A 00
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x8B\xFA\x48\x8B\xD9\xFF\x15\x2A\x2A\x2A\x2A"), // 48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 FF 15 ? ? ? ?
	};

	const std::vector<Symbol> CScriptedEntity_StartFunctionSym = { // int version - hasTraced
		Symbol::FromName("_ZN15CScriptedEntity13StartFunctionEi"), 
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x41\x89\xF4\x53\x48\x89\xFB\xE8****\x84\xC0"), // 55 48 89 E5 41 56 41 55 41 54 41 89 F4 53 48 89 FB E8 ?? ?? ?? ?? 84 C0
		Symbol::FromSignature("\x55\x8B\xEC\x56\x8B\xF1\xFF\x15\x2A\x2A\x2A\x2A\x84\xC0\x74\x2A\x80\x7E\x04\x00\x74\x2A\x8B\x4E\x0C\x8B\x01\xFF\x90\x90\x03\x00\x00\x8B\xC8\x8B\x10\xFF\x92\x84\x00\x00\x00\x8B\x0D\x2A\x2A\x2A\x2A\xFF\x75\x08"), // 55 8B EC 56 8B F1 FF 15 ? ? ? ? 84 C0 74 ? 80 7E 04 00 74 ? 8B 4E 0C 8B 01 FF 90 90 03 00 00 8B C8 8B 10 FF 92 84 00 00 00 8B 0D ? ? ? ? FF 75 08
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x8B\xFA\x48\x8B\xD9\xFF\x15\x2A\x2A\x2A\x2A"), // 48 89 5C 24 08 57 48 83 EC 20 8B FA 48 8B D9 FF 15 ? ? ? ?
	};

	const std::vector<Symbol> CScriptedEntity_CallSym = { // SENT:AcceptInput
		Symbol::FromName("_ZN15CScriptedEntity4CallEii"),
		Symbol::FromSignature("\x48\x8D\x05****\x83\xC6\x01\x85\xD2\x48\x8B\x38\x48\x8B\x07\x75\x0C"), // 48 8D 05 ?? ?? ?? ?? 83 C6 01 85 D2 48 8B 38 48 8B 07 75 0C
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x0D\x2A\x2A\x2A\x2A\x8B\x55\x0C"), // 55 8B EC 8B 0D ? ? ? ? 8B 55 0C
		Symbol::FromSignature("\x48\x83\xEC\x28\x48\x8B\x0D\x2A\x2A\x2A\x2A\xFF\xC2"), // 48 83 EC 28 48 8B 0D ? ? ? ? FF C2
	};

	const std::vector<Symbol> CScriptedEntity_CallFunctionStrSym = { // const char* version - SetupDataTables
		Symbol::FromName("_ZN15CScriptedEntity12CallFunctionEPKcb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x83\xEC\x28\x0F\xB6\x5F\x08"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 28 0F B6 5F 08
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x14\x56\x8B\xF1\x80\x7E\x04\x00\x75\x09"),
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x40\x80\x79\x08\x00\x41\x0F\xB6\xF0"), // 48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 40 80 79 08 00 41 0F B6 F0
	};

	const std::vector<Symbol> CScriptedEntity_CallFunctionSym = { // int version. - AutomaticFrameAdvance
		Symbol::FromName("_ZN15CScriptedEntity12CallFunctionEib"),
		Symbol::FromSignature("\x80\x7F\x08\x00\x0F\x84\xC6\x00\x00\x00\x55\x48\x89\xE5\x41\x56"), // 80 7F 08 00 0F 84 C6 00 00 00 55 48 89 E5 41 56
		Symbol::FromSignature("\x55\x8B\xEC\x56\x8B\xF1\x80\x7E\x04\x00\x74\x2A"), // 55 8B EC 56 8B F1 80 7E 04 00 74 ?
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x80\x79\x08\x00\x41\x0F\xB6\xF8"), // 48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 80 79 08 00 41 0F B6 F8
	};

	const std::vector<Symbol> lj_BC_FUNCCSym = {
		Symbol::FromSignature("\x8B\x6A\xF8\x8B\x7D\x14\x8B\x6C\x24\x30\x8D\x44\xC2\xF8\x89\x55\x10\x8D\x88\xA0\x00\x00\x00\x3B\x4D\x18\x89\x45\x14\x89\x2C\x24"), // 8B 6A F8 8B 7D 14 8B 6C 24 30 8D 44 C2 F8 89 55 10 8D 88 A0 00 00 00 3B 4D 18 89 45 14 89 2C 24
	};

	const std::vector<Symbol> CVProfNode_GetSubNodeSym = {
		Symbol::FromName("_ZN10CVProfNode10GetSubNodeEPKciS1_i"),
		Symbol::FromName("_ZN10CVProfNode10GetSubNodeEPKciS1_i"),
	};

	const std::vector<Symbol> CVProfile_FreeNodes_RSym = {
		Symbol::FromName("_ZN9CVProfile11FreeNodes_REP10CVProfNode"),
		Symbol::FromName("_ZN9CVProfile11FreeNodes_REP10CVProfNode"),
	};

	const std::vector<Symbol> CVProfNode_MarkFrameSym = {
		Symbol::FromName("_ZN10CVProfNode9MarkFrameEv"),
		Symbol::FromName("_ZN10CVProfNode9MarkFrameEv"),
	};

	const std::vector<Symbol> CVProfExport_CalculateBudgetGroupTimes_RecursiveSym = {
		Symbol::FromName("_ZN12CVProfExport35CalculateBudgetGroupTimes_RecursiveEP10CVProfNode"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x49\x89\xFE\x41\x55\x41\x54\x53\x48\x89\xF3\x48\x83\xEC\x18"), // 55 48 89 E5 41 57 41 56 49 89 FE 41 55 41 54 53 48 89 F3 48 83 EC 18
	};

	const std::vector<Symbol> VProfRecord_IsPlayingBackSym = {
		Symbol::FromName("_Z25VProfRecord_IsPlayingBackv"),
		Symbol::FromSignature("\x83\x3D****\x02\x0F\x94\xC0\xC3\x0F\x1F\x44\x00\x00\x8B\x05****\xC3\x66\x0F\x1F\x84\x00\x00\x00\x00\x00"), // 83 3D ?? ?? ?? ?? 02 0F 94 C0 C3 0F 1F 44 00 00 8B 05 ?? ?? ?? ?? C3 66 0F 1F 84 00 00 00 00 00
	};

	const std::vector<Symbol> CLuaInterface_CallFunctionProtectedSym = {
		Symbol::FromName("_ZN13CLuaInterface21CallFunctionProtectedEiib"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x89\xCE\x41\x55\x41\x89\xF5"), // 55 48 89 E5 41 57 41 56 41 89 CE 41 55 41 89 F5
	};

	//---------------------------------------------------------------------------------
	// Purpose: networking Symbols
	// NOTE: This is just optimizations, having CGMOD_Player_CreateViewModelSym on Windows would be good but all the other things are probably best for Linux only.
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> AllocChangeFrameListSym = { // I'm still surprised I managed to get this one :^
		Symbol::FromName("_Z20AllocChangeFrameListii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x89\xF5\x41\x54\x41\x89\xFC\xBF\x28\x00\x00\x00"), // 55 48 89 E5 41 55 41 89 F5 41 54 41 89 FC BF 28 00 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x57\x6A\x18\xE8****\x8B\xF0\x83\xC4\x04"),
		Symbol::FromSignature("\x48\x89\x74\x24\x18\x48\x89\x7C\x24\x20\x41\x56\x48\x83\xEC\x20\x48\x89\x6C\x24\x38"),
		//Search for "SV_PackEntity: SnagChangeFrameList mi"
	};

	const std::vector<Symbol> SendTable_CullPropsFromProxiesSym = { // CPropCullStack::AddProp - m_pOutProps overflowed
		Symbol::FromName("_Z30SendTable_CullPropsFromProxiesPK9SendTablePKiiiPK20CSendProxyRecipientsiS6_iPii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x4D\x89\xC6\x41\x55\x45\x89\xCD"), // 55 48 89 E5 41 57 41 56 4D 89 C6 41 55 45 89 CD
		Symbol::FromSignature("\x55\x8B\xEC\xB8\x4C\x41\x00\x00\xE8****\x8B\x0D****\x53\x56\x57"),
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x48\x89\x7C\x24\x18\x55\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\xAC\x24\x50\xBE\xFF\xFF"),
	};

	const std::vector<Symbol> SendTable_WritePropListSym = { // SendTable_WritePropList->inputBuffer
		Symbol::FromName("_Z23SendTable_WritePropListPK9SendTablePKviP8bf_writeiPKii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xF7\x41\x56\x41\x89\xD6\x41\x55\x41\x54"), // 55 48 89 E5 41 57 49 89 F7 41 56 41 89 D6 41 55 41 54
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x44\x8B\x0D****\x53\x56\x8B\xB1\x0C\x10\x00\x00"),
		Symbol::FromSignature("\x4C\x89\x4C\x24\x20\x48\x89\x4C\x24\x08\x55"),
	};

	const std::vector<Symbol> CBaseServer_WriteDeltaEntitiesSym = { // CBaseServer::WriteDeltaEntities
		Symbol::FromName("_ZN11CBaseServer18WriteDeltaEntitiesEP11CBaseClientP12CClientFrameS3_R8bf_write"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x4D\x89\xC7\x41\x56\x41\x55\x49\x89\xCD"), // 55 48 89 E5 41 57 4D 89 C7 41 56 41 55 49 89 CD
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x84\x04\x00\x00"),
		Symbol::FromSignature("\x4C\x89\x44\x24\x18\x48\x89\x54\x24\x10\x55\x53\x56\x57"),
	};

	const std::vector<Symbol> SendTable_CalcDeltaSym = {//Search for "SendTable_CalcDelta/toBits"
		Symbol::FromName("_Z19SendTable_CalcDeltaPK9SendTablePKviS3_iPiii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x89\xD7\x41\x56\x49\x89\xCE\x41\x55"), // 55 48 89 E5 41 57 41 89 D7 41 56 49 89 CE 41 55
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x4C\x80\x3D\x2A\x2A\x2A\x2A\x00"),
		Symbol::FromSignature("\x48\x8B\xC4\x55\x41\x56\x48\x8D\x68\xC1\x48\x81\xEC\xA8\x00\x00\x00"),
	};

	const std::vector<Symbol> SV_DetermineUpdateTypeSym = { // Search for xref of SendTable_CalcDelta
		Symbol::FromName("_ZL22SV_DetermineUpdateTypeR16CEntityWriteInfo"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xFF\x41\x56\x41\x55\x41\x54\x53\x48\x81\xEC\x98\x80\x00\x00"), // 55 48 89 E5 41 57 49 89 FF 41 56 41 55 41 54 53 48 81 EC 98 80 00 00
		Symbol::FromSignature("\x55\x8B\xEC\xB8\x0C\x40\x00\x00\xE8****\x8B\x0D****\x8B\x81\x0C\x10\x00\x00"),
		Symbol::FromSignature("\xB8\x58\x40\x00\x00\xE8****\x48\x2B\xE0\x48\x89\x9C\x24\x50\x40\x00\x00"),
	};

	const std::vector<Symbol> PackedEntity_GetPropsChangedAfterTickSym = { //Search into SV_DetermineUpdateTypeSym
		Symbol::FromName("_ZN12PackedEntity24GetPropsChangedAfterTickEiPii"),
		Symbol::FromSignature("\x48\x8B\x7F\x50\x48\x85\xFF\x74\x06\x48\x8B\x07\xFF\x60\x18\xB8\xFF\xFF\xFF\xFF"), // 48 8B 7F 50 48 85 FF 74 06 48 8B 07 FF 60 18 B8 FF FF FF FF
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x49\x30"),
		Symbol::FromSignature("\x48\x8B\x49\x50\x48\x85\xC9\x74\x2A\x48\x8B\x01\x48\xFF\x60\x18"),
	};

	const std::vector<Symbol> CGameServer_SendClientMessagesSym = { //Search for "SendClientMessages"
		Symbol::FromName("_ZN11CGameServer18SendClientMessagesEb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x41\x89\xF4\x53\x48\x89\xFB\x48\x81\xEC\x48\x08\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 41 89 F4 53 48 89 FB 48 81 EC 48 08 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x1C\x04\x00\x00\x53\x56\x57"),
		Symbol::FromSignature("\x4C\x8B\xDC\x48\x81\xEC\xA8\x08\x00\x00"),
	};

	const std::vector<Symbol> CBaseEntity_GMOD_SetShouldPreventTransmitToPlayerSym = { //Find CBaseEntity::GetLuaEntity with "m_LuaEntity != ENTITY!"
		Symbol::FromName("_ZN11CBaseEntity37GMOD_SetShouldPreventTransmitToPlayerEP11CBasePlayerb"),
		Symbol::FromSignature("\x55\x48\x8D\x87\xD8\x14\x00\x00\x48\x89\xE5\x41\x57\x41\x89\xD7"), // 55 48 8D 87 D8 14 00 00 48 89 E5 41 57 41 89 D7
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x8B\x75\x08\x57\x8B\xF9\x85\xF6\x74\x2A\x8B\x06\x8B\xCE\xFF\x50\x08\x8B\x00"),
		Symbol::FromSignature("\x40\x53\x56\x57\x41\x55\x41\x56\x48\x83\xEC\x20"),
	};

	const std::vector<Symbol> CBaseEntity_GMOD_ShouldPreventTransmitToPlayerSym = { //Find CBaseEntity::GetLuaEntity with "m_LuaEntity != ENTITY!"
		Symbol::FromName("_ZN11CBaseEntity34GMOD_ShouldPreventTransmitToPlayerEP11CBasePlayer"),
		Symbol::FromSignature("\x31\xC0\x66\x83\xBF\xEE\x14\x00\x00\x00\x75\x04\xC3\x0F\x1F\x00"), // 31 C0 66 83 BF EE 14 00 00 00 75 04 C3 0F 1F 00
		Symbol::FromSignature("\x55\x8B\xEC\x56\x8B\xF1\x66\x83\xBE\x3E\x13\x00\x00\x00"),
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x20\x66\x83\xB9\xBE\x14\x00\x00\x00"),
	};

	const std::vector<Symbol> g_FrameSnapshotManagerSym = {
		Symbol::FromName("framesnapshotmanager"),
		Symbol::FromSignature("\x4C\x8D\x0D****\x4C\x89\x45\xB8\x49\x8B\x39\x4C\x89\x4D\xC0"), // 4C 8D 0D ?? ?? ?? ?? 4C 89 45 B8 49 8B 39 4C 89 4D C0
		Symbol::FromSignature("\x2A\x2A\x2A\x2A\x83\xC4\x04\x8B\x01\xFF\x50\x04\x6A\x40"), // ?? ?? ?? ?? 83 C4 04 8B 01 FF 50 04 6A 40 || "framesnapshotmanager"
		Symbol::FromSignature("\x40\x55\x53\x56\x57\x41\x55\x41\x56\x48\x8D\xAC\x24\x58\xFD\xFF\xFF", 0x3F6)
	};

	const std::vector<Symbol> g_PropTypeFnsSym = {
		Symbol::FromName("g_PropTypeFns"),
		Symbol::FromSignature("\x48\x8D\x0D****\xFF\x54\xC1\x40\x41\x83\xC6\x01\x45\x39\xFE"), // 48 8D 0D ?? ?? ?? ?? FF 54 C1 40 41 83 C6 01 45 39 FE
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x4D\x08\x83\xEC\x28", 0x10B), //55 8B EC 8B 4D 08 83 EC 28
		Symbol::FromSignature("\x40\x55\x53\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\xAC\x24\x88\xFB\xFF\xFF", 0x733), //40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 88 FB FF FF
	};

	const std::vector<Symbol> g_BSPDataSym = { // Search CM_Vis 
		Symbol::FromName("g_BSPData"),
		Symbol::FromSignature("\x48\x8D\x05****\x48\x89\xE5\x53\x4C\x89\xC3\x48\x83\xEC\x48"), // 48 8D 05 ?? ?? ?? ?? 48 89 E5 53 4C 89 C3 48 83 EC 48
		Symbol::FromSignature("\x55\x8B\xEC\x51\x8B\x4D\x14", 0xB3), // 55 83 F9 02
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x30\x0F\xB6\x1D\x2A\x2A\x2A\x2A", 0xD), // Search for "gmod_uncache_test" and find CM_FreeMap
	};

	// Only used on Linux
	const std::vector<Symbol> PackWork_t_ProcessSym = {
		Symbol::FromName("_ZN10PackWork_t7ProcessERS_"),
		Symbol::FromSignature("\x48\x8B\x4F\x10\x48\x63\x07\x48\x8B\x77\x08\x48\x8B\x51\x08\x49\x89\xC0"), // 48 8B 4F 10 48 63 07 48 8B 77 08 48 8B 51 08 49 89 C0
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
		Symbol::FromSignature("\x0F\xB7\x05****\x66\x83\xF8\xFF\x74\x1B\x83\xC0\x01\x31\xD2\x66\x89\x05**\x35\x00"), // 0F B7 05 ?? ?? ?? ?? 66 83 F8 FF 74 1B 83 C0 01 31 D2 66 89 05 ?? ?? 35 00
		Symbol::FromSignature("\x66*****\xB9\xFF\xFF\x00\x00\x66\x3B\xC1**\x57\x33\xFF"), // 66 ?? ?? ?? ?? ?? B9 FF FF 00 00 66 3B C1 ?? ?? 57 33 FF
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x20\x0F\xB7\x05\x2A\x2A\x2A\x2A")
	};

	const std::vector<Symbol> PackEntities_NormalSym = { //Search for 'PackEntities_Normal: Invalid edict ind
		Symbol::FromName("_Z19PackEntities_NormaliPP11CGameClientP14CFrameSnapshot"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x49\x89\xD5\x41\x54\x53\x48\x81\xEC\x78\x00\x03\x00"), // 55 48 89 E5 41 57 41 56 41 55 49 89 D5 41 54 53 48 81 EC 78 00 03 00
		Symbol::FromSignature("\x55\x8B\xEC\xB8\x34\x80\x01\x00\xE8****\x8B\x0D****\x53\x56\x57"),
		Symbol::FromSignature("\x48\x89\x54\x24\x10\x55\x53\x56\x41\x54\x41\x55\x41\x56"),
	};

	const std::vector<Symbol> CGMOD_Player_CreateViewModelSym = { //Search for "CGMOD_Player::PostThink" to find CGMOD_Player__PostThink then xref and find vtable, go upward 12 times
		Symbol::FromName("_ZN12CGMOD_Player15CreateViewModelEi"),
		Symbol::FromSignature("\x55\xBA\x01\x00\x00\x00\x48\x89\xE5\x41\x54\x41\x89\xF4\x53\x48\x89\xFB"), // 55 BA 01 00 00 00 48 89 E5 41 54 41 89 F4 53 48 89 FB
		Symbol::FromSignature("\x55\x8B\xEC\x53\x8B\x5D\x08\x56\x6A\x01"),
		Symbol::FromSignature("\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x48\x63\xF2"),
	};

	const std::vector<Symbol> CBaseCombatCharacter_SetTransmitSym = {//Search for "models/manhack.mdl" and find CNPC_Manhack__Spawn, then vtable above
		Symbol::FromName("_ZN20CBaseCombatCharacter11SetTransmitEP18CCheckTransmitInfob"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x89\xD5\x41\x54\x49\x89\xF4\x53\x48\x89\xFB\x48\x83\xEC\x18\x48\x8B\x47\x40\x4C\x8B\xBE\x10\x20\x00\x00\x48\x85\xC0\x0F\x84****\x44\x0F\xBF\x70\x06\x41\x81\xFE\xFF\x1F\x00\x00\x44\x89\xF1\x0F\x87\x7D\x00\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 89 D5 41 54 49 89 F4 53 48 89 FB 48 83 EC 18 48 8B 47 40 4C 8B BE 10 20 00 00 48 85 C0 0F 84 ?? ?? ?? ?? 44 0F BF 70 06 41 81 FE FF 1F 00 00 44 89 F1 0F 87 7D 00 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x57\x8B\xF9\x8B\x47\x18\x85\xC0\x74\x2A\x0F\xBF\x48\x06\xEB\x2A\x33\xC9\x8B\x5D\x08\x8B\xF1\xC1\xEE\x05\x83\xE1\x1F\xB8\x01\x00\x00\x00\xD3\xE0\x8B\x93\x08\x20\x00\x00\x85\x04\xB2\x0F\x85\x2A\x2A\x2A\x2A\x8B\x75\x0C\x8B\xCF\x56\x53\xE8\x2A\x2A\x2A\x2A\x8B\x03"),
		Symbol::FromSignature("\x48\x89\x5C\x24\x10\x48\x89\x6C\x24\x18\x56\x48\x83\xEC\x20\x48\x8B\x41\x30\x41\x0F\xB6\xE8\x48\x8B\xF2\x48\x8B\xD9\x48\x85\xC0\x74\x2A\x0F\xBF\x48\x06"),
	};

	const std::vector<Symbol> CBaseAnimating_SetTransmitSym = {//Find "Setting CBaseAnimating to non-studio model %s  (type:%i)" to get CBaseAnimating__SetModel then find the last xref and take 4 function upper
		Symbol::FromName("_ZN14CBaseAnimating11SetTransmitEP18CCheckTransmitInfob"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x89\xD7\x41\x56\x41\x55\x49\x89\xF5\x41\x54\x53"), // 55 48 89 E5 41 57 41 89 D7 41 56 41 55 49 89 F5 41 54 53
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x57\x8B\xF9\x8B\x47\x18\x85\xC0**\x0F\xBF\x48\x06**\x33\xC9\x8B\x5D\x08\x8B\xF1\xC1\xEE\x05\x83\xE1\x1F\xB8\x01\x00\x00\x00\xD3\xE0\x8B\x93\x08\x20\x00\x00\x85\x04\xB2**\x8B\x75\x0C\x8B\xCF\x56\x53\xE8\x2A\x2A\x2A\x2A\x8B\x97\x64\x14\x00\x00"), //  55 8B EC 53 56 57 8B F9 8B 47 18 85 C0 ?? ?? 0F BF 48 06 ?? ?? 33 C9 8B 5D 08 8B F1 C1 EE 05 83 E1 1F B8 01 00 00 00 D3 E0 8B 93 08 20 00 00 85 04 B2 ?? ?? 8B 75 0C 8B CF 56 53 E8 C0 BC 02 00 8B 97 64 14 00 00 83 FA FF ?? ?? A1 E0 09 9B 10 8B CA 81 E1 FF 3F 00 00 C1 EA 0E 03 C9
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x48\x8B\x41\x30\x41\x0F\xB6\xF0\x48\x8B\xFA\x48\x8B\xD9\x48\x85\xC0\x74\x2A\x44\x0F\xBF\x48\x06\xEB\x2A\x45\x33\xC9\x48\x8B\x87\x10\x20\x00\x00\x41\x8B\xD1\x41\x83\xE1\x1F\x48\xC1\xEA\x05\x8B\x04\x90\x44\x0F\xA3\xC8\x0F\x82\x2A\x2A\x2A\x2A\x48\x8B\xD7\xE8\x2A\x2A\x2A\x2A\x8B\x93\x14\x16\x00\x00"), //  48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B 41 30 41 0F B6 F0 48 8B FA 48 8B D9 48 85 C0 74 ? 44 0F BF 48 06 EB ? 45 33 C9 48 8B 87 10 20 00 00 41 8B D1 41 83 E1 1F 48 C1 EA 05 8B 04 90 44 0F A3 C8 0F 82 ? ? ? ? 48 8B D7 E8 ? ? ? ? 8B 93 14 16 00 00
	};

	//---------------------------------------------------------------------------------
	// Purpose: steamworks Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CSteam3Server_OnLoggedOffSym = { // Search for "Connection to Steam servers lost."
		Symbol::FromName("_ZN13CSteam3Server11OnLoggedOffEP26SteamServersDisconnected_t"),
		Symbol::FromSignature("\x83\xBF\xF0\x00\x00\x00\x01\x75\x07\xC3\x66\x0F\x1F\x44\x00\x00"), // 83 BF F0 00 00 00 01 75 07 C3 66 0F 1F 44 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x4D\x0C\x83\xEC\x48"), // 55 8B EC 8B 4D 0C 83 EC 48
		Symbol::FromSignature("\x48\x83\xEC\x48\x83\xB9\xC0\x00\x00\x00\x01\x74*\x8B\x12"), // 48 83 EC 48 83 B9 C0 00 00 00 01 74 ?? 8B 12
	};

	const std::vector<Symbol> CSteam3Server_OnLogonSuccessSym = { //Search for "Connection to Steam servers successful."
		Symbol::FromName("_ZN13CSteam3Server14OnLogonSuccessEP23SteamServersConnected_t"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x4C\x8D\x25****\x53\x48\x89\xFB\x48\x83\xEC\x20"), // 55 48 89 E5 41 54 4C 8D 25 ?? ?? ?? ?? 53 48 89 FB 48 83 EC 20
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x1C\x53\x8B\x1D****\x57\x68****\x8B\xF9\xFF\xD3"),
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x50\x48\x8B\xD9\x48\x8D\x0D****\xFF\x15****\x48\x83\x38\x00"), // 40 53 48 83 EC 50 48 8B D9 48 8D 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 48 83 38 00
	};

	const std::vector<Symbol> CSteam3Server_ShutdownSym = {//Look into CSteam3Server_Activate
		Symbol::FromName("_ZN13CSteam3Server8ShutdownEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x8D\x3D****\x48\x83\xEC\x08\xE8****\x48\x83\x38\x00"), // 55 48 89 E5 53 48 89 FB 48 8D 3D ?? ?? ?? ?? 48 83 EC 08 E8 ?? ?? ?? ?? 48 83 38 00
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x08\x56\x68****\x8B\xF1\xFF\x15****\x83\xC4\x04"),
		//Inline in win64 unfortunately
	};

	const std::vector<Symbol> CSteam3Server_ActivateSym = { //Search for "-steamport"
		Symbol::FromName("_ZN13CSteam3Server8ActivateENS_11EServerTypeE"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x41\x89\xF4\x53\x48\x89\xFB\x48\x81\xEC\x38\x05\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 41 89 F4 53 48 89 FB 48 81 EC 38 05 00 00
		Symbol::FromSignature("\x55\x8B\xEC\xA1****\x81\xEC\x28\x05\x00\x00\x8B\x50\x48\x53"),
		Symbol::FromSignature("\x40\x55\x53\x56\x48\x8D\xAC\x24\x80\xFB\xFF\xFF\x48\x81\xEC\x80\x05\x00\x00"), // 40 55 53 56 48 8D AC 24 80 FB FF FF 48 81 EC 80 05 00 00
	};

	const std::vector<Symbol> Steam3ServerSym = { // 64x = Found with "Warning: sv_steamgroup is not applicabl" and then find the unk_[xyz], search for it's usage and find a small function.
		Symbol::FromName("_Z12Steam3Serverv"),
		Symbol::FromSignature("\x48\x8D\x05****\xC3\x0F\x1F\x84\x00\x00\x00\x00\x00\x48\x8D\x05****\x55\x48\x89\xE5\x48\x8B\x78\x38"), // 48 8D 05 ?? ?? ?? ?? C3 0F 1F 84 00 00 00 00 00 48 8D 05 ?? ?? ?? ?? 55 48 89 E5 48 8B 78 38
		NULL_SIGNATURE, // Inline on windows
		Symbol::FromSignature("\x48\x8D\x05****\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x8B\x05****"), // 48 8D 05 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC 48 89 5C 24 08 57 48 83 EC 20 48 8B 05 ?? ?? ?? ??
	};

	const std::vector<Symbol> SV_InitGameServerSteamSym = {
		Symbol::FromName("_Z22SV_InitGameServerSteamv"),
		Symbol::FromSignature("\x83\x3D****\x01\x7F\x07\xC3\x66\x0F\x1F\x44\x00\x00\x55\x48\x89\xE5"), // 83 3D ?? ?? ?? ?? 01 7F 07 C3 66 0F 1F 44 00 00 55 48 89 E5
		//Inline in win unfortunately
	};

	const std::vector<Symbol> CSteam3Server_NotifyClientConnectSym = { // 64x = "S3: Client"
		Symbol::FromName("_ZN13CSteam3Server19NotifyClientConnectEP11CBaseClientjR8netadr_sPKvj"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x4C\x8D\x3D****\x41\x56\x41\x55\x49\x89\xFD"), // 55 48 89 E5 41 57 4C 8D 3D ?? ?? ?? ?? 41 56 41 55 49 89 FD
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x48\x53\x8B\x1D****\x56\x8B\xF1\x68****\x89\x75\xFC"),
		Symbol::FromSignature("\x48\x89\x74\x24\x18\x48\x89\x7C\x24\x20\x55\x41\x54\x41\x57\x48\x8D\x6C\x24\xC9"), // 48 89 74 24 18 48 89 7C 24 20 55 41 54 41 57 48 8D 6C 24 C9
	};

	const std::vector<Symbol> CSteam3Server_SendUpdatedServerDetailsSym = { // 64x = "steamblocking:%d"
		Symbol::FromName("_ZN13CSteam3Server24SendUpdatedServerDetailsEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x4C\x8D\x25****\x53\x48\x89\xFB\x48\x83\xEC\x58"), // 55 48 89 E5 41 57 41 56 41 55 41 54 4C 8D 25 ?? ?? ?? ?? 53 48 89 FB 48 83 EC 58
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x48\x53\x8B\x1D****\x56\x8B\xF1\x68****\x89\x75\xE0"),
		Symbol::FromSignature("\x48\x89\x4C\x24\x08\x56\x48\x83\xEC\x70\x48\x8B\xF1\x48\x8D\x0D****"), // 48 89 4C 24 08 56 48 83 EC 70 48 8B F1 48 8D 0D ?? ?? ?? ??
	};

	const std::vector<Symbol> CSteam3Server_CheckForDuplicateSteamIDSym = { // 64x = "STEAM UserID"
		Symbol::FromName("_ZN13CSteam3Server24CheckForDuplicateSteamIDEPK11CBaseClient"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x83\xEC\x68\x83\xBF\xF0\x00\x00\x00\x01"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 68 83 BF F0 00 00 00 01
		//Inline in win unfortunately
	};

	//---------------------------------------------------------------------------------
	// Purpose: voicechat Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> SV_BroadcastVoiceDataSym = { // Sending voice from: %s
		Symbol::FromName("_Z21SV_BroadcastVoiceDataP7IClientiPcx"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x89\xF6\x41\x55\x49\x89\xFD\x41\x54\x49\x89\xD4"), // 55 48 89 E5 41 57 41 56 41 89 F6 41 55 49 89 FD 41 54 49 89 D4
		Symbol::FromSignature("\x55\x8B\xEC\xA1****\x83\xEC\x50\x8B\x50\x48"), // 55 8B EC A1 ?? ?? ?? ?? 83 EC 50 8B 50 48
		Symbol::FromSignature("\x48\x89\x5C\x24\x18\x56\x57\x41\x56\x48\x81\xEC\x80\x00\x00\x00"), // 48 89 5C 24 18 56 57 41 56 48 81 EC 80 00 00 00
	};

	// Search for "CGameRules" to end up in CGameRules::CGameRules, then find the mov eax, dword_ and one of them will be g_pVoiceGameMgrHelper
	// and then click around wildly until you end up at a function that looks like its right (I got no idea what I clicked, just spammed)
	// The right functions does lots of return 1 in failure causes and between two ifs got two calls (the Push_Entity calls) at one point
	const std::vector<Symbol> CVoiceGameMgrHelper_CanPlayerHearPlayerSym = {
		Symbol::FromName("_ZN19CVoiceGameMgrHelper19CanPlayerHearPlayerEP11CBasePlayerS1_Rb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x4C\x8D\x35****\x41\x55\x41\x54\x53\x49\x8B\x3E"), // 55 48 89 E5 41 56 4C 8D 35 ?? ?? ?? ?? 41 55 41 54 53 49 8B 3E
		Symbol::FromSignature("\x55\x8B\xEC\x8B*****\x85\xC9**\x6A*\xE8****\x84\xC0**\xFF**\xE8****\xFF**\xE8****\x8B*****\x83\xC4\x08\x6A\x02\x6A\x02"), // 55 8B EC 8B ?? ?? ?? ?? ?? 85 C9 ?? ?? 6A ?? E8 ?? ?? ?? ?? 84 C0 ?? ?? FF ?? ?? E8 ?? ?? ?? ?? FF ?? ?? E8 ?? ?? ?? ?? 8B ?? ?? ?? ?? ?? 83 C4 08 6A 02 6A 02
		// Needs all other platforms.
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x48\x8B\x0D****\x49\x8B\xF1"), // 48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B 0D ?? ?? ?? ?? 49 8B F1
	};

	//---------------------------------------------------------------------------------
	// Purpose: physenv Symbols
	// ToDo: Get the Linux64 and Windows32 symbols.
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> IVP_Mindist_do_impactSym = { // Search for '%s in contact with %s, crash. dist = %d', then do xref to 'IVP Failed at %s %d\n' and find the xref bellow
		Symbol::FromName("_ZN11IVP_Mindist9do_impactEv"),
		Symbol::FromName("_ZN11IVP_Mindist9do_impactEv"),
	};

	const std::vector<Symbol> IVP_Event_Manager_Standard_simulate_time_eventsSym = { // NOTE: is virtual
		Symbol::FromName("_ZN26IVP_Event_Manager_Standard20simulate_time_eventsEP16IVP_Time_ManagerP15IVP_Environment8IVP_Time"),
		Symbol::FromName("_ZN26IVP_Event_Manager_Standard20simulate_time_eventsEP16IVP_Time_ManagerP15IVP_Environment8IVP_Time"),
	};

	const std::vector<Symbol> IVP_Mindist_simulate_time_eventSym = {
		Symbol::FromName("_ZN11IVP_Mindist19simulate_time_eventEP15IVP_Environment"),
		Symbol::FromName("_ZN11IVP_Mindist19simulate_time_eventEP15IVP_Environment"),
	};

	const std::vector<Symbol> IVP_Mindist_update_exact_mindist_eventsSym = {
		Symbol::FromName("_ZN11IVP_Mindist27update_exact_mindist_eventsE8IVP_BOOL22IVP_MINDIST_EVENT_HINT"),
		Symbol::FromName("_ZN11IVP_Mindist27update_exact_mindist_eventsE8IVP_BOOL22IVP_MINDIST_EVENT_HINT"),
	};

	const std::vector<Symbol> GMod_Util_IsPhysicsObjectValidSym = { // PhysObject [%s][Entity %d] or "Tried to use a NULL physics object!""
		Symbol::FromName("_ZN4GMod4Util20IsPhysicsObjectValidEP14IPhysicsObject"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x49\x89\xFD\x41\x54\x53\x48\x83\xEC\x10\x48\x8B\x1D****"), // 55 48 89 E5 41 56 41 55 49 89 FD 41 54 53 48 83 EC 10 48 8B 1D ?? ?? ?? ??
		Symbol::FromSignature("\x55\x8B\xEC\x51\x8B*****\x8D\x55\xFC\xC7\x45\xFC\x00\x00\x00\x00"), // 55 8B EC 51 8B ?? ?? ?? ?? ?? 8D 55 FC C7 45 FC 00 00 00 00
	};

	const std::vector<Symbol> CPhysicsHook_FrameUpdatePostEntityThinkSym = { // "CPhysicsHook::FrameUpdatePostEntityThink" - VPROF Call
		Symbol::FromName("_ZN12CPhysicsHook26FrameUpdatePostEntityThinkEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x4C\x8B\x25****\x53\x48\x89\xFB\x45\x8B\xAC\x24\x0C\x10\x00\x00\x45\x85\xED\x0F\x85\xEA\x00\x00\x00"), // 55 48 89 E5 41 56 41 55 41 54 4C 8B 25 ?? ?? ?? ?? 53 48 89 FB 45 8B AC 24 0C 10 00 00 45 85 ED 0F 85 EA 00 00 00
	};

	const std::vector<Symbol> CCollisionEvent_FrameUpdateSym = {
		Symbol::FromName("_ZN15CCollisionEvent11FrameUpdateEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x83\xEC\x08\x4C\x8B\x2D****"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 FC 53 48 83 EC 08 4C 8B 2D ?? ?? ?? ??
	};

	const std::vector<Symbol> CPhysicsEnvironment_DestroyObjectSym = {//Search for 'error deleting physics object\n'
		Symbol::FromName("_ZN19CPhysicsEnvironment13DestroyObjectEP14IPhysicsObject"),
		Symbol::FromName("_ZN19CPhysicsEnvironment13DestroyObjectEP14IPhysicsObject"),
		// On Windows we will use the vtable instead to detour it.... maybe we should do the same on linux?
	};

	const std::vector<Symbol> CPhysicsEnvironment_RestoreSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment7RestoreERK19physrestoreparams_t"),
		Symbol::FromName("_ZN19CPhysicsEnvironment7RestoreERK19physrestoreparams_t"),
	};

	const std::vector<Symbol> CPhysicsEnvironment_TransferObjectSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment14TransferObjectEP14IPhysicsObjectP19IPhysicsEnvironment"),
		Symbol::FromName("_ZN19CPhysicsEnvironment14TransferObjectEP14IPhysicsObjectP19IPhysicsEnvironment"),
	};

	const std::vector<Symbol> CPhysicsEnvironment_CreateSphereObjectSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment18CreateSphereObjectEfiRK6VectorRK6QAngleP14objectparams_tb"),
		Symbol::FromName("_ZN19CPhysicsEnvironment18CreateSphereObjectEfiRK6VectorRK6QAngleP14objectparams_tb"),
	};

	const std::vector<Symbol> CPhysicsEnvironment_UnserializeObjectFromBufferSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment27UnserializeObjectFromBufferEPvPhjb"),
		Symbol::FromName("_ZN19CPhysicsEnvironment27UnserializeObjectFromBufferEPvPhjb"),
	};

	const std::vector<Symbol> CPhysicsEnvironment_CreatePolyObjectStaticSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment22CreatePolyObjectStaticEPK12CPhysCollideiRK6VectorRK6QAngleP14objectparams_t"),
		Symbol::FromName("_ZN19CPhysicsEnvironment22CreatePolyObjectStaticEPK12CPhysCollideiRK6VectorRK6QAngleP14objectparams_t"),
	};

	const std::vector<Symbol> CPhysicsEnvironment_CreatePolyObjectSym = {
		Symbol::FromName("_ZN19CPhysicsEnvironment16CreatePolyObjectEPK12CPhysCollideiRK6VectorRK6QAngleP14objectparams_t"),
		Symbol::FromName("_ZN19CPhysicsEnvironment16CreatePolyObjectEPK12CPhysCollideiRK6VectorRK6QAngleP14objectparams_t"),
	};

	const std::vector<Symbol> CPhysicsEnvironment_D2Sym = {
		Symbol::FromName("_ZN19CPhysicsEnvironmentD2Ev"),
		Symbol::FromName("_ZN19CPhysicsEnvironmentD2Ev"),
	};

	const std::vector<Symbol> CPhysicsEnvironment_C2Sym = {
		Symbol::FromName("_ZN19CPhysicsEnvironmentC2Ev"),
		Symbol::FromName("_ZN19CPhysicsEnvironmentC2Ev"),
	};

	const std::vector<Symbol> IVP_Mindist_Manager_recheck_ov_elementSym = {
		Symbol::FromName("_ZN19IVP_Mindist_Manager18recheck_ov_elementEP15IVP_Real_Object"),
		Symbol::FromName("_ZN19IVP_Mindist_Manager18recheck_ov_elementEP15IVP_Real_Object"),
	};

	const std::vector<Symbol> IVP_Mindist_Minimize_Solver_p_minimize_PKSym = {
		Symbol::FromName("_ZN27IVP_Mindist_Minimize_Solver13p_minimize_PKEPK16IVP_Compact_EdgeS2_P21IVP_Cache_Ledge_PointS4_"),
		Symbol::FromName("_ZN27IVP_Mindist_Minimize_Solver13p_minimize_PKEPK16IVP_Compact_EdgeS2_P21IVP_Cache_Ledge_PointS4_"),
	};

	const std::vector<Symbol> IVP_Mindist_Minimize_Solver_p_minimize_PPSym = {
		Symbol::FromName("_ZN27IVP_Mindist_Minimize_Solver13p_minimize_PPEPK16IVP_Compact_EdgeS2_P21IVP_Cache_Ledge_PointS4_"),
		Symbol::FromName("_ZN27IVP_Mindist_Minimize_Solver13p_minimize_PPEPK16IVP_Compact_EdgeS2_P21IVP_Cache_Ledge_PointS4_"),
	};

	const std::vector<Symbol> IVP_Mindist_Base_get_objectsSym = {
		Symbol::FromName("_ZN16IVP_Mindist_Base11get_objectsEPP15IVP_Real_Object"),
		Symbol::FromName("_ZN16IVP_Mindist_Base11get_objectsEPP15IVP_Real_Object"),
	};

	const std::vector<Symbol> IVP_OV_Element_add_oo_collisionSym = {
		Symbol::FromName("_ZN14IVP_OV_Element16add_oo_collisionEP13IVP_Collision"),
		Symbol::FromName("_ZN14IVP_OV_Element16add_oo_collisionEP13IVP_Collision"),
	};

	const std::vector<Symbol> IVP_OV_Element_remove_oo_collisionSym = {
		Symbol::FromName("_ZN14IVP_OV_Element19remove_oo_collisionEP13IVP_Collision"),
		Symbol::FromName("_ZN14IVP_OV_Element19remove_oo_collisionEP13IVP_Collision"),
	};

	const std::vector<Symbol> CCollisionProperty_MarkSurroundingBoundsDirtySym = { //Search for 'Clamping SetLocalVelocity(%f,%f,%f) on %s' to get CBaseEntity::SetLocalVelocity then into it find CBaseEntity::InvalidatePhysicsRecursive
		Symbol::FromName("_ZN18CCollisionProperty24MarkPartitionHandleDirtyEv"),
		Symbol::FromSignature("\x48\x8B\x57\x08\x48\x8B\x42\x40\x48\x85\xC0\x74\x12\x66\x83\x78\x06\x00"), // 48 8B 57 08 48 8B 42 40 48 85 C0 74 12 66 83 78 06 00
	};

	//---------------------------------------------------------------------------------
	// Purpose: net Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> g_NetIncomingSym = {
		Symbol::FromName("g_NetIncoming"),
		Symbol::FromSignature("\x48\x8B\x3D****\x48\x8B\x18\x8B\x57\x0C\x48\x8B\x03\x89\xD1"), // 48 8B 3D ?? ?? ?? ?? 48 8B 18 8B 57 0C 48 8B 03 89 D1
	};

	const std::vector<Symbol> g_WriteSym = {
		Symbol::FromName("_ZL7g_Write"),
		Symbol::FromSignature("\x48\x8B\x35****\xC1\xFA\x05\x48\x63\xD2\x8B\x0C\xB9\xF7\xD1"), // 48 8B 35 ?? ?? ?? ?? C1 FA 05 48 63 D2 8B 0C B9 F7 D1
	};

	const std::vector<Symbol> g_StartedSym = {
		Symbol::FromName("_ZL9g_Started"),
		Symbol::FromSignature("\xC6\x05****\x00\xC3\x66\x2E\x0F\x1F\x84\x00\x00\x00\x00\x00"), // C6 05 ?? ?? ?? ?? 00 C3 66 2E 0F 1F 84 00 00 00 00 00
	};

	//---------------------------------------------------------------------------------
	// Purpose: luajit Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CLuaInterface_GetTypeSym = { // 64x = CBaseLuaInterface::GetType (If https://github.com/Facepunch/garrysmod-requests/issues/2578 is done update this)
		Symbol::FromName("_ZN13CLuaInterface7GetTypeEi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x89\xF5\x41\x54\x49\x89\xFC\x53\x48\x83\xEC\x08"), // 55 48 89 E5 41 55 41 89 F5 41 54 49 89 FC 53 48 83 EC 08
		// This is not worth for Windows at all, so lets not even try as its just a small performance benefit when actively using vprof
	};

	//---------------------------------------------------------------------------------
	// Purpose: cvar Symbols
	// NOTE: Gmod has merged this optimization, though ours is still like 100x faster??? Nice
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CCvar_RegisterConCommandSym = {
		Symbol::FromName("_ZN5CCvar18RegisterConCommandEP14ConCommandBase"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x48\x89\xF7"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 FC 48 89 F7
	};

	const std::vector<Symbol> CCvar_UnregisterConCommandSym = {
		Symbol::FromName("_ZN5CCvar20UnregisterConCommandEP14ConCommandBase"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x49\x89\xFC\x48\x89\xF7\x53\x48\x8B\x06"), // 55 48 89 E5 41 54 49 89 FC 48 89 F7 53 48 8B 06
	};

	const std::vector<Symbol> CCvar_UnregisterConCommandsSym = {
		Symbol::FromName("_ZN5CCvar21UnregisterConCommandsEi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x45\x31\xF6\x41\x55\x4C\x8D\x6F\x58"), // 55 48 89 E5 41 57 41 56 45 31 F6 41 55 4C 8D 6F 58
	};

	const std::vector<Symbol> CCvar_FindCommandBaseConstSym = {
		Symbol::FromName("_ZNK5CCvar15FindCommandBaseEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xF4\x53\x48\x89\xFB\x48\x83\xEC\x08\x0F\xB6\x05\x42\x95\x04\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 F4 53 48 89 FB 48 83 EC 08 0F B6 05 42 95 04 00 | displacement kept fixed to tell it apart from its identical sibling
	};

	const std::vector<Symbol> CCvar_FindCommandBaseSym = {
		Symbol::FromName("_ZN5CCvar15FindCommandBaseEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xF4\x53\x48\x89\xFB\x48\x83\xEC\x08\x0F\xB6\x05****"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 F4 53 48 89 FB 48 83 EC 08 0F B6 05 ?? ?? ?? ?? | not unique, the first match is the target
	};

	//---------------------------------------------------------------------------------
	// Purpose: gameserver Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CBaseServer_GetFreeClientSym = { // Search for "%s:reconnect\n"
		Symbol::FromName("_ZN11CBaseServer13GetFreeClientER8netadr_s"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xFF\x41\x56\x41\x55\x41\x54\x53\x48\x83\xEC\x18\x8B\x9F\x58\x01\x00\x00"), // 55 48 89 E5 41 57 49 89 FF 41 56 41 55 41 54 53 48 83 EC 18 8B 9F 58 01 00 00
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x89\x5C\x24\x10\x48\x89\x6C\x24\x18\x48\x89\x74\x24\x20\x57\x41\x56\x41\x57\x48\x83\xEC\x20\x4C\x8B\xFA"), // 48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 41 56 41 57 48 83 EC 20 4C 8B FA
	};

	const std::vector<Symbol> CBaseServer_CreateFakeClientSym = { // Use found CBaseServer_GetFreeClientSym and go in the vtable 8 up | or search for "cl_predictweapons"
		Symbol::FromName("_ZN11CBaseServer16CreateFakeClientEPKc"),
		Symbol::FromSignature("\x55\x31\xD2\x48\x89\xE5\x41\x56\x41\x55\x49\x89\xF5\x31\xF6\x41\x54"), // 55 31 D2 48 89 E5 41 56 41 55 49 89 F5 31 F6 41 54
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x8B\xC4\x48\x89\x68\x10\x48\x89\x70\x18\x57\x48\x83\xEC\x60"), // 48 8B C4 48 89 68 10 48 89 70 18 57 48 83 EC 60
	};

	const std::vector<Symbol> CBaseServer_UserInfoChangedSym = { // Use found CBaseServer_GetFreeClientSym and go in the vtable 4 up
		Symbol::FromName("_ZN11CBaseServer15UserInfoChangedEi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x4C\x8D\xBD\x80\xFE\xFF\xFF\x41\x55"), // 55 48 89 E5 41 57 41 56 4C 8D BD 80 FE FF FF 41 55
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x18\x57\x48\x81\xEC\x70\x01\x00\x00"), // 48 89 5C 24 08 48 89 74 24 18 57 48 81 EC 70 01 00 00
	};

	// Use found CBaseServer_GetFreeClientSym and go in the vtable 7 up
	// Alternative: Find "Forcing client reconnect" CBaseClient::Reconnect then xref -> CGameClient::Reconnect -> CGameServer::RemoveClientFromGame
	const std::vector<Symbol> CGameServer_RemoveClientFromGameSym = {
		Symbol::FromName("_ZN11CGameServer20RemoveClientFromGameEP11CBaseClient"),
		Symbol::FromSignature("\x48\x83\xBE\x68\x07\x00\x00\x00\x0F\x84\xF2\x00\x00\x00\x55\x48\x8D\x15****"), // 48 83 BE 68 07 00 00 00 0F 84 F2 00 00 00 55 48 8D 15 ?? ?? ?? ??
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x48\x83\xBA\x68\x07\x00\x00\x00"), // 48 89 74 24 10 57 48 83 EC 20 48 83 BA 68 07 00 00 00
	};

	const std::vector<Symbol> CServerPlugin_ClientSettingsChangedSym = {
		Symbol::FromName("_ZN13CServerPlugin21ClientSettingsChangedEP7edict_t"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x49\x89\xF5\x41\x54\x53\x48\x83\xEC\x08\x8B\x47\x18\x85\xC0\x7E\x3B\x49\x89\xFC\x31\xDB\x0F\x1F\x40\x00\x49\x8B\x44\x24\x08\x48\x8B\x34\xD8\x80\xBE\x80\x00\x00\x00\x00\x75\x15\x48\x8B\xBE\x88\x00\x00\x00\x48\x85\xFF\x74\x3A\x48\x8B\x07\x4C\x89\xEE\xFF\x50\x68"), // 55 48 89 E5 41 55 49 89 F5 41 54 53 48 83 EC 08 8B 47 18 85 C0 7E 3B 49 89 FC 31 DB 0F 1F 40 00 49 8B 44 24 08 48 8B 34 D8 80 BE 80 00 00 00 00 75 15 48 8B BE 88 00 00 00 48 85 FF 74 3A 48 8B 07 4C 89 EE FF 50 68
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x33\xDB\x48\x8B\xF2\x48\x8B\xF9\x39\x59\x18\x7E*\x0F\x1F\x40\x00\x48\x8B\x47\x08\x48\x8B\x14\xD8\x80\xBA\x80\x00\x00\x00\x00\x75*\x48\x8B\x8A\x88\x00\x00\x00\x48\x85\xC9\x74*\x48\x8B\x01\x48\x8B\xD6\xFF\x50\x68"), // 48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 33 DB 48 8B F2 48 8B F9 39 59 18 7E ?? 0F 1F 40 00 48 8B 47 08 48 8B 14 D8 80 BA 80 00 00 00 00 75 ?? 48 8B 8A 88 00 00 00 48 85 C9 74 ?? 48 8B 01 48 8B D6 FF 50 68
	};

	const std::vector<Symbol> CSteam3Server_ClientFindFromSteamIDSym = {
		Symbol::FromName("_ZN13CSteam3Server21ClientFindFromSteamIDER8CSteamID"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x48\x8D\x45\xB0\x45\x31\xF6\x41\x55"), // 55 48 89 E5 41 57 41 56 48 8D 45 B0 45 31 F6 41 55
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x56\x57\x41\x56\x48\x83\xEC\x50"), // 48 89 5C 24 08 48 89 6C 24 10 56 57 41 56 48 83 EC 50
	};

	const std::vector<Symbol> CVEngineServer_GMOD_SendToClientSym = {
		Symbol::FromName("_ZN14CVEngineServer17GMOD_SendToClientEiPvi"),
		Symbol::FromSignature("\x55\x89\xF6\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x83\xEC\x78"), // 55 89 F6 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 78
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x48\x89\x7C\x24\x20\x41\x56\x48\x81\xEC\xA0\x00\x00\x00\x48\x8B\x05****"), // 48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 81 EC A0 00 00 00 48 8B 05 ?? ?? ?? ??
	};

	const std::vector<Symbol> CBaseClient_SetSignonStateSym = {
		Symbol::FromName("_ZN11CBaseClient14SetSignonStateEii"),
		Symbol::FromSignature("\x8B\x87\xA0\x01\x00\x00\x55\x48\x89\xE5\x83\xF8\x04\x74\x51\x7F\x1F"), // 8B 87 A0 01 00 00 55 48 89 E5 83 F8 04 74 51 7F 1F
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x83\xEC\x28\x8B\x91\xA0\x01\x00\x00\x83\xEA\x02\x74*"), // 48 83 EC 28 8B 91 A0 01 00 00 83 EA 02 74 ??
	};

	const std::vector<Symbol> CBaseServer_IsMultiplayerSym = {
		Symbol::FromName("_ZNK11CBaseServer13IsMultiplayerEv"),
		Symbol::FromSignature("\x83\xBF\x34\x01\x00\x00\x01\x0F\x9F\xC0\xC3\x0F\x1F\x44\x00\x00"), // 83 BF 34 01 00 00 01 0F 9F C0 C3 0F 1F 44 00 00
		NULL_SIGNATURE,
		Symbol::FromSignature("\x83\xB9\x34\x01\x00\x00\x01\x0F\x9F\xC0\xC3\xCC\xCC\xCC"), // 83 B9 34 01 00 00 01 0F 9F C0 C3 CC CC CC
	};

	const std::vector<Symbol> GModDataPack_IsSingleplayerSym = {
		Symbol::FromName("_ZN12GModDataPack14IsSingleplayerEv"),
		Symbol::FromSignature("\x48\x8D\x05****\x48\x8B\x00\x83\x78\x1C\x01\x0F\x94\xC0\xC3\x66\x2E\x0F\x1F\x84\x00\x00\x00\x00\x00"), // 48 8D 05 ?? ?? ?? ?? 48 8B 00 83 78 1C 01 0F 94 C0 C3 66 2E 0F 1F 84 00 00 00 00 00
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x8B\x05****\x83\x78\x1C\x01\x0F\x94\xC0"), // 48 8B 05 ?? ?? ?? ?? 83 78 1C 01 0F 94 C0
	};

	const std::vector<Symbol> CBaseClient_ShouldSendMessagesSym = {//Search for '%s overflowed reliable buffer (%i bytes, %s in, %s out)'
		Symbol::FromName("_ZN11CBaseClient18ShouldSendMessagesEv"),
		Symbol::FromSignature("\x55\x48\x8D\x15****\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x83\xEC\x18"), // 55 48 8D 15 ?? ?? ?? ?? 48 89 E5 41 57 41 56 41 55 41 54 53 48 89 FB 48 83 EC 18
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x0C\x56\x57\x8B\xF9\x8B\x47\x04\x8D\x77\x04\x8B\xCE"), // 55 8B EC 83 EC 0C 56 57 8B F9 8B 47 04 8D 77 04 8B CE
	};

	const std::vector<Symbol> CBaseServer_CheckTimeoutsSym = {//Search for 'CBaseServer::CheckTimeouts'
		Symbol::FromName("_ZN11CBaseServer13CheckTimeoutsEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x49\x89\xFE\x41\x55\x41\x54\x53\x48\x83\xEC\x18\x48\x8B\x05****"), // 55 48 89 E5 41 57 41 56 49 89 FE 41 55 41 54 53 48 83 EC 18 48 8B 05 ?? ?? ?? ??
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x8B\xC4\x48\x83\xEC\x48\x48\x89\x68\x10\x48\x8B\xE9"), // 48 8B C4 48 83 EC 48 48 89 68 10 48 8B E9
	};

	const std::vector<Symbol> CBaseClient_OnRequestFullUpdateSym = {//Search for 'CBaseClient::OnRequestFullUpdate'
		Symbol::FromName("_ZN11CBaseClient19OnRequestFullUpdateEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x4C\x8B\x25****\x53\x48\x89\xFB\x45\x8B\xAC\x24\x0C\x10\x00\x00\x45\x85\xED\x0F\x85\x4A\x01\x00\x00"), // 55 48 89 E5 41 56 41 55 41 54 4C 8B 25 ?? ?? ?? ?? 53 48 89 FB 45 8B AC 24 0C 10 00 00 45 85 ED 0F 85 4A 01 00 00
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x30\x48\x8B\xD9\x48\x8B\x0D****"), // 48 89 5C 24 08 57 48 83 EC 30 48 8B D9 48 8B 0D ?? ?? ?? ??
	};

	const std::vector<Symbol> CGameClient_SpawnPlayerSym = {
		Symbol::FromName("_ZN11CGameClient11SpawnPlayerEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x89\xFB\x48\x83\xEC\x38\x48\x8D\x3D****\x80\xBF\x96\x01\x00\x00\x00"), // 55 48 89 E5 53 48 89 FB 48 83 EC 38 48 8D 3D ?? ?? ?? ?? 80 BF 96 01 00 00 00
		NULL_SIGNATURE,
		Symbol::FromSignature("\x40\x53\x48\x83\xEC\x50\x80\x3D****\x00\x48\x8B\xD9\x74*"), // 40 53 48 83 EC 50 80 3D ?? ?? ?? ?? 00 48 8B D9 74 ??
	};

	const std::vector<Symbol> CBaseServer_ProcessConnectionlessPacketSym = { // Search for "Source Engine Query" to find CHLTVServer::ProcessConnectionlessPacket which will call CBaseServer::ProcessConnectionlessPacket
		Symbol::FromName("_ZN11CBaseServer27ProcessConnectionlessPacketEP11netpacket_s"),
		Symbol::FromSignature("\x55\x49\x89\xFA\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53"), // 55 49 89 FA 48 89 E5 41 57 41 56 41 55 41 54 53
		NULL_SIGNATURE,
		Symbol::FromSignature("\x40\x55\x53\x56\x57\x41\x56\x41\x57\x48\x8D\xAC\x24\x48\xF6\xFF\xFF"), // 40 55 53 56 57 41 56 41 57 48 8D AC 24 48 F6 FF FF
	};

	const std::vector<Symbol> NET_SendPacketSym = { // Search for "NET_SendPacket"
		Symbol::FromName("_Z14NET_SendPacketP11INetChanneliRK8netadr_sPKhiP8bf_writeb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x4D\x63\xF8\x41\x56\x41\x55\x41\x54\x4D\x89\xCC"), // 55 48 89 E5 41 57 4D 63 F8 41 56 41 55 41 54 4D 89 CC
		Symbol::FromSignature("\x55\x8B\xEC\xB8\x58\x20\x00\x00\xE8****\x8B\x0D****"), // 55 8B EC B8 58 20 00 00 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ??
		Symbol::FromSignature("\x89\x54\x24\x10\x48\x89\x4C\x24\x08\x55\x56\x48\x8D\xAC\x24****"), // 89 54 24 10 48 89 4C 24 08 55 56 48 8D AC 24 ?? ?? ?? ??
	};

	const std::vector<Symbol> CNetChan_SendDatagramSym = {
		Symbol::FromName("_ZN8CNetChan12SendDatagramEP8bf_write"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xFF\x41\x56\x41\x55\x41\x54\x49\x89\xF4\x53\x48\x81\xEC\xC8\x00\x04\x00"), // 55 48 89 E5 41 57 49 89 FF 41 56 41 55 41 54 49 89 F4 53 48 81 EC C8 00 04 00
		NULL_SIGNATURE,
		Symbol::FromSignature("\x40\x55\x53\x56\x41\x56\x48\x8D\xAC\x24****"), // 40 55 53 56 41 56 48 8D AC 24 ?? ?? ?? ??
	};

	const std::vector<Symbol> CNetChan_D2Sym = {
		Symbol::FromName("_ZN8CNetChanD2Ev"),
		Symbol::FromSignature("\x55\x48\x8D\x05****\x48\x8D\x35****\x48\x89\xE5\x41\x55\x41\x54\x49\x89\xFC"), // 55 48 8D 05 ?? ?? ?? ?? 48 8D 35 ?? ?? ?? ?? 48 89 E5 41 55 41 54 49 89 FC
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x57\x41\x56\x41\x57\x48\x83\xEC\x20\x44\x8B\xF2"), // 48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 20 44 8B F2
	};

	const std::vector<Symbol> NET_CreateNetChannelSym = {
		Symbol::FromName("_Z20NET_CreateNetChanneliP8netadr_sPKcP18INetChannelHandlerbi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x49\x89\xF5\x41\x54\x41\x89\xFC\x53\x48\x83\xEC\x28\x41\x80\xF8\x01"), // 55 48 89 E5 41 57 41 56 41 55 49 89 F5 41 54 41 89 FC 53 48 83 EC 28 41 80 F8 01
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x48\x89\x7C\x24\x20\x41\x56\x48\x83\xEC\x40\x80\x7C\x24\x70\x00"), // 48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 40 80 7C 24 70 00
	};

	const std::vector<Symbol> NET_RemoveNetChannelSym = {
		Symbol::FromName("_Z20NET_RemoveNetChannelP11INetChannelb"),
		Symbol::FromSignature("\x48\x85\xFF\x0F\x84\x17\x01\x00\x00\x55\x48\x89\xE5\x41\x56\x41\x55"), // 48 85 FF 0F 84 17 01 00 00 55 48 89 E5 41 56 41 55
		Symbol::FromSignature("\x55\x8B\xEC\x57\x8B\x7D\x08\x85\xFF\x0F\x84****\x53"), // 55 8B EC 57 8B 7D 08 85 FF 0F 84 ?? ?? ?? ?? 53
		Symbol::FromSignature("\x48\x85\xC9\x0F\x84****\x48\x89\x5C\x24\x18"), // 48 85 C9 0F 84 ?? ?? ?? ?? 48 89 5C 24 18
	};

	const std::vector<Symbol> NET_GetLastErrorSym = {
		Symbol::FromName("_Z16NET_GetLastErrorv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x53\xE8****\x44\x8B\x20\x48\x8D\x3D****\xE8****\xBA\x04\x00\x00\x00"), // 55 48 89 E5 41 54 53 E8 ?? ?? ?? ?? 44 8B 20 48 8D 3D ?? ?? ?? ?? E8 ?? ?? ?? ?? BA 04 00 00 00
	};

	const std::vector<Symbol> NET_ErrorStringSym = {
		Symbol::FromName("_Z15NET_ErrorStringi"),
		Symbol::FromSignature("\xE9****\x66\x66\x2E\x0F\x1F\x84\x00\x00\x00\x00\x00\x55\xB8\x02\x00\x00\x00"), // E9 ?? ?? ?? ?? 66 66 2E 0F 1F 84 00 00 00 00 00 55 B8 02 00 00 00 | tail jump to strerror, the function start behind it makes this unique
	};

	const std::vector<Symbol> NET_ReceiveDatagramSym = {
		Symbol::FromName("_Z19NET_ReceiveDatagramiP11netpacket_s"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x89\xF3\x48\x81\xEC\xA8\x20\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 89 F3 48 81 EC A8 20 00 00
		Symbol::FromSignature("\x55\x8B\xEC\xB8\x48\x20\x00\x00\xE8****\x8B\x15****"), // 55 8B EC B8 48 20 00 00 E8 ?? ?? ?? ?? 8B 15 ?? ?? ?? ??
		Symbol::FromSignature("\x40\x55\x53\x48\x8D\xAC\x24****\xB8\xE8\x20\x00\x00"), // 40 55 53 48 8D AC 24 ?? ?? ?? ?? B8 E8 20 00 00
	};

	const std::vector<Symbol> NET_ReceiveP2PDatagramSym = {
		Symbol::FromName("_Z22NET_ReceiveP2PDatagramiP11netpacket_s"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x55\x41\x54\x41\x89\xFC\x53\x48\x89\xF3\x48\x83\xEC\x18"), // 55 48 89 E5 41 55 41 54 41 89 FC 53 48 89 F3 48 83 EC 18
	};

	const std::vector<Symbol> NET_GetLongSym = {
		Symbol::FromName("_Z11NET_GetLongiP11netpacket_s"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x83\xEC\x38\x48\x63\x46\x48"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 38 48 63 46 48
	};

	const std::vector<Symbol> NET_LagPacketSym = {
		Symbol::FromName("_Z13NET_LagPacketbP11netpacket_s"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x89\xFD\x41\x54\x53\x48\x83\xEC\x18\x83\x7E\x14\x04"), // 55 48 89 E5 41 57 41 56 41 55 41 89 FD 41 54 53 48 83 EC 18 83 7E 14 04
	};

	const std::vector<Symbol> COM_BufferToBufferDecompressSym = {
		Symbol::FromName("_Z28COM_BufferToBufferDecompressPcPjPKcj"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x4C\x8D\x7D\xB0\x41\x56\x49\x89\xFE\x41\x55"), // 55 48 89 E5 41 57 4C 8D 7D B0 41 56 49 89 FE 41 55
	};

	const std::vector<Symbol> net_socketsSym = {
		Symbol::FromName("_ZL11net_sockets"),
		Symbol::FromSignature("\x48\x03\x1D****\x44\x3B\x63\x0C\x74\x16\x48\x83\xC4\x18\x5B"), // 48 03 1D ?? ?? ?? ?? 44 3B 63 0C 74 16 48 83 C4 18 5B
	};

	const std::vector<Symbol> Filter_SendBanSym = {
		Symbol::FromName("_Z14Filter_SendBanRK8netadr_s"),
		Symbol::FromSignature("\x48\x89\xFE\xB9\x6C\x00\x00\x00\xBF\x01\x00\x00\x00\x48\x8D\x15****"), // 48 89 FE B9 6C 00 00 00 BF 01 00 00 00 48 8D 15 ?? ?? ?? ??
		NULL_SIGNATURE,
		Symbol::FromSignature("\x41\xB9\x6C\x00\x00\x00\x4C\x8D\x05****\x48\x8B\xD1"), // 41 B9 6C 00 00 00 4C 8D 05 ?? ?? ?? ?? 48 8B D1
	};

	const std::vector<Symbol> NET_SendStreamSym = {
		Symbol::FromName("_Z14NET_SendStreamiPKcii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x4C\x8B\x25****\x53\x49\x8B\x04\x24\xFF\x90\xD0\x00\x00\x00"), // 55 48 89 E5 41 56 41 55 41 54 4C 8B 25 ?? ?? ?? ?? 53 49 8B 04 24 FF 90 D0 00 00 00
		NULL_SIGNATURE,
		Symbol::FromSignature("\x40\x57\x48\x83\xEC\x20\x48\x8B\x05****\x4C\x8B\x10\x41\xFF\x92\xD0\x00\x00\x00"), // 40 57 48 83 EC 20 48 8B 05 ?? ?? ?? ?? 4C 8B 10 41 FF 92 D0 00 00 00
	};

	const std::vector<Symbol> NET_ReceiveStreamSym = {
		Symbol::FromName("_Z17NET_ReceiveStreamiPcii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x4C\x8B\x25****\x53\x49\x8B\x04\x24\xFF\x90\xC8\x00\x00\x00"), // 55 48 89 E5 41 56 41 55 41 54 4C 8B 25 ?? ?? ?? ?? 53 49 8B 04 24 FF 90 C8 00 00 00
		NULL_SIGNATURE,
		Symbol::FromSignature("\x40\x57\x48\x83\xEC\x20\x48\x8B\x05****\x4C\x8B\x10\x41\xFF\x92\xC8\x00\x00\x00"), // 40 57 48 83 EC 20 48 8B 05 ?? ?? ?? ?? 4C 8B 10 41 FF 92 C8 00 00 00
	};

	const std::vector<Symbol> s_NetChannelsSym = {
		Symbol::FromName("_ZL13s_NetChannels"),
		Symbol::FromSignature("\x48\x8B\x15****\x48\x63\xCE\x31\xC0\xEB\x11\x0F\x1F\x40\x00"), // 48 8B 15 ?? ?? ?? ?? 48 63 CE 31 C0 EB 11 0F 1F 40 00
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x48\x89\x7C\x24\x20\x41\x56\x48\x83\xEC\x40\x80\x7C\x24\x70\x00", 0xC8), // 48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 40 80 7C 24 70 00
	};

	const std::vector<Symbol> NET_SetTimeSym = {
		Symbol::FromName("_Z11NET_SetTimed"),
		Symbol::FromSignature("\x55\x66\x0F\x28\xD0\x48\x89\xE5\x48\x83\xEC\x10\xF2\x0F\x10\x0D****\xF2\x0F\x5C\x15****\xF2\x0F\x11\x05****\x66\x0F\x2F\xD1\x77\x1D\x66\x0F\x28\xCA\x66\x0F\xEF\xC0\xF2\x0F\xC2\xC8\x05\x66\x0F\xEF\xC0\x66\x0F\x54\xD1\x66\x0F\x55\xC8\x66\x0F\x56\xCA\x48\x8D\x05****\x48\x8B\x78\x38\x48\x8B\x47\x78\x48\x85\xC0\x74\x05\x80\x38\x00\x75\x20"), // 55 66 0F 28 D0 48 89 E5 48 83 EC 10 F2 0F 10 0D ?? ?? ?? ?? F2 0F 5C 15 ?? ?? ?? ?? F2 0F 11 05 ?? ?? ?? ?? 66 0F 2F D1 77 1D 66 0F 28 CA 66 0F EF C0 F2 0F C2 C8 05 66 0F EF C0 66 0F 54 D1 66 0F 55 C8 66 0F 56 CA 48 8D 05 ?? ?? ?? ?? 48 8B 78 38 48 8B 47 78 48 85 C0 74 05 80 38 00 75 20
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x83\xEC\x38\x48\x8B\x05****\x48\x8B\x50\x78\x48\x8D\x48\x78\x0F\x29\x74\x24\x20\x0F\x28\xF0\xF2\x0F\x5C\x35****\xF2\x0F\x11\x05****\x0F\x57\xC0\xF2\x0F\x5F\xF0\xF2\x0F\x5D\x35****\x48\x85\xD2\x74*\x80\x3A\x00\x74*\xE8****\x48\x8B\xC8\xE8****\x0F\x57\xC9\xF2\x0F\x5A\xC8\xEB*\xF3\x0F\x10\x48\x54\x0F\x5A\xC1\xF2\x0F\x10\x0D****\xF2\x0F\x59\xC6\x0F\x28\x74\x24\x20"), // 48 83 EC 38 48 8B 05 ?? ?? ?? ?? 48 8B 50 78 48 8D 48 78 0F 29 74 24 20 0F 28 F0 F2 0F 5C 35 ?? ?? ?? ?? F2 0F 11 05 ?? ?? ?? ?? 0F 57 C0 F2 0F 5F F0 F2 0F 5D 35 ?? ?? ?? ?? 48 85 D2 74 ?? 80 3A 00 74 ?? E8 ?? ?? ?? ?? 48 8B C8 E8 ?? ?? ?? ?? 0F 57 C9 F2 0F 5A C8 EB ?? F3 0F 10 48 54 0F 5A C1 F2 0F 10 0D ?? ?? ?? ?? F2 0F 59 C6 0F 28 74 24 20
	};

	const std::vector<Symbol> CGameClient_ExecuteStringCommandSym = {
		Symbol::FromName("_ZN11CGameClient20ExecuteStringCommandEPKc"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x49\x89\xF5\x41\x54\x53\x48\x89\xFB\x48\x81\xEC\x28\x06\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 49 89 F5 41 54 53 48 89 FB 48 81 EC 28 06 00 00
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x48\x89\x7C\x24\x20\x41\x56\x48\x81\xEC\x30\x06\x00\x00\x48\x8B\xEA"), // 48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 81 EC 30 06 00 00 48 8B EA
	};

	//---------------------------------------------------------------------------------
	// Purpose: AutoRefresh Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> GarrysMod_AutoRefresh_HandleChange_LuaSym = {
		Symbol::FromName("_ZN9GarrysMod11AutoRefresh16HandleChange_LuaERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEES8_S8_"),
		Symbol::FromSignature("\x31\xC0\x48\x83\x7A\x08\x03\x74\x07\xC3\x66\x0F\x1F\x44\x00\x00\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x49\x89\xD5"), // 31 C0 48 83 7A 08 03 74 07 C3 66 0F 1F 44 00 00 55 48 89 E5 41 57 41 56 41 55 49 89 D5
	};

	const std::vector<Symbol> GarrysMod_AutoRefresh_InitSym = {
		Symbol::FromName("_ZN9GarrysMod11AutoRefresh4InitEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x83\xEC\x58\xE8****"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 58 E8 ?? ?? ?? ??
	};

	const std::vector<Symbol> GarrysMod_AutoRefresh_CycleSym = {
		Symbol::FromName("_ZN9GarrysMod11AutoRefresh5CycleEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x53\x48\x8D\x1D****\x48\x83\xEC\x20\x48\x89\xDF"), // 55 48 89 E5 41 54 53 48 8D 1D ?? ?? ?? ?? 48 83 EC 20 48 89 DF
	};

	const std::vector<Symbol> Bootil_File_ChangeMonitor_CheckForChangesSym = {
		Symbol::FromName("_ZN6Bootil4File13ChangeMonitor15CheckForChangesEv"),
		Symbol::FromName("_ZN6Bootil4File13ChangeMonitor15CheckForChangesEv"),
	};

	const std::vector<Symbol> Bootil_File_ChangeMonitor_HasChangesSym = {
		Symbol::FromName("_ZN6Bootil4File13ChangeMonitor10HasChangesEv"),
		Symbol::FromName("_ZN6Bootil4File13ChangeMonitor10HasChangesEv"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: Soundscape Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CEnvSoundscape_UpdateForPlayerSym = { // "CSoundscapeSystem::Init" -> Find vtable of CSoundscapeSystem and from there find it
		Symbol::FromName("_ZN14CEnvSoundscape15UpdateForPlayerER11ss_update_t"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x89\xF3\x48\x81\xEC\x08\x01\x00\x00\x80\xBF\xFC\x15\x00\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 FC 53 48 89 F3 48 81 EC 08 01 00 00 80 BF FC 15 00 00 00
		// Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x49\x89\xFC\x53\x48\x89\xF3\x48******\x80"), // 55 48 89 E5 41 57 41 56 41 55 41 54 49 89 FC 53 48 89 F3 48 ?? ?? ?? ?? ?? ?? 80
		// I feel like this symbol is wrong :/
	};

	const std::vector<Symbol> CEnvSoundscape_WriteAudioParamsToSym = {
		Symbol::FromName("_ZN14CEnvSoundscape18WriteAudioParamsToER13audioparams_t"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x49\x89\xF6\x41\x55\x49\x89\xFD\x41\x54\x53\x48\x83\xEC\x18\x8B\x46\x70"), // 55 48 89 E5 41 57 41 56 49 89 F6 41 55 49 89 FD 41 54 53 48 83 EC 18 8B 46 70
	};

	const std::vector<Symbol> g_SoundscapeSystemSym = {
		Symbol::FromName("g_SoundscapeSystem"),
		Symbol::FromSignature("\x48\x89\x05****\xE8****\x48\x89\xDA\x4C\x89\xE6\x66\x0F\xEF\xC0"), // 48 89 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 89 DA 4C 89 E6 66 0F EF C0
	};

	//---------------------------------------------------------------------------------
	// Purpose: networkthreading Symbols
	// NOTE: Tbh this really has no purpose on Windows / really is useful only on Linux Dedicated Servers so let's only support Linux 32x/64x
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> NET_ProcessSocketSym = {
		Symbol::FromName("_Z17NET_ProcessSocketiP28IConnectionlessPacketHandler"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x49\x89\xF5\x41\x54\x41\x89\xFC\x53\x48\x83\xEC\x28\x48\x8B\x05****"), // 55 48 89 E5 41 57 41 56 41 55 49 89 F5 41 54 41 89 FC 53 48 83 EC 28 48 8B 05 ?? ?? ?? ??
	};

	const std::vector<Symbol> NET_GetPacketSym = {
		Symbol::FromName("_Z13NET_GetPacketiPh"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x89\xFD\x41\x54\x49\x89\xF4"), // 55 48 89 E5 41 57 41 56 41 55 41 89 FD 41 54 49 89 F4
	};

	const std::vector<Symbol> Filter_ShouldDiscardSym = {
		Symbol::FromName("_Z20Filter_ShouldDiscardRK8netadr_s"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x83\xEC\x08\x48\x8B\x3D****"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 89 FB 48 83 EC 08 48 8B 3D ?? ?? ?? ??
	};

	const std::vector<Symbol> NET_FindNetChannelSym = {
		Symbol::FromName("_Z18NET_FindNetChanneliR8netadr_s"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x89\xFD\x41\x54\x53\x48\x83\xEC\x18\x48\x89\x75\xC8"), // 55 48 89 E5 41 57 41 56 41 55 41 89 FD 41 54 53 48 83 EC 18 48 89 75 C8
		Symbol::FromSignature("\x55\x8B\xEC\x53\x56\x57\xFF\x15****\x8B\xD0"), // 55 8B EC 53 56 57 FF 15 ?? ?? ?? ?? 8B D0
		Symbol::FromSignature("\x48\x83\xEC\x28\x48\x89\x5C\x24\x30\x48\x89\x6C\x24\x38\x8B\xE9"), // 48 83 EC 28 48 89 5C 24 30 48 89 6C 24 38 8B E9
	};

	const std::vector<Symbol> netadr_s_CompareAdrSym = {
		Symbol::FromName("_ZNK8netadr_s10CompareAdrERKS_b"),
		Symbol::FromSignature("\x8B\x07\x31\xC9\x39\x06\x75\x1B\x44\x8D\x40\xFF\xB9\x01\x00\x00\x00"), // 8B 07 31 C9 39 06 75 1B 44 8D 40 FF B9 01 00 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x55\x08\x8B\x01\x39\x02\x75*\x83\xF8\x01\x74*\x83\xF8\x02"), // 55 8B EC 8B 55 08 8B 01 39 02 75 ?? 83 F8 01 74 ?? 83 F8 02
		Symbol::FromSignature("\x44\x8B\x09\x44\x39\x0A\x75*\x41\x8D\x41\xFF\x83\xF8\x01"), // 44 8B 09 44 39 0A 75 ?? 41 8D 41 FF 83 F8 01
	};

	const std::vector<Symbol> netadr_s_ToStringSym = {
		Symbol::FromName("_ZNK8netadr_s8ToStringEb"),
		Symbol::FromSignature("\x55\xBA\x40\x00\x00\x00\x48\x89\xE5\x41\x55\x41\x89\xF5\x41\x54"), // 55 BA 40 00 00 00 48 89 E5 41 55 41 89 F5 41 54
		Symbol::FromSignature("\x55\x8B\xEC\x56\x6A\x40\x68****\x68****"), // 55 8B EC 56 6A 40 68 ?? ?? ?? ?? 68 ?? ?? ?? ??
		Symbol::FromSignature("\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x40\x0F\xB6\xFA\x48\x8B\xD9"), // 48 89 5C 24 08 57 48 83 EC 40 0F B6 FA 48 8B D9
	};

	// Since we don't have s_NetChannels we gotta do this shit.
	// Would probably be useful to ask for it to be exposed but I don't want to wait 2+ years
	const std::vector<Symbol> CNetChan_ConstructorSym = {
		Symbol::FromName("_ZN8CNetChanC2Ev"),
		Symbol::FromSignature("\x55\x48\x8D\x05****\x48\x89\xE5\x41\x56\x41\x55\x4C\x8D\x6F\x28\x41\x54"), // 55 48 8D 05 ?? ?? ?? ?? 48 89 E5 41 56 41 55 4C 8D 6F 28 41 54
	};

	const std::vector<Symbol> CNetChan_DeconstructorSym = {
		Symbol::FromName("_ZN8CNetChanD2Ev"),
		Symbol::FromSignature("\x55\x48\x8D\x05****\x48\x8D\x35****\x48\x89\xE5\x41\x55\x41\x54\x49\x89\xFC"), // 55 48 8D 05 ?? ?? ?? ?? 48 8D 35 ?? ?? ?? ?? 48 89 E5 41 55 41 54 49 89 FC
	};

	// Yay commands :3
	// Becomes unessesary with https://github.com/Facepunch/garrysmod-requests/issues/2974
	const std::vector<Symbol> writeipSym = {
		Symbol::FromName("_ZL7writeipRK8CCommand"),
		Symbol::FromSignature("\x55\x48\x8D\x35****\xBA\x04\x01\x00\x00\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x4C\x8D\xA5\xC0\xFE\xFF\xFF\x53\x4C\x89\xE7\x48\x81\xEC\x18\x01\x00\x00\xE8****\x31\xC0\x4C\x89\xE6\x48\x8D\x3D****\xE8****\x4C\x8D\x2D****"), // 55 48 8D 35 ?? ?? ?? ?? BA 04 01 00 00 48 89 E5 41 57 41 56 41 55 41 54 4C 8D A5 C0 FE FF FF 53 4C 89 E7 48 81 EC 18 01 00 00 E8 ?? ?? ?? ?? 31 C0 4C 89 E6 48 8D 3D ?? ?? ?? ?? E8 ?? ?? ?? ?? 4C 8D 2D ?? ?? ?? ??
	};

	const std::vector<Symbol> listipSym = {
		Symbol::FromName("_ZL6listipRK8CCommand"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x83\xEC\x08\x8B\x1D****"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC 08 8B 1D ?? ?? ?? ??
	};

	const std::vector<Symbol> removeipSym = {
		Symbol::FromName("_ZL8removeipRK8CCommand"),
		Symbol::FromSignature("\x8B\x07\x85\xC0\x0F\x8E\x26\x03\x00\x00\x55\x48\x89\xE5\x41\x56"), // 8B 07 85 C0 0F 8E 26 03 00 00 55 48 89 E5 41 56
	};

	const std::vector<Symbol> Filter_Add_fSym = {
		Symbol::FromName("_ZL12Filter_Add_fRK8CCommand"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x81\xEC\x28\x01\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 89 FB 48 81 EC 28 01 00 00
	};

	//---------------------------------------------------------------------------------
	// Purpose: networkingreplacement Symbols
	// NOTE: Way to complex for windows
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> SV_EnsureInstanceBaselineSym = {
		Symbol::FromName("_Z25SV_EnsureInstanceBaselineP11ServerClassiPKvi"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x49\x89\xD5\x41\x54\x41\x89\xCC"), // 55 48 89 E5 41 57 41 56 41 55 49 89 D5 41 54 41 89 CC
	};

	const std::vector<Symbol> SendTable_EncodeSym = {
		Symbol::FromName("_Z16SendTable_EncodePK9SendTablePKvP8bf_writeiP10CUtlMemoryI20CSendProxyRecipientsiEb"),
		Symbol::FromSignature("\x55\x66\x49\x0F\x6E\xC8\x48\x89\xE5\x41\x57\x41\x56\x45\x89\xCE"), // 55 66 49 0F 6E C8 48 89 E5 41 57 41 56 45 89 CE
	};

	const std::vector<Symbol> CFrameSnapshotManager_GetPreviouslySentPacketSym = {
		Symbol::FromName("_ZN21CFrameSnapshotManager23GetPreviouslySentPacketEii"),
		Symbol::FromSignature("\x48\x63\xF6\x31\xC0\x48\x8B\x8C\xF7\xA8\x00\x00\x00\x48\x85\xC9"), // 48 63 F6 31 C0 48 8B 8C F7 A8 00 00 00 48 85 C9
	};

	const std::vector<Symbol> CFrameSnapshotManager_UsePreviouslySentPacketSym = {
		Symbol::FromName("_ZN21CFrameSnapshotManager23UsePreviouslySentPacketEP14CFrameSnapshotii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x54\x53\x48\x63\xDA\x48\x83\xEC\x10\x4C\x8B\xA4\xDF\xA8\x00\x00\x00"), // 55 48 89 E5 41 54 53 48 63 DA 48 83 EC 10 4C 8B A4 DF A8 00 00 00
	};

	const std::vector<Symbol> CFrameSnapshotManager_CreatePackedEntitySym = {
		Symbol::FromName("_ZN21CFrameSnapshotManager18CreatePackedEntityEP14CFrameSnapshoti"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x4C\x8D\xBF\xA8\x80\x01\x00\x49\x89\xF6"), // 55 48 89 E5 41 57 41 56 4C 8D BF A8 80 01 00 49 89 F6
	};

	//---------------------------------------------------------------------------------
	// Purpose: nw2 Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> GMODTable_EncodeSym = {
		Symbol::FromName("_Z16GMODTable_EncodePKhP8DVariantPK8SendPropP8bf_writei"),
		Symbol::FromSignature("\x55\x48\x89\xCA\x48\x89\xE5\x41\x54\x53\x48\x89\xFB\x48\x83\xEC\x10"), // 55 48 89 CA 48 89 E5 41 54 53 48 89 FB 48 83 EC 10
	};

	// Client engine only (nw2 baseline decode fix). Linux signatures are valid for engine.so and engine_srv.so.
	const std::vector<Symbol> RecvTable_DecodeSym = { // The only caller passing updateDTI = false is CL_CopyNewEntity (baseline copy)
		Symbol::FromName("_Z16RecvTable_DecodeP9RecvTablePvP7bf_readib"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x49\x89\xF7\x41\x56\x41\x55\x41\x54\x45\x89\xC4"), // 55 48 89 E5 41 57 49 89 F7 41 56 41 55 41 54 45 89 C4
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x64\x03\x00\x00\x8B\x4D\x08\x53\x56"), // 55 8B EC 81 EC 64 03 00 00 8B 4D 08 53 56
		Symbol::FromSignature("\x44\x89\x4C\x24\x20\x55\x53\x56\x48\x8D\xAC\x24\x30\xFC\xFF\xFF"), // 44 89 4C 24 20 55 53 56 48 8D AC 24 30 FC FF FF
	};

	const std::vector<Symbol> GMODTable_DecodeSym = { // Property proxy of the GMODTable recv prop, calls CGMODDataTable::Decode or Skip
		Symbol::FromName("_Z16GMODTable_DecodeP10DecodeInfo"),
		Symbol::FromSignature("\x48\x8B\x47\x30\x48\x85\xC0\x74\x08\x48\x8B\x00\x48\x85\xC0\x75\x0F"), // 48 8B 47 30 48 85 C0 74 08 48 8B 00 48 85 C0 75 0F
		Symbol::FromSignature("\x55\x8B\xEC\x8B\x45\x08\x8B\x48\x20\x85\xC9\x74*\x8B\x09"), // 55 8B EC 8B 45 08 8B 48 20 85 C9 74 ?? 8B 09
		Symbol::FromSignature("\x48\x8B\x41\x30\x48\x85\xC0\x74*\x4C\x8B\x08\x4D\x85\xC9"), // 48 8B 41 30 48 85 C0 74 ?? 4C 8B 08 4D 85 C9
	};

	const std::vector<Symbol> CGMODDataTable_SkipSym = { // Reads the 12 bit count + full update bit and skips all entries
		Symbol::FromName("_ZN14CGMODDataTable4SkipEP7bf_read"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x53\x8B\x77\x0C\x48\x89\xFB"), // 55 48 89 E5 41 56 41 55 41 54 53 8B 77 0C 48 89 FB
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x08\x53\x56\x8B\x75\x08\x57\x8B\x4E\x08"), // 55 8B EC 83 EC 08 53 56 8B 75 08 57 8B 4E 08
		Symbol::FromSignature("\x48\x89\x5C\x24\x10\x56\x48\x83\xEC\x20\x48\x8B\xD9\x8B\x49\x0C"), // 48 89 5C 24 10 56 48 83 EC 20 48 8B D9 8B 49 0C
	};

	//---------------------------------------------------------------------------------
	// Purpose: crashhandler Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CTextConsoleUnix_GetLineSym = { // Search for usage of "sigprocmask", it polls stdin with kbhit and returns the console text buffer once a line was entered
		Symbol::FromName("_ZN16CTextConsoleUnix7GetLineEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x53\x48\x89\xFB\x48\x81\xEC\x90\x00\x00\x00"), // 55 48 89 E5 41 56 41 55 41 54 53 48 89 FB 48 81 EC 90 00 00 00
	};

	//---------------------------------------------------------------------------------
	// Purpose: gmoddatapack Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> GModDataPack_SendFileToClientSym = { // "Client requesting crazy file number (%i)\n"
		Symbol::FromName("_ZN12GModDataPack16SendFileToClientEii"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x89\xD3\x48\x81\xEC\x88\x00\x00\x00"), // 55 48 89 E5 41 57 41 56 41 55 41 54 53 89 D3 48 81 EC 88 00 00 00
		Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x60\x53\x8B\x5D\x0C\x56\x8B\xF1\x89\x75\xF8"),
		Symbol::FromName("?SendFileToClient@GModDataPack@@QEAAXHH@Z"),
	};

	const std::vector<Symbol> GModDataPack_AddOrUpdateFileSym = { // search for singleplayer_files%i
		Symbol::FromName("_ZN12GModDataPack15AddOrUpdateFileEP7LuaFileb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x89\xD5\x41\x54\x48\x8D\x15****"), // 55 48 89 E5 41 57 41 56 41 55 41 89 D5 41 54 48 8D 15 ?? ?? ?? ??
		Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x90\x00\x00\x00\x53\x8B\xD9\x56\x57"),
		Symbol::FromName("?AddOrUpdateFile@GModDataPack@@QEAAXPEAULuaFile@@_N@Z"),
	};

	//---------------------------------------------------------------------------------
	// Purpose: httpserver Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CRConServer_CreateSocketSym = { // Find "Network: IP" then check the last function in the if branch before the print
		Symbol::FromName("_ZN11CRConServer12CreateSocketEv"),
		Symbol::FromSignature("\x48\x8D\x77\x70\x48\x83\xC7\x08\xE9****\x90\x66\x90\x55\x48\x89\xE5"), // 48 8D 77 70 48 83 C7 08 E9 ?? ?? ?? ?? 90 66 90 55 48 89 E5
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x8D\x51\x70\x48\x83\xC1\x08\xE9****\xCC"), // 48 8D 51 70 48 83 C1 08 E9 ?? ?? ?? ?? CC
	};

	//---------------------------------------------------------------------------------
	// Purpose: networkdebugging Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> CNetChan_SendNetMsgSym = { // Search for "55 48 89 E5 41 57 49 89 F7 41 56 41 55 41 54 53 48 89 FB 48 81 EC E8 00 04 00" then find the vtable of CNetChan and go up 6 to find "sub_153BA0"
		Symbol::FromName("_ZN8CNetChan10SendNetMsgER11INetMessagebb"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x41\x56\x41\x89\xD6\x41\x55\x41\x89\xCD\x41\x54"), // 55 48 89 E5 41 56 41 89 D6 41 55 41 89 CD 41 54
		NULL_SIGNATURE,
		Symbol::FromSignature("\x48\x89\x5C\x24\x10\x48\x89\x6C\x24\x18\x48\x89\x7C\x24\x20\x41\x56\x48\x83\xEC\x20\x48\x8B\xF9"), // 48 89 5C 24 10 48 89 6C 24 18 48 89 7C 24 20 41 56 48 83 EC 20 48 8B F9
	};

	//---------------------------------------------------------------------------------
	// Purpose: unholylib Symbols
	//---------------------------------------------------------------------------------
	const std::vector<Symbol> Lua_KillSym = {
		Symbol::FromName("_ZN3Lua4KillEv"),
		Symbol::FromSignature("\x55\x48\x89\xE5\x53\x48\x83\xEC\x08\x48\x83\x3D****\x00\x74\x5F\xE8****"), // 55 48 89 E5 53 48 83 EC 08 48 83 3D ?? ?? ?? ?? 00 74 5F E8 ?? ?? ?? ??
	};
}