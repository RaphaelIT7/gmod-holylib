#include "holylua.h"
#include "GarrysMod/Lua/LuaShared.h"
#include "module.h"
#include "tier0/icommandline.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static GarrysMod::Lua::ILuaInterface* g_HolyLua = NULL;
static void OnLuaChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	bool bNewValue = ((ConVar*)convar)->GetBool();

	if (!bNewValue && g_HolyLua)
	{
		HolyLua::Shutdown();
	}
	else if (bNewValue && !g_HolyLua)
	{
		HolyLua::Init();
	}
}

static ConVar holylib_lua("holylib_lua", "0", 0, "If enabled, it will create a new lua interface that will exist until holylib is unloaded", OnLuaChange);

static void lua_run_holylibCmd(const CCommand &args)
{
	if ( args.ArgC() < 1 || Q_stricmp(args.Arg(1), "") == 0 )
	{
		Msg("Usage: lua_run_holylib <code>\n");
		return;
	}

	if (g_HolyLua)
		g_HolyLua->RunString("RunString", "", args.ArgS(), true, true);
}
static ConCommand lua_run_holylib("lua_run_holylib", lua_run_holylibCmd, "Runs code in the holylib lua state", 0);

void HolyLua::Init()
{
	if (!holylib_lua.GetBool() && !CommandLine()->FindParm("-holylib_lua"))
		return;

	g_HolyLua = Lua::CreateInterface();
	g_HolyLua->SetType(GarrysMod::Lua::State::MENU);

	// Now add all supported HolyLib modules into the new interface.
	g_pModuleManager.LuaInit(g_HolyLua, false);
	g_pModuleManager.LuaInit(g_HolyLua, true);
}

void HolyLua::Shutdown()
{
	if ( !g_HolyLua )
		return;

	g_pModuleManager.LuaShutdown(g_HolyLua);
	Lua::DestroyInterface(g_HolyLua);
	g_HolyLua = NULL;
}
