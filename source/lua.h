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

	   When you call CallFunctionProtected you need to use your number of arguments + 1 since the hook name is also an argument!
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
		unsigned char iType = UCHAR_MAX;
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
		// WavAudioFile,

		TOTAL_TYPES = 255,
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
		GarrysMod::Lua::ILuaInterface* pLua = NULL;

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
			pLuaTypes[type].iType = (unsigned char)metaID;
			Msg("Registered MetaTable: %i - %i\n", (int)type, metaID);
		}

		inline unsigned char GetMetaTable(LuaTypes type)
		{
			return pLuaTypes[type].iType;
		}

		inline LuaTypes FindMetaTable(unsigned char type)
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

	/*
	 * Where do we store our StateData?
	 * In the ILuaInterface itself.
	 * We abuse the GetPathID var as it's a char[32] but it'll never actually fully use it.
	 * Why? Because I didn't want to use yet another unordered_map for this, also this should be faster.
	 */
	inline Lua::StateData* GetLuaData(GarrysMod::Lua::ILuaInterface* LUA)
	{
		return *reinterpret_cast<Lua::StateData**>((char*)LUA->GetPathID() + 24);
	}
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
	extern void DestroyTValue(TValue* o);
	extern void PushTValue(lua_State* L, TValue* o);
	extern void SetReadOnly(TValue* o, bool readOnly);
	extern void* GetUserDataOrFFIVar(lua_State* L, int idx, bool cDataTypes[USHRT_MAX]);
	extern uint16_t GetCDataType(lua_State* L, int idx);
}

// Creates a function Get[funcName]LuaData and returns the stored module data from the given module.
#define LUA_GetModuleData(className, moduleName, funcName) \
static inline className* Get##funcName##LuaData(GarrysMod::Lua::ILuaInterface* pLua) \
{ \
	if (!pLua) { \
		return NULL; \
	} \
\
	return (className*)Lua::GetLuaData(pLua)->GetModuleData(moduleName.m_pID); \
}
// Another new line just for the macro to not shit itself. GG