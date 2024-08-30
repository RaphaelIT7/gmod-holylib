#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/InterfacePointers.hpp>
#include "iserver.h"
#include "detours.h"
#include "util.h"
#include "lua.h"
#include "player.h"
#include "iclient.h"

class CHolyLibModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "holylib"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; };
};

static CHolyLibModule g_pHolyLibModule;
IModule* pHolyLibModule = &g_pHolyLibModule;

static IServer* pServer;
LUA_FUNCTION_STATIC(Reconnect)
{
	CBasePlayer* ent = Util::Get_Player(1, true);
	if (!ent || strcmp(ent->GetClassname(), "player") != 0) {
		LUA->PushBool(false);
		return 1;
	}

	IClient* client = pServer->GetClient(ent->GetClientIndex());
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

	return 0;
}

static Detouring::Hook detour_CServerGameDLL_ShouldHideServer;
static bool hook_CServerGameDLL_ShouldHideServer()
{
	if (g_bHideServer)
		return true;

	// If we don't want to force the server hidden, we can fallback to the original function.
	return detour_CServerGameDLL_ShouldHideServer.GetTrampoline<Symbols::CServerGameDLL_ShouldHideServer>()(); // "commentary 1" will also hide it.
}

void CHolyLibModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	pServer = InterfacePointers::Server();
}

void CHolyLibModule::LuaInit(bool bServerInit)
{
	if ( !bServerInit )
	{
		Util::StartTable();
			Util::AddFunc(HideServer, "HideServer");
			Util::AddFunc(Reconnect, "Reconnect");
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
	if ( bPreServer ) { return; }

	SourceSDK::ModuleLoader server_loader("server");
	Detour::Create(
		&detour_CServerGameDLL_ShouldHideServer, "CServerGameDLL::ShouldHideServer",
		server_loader.GetModule(), Symbols::CServerGameDLL_ShouldHideServerSym,
		(void*)hook_CServerGameDLL_ShouldHideServer, m_pID
	);
}