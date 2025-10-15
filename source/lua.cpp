#include <LuaInterface.h>
#include <GarrysMod/FactoryLoader.hpp>
#include "filesystem.h"
#include "lua.h"
#include "iluashared.h"
#include <GarrysMod/InterfacePointers.hpp>
#include "detours.h"
#include "module.h"
#include "CLuaInterface.h"
#include "player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Testing functions

static CBaseEntity* pTestWorld = nullptr;
LUA_FUNCTION_STATIC(Test_PushEntity)
{
	// We just push the world entity, in Lua we then test if we got an Entity.
	if (!pTestWorld)
		pTestWorld = Util::GetCBaseEntityFromEdict(Util::engineserver->PEntityOfEntIndex(0));

	Util::Push_Entity(pTestWorld);
	return 1;
}

LUA_FUNCTION_STATIC(Test_RawGetEntity)
{
	Util::Get_Entity(1, true);
	return 0;
}

LUA_FUNCTION_STATIC(Test_GetEntity)
{
	CBaseEntity* pEntity = Util::Get_Entity(1, true);
	EHANDLE* pEntHandle = LUA->GetUserType<EHANDLE>(1, GarrysMod::Lua::Type::Entity);
	// If something broke, either we will return false, or we will crash which is intented to make the tests fail.
	LUA->PushBool(pEntity && pEntity->edict() && pEntity->edict()->m_EdictIndex == pEntHandle->GetEntryIndex());
	return 1;
}

LUA_FUNCTION_STATIC(Test_GetPlayer) // Same as Test_GetEntity though calling Get_Player since it's a seperate independent function.
{
	CBasePlayer* pEntity = Util::Get_Player(1, true);
	EHANDLE* pEntHandle = LUA->GetUserType<EHANDLE>(1, GarrysMod::Lua::Type::Entity);
	LUA->PushBool(pEntity && pEntity->edict() && pEntity->edict()->m_EdictIndex == pEntHandle->GetEntryIndex());
	return 1;
}

struct _HOLYLIB_CORE_TEST
{
	int JustAnExample = 123;
};

static int nCoreTestMetaID = -1;
Push_LuaClass(_HOLYLIB_CORE_TEST, nCoreTestMetaID)
Get_LuaClass(_HOLYLIB_CORE_TEST, nCoreTestMetaID, "_HOLYLIB_CORE_TEST")

Default__index(_HOLYLIB_CORE_TEST);
Default__newindex(_HOLYLIB_CORE_TEST);
Default__GetTable(_HOLYLIB_CORE_TEST);
Default__gc(_HOLYLIB_CORE_TEST, 
	_HOLYLIB_CORE_TEST* pTestData = (_HOLYLIB_CORE_TEST*)pData;
	if (pTestData)
		delete pTestData;
)

LUA_FUNCTION_STATIC(Test_PushTestUserData)
{
	Push__HOLYLIB_CORE_TEST(new _HOLYLIB_CORE_TEST);
	return 1;
}

LUA_FUNCTION_STATIC(Test_GetTestUserData)
{
	_HOLYLIB_CORE_TEST* pTest = Get__HOLYLIB_CORE_TEST(1, true);
	LUA->PushBool(pTest->JustAnExample == 123); // Verify we didn't get garbage
	return 1;
}

LUA_FUNCTION_STATIC(Test_RawGetTestUserData)
{
	Get__HOLYLIB_CORE_TEST(1, true);
	return 0;
}

LUA_FUNCTION_STATIC(Test_ClearLuaTable)
{
	LuaUserData* pUserData = Get__HOLYLIB_CORE_TEST_Data(1, true);
	pUserData->ClearLuaTable();
	return 0;
}

