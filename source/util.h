#pragma once

#include <sourcesdk/ILuaInterface.h>
#include "Platform.hpp"
#include "eiface.h"
#include "vprof.h"

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

extern GarrysMod::Lua::IUpdatedLuaInterface* g_Lua;

class CBasePlayer;
class CBaseClient;
class CGlobalEntityList;
class CUserMessages;
namespace Util
{
	extern void StartTable();
	extern void AddValue(int, const char*);
	extern void AddFunc(GarrysMod::Lua::CFunc, const char*);
	extern void FinishTable(const char*);
	extern void NukeTable(const char*);
	extern bool PushTable(const char*);
	extern void PopTable();
	extern void RemoveField(const char*);

	// Gmod's functions:
	extern CBasePlayer* Get_Player(int iStackPos, bool unknown);
	extern CBaseEntity* Get_Entity(int iStackPos, bool unknown);
	extern void Push_Entity(CBaseEntity* pEnt);

	extern CBaseClient* GetClientByUserID(int userid);

	extern void AddDetour(); // We load Gmod's functions in there.
	
	extern IVEngineServer* engineserver;
	extern IServerGameClients* servergameclients;
	extern IServerGameEnts* servergameents;
	extern IServer* server;
	extern CGlobalEntityList* entitylist;
	extern CUserMessages* pUserMessages;

	inline CBaseEntity* GetCBaseEntityFromEdict(edict_t* edict)
	{
		return servergameents->EdictToBaseEntity(edict);
	}

	inline edict_t* GetEdictOfEnt(CBaseEntity* entity)
	{
		return servergameents->BaseEntityToEdict(entity);
	}

	extern CBaseClient* GetClientByPlayer(CBasePlayer* ply);
	extern CBaseClient* GetClientByIndex(int index);
	extern std::vector<CBaseClient*> GetClients();
	extern CBasePlayer* GetPlayerByClient(CBaseClient* client);

	extern bool ShouldLoad();

	inline void StartThreadPool(IThreadPool* pool, ThreadPoolStartParams_t& startParams)
	{
#if ARCHITECTURE_IS_X86_64
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
void Push_##className(##className* var) \
{ \
	if (!var) \
	{ \
		g_Lua->PushNil(); \
		return; \
	} \
\
	g_Lua->PushUserType(var, luaType); \
}

// This one is special
#define PushReferenced_LuaClass( className, luaType ) \
static std::unordered_map<##className*, int> g_pPushed##className; \
static void Push_##className(##className* var) \
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
		g_Lua->ReferencePush(it->second); \
	} else { \
		g_Lua->PushUserType(var, luaType); \
		g_Lua->Push(-1); \
		g_pPushed##className[var] = g_Lua->ReferenceCreate(); \
	} \
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