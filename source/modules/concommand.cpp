#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"

class CConCommandModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn);
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour(bool bPreServer);
	virtual void Think(bool bSimulating);
	virtual void Shutdown();
	virtual const char* Name() { return "concommand"; };
};

ConVar holylib_concommand_disableblacklist("holylib_concommand_disableblacklist", "0", 0, "If enabled, it will allow you to run use RunConsoleCommand with any command/convar.");

CConCommandModule g_pConCommandModule;
IModule* pConCommandModule = &g_pConCommandModule;

Detouring::Hook detour_ConCommand_IsBlocked;
bool hook_ConCommand_IsBlocked(const char* cmd)
{
	if (holylib_concommand_disableblacklist.GetBool())
		return false;

	// https://github.com/Facepunch/garrysmod-requests/issues/1534
	if (V_stricmp(cmd, "quit") == 0)
		return false;

	if (V_stricmp(cmd, "exit") == 0)
		return false;

	return detour_ConCommand_IsBlocked.GetTrampoline<Symbols::ConCommand_IsBlocked>()(cmd);
}

void CConCommandModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
}

void CConCommandModule::LuaInit(bool bServerInit)
{
	if (bServerInit) { return; }
}

void CConCommandModule::LuaShutdown()
{
}

void CConCommandModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }

	SourceSDK::ModuleLoader server_loader("server_srv");
	Detour::Create(
		&detour_ConCommand_IsBlocked, "ConCommand_IsBlocked",
		server_loader.GetModule(), Symbols::ConCommand_IsBlockedSym,
		(void*)hook_ConCommand_IsBlocked, m_pID
	);
}

void CConCommandModule::Think(bool simulating)
{
}

void CConCommandModule::Shutdown()
{
	Detour::Remove(m_pID);
}