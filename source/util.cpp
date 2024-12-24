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

// Try not to use it. We want to move away from it.
// Additionaly, we will add checks in many functions.
GarrysMod::Lua::ILuaInterface* g_Lua;

IVEngineServer* engine;
CGlobalEntityList* Util::entitylist = NULL;
CUserMessages* Util::pUserMessages;

CBasePlayer* Util::Get_Player(int iStackPos, bool bError) // bError = error if not a valid player
{
	EHANDLE* pEntHandle = g_Lua->GetUserType<EHANDLE>(iStackPos, GarrysMod::Lua::Type::Entity);
	if (!pEntHandle)
	{
		if (bError)
			g_Lua->ThrowError("Tried to use a NULL Entity!");

		return NULL;
	}
	
	CBaseEntity* pEntity = Util::entitylist->GetBaseEntity(*pEntHandle);
	if (!pEntity->IsPlayer())
	{
		if (bError)
			g_Lua->ThrowError("Player entity is NULL or not a player (!?)");

		return NULL;
	}

	return (CBasePlayer*)pEntity;
}

IModuleWrapper* Util::pEntityList;
void Util::Push_Entity(CBaseEntity* pEnt)
{
	if (!pEnt)
	{
		g_Lua->GetField(LUA_GLOBALSINDEX, "NULL");
		return;
	}

	GarrysMod::Lua::CLuaObject* pObject = (GarrysMod::Lua::CLuaObject*)pEnt->GetLuaEntity();
	if (!pObject)
	{
		g_Lua->GetField(LUA_GLOBALSINDEX, "NULL");
		return;
	}

	Util::ReferencePush(pObject->GetReference()); // Assuming the reference is always right.
}

CBaseEntity* Util::Get_Entity(int iStackPos, bool bError)
{
	EHANDLE* pEntHandle = g_Lua->GetUserType<EHANDLE>(iStackPos, GarrysMod::Lua::Type::Entity);
	if (!pEntHandle && bError)
		g_Lua->ThrowError("Tried to use a NULL Entity!");

	CBaseEntity* pEntity = Util::entitylist->GetBaseEntity(*pEntHandle);
	if (!pEntity && bError)
		g_Lua->ThrowError("Tried to use a NULL Entity! (The weird case?)");
		
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
	return Util::servergameents->EdictToBaseEntity(edict);
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

IGet* Util::get;
CBaseEntityList* g_pEntityList = NULL;
Symbols::lua_rawseti Util::func_lua_rawseti;
Symbols::lua_rawgeti Util::func_lua_rawgeti;
IGameEventManager2* Util::gameeventmanager;
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

	SourceSDK::FactoryLoader lua_shared_loader("lua_shared");
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

Get_LuaClass(IRecipientFilter, GarrysMod::Lua::Type::RecipientFilter, "RecipientFilter")
Get_LuaClass(Vector, GarrysMod::Lua::Type::Vector, "Vector")
Get_LuaClass(QAngle, GarrysMod::Lua::Type::Angle, "Angle")
Get_LuaClass(ConVar, GarrysMod::Lua::Type::ConVar, "ConVar")