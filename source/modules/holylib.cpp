#include "LuaInterface.h"
#include "symbols.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include <GarrysMod/InterfacePointers.hpp>
#include "iserver.h"
#include "util.h"
#include "player.h"
#include "iclient.h"
#include "netmessages.h"
#include "net.h"
#include "sourcesdk/baseclient.h"
#include "hl2/hl2_player.h"
#include "unordered_set"

class CHolyLibModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "holylib"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; };
};

static CHolyLibModule g_pHolyLibModule;
IModule* pHolyLibModule = &g_pHolyLibModule;

class SVC_CustomMessage: public CNetMessage
{
public:
	bool			ReadFromBuffer( bf_read &buffer ) { return true; };
	bool			WriteToBuffer( bf_write &buffer ) {
		buffer.WriteUBitLong(GetType(), NETMSG_TYPE_BITS);
		return buffer.WriteBits(m_DataOut.GetData(), m_DataOut.GetNumBitsWritten());
	};
	const char		*ToString() const { return "HolyLib:CustomMessage"; };
	int				GetType() const { return m_iType; }
	const char		*GetName() const { return m_strName;}

	INetMessageHandler *m_pMessageHandler = NULL;
	bool Process() { Warning("holylib: Tried to process this message? This should never happen!\n"); return true; };

	SVC_CustomMessage() { m_bReliable = false; }

	int	GetGroup() const { return INetChannelInfo::GENERIC; }

	int m_iType = 0;
	char m_strName[64] = "";
	bf_write m_DataOut;
};

