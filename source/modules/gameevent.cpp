#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "sourcesdk/GameEventManager.h"
#include "sourcesdk/sv_client.h"
#include "baseserver.h"
#include "vprof.h"
#include "sourcesdk/netmessages.h"
#include <unordered_set>
#include "eiface.h"

class CUserCmd; // Fixes an error in igamesystem.h
#include <igamesystem.h>

class CGameeventLibModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "gameevent"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; };
};

static ConVar gameevent_callhook("holylib_gameevent_callhook", "1", 0, "If enabled, the HolyLib:Pre/PostListenGameEvent hooks get called");

static CGameeventLibModule g_pGameeventLibModule;
IModule* pGameeventLibModule = &g_pGameeventLibModule;

static CGameEventManager* pManager;
LUA_FUNCTION_STATIC(gameevent_GetListeners)
{
	if (LUA->IsType(1, GarrysMod::Lua::Type::String))
	{
		CGameEventDescriptor* desciptor = pManager->GetEventDescriptor(LUA->GetString(1));
		if (!desciptor)
			return 0; // Return nothing -> nil on failure

		LUA->PushNumber(desciptor->listeners.Count());
	} else {
		LUA->CreateTable();
			FOR_EACH_VEC(pManager->m_GameEvents, i)
			{
				CGameEventDescriptor& descriptor = pManager->m_GameEvents[i];
				LUA->PushString((const char*)&descriptor.name);
				LUA->PushNumber(descriptor.listeners.Count());
				LUA->RawSet(-3); // Does it even need to be a const char* ?
			}
	}

	return 1;
}

static IGameEventListener2* pLuaGameEventListener;
LUA_FUNCTION_STATIC(gameevent_RemoveListener)
{
	const char* strEvent = LUA->CheckString(1);

	bool bSuccess = false;
	if (pLuaGameEventListener)
	{
		CGameEventDescriptor* desciptor = pManager->GetEventDescriptor(strEvent);
		if (!desciptor)
		{
			LUA->PushBool(false);
			return 1;
		}

		FOR_EACH_VEC(desciptor->listeners, i)
		{
			CGameEventCallback* callback = desciptor->listeners[i];
			if (callback->m_nListenerType != CGameEventManager::SERVERSIDE)
				continue;

			IGameEventListener2* listener = (IGameEventListener2*)callback->m_pCallback;
			if (g_pGameeventLibModule.InDebug())
				Msg("holylib: Pointer 1: %p\nPointer 2: %p\n", listener, pLuaGameEventListener);

			if (listener == pLuaGameEventListener)
			{
				desciptor->listeners.Remove(i); // ToDo: Verify that this doesn't cause a memory leak because CGameEventCallback isn't deleted.
				bSuccess = true;
				break;
			}
		}
	} else {
		Warning("holylib: Failed to find LuaGameEventListener in GameSystems?\n");
	}

	LUA->PushBool(bSuccess);

	return 1;
}

