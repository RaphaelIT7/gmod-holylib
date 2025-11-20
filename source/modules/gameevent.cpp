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

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CGameeventLibModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "gameevent"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; };
	virtual bool SupportsMultipleLuaStates() { return true; };
};

static ConVar gameevent_callhook("holylib_gameevent_callhook", "1", FCVAR_ARCHIVE, "If enabled, the HolyLib:Pre/PostListenGameEvent hooks get called");

static CGameeventLibModule g_pGameeventLibModule;
IModule* pGameeventLibModule = &g_pGameeventLibModule;

static CGameEventManager* pGameEventManager = nullptr;
LUA_FUNCTION_STATIC(gameevent_GetListeners)
{
	if (LUA->IsType(1, GarrysMod::Lua::Type::String))
	{
		CGameEventDescriptor* desciptor = pGameEventManager->GetEventDescriptor(LUA->GetString(1));
		if (!desciptor)
			return 0; // Return nothing -> nil on failure

		LUA->PushNumber(desciptor->listeners.Count());
	} else {
		LUA->CreateTable();
			FOR_EACH_VEC(pGameEventManager->m_GameEvents, i)
			{
				CGameEventDescriptor& descriptor = pGameEventManager->m_GameEvents[i];
				LUA->PushString((const char*)&descriptor.name);
				LUA->PushNumber(descriptor.listeners.Count());
				LUA->RawSet(-3); // Does it even need to be a const char* ?
			}
	}

	return 1;
}

static IGameEventListener2* pLuaGameEventListener = nullptr;
LUA_FUNCTION_STATIC(gameevent_RemoveListener)
{
	const char* strEvent = LUA->CheckString(1);

	bool bSuccess = false;
	if (pLuaGameEventListener)
	{
		CGameEventDescriptor* desciptor = pGameEventManager->GetEventDescriptor(strEvent);
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
				Msg(PROJECT_NAME ": Pointer 1: %p\nPointer 2: %p\n", listener, pLuaGameEventListener);

			if (listener == pLuaGameEventListener)
			{
				desciptor->listeners.Remove(i); // ToDo: Verify that this doesn't cause a memory leak because CGameEventCallback isn't deleted.
				bSuccess = true;
				break;
			}
		}
	} else {
		Warning(PROJECT_NAME ": Failed to find LuaGameEventListener in GameSystems?\n");
	}

	LUA->PushBool(bSuccess);

	return 1;
}

