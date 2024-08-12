#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "sourcesdk/GameEventManager.h"
#include "lua.h"
#include "sourcesdk/baseclient.h"
#include "iserver.h"
#include "vprof.h"
#include "sourcesdk/netmessages.h"

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
		LUA->PushNumber(desciptor->listeners.Count());
	} else {
		LUA->CreateTable();
			FOR_EACH_VEC(pManager->m_GameEvents, i)
			{
				CGameEventDescriptor& descriptor = pManager->m_GameEvents[i];
				LUA->PushNumber(descriptor.listeners.Count());
				LUA->SetField(-2, (const char*)&descriptor.name); // Does it even need to be a const char* ?
			}
	}

	return 1;
}

static CUtlVector<IGameSystem*>* s_GameSystems;
LUA_FUNCTION_STATIC(gameevent_RemoveListener)
{
	const char* strEvent = LUA->CheckString(1);

	bool bSuccess = false;
	IGameEventListener2* pLuaGameEventListener = NULL;
	FOR_EACH_VEC(*s_GameSystems, i)
	{
		if (V_stricmp((*s_GameSystems)[i]->Name(), "LuaGameEventListener") == 0)
		{
			pLuaGameEventListener = (IGameEventListener2*)(*s_GameSystems)[i];
			break;
		}
	}

	if (pLuaGameEventListener)
	{
		CGameEventDescriptor* desciptor = pManager->GetEventDescriptor(strEvent);
		FOR_EACH_VEC(desciptor->listeners, i)
		{
			CGameEventCallback* callback = desciptor->listeners[i];
			if (callback->m_nListenerType != CGameEventManager::SERVERSIDE)
				continue;

			IGameEventListener2* listener = (IGameEventListener2*)callback->m_pCallback;
			//Msg("Pointer 1: %hhu\nPointer 2: %hhu\n", (uint8_t*)listener, (uint8_t*)pLuaGameEventListener + 12);
			if ( (uint8_t*)listener == ((uint8_t*)pLuaGameEventListener + 12) ) // WHY is pLuaGameEventListener always off by 12 bytes? I HATE THAT THIS WORKS
			{
				desciptor->listeners.Remove(i); // ToDo: Verify that this doesn't cause a memory leak because CGameEventCallback isn't deleted.
				bSuccess = true;
				break;
			}
		}
	} else {
		Warning("Failed to find LuaGameEventListener in GameSystems?\n");
	}

	LUA->PushBool(bSuccess);

	return 1;
}

LUA_FUNCTION_STATIC(gameevent_GetClientListeners)
{
	if (LUA->IsType(1, GarrysMod::Lua::Type::Entity))
	{
		CBasePlayer* pEntity = Util::Get_Player(1, true);
		CBaseClient* pClient = Util::GetClientByPlayer(pEntity);

		LUA->CreateTable();
		int idx = 0;
		FOR_EACH_VEC(pManager->m_GameEvents, i)
		{
			CGameEventDescriptor* desciptor = pManager->GetEventDescriptor(i);
			FOR_EACH_VEC(desciptor->listeners, i)
			{
				CGameEventCallback* callback = desciptor->listeners[i];
				if (callback->m_nListenerType != CGameEventManager::CLIENTSTUB)
					continue;

				IGameEventListener2* listener = (IGameEventListener2*)callback->m_pCallback;
				Msg("Pointer 1: %hhu\nPointer 2: %hhu\n", (uint8_t*)listener, (uint8_t*)pClient + 12);
				if ( (uint8_t*)listener == ((uint8_t*)pClient + 12) ) // WHY is pLuaGameEventListener always off by 12 bytes? I HATE THAT THIS WORKS
				{
					++idx;
					LUA->PushNumber(idx);
					LUA->PushString(desciptor->name);
					LUA->SetTable(-3);
					break;
				}
			}
		}
	} else {
		LUA->CreateTable();
		int iClient = 0;
		for (int iClient = 0; iClient<Util::server->GetMaxClients(); ++iClient)
		{
			Util::Push_Entity(Util::GetCBaseEntityFromEdict(Util::engineserver->PEntityOfEntIndex(iClient)));
			LUA->CreateTable();

			CBaseClient* pClient = Util::GetClientByIndex(iClient);
			int idx = 0;
			FOR_EACH_VEC(pManager->m_GameEvents, i)
			{
				CGameEventDescriptor* desciptor = pManager->GetEventDescriptor(i);
				FOR_EACH_VEC(desciptor->listeners, i)
				{
					CGameEventCallback* callback = desciptor->listeners[i];
					if (callback->m_nListenerType != CGameEventManager::CLIENTSTUB)
						continue;

					IGameEventListener2* listener = (IGameEventListener2*)callback->m_pCallback;
					Msg("Pointer 1: %hhu\nPointer 2: %hhu\n", (uint8_t*)listener, (uint8_t*)pClient + 12);
					if ( (uint8_t*)listener == ((uint8_t*)pClient + 12) ) // WHY is pLuaGameEventListener always off by 12 bytes? I HATE THAT THIS WORKS
					{
						++idx;
						LUA->PushNumber(idx);
						LUA->PushString(desciptor->name);
						LUA->SetTable(-3);
						break;
					}
				}
			}
		}
	}

	return 1;
}

