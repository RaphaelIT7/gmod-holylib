#include "lua.h"


namespace HolyLua
{
	extern void Init();
	extern void Think();
	extern void Shutdown();
	extern GarrysMod::Lua::ILuaInterface* GetInterface();
}