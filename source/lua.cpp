#include <LuaInterface.h>
#include <GarrysMod/FactoryLoader.hpp>
#include "filesystem.h"
#include "lua.h"
#include "iluashared.h"
#include <GarrysMod/InterfacePointers.hpp>
#include "detours.h"
#include "module.h"
#include "CLuaInterface.h"

bool Lua::PushHook(const char* hook)
{
	if ( !g_Lua )
	{
		Warning("holylib: Lua::PushHook was called while g_Lua was NULL! (%s)\n", hook);
		return false;
	}

	if (!ThreadInMainThread())
	{
		Warning("holylib: Lua::PushHook was called ouside of the main thread! (%s)\n", hook);
		return false;
	}

	g_Lua->GetField(LUA_GLOBALSINDEX, "hook");
		if (g_Lua->GetType(-1) != GarrysMod::Lua::Type::Table)
		{
			g_Lua->Pop(1);
			DevMsg("holylib: Missing hook table!\n");
			return false;
		}

		g_Lua->GetField(-1, "Run");
			if (g_Lua->GetType(-1) != GarrysMod::Lua::Type::Function)
			{
				g_Lua->Pop(2);
				DevMsg("holylib: Missing hook.Run function!\n");
				return false;
			} else {
				g_Lua->Remove(-2);
				g_Lua->PushString(hook);
			}

	return true;
}

void Lua::Init(GarrysMod::Lua::ILuaInterface* LUA)
{
	if (g_Lua)
	{
		Warning("holylib: g_Lua is already Initialized! Skipping... (%p, %p)\n", g_Lua, LUA);
		return;
	}

	g_Lua = LUA;
	Lua::CreateLuaData(g_Lua);
	g_pModuleManager.LuaInit(g_Lua, false);

	std::vector<LuaFindResult> results;
	GetShared()->FindScripts("lua/autorun/_holylib/*.lua", "GAME", results);
	for (LuaFindResult& result : results)
	{
		std::string fileName = "lua/autorun/_holylib/";
		fileName.append(result.GetFileName());
		FileHandle_t fh = g_pFullFileSystem->Open(fileName.c_str(), "rb", "GAME");
		if (fh)
		{
			int length = g_pFullFileSystem->Size(fh);
			char* buffer = new char[length + 1];
			g_pFullFileSystem->Read(buffer, length, fh);
			buffer[length] = 0;
			g_Lua->RunStringEx(fileName.c_str(), "", buffer, true, true, true, true);
			delete[] buffer;
			g_pFullFileSystem->Close(fh);
		}
	}
}

void Lua::ServerInit()
{
	if (g_Lua == nullptr) {
		DevMsg(1, "Lua::ServerInit failed! g_Lua is NULL\n");
		return;
	}

	g_pModuleManager.LuaInit(g_Lua, true);
}

void Lua::Shutdown()
{
	g_pModuleManager.LuaShutdown(g_Lua);

	g_pRemoveLuaUserData = false; // We are iterating over g_pLuaUserData so DON'T modify it.
	for (auto& ref : g_pLuaUserData)
	{
		if (Util::holylib_debug_mainutil.GetBool())
			Msg("holylib: This should NEVER happen! Discarding of old userdata %p\n", ref);

		delete ref;
	}
	g_pRemoveLuaUserData = true;
	g_pLuaUserData.clear();
}

void Lua::FinalShutdown()
{
	Lua::RemoveLuaData(g_Lua);

	// g_Lua is bad at this point / the lua_State is already gone so we CAN'T allow any calls
	g_Lua = NULL;

	for (auto& ref : Util::g_pReference)
	{
		if (Util::holylib_debug_mainutil.GetBool())
			Msg("holylib: This should NEVER happen! Discarding of old reference %i\n", ref);
	}
	Util::g_pReference.clear();
}

void Lua::ManualShutdown()
{
	if (!g_Lua)
		return;

	Lua::Shutdown();
	Lua::FinalShutdown();
}

static bool bManualShutdown = false;
static Detouring::Hook detour_InitLuaClasses;
static void hook_InitLuaClasses(GarrysMod::Lua::ILuaInterface* LUA) // ToDo: Add a hook to Lua::Startup or whatever it's name was and use that for the ServerInit.
{
	detour_InitLuaClasses.GetTrampoline<Symbols::InitLuaClasses>()(LUA);

	Lua::Init(LUA);
}

