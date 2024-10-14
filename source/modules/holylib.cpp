#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/InterfacePointers.hpp>
#include "iserver.h"
#include "detours.h"
#include "util.h"
#include "lua.h"
#include "player.h"
#include "iclient.h"
#include "netmessages.h"
#include "net.h"
#include "sourcesdk/baseclient.h"

class CHolyLibModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
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
	CBasePlayer* ent = Util::Get_Player(1, true);
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

static bool g_bHideServer = false;
LUA_FUNCTION_STATIC(HideServer)
{
	LUA->CheckType(1, GarrysMod::Lua::Type::Bool);
	g_bHideServer = LUA->GetBool(1);

	ConVarRef hide_server("hide_server");
	if (hide_server.IsValid())
		hide_server.SetValue(g_bHideServer); // Remove our hook with the next gmod update. I should start to reduce the amount of Symbols I use.

	return 0;
}

LUA_FUNCTION_STATIC(FadeClientVolume)
{
	CBasePlayer* ent = Util::Get_Player(1, true);
	if (!ent)
		LUA->ArgError(1, "Tried to use a NULL player!");

	edict_t* pEdict = Util::GetEdictOfEnt(ent);
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
	CBaseEntity* pEnt = Util::Get_Entity(1, true);
	if (!pEnt)
		LUA->ArgError(1, "Tried to use a NULL entity!");

	bool bReliable = LUA->GetBool(2);

	if (!pBitBufWrapper->IsEnabled())
		g_Lua->ThrowError("This won't work when the bitbuf library is disabled!");

	EntityMessageBegin(pEnt, bReliable);
	Push_bf_write(GetActiveMessage());
	return 1;
}

LUA_FUNCTION_STATIC(_UserMessageBegin)
{
	IRecipientFilter* pFilter = Get_IRecipientFilter(1, true);
	const char* pName = LUA->CheckString(2);

	if (!pBitBufWrapper->IsEnabled())
		g_Lua->ThrowError("This won't work when the bitbuf library is disabled!");

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
	CBasePlayer* ply = Util::Get_Player(4, true);
	IClient* pClient = (IClient*)Util::GetClientByPlayer(ply);
	if (!pClient)
		LUA->ThrowError("Failed to get IClient from player!");

	SVC_CustomMessage msg;
	msg.m_iType = iType;
	strcpy(msg.m_strName, strName);
	msg.m_DataOut.StartWriting(bf->GetData(), 0, 0, bf->GetMaxNumBits());

	LUA->PushBool(pClient->SendNetMsg(msg));
	return 1;
}

static Detouring::Hook detour_CServerGameDLL_ShouldHideServer;
static bool hook_CServerGameDLL_ShouldHideServer()
{
	if (g_bHideServer)
		return true;

	// If we don't want to force the server hidden, we can fallback to the original function.
	return detour_CServerGameDLL_ShouldHideServer.GetTrampoline<Symbols::CServerGameDLL_ShouldHideServer>()(); // "commentary 1" will also hide it.
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

Symbols::CBaseAnimating_InvalidateBoneCache func_CBaseAnimating_InvalidateBoneCache;
LUA_FUNCTION_STATIC(InvalidateBoneCache)
{
	CBaseEntity* pEnt = Util::Get_Entity(1, true);
	if (!pEnt)
		LUA->ArgError(1, "Tried to use a NULL entity");

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
		Util::Push_Entity(pEnt);
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
		pClient = Util::GetClientByPlayer(Util::Get_Player(1, true));
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

void CHolyLibModule::LuaInit(bool bServerInit)
{
	if (!bServerInit)
	{
		pBitBufWrapper = g_pModuleManager.FindModuleByName("bitbuf");

		Util::StartTable();
			Util::AddFunc(HideServer, "HideServer");
			Util::AddFunc(Reconnect, "Reconnect");
			Util::AddFunc(FadeClientVolume, "FadeClientVolume");
			Util::AddFunc(ServerExecute, "ServerExecute");
			Util::AddFunc(IsMapValid, "IsMapValid");
			Util::AddFunc(InvalidateBoneCache, "InvalidateBoneCache");
			Util::AddFunc(SetSignOnState, "SetSignOnState");

			// Networking stuff
			Util::AddFunc(_EntityMessageBegin, "EntityMessageBegin");
			Util::AddFunc(_UserMessageBegin, "UserMessageBegin");
			Util::AddFunc(_MessageEnd, "MessageEnd");
			Util::AddFunc(BroadcastCustomMessage, "BroadcastCustomMessage");
			Util::AddFunc(SendCustomMessage, "SendCustomMessage");
		Util::FinishTable("HolyLib");
	} else {
		if (Lua::PushHook("HolyLib:Initialize"))
		{
			g_Lua->CallFunctionProtected(1, 0, true);
		} else {
			DevMsg(1, "Failed to call HolyLib:Initialize!\n");
		}
	}
}

void CHolyLibModule::LuaShutdown()
{
	Util::NukeTable("holylib");
}

void CHolyLibModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::ModuleLoader server_loader("server");
	Detour::Create(
		&detour_CServerGameDLL_ShouldHideServer, "CServerGameDLL::ShouldHideServer",
		server_loader.GetModule(), Symbols::CServerGameDLL_ShouldHideServerSym,
		(void*)hook_CServerGameDLL_ShouldHideServer, m_pID
	);

	Detour::Create(
		&detour_CBaseEntity_PostConstructor, "CBaseEntity::PostConstructor",
		server_loader.GetModule(), Symbols::CBaseEntity_PostConstructorSym,
		(void*)hook_CBaseEntity_PostConstructor, m_pID
	);

	SourceSDK::ModuleLoader engine_loader("engine");
	Detour::Create(
		&detour_GetGModServerTags, "GetGModServerTags",
		engine_loader.GetModule(), Symbols::GetGModServerTagsSym,
		(void*)hook_GetGModServerTags, m_pID
	);

	func_CBaseAnimating_InvalidateBoneCache = (Symbols::CBaseAnimating_InvalidateBoneCache)Detour::GetFunction(server_loader.GetModule(), Symbols::CBaseAnimating_InvalidateBoneCacheSym);
	Detour::CheckFunction((void*)func_CBaseAnimating_InvalidateBoneCache, "CBaseAnimating::InvalidateBoneCache");
}