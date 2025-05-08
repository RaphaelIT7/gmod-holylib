#pragma once

#include <lua/ILuaInterface.h>
#include "Platform.hpp"
#include "vprof.h"
#include <unordered_map>
#include <algorithm>
#include "symbols.h"
#include "unordered_set"

#define DEDICATED
#include "vstdlib/jobthread.h"
#include "lua.hpp"

#if GITHUB_RUN_DATA == 0
#define HOLYLIB_BUILD_RELEASE 0
#else
#define HOLYLIB_BUILD_RELEASE 1
#endif

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

/*
	ToDo: Implement a proper class like gmod has with CLuaCLass/CLuaLibrary & use thoes instead for everything.
*/

/*
 * How to properly push & clean up LuaUserData.
 *
 * First, use the proper function for the userdata you want to push.
 *
 * Push_LuaClass - Use this macro if you want the Lua GC to free the userdata.
 * PushReferenced_LuaClass - Use this macro if you want to manually delete the userdata, it will hold a reference of itself stopping the GC.
 *
 * NOTE: If you use PushReferenced_LuaClass you should ALWAYS call DeleteAll_[YourClass] for your userdata
 *	   as else it COULD persist across lua states which is VERY BAD as the references will ALL be INVALID.
 */

// This WILL slow down userData creation & deletion so we disable this in release builds.
#if HOLYLIB_BUILD_RELEASE
#define HOLYLIB_UTIL_DEBUG_LUAUSERDATA 0
#else
#define HOLYLIB_UTIL_DEBUG_LUAUSERDATA 1
#endif
#define HOLYLIB_UTIL_DEBUG_BASEUSERDATA 0

/*
 * Base UserData struct that is used by LuaUserData.
 * Main purpose is for it to handle the stored pData & implement a reference counter.
 * It also maskes sharing data between threads easier.
 *
 * NOTE:
 * Memory usage is utter garbage.
 * For UserData which stores a few bytes (often 4) we setup UserData that has a average size of 60 bytes...
 */

struct LuaUserData;
struct BaseUserData;
extern std::unordered_map<void*, BaseUserData*> g_pGlobalLuaUserData; // A set containing all BaseUserData
struct BaseUserData {
	BaseUserData(void* data)
	{
		m_pData = data;
		g_pGlobalLuaUserData[m_pData] = this; // Link it
	}

	~BaseUserData()
	{
		if (m_pData != NULL)
		{
			auto it = g_pGlobalLuaUserData.find(m_pData);
			if (it != g_pGlobalLuaUserData.end())
				g_pGlobalLuaUserData.erase(it);
		}

		m_pData = NULL;
	}

	inline void* GetData(GarrysMod::Lua::ILuaInterface* pLua)
	{
		if (m_bLocked && pLua != m_pOwningLua)
			return NULL;

		return m_pData;
	}

	/*inline void SetData(void* data)
	{
		if (pData != NULL) // This UserData was reassigned?!? Shouldn't happen normally... Anyways.
		{
			auto it = g_pGlobalLuaUserData.find(pData);
			if (it != g_pGlobalLuaUserData.end())
				g_pGlobalLuaUserData.erase(it);
		}

		pData = data;
		g_pGlobalLuaUserData[pData] = this; // Link it
	}*/

	/*
	 * Locks/Unlocks the UserData making GetData only return a valid result for the owning ILuaInterface.
	 */
	inline void SetLocked(bool bLocked)
	{
		m_bLocked = bLocked;
	}

	inline bool Release(LuaUserData* pLuaUserData)
	{
		m_pMutex.Lock();
		if (m_iReferenceCount == 0) // Don't accidentally delete it again...
		{
			m_pMutex.Unlock();
			return false;
		}

		--m_iReferenceCount;
#if HOLYLIB_UTIL_DEBUG_BASEUSERDATA
		Msg("holylib - util: Userdata %p(%p) got released %i\n", this, m_pData, m_iReferenceCount);
#endif
		m_pOwningData.erase(pLuaUserData);

		if (m_iReferenceCount == 0)
		{
			m_pMutex.Unlock();
			delete this;
			return true;
		}

		m_pMutex.Unlock();
		return false;
	}

	static inline BaseUserData* LuaAquire(LuaUserData* pLuaUserData, void* pData)
	{
		if (!pData)
			return NULL;

		auto it = g_pGlobalLuaUserData.find(pData);
		if (it != g_pGlobalLuaUserData.end())
		{
			it->second->Aquire();
			return it->second;
		}

		BaseUserData* pUserData = new BaseUserData(pData);
		pUserData->Aquire(); // Increment counter
		pUserData->m_pOwningData.insert(pLuaUserData);
		return pUserData;
	}

