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

	/*
	   Tries to push hook.Run and the given string.
	   Stack:
	   -2 = hook.Run(function)
	   -1 = hook name(string)
	 */
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
		void* pOtherData[4]; // If any other plugin wants to use this, they can.
	};

	extern Lua::StateData* GetLuaData(GarrysMod::Lua::ILuaInterface* LUA);
	extern void CreateLuaData(GarrysMod::Lua::ILuaInterface* LUA);
	extern void RemoveLuaData(GarrysMod::Lua::ILuaInterface* LUA);
}