LUA_FUNCTION_STATIC(Test_GetModuleData)
{
	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION_STATIC(Test_RawGetModuleData)
{
	// GetHolyLib_CoreLuaData(LUA);
	return 0;
}

LUA_FUNCTION_STATIC(Test_GetGModVector)
{
	Vector* pData = Get_Vector(1);
	LUA->PushBool(pData->x == 1 && pData->y == 2 && pData->z == 3);
	return 1;
}

LUA_FUNCTION_STATIC(Test_RawGetGModVector)
{
	Get_Vector(1);
	return 0;
}

LUA_FUNCTION_STATIC(Test_EnableStressBots)
{
	if (!g_pCVar)
		return 0;

	ConVar* pConVar = g_pCVar->FindVar("sv_stressbots");
	if (!pConVar)
	{
		LUA->PushBool(false);
		return 1;
	}

	pConVar->SetValue("1");
	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION_STATIC(Test_DisableStressBots)
{
	if (!g_pCVar)
		return 0;

	ConVar* pConVar = g_pCVar->FindVar("sv_stressbots");
	if (!pConVar)
	{
		LUA->PushBool(false);
		return 1;
	}

	pConVar->SetValue("1");
	LUA->PushBool(true);
	return 1;
}

static void SetupCoreTestFunctions(GarrysMod::Lua::ILuaInterface* pLua)
{
	nCoreTestMetaID = g_Lua->CreateMetaTable("_HOLYLIB_CORE_TEST");
		Util::AddFunc(_HOLYLIB_CORE_TEST__index, "__index");
		Util::AddFunc(_HOLYLIB_CORE_TEST__newindex, "__newindex");
		Util::AddFunc(_HOLYLIB_CORE_TEST__gc, "__gc");
		Util::AddFunc(_HOLYLIB_CORE_TEST_GetTable, "GetTable");
	pLua->Pop(1);

	Util::StartTable();
		Util::AddFunc(Test_PushEntity, "PushEntity");
		Util::AddFunc(Test_GetEntity, "GetEntity");
		Util::AddFunc(Test_RawGetEntity, "RawGetEntity"); // Used to test performance
		Util::AddFunc(Test_GetPlayer, "GetPlayer");
		Util::AddFunc(Test_ClearLuaTable, "ClearLuaTable");
		Util::AddFunc(Test_PushTestUserData, "PushTestUserData");
		Util::AddFunc(Test_GetTestUserData, "GetTestUserData");
		Util::AddFunc(Test_RawGetTestUserData, "RawGetTestUserData");
		Util::AddFunc(Test_GetModuleData, "GetModuleData");
		Util::AddFunc(Test_RawGetModuleData, "RawGetModuleData");
		Util::AddFunc(Test_GetGModVector, "GetGModVector");
		Util::AddFunc(Test_RawGetGModVector, "RawGetGModVector");
		
		Util::AddFunc(Test_EnableStressBots, "EnableStressBots"); // Required until we get https://github.com/Facepunch/garrysmod-requests/issues/2948
		Util::AddFunc(Test_DisableStressBots, "DisableStressBots");
	Util::FinishTable("_HOLYLIB_CORE");
}

static void RemoveCoreTestFunctions(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable("_HOLYLIB_CORE");
	pTestWorld = nullptr;
}

// Testing functions end

bool Lua::PushHook(const char* hook)
{
	if ( !g_Lua )
	{
		Warning(PROJECT_NAME ": Lua::PushHook was called while g_Lua was NULL! (%s)\n", hook);
		return false;
	}

	if (!ThreadInMainThread())
	{
		Warning(PROJECT_NAME ": Lua::PushHook was called ouside of the main thread! (%s)\n", hook);
		return false;
	}

	g_Lua->GetField(LUA_GLOBALSINDEX, "hook");
		if (g_Lua->GetType(-1) != GarrysMod::Lua::Type::Table)
		{
			g_Lua->Pop(1);
			DevMsg(PROJECT_NAME ": Missing hook table!\n");
			return false;
		}

		g_Lua->GetField(-1, "Run");
			if (g_Lua->GetType(-1) != GarrysMod::Lua::Type::Function)
			{
				g_Lua->Pop(2);
				DevMsg(PROJECT_NAME ": Missing hook.Run function!\n");
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
		Warning(PROJECT_NAME ": g_Lua is already Initialized! Skipping... (%p, %p)\n", g_Lua, LUA);
		return;
	}

	g_Lua = LUA;
	Lua::CreateLuaData(g_Lua, true);
	g_pModuleManager.LuaInit(g_Lua, false);

	SetupCoreTestFunctions(g_Lua);

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
	if (g_Lua == NULL) {
		DevMsg(1, "Lua::ServerInit failed! g_Lua is NULL\n");
		return;
	}

	g_pModuleManager.LuaInit(g_Lua, true);
}

void Lua::Shutdown()
{
	g_pModuleManager.LuaShutdown(g_Lua);

	RemoveCoreTestFunctions(g_Lua);

#if HOLYLIB_UTIL_DEBUG_LUAUSERDATA
	g_pRemoveLuaUserData = false; // We are iterating over g_pLuaUserData so DON'T modify it.
	for (auto& ref : g_pLuaUserData)
	{
		// If it doesn't hold a reference of itself, the gc will take care of it & call __gc in the lua_close call.
		// This check only focuses on LuaUserData that holds a reference to itself as thoes are never freed by the GC even when lua_close is called.
		if (ref->GetReference() == -1)
			continue;

		if (Util::holylib_debug_mainutil.GetBool())
			Msg(PROJECT_NAME ": This should NEVER happen! Discarding of old userdata %p\n", ref);

		delete ref;
	}
	g_pRemoveLuaUserData = true;
	g_pLuaUserData.clear();
#endif
}

void Lua::FinalShutdown()
{
	Lua::RemoveLuaData(g_Lua);

	// g_Lua is bad at this point / the lua_State is already gone so we CAN'T allow any calls
	g_Lua = NULL;

	for (auto& ref : Util::g_pReference)
	{
		if (Util::holylib_debug_mainutil.GetBool())
			Msg(PROJECT_NAME ": This should NEVER happen! Discarding of old reference %i\n", ref);
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
		Msg(PROJECT_NAME ": failed to get ILuaShared!\n");
		return nullptr;
	}

	return LuaShared->GetLuaInterface(realm);
}

GarrysMod::Lua::ILuaShared* Lua::GetShared() {
	SourceSDK::FactoryLoader luashared_loader("lua_shared");
	if ( !luashared_loader.GetFactory() )
		Msg(PROJECT_NAME ": About to crash!\n");

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

void Lua::CreateLuaData(GarrysMod::Lua::ILuaInterface* LUA, bool bNullOut)
{
	if (bNullOut)
	{
		char* pathID = (char*)LUA->GetPathID();
		size_t usedLength = strlen(pathID);
		memset(pathID + usedLength, 0, 32 - usedLength); // Gmod doesn't NULL out m_sPathID which means it could contain junk.
	}

	if (Lua::GetLuaData(LUA))
	{
		Msg("holylib - Skipping thread data creation since we already found data %p\n", Lua::GetLuaData(LUA));
		return;
	}

	char* pathID = (char*)LUA->GetPathID();
	Lua::StateData* data = new Lua::StateData;
	*reinterpret_cast<Lua::StateData**>(pathID + 24) = data;
	Msg("holylib - Created thread data %p (%s)\n", data, pathID);
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