#include "module.h"
#include "symbols.h"
#include "detours.h"
#include "LuaInterface.h"
#include "lua.h"
#include "tier0/icommandline.h"
#include "iluashared.h"
#include "sourcesdk/GameEventManager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CHolyLuaModule : public IModule
{
public:
	void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) override;
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void Think(bool bSimulating) override;
	void Shutdown() override;
	const char* Name() override { return "holylua"; };
	int Compatibility() override { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };

public: // Just to make it easier with the ConVar callback.
	void HolyLua_Init();
	void HolyLua_Shutdown();
};

static CHolyLuaModule g_pHolyLuaModule;
IModule* pHolyLuaModule = &g_pHolyLuaModule;

static GarrysMod::Lua::ILuaInterface* g_HolyLua = nullptr;
static void OnLuaChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	bool bNewValue = ((ConVar*)convar)->GetBool();

	if (!bNewValue && g_HolyLua)
	{
		g_pHolyLuaModule.HolyLua_Shutdown();
	}
	else if (bNewValue && !g_HolyLua)
	{
		g_pHolyLuaModule.HolyLua_Init();
	}
}
static ConVar holylib_lua("holylib_lua", "0", 0, "If enabled, it will create a new lua interface that will exist until holylib is unloaded", OnLuaChange);

static void lua_run_holylibCmd(const CCommand &args)
{
	if (args.ArgC() < 1 || Q_stricmp(args.Arg(1), "") == 0)
	{
		Msg("Usage: lua_run_holylib <code>\n");
		return;
	}

	if (g_HolyLua)
		g_HolyLua->RunString("RunString", "", args.ArgS(), true, true);
}
static ConCommand lua_run_holylib("lua_run_holylib", lua_run_holylibCmd, "Runs code in the holylib lua state", 0);

void CHolyLuaModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	g_pHolyLuaModule.HolyLua_Init();
}

void CHolyLuaModule::Think(bool bSimulating)
{
	if (!g_HolyLua)
		return;

	g_pModuleManager.LuaThink(g_HolyLua);
	g_HolyLua->Cycle();
}

void CHolyLuaModule::Shutdown()
{
	g_pHolyLuaModule.HolyLua_Shutdown();
}

void CHolyLuaModule::HolyLua_Init()
{
	if (!holylib_lua.GetBool() && !CommandLine()->FindParm("-holylib_lua"))
		return;

	if (g_HolyLua)
	{
		Warning(PROJECT_NAME " - HolyLua: Called init while already having an interface!\n");
		HolyLua_Shutdown();
	}

	g_HolyLua = Lua::CreateInterface();

	// Now add all supported HolyLib modules into the new interface.
	g_pModuleManager.LuaInit(g_HolyLua, false);

	// Finally, load any holylua scripts
	std::vector<GarrysMod::Lua::LuaFindResult> results;
	Lua::GetShared()->FindScripts("lua/autorun/_holylua/*.lua", "GAME", results);
	for (GarrysMod::Lua::LuaFindResult& result : results)
	{
		std::string fileName = "lua/autorun/_holylua/";
		fileName.append(result.GetFileName());
		FileHandle_t fh = g_pFullFileSystem->Open(fileName.c_str(), "rb", "GAME");
		if (fh)
		{
			int length = (int)g_pFullFileSystem->Size(fh);

			char* buffer = new char[length + 1];
			g_pFullFileSystem->Read(buffer, length, fh);
			buffer[length] = 0;

			g_Lua->RunStringEx(fileName.c_str(), "", buffer, true, true, true, true);

			delete[] buffer;

			g_pFullFileSystem->Close(fh);
		}
	}
}

void CHolyLuaModule::HolyLua_Shutdown()
{
	if (!g_HolyLua)
		return;

	g_pModuleManager.LuaShutdown(g_HolyLua);
	Lua::DestroyInterface(g_HolyLua);
	g_HolyLua = nullptr;
}