LUA_FUNCTION_STATIC(gameevent_GetClientListeners)
{
	if (LUA->IsType(1, GarrysMod::Lua::Type::Entity))
	{
		CBasePlayer* pEntity = Util::Get_Player(1, false);
		if (!pEntity)
			LUA->ThrowError("Tried to use a NULL Player!\n");

		CBaseClient* pClient = Util::GetClientByPlayer(pEntity);

		LUA->CreateTable();
		int idx = 0;
		FOR_EACH_VEC(pManager->m_GameEvents, i)
		{
			CGameEventDescriptor& descriptor = pManager->m_GameEvents[i];
			FOR_EACH_VEC(descriptor.listeners, j)
			{
				CGameEventCallback* callback = descriptor.listeners[j];
				if (callback->m_nListenerType != CGameEventManager::CLIENTSTUB)
					continue;

				CBaseClient* listener = (CBaseClient*)callback->m_pCallback;
				if (g_pGameeventLibModule.InDebug())
					Msg("Pointer 1: %p\nPointer 2: %p\n", listener, pClient);

				if (listener == pClient)
				{
					LUA->PushString(descriptor.name, 32);
					Util::RawSetI(LUA, -2, ++idx);
					break;
				}
			}
		}
	} else {
		LUA->CreateTable();
		for (int iClient = 0; iClient<Util::server->GetMaxClients(); ++iClient)
		{
			CBaseEntity* ent = Util::GetCBaseEntityFromEdict(Util::engineserver->PEntityOfEntIndex(iClient+1));
			if (!ent)
				continue;

			Util::Push_Entity(ent);
			LUA->CreateTable();

			CBaseClient* pClient = Util::GetClientByIndex(iClient);
			int idx = 0;
			FOR_EACH_VEC(pManager->m_GameEvents, i)
			{
				CGameEventDescriptor& descriptor = pManager->m_GameEvents[i];

				FOR_EACH_VEC(descriptor.listeners, j)
				{
					CGameEventCallback* callback = descriptor.listeners[j];
					if (callback->m_nListenerType != CGameEventManager::CLIENTSTUB)
						continue;

					CBaseClient* listener = (CBaseClient*)callback->m_pCallback;
					if (listener == pClient)
					{
						LUA->PushString(descriptor.name);
						Util::RawSetI(LUA, -2, ++idx);
						break;
					}
				}
			}

			LUA->RawSet(-3);
		}
	}

	return 1;
}

LUA_FUNCTION_STATIC(gameevent_RemoveClientListener)
{
	CBasePlayer* pEntity = Util::Get_Player(1, false);
	if (!pEntity)
		LUA->ThrowError("Tried to use a NULL Player!\n");

	CBaseClient* pClient = Util::GetClientByPlayer(pEntity);
	const char* strEvent = LUA->CheckStringOpt(2, NULL);

	bool bSuccess = false;
	if (strEvent)
	{
		CGameEventDescriptor* desciptor = pManager->GetEventDescriptor(strEvent);
		if (!desciptor)
		{
			LUA->PushBool(false);
			return 1;
		}

		FOR_EACH_VEC(desciptor->listeners, i)
		{
			CGameEventCallback* callback = desciptor->listeners[i];
			if (callback->m_nListenerType != CGameEventManager::CLIENTSTUB)
				continue;

			CBaseClient* listener = (CBaseClient*)callback->m_pCallback;
			if (listener == pClient)
			{
				desciptor->listeners.Remove(i); // ToDo: Verify that this doesn't cause a memory leak because CGameEventCallback isn't deleted.
				bSuccess = true;
				break;
			}
		}
	} else {
		pManager->RemoveListener(pClient);
		bSuccess = true; // Always true?
	}

	LUA->PushBool(bSuccess);

	return 1;
}

Symbols::CGameEventManager_AddListener func_CGameEventManager_AddListener;
LUA_FUNCTION_STATIC(gameevent_AddClientListener)
{
	if (!g_pGameeventLibModule.InDebug())
	{
		LUA->ThrowError("This function is currently disabled.");
		return 0;
	}

	CBasePlayer* pEntity = Util::Get_Player(1, true);
	if (!pEntity)
		LUA->ThrowError("Tried to use a NULL Player!\n");

	const char* strEvent = LUA->CheckString(2);

	if (!func_CGameEventManager_AddListener)
		LUA->ThrowError("Failed to get CGameEventManager::AddListener");

	CGameEventDescriptor* desciptor = pManager->GetEventDescriptor(strEvent);
	if (!desciptor)
	{
		LUA->PushBool(false);
		return 1;
	}

	func_CGameEventManager_AddListener(pManager, Util::GetClientByPlayer(pEntity), desciptor, CGameEventManager::CLIENTSTUB);

	LUA->PushBool(true);
	return 1;
}

