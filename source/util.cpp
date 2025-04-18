#include "util.h"
#include "GarrysMod/Lua/LuaObject.h"
#include <string>
#include "GarrysMod/InterfacePointers.hpp"
#include "sourcesdk/baseclient.h"
#include "iserver.h"
#include "module.h"
#include "icommandline.h"
#include "player.h"
#include "detours.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Try not to use it. We want to move away from it.
// Additionaly, we will add checks in many functions.
GarrysMod::Lua::ILuaInterface* g_Lua;

IVEngineServer* engine;
CGlobalEntityList* Util::entitylist = NULL;
CUserMessages* Util::pUserMessages;

bool g_pRemoveLuaUserData = true;
std::unordered_set<LuaUserData*> g_pLuaUserData;
std::unordered_map<void*, BaseUserData*> g_pGlobalLuaUserData;

std::unordered_set<int> Util::g_pReference;
ConVar Util::holylib_debug_mainutil("holylib_debug_mainutil", "1");

CBasePlayer* Util::Get_Player(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError) // bError = error if not a valid player
{
	if (!entitylist) // In case we don't have a entitylist, fallback to continue working.
	{
		GarrysMod::Lua::ILuaObject* pObj = LUA->NewTemporaryObject(); // NOTE: This doesn't actually allocate a new object, it reuses one of 32 objects that were created for this.
		pObj->SetFromStack(iStackPos);
		CBaseEntity* pEntity = pObj->GetEntity();

		if (!pEntity && bError)
			LUA->ThrowError("Tried to use a NULL Entity!");

		return (CBasePlayer*)pEntity;
	}

	EHANDLE* pEntHandle = LUA->GetUserType<EHANDLE>(iStackPos, GarrysMod::Lua::Type::Entity);
	if (!pEntHandle)
	{
		if (bError)
			LUA->ThrowError("Tried to use a NULL Entity!");

		return NULL;
	}
	
	CBaseEntity* pEntity = Util::entitylist->GetBaseEntity(*pEntHandle);
	if (!pEntity->IsPlayer())
	{
		if (bError)
			LUA->ThrowError("Player entity is NULL or not a player (!?)");

		return NULL;
	}

	return (CBasePlayer*)pEntity;
}

IModuleWrapper* Util::pEntityList;
void Util::Push_Entity(GarrysMod::Lua::ILuaInterface* LUA, CBaseEntity* pEnt)
{
	if (!pEnt)
	{
		LUA->GetField(LUA_GLOBALSINDEX, "NULL");
		return;
	}

	if (LUA == g_Lua)
	{
		GarrysMod::Lua::CLuaObject* pObject = (GarrysMod::Lua::CLuaObject*)pEnt->GetLuaEntity();
		if (!pObject)
		{
			LUA->GetField(LUA_GLOBALSINDEX, "NULL");
			return;
		}

		Util::ReferencePush(LUA, pObject->GetReference()); // Assuming the reference is always right.
	} else {
		Warning("holylib: tried to push a entity, but this wasn't implemented for other lua states yet!\n");
 		LUA->PushNil();
	}
}

CBaseEntity* Util::Get_Entity(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError)
{
	if (!entitylist) // In case we don't have a entitylist, fallback to continue working.
	{
		GarrysMod::Lua::ILuaObject* pObj = LUA->NewTemporaryObject(); // NOTE: This doesn't actually allocate a new object, it reuses one of 32 objects that were created for this.
		pObj->SetFromStack(iStackPos);
		CBaseEntity* pEntity = pObj->GetEntity();

		if (!pEntity && bError)
			LUA->ThrowError("Tried to use a NULL Entity!");

		return pEntity;
	}

	EHANDLE* pEntHandle = LUA->GetUserType<EHANDLE>(iStackPos, GarrysMod::Lua::Type::Entity);
	if (!pEntHandle && bError)
		LUA->ThrowError("Tried to use a NULL Entity!");

	CBaseEntity* pEntity = Util::entitylist->GetBaseEntity(*pEntHandle);
	if (!pEntity && bError)
		LUA->ThrowError("Tried to use a NULL Entity! (The weird case?)");
		
	return pEntity;
}

IServer* Util::server;
CBaseClient* Util::GetClientByUserID(int userid)
{
	for (int i = 0; i < Util::server->GetClientCount(); i++)
	{
		IClient* pClient = Util::server->GetClient(i);
		if ( pClient && pClient->GetUserID() == userid)
			return (CBaseClient*)pClient;
	}

	return NULL;
}

