#include "holylua.h"
#include "GarrysMod/Lua/LuaShared.h"

static GarrysMod::Lua::ILuaInterface* g_HolyLua;

static void lua_run_holylibCmd(const CCommand &args)
{
	if ( args.ArgC() < 1 || args.Arg(1) == "" )
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
	g_HolyLua = Lua::CreateInterface();
	g_HolyLua->SetType(GarrysMod::Lua::State::MENU);
}

void HolyLua::Shutdown()
{
	Lua::DestoryInterface(g_HolyLua);
	g_HolyLua = NULL;
}
