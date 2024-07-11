#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "sourcesdk/GameEventManager.h"
#include "lua.h"

class CUserCmd; // Fixes an error in igamesystem.h
#include <igamesystem.h>

class CGameeventLibModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* fn);
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour(bool bPreServer);
	virtual void Think(bool bSimulating);
	virtual void Shutdown();
	virtual const char* Name() { return "gameevent"; };
	virtual void LoadConfig(KeyValues* config) {};
};

CGameeventLibModule g_pGameeventLibModule;
IModule* pGameeventLibModule = &g_pGameeventLibModule;

CGameEventDescriptor *CGameEventManager::GetEventDescriptor(const char * name)
{
	if ( !name || !name[0] )
		return NULL;

	for (int i=0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor *descriptor = &m_GameEvents[i];

		if ( Q_strcmp( descriptor->name, name ) == 0 )
			return descriptor;
	}

	return NULL;
}

CGameEventManager* pManager;
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

CUtlVector<IGameSystem*>* s_GameSystems;
LUA_FUNCTION_STATIC(gameevent_RemoveListener)
{
	const char* strEvent = LUA->CheckString(1);

	bool bSuccess = false;
	IGameEventListener2* pLuaGameEventListener = NULL;
	FOR_EACH_VEC(*s_GameSystems, i)
	{
		if (V_stricmp((*s_GameSystems)[i]->Name(), "LuaGameEventListener") == 0)
		{
			pLuaGameEventListener = dynamic_cast<IGameEventListener2*>((*s_GameSystems)[i]);
			break;
		}
	}

	if (pLuaGameEventListener)
	{
		CGameEventDescriptor* desciptor = pManager->GetEventDescriptor(strEvent);
		FOR_EACH_VEC(desciptor->listeners, i)
		{
			IGameEventListener2* listener = (IGameEventListener2*)desciptor->listeners[i]->m_pCallback;
			Msg("Pointer 1: %p\nPointer 2: %p\n", listener, pLuaGameEventListener);
			if ( listener == pLuaGameEventListener )
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

void CGameeventLibModule::Init(CreateInterfaceFn* fn)
{
	pManager = (CGameEventManager*)fn[0](INTERFACEVERSION_GAMEEVENTSMANAGER2, NULL);
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
				}
		g_Lua->Pop(2);
	}
}

void CGameeventLibModule::LuaShutdown()
{
}

void CGameeventLibModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }

	SourceSDK::FactoryLoader server_loader("server_srv");
	s_GameSystems = ResolveSymbol<CUtlVector<IGameSystem*>>(server_loader, Symbols::s_GameSystemsSym);
}

void CGameeventLibModule::Think(bool simulating)
{
}

void CGameeventLibModule::Shutdown()
{
}
