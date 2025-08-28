#include "httplib.h"
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
#include "toolframework/itoolentity.h"
#include "GarrysMod/IGet.h"
#include <lua.h>
#include "versioninfo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Try not to use it. We want to move away from it.
// Additionaly, we will add checks in many functions.
GarrysMod::Lua::ILuaInterface* g_Lua = nullptr;

IVEngineServer* engine = nullptr;
CGlobalEntityList* Util::entitylist = nullptr;
CUserMessages* Util::pUserMessages = nullptr;

std::unordered_set<LuaUserData*> g_pLuaUserData;
#if HOLYLIB_UTIL_BASEUSERDATA && HOLYLIB_UTIL_GLOBALUSERDATA 
std::shared_mutex g_UserDataMutex;
std::unordered_map<void*, BaseUserData*> g_pGlobalLuaUserData;
#endif

std::unordered_set<int> Util::g_pReference;
ConVar Util::holylib_debug_mainutil("holylib_debug_mainutil", "1");

// We require this here since we depend on the Lua namespace
void LuaUserData::ForceGlobalRelease(void* pData)
{
#if HOLYLIB_UTIL_DEBUG_BASEUSERDATA
	Msg("holylib - util: Global release for Userdata %p (%p) got aquired %i\n", it->second, pData, it->second->m_iReferenceCount);
#endif

	bool bFound = false;
	const std::unordered_set<Lua::StateData*> pStateData = Lua::GetAllLuaData();
	for (Lua::StateData* pState : pStateData)
	{
		std::unordered_map<void*, LuaUserData*> owningData = pState->GetPushedUserData(); // Copy it over in case it->second gets deleted while iterating
		auto it2 = owningData.find(pData);
		if (it2 == owningData.end())
			continue;
		
		bFound = true;
		for (auto& [_, userData] : owningData)
		{
			if (userData->GetData())
			{
				userData->SetData(nullptr); // Remove any references any LuaUserData holds to us.
			}
		}
	}

	if (!bFound)
		return;

	/*
		We need to pull it again since the it->second might have now been deleted.
		This is because SetData internally releases the UserData it holds and the UserData will delete itself if all references were freed
	*/
	for (Lua::StateData* pState : pStateData)
	{
		std::unordered_map<void*, LuaUserData*> owningData = pState->GetPushedUserData(); // Copy it over in case it->second gets deleted while iterating
		auto it2 = owningData.find(pData);
		if (it2 == owningData.end())
			continue;

		// Reference leak case. This should normally never happen.
#if HOLYLIB_UTIL_BASEUSERDATA 
//#if HOLYLIB_UTIL_DEBUG_BASEUSERDATA
		Warning(PROJECT_NAME " - BaseUserData: Found a reference leak while deleting it! (%p, %p, %i)\n", pData, it2->second, it2->second->GetReferenceCount());
//#endif

		it2->second-> = 1; // Set it to 1 because Release will only fully execute if there aren't any other references left.
		it2->second->Release(nullptr);
#else
		it2->second->Release(pState->pLua);
#endif
	}
}

CBasePlayer* Util::Get_Player(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError) // bError = error if not a valid player
{
	if (!Util::entitylist) // In case we don't have a entitylist, fallback to continue working.
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

		return nullptr;
	}
	
	CBaseEntity* pEntity = Util::entitylist->GetBaseEntity(*pEntHandle);
	if (!pEntity->IsPlayer())
	{
		if (bError)
			LUA->ThrowError("Player entity is NULL or not a player (!?)");

		return nullptr;
	}

	return (CBasePlayer*)pEntity;
}