IVEngineServer* Util::engineserver = NULL;
IServerGameEnts* Util::servergameents = NULL;
IServerGameClients* Util::servergameclients = NULL;
CBaseClient* Util::GetClientByPlayer(const CBasePlayer* ply)
{
	return Util::GetClientByUserID(Util::engineserver->GetPlayerUserId(((CBaseEntity*)ply)->edict()));
}

CBaseClient* Util::GetClientByIndex(int index)
{
	if (server->GetClientCount() <= index || index < 0)
		return NULL;

	return (CBaseClient*)server->GetClient(index);
}

std::vector<CBaseClient*> Util::GetClients()
{
	std::vector<CBaseClient*> pClients;

	for (int i = 0; i < server->GetClientCount(); i++)
	{
		IClient* pClient = server->GetClient(i);
		pClients.push_back((CBaseClient*)pClient);
	}

	return pClients;
}

CBasePlayer* Util::GetPlayerByClient(CBaseClient* client)
{
	return (CBasePlayer*)servergameents->EdictToBaseEntity(engineserver->PEntityOfEntIndex(client->GetPlayerSlot() + 1));
}

byte Util::g_pCurrentCluster[MAX_MAP_LEAFS / 8];
void Util::ResetClusers()
{
	Q_memset(Util::g_pCurrentCluster, 0, sizeof(Util::g_pCurrentCluster));
}

Symbols::CM_Vis func_CM_Vis = NULL;
void Util::CM_Vis(const Vector& orig, int type)
{
	Util::ResetClusers();

	if (func_CM_Vis)
		func_CM_Vis(Util::g_pCurrentCluster, sizeof(Util::g_pCurrentCluster), engine->GetClusterForOrigin(orig), type);
}

bool Util::CM_Vis(byte* cluster, int clusterSize, int clusterID, int type)
{
	if (func_CM_Vis)
		func_CM_Vis(cluster, clusterSize, clusterID, type);
	else
		return false;

	return true;
}


static Symbols::CBaseEntity_CalcAbsolutePosition func_CBaseEntity_CalcAbsolutePosition;
void CBaseEntity::CalcAbsolutePosition(void)
{
	func_CBaseEntity_CalcAbsolutePosition(this);
}

static Symbols::CCollisionProperty_MarkSurroundingBoundsDirty func_CCollisionProperty_MarkSurroundingBoundsDirty;
void CCollisionProperty::MarkSurroundingBoundsDirty()
{
	func_CCollisionProperty_MarkSurroundingBoundsDirty(this);
}

CBaseEntity* Util::GetCBaseEntityFromEdict(edict_t* edict)
{
	if (!edict)
		return NULL;

	return Util::servergameents->EdictToBaseEntity(edict);
}

CBaseEntity* Util::FirstEnt()
{
	if (Util::entitylist)
		return Util::entitylist->FirstEnt();

	if (!Util::engineserver)
		return NULL; // We can't continue like this...

	return Util::GetCBaseEntityFromEdict(Util::engineserver->PEntityOfEntIndex(0)); // Return the world as the start
}

CBaseEntity* Util::NextEnt(CBaseEntity* pEnt)
{
	if (Util::entitylist)
		return Util::entitylist->NextEnt(pEnt);

	if (!Util::engineserver)
		return NULL; // We can't continue like this...

	int nextIndex = pEnt->edict()->m_EdictIndex + 1;
	int totalCount = Util::engineserver->GetEntityCount();
	if (totalCount <= nextIndex) // ToDo: Verify that we don't skip the last entitiy.
		return NULL;

	CBaseEntity* pEntity = Util::GetCBaseEntityFromEdict(Util::engineserver->PEntityOfEntIndex(nextIndex));
	while (!pEntity && totalCount > nextIndex) // Search for the next entity, we stop if the next index reaches the total count of edicts.
	{
		pEntity = Util::GetCBaseEntityFromEdict(Util::engineserver->PEntityOfEntIndex(++nextIndex));
	}

	return pEntity;
}

class HolyEntityListener : public IEntityListener
{
public:
	virtual void OnEntityCreated(CBaseEntity* pEntity)
	{
		g_pModuleManager.OnEntityCreated(pEntity);
	}

	virtual void OnEntitySpawned(CBaseEntity* pEntity)
	{
		g_pModuleManager.OnEntitySpawned(pEntity);
	}
	
