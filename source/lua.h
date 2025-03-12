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

	// Each new metatable has this entry.
	struct LuaMetaEntry {
		int iType = -1;
		std::unordered_map<void*, LuaUserData*> g_pPushedUserData;
	};

	/*
		All lua types that exist.
		Use LUA_className as formatting.
	*/
	enum LuaTypes {
		IGModAudioChannel,
		bf_read,
		bf_write,
		CBaseClient,
		CHLTVClient,
		QAngle,
		Vector,
		EntityList,
		ConVar,

		TOTAL_TYPES,
	};

	// A structure in which modules can store data specific to a ILuaInterface.
	// This will be required when we work with multiple ILuaInterface's
	struct StateData
	{
		void* pOtherData[4]; // If any other plugin wants to use this, they can.
		void* pModuelData[64];
		LuaMetaEntry pLuaTypes[LuaTypes::TOTAL_TYPES];

		inline void RegisterMetaTable(LuaTypes type, int metaID)
		{
			pLuaTypes[type].iType = metaID;
		}

		inline int GetMetaTable(LuaTypes type)
		{
			return pLuaTypes[type].iType;
		}

		inline std::unordered_map<void*, LuaUserData*>& GetPushedUserData(LuaTypes type)
		{
			return pLuaTypes[type].g_pPushedUserData;
		}
	};

	extern Lua::StateData* GetLuaData(GarrysMod::Lua::ILuaInterface* LUA);
	extern void CreateLuaData(GarrysMod::Lua::ILuaInterface* LUA);
	extern void RemoveLuaData(GarrysMod::Lua::ILuaInterface* LUA);
}

#define LUATHREAD_FUNCTION_STATIC( FUNC )								\
static int FUNC##__Imp( GarrysMod::Lua::ILuaInterface* LUA, Lua::StateData* LUADATA );	\
static int FUNC( lua_State* L )                             \
{                                                           \
	GarrysMod::Lua::ILuaInterface* LUA = L->luabase;		\
	LUA->SetState(L);										\
	Lua::StateData* LUADATA = Lua::GetLuaData( LUA );		\
	return FUNC##__Imp( LUA, LUADATA );						\
}															\
static int FUNC##__Imp( [[maybe_unused]] GarrysMod::Lua::ILuaInterface* LUA, Lua::StateData* LUADATA )