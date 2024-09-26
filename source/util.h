#pragma once

#include <sourcesdk/ILuaInterface.h>
#include "detours.h"
#include "eiface.h"
#include <tier3/tier3.h>
#include "vprof.h"

class IVEngineServer;
extern IVEngineServer* engine;

#define VPROF_BUDGETGROUP_HOLYLIB _T("HolyLib")

extern GarrysMod::Lua::IUpdatedLuaInterface* g_Lua;

class CBaseClient;
class CGlobalEntityList;
class CUserMessages;
namespace Util
{
	extern void StartTable();
	extern void AddFunc(GarrysMod::Lua::CFunc, const char*);
	extern void FinishTable(const char*);
	extern void NukeTable(const char*);
	extern bool PushTable(const char*);
	extern void PopTable();
	extern void RemoveFunc(const char*);

	// Gmod's functions:
	extern CBasePlayer* Get_Player(int iStackPos, bool unknown);
	extern CBaseEntity* Get_Entity(int iStackPos, bool unknown);
	extern void Push_Entity(CBaseEntity* pEnt);

	extern CBaseClient* GetClientByUserID(int userid);

	extern void AddDetour(); // We load Gmod's functions in there.
	
	extern IVEngineServer* engineserver;
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
}

// Push functions from modules: 
// ToDo: move this at a later point into a seperate file. Maybe into _modules?
class bf_read;
extern void Push_bf_read(bf_read* tbl);

class bf_write;
extern void Push_bf_write(bf_write* tbl);

class IGameEvent;
extern IGameEvent* Get_IGameEvent(int iStackPos, bool bError);

class IRecipientFilter;
extern IRecipientFilter* Get_IRecipientFilter(int iStackPos, bool bError);