IModuleWrapper* Util::pEntityList;
static Symbols::CBaseEntity_GetLuaEntity func_CBaseEntity_GetLuaEntity = nullptr;
void Util::Push_Entity(GarrysMod::Lua::ILuaInterface* LUA, CBaseEntity* pEnt)
{
	if (!pEnt)
	{
		LUA->GetField(LUA_GLOBALSINDEX, "NULL");
		return;
	}

	// In the case we are missing our symbol for CBaseEntity::GetLuaEntity, this will be slower though will still be fully functional.
	if (!func_CBaseEntity_GetLuaEntity)
	{
		GarrysMod::Lua::ILuaObject* pObj = LUA->NewTemporaryObject();
		pObj->SetEntity(pEnt);
		pObj->Push();
		return;
	}

	if (LUA == g_Lua)
	{
		GarrysMod::Lua::CLuaObject* pObject = (GarrysMod::Lua::CLuaObject*)func_CBaseEntity_GetLuaEntity(pEnt);
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
	if (!Util::entitylist) // In case we don't have a entitylist, fallback to continue working.
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

IServer* Util::server = nullptr;
CBaseClient* Util::GetClientByUserID(int userid)
{
	for (int i=0; i < Util::server->GetClientCount(); ++i)
	{
		IClient* pClient = Util::server->GetClient(i);
		if (pClient && pClient->GetUserID() == userid)
			return (CBaseClient*)pClient;
	}

	return nullptr;
}

IVEngineServer* Util::engineserver = nullptr;
IServerGameEnts* Util::servergameents = nullptr;
IServerGameClients* Util::servergameclients = nullptr;
CBaseClient* Util::GetClientByPlayer(const CBasePlayer* ply)
{
	return Util::GetClientByUserID(Util::engineserver->GetPlayerUserId(((CBaseEntity*)ply)->edict()));
}

CBaseClient* Util::GetClientByIndex(int index)
{
	if (server->GetClientCount() <= index || index < 0)
		return nullptr;

	return (CBaseClient*)server->GetClient(index);
}

std::vector<CBaseClient*> Util::GetClients()
{
	std::vector<CBaseClient*> pClients;

	int clientCount = Util::server->GetClientCount();
	pClients.reserve(clientCount);

	for (int i = 0; i < clientCount; ++i)
	{
		IClient* pClient = Util::server->GetClient(i);
		if (pClient)
		pClients.push_back((CBaseClient*)pClient);
	}

	return pClients;
}

CBasePlayer* Util::GetPlayerByClient(CBaseClient* client)
{
	return (CBasePlayer*)servergameents->EdictToBaseEntity(engineserver->PEntityOfEntIndex(client->GetPlayerSlot() + 1));
}

void Util::ResetClusers(VisData* data)
{
	Q_memset(data->cluster, 0, sizeof(data->cluster));
}

Symbols::CM_Vis func_CM_Vis = nullptr;
Util::VisData* Util::CM_Vis(const Vector& orig, int type)
{
	Util::VisData* data = new Util::VisData;
	Util::ResetClusers(data);

	if (func_CM_Vis)
		func_CM_Vis(data->cluster, sizeof(data->cluster), engine->GetClusterForOrigin(orig), type);

	return data;
}

bool Util::CM_Vis(byte* cluster, int clusterSize, int clusterID, int type)
{
	if (func_CM_Vis)
		func_CM_Vis(cluster, clusterSize, clusterID, type);
	else
		return false;

	return true;
}


static Symbols::CBaseEntity_CalcAbsolutePosition func_CBaseEntity_CalcAbsolutePosition = nullptr;
void CBaseEntity::CalcAbsolutePosition(void)
{
	if (func_CBaseEntity_CalcAbsolutePosition)
	{
		func_CBaseEntity_CalcAbsolutePosition(this);
	} else {
		Warning(PROJECT_NAME " - Tried to use missing CBaseEntity::CalcAbsolutePosition!\n");
	}
}

static Symbols::CCollisionProperty_MarkSurroundingBoundsDirty func_CCollisionProperty_MarkSurroundingBoundsDirty = nullptr;
void CCollisionProperty::MarkSurroundingBoundsDirty()
{
	if (func_CCollisionProperty_MarkSurroundingBoundsDirty)
	{
		func_CCollisionProperty_MarkSurroundingBoundsDirty(this);
	} else {
		Warning(PROJECT_NAME " - Tried to use missing CCollisionProperty::MarkSurroundingBoundsDirty!\n");
	}
}

CBaseEntity* Util::GetCBaseEntityFromEdict(edict_t* edict)
{
	if (!edict)
		return nullptr;

	return Util::servergameents->EdictToBaseEntity(edict);
}

CBaseEntity* Util::FirstEnt()
{
	if (Util::entitylist)
		return Util::entitylist->FirstEnt();

	if (!Util::engineserver)
		return nullptr; // We can't continue like this...

	return Util::GetCBaseEntityFromEdict(Util::engineserver->PEntityOfEntIndex(0)); // Return the world as the start
}

CBaseEntity* Util::NextEnt(CBaseEntity* pEnt)
{
	if (Util::entitylist)
		return Util::entitylist->NextEnt(pEnt);

	if (!Util::engineserver)
		return nullptr; // We can't continue like this...

	int nextIndex = pEnt->edict()->m_EdictIndex + 1;
	int totalCount = Util::engineserver->GetEntityCount();
	if (totalCount <= nextIndex) // ToDo: Verify that we don't skip the last entitiy.
		return nullptr;

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
static void hook_CSteam3Server_NotifyClientDisconnect(void* pServer, CBaseClient* pClient)
{
	VPROF_BUDGET("HolyLib - CSteam3Server::NotifyClientDisconnect", VPROF_BUDGETGROUP_HOLYLIB);

	g_pModuleManager.OnClientDisconnect(pClient);

	detour_CSteam3Server_NotifyClientDisconnect.GetTrampoline<Symbols::CSteam3Server_NotifyClientDisconnect>()(pServer, pClient);
}

static HSteamPipe hSteamPipe;
static HSteamUser hSteamUser;
static ISteamUser* g_pSteamUser = nullptr;
void ShutdownSteamUser()
{
	// Warning("ShutdownSteamUser called! %p\n", g_pSteamUser);
	if (!g_pSteamUser)
		return;

	//NOTE: Fking don't deal with this BS, steam just loves to crash no matter what, the SteamGameServer shutdown should already do the job so it should be fine.
	ISteamClient* pSteamClient = SteamGameServerClient();
	if (pSteamClient)
	{
		if (hSteamPipe)
		{
			pSteamClient->ReleaseUser(hSteamPipe, hSteamUser);
			pSteamClient->BReleaseSteamPipe(hSteamPipe);
			hSteamPipe = NULL;
			hSteamUser = NULL;
		}
	}

	g_pSteamUser = NULL;
	hSteamPipe = NULL;
	hSteamUser = NULL;
	// Warning("Nuked g_pSteamUser\n");
}

void CreateSteamUserIfMissing()
{
	// Warning("CreateSteamUserIfMissing called! %p\n", g_pSteamUser);
	if (!g_pSteamUser)
	{
		if (SteamUser())
		{
			g_pSteamUser = SteamUser();
			return;
		}

		//if (Util::get != NULL)
		//	g_pSteamUser = Util::get->SteamUser();

		ISteamClient* pSteamClient = SteamGameServerClient();

		if (pSteamClient)
		{
			hSteamPipe = pSteamClient->CreateSteamPipe();
			hSteamUser = pSteamClient->CreateLocalUser(&hSteamPipe, k_EAccountTypeAnonUser);
			g_pSteamUser = pSteamClient->GetISteamUser(hSteamUser, hSteamPipe, "SteamUser023");

			// Warning("CreateSteamUserIfMissing done! %p\n", g_pSteamUser);
		}
	}
}

ISteamUser* Util::GetSteamUser()
{
	CreateSteamUserIfMissing(); // We may still fail.
	return g_pSteamUser;
}

static Detouring::Hook detour_SteamGameServer_Shutdown;
static void hook_SteamGameServer_Shutdown()
{
	// Their taken care of already when shutting down.
	// Previous Bug: The voicechat caused a crash because this function was called before we were shutting down which caused us to later try to release a invalid steam user.
	ShutdownSteamUser();

	Warning("SteamGameServer_Shutdown called!\n");
	detour_SteamGameServer_Shutdown.GetTrampoline<Symbols::SteamGameServer_Shutdown>()();
}

IGet* Util::get = nullptr;
CBaseEntityList* g_pEntityList = nullptr;
Symbols::lua_rawseti Util::func_lua_rawseti = nullptr;
Symbols::lua_rawgeti Util::func_lua_rawgeti = nullptr;
IGameEventManager2* Util::gameeventmanager = nullptr;
IServerGameDLL* Util::servergamedll = nullptr;
void Util::AddDetour()
{
	if (g_pModuleManager.GetAppFactory())
		engineserver = (IVEngineServer*)g_pModuleManager.GetAppFactory()(INTERFACEVERSION_VENGINESERVER, nullptr);
	else
		engineserver = InterfacePointers::VEngineServer();
	Detour::CheckValue("get interface", "IVEngineServer", engineserver != nullptr);
	
	SourceSDK::FactoryLoader engine_loader("engine");
	if (g_pModuleManager.GetAppFactory())
		gameeventmanager = (IGameEventManager2*)g_pModuleManager.GetAppFactory()(INTERFACEVERSION_GAMEEVENTSMANAGER2, nullptr);
	else
		gameeventmanager = engine_loader.GetInterface<IGameEventManager2>(INTERFACEVERSION_GAMEEVENTSMANAGER2);
	Detour::CheckValue("get interface", "IGameEventManager", gameeventmanager != nullptr);

	SourceSDK::FactoryLoader server_loader("server");
	pUserMessages = Detour::ResolveSymbol<CUserMessages>(server_loader, Symbols::UsermessagesSym);
	Detour::CheckValue("get class", "usermessages", pUserMessages != nullptr);

	if (g_pModuleManager.GetAppFactory())
		servergameents = (IServerGameEnts*)g_pModuleManager.GetGameFactory()(INTERFACEVERSION_SERVERGAMEENTS, nullptr);
	else
		servergameents = server_loader.GetInterface<IServerGameEnts>(INTERFACEVERSION_SERVERGAMEENTS);
	Detour::CheckValue("get interface", "IServerGameEnts", servergameents != nullptr);

	if (g_pModuleManager.GetAppFactory())
		servergameclients = (IServerGameClients*)g_pModuleManager.GetGameFactory()(INTERFACEVERSION_SERVERGAMECLIENTS, nullptr);
	else
		servergameclients = server_loader.GetInterface<IServerGameClients>(INTERFACEVERSION_SERVERGAMECLIENTS);
	Detour::CheckValue("get interface", "IServerGameClients", servergameclients != nullptr);

	if (g_pModuleManager.GetAppFactory())
		servergamedll = (IServerGameDLL*)g_pModuleManager.GetGameFactory()(INTERFACEVERSION_SERVERGAMEDLL, nullptr);
	else
		servergamedll = server_loader.GetInterface<IServerGameDLL>(INTERFACEVERSION_SERVERGAMEDLL);
	Detour::CheckValue("get interface", "IServerGameDLL", servergamedll != nullptr);

	Detour::Create(
		&detour_CSteam3Server_NotifyClientDisconnect, "CSteam3Server::NotifyClientDisconnect",
		engine_loader.GetModule(), Symbols::CSteam3Server_NotifyClientDisconnectSym,
		(void*)hook_CSteam3Server_NotifyClientDisconnect, 0
	);

	SourceSDK::ModuleLoader steam_api_loader("steam_api");
	Detour::Create(
		&detour_SteamGameServer_Shutdown, "SteamGameServer_Shutdown",
		steam_api_loader.GetModule(), Symbols::SteamGameServer_ShutdownSym,
		(void*)hook_SteamGameServer_Shutdown, 0
	);

	server = InterfacePointers::Server();
	Detour::CheckValue("get class", "IServer", server != nullptr);

	IServerTools* serverTools = nullptr;
	if (g_pModuleManager.GetAppFactory())
		serverTools = (IServerTools*)g_pModuleManager.GetGameFactory()(VSERVERTOOLS_INTERFACE_VERSION, nullptr);
	else
		serverTools = server_loader.GetInterface<IServerTools>(VSERVERTOOLS_INTERFACE_VERSION);

	if (serverTools)
	{
		entitylist = serverTools->GetEntityList();
	}

	if (!entitylist)
	{
		entitylist = Detour::ResolveSymbol<CGlobalEntityList>(server_loader, Symbols::gEntListSym);
	}

	Detour::CheckValue("get class", "gEntList", entitylist != nullptr);
	g_pEntityList = entitylist;
	if (entitylist)
		entitylist->AddListenerEntity(&pHolyEntityListener);

#ifdef ARCHITECTURE_X86 // We don't use it on 64x, do we. Look into pas_FindInPAS to see how we do it ^^
	get = Detour::ResolveSymbol<IGet>(server_loader, Symbols::CGetSym);
	Detour::CheckValue("get class", "IGet", get != nullptr);
#endif

	func_CM_Vis = (Symbols::CM_Vis)Detour::GetFunction(engine_loader.GetModule(), Symbols::CM_VisSym);
	Detour::CheckFunction((void*)func_CM_Vis, "CM_Vis");

	SourceSDK::ModuleLoader lua_shared_loader("lua_shared");
	func_lua_rawseti = (Symbols::lua_rawseti)Detour::GetFunction(lua_shared_loader.GetModule(), Symbols::lua_rawsetiSym);
	Detour::CheckFunction((void*)func_lua_rawseti, "lua_rawseti");

	func_lua_rawgeti = (Symbols::lua_rawgeti)Detour::GetFunction(lua_shared_loader.GetModule(), Symbols::lua_rawgetiSym);
	Detour::CheckFunction((void*)func_lua_rawgeti, "lua_rawgeti");

	func_CBaseEntity_GetLuaEntity = (Symbols::CBaseEntity_GetLuaEntity)Detour::GetFunction(server_loader.GetModule(), Symbols::CBaseEntity_GetLuaEntitySym);
	Detour::CheckFunction((void*)func_CBaseEntity_GetLuaEntity, "CBaseEntity::GetLuaEntity");

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

	ShutdownSteamUser();
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

void Util::CheckVersion() // This is called only when holylib is initially loaded!
{
	// ToDo: Implement this someday / finish this implementation
	httplib::Client pClient("http://holylib.raphaelit7.com");

	httplib::Headers headers = {
		{ "HolyLib_Branch", HolyLib_GetBranch() },
		{ "HolyLib_RunNumber", HolyLib_GetRunNumber() },
		{ "HolyLib_Version", HolyLib_GetVersion() }
	};

	auto res = pClient.Get("/api/check_version");
	if (res->status == 200)
	{
		Bootil::Data::Tree pTree;
		if (Bootil::Data::Json::Import(pTree, res->body.c_str()))
		{
			if (pTree.GetChild("status").Value() == "ok")
			{
				Msg(PROJECT_NAME " - versioncheck: We are up to date\n");
			}
		} else {
			DevMsg(PROJECT_NAME " - versioncheck: Received invalid json!\n");
		}
	}
}

extern void LoadDLLs(); // From the dllsystem.cpp
static bool g_bUtilInit = false;
void Util::Load()
{
	if (g_bUtilInit)
		return;

	g_bUtilInit = true;

	LoadDLLs();

	IConfig* pConVarConfig = g_pConfigSystem->LoadConfig("garrysmod/holylib/cfg/convars.json");
	if (pConVarConfig)
	{
		if (pConVarConfig->GetState() == ConfigState::INVALID_JSON)
		{
			Warning(PROJECT_NAME " - core: Failed to load convars.json!\n- Check if the json is valid or delete the config to let a new one be generated!\n");
			pConVarConfig->Destroy(); // Our config is in a invaid state :/
			pConVarConfig = nullptr;
			return;
		}

		std::unordered_set<ConVar*> pSkipConVars;
		for (CModule* pWrapper : g_pModuleManager.GetModules())
		{
			if (pWrapper->GetConVar())
				pSkipConVars.insert(pWrapper->GetConVar());
			
			if (pWrapper->GetDebugConVar())
				pSkipConVars.insert(pWrapper->GetDebugConVar());
		}

		Bootil::Data::Tree& pData = pConVarConfig->GetData();
		ConCommandBase *pCur, *pNext;
		pCur = ConCommandBase::InternalConCommandBases();
		while (pCur)
		{
			pNext = pCur->InternalNext();
			if (pCur->IsCommand())
			{
				pCur = pNext;
				continue;
			}

			ConVar* pConVar = (ConVar*)pCur;
			if (pSkipConVars.find(pConVar)	!= pSkipConVars.end())
			{
				pCur = pNext;
				continue;
			}

			Bootil::BString strName = pCur->GetName();
			if (strName.length() <= 0)
			{
				pCur = pNext;
				continue;
			}

			Bootil::Data::Tree& pEntry = pData.GetChild(strName);
			Bootil::BString pDefault = pEntry.EnsureChildVar<Bootil::BString>("default", pConVar->GetDefault());
			if (pDefault == (std::string_view)pConVar->GetDefault())
			{
				pConVar->SetValue(pEntry.EnsureChildVar<Bootil::BString>("value", pConVar->GetString()).c_str());
			} else {
				pEntry.GetChild("value").Var((Bootil::BString)pConVar->GetString());
				pEntry.GetChild("default").Var((Bootil::BString)pConVar->GetDefault());
			}
			pEntry.EnsureChildVar<Bootil::BString>("help", pConVar->GetHelpText());

			pCur = pNext;
		}

		pConVarConfig->Save();
		pConVarConfig->Destroy();
	}

	IConfig* pCoreConfig = g_pConfigSystem->LoadConfig("garrysmod/holylib/cfg/core.json");
	if (pCoreConfig)
	{
		if (pCoreConfig->GetState() == ConfigState::INVALID_JSON)
		{
			Warning(PROJECT_NAME " - core: Failed to load core.json!\n- Check if the json is valid or delete the config to let a new one be generated!\n");
			pCoreConfig->Destroy(); // Our config is in a invaid state :/
			pCoreConfig = nullptr;
			return;
		}

		Bootil::Data::Tree& pData = pCoreConfig->GetData();

		// checkVersion block
		Bootil::Data::Tree& pEntry = pData.GetChild("checkVersion");
		pEntry.SetChildVar<Bootil::BString>("description", "(Unfinished implementation) If enabled, HolyLib will attempt to request the newest version from the wiki and compare them.");
		if (pEntry.EnsureChildVar<bool>("enabled", false))
		{
			Util::CheckVersion();
		}

		pCoreConfig->Save();
		pCoreConfig->Destroy();
	}
}

void Util::Unload()
{
	if (!g_bUtilInit)
		return;

	g_bUtilInit = false;
}

static void CreateDebugDump(const CCommand &args)
{
	IConfig* pDebugDump = g_pConfigSystem->LoadConfig("garrysmod/holylib/debug/dump.json");
	if (pDebugDump)
	{
		Bootil::Data::Tree& pData = pDebugDump->GetData();

		// This will contain all kind of information that could be useful to figure out an issue
		Bootil::Data::Tree& pInformation = pData.GetChild("information");
		extern const char* HolyLib_GetRunNumber();
		pInformation.EnsureChildVar<Bootil::BString>("runID", HolyLib_GetRunNumber());
		extern const char* HolyLib_GetBranch();
		pInformation.EnsureChildVar<Bootil::BString>("branch", HolyLib_GetBranch());
		pInformation.EnsureChildVar<bool>("release", HOLYLIB_BUILD_RELEASE == 1);
		pInformation.EnsureChildVar<int>("slots", Util::server->GetMaxClients());
		pInformation.EnsureChildVar<Bootil::BString>("map", Util::server->GetMapName());
		pInformation.EnsureChildVar<bool>("dedicated", Util::server->IsDedicated());
		pInformation.EnsureChildVar<int>("numclients", Util::server->GetNumClients());
		pInformation.EnsureChildVar<int>("numfakeclients", Util::server->GetNumFakeClients());
		pInformation.EnsureChildVar<int>("numproxies", Util::server->GetNumProxies());
		pInformation.EnsureChildVar<int>("tick", Util::server->GetTick());
		pInformation.EnsureChildVar<int>("tickinterval", Util::server->GetTickInterval());

		if (Util::get)
		{
			pInformation.EnsureChildVar<Bootil::BString>("version", Util::get->VersionStr());
			pInformation.EnsureChildVar<Bootil::BString>("versionTime", Util::get->VersionTimeStr());
			pInformation.EnsureChildVar<Bootil::BString>("branch", Util::get->SteamBranch());
		}

		// Dump all holylib convars.
		{
		#if ARCHITECTURE_IS_X86_64
			ICvar::Iterator iter(g_pCVar);
			for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
			{
				ConCommandBase* pCommand = iter.Get();
		#else
			for (const ConCommandBase* pCommand = g_pCVar->GetCommands(); pCommand; pCommand = pCommand->GetNext())
			{
		#endif
				if (!pCommand->IsCommand() && pCommand->GetDLLIdentifier() == *ConVar_GetDLLIdentifier())
				{
					ConVar* pConVar = (ConVar*)pCommand;
					Bootil::Data::Tree& pEntry = pData.GetChild("convars").GetChild(pConVar->GetName());
					pEntry.EnsureChildVar<Bootil::BString>("value", pConVar->GetString());
				}
			}
		}

		pDebugDump->Save();
		pDebugDump->Destroy();

		Msg(PROJECT_NAME ": Created dump inside holylib/debug/dump.json!\n");
	} else {
		Msg(PROJECT_NAME ": Failed to create debug dump!\n");
	}
}
static ConCommand createdebugdump("holylib_createdebugdump", CreateDebugDump, "Creates a debug dump that can be provided in a issue or bug report", 0);

GMODGet_LuaClass(IRecipientFilter, GarrysMod::Lua::Type::RecipientFilter, "RecipientFilter", )
GMODGet_LuaClass(Vector, GarrysMod::Lua::Type::Vector, "Vector", )
GMODGet_LuaClass(QAngle, GarrysMod::Lua::Type::Angle, "Angle", )
GMODGet_LuaClass(ConVar, GarrysMod::Lua::Type::ConVar, "ConVar", )

GMODPush_LuaClass(QAngle, GarrysMod::Lua::Type::Angle)
GMODPush_LuaClass(Vector, GarrysMod::Lua::Type::Vector)