	static inline void ForceGlobalRelease(void* pData);

private:
	inline void Aquire()
	{
		m_pMutex.Lock();
		++m_iReferenceCount;
		m_pMutex.Unlock();
#if HOLYLIB_UTIL_DEBUG_BASEUSERDATA
		Msg("holylib - util: Userdata %p(%p) got aquired %i\n", this, m_pData, m_iReferenceCount);
#endif
	}

	void* m_pData = NULL;
	bool m_bLocked = false;
	GarrysMod::Lua::ILuaInterface* m_pOwningLua = NULL;

	unsigned int m_iReferenceCount = 0;
	std::unordered_set<LuaUserData*> m_pOwningData;
	CThreadFastMutex m_pMutex;
};

#if HOLYLIB_UTIL_DEBUG_LUAUSERDATA
extern bool g_pRemoveLuaUserData;
extern std::unordered_set<LuaUserData*> g_pLuaUserData; // A set containing all LuaUserData that actually hold a reference.
#endif
struct LuaUserData {
	LuaUserData() {
#if HOLYLIB_UTIL_DEBUG_LUAUSERDATA
		g_pLuaUserData.insert(this);
#endif
	}

	~LuaUserData() {
		if (!ThreadInMainThread())
		{
			Warning(PROJECT_NAME ": Tried to delete usetdata from another thread!\n");

#if HOLYLIB_UTIL_DEBUG_LUAUSERDATA
			if (g_pRemoveLuaUserData)
			{
				g_pLuaUserData.erase(this);
			}
#endif
			return;
		}

#if HOLYLIB_UTIL_DEBUG_LUAUSERDATA
		if (g_pRemoveLuaUserData)
		{
			g_pLuaUserData.erase(this);
		}
#endif

		if (iReference != -1)
		{
			if (pLua)
			{
				pLua->ReferencePush(iReference);
				pLua->SetUserType(-1, NULL);
				pLua->Pop(1);
				Util::ReferenceFree(pLua, iReference, "LuaUserData::~LuaUserData(UserData)");
			}

			iReference = -1;
		}

		if (iTableReference != -1)
		{
			if (pLua)
				Util::ReferenceFree(pLua, iTableReference, "LuaUserData::~LuaUserData(Table)");

			iTableReference = -1;
		}

		pAdditionalData = NULL;

		if (pBaseData != NULL)
		{
			pBaseData->Release(this);
			pBaseData = NULL;
		}
	}

	inline void Init(GarrysMod::Lua::ILuaInterface* LUA, int type)
	{
		pLua = LUA;
		iType = (unsigned char)type;
	}

	inline void EnsureLuaTable()
	{
		if (iTableReference == -1 && pLua)
		{
			pLua->CreateTable();
			iTableReference = Util::ReferenceCreate(pLua, "LuaUserData::EnsureLuaTable");
		}
	}

	inline void CreateReference()
	{
		if (iReference != -1)
			Warning("holylib: something went wrong when pushing userdata! (Reference leak!)\n");

		pLua->Push(-1); // When CreateReference is called we expect our object to be already be on the stack at -1!
		iReference = Util::ReferenceCreate(pLua, "LuaUserData::CreateReference");
	}

	inline void* GetData()
	{
		return pBaseData ? pBaseData->GetData(pLua) : NULL;
	}

	inline void SetData(void* data)
	{
		if (pBaseData != NULL)
		{
			pBaseData->Release(this);
			pBaseData = NULL;
		}

		pBaseData = BaseUserData::LuaAquire(this, data);
	}

	inline int GetAdditionalData()
	{
		return pAdditionalData;
	}

	inline void SetAdditionalData(int data)
	{
		pAdditionalData = data;
	}

	inline int GetLuaTable()
	{
		EnsureLuaTable();
		return iTableReference;
	}

	inline void ClearLuaTable()
	{
		if (iTableReference != -1 && pLua)
		{
			Util::ReferenceFree(pLua, iTableReference, "LuaUserData::ClearLuaTable");
			iTableReference = -1;
		}
	}

	inline int GetReference()
	{
		return iReference;
	}

	inline int GetType()
	{
		return iType;
	}

	inline bool Push()
	{
		if (iReference == -1)
			return false;

		Util::ReferencePush(pLua, iReference);
		return true;
	}

	inline bool Release()
	{
		if (pBaseData != NULL)
		{
			bool bDelete = pBaseData->Release(this);
			pBaseData = NULL;
			delete this;
			return bDelete;
		}

		delete this;
		return false;
	}

private:
	BaseUserData* pBaseData = NULL;
	int iReference = -1;
	int iTableReference = -1;
	int pAdditionalData = NULL; // Used by HLTVClient.
	GarrysMod::Lua::ILuaInterface* pLua = NULL;
	unsigned char iType = GarrysMod::Lua::Type::Nil;
};

