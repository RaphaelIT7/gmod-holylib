#pragma once

#include <lua/ILuaInterface.h>
#include "Platform.hpp"
#include "vprof.h"
#include <unordered_map>
#include <algorithm>
#include "symbols.h"

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
namespace Util
{
	#define LUA_REGISTRYINDEX	(-10000)
	#define LUA_ENVIRONINDEX	(-10001)
	#define LUA_GLOBALSINDEX	(-10002)

	/*
	 * RawSetI & RawGetI are way faster but Gmod doesn't expose or even use them :(
	 */
	extern Symbols::lua_rawseti func_lua_rawseti;
	inline void RawSetI(int iStackPos, int iValue)
	{
		func_lua_rawseti(g_Lua->GetState(), iStackPos, iValue);
	}

	extern Symbols::lua_rawgeti func_lua_rawgeti;
	inline void RawGetI(int iStackPos, int iValue)
	{
		func_lua_rawgeti(g_Lua->GetState(), iStackPos, iValue);
	}

	/*
	 * Why do we have the same code here when CLuaInterface::ReferencePush does exactly the same?
	 * Because like this we should hopefully skip possibly funny code & the vtable call.
	 * 
	 * NOTE: It does seem to be faster so were going to keep this.
	 */
	inline void ReferencePush(int iReference)
	{
		func_lua_rawgeti(g_Lua->GetState(), LUA_REGISTRYINDEX, iReference);
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
	Update: Nope, we'll do it the old way but properly this time. Time to redo most of the lua functions.
*/

// BUG: This LuaClass function and all others were made in mind to support a single Lua Instance. Now we got multiple.
#define MakeString( str1, str2, str3 ) ((std::string)str1).append(str2).append(str3)
#define Get_LuaClass( className, luaType, strName ) \
static std::string invalidType_##className = MakeString("Tried to use something that wasn't a ", strName, "!"); \
static std::string triedNull_##className = MakeString("Tried to use a NULL ", strName, "!"); \
className* Get_##className(GarrysMod::Lua::ILuaInterface* pLua, int iStackPos, bool bError) \
{ \
	if (!pLua->IsType(iStackPos, luaType)) \
	{ \
		if (bError) \
			pLua->ThrowError(invalidType_##className.c_str()); \
\
		return NULL; \
	} \
\
	className* pVar = pLua->GetUserType<className>(iStackPos, luaType); \
	if (!pVar && bError) \
		pLua->ThrowError(triedNull_##className.c_str()); \
\
	return pVar; \
}

#define Push_LuaClass( className, luaType ) \
void Push_##className(GarrysMod::Lua::ILuaInterface* pLua, className* var) \
{ \
	if (!var) \
	{ \
		pLua->PushNil(); \
		return; \
	} \
\
	pLua->PushUserType(var, luaType); \
}

/*
	Idea: Use the LuaUserData directly to push onto the stack instead of making a reference.
*/
struct LuaUserData { // ToDo: Maybe implement this also for other things? (Probably not)
	LuaUserData(GarrysMod::Lua::ILuaInterface* pLua)
	{
		m_pLua = pLua;
	}

	~LuaUserData() {
		/*if (!ThreadInMainThread())
		{
			Warning("holylib: Tried to delete usetdata from another thread!\n");
			return;
		}*/

		if (m_iReference != -1)
		{
			m_pLua->ReferencePush(m_iReference);
			m_pLua->SetUserType(-1, NULL);
			m_pLua->Pop(1);
			m_pLua->ReferenceFree(m_iReference);
			m_iReference = -1;
		}

		if (m_iTableReference != -1)
		{
			m_pLua->ReferenceFree(m_iTableReference);
			m_iTableReference = -1;
		}

		m_pAdditionalData = NULL;
	}

	// Creates a reference of the object on top of the stack and pops it.
	inline void Set()
	{
		m_iReference = m_pLua->ReferenceCreate();
		
		if (m_iTableReference != -1)
			m_pLua->ReferenceFree(m_iTableReference);

		m_pLua->CreateTable();
		m_iTableReference = m_pLua->ReferenceCreate();
	}

	inline void Push()
	{
		if (m_iReference != -1)
			m_pLua->ReferencePush(m_iReference);
	}

	inline void PushTable()
	{
		if (m_iTableReference != -1)
			m_pLua->ReferencePush(m_iTableReference);
	}

	inline int GetData()
	{
		return m_pAdditionalData;
	}

	int m_iReference = -1;
	int m_iTableReference = -1;
	int m_pAdditionalData = NULL; // Used by HLTVClient.
	GarrysMod::Lua::ILuaInterface* m_pLua = NULL;
};

struct LuaInterfaceData
{
public:
	LuaInterfaceData(GarrysMod::Lua::ILuaInterface* pLua)
	{
		m_pLua = pLua;
	}

	~LuaInterfaceData()
	{
		for (auto&[_, val] : m_pLuaClasses)
			delete val;
	}

	inline LuaUserData* AddLuaClass(void* pObject, int iType)
	{
		LuaUserData* userData = new LuaUserData(m_pLua);
		m_pLuaClasses[pObject] = userData;
		m_pLua->PushUserType(pObject, iType);
		userData->Set();

		return userData;
	}

	inline LuaUserData* FindLuaClass(void* pObject)
	{
		auto it = m_pLuaClasses.find(pObject);
		if (it != m_pLuaClasses.end())
			return it->second;

		return NULL; // This should probably never happen
	}

	inline void DeleteLuaClass(void* pObject)
	{
		auto it = m_pLuaClasses.find(pObject);
		if (it != m_pLuaClasses.end())
		{
			delete it->second;
			m_pLuaClasses.erase(it);
		} 
	}

private:
	GarrysMod::Lua::ILuaInterface* m_pLua = NULL;

	std::unordered_map<void*, LuaUserData*> m_pLuaClasses;

	// ToDo: Add a field to store module data.
};

extern std::unordered_map<GarrysMod::Lua::ILuaInterface*, LuaInterfaceData*> g_pLuaInterfaceData;

// Will NEVER return NULL.
inline LuaInterfaceData* GetLuaInterfaceData(GarrysMod::Lua::ILuaInterface* pLua)
{
	auto it = g_pLuaInterfaceData.find(pLua);
	if (it != g_pLuaInterfaceData.end())
		return it->second;

	Error("holylib: Tried to get lua interface data while it was NULL! (Should NEVER happen, report this!)\n");
}

// Should be called ONCE per interface.
inline void CreateLuaInterfaceData(GarrysMod::Lua::ILuaInterface* pLua)
{
	auto it = g_pLuaInterfaceData.find(pLua);
	if (it != g_pLuaInterfaceData.end())
		Error("holylib: Tried to create lua interface data while it already had some! (Should NEVER happen, report this!)\n");

	g_pLuaInterfaceData[pLua] = new LuaInterfaceData(pLua);
}

inline void DestroyLuaInterfaceData(GarrysMod::Lua::ILuaInterface* pLua)
{
	auto it = g_pLuaInterfaceData.find(pLua);
	if (it == g_pLuaInterfaceData.end())
		Error("holylib: Tried to destroy lua interface data while it was never created! (Should NEVER happen, report this!)\n");

	delete it->second;
	g_pLuaInterfaceData.erase(it);
}

// This one is special
#define PushReferenced_LuaClass( className, luaType ) \
void Push_##className(GarrysMod::Lua::ILuaInterface* pLua, className* var) \
{ \
	if (!var) \
	{ \
		pLua->PushNil(); \
		return; \
	} \
\
	LuaInterfaceData* pInterface = GetLuaInterfaceData(pLua); \
	LuaUserData* pData = pInterface->FindLuaClass(var); \
	if (pData) \
	{ \
		pData->Push(); \
	} else { \
		pInterface->AddLuaClass(var, luaType)->Push(); \
	} \
} \
\
static void Delete_##className(GarrysMod::Lua::ILuaInterface* pLua, className* var) \
{ \
	GetLuaInterfaceData(pLua)->DeleteLuaClass(var); \
}

#define Vector_RemoveElement(vec, element) \
{ \
    auto _it = std::find((vec).begin(), (vec).end(), (element)); \
    if (_it != (vec).end()) \
        (vec).erase(_it); \
}

// Push functions from modules: 
// ToDo: move this at a later point into a seperate file. Maybe into _modules?
Vector* Get_Vector(GarrysMod::Lua::ILuaInterface* pLua, int iStackPos, bool bError = true);
QAngle* Get_QAngle(GarrysMod::Lua::ILuaInterface* pLua, int iStackPos, bool bError = true);

class bf_read;
extern void Push_bf_read(GarrysMod::Lua::ILuaInterface* pLua, bf_read* tbl);

class bf_write;
extern void Push_bf_write(GarrysMod::Lua::ILuaInterface* pLua, bf_write* tbl);
extern bf_write* Get_bf_write(GarrysMod::Lua::ILuaInterface* pLua, int iStackPos, bool bError);

class IGameEvent;
extern IGameEvent* Get_IGameEvent(GarrysMod::Lua::ILuaInterface* pLua, int iStackPos, bool bError);

class IRecipientFilter;
extern IRecipientFilter* Get_IRecipientFilter(GarrysMod::Lua::ILuaInterface* pLua, int iStackPos, bool bError);

class ConVar;
extern ConVar* Get_ConVar(GarrysMod::Lua::ILuaInterface* pLua, int iStackPos, bool bError);

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

extern bool Is_EntityList(GarrysMod::Lua::ILuaInterface* pLua, int iStackPos);
extern EntityList* Get_EntityList(GarrysMod::Lua::ILuaInterface* pLua, int iStackPos, bool bError);

extern void BlockGameEvent(const char* pName);
extern void UnblockGameEvent(const char* pName);