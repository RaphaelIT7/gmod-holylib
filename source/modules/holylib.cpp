#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include <GarrysMod/InterfacePointers.hpp>
#include "baseserver.h"
#include "util.h"
#include "player.h"
#include "iclient.h"
#include "netmessages.h"
#include "net.h"
#include "sourcesdk/baseclient.h"
#include "hl2/hl2_player.h"
#include "unordered_set"
#include "host_state.h"
#include "detouring/customclassproxy.hpp"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CHolyLibModule : public IModule
{
public:
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void InitDetour(bool bPreServer) override;
	void LevelShutdown() override;
	const char* Name() override { return "holylib"; };
	int Compatibility() override { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
	bool SupportsMultipleLuaStates() override { return true; };
};

static CHolyLibModule g_pHolyLibModule;
IModule* pHolyLibModule = &g_pHolyLibModule;

LUA_FUNCTION_STATIC(Reconnect)
{
	CBasePlayer* ent = Util::Get_Player(LUA, 1, true);
	if (!ent)
		LUA->ArgError(1, "Tried to use a NULL player!");

	CBaseClient* client = Util::GetClientByIndex(ent->GetClientIndex());
	if (client->GetNetChannel()) {
		client->Reconnect();
		LUA->PushBool(true);
	} else {
		LUA->PushBool(false);
	}

	return 1;
}

LUA_FUNCTION_STATIC(HideServer)
{
	LUA->CheckType(1, GarrysMod::Lua::Type::Bool);

	ConVarRef hide_server("hide_server");
	if (hide_server.IsValid())
		hide_server.SetValue(LUA->GetBool(1));
	else
		LUA->ThrowError("Failed to find hide_server?");

	return 0;
}

LUA_FUNCTION_STATIC(FadeClientVolume)
{
	CBasePlayer* ent = Util::Get_Player(LUA, 1, true);
	edict_t* pEdict = ent->edict();
	if (!pEdict)
		LUA->ArgError(1, "Failed to get edict?");

	float fadePercent = LUA->CheckNumber(2);
	float fadeOutSeconds = LUA->CheckNumber(3);
	float holdTime = LUA->CheckNumber(4);
	float fadeInSeconds = LUA->CheckNumber(5);

	// It basically just runs a command clientside.
	Util::engineserver->FadeClientVolume(pEdict, fadePercent, fadeOutSeconds, holdTime, fadeInSeconds);

	return 1;
}

LUA_FUNCTION_STATIC(ServerExecute)
{
	VPROF_BUDGET("HolyLib(Lua) - HolyLib.ServerExecute", VPROF_BUDGETGROUP_HOLYLIB);
	Util::engineserver->ServerExecute();

	return 0;
}

LUA_FUNCTION_STATIC(IsMapValid)
{
	const char* pMapName = LUA->CheckString(1);

	LUA->PushBool(Util::engineserver->IsMapValid(pMapName));
	return 1;
}

extern bf_write* GetActiveMessage();
LUA_FUNCTION_STATIC(_EntityMessageBegin)
{
	CBaseEntity* pEnt = Util::Get_Entity(LUA, 1, true);
	bool bReliable = LUA->GetBool(2);

#if MODULE_EXISTS_BITBUF
	EntityMessageBegin(pEnt, bReliable);
	Push_bf_write(LUA, GetActiveMessage(), false);
#else
	MISSING_MODULE_ERROR(LUA, bitbuf);
#endif
	return 1;
}

LUA_FUNCTION_STATIC(_UserMessageBegin)
{
	IRecipientFilter* pFilter = Get_IRecipientFilter(LUA, 1, true);
	const char* pName = LUA->CheckString(2);

#if MODULE_EXISTS_BITBUF
	UserMessageBegin(*pFilter, pName);
	Push_bf_write(LUA, GetActiveMessage(), false);
#else
	MISSING_MODULE_ERROR(LUA, bitbuf);
#endif
	return 1;
}

LUA_FUNCTION_STATIC(_MessageEnd)
{
	MessageEnd();
	return 0;
}

static Detouring::Hook detour_GetGModServerTags;
static void hook_GetGModServerTags(char* pDest, int iMaxLength, bool bUnknown)
{
	if (Lua::PushHook("HolyLib:GetGModTags"))
	{
		if (g_Lua->CallFunctionProtected(1, 1, true))
		{
			if (g_Lua->IsType(-1, GarrysMod::Lua::Type::String))
			{
				const char* pTags = g_Lua->GetString(-1);

				V_strncpy(pDest, pTags, iMaxLength);
			} else {
				detour_GetGModServerTags.GetTrampoline<Symbols::GetGModServerTags>()(pDest, iMaxLength, bUnknown);
			}

			g_Lua->Pop(1);
			return;
		}
	}

	detour_GetGModServerTags.GetTrampoline<Symbols::GetGModServerTags>()(pDest, iMaxLength, bUnknown);
}

static Symbols::CBaseAnimating_InvalidateBoneCache func_CBaseAnimating_InvalidateBoneCache;
LUA_FUNCTION_STATIC(InvalidateBoneCache)
{
	CBaseEntity* pEnt = Util::Get_Entity(LUA, 1, true);

	if (!func_CBaseAnimating_InvalidateBoneCache)
		LUA->ThrowError("Failed to get CBaseAnimating::InvalidateBoneCache");

	CBaseAnimating* pAnimating = pEnt->GetBaseAnimating();
	if (!pAnimating)
		LUA->ArgError(1, "Tried use use an Entity that isn't a CBaseAnimating");

	func_CBaseAnimating_InvalidateBoneCache(pAnimating);
	return 0;
}

// NOTE: This assumes that the field m_iEFlags is directly after the m_spawnflags field!
static DTVarByOffset m_spawnflags_Offset("DT_BaseEntity", "m_spawnflags");
static inline int* GetEntityEFlags(const void* pEnt)
{
	return (int*)((char*)m_spawnflags_Offset.GetPointer(pEnt) + sizeof(int));
}

static Detouring::Hook detour_CBaseEntity_PostConstructor;
static void hook_CBaseEntity_PostConstructor(CBaseEntity* pEnt, const char* szClassname)
{
	if (Lua::PushHook("HolyLib:PostEntityConstructor"))
	{
		// Util::Push_Entity(g_Lua, pEnt); // Broken since Lua sees it as NULL
		g_Lua->PushString(szClassname);
		if (g_Lua->CallFunctionProtected(2, 1, true))
		{
			bool bMakeServerOnly = g_Lua->GetBool(-1);
			g_Lua->Pop(1);

			if (bMakeServerOnly)
				*GetEntityEFlags(pEnt) |= EFL_SERVER_ONLY;
		}

		/*g_Lua->PushUserType(pEnt, GarrysMod::Lua::Type::Entity);
		g_Lua->Push(-1);
		int iReference = Util::ReferenceCreate();
		g_Lua->PushString(szClassname);

		g_Lua->CallFunctionProtected(3, 0, true);

		Util::ReferencePush(iReference);
		Util::ReferenceFree(iReference);
		g_Lua->SetUserType(-1, nullptr);
		g_Lua->Pop(1)*/
	}

	detour_CBaseEntity_PostConstructor.GetTrampoline<Symbols::CBaseEntity_PostConstructor>()(pEnt, szClassname);
}

LUA_FUNCTION_STATIC(SetSignOnState)
{
	CBaseClient* pClient = nullptr;
	if (LUA->IsType(1, GarrysMod::Lua::Type::Entity))
	{
		pClient = Util::GetClientByPlayer(Util::Get_Player(LUA, 1, true));
	} else {
		pClient = Util::GetClientByUserID(LUA->CheckNumber(1));
	}

	int iSignOnState = LUA->CheckNumber(2);
	int iSpawnCount = LUA->GetNumber(3);
	bool bRawSet = LUA->GetBool(4);

	if (!pClient)
	{
		LUA->PushBool(false);
		return 1;
	}

	if (bRawSet)
	{
		pClient->m_nSignonState = iSignOnState;
		LUA->PushBool(true);
		return 1;
	}

	LUA->PushBool(pClient->SetSignonState(iSignOnState, iSpawnCount));
	return 1;
}

static Detouring::Hook detour_CFuncLadder_PlayerGotOn;
static void hook_CFuncLadder_PlayerGotOn(CBaseEntity* pLadder, CBasePlayer* pPly)
{
	if (Lua::PushHook("HolyLib:OnPlayerGotOnLadder"))
	{
		Util::Push_Entity(g_Lua, pLadder);
		Util::Push_Entity(g_Lua, pPly);
		g_Lua->CallFunctionProtected(3, 0, true);
	}

	detour_CFuncLadder_PlayerGotOn.GetTrampoline<Symbols::CFuncLadder_PlayerGotOn>()(pLadder, pPly);
}

static Detouring::Hook detour_CFuncLadder_PlayerGotOff;
static void hook_CFuncLadder_PlayerGotOff(CBaseEntity* pLadder, CBasePlayer* pPly)
{
	if (Lua::PushHook("HolyLib:OnPlayerGotOffLadder"))
	{
		Util::Push_Entity(g_Lua, pLadder);
		Util::Push_Entity(g_Lua, pPly);
		g_Lua->CallFunctionProtected(3, 0, true);
	}

	detour_CFuncLadder_PlayerGotOff.GetTrampoline<Symbols::CFuncLadder_PlayerGotOff>()(pLadder, pPly);
}

static Symbols::CHL2_Player_ExitLadder func_CHL2_Player_ExitLadder;
LUA_FUNCTION_STATIC(ExitLadder)
{
	CBasePlayer* pPly = Util::Get_Player(LUA, 1, true);

	if (!func_CHL2_Player_ExitLadder)
		LUA->ThrowError("Failed to get CHL2_Player::ExitLadder");

	func_CHL2_Player_ExitLadder(pPly);
	return 0;
}

static DTVarByOffset m_hLadder_Offset("DT_HL2Local", "m_hLadder");
static DTVarByOffset m_HL2Local_Offset("DT_HL2_Player", "m_HL2Local");
static inline CBaseEntity* GetLadder(void* pPlayer)
{
	void* pHL2Local = m_HL2Local_Offset.GetPointer(pPlayer);
	if (!pHL2Local)
		return nullptr;

	void* pLadder = m_hLadder_Offset.GetPointer(pHL2Local);
	if (!pLadder)
		return nullptr;

	if (!g_pEntityList)
		return Util::GetCBaseEntityFromIndex(((EHANDLE*)pLadder)->GetEntryIndex());

	return ((EHANDLE*)pLadder)->Get();
}

LUA_FUNCTION_STATIC(GetLadder)
{
	CHL2_Player* pPly = (CHL2_Player*)Util::Get_Player(LUA, 1, true);

	Util::Push_Entity(LUA, GetLadder(pPly));
	return 1;
}

static bool bInMoveTypeCall = false; // If someone calls SetMoveType inside the hook, we don't want a black hole to form.
static Detouring::Hook detour_CBaseEntity_SetMoveType;
static void hook_CBaseEntity_SetMoveType(CBaseEntity* pEnt, int iMoveType, int iMoveCollide)
{
	int iCurrentMoveType = pEnt->GetMoveType();
	if (!bInMoveTypeCall && iCurrentMoveType != iMoveType && Lua::PushHook("HolyLib:OnMoveTypeChange"))
	{
		// Uncomment the code below to see if the entity is valid. GetClassname should almost always return a valid class
		// Msg("hook_CBaseEntity_SetMoveType: %p - %s\n", pEnt, pEnt->GetClassname());
		Util::Push_Entity(g_Lua, pEnt);
		g_Lua->PushNumber(iCurrentMoveType);
		g_Lua->PushNumber(iMoveType);
		g_Lua->PushNumber(iMoveCollide);
		bInMoveTypeCall = true;
		g_Lua->CallFunctionProtected(5, 0, true);
		bInMoveTypeCall = false;
	}

	detour_CBaseEntity_SetMoveType.GetTrampoline<Symbols::CBaseEntity_SetMoveType>()(pEnt, iMoveType, iMoveCollide);
}

static std::unordered_set<std::string> g_pHideMsg;
LUA_FUNCTION_STATIC(HideMsg) // ToDo: Final logic is still missing.
{
	std::string pRegex = LUA->CheckString(1);
	bool bRemove = LUA->GetBool(2);

	auto it = g_pHideMsg.find(pRegex);
	if (it != g_pHideMsg.end())
	{
		if (bRemove)
			g_pHideMsg.erase(it);
	} else {
		g_pHideMsg.insert(pRegex);
	}

	return 0;
}

LUA_FUNCTION_STATIC(GetRegistry)
{
	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_REG);
	return 1;
}

