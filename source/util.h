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

extern GarrysMod::Lua::ILuaInterface* g_Lua;

struct edict_t;
class CBaseEntity;
class CBasePlayer;
class CBaseClient;
class CGlobalEntityList;
class CUserMessages;
class IServerGameClients;
class IServerGameEnts;
class IServer;
namespace Util
{
	inline void StartTable() {
		g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		g_Lua->CreateTable();
	}

	inline void AddFunc(GarrysMod::Lua::CFunc Func, const char* Name) {
		g_Lua->PushCFunction(Func);
		g_Lua->SetField(-2, Name);
	}

	inline void AddValue(int value, const char* Name) {
		g_Lua->PushNumber(value);
		g_Lua->SetField(-2, Name);
	}

	inline void FinishTable(const char* Name) {
		g_Lua->SetField(-2, Name);
		g_Lua->Pop();
	}

	inline void NukeTable(const char* pName)
	{
		g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		g_Lua->PushNil();
		g_Lua->SetField(-2, pName);
		g_Lua->Pop(1);
	}

	inline bool PushTable(const char* pName)
	{
		g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		g_Lua->GetField(-1, pName);
		g_Lua->Remove(-2);
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

	extern CBaseEntity* GetCBaseEntityFromEdict(edict_t* edict);

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

// This one is special
#define PushReferenced_LuaClass( className, luaType ) \
static std::unordered_map<className*, int> g_pPushed##className; \
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
		g_Lua->ReferencePush(it->second); \
	} else { \
		g_Lua->PushUserType(var, luaType); \
		g_Lua->Push(-1); \
		g_pPushed##className[var] = g_Lua->ReferenceCreate(); \
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
	std::vector<CBaseEntity*> pEntities;
	std::unordered_map<short, CBaseEntity*> pEdictHash;
};
extern bool Is_EntityList(int iStackPos);
extern EntityList* Get_EntityList(int iStackPos, bool bError);