	virtual void OnEntityDeleted(CBaseEntity* pEntity)
	{
		g_pModuleManager.OnEntityDeleted(pEntity);
	}
};
static HolyEntityListener pHolyEntityListener;

static Detouring::Hook detour_CSteam3Server_NotifyClientDisconnect;
extern void GameServer_OnClientDisconnect(CBaseClient* pClient);
extern void SourceTV_OnClientDisconnect(CBaseClient* pClient);
static void hook_CSteam3Server_NotifyClientDisconnect(void* pServer, CBaseClient* pClient)
{
	VPROF_BUDGET("HolyLib - CSteam3Server::NotifyClientDisconnect", VPROF_BUDGETGROUP_HOLYLIB);

	GameServer_OnClientDisconnect(pClient);
	SourceTV_OnClientDisconnect(pClient);
	detour_CSteam3Server_NotifyClientDisconnect.GetTrampoline<Symbols::CSteam3Server_NotifyClientDisconnect>()(pServer, pClient);
}

IGet* Util::get;
CBaseEntityList* g_pEntityList = NULL;
Symbols::lua_rawseti Util::func_lua_rawseti;
Symbols::lua_rawgeti Util::func_lua_rawgeti;
IGameEventManager2* Util::gameeventmanager;
IServerGameDLL* Util::servergamedll;
void Util::AddDetour()
{
	if (g_pModuleManager.GetAppFactory())
		engineserver = (IVEngineServer*)g_pModuleManager.GetAppFactory()(INTERFACEVERSION_VENGINESERVER, NULL);
	else
		engineserver = InterfacePointers::VEngineServer();
	Detour::CheckValue("get interface", "IVEngineServer", engineserver != NULL);
	
	SourceSDK::FactoryLoader engine_loader("engine");
	if (g_pModuleManager.GetAppFactory())
		gameeventmanager = (IGameEventManager2*)g_pModuleManager.GetAppFactory()(INTERFACEVERSION_GAMEEVENTSMANAGER2, NULL);
	else
		gameeventmanager = engine_loader.GetInterface<IGameEventManager2>(INTERFACEVERSION_GAMEEVENTSMANAGER2);
	Detour::CheckValue("get interface", "IGameEventManager", gameeventmanager != NULL);

	SourceSDK::FactoryLoader server_loader("server");
	pUserMessages = Detour::ResolveSymbol<CUserMessages>(server_loader, Symbols::UsermessagesSym);
	Detour::CheckValue("get class", "usermessages", pUserMessages != NULL);

	if (g_pModuleManager.GetAppFactory())
		servergameents = (IServerGameEnts*)g_pModuleManager.GetGameFactory()(INTERFACEVERSION_SERVERGAMEENTS, NULL);
	else
		servergameents = server_loader.GetInterface<IServerGameEnts>(INTERFACEVERSION_SERVERGAMEENTS);
	Detour::CheckValue("get interface", "IServerGameEnts", servergameents != NULL);

	if (g_pModuleManager.GetAppFactory())
		servergameclients = (IServerGameClients*)g_pModuleManager.GetGameFactory()(INTERFACEVERSION_SERVERGAMECLIENTS, NULL);
	else
		servergameclients = server_loader.GetInterface<IServerGameClients>(INTERFACEVERSION_SERVERGAMECLIENTS);
	Detour::CheckValue("get interface", "IServerGameClients", servergameclients != NULL);

	if (g_pModuleManager.GetAppFactory())
		servergamedll = (IServerGameDLL*)g_pModuleManager.GetGameFactory()(INTERFACEVERSION_SERVERGAMEDLL, NULL);
	else
		servergamedll = server_loader.GetInterface<IServerGameDLL>(INTERFACEVERSION_SERVERGAMEDLL);
	Detour::CheckValue("get interface", "IServerGameDLL", servergamedll != NULL);

	Detour::Create(
		&detour_CSteam3Server_NotifyClientDisconnect, "CSteam3Server::NotifyClientDisconnect",
		engine_loader.GetModule(), Symbols::CSteam3Server_NotifyClientDisconnectSym,
		(void*)hook_CSteam3Server_NotifyClientDisconnect, 0
	);

	server = InterfacePointers::Server();
	Detour::CheckValue("get class", "IServer", server != NULL);

	entitylist = Detour::ResolveSymbol<CGlobalEntityList>(server_loader, Symbols::gEntListSym);
	Detour::CheckValue("get class", "gEntList", entitylist != NULL);
	g_pEntityList = entitylist;
	if (entitylist)
		entitylist->AddListenerEntity(&pHolyEntityListener);

#ifdef ARCHITECTURE_X86 // We don't use it on 64x, do we. Look into pas_FindInPAS to see how we do it ^^
	get = Detour::ResolveSymbol<IGet>(server_loader, Symbols::CGetSym);
	Detour::CheckValue("get class", "IGet", get != NULL);
#endif

	func_CM_Vis = (Symbols::CM_Vis)Detour::GetFunction(engine_loader.GetModule(), Symbols::CM_VisSym);
	Detour::CheckFunction((void*)func_CM_Vis, "CM_Vis");

	SourceSDK::ModuleLoader lua_shared_loader("lua_shared");
	func_lua_rawseti = (Symbols::lua_rawseti)Detour::GetFunction(lua_shared_loader.GetModule(), Symbols::lua_rawsetiSym);
	Detour::CheckFunction((void*)func_lua_rawseti, "lua_rawseti");

	func_lua_rawgeti = (Symbols::lua_rawgeti)Detour::GetFunction(lua_shared_loader.GetModule(), Symbols::lua_rawgetiSym);
	Detour::CheckFunction((void*)func_lua_rawgeti, "lua_rawgeti");

	pEntityList = g_pModuleManager.FindModuleByName("entitylist");

	/*
	 * IMPORTANT TODO
	 * 
	 * We now will run in the menu state so if we try to push an entity or so, we may push it in the wrong realm!
	 * How will we handle multiple realms?
	 * 
	 * Idea: Fk menu, if there is a server realm, we'll use it. If not, we wait for one to start.
	 *		We also could introduce a Lua Flag so that modules can register for Menu/Client realm if wanted.
	 *		But I won't really support client. At best only menu.
	 * 
	 * New Idea: I'm updating everything. The goal is to support any realm & even multiple ILuaInterfaces at the same time (Preperation for lua_threaded support).
	 */

#ifndef SYSTEM_WINDOWS
	func_CBaseEntity_CalcAbsolutePosition = (Symbols::CBaseEntity_CalcAbsolutePosition)Detour::GetFunction(server_loader.GetModule(), Symbols::CBaseEntity_CalcAbsolutePositionSym);
	Detour::CheckFunction((void*)func_CBaseEntity_CalcAbsolutePosition, "CBaseEntity::CalcAbsolutePosition");

	func_CCollisionProperty_MarkSurroundingBoundsDirty = (Symbols::CCollisionProperty_MarkSurroundingBoundsDirty)Detour::GetFunction(server_loader.GetModule(), Symbols::CCollisionProperty_MarkSurroundingBoundsDirtySym);
	Detour::CheckFunction((void*)func_CCollisionProperty_MarkSurroundingBoundsDirty, "CCollisionProperty::MarkSurroundingBoundsDirty");
#endif
}