static Detouring::Hook detour_CLuaInterface_Shutdown;
static void hook_CLuaInterface_Shutdown(GarrysMod::Lua::ILuaInterface* LUA)
{
	if ((void*)LUA == (void*)g_Lua)
		Lua::Shutdown();

	detour_CLuaInterface_Shutdown.GetTrampoline<Symbols::CLuaInterface_Shutdown>()(LUA); 
	// Garbage collection will kick in so our remaining objects could call theirs __gc function so g_Lua still needs to be valid.

	Lua::FinalShutdown();
}

void Lua::AddDetour() // Our Lua Loader.
{
	if (!bManualShutdown)
	{
		SourceSDK::ModuleLoader server_loader("server");
		Detour::Create(
			&detour_InitLuaClasses, "InitLuaClasses",
			server_loader.GetModule(), Symbols::InitLuaClassesSym,
			(void*)hook_InitLuaClasses, 0
		);

		SourceSDK::ModuleLoader lua_shared_loader("lua_shared");
		Detour::Create(
			&detour_CLuaInterface_Shutdown, "CLuaInterface::Shutdown",
			lua_shared_loader.GetModule(), Symbols::CLuaInterface_ShutdownSym,
			(void*)hook_CLuaInterface_Shutdown, 0
		);
	}
}

void Lua::SetManualShutdown()
{
	bManualShutdown = true;
}

GarrysMod::Lua::ILuaInterface* Lua::GetRealm(unsigned char realm) {
	SourceSDK::FactoryLoader luashared_loader("lua_shared");
	GarrysMod::Lua::ILuaShared* LuaShared = (GarrysMod::Lua::ILuaShared*)luashared_loader.GetFactory()(GMOD_LUASHARED_INTERFACE, nullptr);
	if (LuaShared == nullptr) {
		Msg("holylib: failed to get ILuaShared!\n");
		return nullptr;
	}

	return LuaShared->GetLuaInterface(realm);
}

GarrysMod::Lua::ILuaShared* Lua::GetShared() {
	SourceSDK::FactoryLoader luashared_loader("lua_shared");
	if ( !luashared_loader.GetFactory() )
		Msg("holylib: About to crash!\n");

	return luashared_loader.GetInterface<GarrysMod::Lua::ILuaShared>(GMOD_LUASHARED_INTERFACE);
}

extern GarrysMod::Lua::ILuaGameCallback* g_LuaCallback;
GarrysMod::Lua::ILuaInterface* Lua::CreateInterface()
{
	GarrysMod::Lua::ILuaInterface* LUA = CreateLuaInterface(true);
	LUA->Init(g_LuaCallback, true);

	return LUA;
}

void Lua::DestroyInterface(GarrysMod::Lua::ILuaInterface* LUA)
{
	CloseLuaInterface(LUA);
}

/*
 * Where do we store our StateData?
 * In the ILuaInterface itself.
 * We abuse the GetPathID var as it's a char[32] but it'll never actually fully use it.
 * Why? Because I didn't want to use yet another unordered_map for this, also this should be faster.
 */
Lua::StateData* Lua::GetLuaData(GarrysMod::Lua::ILuaInterface* LUA)
{
	return *reinterpret_cast<Lua::StateData**>((char*)LUA->GetPathID() + 24);
}

void Lua::CreateLuaData(GarrysMod::Lua::ILuaInterface* LUA)
{
	if (Lua::GetLuaData(LUA))
		return;

	char* pathID = (char*)LUA->GetPathID();
	Lua::StateData* data = new Lua::StateData;
	*reinterpret_cast<Lua::StateData**>(pathID + 24) = data;
	Msg("holylib - Created thread data %p\n", data);
}

void Lua::RemoveLuaData(GarrysMod::Lua::ILuaInterface* LUA)
{
	auto data = Lua::GetLuaData(LUA);
	if (!data)
		return;

	delete data;
	*reinterpret_cast<Lua::StateData**>((char*)LUA->GetPathID() + 24) = NULL;
	Msg("holylib - Removed thread data %p\n", data);
}

static void LuaCheck(const CCommand& args)
{
	if (!g_Lua)
		return;

	Msg("holylib - Found data %p\n", Lua::GetLuaData(g_Lua));
}
static ConCommand luacheck("holylib_luacheck", LuaCheck, "Temp", 0);