LUA_FUNCTION_STATIC(gameevent_GetClientListeners)
{
	if (LUA->IsType(1, GarrysMod::Lua::Type::Entity))
	{
		CBasePlayer* pEntity = Util::Get_Player(LUA, 1, false);
		if (!pEntity)
			LUA->ThrowError("Tried to use a NULL Player!\n");

		CBaseClient* pClient = Util::GetClientByPlayer(pEntity);

		LUA->CreateTable();
		int idx = 0;
		FOR_EACH_VEC(pGameEventManager->m_GameEvents, i)
		{
			CGameEventDescriptor& descriptor = pGameEventManager->m_GameEvents[i];
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

			Util::Push_Entity(LUA, ent);
			LUA->CreateTable();

			CBaseClient* pClient = Util::GetClientByIndex(iClient);
			int idx = 0;
			FOR_EACH_VEC(pGameEventManager->m_GameEvents, i)
			{
				CGameEventDescriptor& descriptor = pGameEventManager->m_GameEvents[i];

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
	CBasePlayer* pEntity = Util::Get_Player(LUA, 1, false);
	if (!pEntity)
		LUA->ThrowError("Tried to use a NULL Player!\n");

	CBaseClient* pClient = Util::GetClientByPlayer(pEntity);
	const char* strEvent = LUA->CheckStringOpt(2, NULL);

	bool bSuccess = false;
	if (strEvent)
	{
		CGameEventDescriptor* desciptor = pGameEventManager->GetEventDescriptor(strEvent);
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
		pGameEventManager->RemoveListener(pClient);
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

	CBasePlayer* pEntity = Util::Get_Player(LUA, 1, true);
	if (!pEntity)
		LUA->ThrowError("Tried to use a NULL Player!\n");

	const char* strEvent = LUA->CheckString(2);

	if (!func_CGameEventManager_AddListener)
		LUA->ThrowError("Failed to get CGameEventManager::AddListener");

	CGameEventDescriptor* desciptor = pGameEventManager->GetEventDescriptor(strEvent);
	if (!desciptor)
	{
		LUA->PushBool(false);
		return 1;
	}

	func_CGameEventManager_AddListener(pGameEventManager, Util::GetClientByPlayer(pEntity), desciptor, CGameEventManager::CLIENTSTUB);

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
				CGameEventDescriptor *descriptor = pGameEventManager->GetEventDescriptor(i);

				if (descriptor)
				{
					g_Lua->PushString(descriptor->name);
					Util::RawSetI(g_Lua, -2, ++idx);
				}
			}
		}

	if (g_pGameeventLibModule.InDebug())
		Msg("Player: %p\nIndex: %i\n", pPlayer, client->GetPlayerSlot());

	int iReference = Util::ReferenceCreate(g_Lua, "CBaseClient::ProcessListenEvents");
	if (Lua::PushHook("HolyLib:PreProcessGameEvent"))
	{
		Util::Push_Entity(g_Lua, (CBaseEntity*)pPlayer);
		Util::ReferencePush(g_Lua, iReference);
		g_Lua->PushNumber(client->GetPlayerSlot() + 1);
		if (g_Lua->CallFunctionProtected(4, 1, false))
		{
			bool pCancel = g_Lua->GetBool(-1);
			g_Lua->Pop(1);
			if (pCancel)
			{
				Util::ReferenceFree(g_Lua, iReference, "CBaseClient::ProcessListenEvents - Cancel");
				return true;
			}
		}
	}

	bool bRet = detour_CBaseClient_ProcessListenEvents.GetTrampoline<Symbols::CBaseClient_ProcessListenEvents>()(client, msg);

	if (Lua::PushHook("HolyLib:PostProcessGameEvent"))
	{
		Util::Push_Entity(g_Lua, (CBaseEntity*)pPlayer);
		Util::ReferencePush(g_Lua, iReference);
		g_Lua->PushNumber(client->GetPlayerSlot() + 1);
		g_Lua->CallFunctionProtected(4, 0, false);
	}

	Util::ReferenceFree(g_Lua, iReference, "CBaseClient::ProcessListenEvents - Done");

	return bRet;
}

Push_LuaClass(IGameEvent)
Get_LuaClass(IGameEvent, "IGameEvent")

LUA_FUNCTION_STATIC(IGameEvent__tostring)
{
	IGameEvent* pEvent = Get_IGameEvent(LUA, 1, false);
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
Default__gc(IGameEvent, 
	pGameEventManager->FreeEvent((IGameEvent*)pStoredData);
)

LUA_FUNCTION_STATIC(IGameEvent_IsValid)
{
	IGameEvent* pEvent = Get_IGameEvent(LUA, 1, false);

	LUA->PushBool(pEvent != nullptr);
	return 1;
}

LUA_FUNCTION_STATIC(IGameEvent_IsEmpty)
{
	IGameEvent* pEvent = Get_IGameEvent(LUA, 1, true);

	LUA->PushBool(pEvent->IsEmpty());
	return 1;
}

LUA_FUNCTION_STATIC(IGameEvent_IsReliable)
{
	IGameEvent* pEvent = Get_IGameEvent(LUA, 1, true);

	LUA->PushBool(pEvent->IsReliable());
	return 1;
}

LUA_FUNCTION_STATIC(IGameEvent_IsLocal)
{
	IGameEvent* pEvent = Get_IGameEvent(LUA, 1, true);

	LUA->PushBool(pEvent->IsLocal());
	return 1;
}

LUA_FUNCTION_STATIC(IGameEvent_GetName)
{
	IGameEvent* pEvent = Get_IGameEvent(LUA, 1, true);
	
	LUA->PushString(pEvent->GetName());
	return 1;
}


LUA_FUNCTION_STATIC(IGameEvent_GetBool)
{
	IGameEvent* pEvent = Get_IGameEvent(LUA, 1, true);
	const char* pName = LUA->CheckString(2);
	bool pFallback = LUA->GetBool(3);

	LUA->PushBool(pEvent->GetBool(pName, pFallback));
	return 1;
}

LUA_FUNCTION_STATIC(IGameEvent_GetFloat)
{
	IGameEvent* pEvent = Get_IGameEvent(LUA, 1, true);
	const char* pName = LUA->CheckString(2);
	float pFallback = (float)LUA->GetNumber(3);

	LUA->PushNumber(pEvent->GetFloat(pName, pFallback));
	return 1;
}

LUA_FUNCTION_STATIC(IGameEvent_GetInt)
{
	IGameEvent* pEvent = Get_IGameEvent(LUA, 1, true);
	const char* pName = LUA->CheckString(2);
	int pFallback = (int)LUA->GetNumber(3);

	LUA->PushNumber(pEvent->GetInt(pName, pFallback));
	return 1;
}

LUA_FUNCTION_STATIC(IGameEvent_GetString)
{
	IGameEvent* pEvent = Get_IGameEvent(LUA, 1, true);
	const char* pName = LUA->CheckString(2);
	const char* pFallback = LUA->GetString(3);
	if (!pFallback)
		pFallback = "";

	LUA->PushString(pEvent->GetString(pName, pFallback));
	return 1;
}

LUA_FUNCTION_STATIC(IGameEvent_SetBool)
{
	IGameEvent* pEvent = Get_IGameEvent(LUA, 1, true);
	const char* pName = LUA->CheckString(2);
	bool pValue = LUA->GetBool(3);

	pEvent->SetBool(pName, pValue);
	return 0;
}

LUA_FUNCTION_STATIC(IGameEvent_SetFloat)
{
	IGameEvent* pEvent = Get_IGameEvent(LUA, 1, true);
	const char* pName = LUA->CheckString(2);
	float pValue = (float)LUA->CheckNumber(3);

	pEvent->SetFloat(pName, pValue);
	return 0;
}

LUA_FUNCTION_STATIC(IGameEvent_SetInt)
{
	IGameEvent* pEvent = Get_IGameEvent(LUA, 1, true);
	const char* pName = LUA->CheckString(2);
	int pValue = (int)LUA->CheckNumber(3);

	pEvent->SetInt(pName, pValue);
	return 0;
}

LUA_FUNCTION_STATIC(IGameEvent_SetString)
{
	IGameEvent* pEvent = Get_IGameEvent(LUA, 1, true);
	const char* pName = LUA->CheckString(2);
	const char* pValue = LUA->CheckString(3);

	pEvent->SetString(pName, pValue);
	return 0;
}

LUA_FUNCTION_STATIC(gameevent_Create)
{
	const char* pName = LUA->CheckString(1); // Let's hope that gc won't break something
	bool bForce = LUA->GetBool(2);

	IGameEvent* pEvent = pGameEventManager->CreateEvent(pName, bForce);
	Push_IGameEvent(LUA, pEvent);
	return 1;
}

LUA_FUNCTION_STATIC(gameevent_DuplicateEvent)
{
	IGameEvent* pEvent = Get_IGameEvent(LUA, 1, true);

	IGameEvent* pNewEvent = pGameEventManager->DuplicateEvent(pEvent);
	Push_IGameEvent(LUA, pNewEvent);
	return 1;
}

LUA_FUNCTION_STATIC(gameevent_FireEvent)
{
	IGameEvent* pEvent = Get_IGameEvent(LUA, 1, true);
	bool bDontBroadcast = LUA->GetBool(2);

	LUA->PushBool(pGameEventManager->FireEvent(pEvent, bDontBroadcast));
	return 1;
}

LUA_FUNCTION_STATIC(gameevent_FireClientEvent)
{
	IGameEvent* pEvent = Get_IGameEvent(LUA, 1, true);
	CBasePlayer* pPlayer = Util::Get_Player(LUA, 2, true);
	if (!pPlayer)
		LUA->ArgError(2, "Tried to use a NULL player!");

	CBaseClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient from player!");

	pClient->FireGameEvent(pEvent);
	return 0;
}

LUA_FUNCTION_STATIC(gameevent_BlockCreation)
{
	const char* pName = LUA->CheckString(1);
	bool bBlock = LUA->GetBool(2);

	auto it = Util::pBlockedEvents.find(pName);
	if (bBlock)
	{
		if (it != Util::pBlockedEvents.end())
			return 0;

		Util::pBlockedEvents.insert(pName);
	} else {
		if (it == Util::pBlockedEvents.end())
			return 0;

		Util::pBlockedEvents.erase(it);
	}

	return 0;
}

void CGameeventLibModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	pGameEventManager = (CGameEventManager*)Util::gameeventmanager;

	Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::IGameEvent, pLua->CreateMetaTable("IGameEvent"));
		Util::AddFunc(pLua, IGameEvent__tostring, "__tostring");
		Util::AddFunc(pLua, IGameEvent__index, "__index");
		Util::AddFunc(pLua, IGameEvent__newindex, "__newindex");
		Util::AddFunc(pLua, IGameEvent__gc, "__gc");
		Util::AddFunc(pLua, IGameEvent_IsValid, "IsValid");
		Util::AddFunc(pLua, IGameEvent_GetTable, "GetTable");
		Util::AddFunc(pLua, IGameEvent_IsEmpty, "IsEmpty");
		Util::AddFunc(pLua, IGameEvent_IsReliable, "IsReliable");
		Util::AddFunc(pLua, IGameEvent_IsLocal, "IsLocal");
		Util::AddFunc(pLua, IGameEvent_GetName, "GetName");
		Util::AddFunc(pLua, IGameEvent_GetBool, "GetBool");
		Util::AddFunc(pLua, IGameEvent_GetInt, "GetInt");
		Util::AddFunc(pLua, IGameEvent_GetFloat, "GetFloat");
		Util::AddFunc(pLua, IGameEvent_GetString, "GetString");
		Util::AddFunc(pLua, IGameEvent_SetBool, "SetBool");
		Util::AddFunc(pLua, IGameEvent_SetInt, "SetInt");
		Util::AddFunc(pLua, IGameEvent_SetFloat, "SetFloat");
		Util::AddFunc(pLua, IGameEvent_SetString, "SetString");
	pLua->Pop(1);

	if (pLua != g_Lua)
	{
		if (Util::PushTable(pLua, "gameevent"))
		{
			pLua->Pop(1);
		} else {
			pLua->CreateTable();
			Util::FinishTable(pLua, "gameevent");
		}
	}

	if (Util::PushTable(pLua, "gameevent"))
	{
		if (pLua == g_Lua) // Don't exist in the HolyLua state since these are specific to the main lua state.
		{
			Util::AddFunc(pLua, gameevent_GetListeners, "GetListeners");
			Util::AddFunc(pLua, gameevent_RemoveListener, "RemoveListener");
		}

		Util::AddFunc(pLua, gameevent_GetClientListeners, "GetClientListeners");
		Util::AddFunc(pLua, gameevent_RemoveClientListener, "RemoveClientListener");
		Util::AddFunc(pLua, gameevent_AddClientListener, "AddClientListener");

		Util::AddFunc(pLua, gameevent_Create, "Create");
		Util::AddFunc(pLua, gameevent_FireEvent, "FireEvent");
		Util::AddFunc(pLua, gameevent_FireClientEvent, "FireClientEvent");
		Util::AddFunc(pLua, gameevent_DuplicateEvent, "DuplicateEvent");
		Util::AddFunc(pLua, gameevent_BlockCreation, "BlockCreation");

		pLua->GetField(-1, "Listen");
		if (pLua->IsType(-1, GarrysMod::Lua::Type::Function))
		{
			pLua->PushString("vote_cast"); // Yes this is a valid gameevent.
			pLua->CallFunctionProtected(1, 0, true);
			CGameEventDescriptor* descriptor = pGameEventManager->GetEventDescriptor("vote_cast");
			if (descriptor)
			{
				FOR_EACH_VEC(descriptor->listeners, i)
				{
					pLuaGameEventListener = (IGameEventListener2*)descriptor->listeners[i]->m_pCallback;
					descriptor->listeners.Remove(i); // We also remove the listener again
					break;
				}
			}

			if (!pLuaGameEventListener)
				Warning(PROJECT_NAME ": Failed to find pLuaGameEventListener!\n");
		} else {
			pLua->Pop(1);

			// No listener function? We should probably add one
		}

		Util::PopTable(pLua);
	}
}

void CGameeventLibModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	if (Util::PushTable(pLua, "gameevent"))
	{
		Util::RemoveField(pLua, "GetListeners");
		Util::RemoveField(pLua, "RemoveListener");
		Util::RemoveField(pLua, "GetClientListeners");
		Util::RemoveField(pLua, "RemoveClientListener");
		Util::RemoveField(pLua, "AddClientListener");
	}
	Util::PopTable(pLua);
}

#if SYSTEM_WINDOWS
DETOUR_THISCALL_START()
	DETOUR_THISCALL_ADDRETFUNC1( hook_CBaseClient_ProcessListenEvents, bool, ProcessListenEvents, CBaseClient*, CLC_ListenEvents* );
DETOUR_THISCALL_FINISH();
#endif

void CGameeventLibModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	DETOUR_PREPARE_THISCALL();
	SourceSDK::ModuleLoader engine_loader("engine");
	Detour::Create(
		&detour_CBaseClient_ProcessListenEvents, "CBaseClient::ProcessListenEvents",
		engine_loader.GetModule(), Symbols::CBaseClient_ProcessListenEventsSym,
		(void*)DETOUR_THISCALL(hook_CBaseClient_ProcessListenEvents, ProcessListenEvents), m_pID
	);

#if ARCHITECTURE_IS_X86
	func_CGameEventManager_AddListener = (Symbols::CGameEventManager_AddListener)Detour::GetFunction(engine_loader.GetModule(), Symbols::CGameEventManager_AddListenerSym);
	Detour::CheckFunction((void*)func_CGameEventManager_AddListener, "CGameEventManager::AddListener");
#endif
}