static Detouring::Hook detour_CBaseClient_ProcessListenEvents;
bool hook_CBaseClient_ProcessListenEvents(CBaseClient* client, CLC_ListenEvents* msg)
{
	VPROF_BUDGET("HolyLib - ProcessGameEventList", VPROF_BUDGETGROUP_OTHER_NETWORKING);

	if (!gameevent_callhook.GetBool())
		return detour_CBaseClient_ProcessListenEvents.GetTrampoline<Symbols::CBaseClient_ProcessListenEvents>()(client, msg);

	int idx = 0;
	CBasePlayer* pPlayer = Util::GetPlayerByClient(client);
	if (g_pGameeventLibModule.InDebug())
	{
		Msg("Player: %p\n", pPlayer);
		Msg("Client: %p\n", client);
		Msg("Index: %i\n", ((IClient*)client)->GetPlayerSlot() + 1);
		Msg("Index2: %i\n", client->GetPlayerSlot());
		Msg("Index3: %i\n", client->m_nClientSlot);
		Msg("Edict: %i\n", Util::engineserver->PEntityOfEntIndex(client->GetPlayerSlot() + 1)->m_EdictIndex);
		Msg("Edict Class: %s\n", Util::engineserver->PEntityOfEntIndex(client->GetPlayerSlot() + 1)->GetClassName());
		Msg("Edict Ent: %p\n", Util::servergameents->EdictToBaseEntity(Util::engineserver->PEntityOfEntIndex(client->GetPlayerSlot() + 1)));
	}

	g_Lua->CreateTable();
		for (int i=0; i < MAX_EVENT_NUMBER; i++)
		{
			if (msg->m_EventArray.Get(i))
			{
				CGameEventDescriptor *descriptor = pManager->GetEventDescriptor(i);

				if (descriptor)
				{
					g_Lua->PushString(descriptor->name);
					Util::RawSetI(g_Lua, -2, ++idx);
				}
			}
		}

	if (g_pGameeventLibModule.InDebug())
		Msg("Player: %p\nIndex: %i\n", pPlayer, client->GetPlayerSlot());

	int iReference = g_Lua->ReferenceCreate();
	if (Lua::PushHook("HolyLib:PreProcessGameEvent"))
	{
		Util::Push_Entity((CBaseEntity*)pPlayer);
		Util::ReferencePush(g_Lua, iReference);
		g_Lua->PushNumber(client->GetPlayerSlot() + 1);
		if (g_Lua->CallFunctionProtected(4, 1, false))
		{
			bool pCancel = g_Lua->GetBool(-1);
			g_Lua->Pop(1);
			if (pCancel)
			{
				g_Lua->ReferenceFree(iReference);
				return true;
			}
		}
	}

	bool bRet = detour_CBaseClient_ProcessListenEvents.GetTrampoline<Symbols::CBaseClient_ProcessListenEvents>()(client, msg);

	if (Lua::PushHook("HolyLib:PostProcessGameEvent"))
	{
		Util::Push_Entity((CBaseEntity*)pPlayer);
		Util::ReferencePush(g_Lua, iReference);
		g_Lua->PushNumber(client->GetPlayerSlot() + 1);
		g_Lua->CallFunctionProtected(4, 0, false);
	}

	g_Lua->ReferenceFree(iReference);

	return bRet;
}

static int IGameEvent_TypeID = -1;
PushReferenced_LuaClass(IGameEvent, IGameEvent_TypeID)
Get_LuaClass(IGameEvent, IGameEvent_TypeID, "IGameEvent")

LUA_FUNCTION_STATIC(IGameEvent__tostring)
{
	IGameEvent* pEvent = Get_IGameEvent(1, false);
	if (!pEvent)
	{
		LUA->PushString("IGameEvent [NULL]");
		return 1;
	}

	char szBuf[64] = {};
	V_snprintf(szBuf, sizeof(szBuf), "IGameEvent [%s]", pEvent->GetName());
	LUA->PushString(szBuf);
	return 1;
}