LUA_FUNCTION_STATIC(Reconnect)
{
	CBasePlayer* ent = Util::Get_Player(LUA, 1, true);
	if (!ent)
		LUA->ArgError(1, "Tried to use a NULL player!");

	IClient* client = Util::server->GetClient(ent->GetClientIndex());
	if (!client->IsFakeClient()) { // ToDo: Verify that this 100% works. If it can crash here, we add a workaround.
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
		hide_server.SetValue(LUA->GetBool(1)); // Remove our hook with the next gmod update. I should start to reduce the amount of Symbols I use.
	else
		LUA->ThrowError("Failed to find hide_server?");

	return 0;
}

LUA_FUNCTION_STATIC(FadeClientVolume)
{
	CBasePlayer* ent = Util::Get_Player(LUA, 1, true);
	if (!ent)
		LUA->ArgError(1, "Tried to use a NULL player!");

	edict_t* pEdict = ent->edict();
	if (!pEdict)
		LUA->ThrowError("How....");

	float fadePercent = LUA->CheckNumber(2);
	float fadeOutSeconds = LUA->CheckNumber(3);
	float holdTime = LUA->CheckNumber(4);
	float fadeInSeconds = LUA->CheckNumber(5);

	// It basicly just runs a command clientside.
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

static IModuleWrapper* pBitBufWrapper = NULL;
extern bf_write* GetActiveMessage();
LUA_FUNCTION_STATIC(_EntityMessageBegin)
{
	CBaseEntity* pEnt = Util::Get_Entity(LUA, 1, true);
	bool bReliable = LUA->GetBool(2);

	if (!pBitBufWrapper->IsEnabled())
		LUA->ThrowError("This won't work when the bitbuf library is disabled!");

	EntityMessageBegin(pEnt, bReliable);
	Push_bf_write(GetActiveMessage());
	return 1;
}

LUA_FUNCTION_STATIC(_UserMessageBegin)
{
	IRecipientFilter* pFilter = Get_IRecipientFilter(1, true);
	const char* pName = LUA->CheckString(2);

	if (!pBitBufWrapper->IsEnabled())
		LUA->ThrowError("This won't work when the bitbuf library is disabled!");

	UserMessageBegin(*pFilter, pName);
	Push_bf_write(GetActiveMessage());
	return 1;
}

LUA_FUNCTION_STATIC(_MessageEnd)
{
	MessageEnd();
	return 0;
}

LUA_FUNCTION_STATIC(BroadcastCustomMessage)
{
	int iType = LUA->CheckNumber(1);
	const char* strName = LUA->CheckString(2);
	bf_write* bf = Get_bf_write(3, true);

	SVC_CustomMessage msg;
	msg.m_iType = iType;
	strcpy(msg.m_strName, strName);
	msg.m_DataOut.StartWriting(bf->GetData(), 0, 0, bf->GetMaxNumBits());

	Util::server->BroadcastMessage(msg);
	return 0;
}

LUA_FUNCTION_STATIC(SendCustomMessage)
{
	int iType = LUA->CheckNumber(1);
	const char* strName = LUA->CheckString(2);
	bf_write* bf = Get_bf_write(3, true);
	IClient* pClient = NULL;
	if (LUA->IsType(4, GarrysMod::Lua::Type::Number))
	{
		pClient = Util::GetClientByUserID(LUA->GetNumber(4));
	} else {	
		CBasePlayer* ply = Util::Get_Player(LUA, 4, true);
		pClient = (IClient*)Util::GetClientByPlayer(ply);
	}

	if (!pClient)
		LUA->ThrowError("Failed to get IClient from player!");

	SVC_CustomMessage msg;
	msg.m_iType = iType;
	strcpy(msg.m_strName, strName);
	msg.m_DataOut.StartWriting(bf->GetData(), 0, 0, bf->GetMaxNumBits());

	LUA->PushBool(pClient->SendNetMsg(msg));
	return 1;
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
		LUA->ThrowError("Tried use use an Entity that isn't a CBaseAnimating");

	func_CBaseAnimating_InvalidateBoneCache(pAnimating);
	return 0;
}

static Detouring::Hook detour_CBaseEntity_PostConstructor;
static void hook_CBaseEntity_PostConstructor(CBaseEntity* pEnt, const char* szClassname)
{
	if (Lua::PushHook("HolyLib:PostEntityConstructor"))
	{
		Util::Push_Entity(g_Lua, pEnt);
		g_Lua->PushString(szClassname);
		g_Lua->CallFunctionProtected(3, 0, true);
	}

	detour_CBaseEntity_PostConstructor.GetTrampoline<Symbols::CBaseEntity_PostConstructor>()(pEnt, szClassname);
}

LUA_FUNCTION_STATIC(SetSignOnState)
{
	CBaseClient* pClient = NULL;
	if (LUA->IsType(1, GarrysMod::Lua::Type::Entity))
	{
		pClient = Util::GetClientByPlayer(Util::Get_Player(LUA, 1, true));
	} else {
		pClient = Util::GetClientByUserID(LUA->CheckNumber(1));
	}

	int iSignOnState = LUA->CheckNumber(2);
	int iSpawnCount = LUA->CheckNumber(3);

	if (!pClient)
	{
		LUA->PushBool(false);
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

class CHL2GameMovement // Workaround to make the compiler happy since were not allowed to acces it but this is a friend class.
{
public:
	static CBaseEntity* GetLadder(CHL2_Player* ply)
	{
		return ply->m_HL2Local.m_hLadder.Get().Get(); // Make m_HL2Local a public member and verify that the var offset is right on 64x.
	}
};

LUA_FUNCTION_STATIC(GetLadder)
{
	CHL2_Player* pPly = (CHL2_Player*)Util::Get_Player(LUA, 1, true);

	Util::Push_Entity(LUA, CHL2GameMovement::GetLadder(pPly));
	return 1;
}

static bool bInMoveTypeCall = false; // If someone calls SetMoveType inside the hook, we don't want a black hole to form.
static Detouring::Hook detour_CBaseEntity_SetMoveType;
static void hook_CBaseEntity_SetMoveType(CBaseEntity* pEnt, int iMoveType, int iMoveCollide)
{
	int iCurrentMoveType = pEnt->GetMoveType();
	if (!bInMoveTypeCall && iCurrentMoveType != iMoveType && Lua::PushHook("HolyLib:OnMoveTypeChange"))
	{
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

void CHolyLibModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (!bServerInit)
	{
		pBitBufWrapper = g_pModuleManager.FindModuleByName("bitbuf");

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

			// Networking stuff
			Util::AddFunc(pLua, _EntityMessageBegin, "EntityMessageBegin");
			Util::AddFunc(pLua, _UserMessageBegin, "UserMessageBegin");
			Util::AddFunc(pLua, _MessageEnd, "MessageEnd");
			Util::AddFunc(pLua, BroadcastCustomMessage, "BroadcastCustomMessage");
			Util::AddFunc(pLua, SendCustomMessage, "SendCustomMessage");
		Util::FinishTable(pLua, "HolyLib");
	} else {
		if (Lua::PushHook("HolyLib:Initialize"))
		{
			pLua->CallFunctionProtected(1, 0, true);
		} else {
			DevMsg(1, "holylib: Failed to call HolyLib:Initialize!\n");
		}
	}
}

void CHolyLibModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "holylib");
}

void CHolyLibModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::ModuleLoader server_loader("server");
	Detour::Create(
		&detour_CBaseEntity_PostConstructor, "CBaseEntity::PostConstructor",
		server_loader.GetModule(), Symbols::CBaseEntity_PostConstructorSym,
		(void*)hook_CBaseEntity_PostConstructor, m_pID
	);

	Detour::Create(
		&detour_CFuncLadder_PlayerGotOn, "CFuncLadder::PlayerGotOn",
		server_loader.GetModule(), Symbols::CFuncLadder_PlayerGotOnSym,
		(void*)hook_CFuncLadder_PlayerGotOn, m_pID
	);

	Detour::Create(
		&detour_CFuncLadder_PlayerGotOff, "CFuncLadder::PlayerGotOff",
		server_loader.GetModule(), Symbols::CFuncLadder_PlayerGotOffSym,
		(void*)hook_CFuncLadder_PlayerGotOff, m_pID
	);

	Detour::Create(
		&detour_CBaseEntity_SetMoveType, "CBaseEntity::SetMoveType",
		server_loader.GetModule(), Symbols::CBaseEntity_SetMoveTypeSym,
		(void*)hook_CBaseEntity_SetMoveType, m_pID
	);

	SourceSDK::ModuleLoader engine_loader("engine");
	Detour::Create(
		&detour_GetGModServerTags, "GetGModServerTags",
		engine_loader.GetModule(), Symbols::GetGModServerTagsSym,
		(void*)hook_GetGModServerTags, m_pID
	);

	func_CBaseAnimating_InvalidateBoneCache = (Symbols::CBaseAnimating_InvalidateBoneCache)Detour::GetFunction(server_loader.GetModule(), Symbols::CBaseAnimating_InvalidateBoneCacheSym);
	Detour::CheckFunction((void*)func_CBaseAnimating_InvalidateBoneCache, "CBaseAnimating::InvalidateBoneCache");

	func_CHL2_Player_ExitLadder = (Symbols::CHL2_Player_ExitLadder)Detour::GetFunction(server_loader.GetModule(), Symbols::CHL2_Player_ExitLadderSym);
	Detour::CheckFunction((void*)func_CHL2_Player_ExitLadder, "CHL2_Player::ExitLadder");
}