void Util::RemoveDetour()
{
	if (Util::entitylist)
		Util::entitylist->RemoveListenerEntity(&pHolyEntityListener);
}

// If HolyLib was already loaded, we won't load a second time.
// How could this happen?
// In cases some other module utilizes HolyLib/compiled it to a .lib and uses the lib file they can load/execute HolyLib themself.
// & yes, you can compile HolyLib to a .lib file & load it using the 
static bool g_pShouldLoad = false;
bool Util::ShouldLoad()
{
	if (CommandLine()->FindParm("-holylibexists") && !g_pShouldLoad) // Don't set this manually!
		return false;

	if (g_pShouldLoad)
		return true;

	g_pShouldLoad = true;
	CommandLine()->AppendParm("-holylibexists", "true");

	return true;
}

void Util::CheckVersion()
{
	// ToDo: Implement this someday
}

GMODGet_LuaClass(IRecipientFilter, GarrysMod::Lua::Type::RecipientFilter, "RecipientFilter")
GMODGet_LuaClass(Vector, GarrysMod::Lua::Type::Vector, "Vector")
GMODGet_LuaClass(QAngle, GarrysMod::Lua::Type::Angle, "Angle")
GMODGet_LuaClass(ConVar, GarrysMod::Lua::Type::ConVar, "ConVar")

GMODPush_LuaClass(QAngle, GarrysMod::Lua::Type::Angle)
GMODPush_LuaClass(Vector, GarrysMod::Lua::Type::Vector)