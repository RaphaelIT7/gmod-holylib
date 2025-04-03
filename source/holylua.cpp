#include "holylua.h"
#include "GarrysMod/Lua/LuaShared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static GarrysMod::Lua::ILuaInterface* g_HolyLua = NULL;
static ConVar holylib_lua("holylib_lua", "0", 0, "If enabled, it will create a new lua interface that will exist until holylib is unloaded");

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
	if (!holylib_lua.GetBool())
		return;

	g_HolyLua = Lua::CreateInterface();
	g_HolyLua->SetType(GarrysMod::Lua::State::MENU);
}

void HolyLua::Shutdown()
{
	if ( !g_HolyLua )
		return;

	Lua::DestroyInterface(g_HolyLua);
	g_HolyLua = NULL;
}
