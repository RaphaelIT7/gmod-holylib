#include "module.h"
#include "symbols.h"
#include "detours.h"
#include "LuaInterface.h"
#include "lua.h"
#include "tier0/icommandline.h"
#include "iluashared.h"
#include "sourcesdk/GameEventManager.h"
#include <atomic>
#include <xmmintrin.h>

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

static std::atomic<GarrysMod::Lua::ILuaInterface*> g_HolyLua = nullptr;
GarrysMod::Lua::ILuaInterface* GetHolyLuaInterface()
{
	return g_HolyLua.load();
}

static void OnLuaChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	// Util::Load may load convars and set them, meaning this may be called before we're even ready!
	if (!(g_pModuleManager.GetStatus() & LoadStatus_Init))
		return;

	bool bNewValue = ((ConVar*)convar)->GetBool();

	bool bExists = false;
	{
		Lua::ScopedThreadAccess pThreadScope;
		bExists = GetHolyLuaInterface() != nullptr;
	}

	if (!bNewValue && bExists) {
		g_pHolyLuaModule.HolyLua_Shutdown();
	} else if (bNewValue && !bExists) {
		g_pHolyLuaModule.HolyLua_Init();
	}
}
static ConVar holylib_lua("holylib_lua", "1", 0, "If enabled, it will create a new lua interface that will exist until holylib is unloaded", OnLuaChange);

static void lua_run_holylibCmd(const CCommand &args)
{
	if (args.ArgC() < 1 || Q_stricmp(args.Arg(1), "") == 0)
	{
		Msg("Usage: lua_run_holylib <code>\n");
		return;
	}

	Lua::ScopedThreadAccess pThreadScope;
	Lua::ThreadAccess pAccess(GetHolyLuaInterface());
	if (pAccess.IsValid())
		pAccess.GetLua()->RunString("RunString", "", args.ArgS(), true, true);
}
static ConCommand lua_run_holylib("lua_run_holylib", lua_run_holylibCmd, "Runs code in the holylib lua state", 0);

void CHolyLuaModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	g_pHolyLuaModule.HolyLua_Init();
}

void CHolyLuaModule::Think(bool bSimulating)
{
	Lua::ScopedThreadAccess pThreadScope;
	Lua::ThreadAccess pAccess(GetHolyLuaInterface());
	if (pAccess.IsValid())
	{
		g_pModuleManager.LuaThink(pAccess.GetLua());
		pAccess.GetLua()->Cycle();
	}
}

void CHolyLuaModule::Shutdown()
{
	g_pHolyLuaModule.HolyLua_Shutdown();
}

void CHolyLuaModule::HolyLua_Init()
{
	if (!holylib_lua.GetBool() && !CommandLine()->FindParm("-holylib_lua"))
		return;

	bool bShutdown = false;
	{
		Lua::ScopedThreadAccess pThreadScope;
		bShutdown = GetHolyLuaInterface() != nullptr;
	}

	if (bShutdown)
	{
		Warning(PROJECT_NAME " - HolyLua: Called init while already having an interface!\n");
		HolyLua_Shutdown();
	}

	GarrysMod::Lua::ILuaInterface* pHolyLua = Lua::CreateInterface();

	// Now add all supported HolyLib modules into the new interface.
	g_pModuleManager.LuaInit(pHolyLua, false);

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

			pHolyLua->RunStringEx(fileName.c_str(), "", buffer, true, true, true, true);

			delete[] buffer;

			g_pFullFileSystem->Close(fh);
		}
	}

	Lua::CriticalThreadAccess pThreadScope;
	g_HolyLua.store(pHolyLua);
}

void CHolyLuaModule::HolyLua_Shutdown()
{
	Lua::CriticalThreadAccess pThreadScope;
	Lua::ThreadAccess pScope(GetHolyLuaInterface());
	if (pScope.IsValid())
	{
		g_pModuleManager.LuaShutdown(pScope.GetLua());
		Lua::DestroyInterface(pScope.GetLua());
		g_HolyLua.store(nullptr);
	}
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

struct ILuaValue
{
	unsigned char type = -1;

	double number = -1;
	const char* string = "";
	std::unordered_map<ILuaValue*, ILuaValue*> tbl;
	float x, y, z;
	void* data = nullptr; // Used for LUA_File
};

class HolyLuaModuleData : public Lua::ModuleData
{
public:
	CustomGameEventListener m_pEventListener;
};
LUA_GetModuleData(HolyLuaModuleData, g_pHolyLuaModule, HolyLua)

LUA_FUNCTION_STATIC(gameevent_Listen)
{
	const char* name = LUA->CheckString(1);

	auto pData = GetHolyLuaLuaData(LUA);
	if (!Util::gameeventmanager->FindListener(&pData->m_pEventListener, name))
		Util::gameeventmanager->AddListener(&pData->m_pEventListener, name, false);
 
	return 0;
}

LUA_FUNCTION(holylua_RunString)
{
	const char* pCode = LUA->CheckString(1);

	Lua::ScopedThreadAccess pThreadScope;
	Lua::ThreadAccess pAccess(GetHolyLuaInterface());
	if (pAccess.IsValid())
	{
		if (LUA == pAccess.GetLua())
			return 0;

		pAccess.GetLua()->RunString("RunString", "", pCode, true, true);
		LUA->PushBool(true);
		return 1;
	}

	LUA->PushBool(false);
    return 1;
}

LUA_FUNCTION_STATIC(GetMXCSR)
{
	unsigned int mxcsr = _mm_getcsr();
	printf("MXCSR: %08x\n", mxcsr);
 
	return 0;
}

LUA_FUNCTION_STATIC(SetMXCSR)
{
	unsigned int csr = _mm_getcsr();
	csr &= ~(1 << 15); // disable FTZ
	csr &= ~(1 << 6);  // disable DAZ
	_mm_setcsr(csr);
 
	return 0;
}

void CHolyLuaModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	Util::StartTable(pLua);
		Util::AddFunc(pLua, holylua_RunString, "RunString");
		Util::AddFunc(pLua, GetMXCSR, "GetMXCSR");
		Util::AddFunc(pLua, SetMXCSR, "SetMXCSR");
	Util::FinishTable(pLua, "holylua");

	Lua::ScopedThreadAccess pThreadScope;
	if (pLua != GetHolyLuaInterface())
		return;

	HolyLuaModuleData* pLuaData = new HolyLuaModuleData;
	Lua::GetLuaData(pLua)->SetModuleData(m_pID, pLuaData);
	pLuaData->m_pEventListener.SetLua(pLua);

	Util::StartTable(pLua);
		Util::AddFunc(pLua, gameevent_Listen, "Listen");
	Util::FinishTable(pLua, "gameevent");
}

void CHolyLuaModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	// No Scope lock since were always called from HolyLua_Shutdown
	if (pLua != GetHolyLuaInterface())
		return;

	auto pData = GetHolyLuaLuaData(pLua);
	Util::gameeventmanager->RemoveListener(&pData->m_pEventListener);
}