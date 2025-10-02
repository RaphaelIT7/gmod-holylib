#include "module.h"
#include "symbols.h"
#include "detours.h"
#include "LuaInterface.h"
#include "lua.h"
#include "tier0/icommandline.h"
#include "iluashared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CHolyLuaModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void Think(bool bSimulating) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual const char* Name() { return "holylua"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };

public: // Just to make it easier with the ConVar callback.
	void HolyLua_Init();
	void HolyLua_Shutdown();
};

static CHolyLuaModule g_pHolyLuaModule;
IModule* pHolyLuaModule = &g_pHolyLuaModule;

static GarrysMod::Lua::ILuaInterface* g_HolyLua = NULL;
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
	g_pHolyLuaModule.HolyLua_Shutdown();
}

void CHolyLuaModule::Think(bool bSimulating)
{
	if (!g_HolyLua)
		return;

	g_pModuleManager.LuaThink(g_HolyLua);
}

void CHolyLuaModule::Shutdown()
{
	g_pHolyLuaModule.HolyLua_Init();
}

void CHolyLuaModule::HolyLua_Init()
{
	if (!holylib_lua.GetBool() && !CommandLine()->FindParm("-holylib_lua"))
		return;

	g_HolyLua = Lua::CreateInterface();

	// Now add all supported HolyLib modules into the new interface.
	g_pModuleManager.LuaInit(g_HolyLua, false);
	g_pModuleManager.LuaInit(g_HolyLua, true);

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
	g_HolyLua = NULL;
}

void CHolyLuaModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (pLua != g_HolyLua)
		return;
}

void CHolyLuaModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	if (pLua != g_HolyLua)
		return;
}