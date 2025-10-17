#pragma once

#include "modules/_modules.h"
#include <lua/ILuaInterface.h>
#include "Platform.hpp"
#include "vprof.h"
#include <unordered_map>
#include <algorithm>
#include "symbols.h"
#include "unordered_set"
#include <shared_mutex>

#define DEDICATED
#include "vstdlib/jobthread.h"
#include "../luajit/src/lua.hpp"

class IVEngineServer;

// Added to not break some sourcesdk things. Use Util::engineserver!
extern IVEngineServer* engine;

#define VPROF_BUDGETGROUP_HOLYLIB _T("HolyLib")

#if ARCHITECTURE_IS_X86_64
#define V_CreateThreadPool CreateNewThreadPool
#define V_DestroyThreadPool DestroyThreadPool
#endif

extern GarrysMod::Lua::ILuaInterface* g_Lua;

struct edict_t;
class IGet;
class CBaseEntity;
class CBasePlayer;
class CBaseClient;
class CGlobalEntityList;
class CUserMessages;
class IServerGameClients;
class IServerGameEnts;
class IModuleWrapper;
class IGameEventManager2;
class IServer;
class IServerGameDLL;
class ISteamUser;
struct CBaseHandle;
namespace Util
{
	#define LUA_REGISTRYINDEX	(-10000)
	#define LUA_ENVIRONINDEX	(-10001)
	#define LUA_GLOBALSINDEX	(-10002)

	#define SHIFT_STACK(stack, offset) (stack > 0) ? (stack + offset) : (stack - offset)

	/*
	 * RawSetI & RawGetI are way faster but Gmod doesn't expose or even use them :(
	 */
	extern Symbols::lua_rawseti func_lua_rawseti;
	inline void RawSetI(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, int iValue)
	{
		if (LUA != g_Lua)
		{
			lua_rawseti(LUA->GetState(), iStackPos, iValue);
			return;
		}

		if (func_lua_rawseti)
		{
			func_lua_rawseti(LUA->GetState(), iStackPos, iValue);
		} else {
			LUA->PushNumber(iValue);
			LUA->Push(-2);
			LUA->Remove(-3);
			LUA->RawSet(SHIFT_STACK(iStackPos, 1));
		}
	}

	extern Symbols::lua_rawgeti func_lua_rawgeti;
	inline void RawGetI(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, int iValue)
	{
		if (LUA != g_Lua)
		{
			lua_rawgeti(LUA->GetState(), iStackPos, iValue);
			return;
		}

		if (func_lua_rawgeti)
		{
			func_lua_rawgeti(LUA->GetState(), iStackPos, iValue);
		} else {
			LUA->PushNumber(iValue);
			LUA->RawGet(SHIFT_STACK(iStackPos, 1));
		}
	}

	extern Symbols::luaL_checklstring func_luaL_checklstring;
	inline const char* CheckLString(GarrysMod::Lua::ILuaInterface* LUA, int nStackPos, size_t* nOutLength)
	{
		if (LUA != g_Lua)
		{
			return luaL_checklstring(LUA->GetState(), nStackPos, nOutLength);
		}

		if (func_luaL_checklstring)
		{
			return func_luaL_checklstring(LUA->GetState(), nStackPos, nOutLength);
		} else {
			const char* pStr = LUA->CheckString(nStackPos);
			if (nOutLength)
				*nOutLength = LUA->ObjLen(nStackPos);

			return pStr;
		}
	}

	/*
	 * Why do we have the same code here when CLuaInterface::ReferencePush does exactly the same?
	 * Because like this we should hopefully skip possibly funny code & the vtable call.
	 *
	 * NOTE: It does seem to be faster so were going to keep this.
	 */
	inline void ReferencePush(GarrysMod::Lua::ILuaInterface* LUA, int iReference)
	{
		if (LUA != g_Lua)
		{
			lua_rawgeti(LUA->GetState(), LUA_REGISTRYINDEX, iReference);
			return;
		}

		if (func_lua_rawgeti)
		{
			func_lua_rawgeti(LUA->GetState(), LUA_REGISTRYINDEX, iReference);
		} else {
			LUA->ReferencePush(iReference);
		}
	}

	/*
	 * Pure debugging
	 */
#define HOLYLIB_UTIL_DEBUG_REFERENCES 0
	extern std::unordered_set<int> g_pReference;
	extern ConVar holylib_debug_mainutil;
	inline int ReferenceCreate(GarrysMod::Lua::ILuaInterface* LUA, const char* reason)
	{
		int iReference = LUA->ReferenceCreate();

#if HOLYLIB_UTIL_DEBUG_REFERENCES
		if (holylib_debug_mainutil.GetBool())
			Msg(PROJECT_NAME ": Created reference %i (%s)\n", iReference, reason);

		auto it = g_pReference.find(iReference);
		if (it != g_pReference.end())
		{
			Error(PROJECT_NAME ": Created a reference when we already holded it. How. Crash this shit. (%s)\n", reason); // If this happens maybe gmod does some weird shit?
		}

		g_pReference.insert(iReference);
#endif

		return iReference;
	}

