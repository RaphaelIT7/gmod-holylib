#pragma once

#include <GarrysMod/Lua/LuaShared.h>
#include "util.h"

namespace Lua
{
	extern void Init(GarrysMod::Lua::ILuaInterface* LUA);
	extern void Shutdown();
	extern void ServerInit();
	extern bool PushHook(const char* pName);
	extern void AddDetour();
	extern GarrysMod::Lua::ILuaInterface* GetRealm(unsigned char);
	extern GarrysMod::Lua::ILuaShared* GetShared();
}