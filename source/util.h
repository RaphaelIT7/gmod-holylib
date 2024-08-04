#pragma once

#include <sourcesdk/ILuaInterface.h>
#include "detours.h"
#include "eiface.h"
#include <tier3/tier3.h>

class IVEngineServer;
extern IVEngineServer* engine;

extern GarrysMod::Lua::IUpdatedLuaInterface* g_Lua;

class CBaseClient;
namespace Util
{
	extern void StartTable();
	extern void AddFunc(GarrysMod::Lua::CFunc, const char*);
	extern void FinishTable(const char*);

	// Gmod's functions:
	extern CBasePlayer* Get_Player(int iStackPos, bool unknown);
	extern CBaseEntity* Get_Entity(int iStackPos, bool unknown);
	extern void Push_Entity(CBaseEntity* pEnt);

	extern CBaseClient* GetClientByUserID(int userid);

	extern void AddDetour(); // We load Gmod's functions in there.
}

// Push functions from modules: 
// ToDo: move this at a later point into a seperate file. Maybe into _modules?
class bf_read;
extern void Push_bf_read(bf_read* tbl);