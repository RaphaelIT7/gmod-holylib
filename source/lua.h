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
	   Hooks ALWAYS run on g_Lua.
	   Tries to push hook.Run and the given string.
	   Stack:
	   -2 = hook.Run(function)
	   -1 = hook name(string)
	 */
	extern bool PushHook(const char* pName, GarrysMod::Lua::ILuaInterface* pLua = g_Lua);
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
	};

	/*
		All lua types that exist.
		Use LUA_className as formatting.
	*/
	enum LuaTypes {
		IGModAudioChannel,
		LUA_bf_read,
		LUA_bf_write,
		CBaseClient,
		CHLTVClient,
		QAngle,
		Vector,
		EntityList,
		ConVar,
		IPhysicsCollisionSet,
		CPhysCollide,
		CPhysPolysoup,
		CPhysConvex,
		ICollisionQuery,
		IPhysicsObject,
		ILuaPhysicsEnvironment,
		IGameEvent,
		HttpResponse,
		HttpRequest,
		HttpServer,
		VoiceData,
		VoiceStream,
		CNetChan,
		INetworkStringTable,
		CVProfNode,
		VProfCounter,
		LuaInterface,

		TOTAL_TYPES,
	};

	class ModuleData {
	public:
		virtual ~ModuleData() = default; // Virtual deconstructor if anyone has their own deleting process
	};

	namespace Internal {
		static constexpr int pMaxEntries = 64;
	}

	// A structure in which modules can store data specific to a ILuaInterface.
	// This will be required when we work with multiple ILuaInterface's
	struct StateData
	{
		void* pOtherData[4]; // If any other plugin wants to use this, they can.
		Lua::ModuleData* pModuelData[Lua::Internal::pMaxEntries] = { NULL }; // It uses the assigned module IDs
		LuaMetaEntry pLuaTypes[LuaTypes::TOTAL_TYPES];
		std::unordered_map<void*, LuaUserData*> pPushedUserData; // Would love to get rid of this
		CThreadMutex pMutex;

		~StateData()
		{
			for (int i = 0; i < Lua::Internal::pMaxEntries; ++i)
			{
				Lua::ModuleData* pData = pModuelData[i];
				if (pData == NULL)
					continue;

				delete pData;
				pModuelData[i] = NULL;
			}
		}

		inline void RegisterMetaTable(LuaTypes type, int metaID)
		{
			pLuaTypes[type].iType = metaID;
			Msg("Registered MetaTable: %i - %i\n", (int)type, metaID);
		}

		inline int GetMetaTable(LuaTypes type)
		{
			return pLuaTypes[type].iType;
		}

		inline LuaTypes FindMetaTable(int type)
		{
			for (int i=0; i<LuaTypes::TOTAL_TYPES; ++i)
			{
				if (pLuaTypes[i].iType == type)
					return (LuaTypes)i;
			}

			return LuaTypes::TOTAL_TYPES;
		}

		/*
		 * Returns the module data by the given module id.
		 */
		inline void* GetModuleData(int moduleID)
		{
			if (moduleID >= Lua::Internal::pMaxEntries || moduleID < 0) // out of bounds
				return NULL;

			return (void*)pModuelData[moduleID];
		}

		/*
		 * Sets the given module data by the given module id.
		 */
		inline void SetModuleData(int moduleID, Lua::ModuleData* moduleData)
		{
			if (moduleID >= Lua::Internal::pMaxEntries || moduleID < 0) // out of bounds
			{
				Warning(PROJECT_NAME ": Tried to set module id for a module out of range! (%i)\n", moduleID);
				return;
			}

			pModuelData[moduleID] = moduleData;
		}

		inline std::unordered_map<void*, LuaUserData*>& GetPushedUserData()
		{
			return pPushedUserData;
		}
	};

	extern Lua::StateData* GetLuaData(GarrysMod::Lua::ILuaInterface* LUA);
	extern void CreateLuaData(GarrysMod::Lua::ILuaInterface* LUA, bool bNullOut = false);
	extern void RemoveLuaData(GarrysMod::Lua::ILuaInterface* LUA);
	extern const std::unordered_set<Lua::StateData*>& GetAllLuaData();

	// ToDo
	// - EnterLockdown
	// - LeaveLockdown
	// What do they do?
	// Their used when All ILuaInterfaces need to be locked in cases like a referenced userdata being deleted.
	// This is because, a referenced userdata can be shared across threads.
	// This sounds like a dumb idea, because it is.
	// it would be painful to implement differently.
}

union TValue;
namespace RawLua {
	extern TValue *index2adr(lua_State *L, int idx);
	extern TValue* CopyTValue(lua_State* L, TValue* o);
	extern void PushTValue(lua_State* L, TValue* o);
	extern void SetReadOnly(TValue* o, bool readOnly);
}