LUA_FUNCTION_STATIC(Disconnect)
{
	CBaseClient* pClient = nullptr;
	if (LUA->IsType(1, GarrysMod::Lua::Type::Entity))
	{
		pClient = Util::GetClientByPlayer(Util::Get_Player(LUA, 1, true));
	} else {
		pClient = Util::GetClientByUserID(LUA->CheckNumber(1));
	}

	const char* strReason = LUA->CheckString(2);
	bool bSilent = LUA->GetBool(3);
	bool bNoEvent = LUA->GetBool(4);

	if (!pClient)
	{
		LUA->PushBool(false);
		return 1;
	}

	if (bSilent)
		pClient->GetNetChannel()->Shutdown(nullptr); // nullptr = Send no disconnect message

	if (bNoEvent)
		Util::BlockGameEvent("player_disconnect");

	pClient->Disconnect(strReason);

	if (bNoEvent)
		Util::UnblockGameEvent("player_disconnect");

	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION_STATIC(ReceiveClientMessage)
{
#if MODULE_EXISTS_BITBUF
	int userID = LUA->CheckNumber(1);
	CBaseEntity* pEnt = Util::Get_Entity(LUA, 2, true);
	bf_read* msg = Get_bf_read(LUA, 3, true);
	int bits = LUA->CheckNumber(4);

	Util::servergameclients->GMOD_ReceiveClientMessage(userID, pEnt->edict(), msg, bits);
#else
	MISSING_MODULE_ERROR(LUA, bitbuf);
#endif
	return 0;
}

static char pLevelName[256], pLandmarkName[256] = {0};
static Detouring::Hook detour_CHostState_State_ChangeLevelMP;
static void hook_CHostState_State_ChangeLevelMP(const char* levelName, const char* landmarkName)
{
	if (levelName) 
	{
		V_strncpy(pLevelName, levelName, sizeof(pLevelName));
	}

	if (landmarkName)
	{
		V_strncpy(pLandmarkName, landmarkName, sizeof(pLandmarkName));
	} else {
		pLandmarkName[0] = '\0';
	}

	detour_CHostState_State_ChangeLevelMP.GetTrampoline<Symbols::CHostState_State_ChangeLevelMP>()(levelName, landmarkName);
}

void CHolyLibModule::LevelShutdown()
{
	if (*pLevelName == '\0') {
		return;
	}

	if (Lua::PushHook("HolyLib:OnMapChange"))
	{
		g_Lua->PushString(pLevelName);
		g_Lua->PushString(pLandmarkName);
		g_Lua->CallFunctionProtected(3, 0, true);
	}

	pLevelName[0] = '\0';
	pLandmarkName[0] = '\0';
}

static Detouring::Hook detour_CLuaInterface_RunStringEx;
static bool hook_CLuaInterface_RunStringEx(GarrysMod::Lua::ILuaInterface* pLua, const char *filename, const char *path, const char *stringToRun, bool run, bool printErrors, bool dontPushErrors, bool noReturns)
{
	if (Lua::PushHook("HolyLib:OnLuaRunString", pLua))
	{
		pLua->PushString(stringToRun);
		pLua->PushString(filename);
		pLua->PushString(path);
		if (pLua->CallFunctionProtected(4, 1, true))
		{
			int nType = pLua->GetType(-1);
			if (nType == GarrysMod::Lua::Type::String)
			{
				// We do it like this to avoid having to copy/allocate a space to store the string in as popping it and then using it is unsafe as the GC could have freed it.
				const char* pCode = pLua->GetString(-1);
				bool bRet = detour_CLuaInterface_RunStringEx.GetTrampoline<Symbols::CLuaInterface_RunStringEx>()(pLua, filename, path, pCode, run, printErrors, dontPushErrors, noReturns);
				pLua->Pop(1);
				return bRet;
			} else if (nType == GarrysMod::Lua::Type::Bool) {
				bool bRet = pLua->GetBool(-1);
				pLua->Pop(1);
				return bRet;
			}

			pLua->Pop(1);
		}
	}

	return detour_CLuaInterface_RunStringEx.GetTrampoline<Symbols::CLuaInterface_RunStringEx>()(pLua, filename, path, stringToRun, run, printErrors, dontPushErrors, noReturns);;
}

void CHolyLibModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (!bServerInit)
	{
		Util::StartTable(pLua);
			Util::AddFunc(pLua, HideServer, "HideServer");
			Util::AddFunc(pLua, Reconnect, "Reconnect");
			Util::AddFunc(pLua, FadeClientVolume, "FadeClientVolume");
			Util::AddFunc(pLua, ServerExecute, "ServerExecute");
			Util::AddFunc(pLua, IsMapValid, "IsMapValid");
			Util::AddFunc(pLua, InvalidateBoneCache, "InvalidateBoneCache");
			Util::AddFunc(pLua, SetSignOnState, "SetSignOnState");
			Util::AddFunc(pLua, ExitLadder, "ExitLadder");
			Util::AddFunc(pLua, GetLadder, "GetLadder");
			Util::AddFunc(pLua, HideMsg, "HideMsg");
			Util::AddFunc(pLua, GetRegistry, "GetRegistry");
			Util::AddFunc(pLua, Disconnect, "Disconnect");

			// Networking stuff
			Util::AddFunc(pLua, _EntityMessageBegin, "EntityMessageBegin");
			Util::AddFunc(pLua, _UserMessageBegin, "UserMessageBegin");
			Util::AddFunc(pLua, _MessageEnd, "MessageEnd");
			Util::AddFunc(pLua, ReceiveClientMessage, "ReceiveClientMessage");
		Util::FinishTable(pLua, "HolyLib");
	} else {
		if (Lua::PushHook("HolyLib:Initialize", pLua))
		{
			pLua->CallFunctionProtected(1, 0, true);
		} else {
			DevMsg(1, PROJECT_NAME ": Failed to call HolyLib:Initialize!\n");
		}
	}
}

void CHolyLibModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "holylib");
}

#if SYSTEM_WINDOWS
DETOUR_THISCALL_START()
	DETOUR_THISCALL_ADDFUNC1( hook_CBaseEntity_PostConstructor, PostConstructor, CBaseEntity*, const char* );
	DETOUR_THISCALL_ADDFUNC1( hook_CFuncLadder_PlayerGotOn, PlayerGotOn, CBaseEntity*, CBasePlayer* );
	DETOUR_THISCALL_ADDFUNC1( hook_CFuncLadder_PlayerGotOff, PlayerGotOff, CBaseEntity*, CBasePlayer* );
	DETOUR_THISCALL_ADDFUNC2( hook_CBaseEntity_SetMoveType, SetMoveType, CBaseEntity*, int, int );
	DETOUR_THISCALL_ADDRETFUNC7( hook_CLuaInterface_RunStringEx, bool, RunStringEx, GarrysMod::Lua::ILuaInterface*, const char*, const char*, const char*, bool, bool, bool, bool );
DETOUR_THISCALL_FINISH();
#endif

void CHolyLibModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	DETOUR_PREPARE_THISCALL();
	SourceSDK::ModuleLoader server_loader("server");
	Detour::Create(
		&detour_CBaseEntity_PostConstructor, "CBaseEntity::PostConstructor",
		server_loader.GetModule(), Symbols::CBaseEntity_PostConstructorSym,
		(void*)DETOUR_THISCALL(hook_CBaseEntity_PostConstructor, PostConstructor), m_pID
	);

	Detour::Create(
		&detour_CFuncLadder_PlayerGotOn, "CFuncLadder::PlayerGotOn",
		server_loader.GetModule(), Symbols::CFuncLadder_PlayerGotOnSym,
		(void*)DETOUR_THISCALL(hook_CFuncLadder_PlayerGotOn, PlayerGotOn), m_pID
	);

	Detour::Create(
		&detour_CFuncLadder_PlayerGotOff, "CFuncLadder::PlayerGotOff",
		server_loader.GetModule(), Symbols::CFuncLadder_PlayerGotOffSym,
		(void*)DETOUR_THISCALL(hook_CFuncLadder_PlayerGotOff, PlayerGotOff), m_pID
	);

	Detour::Create(
		&detour_CBaseEntity_SetMoveType, "CBaseEntity::SetMoveType",
		server_loader.GetModule(), Symbols::CBaseEntity_SetMoveTypeSym,
		(void*)DETOUR_THISCALL(hook_CBaseEntity_SetMoveType, SetMoveType), m_pID
	);

	SourceSDK::ModuleLoader engine_loader("engine");
	Detour::Create(
		&detour_GetGModServerTags, "GetGModServerTags",
		engine_loader.GetModule(), Symbols::GetGModServerTagsSym,
		(void*)hook_GetGModServerTags, m_pID
	);

