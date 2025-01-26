#pragma once

#include "util.h"

namespace GarrysMod::Lua
{
	class ILuaShared;
}

namespace Lua
{
	extern void Init(GarrysMod::Lua::ILuaInterface* LUA);
	extern void Shutdown();
	extern void FinalShutdown();
	extern void ServerInit();
	extern bool PushHook(const char* pName);
	extern void AddDetour();
	extern void SetManualShutdown();
	extern void ManualShutdown();
	extern GarrysMod::Lua::ILuaInterface* GetRealm(unsigned char);
	extern GarrysMod::Lua::ILuaShared* GetShared();

	extern GarrysMod::Lua::ILuaInterface* CreateInterface();
	extern void DestroyInterface(GarrysMod::Lua::ILuaInterface* LUA);
}