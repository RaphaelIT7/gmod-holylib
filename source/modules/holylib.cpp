#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/InterfacePointers.hpp>
#include "iserver.h"
#include "detours.h"
#include "util.h"
#include "lua.h"

class CHolyLibModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* fn);
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour();
	virtual void Think(bool simulating);
	virtual void Shutdown();
	virtual const char* Name() { return "holylib"; };
	virtual void LoadConfig(KeyValues* config) {};
};

CHolyLibModule g_pHolyLibModule;
IModule* pHolyLibModule = &g_pHolyLibModule;

IServer* pServer;
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

bool g_bHideServer = false;
LUA_FUNCTION_STATIC(HideServer)
{
	LUA->CheckType(1, GarrysMod::Lua::Type::Bool);
	g_bHideServer = LUA->GetBool(1);

	return 0;
}

Detouring::Hook detour_CServerGameDLL_ShouldHideServer;
bool hook_CServerGameDLL_ShouldHideServer()
{
	if (g_bHideServer)
		return true;

	// If we don't want to force the server hidden, we can fallback to the original function.
	return detour_CServerGameDLL_ShouldHideServer.GetTrampoline<Symbols::CServerGameDLL_ShouldHideServer>()(); // "commentary 1" will also hide it.
}

void CHolyLibModule::Init(CreateInterfaceFn* fn)
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
}

void CHolyLibModule::InitDetour()
{
	SourceSDK::ModuleLoader server_loader("server_srv");
	Detour::Create(
		&detour_CServerGameDLL_ShouldHideServer, "CServerGameDLL::ShouldHideServer",
		server_loader.GetModule(), Symbols::CServerGameDLL_ShouldHideServerSym,
		hook_CServerGameDLL_ShouldHideServer, m_pID
	);
}

void CHolyLibModule::Think(bool bSimulating)
{
}

void CHolyLibModule::Shutdown()
{
	Detour::Remove(m_pID);
}