Default__index(IGameEvent);
Default__newindex(IGameEvent);
Default__GetTable(IGameEvent);

LUA_FUNCTION_STATIC(IGameEvent__gc)
{
	IGameEvent* pEvent = Get_IGameEvent(1, false);
	if (pEvent)
	{
		Delete_IGameEvent(pEvent);
		pManager->FreeEvent(pEvent);
	}

	return 0;
}

LUA_FUNCTION_STATIC(IGameEvent_IsValid)
{
	IGameEvent* pEvent = Get_IGameEvent(1, false);

	LUA->PushBool(pEvent != nullptr);
	return 1;
}

LUA_FUNCTION_STATIC(IGameEvent_IsEmpty)
{
	IGameEvent* pEvent = Get_IGameEvent(1, true);

	LUA->PushBool(pEvent->IsEmpty());
	return 1;
}

LUA_FUNCTION_STATIC(IGameEvent_IsReliable)
{
	IGameEvent* pEvent = Get_IGameEvent(1, true);

	LUA->PushBool(pEvent->IsReliable());
	return 1;
}

LUA_FUNCTION_STATIC(IGameEvent_IsLocal)
{
	IGameEvent* pEvent = Get_IGameEvent(1, true);

	LUA->PushBool(pEvent->IsLocal());
	return 1;
}

LUA_FUNCTION_STATIC(IGameEvent_GetName)
{
	IGameEvent* pEvent = Get_IGameEvent(1, true);
	
	LUA->PushString(pEvent->GetName());
	return 1;
}


LUA_FUNCTION_STATIC(IGameEvent_GetBool)
{
	IGameEvent* pEvent = Get_IGameEvent(1, true);
	const char* pName = LUA->CheckString(2);
	bool pFallback = LUA->GetBool(3);

	LUA->PushBool(pEvent->GetBool(pName, pFallback));
	return 1;
}

LUA_FUNCTION_STATIC(IGameEvent_GetFloat)
{
	IGameEvent* pEvent = Get_IGameEvent(1, true);
	const char* pName = LUA->CheckString(2);
	float pFallback = (float)LUA->GetNumber(3);

	LUA->PushNumber(pEvent->GetFloat(pName, pFallback));
	return 1;
}

LUA_FUNCTION_STATIC(IGameEvent_GetInt)
{
	IGameEvent* pEvent = Get_IGameEvent(1, true);
	const char* pName = LUA->CheckString(2);
	int pFallback = (int)LUA->GetNumber(3);

	LUA->PushNumber(pEvent->GetInt(pName, pFallback));
	return 1;
}

LUA_FUNCTION_STATIC(IGameEvent_GetString)
{
	IGameEvent* pEvent = Get_IGameEvent(1, true);
	const char* pName = LUA->CheckString(2);
	const char* pFallback = LUA->GetString(3);
	if (!pFallback)
		pFallback = "";

	LUA->PushString(pEvent->GetString(pName, pFallback));
	return 1;
}

LUA_FUNCTION_STATIC(IGameEvent_SetBool)
{
	IGameEvent* pEvent = Get_IGameEvent(1, true);
	const char* pName = LUA->CheckString(2);
	bool pValue = LUA->GetBool(3);

	pEvent->SetBool(pName, pValue);
	return 0;
}

LUA_FUNCTION_STATIC(IGameEvent_SetFloat)
{
	IGameEvent* pEvent = Get_IGameEvent(1, true);
	const char* pName = LUA->CheckString(2);
	float pValue = (float)LUA->CheckNumber(3);

	pEvent->SetFloat(pName, pValue);
	return 0;
}

LUA_FUNCTION_STATIC(IGameEvent_SetInt)
{
	IGameEvent* pEvent = Get_IGameEvent(1, true);
	const char* pName = LUA->CheckString(2);
	int pValue = (int)LUA->CheckNumber(3);

	pEvent->SetInt(pName, pValue);
	return 0;
}