#if ARCHITECTURE_IS_X86
	Detour::Create(
		&detour_CHostState_State_ChangeLevelMP, "CHostState_State_ChangeLevelMP",
		engine_loader.GetModule(), Symbols::CHostState_State_ChangeLevelMPSym,
		(void*)hook_CHostState_State_ChangeLevelMP, m_pID
	);
#endif

	SourceSDK::ModuleLoader lua_loader("lua_shared");
	Detour::Create(
		&detour_CLuaInterface_RunStringEx, "CLuaInterface::RunStringEx",
		lua_loader.GetModule(), Symbols::CLuaInterface_RunStringExSym,
		(void*)DETOUR_THISCALL(hook_CLuaInterface_RunStringEx, RunStringEx), m_pID
	);

	func_CBaseAnimating_InvalidateBoneCache = (Symbols::CBaseAnimating_InvalidateBoneCache)Detour::GetFunction(server_loader.GetModule(), Symbols::CBaseAnimating_InvalidateBoneCacheSym);
	Detour::CheckFunction((void*)func_CBaseAnimating_InvalidateBoneCache, "CBaseAnimating::InvalidateBoneCache");

	func_CHL2_Player_ExitLadder = (Symbols::CHL2_Player_ExitLadder)Detour::GetFunction(server_loader.GetModule(), Symbols::CHL2_Player_ExitLadderSym);
	Detour::CheckFunction((void*)func_CHL2_Player_ExitLadder, "CHL2_Player::ExitLadder");
}