	inline void ReferenceFree(GarrysMod::Lua::ILuaInterface* LUA, int iReference, const char* reason)
	{
#if HOLYLIB_UTIL_DEBUG_REFERENCES
		if (holylib_debug_mainutil.GetBool())
			Msg(PROJECT_NAME ": Freed reference %i (%s)\n", iReference, reason);

		auto it = g_pReference.find(iReference);
		if (it == g_pReference.end())
		{
			Error(PROJECT_NAME ": Freed a reference when we didn't holded it. How. Crash this shit. (%s)\n", reason); // If this happens I'm happy.
		}

		g_pReference.erase(it);
#endif

		LUA->ReferenceFree(iReference);
	}

	inline void StartTable(GarrysMod::Lua::ILuaInterface* LUA) {
		LUA->CreateTable();
	}

	inline void AddFunc(GarrysMod::Lua::ILuaInterface* LUA, GarrysMod::Lua::CFunc Func, const char* Name) {
		LUA->PushString(Name);
		LUA->PushCFunction(Func);
		LUA->RawSet(-3);
	}

	inline void AddValue(GarrysMod::Lua::ILuaInterface* LUA, double value, const char* Name) {
		LUA->PushString(Name);
		LUA->PushNumber(value);
		LUA->RawSet(-3);
	}

	inline void FinishTable(GarrysMod::Lua::ILuaInterface* LUA, const char* Name) {
		LUA->SetField(LUA_GLOBALSINDEX, Name);
	}

	inline void NukeTable(GarrysMod::Lua::ILuaInterface* LUA, const char* pName)
	{
		LUA->PushNil();
		LUA->SetField(LUA_GLOBALSINDEX, pName);
	}

	inline bool PushTable(GarrysMod::Lua::ILuaInterface* LUA, const char* pName)
	{
		LUA->GetField(LUA_GLOBALSINDEX, pName);
		if (LUA->IsType(-1, GarrysMod::Lua::Type::Table))
			return true;

		LUA->Pop(1);
		return false;
	}

	inline void PopTable(GarrysMod::Lua::ILuaInterface* LUA)
	{
		LUA->Pop(1);
	}

	inline void RemoveField(GarrysMod::Lua::ILuaInterface* LUA, const char* pName)
	{
		LUA->PushNil();
		LUA->SetField(-2, pName);
	}

	inline bool HasField(GarrysMod::Lua::ILuaInterface* LUA, const char* pName, int iType)
	{
		LUA->GetField(-1, pName);
		return LUA->IsType(-1, iType);
	}

	// Gmod's functions:
	extern CBasePlayer* Get_Player(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError);
	extern CBaseEntity* Get_Entity(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError);
	extern void Push_Entity(GarrysMod::Lua::ILuaInterface* LUA, CBaseEntity* pEnt);
	extern CBaseEntity* GetCBaseEntityFromEdict(edict_t* edict);
	extern CBaseEntity* GetCBaseEntityFromIndex(int nEntIndex);
	extern CBaseEntity* GetCBaseEntityFromHandle(const CBaseHandle& pHandle);

	extern void AddDetour(); // We load Gmod's functions in there.
	extern void RemoveDetour();

	extern CBaseClient* GetClientByUserID(int userID);
	extern CBaseClient* GetClientByPlayer(const CBasePlayer* ply);
	extern CBaseClient* GetClientByIndex(int index);
	extern std::vector<CBaseClient*> GetClients();
	extern CBasePlayer* GetPlayerByClient(CBaseClient* client);
	
	#define MAX_MAP_LEAFS 65536
	struct VisData {
		byte cluster[MAX_MAP_LEAFS / 8];
	};

	// Returns new VisData, delete it after use
	extern VisData* CM_Vis(const Vector& orig, int type);
	extern bool CM_Vis(byte* cluster, int clusterSize, int cluserID, int type);
	extern void ResetClusers(VisData* data);

	extern bool ShouldLoad();
	extern void CheckVersion();

	// The main load/unload functions
	extern void Load();
	extern void Unload();

	// Iterator functions for entities that reliably work even if the entitylist isn't available.
	extern CBaseEntity* FirstEnt();
	extern CBaseEntity* NextEnt(CBaseEntity* pEntity);

	// Steam API functions as they love to crash apparently
	// Can return NULL
	extern ISteamUser* GetSteamUser();