inline void BaseUserData::ForceGlobalRelease(void* pData)
{
	auto it = g_pGlobalLuaUserData.find(pData);
	if (it == g_pGlobalLuaUserData.end())
		return;

	it->second->m_iReferenceCount = 0;
	for (auto& userData : it->second->m_pOwningData)
	{
		userData->SetData(NULL); // Remove any references any LuaUserData holds to us.
	}

	it->second->Release(NULL);
}

#define TO_LUA_TYPE( className ) Lua::className

// BUG: This LuaClass function and all others were made in mind to support a single Lua Instance. Now we got multiple.
#define MakeString( str1, str2, str3 ) ((std::string)str1).append(str2).append(str3)
#define Get_LuaClass( className, strName ) \
static std::string invalidType_##className = MakeString("Tried to use something that wasn't a ", strName, "!"); \
static std::string triedNull_##className = MakeString("Tried to use a NULL ", strName, "!"); \
LuaUserData* Get_##className##_Data(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError) \
{ \
	int luaType = Lua::GetLuaData(LUA)->GetMetaTable(TO_LUA_TYPE(className)); \
	if (!LUA->IsType(iStackPos, luaType)) \
	{ \
		if (bError) \
			LUA->ThrowError(invalidType_##className.c_str()); \
\
		return NULL; \
	} \
\
	LuaUserData* pVar = LUA->GetUserType<LuaUserData>(iStackPos, luaType); \
	if ((!pVar || !pVar->GetData()) && bError) \
		LUA->ThrowError(triedNull_##className.c_str()); \
\
	return pVar; \
} \
\
className* Get_##className(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError) \
{ \
	LuaUserData* pLuaData = Get_##className##_Data(LUA, iStackPos, bError); \
	if (!pLuaData) \
		return NULL; \
 \
	return (className*)pLuaData->GetData(); \
}

#define GMODPush_LuaClass( className, luaType ) \
void Push_##className(GarrysMod::Lua::ILuaInterface* LUA, className* var) \
{ \
	if (!var) \
	{ \
		LUA->PushNil(); \
		return; \
	} \
\
	LUA->PushUserType(var, luaType); \
}

#define GMODGet_LuaClass( className, luaType, strName, additional ) \
static std::string invalidType_##className = MakeString("Tried to use something that wasn't a ", strName, "!"); \
static std::string triedNull_##className = MakeString("Tried to use a NULL ", strName, "!"); \
className* Get_##className(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError) \
{ \
	if (!LUA->IsType(iStackPos, luaType)) \
	{ \
		if (bError) \
			LUA->ThrowError(invalidType_##className.c_str()); \
\
		return NULL; \
	} \
\
	className* pVar = LUA->GetUserType<className>(iStackPos, luaType); \
	if (!pVar && bError) \
		LUA->ThrowError(triedNull_##className.c_str()); \
\
	additional \
\
	return pVar; \
}

// Only used by CBaseClient & CHLTVClient as they both work with each other / a CHLTVClient can use all CBaseClient functions.
#define SpecialGet_LuaClass( className, className2, strName ) \
static std::string invalidType_##className = MakeString("Tried to use something that wasn't a ", strName, "!"); \
static std::string triedNull_##className = MakeString("Tried to use a NULL ", strName, "!"); \
LuaUserData* Get_##className##_Data(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError) \
{ \
	Lua::StateData* LUADATA = Lua::GetLuaData(LUA); \
	int luaType = LUADATA->GetMetaTable(TO_LUA_TYPE(className)); \
	int luaType2 = LUADATA->GetMetaTable(TO_LUA_TYPE(className2)); \
\
	int iType = LUA->GetType(iStackPos); \
	if (iType != luaType && iType != luaType2) \
	{ \
		if (bError) \
			LUA->ThrowError(invalidType_##className.c_str()); \
\
		return NULL; \
	} \
\
	if (iType == luaType) \
	{ \
		LuaUserData* pVar = LUA->GetUserType<LuaUserData>(iStackPos, luaType); \
		if (pVar) \
			return pVar; \
	} \
\
	if (iType == luaType2) \
	{ \
		LuaUserData* pVar = LUA->GetUserType<LuaUserData>(iStackPos, luaType2); \
		if (pVar) \
			return pVar; \
	} \
 \
	if (bError) \
		LUA->ThrowError(triedNull_##className.c_str()); \
\
	return NULL; \
} \
\
className* Get_##className(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError) \
{ \
	LuaUserData* pLuaData = Get_##className##_Data(LUA, iStackPos, bError); \
	if (!pLuaData) \
		return NULL; \
 \
	return (className*)pLuaData->GetData(); \
}

#define Push_LuaClass( className ) \
LuaUserData* Push_##className(GarrysMod::Lua::ILuaInterface* LUA, className* var) \
{ \
	if (!var) \
	{ \
		LUA->PushNil(); \
		return NULL; \
	} \
\
	LuaUserData* userData = new LuaUserData; \
	userData->SetData(var); \
	int iMeta = Lua::GetLuaData(LUA)->GetMetaTable(TO_LUA_TYPE(className)); \
	userData->Init(LUA, iMeta); \
	LUA->PushUserType(userData, iMeta); \
	return userData; \
}

/*
 * This one is special
 * the GC WONT free the LuaClass meaning this "could" (and did in the past) cause a memory/reference leak
 * The data thats passed won't be freed by lua.
 */
#pragma warning(disable:4505) // Why would we need a warning if a function got removed because its unused.
#define PushReferenced_LuaClass( className ) \
void Push_##className(GarrysMod::Lua::ILuaInterface* LUA, className* var) \
{ \
	if (!var) \
	{ \
		LUA->PushNil(); \
		return; \
	} \
\
	auto pushedUserData = Lua::GetLuaData(LUA)->GetPushedUserData(); \
	auto it = pushedUserData.find(var); \
	if (it != pushedUserData.end()) \
	{ \
		LUA->ReferencePush(it->second->GetReference()); \
	} else { \
		LuaUserData* userData = new LuaUserData; \
		userData->SetData(var); \
		int iMeta = Lua::GetLuaData(LUA)->GetMetaTable(TO_LUA_TYPE(className)); \
		LUA->PushUserType(userData, iMeta); \
		userData->Init(LUA, iMeta); \
		userData->CreateReference(); \
		pushedUserData[var] = userData; \
	} \
} \
\
[[maybe_unused]] static void Delete_##className(GarrysMod::Lua::ILuaInterface* LUA, className* var) \
{ \
	auto pushedUserData = Lua::GetLuaData(LUA)->GetPushedUserData(); \
	auto it = pushedUserData.find(var); \
	if (it != pushedUserData.end()) \
	{ \
		it->second->Release(); \
		pushedUserData.erase(it); \
	} \
} \
\
[[maybe_unused]] static void DeleteAll_##className(GarrysMod::Lua::ILuaInterface* LUA) \
{ \
	Lua::StateData* LUADATA = Lua::GetLuaData(LUA); \
	int luaType = LUADATA->GetMetaTable(TO_LUA_TYPE(className)); \
	auto pushedUserData = Lua::GetLuaData(LUA)->GetPushedUserData(); \
	for (auto it = pushedUserData.begin(); it != pushedUserData.end(); ) \
	{ \
		if (it->second->GetType() == luaType) \
		{ \
			it->second->Release(); \
			it = pushedUserData.erase(it); \
		} else { \
			it++; \
		} \
	} \
	pushedUserData.clear(); \
} \
\
[[maybe_unused]] static void DeleteGlobal_##className(className* var) \
{ \
	BaseUserData::ForceGlobalRelease(var); \
}

#define Vector_RemoveElement(vec, element) \
{ \
	auto _it = std::find((vec).begin(), (vec).end(), (element)); \
	if (_it != (vec).end()) \
		(vec).erase(_it); \
}

// A default index function for userData,
// which handles everything like finding the functions in the metatable
// or getting vars on the object like: print(userData.value)
#define Default__index(className) \
LUA_FUNCTION_STATIC(className ## __index) \
{ \
	if (LUA->FindOnObjectsMetaTable(1, 2)) \
		return 1; \
\
	LUA->Pop(1); \
	Util::ReferencePush(LUA, Get_##className##_Data(LUA, 1, true)->GetLuaTable()); \
	if (!LUA->FindObjectOnTable(-1, 2)) \
		LUA->PushNil(); \
\
	LUA->Remove(-2); \
\
	return 1; \
}

// A default newindex function for userData,
// handles setting vars on the object like: userData.value = true
#define Default__newindex(className) \
LUA_FUNCTION_STATIC(className ## __newindex) \
{ \
	Util::ReferencePush(LUA, Get_##className##_Data(LUA, 1, true)->GetLuaTable()); \
	LUA->Push(2); \
	LUA->Push(3); \
	LUA->RawSet(-3); \
	LUA->Pop(1); \
\
	return 0; \
}

// A default gc function for userData,
// handles garbage collection, inside the func argument you can use pStoredData
// Use the pStoredData variable as pData->GetData() will return NULL. Just see how it's done inside the bf_read gc definition.
// NOTE: You need to manually delete the data inside the callback function -> delete pStoredData
// WARNING: DO NOT USE pData as it will be invalid because of the Release call!
#define Default__gc(className, func) \
LUA_FUNCTION_STATIC(className ## __gc) \
{ \
	LuaUserData* pData = Get_##className##_Data(LUA, 1, false); \
	if (pData) \
	{ \
		LUA->SetUserType(1, NULL); \
		void* pStoredData = pData->GetData(); \
		if (pData->Release()) \
		{ \
			pData = NULL; /*Don't let any idiot(that's me) use it*/ \
			func \
		} \
	} \
 \
	return 0; \
} \

// Default function for GetTable. Simple.
#define Default__GetTable(className) \
LUA_FUNCTION_STATIC(className ## _GetTable) \
{ \
	Util::ReferencePush(LUA, Get_##className##_Data(LUA, 1, true)->GetLuaTable()); \
	return 1; \
}

/*
 * Shouldn't we have a Default__eq?
 * No.
 * Because if we push the same object, we will ALWAYS use PushReferenced_LuaClass
 * and since it's the same userdata thats reference pushed, we won't need a __eq
 */

// Push functions from modules: 
// ToDo: move this at a later point into a seperate file. Maybe into _modules?
Vector* Get_Vector(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError = true);
QAngle* Get_QAngle(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError = true);

class bf_read;
/*
 * It will be deleted by Lua GC.
 * ensure that either you set it to NULL afterwards or push a Copy if you don't own the original.
 */
extern LuaUserData* Push_bf_read(GarrysMod::Lua::ILuaInterface* LUA, bf_read* tbl);
extern bf_read* Get_bf_read(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError);

class bf_write;
/*
 * It will be deleted by Lua GC.
 * ensure that either you set it to NULL afterwards or push a Copy if you don't own the original.
 */
extern LuaUserData* Push_bf_write(GarrysMod::Lua::ILuaInterface* LUA, bf_write* tbl);
extern bf_write* Get_bf_write(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError);

class IGameEvent;
extern IGameEvent* Get_IGameEvent(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError);

class IRecipientFilter;
extern IRecipientFilter* Get_IRecipientFilter(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError);

class ConVar;
extern ConVar* Get_ConVar(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError);

struct EntityList // entitylist module.
{
	EntityList();
	~EntityList();

	void Clear();
	inline bool IsValidReference(int iReference) { return iReference != -1; };
	void CreateReference(CBaseEntity* pEntity);
	void FreeEntity(CBaseEntity* pEntity);

	inline void EnsureReference(CBaseEntity* pEntity, int iReference)
	{
		if (!IsValidReference(iReference))
			CreateReference(pEntity);
	}

	inline void AddEntity(CBaseEntity* pEntity, bool bCreateReference = false)
	{
		m_pEntities.push_back(pEntity);
		if (bCreateReference)
			CreateReference(pEntity);
		else
			m_pEntReferences[pEntity] = -1;
	}

	inline const std::unordered_map<CBaseEntity*, int>& GetReferences()
	{
		return m_pEntReferences;
	}

	inline const std::vector<CBaseEntity*>& GetEntities()
	{
		return m_pEntities;
	}

	inline void Invalidate()
	{
		Clear();
		m_pLua = NULL;
	}

	inline void SetLua(GarrysMod::Lua::ILuaInterface* pLua)
	{
		m_pLua = pLua;
	}

private:
	// NOTE: The Entity will always be valid but the reference can be -1!
	std::unordered_map<CBaseEntity*, int> m_pEntReferences;
	std::vector<CBaseEntity*> m_pEntities;
	GarrysMod::Lua::ILuaInterface* m_pLua = NULL;
};

extern EntityList& GetGlobalEntityList(GarrysMod::Lua::ILuaInterface* pLua);

extern bool Is_EntityList(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos);
extern EntityList* Get_EntityList(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError);

extern void BlockGameEvent(const char* pName);
extern void UnblockGameEvent(const char* pName);

class CBaseClient;
extern void Push_CBaseClient(GarrysMod::Lua::ILuaInterface* LUA, CBaseClient* tbl);
extern CBaseClient* Get_CBaseClient(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError);

extern void Push_QAngle(GarrysMod::Lua::ILuaInterface* LUA, QAngle* var);
extern void Push_Vector(GarrysMod::Lua::ILuaInterface* LUA, Vector* var);