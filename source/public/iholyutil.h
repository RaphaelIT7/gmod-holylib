#pragma once

#include "interface.h"
#include <vector>

class CBasePlayer;
class CBaseEntity;
class CBaseClient;
class IVEngineServer;
class IServerGameClients;
class IServerGameEnts;
class IServer;
class CGlobalEntityList;
class CUserMessages;
class IGameEventManager2;
struct edict_t;

class IHolyUtil
{
public: // First all variables
	virtual IVEngineServer* GetEngineServer() = 0;
	virtual IServerGameClients* GetServerGameClients() = 0;
	virtual IServerGameEnts* GetServerGameEnts() = 0;
	virtual IServer* GetGameServer() = 0;
	virtual CGlobalEntityList* GetGlobalEntityList() = 0;
	virtual CUserMessages* GetUserMessages() = 0;
	virtual IGameEventManager2* GetGameEventManager() = 0;

	// This is the current PVS/PAS used by HolyLib.
	// Modifying it will change the behavior of the functions using it.
	virtual byte* GetCurrentCluster() = 0;

public: // Then all functions
	// Bind to Gmod's Get_Player function
	// Will return the given CBasePlayer that is at the given Stack.
	// If bError is false, it will simply return NULL instead on failure.
	virtual CBasePlayer* Get_Player(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError) = 0;

	// Bind to Gmod's Get_Entity function
	// Will return the given CBaseClient that is at the given Stack.
	//  If bError is false, it will simply return NULL instead on failure.
	virtual CBaseEntity* Get_Entity(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError) = 0;

	// Bind to Gmod's Push_Entity function
	// Will Push the given Entity onto the lua stack.
	// Verify: If given NULL it will push the NULL entity?
	virtual void Push_Entity(GarrysMod::Lua::ILuaInterface* LUA, CBaseEntity* pEntity) = 0;

	// Returns the CBaseEntity from the given editct.
	// Verify: This is used since some funny stuff happens if you try to do it yourself?
	virtual CBaseEntity* GetCBaseEntityFromEdict(edict_t* edict) = 0;

	// Returns the CBaseClient from the given userID.
	// This function is slower since it loops thru all clients.
	virtual CBaseClient* GetClientByUserID(int userID) = 0;

	// Returns the CBaseClient from the given CBasePlayer.
	// This is faster since we don't loop thru all the clients.
	// NOTE: Internally uses GetClientByIndex to get the CBaseClient.
	virtual CBaseClient* GetClientByPlayer(const CBasePlayer* ply) = 0;

	// Returns the CBaseClient from the given player slot.
	// NOTE: Internally uses sv->GetClient(playerSlot)
	virtual CBaseClient* GetClientByIndex(int playerSlot) = 0;

	// Returns a std::vector containing all CBaseClients.
	virtual std::vector<CBaseClient*> GetClients() = 0;

	// Returns the CBasePlayer from the given CBaseClient.
	virtual CBasePlayer* GetPlayerByClient(CBaseClient* client) = 0;

	// A bind to CM_Vis allowing you to load the PVS/PAS into the given cluster.
	// type = DVIS_PAS(1) or DVIS_PVS(0)
	virtual bool CM_Vis(byte* cluster, int clusterSize, int cluserID, int type) = 0;
};

// Should we make this also a interface? :^
// #define INTERFACEVERSION_HOLYUTIL "IHOLYUTIL001"