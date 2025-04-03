#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CConCommandModule : public IModule
{
public:
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "concommand"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; };
};

static ConVar holylib_concommand_disableblacklist("holylib_concommand_disableblacklist", "0", 0, "If enabled, it will allow you to run use RunConsoleCommand with any command/convar.");

static CConCommandModule g_pConCommandModule;
IModule* pConCommandModule = &g_pConCommandModule;

static Detouring::Hook detour_ConCommand_IsBlocked;
static bool hook_ConCommand_IsBlocked(const char* cmd)
{
	if (holylib_concommand_disableblacklist.GetBool())
		return false;

	// https://github.com/Facepunch/garrysmod-requests/issues/1534
	if (V_stricmp(cmd, "quit") == 0)
		return false;

	if (V_stricmp(cmd, "exit") == 0)
		return false;

	if (V_stricmp(cmd, "holylib_concommand_disableblacklist") == 0)
		return true;

	return detour_ConCommand_IsBlocked.GetTrampoline<Symbols::ConCommand_IsBlocked>()(cmd);
}

void CConCommandModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::ModuleLoader server_loader("server");
	Detour::Create(
		&detour_ConCommand_IsBlocked, "ConCommand_IsBlocked",
		server_loader.GetModule(), Symbols::ConCommand_IsBlockedSym,
		(void*)hook_ConCommand_IsBlocked, m_pID
	);
}