LUA_FUNCTION_STATIC(gameevent_RemoveClientListener)
{
	CBasePlayer* pEntity = Util::Get_Player(1, true);
	const char* strEvent = g_Lua->CheckStringOpt(2, NULL);

	bool bSuccess = false;
	if (strEvent)
	{
		CGameEventDescriptor* desciptor = pManager->GetEventDescriptor(strEvent);
		FOR_EACH_VEC(desciptor->listeners, i)
		{
			CGameEventCallback* callback = desciptor->listeners[i];
			if (callback->m_nListenerType != CGameEventManager::CLIENTSTUB)
				continue;

			IGameEventListener2* listener = (IGameEventListener2*)callback->m_pCallback;
			Msg("Pointer 1: %hhu\nPointer 2: %hhu\n", (uint8_t*)listener, (uint8_t*)pEntity + 12);
			if ( (uint8_t*)listener == ((uint8_t*)pEntity + 12) ) // WHY is pLuaGameEventListener always off by 12 bytes? I HATE THAT THIS WORKS
			{
				desciptor->listeners.Remove(i); // ToDo: Verify that this doesn't cause a memory leak because CGameEventCallback isn't deleted.
				bSuccess = true;
				break;
			}
		}
	} else {
		pManager->RemoveListener(Util::GetClientByPlayer(pEntity));
		bSuccess = true; // Always true?
	}

	LUA->PushBool(bSuccess);

	return 1;
}

LUA_FUNCTION_STATIC(gameevent_AddClientListener)
{
	CBasePlayer* pEntity = Util::Get_Player(1, true);
	const char* strEvent = g_Lua->CheckStringOpt(2, NULL);

	CGameEventDescriptor* desciptor = pManager->GetEventDescriptor(strEvent);
	pManager->AddListener(Util::GetClientByPlayer(pEntity), desciptor, CGameEventManager::CLIENTSTUB);

	return 0;
}