LUA_FUNCTION_STATIC(IGameEvent_SetString)
{
	IGameEvent* pEvent = Get_IGameEvent(1, true);
	const char* pName = LUA->CheckString(2);
	const char* pValue = LUA->CheckString(3);

	pEvent->SetString(pName, pValue);
	return 0;
}

LUA_FUNCTION_STATIC(gameevent_Create)
{
	const char* pName = LUA->CheckString(1); // Let's hope that gc won't break something
	bool bForce = LUA->GetBool(2);

	IGameEvent* pEvent = pManager->CreateEvent(pName, bForce);
	Push_IGameEvent(pEvent);
	return 1;
}

LUA_FUNCTION_STATIC(gameevent_DuplicateEvent)
{
	IGameEvent* pEvent = Get_IGameEvent(1, true);

	IGameEvent* pNewEvent = pManager->DuplicateEvent(pEvent);
	Push_IGameEvent(pNewEvent);
	return 1;
}

LUA_FUNCTION_STATIC(gameevent_FireEvent)
{
	IGameEvent* pEvent = Get_IGameEvent(1, true);
	bool bDontBroadcast = LUA->GetBool(2);

	LUA->PushBool(pManager->FireEvent(pEvent, bDontBroadcast));
	return 1;
}

LUA_FUNCTION_STATIC(gameevent_FireClientEvent)
{
	IGameEvent* pEvent = Get_IGameEvent(1, true);
	CBasePlayer* pPlayer = Util::Get_Player(2, true);
	if (!pPlayer)
		LUA->ArgError(2, "Tried to use a NULL player!");

	CBaseClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient from player!");

	pClient->FireGameEvent(pEvent);
	return 0;
}

static std::unordered_set<std::string> pBlockedEvents;
static Detouring::Hook detour_CGameEventManager_CreateEvent;
static IGameEvent* hook_CGameEventManager_CreateEvent(void* manager, const char* name, bool bForce)
{
	auto it = pBlockedEvents.find(name);
	if (it != pBlockedEvents.end())
		return NULL;

	return detour_CGameEventManager_CreateEvent.GetTrampoline<Symbols::CGameEventManager_CreateEvent>()(manager, name, bForce);
}

// Exposed for other parts of HolyLib to use.
void BlockGameEvent(const char* pName)
{
	auto it = pBlockedEvents.find(pName);
	if (it != pBlockedEvents.end())
			return;

	pBlockedEvents.insert(pName);
}

void UnblockGameEvent(const char* pName)
{
	auto it = pBlockedEvents.find(pName);
	if (it == pBlockedEvents.end())
			return;

	pBlockedEvents.erase(it);
}

LUA_FUNCTION_STATIC(gameevent_BlockCreation)
{
	const char* pName = LUA->CheckString(1);
	bool bBlock = LUA->GetBool(2);

	auto it = pBlockedEvents.find(pName);
	if (bBlock)
	{
		if (it != pBlockedEvents.end())
			return 0;

		pBlockedEvents.insert(pName);
	} else {
		if (it == pBlockedEvents.end())
			return 0;

		pBlockedEvents.erase(it);
	}

	return 0;
}

void CGameeventLibModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	pManager = (CGameEventManager*)appfn[0](INTERFACEVERSION_GAMEEVENTSMANAGER2, NULL);
	Detour::CheckValue("get interface", "CGameEventManager", pManager != NULL);
}

void CGameeventLibModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	IGameEvent_TypeID = g_Lua->CreateMetaTable("IGameEvent");
		Util::AddFunc(IGameEvent__tostring, "__tostring");
		Util::AddFunc(IGameEvent__index, "__index");
		Util::AddFunc(IGameEvent__newindex, "__newindex");
		Util::AddFunc(IGameEvent__gc, "__gc");
		Util::AddFunc(IGameEvent_IsValid, "IsValid");
		Util::AddFunc(IGameEvent_GetTable, "GetTable");
		Util::AddFunc(IGameEvent_IsEmpty, "IsEmpty");
		Util::AddFunc(IGameEvent_IsReliable, "IsReliable");
		Util::AddFunc(IGameEvent_IsLocal, "IsLocal");
		Util::AddFunc(IGameEvent_GetName, "GetName");
		Util::AddFunc(IGameEvent_GetBool, "GetBool");
		Util::AddFunc(IGameEvent_GetInt, "GetInt");
		Util::AddFunc(IGameEvent_GetFloat, "GetFloat");
		Util::AddFunc(IGameEvent_GetString, "GetString");
		Util::AddFunc(IGameEvent_SetBool, "SetBool");
		Util::AddFunc(IGameEvent_SetInt, "SetInt");
		Util::AddFunc(IGameEvent_SetFloat, "SetFloat");
		Util::AddFunc(IGameEvent_SetString, "SetString");
	g_Lua->Pop(1);

	if (Util::PushTable("gameevent"))
	{
		Util::AddFunc(gameevent_GetListeners, "GetListeners");
		Util::AddFunc(gameevent_RemoveListener, "RemoveListener");

		Util::AddFunc(gameevent_GetClientListeners, "GetClientListeners");
		Util::AddFunc(gameevent_RemoveClientListener, "RemoveClientListener");
		Util::AddFunc(gameevent_AddClientListener, "AddClientListener");

		Util::AddFunc(gameevent_Create, "Create");
		Util::AddFunc(gameevent_FireEvent, "FireEvent");
		Util::AddFunc(gameevent_FireClientEvent, "FireClientEvent");
		Util::AddFunc(gameevent_DuplicateEvent, "DuplicateEvent");
		Util::AddFunc(gameevent_BlockCreation, "BlockCreation");

		g_Lua->GetField(-1, "Listen");
		g_Lua->PushString("vote_cast"); // Yes this is a valid gameevent.
		g_Lua->CallFunctionProtected(1, 0, true);
		CGameEventDescriptor* descriptor = pManager->GetEventDescriptor("vote_cast");
		FOR_EACH_VEC(descriptor->listeners, i)
		{
			pLuaGameEventListener = (IGameEventListener2*)descriptor->listeners[i]->m_pCallback;
			descriptor->listeners.Remove(i); // We also remove the listener again
			break;
		}
		if (!pLuaGameEventListener)
			Warning("holylib: Failed to find pLuaGameEventListener!\n");
	}
	Util::PopTable();
}

void CGameeventLibModule::LuaShutdown()
{
	if (Util::PushTable("gameevent"))
	{
		Util::RemoveField("GetListeners");
		Util::RemoveField("RemoveListener");
		Util::RemoveField("GetClientListeners");
		Util::RemoveField("RemoveClientListener");
		Util::RemoveField("AddClientListener");
	}
	Util::PopTable();
}

void CGameeventLibModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::ModuleLoader engine_loader("engine");
	Detour::Create(
		&detour_CBaseClient_ProcessListenEvents, "CBaseClient::ProcessListenEvents",
		engine_loader.GetModule(), Symbols::CBaseClient_ProcessListenEventsSym,
		(void*)hook_CBaseClient_ProcessListenEvents, m_pID
	);

	Detour::Create(
		&detour_CGameEventManager_CreateEvent, "CGameEventManager::CreateEvent",
		engine_loader.GetModule(), Symbols::CGameEventManager_CreateEventSym,
		(void*)hook_CGameEventManager_CreateEvent, m_pID
	);

#if ARCHITECTURE_IS_X86
	func_CGameEventManager_AddListener = (Symbols::CGameEventManager_AddListener)Detour::GetFunction(engine_loader.GetModule(), Symbols::CGameEventManager_AddListenerSym);
	Detour::CheckFunction((void*)func_CGameEventManager_AddListener, "CGameEventManager::AddListener");
#endif
}