	inline void StartThreadPool(IThreadPool* pool, ThreadPoolStartParams_t& startParams)
	{
#if ARCHITECTURE_IS_X86_64 && SYSTEM_LINUX
		startParams.bEnableOnLinuxDedicatedServer = true;
#endif
		pool->Start(startParams);
	}

	inline void StartThreadPool(IThreadPool* pool, int iThreads)
	{
		ThreadPoolStartParams_t startParams;
		startParams.nThreads = iThreads;
		startParams.nThreadsMax = startParams.nThreads;
		Util::StartThreadPool(pool, startParams);
	}

	// Gameevent stuff
	extern std::unordered_set<std::string> pBlockedEvents; // For direct access
	extern void BlockGameEvent(const char* pName);
	extern void UnblockGameEvent(const char* pName);

	// Offset / Networking related stuff
	
	// tries to find a SendProp with the given name
	// and if found it will return the offset stored in the sendprop.
	// Returns -1 on failure
	extern int FindOffsetForNetworkVar(const char* pDTName, const char* pVarName);

	// Returns a pointer to the given offset for the base, do the casting yourself.
	inline void* GoToNetworkVarOffset(void* pBase, int nOffset)
	{
		if (nOffset == -1)
			return nullptr;

		// NOTE: Normally I'd use *(void**) to dereference it but apparently it's only a thing needed for vars that store a pointer / only m_GMOD_DataTable
		return (void*)((char*)pBase + nOffset);
	}

	// More Lua stuff for UserData (NEVER NULL)
	extern Symbols::lua_setfenv func_lua_setfenv;
	extern Symbols::lua_touserdata func_lua_touserdata;
	extern Symbols::lua_type func_lua_type;

	extern Symbols::lua_pcall func_lua_pcall;
	extern Symbols::lua_insert func_lua_insert;
	extern Symbols::lua_toboolean func_lua_toboolean;

	// These can be NULL. Why? Because on 64x all the names are mangled making shit far more difficult...
	extern Symbols::lj_tab_new func_lj_tab_new;
	extern Symbols::lj_gc_barrierf func_lj_gc_barrierf;
	extern Symbols::lj_tab_get func_lj_tab_get;

	extern IVEngineServer* engineserver;
	extern IServerGameClients* servergameclients;
	extern IServerGameEnts* servergameents;
	extern IServerGameDLL* servergamedll;
	extern IServer* server;
	extern CGlobalEntityList* entitylist;
	extern CUserMessages* pUserMessages;
	extern IModuleWrapper* pEntityList; // Other rely on this module.
	extern IGameEventManager2* gameeventmanager;
	extern IGet* get;
}

// Helper class to get a pointer to our DTVar
class DTVarByOffset
{
public:
	DTVarByOffset(const char* pDTName, const char* pVarName)
	{
		m_pDTName = pDTName;
		m_pVarName = pVarName;
	}

	DTVarByOffset(const char* pDTName, const char* pVarName, int nArraySize)
	{
		m_pDTName = pDTName;
		m_pVarName = pVarName;
	}

	inline void Init()
	{
		if (m_nOffset != -1)
			return;

		m_nOffset = Util::FindOffsetForNetworkVar(m_pDTName, m_pVarName);
		if (m_nOffset == -1)
			Error(PROJECT_NAME ": Failed to find DTVar offset of var \"%s\" in DataTable \"%s\"!\n", m_pDTName, m_pVarName);
	}

	FORCEINLINE void* GetPointer(void* pBase)
	{
		if (m_nOffset == -1)
			Init();

		return Util::GoToNetworkVarOffset(pBase, m_nOffset);
	}

	// For DTVars that store a pointer like m_GMOD_DataTable
	FORCEINLINE void* GetPointerDereferenced(void* pBase)
	{
		if (m_nOffset == -1)
			Init();

		return *(void**)Util::GoToNetworkVarOffset(pBase, m_nOffset);
	}

	FORCEINLINE void* GetPointerArray(void* pBase, int nArraySlot)
	{
		if (m_nOffset == -1)
			Init();

		return Util::GoToNetworkVarOffset(pBase, m_nOffset + (m_nArraySize * nArraySlot));
	}

	int m_nOffset = -1;
	int m_nArraySize = 0;
	const char* m_pDTName = nullptr;
	const char* m_pVarName = nullptr;
};

#if SYSTEM_LINUX // Linux got a bigger default stack.
constexpr size_t _MAX_ALLOCA_SIZE = 64 * 1024;
#else
constexpr size_t _MAX_ALLOCA_SIZE = 8 * 1024;
#endif

#define holylib_canstackalloc(size) (static_cast<size_t>(size) <= _MAX_ALLOCA_SIZE)

#define Vector_RemoveElement(vec, element) \
{ \
	auto _it = std::find((vec).begin(), (vec).end(), (element)); \
	if (_it != (vec).end()) \
		(vec).erase(_it); \
}
// New line so that no warning is thrown.