Detouring::Hook detour_CBaseClient_ProcessListenEvents;
bool hook_CBaseClient_ProcessListenEvents(CBaseClient* client, CLC_ListenEvents* msg)
{
	VPROF_BUDGET("HolyLib - ProcessGameEventList", VPROF_BUDGETGROUP_OTHER_NETWORKING);

	if (!gameevent_callhook.GetBool())
		return detour_CBaseClient_ProcessListenEvents.GetTrampoline<Symbols::CBaseClient_ProcessListenEvents>()(client, msg);

	CBasePlayer* pPlayer = Util::GetPlayerByClient(client);
	int idx = 0;
	g_Lua->CreateTable();
		for (int i=0; i < MAX_EVENT_NUMBER; i++)
		{
			if (msg->m_EventArray.Get(i))
			{
				CGameEventDescriptor *descriptor = pManager->GetEventDescriptor(i);

				if (descriptor)
				{
					++idx;
					g_Lua->PushNumber(idx);
					g_Lua->PushString(descriptor->name);
					g_Lua->SetTable(-3);
				}
			}
		}

	int iReference = g_Lua->ReferenceCreate();
	if (Lua::PushHook("HolyLib::PreProcessGameEvent"))
	{
		Util::Push_Entity((CBaseEntity*)pPlayer);
		g_Lua->ReferencePush(iReference);
		g_Lua->CallFunctionProtected(3, 1, true);

		bool pCancel = g_Lua->GetBool(-1);
		g_Lua->Pop(-1);
		if (pCancel)
		{
			g_Lua->ReferenceFree(iReference);
			return true;
		}
	}

	detour_CBaseClient_ProcessListenEvents.GetTrampoline<Symbols::CBaseClient_ProcessListenEvents>()(client, msg);

	if (Lua::PushHook("HolyLib::PostProcessGameEvent"))
	{
		Util::Push_Entity((CBaseEntity*)pPlayer);
		g_Lua->ReferencePush(iReference);
		g_Lua->CallFunctionProtected(3, 0, true);
	}

	g_Lua->ReferenceFree(iReference);

	return true;
}

void CGameeventLibModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	pManager = (CGameEventManager*)appfn[0](INTERFACEVERSION_GAMEEVENTSMANAGER2, NULL);
	Detour::CheckValue("get interface", "CGameEventManager", pManager != NULL);
}

void CGameeventLibModule::LuaInit(bool bServerInit)
{
	if (!bServerInit)
	{
		g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
			g_Lua->GetField(-1, "gameevent");
				if (g_Lua->IsType(-1, GarrysMod::Lua::Type::Table)) // Maybe add a hook that allows one to block the networking of a gameevent?
				{
					Util::AddFunc(gameevent_GetListeners, "GetListeners");
					Util::AddFunc(gameevent_RemoveListener, "RemoveListener");

					Util::AddFunc(gameevent_GetClientListeners, "GetClientListeners");
					Util::AddFunc(gameevent_RemoveClientListener, "RemoveClientListener");
					Util::AddFunc(gameevent_AddClientListener, "AddClientListener");
				}
		g_Lua->Pop(2);
	}
}

void CGameeventLibModule::LuaShutdown()
{
	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		g_Lua->GetField(-1, "gameevent");
			if (g_Lua->IsType(-1, GarrysMod::Lua::Type::Table))
			{
				g_Lua->PushNil();
				g_Lua->SetField(-2, "GetListeners");

				g_Lua->PushNil();
				g_Lua->SetField(-2, "RemoveListener");

				g_Lua->PushNil();
				g_Lua->SetField(-2, "GetClientListeners");

				g_Lua->PushNil();
				g_Lua->SetField(-2, "RemoveClientListener");

				g_Lua->PushNil();
				g_Lua->SetField(-2, "AddClientListener");
			}
	g_Lua->Pop(2);
}

void CGameeventLibModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }

	SourceSDK::ModuleLoader engine_loader("engine");
	Detour::Create(
		&detour_CBaseClient_ProcessListenEvents, "CBaseClient::ProcessListenEvents",
		engine_loader.GetModule(), Symbols::CBaseClient_ProcessListenEventsSym,
		(void*)hook_CBaseClient_ProcessListenEvents, m_pID
	);

	SourceSDK::FactoryLoader server_loader("server");
	s_GameSystems = Detour::ResolveSymbol<CUtlVector<IGameSystem*>>(server_loader, Symbols::s_GameSystemsSym);
	Detour::CheckValue("get class", "s_GameSystems", s_GameSystems != NULL);
}