static inline void PushEvent(GarrysMod::Lua::ILuaInterface* pLua, CGameEvent* event)
{
	KeyValues* subkey = event->m_pDataKeys->GetFirstSubKey();
	while (subkey)
	{
		KeyValues::types_t type = subkey->GetDataType();
		if (type == KeyValues::TYPE_STRING) {
			pLua->PushString(event->GetString(subkey->GetName()));
		} else if (type == KeyValues::TYPE_UINT64 || type == KeyValues::TYPE_INT) {
			pLua->PushNumber(event->GetInt(subkey->GetName()));
		} else if (type == KeyValues::TYPE_FLOAT) {
			pLua->PushNumber(event->GetFloat(subkey->GetName()));
		} else {
			pLua->PushNil();
			Msg("Invalid Type?!? (%s -> %s)\n", event->GetName(), subkey->GetName());
		}

		pLua->SetField(-2, subkey->GetName());

		subkey = subkey->GetNextKey();
	}
}

class CLuaGameEventCallbackCall : GarrysMod::Lua::ILuaThreadedCall
{
public:
	CLuaGameEventCallbackCall(IGameEvent* pEvent)
	{
		if (!pEvent) // In case of DuplicateEvent failing
			return;

		m_pEvent = (CGameEvent*)pEvent;
	}

	bool IsDone() { return true; } // Trigger Done call
	void OnShutdown() { delete this; } // Cleanup ourself
	void Done(GarrysMod::Lua::ILuaInterface* LUA)
	{
		if (!m_pEvent)
			return;

		if (Lua::PushHook(m_pEvent->GetName(), LUA))
		{
			LUA->CreateTable();
			PushEvent(LUA, m_pEvent); // Pushes into our table

			LUA->CallFunctionProtected(2, 0, true);
		}

		Util::gameeventmanager->FreeEvent(m_pEvent);
	}

private:
	CGameEvent* m_pEvent = nullptr;
};

class CustomGameEventListener : public IGameEventListener2
{
public:
	CustomGameEventListener() = default;
	
	void SetLua(GarrysMod::Lua::ILuaInterface* pLua)
	{
		m_pLua = pLua;
	}

	void FireGameEvent(IGameEvent* pEvent)
	{
		CLuaGameEventCallbackCall* pCallback = new CLuaGameEventCallbackCall(Util::gameeventmanager->DuplicateEvent(pEvent));
		m_pLua->AddThreadedCall((GarrysMod::Lua::ILuaThreadedCall*)pCallback);
	}

private:
	GarrysMod::Lua::ILuaInterface* m_pLua = nullptr;
};

class HolyLuaModuleData : public Lua::ModuleData
{
public:
	CustomGameEventListener m_pEventListener;
};
LUA_GetModuleData(HolyLuaModuleData, g_pHolyLuaModule, HolyLua)

LUA_FUNCTION_STATIC(gameevent_Listen) {
	const char* name = LUA->CheckString(1);

	auto pData = GetHolyLuaLuaData(LUA);
	if (!Util::gameeventmanager->FindListener(&pData->m_pEventListener, name))
		Util::gameeventmanager->AddListener(&pData->m_pEventListener, name, false);
 
	return 0;
}

void CHolyLuaModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (pLua != g_HolyLua)
		return;

	Lua::GetLuaData(pLua)->SetModuleData(m_pID, new HolyLuaModuleData);

	auto pData = GetHolyLuaLuaData(pLua);
	pData->m_pEventListener.SetLua(pLua);

	Util::StartTable(pLua);
		Util::AddFunc(pLua, gameevent_Listen, "Listen");
	Util::FinishTable(pLua, "gameevent");
}

void CHolyLuaModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	if (pLua != g_HolyLua)
		return;

	auto pData = GetHolyLuaLuaData(pLua);
	Util::gameeventmanager->RemoveListener(&pData->m_pEventListener);
}