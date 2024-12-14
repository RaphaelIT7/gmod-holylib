#pragma once

#include <sourcesdk/ILuaInterface.h>
#include "Platform.hpp"
#include "vprof.h"
#include <unordered_map>
#include <algorithm>

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

// Try to not use it. We want to move away from it.
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
	inline void StartTable(GarrysMod::Lua::ILuaInterface* pLua) {
		pLua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		pLua->CreateTable();
	}

	inline void AddFunc(GarrysMod::Lua::ILuaInterface* pLua, GarrysMod::Lua::CFunc Func, const char* Name) {
		pLua->PushString(Name);
		pLua->PushCFunction(Func);
		pLua->RawSet(-3);
	}

	inline void AddValue(GarrysMod::Lua::ILuaInterface* pLua, double value, const char* Name) {
		pLua->PushString(Name);
		pLua->PushNumber(value);
		pLua->RawSet(-3);
	}

	inline void FinishTable(GarrysMod::Lua::ILuaInterface* pLua, const char* Name) {
		pLua->SetField(-2, Name);
		pLua->Pop();
	}

	inline void NukeTable(GarrysMod::Lua::ILuaInterface* pLua, const char* pName)
	{
		pLua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		pLua->PushNil();
		pLua->SetField(-2, pName);
		pLua->Pop(1);
	}

	inline bool PushTable(GarrysMod::Lua::ILuaInterface* pLua, const char* pName)
	{
		pLua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		pLua->GetField(-1, pName);
		pLua->Remove(-2);
		if (pLua->IsType(-1, GarrysMod::Lua::Type::Table))
			return true;

		pLua->Pop(1);
		return false;
	}

	inline void PopTable(GarrysMod::Lua::ILuaInterface* pLua)
	{
		pLua->Pop(1);
	}

	inline void RemoveField(GarrysMod::Lua::ILuaInterface* pLua, const char* pName)
	{
		pLua->PushNil();
		pLua->SetField(-2, pName);
	}

	inline bool HasField(GarrysMod::Lua::ILuaInterface* pLua, const char* pName, int iType)
	{
		pLua->GetField(-1, pName);
		return pLua->IsType(-1, iType);
	}

	// Gmod's functions:
	extern CBasePlayer* Get_Player(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool unknown);
	extern CBaseEntity* Get_Entity(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool unknown);
	extern void Push_Entity(GarrysMod::Lua::ILuaInterface* LUA, CBaseEntity* pEnt);
	extern CBaseEntity* GetCBaseEntityFromEdict(edict_t* edict);

	extern void AddDetour(); // We load Gmod's functions in there.

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

#define MakeString( str1, str2, str3 ) ((std::string)str1).append(str2).append(str3)
#define Get_LuaClass( className, luaType, strName ) \
static std::string invalidType_##className = MakeString("Tried to use something that wasn't a ", strName, "!"); \
static std::string triedNull_##className = MakeString("Tried to use a NULL ", strName, "!"); \
className* Get_##className(int iStackPos, bool bError) \
{ \
	if (!g_Lua->IsType(iStackPos, luaType)) \
	{ \
		if (bError) \
			g_Lua->ThrowError(invalidType_##className.c_str()); \
\
		return NULL; \
	} \
\
	className* pVar = g_Lua->GetUserType<className>(iStackPos, luaType); \
	if (!pVar && bError) \
		g_Lua->ThrowError(triedNull_##className.c_str()); \
\
	return pVar; \
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
	g_Lua->PushUserType(var, luaType); \
}

struct LuaUserData { // ToDo: Maybe implement this also for other things?
	~LuaUserData() {
		if (!ThreadInMainThread())
		{
			Warning("holylib: Tried to delete usetdata from another thread!\n");
			return;
		}

		if (iReference != -1)
		{
			g_Lua->ReferencePush(iReference);
			g_Lua->SetUserType(-1, NULL);
			g_Lua->Pop(1);
			g_Lua->ReferenceFree(iReference);
			iReference = -1;
		}

		if (iTableReference != -1)
		{
			g_Lua->ReferenceFree(iTableReference);
			iTableReference = -1;
		}

		pAdditionalData = NULL;
	}

	int iReference = -1;
	int iTableReference = -1;
	int pAdditionalData = NULL; // Used by HLTVClient.
};

// This one is special
#define PushReferenced_LuaClass( className, luaType ) \
static std::unordered_map<className*, LuaUserData*> g_pPushed##className; \
static void Push_##className(className* var) \
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
		g_Lua->ReferencePush(it->second->iReference); \
	} else { \
		g_Lua->PushUserType(var, luaType); \
		g_Lua->Push(-1); \
		LuaUserData* userData = new LuaUserData; \
		userData->iReference = g_Lua->ReferenceCreate(); \
		g_Lua->CreateTable(); \
		userData->iTableReference = g_Lua->ReferenceCreate(); \
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
}

#define Vector_RemoveElement(vec, element) \
{ \
    auto _it = std::find((vec).begin(), (vec).end(), (element)); \
    if (_it != (vec).end()) \
        (vec).erase(_it); \
}

// Push functions from modules: 
// ToDo: move this at a later point into a seperate file. Maybe into _modules?
Vector* Get_Vector(int iStackPos, bool bError = true);
QAngle* Get_QAngle(int iStackPos, bool bError = true);

class bf_read;
extern void Push_bf_read(bf_read* tbl);

class bf_write;
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
	EntityList();
	~EntityList();
	void Clear();
	std::unordered_map<CBaseEntity*, int> pEntReferences;
	std::vector<CBaseEntity*> pEntities;
	std::unordered_map<short, CBaseEntity*> pEdictHash;
};
extern EntityList g_pGlobalEntityList;

extern bool Is_EntityList(int iStackPos);
extern EntityList* Get_EntityList(int iStackPos, bool bError);
extern void UpdateGlobalEntityList(GarrysMod::Lua::ILuaInterface* pLua);