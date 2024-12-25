#include "holylua.h"
#include "GarrysMod/Lua/LuaShared.h"

static GarrysMod::Lua::ILuaInterface* g_HolyLua;
void HolyLua::Init()
{
	g_HolyLua = Lua::CreateInterface();
	g_HolyLua->SetType(GarrysMod::Lua::State::MENU);
}

void HolyLua::Shutdown()
{

}
