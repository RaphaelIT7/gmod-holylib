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

	/*
	 * RawSetI & RawGetI are way faster but Gmod doesn't expose or even use them :(
	 */
	extern Symbols::lua_rawseti func_lua_rawseti;
	inline void RawSetI(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, int iValue)
	{
		if (func_lua_rawseti)
		{
			func_lua_rawseti(LUA->GetState(), iStackPos, iValue);
		} else {
			LUA->PushNumber(iValue);
			LUA->Push(-2);
			LUA->Remove(-3);
			LUA->RawSet(iStackPos + 1);
		}
	}

	extern Symbols::lua_rawgeti func_lua_rawgeti;
	inline void RawGetI(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, int iValue)
	{
		if (func_lua_rawgeti)
		{
			func_lua_rawgeti(LUA->GetState(), iStackPos, iValue);
		} else {
			LUA->PushNumber(iValue);
			LUA->RawGet(iStackPos + 1);
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
	inline int ReferenceCreate(const char* reason)
	{
		int iReference = g_Lua->ReferenceCreate();

#if HOLYLIB_UTIL_DEBUG_REFERENCES
		if (holylib_debug_mainutil.GetBool())
			Msg("holylib: Created reference %i (%s)\n", iReference, reason);

		auto it = g_pReference.find(iReference);
		if (it != g_pReference.end())
		{
			Error("holylib: Created a reference when we already holded it. How. Crash this shit. (%s)\n", reason); // If this happens maybe gmod does some weird shit?
		}

		g_pReference.insert(iReference);
#endif

		return iReference;
	}

	inline void ReferenceFree(int iReference, const char* reason)
	{
#if HOLYLIB_UTIL_DEBUG_REFERENCES
		if (holylib_debug_mainutil.GetBool())
			Msg("holylib: Freed reference %i (%s)\n", iReference, reason);

		auto it = g_pReference.find(iReference);
		if (it == g_pReference.end())
		{
			Error("holylib: Freed a reference when we didn't holded it. How. Crash this shit. (%s)\n", reason); // If this happens I'm happy.
		}

		g_pReference.erase(it);
#endif

		g_Lua->ReferenceFree(iReference);
	}

	inline void StartTable() {
		g_Lua->CreateTable();
	}

	inline void AddFunc(GarrysMod::Lua::CFunc Func, const char* Name) {
		g_Lua->PushString(Name);
		g_Lua->PushCFunction(Func);
		g_Lua->RawSet(-3);
	}

	inline void AddValue(double value, const char* Name) {
		g_Lua->PushString(Name);
		g_Lua->PushNumber(value);
		g_Lua->RawSet(-3);
	}

	inline void FinishTable(const char* Name) {
		g_Lua->SetField(LUA_GLOBALSINDEX, Name);
	}

	inline void NukeTable(const char* pName)
	{
		g_Lua->PushNil();
		g_Lua->SetField(LUA_GLOBALSINDEX, pName);
	}

	inline bool PushTable(const char* pName)
	{
		g_Lua->GetField(LUA_GLOBALSINDEX, pName);
		if (g_Lua->IsType(-1, GarrysMod::Lua::Type::Table))
			return true;

		g_Lua->Pop(1);
		return false;
	}

	inline void PopTable()
	{
		g_Lua->Pop(1);
	}

	inline void RemoveField(const char* pName)
	{
		g_Lua->PushNil();
		g_Lua->SetField(-2, pName);
	}

	inline bool HasField(const char* pName, int iType)
	{
		g_Lua->GetField(-1, pName);
		return g_Lua->IsType(-1, iType);
	}

	// Gmod's functions:
	extern CBasePlayer* Get_Player(int iStackPos, bool unknown);
	extern CBaseEntity* Get_Entity(int iStackPos, bool unknown);
	extern void Push_Entity(CBaseEntity* pEnt);
	extern CBaseEntity* GetCBaseEntityFromEdict(edict_t* edict);

	extern void AddDetour(); // We load Gmod's functions in there.
	extern void RemoveDetour();

	extern CBaseClient* GetClientByUserID(int userID);
	extern CBaseClient* GetClientByPlayer(const CBasePlayer* ply);
	extern CBaseClient* GetClientByIndex(int index);
	extern std::vector<CBaseClient*> GetClients();
	extern CBasePlayer* GetPlayerByClient(CBaseClient* client);
	extern void CM_Vis(const Vector& orig, int type);
	extern bool CM_Vis(byte* cluster, int clusterSize, int cluserID, int type);
	extern void ResetClusers();
	extern bool ShouldLoad();
	extern void CheckVersion();

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
	#define MAX_MAP_LEAFS 65536
	extern byte g_pCurrentCluster[MAX_MAP_LEAFS / 8];
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
 *       as else it COULD persist across lua states which is VERY BAD as the references will ALL be INVALID.
 */

struct LuaUserData;
extern bool g_pRemoveLuaUserData;
extern std::unordered_set<LuaUserData*> g_pLuaUserData; // A set containing all LuaUserData that actually hold a reference.
struct LuaUserData {
	LuaUserData() {
		g_pLuaUserData.insert(this);
	}

	~LuaUserData() {
		if (!ThreadInMainThread())
		{
			Warning("holylib: Tried to delete usetdata from another thread!\n");

			if (g_pRemoveLuaUserData)
			{
				g_pLuaUserData.erase(this);
			}
			return;
		}

		if (g_pRemoveLuaUserData)
		{
			g_pLuaUserData.erase(this);
		}

		if (iReference != -1)
		{
			if (g_Lua)
			{
				g_Lua->ReferencePush(iReference);
				g_Lua->SetUserType(-1, NULL);
				g_Lua->Pop(1);
				Util::ReferenceFree(iReference, "LuaUserData::~LuaUserData(UserData)");
			}

			iReference = -1;
		}

		if (iTableReference != -1)
		{
			if (g_Lua)
				Util::ReferenceFree(iTableReference, "LuaUserData::~LuaUserData(Table)");

			iTableReference = -1;
		}

		pAdditionalData = NULL;
	}

	inline void Init(GarrysMod::Lua::ILuaInterface* LUA)
	{
	}

	inline void EnsureLuaTable()
	{
		if (iTableReference == -1 && g_Lua)
		{
			g_Lua->CreateTable();
			iTableReference = Util::ReferenceCreate("LuaUserData::EnsureLuaTable");
		}
	}

	inline void CreateReference()
	{
		if (iReference != -1)
			Warning("holylib: something went wrong when pushing userdata! (Reference leak!)\n");

		g_Lua->Push(-1); // When CreateReference is called we expect our object to be already be on the stack at -1!
		iReference = Util::ReferenceCreate("LuaUserData::CreateReference");
	}

	inline void* GetData()
	{
		return pData;
	}

	inline void SetData(void* data)
	{
		pData = data;
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
		if (iTableReference != -1 && g_Lua)
		{
			Util::ReferenceFree(iTableReference, "LuaUserData::ClearLuaTable");
			iTableReference = -1;
		}
	}

	inline int GetReference()
	{
		return iReference;
	}

	inline bool Push()
	{
		if (iReference == -1)
			return false;

		Util::ReferencePush(g_Lua, iReference);
		return true;
	}

private:
	void* pData = NULL;
	int iReference = -1;
	int iTableReference = -1;
	int pAdditionalData = NULL; // Used by HLTVClient.
};

// BUG: This LuaClass function and all others were made in mind to support a single Lua Instance. Now we got multiple.
#define MakeString( str1, str2, str3 ) ((std::string)str1).append(str2).append(str3)
#define Get_LuaClass( className, luaType, strName ) \
static std::string invalidType_##className = MakeString("Tried to use something that wasn't a ", strName, "!"); \
static std::string triedNull_##className = MakeString("Tried to use a NULL ", strName, "!"); \
LuaUserData* Get_##className##_Data(int iStackPos, bool bError) \
{ \
	if (!g_Lua->IsType(iStackPos, luaType)) \
	{ \
		if (bError) \
			g_Lua->ThrowError(invalidType_##className.c_str()); \
\
		return NULL; \
	} \
\
	LuaUserData* pVar = g_Lua->GetUserType<LuaUserData>(iStackPos, luaType); \
	if ((!pVar || !pVar->GetData()) && bError) \
		g_Lua->ThrowError(triedNull_##className.c_str()); \
\
	return pVar; \
} \
\
className* Get_##className(int iStackPos, bool bError) \
{ \
	LuaUserData* pLuaData = Get_##className##_Data(iStackPos, bError); \
	if (!pLuaData) \
		return NULL; \
 \
	return (className*)pLuaData->GetData(); \
}

// Only used by CBaseClient & CHLTVClient as they both work with each other / a CHLTVClient can use all CBaseClient functions.
#define SpecialGet_LuaClass( className, luaType, luaType2, strName ) \
static std::string invalidType_##className = MakeString("Tried to use something that wasn't a ", strName, "!"); \
static std::string triedNull_##className = MakeString("Tried to use a NULL ", strName, "!"); \
LuaUserData* Get_##className##_Data(int iStackPos, bool bError) \
{ \
	int iType = g_Lua->GetType(iStackPos); \
	if (iType != luaType && iType != luaType2) \
	{ \
		if (bError) \
			g_Lua->ThrowError(invalidType_##className.c_str()); \
\
		return NULL; \
	} \
\
	if (iType == luaType) \
	{ \
		LuaUserData* pVar = g_Lua->GetUserType<LuaUserData>(iStackPos, luaType); \
		if (pVar) \
			return pVar; \
	} \
\
	if (iType == luaType2) \
	{ \
		LuaUserData* pVar = g_Lua->GetUserType<LuaUserData>(iStackPos, luaType2); \
		if (pVar) \
			return pVar; \
	} \
 \
	if (bError) \
		g_Lua->ThrowError(triedNull_##className.c_str()); \
\
	return NULL; \
} \
\
className* Get_##className(int iStackPos, bool bError) \
{ \
	LuaUserData* pLuaData = Get_##className##_Data(iStackPos, bError); \
	if (!pLuaData) \
		return NULL; \
 \
	return (className*)pLuaData->GetData(); \
}

#define Push_LuaClass( className, luaType ) \
void Push_##className(className* var) \
{ \
	if (!var) \
	{ \
		g_Lua->PushNil(); \
		return; \
	} \
\
	LuaUserData* userData = new LuaUserData; \
	userData->SetData(var); \
	userData->Init(g_Lua); \
	g_Lua->PushUserType(userData, luaType); \
}

// This one is special, the GC WONT free the LuaClass meaning this "could" (and did in the past) cause a memory/reference leak
#define PushReferenced_LuaClass( className, luaType ) \
static std::unordered_map<className*, LuaUserData*> g_pPushed##className; \
void Push_##className(className* var) \
{ \
	if (!var) \
	{ \
		g_Lua->PushNil(); \
		return; \
	} \
\
	auto it = g_pPushed##className.find(var); \
	if (it != g_pPushed##className.end()) \
	{ \
		g_Lua->ReferencePush(it->second->GetReference()); \
	} else { \
		LuaUserData* userData = new LuaUserData; \
		userData->SetData(var); \
		g_Lua->PushUserType(userData, luaType); \
		userData->Init(g_Lua); \
		userData->CreateReference(); \
		g_pPushed##className[var] = userData; \
	} \
} \
\
static void Delete_##className(className* var) \
{ \
	auto it = g_pPushed##className.find(var); \
	if (it != g_pPushed##className.end()) \
	{ \
		delete it->second; \
		g_pPushed##className.erase(it); \
	} \
} \
\
static void DeleteAll_##className() \
{ \
	for (auto& [key, val] : g_pPushed##className) \
	{ \
		delete val; \
	} \
	g_pPushed##className.clear(); \
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
	Util::ReferencePush(LUA, Get_##className##_Data(1, true)->GetLuaTable()); \
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
	Util::ReferencePush(LUA, Get_##className##_Data(1, true)->GetLuaTable()); \
	LUA->Push(2); \
	LUA->Push(3); \
	LUA->RawSet(-3); \
	LUA->Pop(1); \
\
	return 0; \
}

// A default gc function for userData,
// handles garbage collection, inside the func argument you can use pData as a variable
// like pData->GetData() to get your class. Just see how it's done inside the bf_read gc definition.
#define Default__gc(className, func) \
LUA_FUNCTION_STATIC(className ## __gc) \
{ \
	LuaUserData* pData = Get_##className##_Data(1, false); \
	if (pData) \
	{ \
		func \
		delete pData; \
		LUA->SetUserType(1, NULL); \
	} \
 \
	return 0; \
} \

// Default function for GetTable. Simple.
#define Default__GetTable(className) \
LUA_FUNCTION_STATIC(className ## _GetTable) \
{ \
	Util::ReferencePush(LUA, Get_##className##_Data(1, true)->GetLuaTable()); \
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
Vector* Get_Vector(int iStackPos, bool bError = true);
QAngle* Get_QAngle(int iStackPos, bool bError = true);

class bf_read;
/*
 * It will be deleted by Lua GC.
 * ensure that either you set it to NULL afterwards or push a Copy if you don't own the original.
 */
extern void Push_bf_read(bf_read* tbl);
extern bf_read* Get_bf_read(int iStackPos, bool bError);

class bf_write;
/*
 * It will be deleted by Lua GC.
 * ensure that either you set it to NULL afterwards or push a Copy if you don't own the original.
 */
extern void Push_bf_write(bf_write* tbl);
extern bf_write* Get_bf_write(int iStackPos, bool bError);

class IGameEvent;
extern IGameEvent* Get_IGameEvent(int iStackPos, bool bError);

class IRecipientFilter;
extern IRecipientFilter* Get_IRecipientFilter(int iStackPos, bool bError);

class ConVar;
extern ConVar* Get_ConVar(int iStackPos, bool bError);

struct EntityList // entitylist module.
{
	EntityList(GarrysMod::Lua::ILuaInterface* pLua);
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
	GarrysMod::Lua::ILuaInterface* m_pLua;
};
extern EntityList g_pGlobalEntityList;

extern bool Is_EntityList(int iStackPos);
extern EntityList* Get_EntityList(int iStackPos, bool bError);

extern void BlockGameEvent(const char* pName);
extern void UnblockGameEvent(const char* pName);

class CBaseClient;
extern void Push_CBaseClient(CBaseClient* tbl);
extern CBaseClient* Get_CBaseClient(int iStackPos, bool bError);