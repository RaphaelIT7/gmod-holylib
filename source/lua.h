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

	// A structure in which modules can store data specific to a ILuaInterface.
	// This will be required when we work with multiple ILuaInterface's
	struct StateData
	{
		int iReference = -1; // Reference to this structure to stop GC
	};

	extern Lua::StateData* GetLuaData(GarrysMod::Lua::ILuaInterface* LUA);
	extern void CreateLuaData(GarrysMod::Lua::ILuaInterface* LUA);
	extern void RemoveLuaData(GarrysMod::Lua::ILuaInterface* LUA);
}