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

#undef isalnum // 64x loves to shit on this one

// Try not to use it. We want to move away from it.
// Additionaly, we will add checks in many functions.
GarrysMod::Lua::ILuaInterface* g_Lua = nullptr;

IVEngineServer* engine = nullptr;
CGlobalEntityList* Util::entitylist = nullptr;
CUserMessages* Util::pUserMessages = nullptr;

std::unordered_set<LuaUserData*> g_pLuaUserData;

std::unordered_set<int> Util::g_pReference;
ConVar Util::holylib_debug_mainutil("holylib_debug_mainutil", "1");

// We require this here since we depend on the Lua namespace
void LuaUserData::ForceGlobalRelease(void* pData)
{
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

GCudata* g_pEntityReferences[MAX_EDICTS] = {nullptr}; // nulled out in Util::InitDetours
int g_pEntitySerialNum[MAX_EDICTS] = {-1};

IModuleWrapper* Util::pEntityList;
static Symbols::CBaseEntity_GetLuaEntity func_CBaseEntity_GetLuaEntity = nullptr;
void Util::Push_Entity(GarrysMod::Lua::ILuaInterface* LUA, CBaseEntity* pEnt)
{
	if (!pEnt)
	{
		LUA->GetField(LUA_GLOBALSINDEX, "NULL");
		return;
	}

	if (LUA == g_Lua)
	{
		const CBaseHandle& pHandle = pEnt->GetRefEHandle(); // The only virtual function that would never dare to change
		int nEntryIndex = pHandle.GetEntryIndex();
		if (nEntryIndex >= 0 && MAX_EDICTS > nEntryIndex)
		{
			int nSerial = g_pEntitySerialNum[nEntryIndex];
			int nEntSerial = pHandle.GetSerialNumber();
			lua_State* L = LUA->GetState();
			if (nSerial != nEntSerial)
			{
				g_pEntitySerialNum[nEntryIndex] = nEntSerial;
				g_pEntityReferences[nEntryIndex] = nullptr;


				if (!func_CBaseEntity_GetLuaEntity)
				{
					GarrysMod::Lua::ILuaObject* pObj = LUA->NewTemporaryObject();
					pObj->SetEntity(pEnt);
					pObj->Push();
				} else {
					GarrysMod::Lua::CLuaObject* pObject = (GarrysMod::Lua::CLuaObject*)func_CBaseEntity_GetLuaEntity(pEnt);
					if (!pObject)
					{
						LUA->GetField(LUA_GLOBALSINDEX, "NULL");
						return;
					}

					Util::ReferencePush(LUA, pObject->GetReference()); // Assuming the reference is always right.
				}

				g_pEntityReferences[nEntryIndex] = udataV(L->top-1); // Should be fine since the GCudata is never moved/nuked.
				return;
			}

			GCudata* pData = g_pEntityReferences[nEntryIndex];
			if (pData)
			{
				setudataV(L, L->top++, g_pEntityReferences[nEntryIndex]);
				return;
			}
		}

		// In the case we are missing our symbol for CBaseEntity::GetLuaEntity, this will be slower though will still be fully functional.
		if (!func_CBaseEntity_GetLuaEntity)
		{
			GarrysMod::Lua::ILuaObject* pObj = LUA->NewTemporaryObject();
			pObj->SetEntity(pEnt);
			pObj->Push();
			return;
		}

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
	{
		Warning(PROJECT_NAME ": EHANDLE Index %i - %i\n", pEntHandle->GetEntryIndex(), pEntHandle->GetSerialNumber());
		LUA->ThrowError("Tried to use a NULL Entity! (The weird case?)");
	}
		
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

CBaseEntity* Util::GetCBaseEntityFromEdict(edict_t* edict)
{
	if (!edict)
		return nullptr;

	return Util::servergameents->EdictToBaseEntity(edict);
}

CBaseEntity* Util::GetCBaseEntityFromIndex(int nEntIndex)
{
	if (nEntIndex < 0 || nEntIndex > MAX_EDICTS)
		return nullptr;

	return Util::servergameents->EdictToBaseEntity(Util::engineserver->PEntityOfEntIndex(nEntIndex));
}

CBaseEntity* Util::GetCBaseEntityFromHandle(const CBaseHandle& pHandle)
{
	if (g_pEntityList)
		return (CBaseEntity*)pHandle.Get();

	return Util::GetCBaseEntityFromIndex(pHandle.GetEntryIndex());
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

std::unordered_set<std::string> Util::pBlockedEvents;
static Detouring::Hook detour_CGameEventManager_CreateEvent;
static IGameEvent* hook_CGameEventManager_CreateEvent(void* manager, const char* name, bool bForce)
{
	auto it = Util::pBlockedEvents.find(name);
	if (it != Util::pBlockedEvents.end())
		return NULL;

	return detour_CGameEventManager_CreateEvent.GetTrampoline<Symbols::CGameEventManager_CreateEvent>()(manager, name, bForce);
}

void Util::BlockGameEvent(const char* pName)
{
	auto it = pBlockedEvents.find(pName);
	if (it != pBlockedEvents.end())
			return;

	pBlockedEvents.insert(pName);
}

void Util::UnblockGameEvent(const char* pName)
{
	auto it = pBlockedEvents.find(pName);
	if (it == pBlockedEvents.end())
			return;

	pBlockedEvents.erase(it);
}

/*
	This isn't made for speed, instead this will be called once by code on ServerActivate
	where then the calling code can cache the value.

	Why do we use SendProp?
	Because they are the most reliable and secure way to get offsets to variables even across platforms.
	We normally try to avoid offsets, but these offset are our love.
*/
static std::unordered_map<std::string, std::unordered_set<SendProp*>> g_pSendProps;
extern void AddSendProp(SendProp* pProp, std::unordered_set<SendProp*>& pSendProp);
extern void AddSendTable(SendTable* pTables);
void AddSendProp(SendProp* pProp, std::unordered_set<SendProp*>& pSendProp)
{
	if (pSendProp.find(pProp) == pSendProp.end())
		pSendProp.insert(pProp);

	if (pProp->GetDataTable())
		AddSendTable(pProp->GetDataTable());

	if (pProp->GetArrayProp())
		AddSendProp(pProp->GetArrayProp(), pSendProp);
}

void AddSendTable(SendTable* pTable)
{
	std::unordered_set<SendProp*> pSendProp;
	for (int i = 0; i < pTable->GetNumProps(); i++) {
		SendProp* pProp = &pTable->m_pProps[i]; // Windows screwing with GetProp
		
		AddSendProp(pProp, pSendProp);
	}

	g_pSendProps[pTable->GetName()] = pSendProp;
}

int Util::FindOffsetForNetworkVar(const char* pDTName, const char* pVarName)
{
	if (!Util::servergamedll)
		return -1;

	if (g_pSendProps.size() == 0)
	{
		for(ServerClass *serverclass = Util::servergamedll->GetAllServerClasses(); serverclass->m_pNext != nullptr; serverclass = serverclass->m_pNext)
			AddSendTable(serverclass->m_pTable);
	}

	auto it = g_pSendProps.find(pDTName);
	if (it != g_pSendProps.end())
	{
		for (SendProp* pProp : it->second)
		{
			if (((std::string_view)pVarName) == pProp->GetName())
			{
				if (pProp->GetArrayProp())
					return pProp->GetArrayProp()->GetOffset();

				return pProp->GetOffset();
			}
		}
	}

	return -1;
}

#if SYSTEM_WINDOWS
#undef CreateEvent // Where tf is that windows include >:(
DETOUR_THISCALL_START()
	DETOUR_THISCALL_ADDFUNC1( hook_CSteam3Server_NotifyClientDisconnect, NotifyClientDisconnect, void*, CBaseClient* );
	DETOUR_THISCALL_ADDRETFUNC2( hook_CGameEventManager_CreateEvent, IGameEvent*, CreateEvent, void*, const char*, bool );
DETOUR_THISCALL_FINISH();
#endif

IGet* Util::get = nullptr;
CBaseEntityList* g_pEntityList = nullptr;
Symbols::lua_rawseti Util::func_lua_rawseti = nullptr;
Symbols::lua_rawgeti Util::func_lua_rawgeti = nullptr;
IGameEventManager2* Util::gameeventmanager = nullptr;
IServerGameDLL* Util::servergamedll = nullptr;
Symbols::lj_tab_new Util::func_lj_tab_new = nullptr;
Symbols::lj_gc_barrierf Util::func_lj_gc_barrierf = nullptr;
Symbols::lj_tab_get Util::func_lj_tab_get = nullptr;
Symbols::lua_setfenv Util::func_lua_setfenv = nullptr;
Symbols::lua_touserdata Util::func_lua_touserdata = nullptr;
Symbols::lua_type Util::func_lua_type = nullptr;
Symbols::luaL_checklstring Util::func_luaL_checklstring = nullptr;
Symbols::lua_pcall Util::func_lua_pcall = nullptr;
Symbols::lua_insert Util::func_lua_insert = nullptr;
Symbols::lua_toboolean Util::func_lua_toboolean = nullptr;
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

	DETOUR_PREPARE_THISCALL();
	Detour::Create(
		&detour_CSteam3Server_NotifyClientDisconnect, "CSteam3Server::NotifyClientDisconnect",
		engine_loader.GetModule(), Symbols::CSteam3Server_NotifyClientDisconnectSym,
		(void*)DETOUR_THISCALL(hook_CSteam3Server_NotifyClientDisconnect, NotifyClientDisconnect), 0
	);

	Detour::Create(
		&detour_CGameEventManager_CreateEvent, "CGameEventManager::CreateEvent",
		engine_loader.GetModule(), Symbols::CGameEventManager_CreateEventSym,
		(void*)DETOUR_THISCALL(hook_CGameEventManager_CreateEvent, CreateEvent), 0
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
		#ifdef ARCHITECTURE_X86
			entitylist = Detour::ResolveSymbol<CGlobalEntityList>(server_loader, Symbols::gEntListSym);
		#else
			entitylist = Detour::ResolveSymbolFromLea<CGlobalEntityList>(server_loader.GetModule(), Symbols::gEntListSym);
		#endif
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

	func_lj_tab_new = (Symbols::lj_tab_new)Detour::GetFunction(lua_shared_loader.GetModule(), Symbols::lj_tab_newSym);
	Detour::CheckFunction((void*)func_lj_tab_new, "lj_tab_new");

	func_lua_touserdata = (Symbols::lua_touserdata)Detour::GetFunction(lua_shared_loader.GetModule(), Symbols::lua_touserdataSym);
	Detour::CheckFunction((void*)func_lua_touserdata, "lua_touserdata");

	func_lua_type = (Symbols::lua_type)Detour::GetFunction(lua_shared_loader.GetModule(), Symbols::lua_typeSym);
	Detour::CheckFunction((void*)func_lua_type, "lua_type");

	func_lua_setfenv = (Symbols::lua_type)Detour::GetFunction(lua_shared_loader.GetModule(), Symbols::lua_setfenvSym);
	Detour::CheckFunction((void*)func_lua_setfenv, "lua_setfenv");

	func_luaL_checklstring = (Symbols::luaL_checklstring)Detour::GetFunction(lua_shared_loader.GetModule(), Symbols::luaL_checklstringSym);
	Detour::CheckFunction((void*)func_luaL_checklstring, "luaL_checklstring");

	func_lua_pcall = (Symbols::lua_pcall)Detour::GetFunction(lua_shared_loader.GetModule(), Symbols::lua_pcallSym);
	Detour::CheckFunction((void*)func_lua_pcall, "lua_pcall");

	func_lua_insert = (Symbols::lua_insert)Detour::GetFunction(lua_shared_loader.GetModule(), Symbols::lua_insertSym);
	Detour::CheckFunction((void*)func_lua_insert, "lua_insert");

	func_lua_toboolean = (Symbols::lua_toboolean)Detour::GetFunction(lua_shared_loader.GetModule(), Symbols::lua_tobooleanSym);
	Detour::CheckFunction((void*)func_lua_toboolean, "lua_toboolean");

	func_lj_gc_barrierf = (Symbols::lj_gc_barrierf)Detour::GetFunction(lua_shared_loader.GetModule(), Symbols::lj_gc_barrierfSym);
	Detour::CheckFunction((void*)func_lj_gc_barrierf, "lj_gc_barrierf");

	func_lj_tab_get = (Symbols::lj_tab_get)Detour::GetFunction(lua_shared_loader.GetModule(), Symbols::lj_tab_getSym);
	Detour::CheckFunction((void*)func_lj_tab_get, "lj_tab_get");

	if (!func_lua_touserdata || !func_lua_type || !func_lua_setfenv || !func_luaL_checklstring || !func_lua_pcall || !func_lua_insert || !func_lua_toboolean)
	{
		// This is like the ONLY dependency we have on symbols that without we cannot function.
		Error(PROJECT_NAME " - core: Failed to load an important symbol which we utterly depend on.\n");
	}

	func_CBaseEntity_GetLuaEntity = (Symbols::CBaseEntity_GetLuaEntity)Detour::GetFunction(server_loader.GetModule(), Symbols::CBaseEntity_GetLuaEntitySym);
	Detour::CheckFunction((void*)func_CBaseEntity_GetLuaEntity, "CBaseEntity::GetLuaEntity");

	func_CBaseEntity_CalcAbsolutePosition = (Symbols::CBaseEntity_CalcAbsolutePosition)Detour::GetFunction(server_loader.GetModule(), Symbols::CBaseEntity_CalcAbsolutePositionSym);
	Detour::CheckFunction((void*)func_CBaseEntity_CalcAbsolutePosition, "CBaseEntity::CalcAbsolutePosition");

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

// Checks if the given string contains illegal characters
static inline bool ValidateURLVariable(Bootil::BString value)
{
	for (char c : value)
	{
		if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_' && c != '.')
			return false;
	}

	return true;
}

void Util::CheckVersion(bool bAutoUpdate) // This is called only when holylib is initially loaded!
{
	// ToDo: Implement this someday / finish this implementation
	httplib::Client pClient("http://holylib.raphaelit7.com");

	httplib::Headers headers = {
		{ "HolyLib_Branch", HolyLib_GetBranch() },
		{ "HolyLib_RunNumber", HolyLib_GetRunNumber() },
		{ "HolyLib_Version", HolyLib_GetVersion() },
	};

	auto res = pClient.Get("/api/check_version");
	if (res->status != 200)
		return;

	Bootil::Data::Tree pTree;
	if (!Bootil::Data::Json::Import(pTree, res->body.c_str())) {
		DevMsg(PROJECT_NAME " - versioncheck: Received invalid json!\n");
		return;
	}

	if (pTree.GetChild("status").Value() == "ok")
	{
		Msg(PROJECT_NAME " - versioncheck: We are up to date\n");
	} else if (pTree.GetChild("status").Value() == "custom") {
		Msg(PROJECT_NAME " - versioncheck: We are running a custom version\n");
	} else if (pTree.GetChild("status").Value() == "outdated") {
		Msg(PROJECT_NAME " - versioncheck: We are running an outdated version\n");

		if (!bAutoUpdate)
			return;

		// NOTE: We use GH releases as I do not at all want to use any other service that could be vulnerable.
		// AutoUpdate should be absolutely safe with no external services
		// the wiki may tell you if your outdated or not but it does not specify the download URL!
		Bootil::BString releaseVersion = pTree.GetChild("releaseVersion").Value();
		Bootil::BString releaseFile = pTree.GetChild("releaseFile").Value();
		if (releaseFile.empty() || releaseVersion.empty())
		{
				Msg(PROJECT_NAME " - versioncheck: Auto update failed since we got no valid release response\n");
				return;
		}

		if (!ValidateURLVariable(releaseVersion) || !ValidateURLVariable(releaseFile))
		{
			Msg(PROJECT_NAME " - versioncheck: Auto update was blocked due to malicious response!\n");
			return;
		}
				
		Bootil::BString downloadURL = Bootil::String::Format::Print(
			"https://github.com/RaphaelIT7/gmod-holylib/releases/download/%s/%s",
			releaseVersion.c_str(),
			releaseFile.c_str()
		);
		pClient.set_follow_location(true); // GitHub does redirect one
		auto pReleaseDownload = pClient.Get(downloadURL);

		if (!pReleaseDownload || pReleaseDownload->status != 200)
		{
			Msg(PROJECT_NAME " - versioncheck: Autoupdate failed to download new version! (%i)\n", pReleaseDownload->status);
			return;
		}

		std::string updaterPath = "holylib/autoupdater/";
		std::string filePath = updaterPath;
		filePath.append(releaseVersion);
		filePath.append("-");
		filePath.append(releaseFile);

		g_pFullFileSystem->CreateDirHierarchy(updaterPath.c_str(), "MOD");
		FileHandle_t pHandle = g_pFullFileSystem->Open(filePath.c_str(), "wb", "MOD");
		if (!pHandle)
		{
			Msg(PROJECT_NAME " - versioncheck: Autoupdate failed to open file to write! (%s)\n", filePath.c_str());
			return;
		}

		g_pFullFileSystem->Write(pReleaseDownload->body.c_str(), pReleaseDownload->body.length(), pHandle);
		g_pFullFileSystem->Close(pHandle);

		Bootil::BString outputFileName = "gmsv_holylib_" MODULE_EXTENSION "_updated." LIBRARY_EXTENSION;
			
		Bootil::BString outputRelativeFilePath = updaterPath; // For the filesystem
		outputRelativeFilePath.append(outputFileName);

		char fullFolderPath[MAX_PATH];
		g_pFullFileSystem->RelativePathToFullPath(filePath.c_str(), "MOD", fullFolderPath, sizeof(fullFolderPath));
		Bootil::BString outputFullFilePath = fullFolderPath; // For Bootil
		outputFullFilePath.append(outputFileName);

		Bootil::Compression::Zip::File pDownloadBuild(filePath.c_str());
		for (int i=0; i<pDownloadBuild.GetNumItems(); ++i)
		{
			Bootil::BString pFileName = pDownloadBuild.GetFileName( i );
			if (pFileName.find("gmsv_holylib_" MODULE_EXTENSION) == std::string::npos)
				continue; // not our file

			// Found our version file (this is in case future builds contain multiple platforms)
			pDownloadBuild.ExtractFile(i, outputFullFilePath);
		}

		if (!g_pFullFileSystem->FileExists(outputRelativeFilePath.c_str(), "MOD"))
		{
			Msg(PROJECT_NAME " - versioncheck: Autoupdate failed to write output file! (%s)\n", outputRelativeFilePath.c_str());
			return;
		}

		// Our location will be in the BASE_PATH so it'll end up besides the ghostinj
		Bootil::File::Copy(outputFullFilePath.c_str(), outputFileName.c_str());
		Msg(PROJECT_NAME " - versioncheck: Autoupdate succeeded, newer version will be installed on next boot\n");
	} else if (pTree.GetChild("status").Value() == "toonew") {
		Msg(PROJECT_NAME " - versioncheck: We are running a too new version? Damn...\n");
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

	IConfig* pConVarConfig = g_pConfigSystem->LoadConfig(HOLYLIB_CONFIG_PATH "convars.json");
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

	IConfig* pCoreConfig = g_pConfigSystem->LoadConfig(HOLYLIB_CONFIG_PATH "core.json");
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
		pEntry.GetChild("description").Var<Bootil::BString>("(Unfinished implementation) If enabled, HolyLib will attempt to request the newest version from the wiki and compare them.");
		bool bAllowAutoUpdate = pEntry.EnsureChildVar<bool>("autoUpdate", false);
		if (pEntry.EnsureChildVar<bool>("enabled", false))
		{
			Util::CheckVersion(bAllowAutoUpdate);
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
			Bootil::Data::Tree& pGmodInformation = pData.GetChild("gmod");
			pGmodInformation.EnsureChildVar<Bootil::BString>("version", Util::get->VersionStr());
			pGmodInformation.EnsureChildVar<Bootil::BString>("versionTime", Util::get->VersionTimeStr());
			pGmodInformation.EnsureChildVar<Bootil::BString>("branch", Util::get->SteamBranch());
		}

		// Dump all holylib convars.
		{
			Bootil::Data::Tree& pConVars = pData.GetChild("convars");
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
					Bootil::Data::Tree& pEntry = pConVars.GetChild(pConVar->GetName());
					pEntry.EnsureChildVar<Bootil::BString>("value", pConVar->GetString());
				}
			}
		}

		// Dump detour information
		{
			Bootil::Data::Tree& pDetours = pData.GetChild("detours");

			Bootil::Data::Tree& pLoadedDetours = pDetours.GetChild("loaded");
			for (auto& [pName, _] : Detour::GetLoadedDetours())
				pLoadedDetours.EnsureChildVar<bool>(pName, true);

			Bootil::Data::Tree& pFailedDetours = pDetours.GetChild("failed");
			for (auto& pName : Detour::GetFailedDetours())
				pFailedDetours.EnsureChildVar<bool>(pName, true);

			Bootil::Data::Tree& pDisabledDetours = pDetours.GetChild("disabled");
			for (auto& pName : Detour::GetDisabledDetours())
				pDisabledDetours.EnsureChildVar<bool>(pName, true);
		}

		pDebugDump->Save();
		pDebugDump->Destroy();

		Msg(PROJECT_NAME ": Created dump inside holylib/debug/dump.json!\n");
	} else {
		Msg(PROJECT_NAME ": Failed to create debug dump!\n");
	}
}
static ConCommand createdebugdump("holylib_createdebugdump", CreateDebugDump, "Creates a debug dump that can be provided in a issue or bug report", 0);

static void ShowOffsetOfVar(const CCommand &args)
{
	if (args.ArgC() != 3)
	{
		Msg("holylib_showdtoffset [dtname] [varname]\n");
		return;
	}

	Msg("Offset: %i\n", Util::FindOffsetForNetworkVar(args.Arg(1), args.Arg(2)));
}
static ConCommand showdtoffset("holylib_showdtoffset", ShowOffsetOfVar, "Shows the offset", 0);

GMODGet_LuaClass(IRecipientFilter, GarrysMod::Lua::Type::RecipientFilter, "RecipientFilter", )
GMODGet_LuaClass(Vector, GarrysMod::Lua::Type::Vector, "Vector", )
GMODGet_LuaClass(QAngle, GarrysMod::Lua::Type::Angle, "Angle", )
GMODGet_LuaClass(ConVar, GarrysMod::Lua::Type::ConVar, "ConVar", )

GMODPush_LuaClass(QAngle, GarrysMod::Lua::Type::Angle)
GMODPush_LuaClass(Vector, GarrysMod::Lua::Type::Vector)
