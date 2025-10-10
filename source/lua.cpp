#include <LuaInterface.h>
#include <GarrysMod/FactoryLoader.hpp>
#include "filesystem.h"
#include "lua.h"
#include "iluashared.h"
#include <GarrysMod/InterfacePointers.hpp>
#include "detours.h"
#include "module.h"
#include "CLuaInterface.h"
#include "versioninfo.h"
#include "detouring/customclassproxy.hpp"
#include "eiface.h"
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

	Util::Push_Entity(LUA, pTestWorld);
	return 1;
}

LUA_FUNCTION_STATIC(Test_RawGetEntity)
{
	Util::Get_Entity(LUA, 1, true);
	return 0;
}

LUA_FUNCTION_STATIC(Test_GetEntity)
{
	CBaseEntity* pEntity = Util::Get_Entity(LUA, 1, true);
	EHANDLE* pEntHandle = LUA->GetUserType<EHANDLE>(1, GarrysMod::Lua::Type::Entity);
	// If something broke, either we will return false, or we will crash which is intented to make the tests fail.
	LUA->PushBool(pEntity && pEntity->edict() && pEntity->edict()->m_EdictIndex == pEntHandle->GetEntryIndex());
	return 1;
}

LUA_FUNCTION_STATIC(Test_GetPlayer) // Same as Test_GetEntity though calling Get_Player since it's a seperate independent function.
{
	CBasePlayer* pEntity = Util::Get_Player(LUA, 1, true);
	EHANDLE* pEntHandle = LUA->GetUserType<EHANDLE>(1, GarrysMod::Lua::Type::Entity);
	LUA->PushBool(pEntity && pEntity->edict() && pEntity->edict()->m_EdictIndex == pEntHandle->GetEntryIndex());
	return 1;
}

LUA_FUNCTION_STATIC(Test_ClearLuaTable)
{
	LuaUserData* pUserData = Lua::GetHolyLibUserData(LUA, 1);
	pUserData->ClearLuaTable(LUA);
	return 0;
}

struct _HOLYLIB_CORE_TEST
{
	int JustAnExample = 123;
};

Push_LuaClass(_HOLYLIB_CORE_TEST)
Get_LuaClass(_HOLYLIB_CORE_TEST, "_HOLYLIB_CORE_TEST")

Default__index(_HOLYLIB_CORE_TEST);
Default__newindex(_HOLYLIB_CORE_TEST);
Default__GetTable(_HOLYLIB_CORE_TEST);
Default__gc(_HOLYLIB_CORE_TEST, 
	_HOLYLIB_CORE_TEST* pTestData = (_HOLYLIB_CORE_TEST*)pStoredData;
	if (pTestData)
		delete pTestData;
)

LUA_FUNCTION_STATIC(Test_PushTestUserData)
{
	Push__HOLYLIB_CORE_TEST(LUA, new _HOLYLIB_CORE_TEST);
	return 1;
}

LUA_FUNCTION_STATIC(Test_GetTestUserData)
{
	_HOLYLIB_CORE_TEST* pTest = Get__HOLYLIB_CORE_TEST(LUA, 1, true);
	LUA->PushBool(pTest->JustAnExample == 123); // Verify we didn't get garbage
	return 1;
}

LUA_FUNCTION_STATIC(Test_RawGetTestUserData)
{
	Get__HOLYLIB_CORE_TEST(LUA, 1, true);
	return 0;
}

class LuaCoreTestModuleData : public Lua::ModuleData
{
public:
	int nTest = 456;
};

LUA_GetModuleDataWithID(LuaCoreTestModuleData, HolyLib_Core, 0);

LUA_FUNCTION_STATIC(Test_GetModuleData)
{
	LuaCoreTestModuleData* pData = GetHolyLib_CoreLuaData(LUA);
	LUA->PushBool(pData->nTest == 456);
	return 1;
}

LUA_FUNCTION_STATIC(Test_RawGetModuleData)
{
	GetHolyLib_CoreLuaData(LUA);
	return 0;
}

static void SetupCoreTestFunctions(GarrysMod::Lua::ILuaInterface* pLua)
{
	Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::_HOLYLIB_CORE_TEST, pLua->CreateMetaTable("_HOLYLIB_CORE_TEST"));
		Util::AddFunc(pLua, _HOLYLIB_CORE_TEST__index, "__index");
		Util::AddFunc(pLua, _HOLYLIB_CORE_TEST__newindex, "__newindex");
		Util::AddFunc(pLua, _HOLYLIB_CORE_TEST__gc, "__gc");
		Util::AddFunc(pLua, _HOLYLIB_CORE_TEST_GetTable, "GetTable");
	pLua->Pop(1);

	Lua::GetLuaData(pLua)->SetModuleData(0, new LuaCoreTestModuleData);

	Util::StartTable(pLua);
		Util::AddFunc(pLua, Test_PushEntity, "PushEntity");
		Util::AddFunc(pLua, Test_GetEntity, "GetEntity");
		Util::AddFunc(pLua, Test_RawGetEntity, "RawGetEntity"); // Used to test performance
		Util::AddFunc(pLua, Test_GetPlayer, "GetPlayer");
		Util::AddFunc(pLua, Test_ClearLuaTable, "ClearLuaTable");
		Util::AddFunc(pLua, Test_PushTestUserData, "PushTestUserData");
		Util::AddFunc(pLua, Test_GetTestUserData, "GetTestUserData");
		Util::AddFunc(pLua, Test_RawGetTestUserData, "RawGetTestUserData");
		Util::AddFunc(pLua, Test_GetModuleData, "GetModuleData");
		Util::AddFunc(pLua, Test_RawGetModuleData, "RawGetModuleData");
	Util::FinishTable(pLua, "_HOLYLIB_CORE");
}

static void RemoveCoreTestFunctions(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "_HOLYLIB_CORE");
	pTestWorld = nullptr;
}

// Testing functions end

bool Lua::PushHook(const char* hook, GarrysMod::Lua::ILuaInterface* pLua)
{
	if (!pLua)
	{
		Warning(PROJECT_NAME ": Lua::PushHook was called while pLua was NULL! (%s)\n", hook);
		return false;
	}

	if (!ThreadInMainThread() && pLua == g_Lua)
	{
		Warning(PROJECT_NAME ": Lua::PushHook was called ouside of the main thread! (%s)\n", hook);
		return false;
	}

	pLua->GetField(LUA_GLOBALSINDEX, "hook");
		if (pLua->GetType(-1) != GarrysMod::Lua::Type::Table)
		{
			pLua->Pop(1);
			DevMsg(PROJECT_NAME ": Missing hook table!\n");
			return false;
		}

		pLua->GetField(-1, "Run");
			if (pLua->GetType(-1) != GarrysMod::Lua::Type::Function)
			{
				pLua->Pop(2);
				DevMsg(PROJECT_NAME ": Missing hook.Run function!\n");
				return false;
			} else {
				pLua->Remove(-2);
				pLua->PushString(hook);
			}

	return true;
}

extern void SetupUnHolyVTableForThisShit(GarrysMod::Lua::ILuaInterface* pLua);
void Lua::Init(GarrysMod::Lua::ILuaInterface* LUA)
{
	if (g_Lua)
	{
		Warning(PROJECT_NAME ": g_Lua is already Initialized! Skipping... (%p, %p)\n", g_Lua, LUA);
		return;
	}

	g_Lua = LUA;

	// Setup HolyLib Vars
	LUA->PushBool(true);
	LUA->SetField(GarrysMod::Lua::INDEX_GLOBAL, "_HOLYLIB");

	LUA->PushString(HolyLib_GetRunNumber());
	LUA->SetField(GarrysMod::Lua::INDEX_GLOBAL, "_HOLYLIB_RUN_NUMBER");

	LUA->PushString(HolyLib_GetBranch());
	LUA->SetField(GarrysMod::Lua::INDEX_GLOBAL, "_HOLYLIB_BRANCH");

	LUA->PushString(HolyLib_GetVersion());
	LUA->SetField(GarrysMod::Lua::INDEX_GLOBAL, "_HOLYLIB_VERSION");

	// Lua Interface setup to prepare them for modules.
	Lua::CreateLuaData(g_Lua, true);
	g_pModuleManager.LuaInit(g_Lua, false);
	SetupUnHolyVTableForThisShit(g_Lua);

	SetupCoreTestFunctions(g_Lua); // For Tests.

	std::vector<GarrysMod::Lua::LuaFindResult> results;
	GetShared()->FindScripts("lua/autorun/_holylib/*.lua", "GAME", results);
	for (GarrysMod::Lua::LuaFindResult& result : results)
	{
		std::string fileName = "lua/autorun/_holylib/";
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

	g_Lua->PushNil();
	g_Lua->SetField(GarrysMod::Lua::INDEX_GLOBAL, "_HOLYLIB");

	RemoveCoreTestFunctions(g_Lua);
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
	bool bIsOurInterface = ((void*)LUA == (void*)g_Lua);
	if (bIsOurInterface)
		Lua::Shutdown();

	detour_CLuaInterface_Shutdown.GetTrampoline<Symbols::CLuaInterface_Shutdown>()(LUA); 
	// Garbage collection will kick in so our remaining objects could call theirs __gc function so g_Lua still needs to be valid.

	if (bIsOurInterface)
		Lua::FinalShutdown();
}

static Detouring::Hook detour_GMOD_LoadBinaryModule;
static int hook_GMOD_LoadBinaryModule(lua_State* L, const char* pFileName)
{
	g_pModuleManager.PreLuaModuleLoaded(L, pFileName);

	bool retCode = detour_GMOD_LoadBinaryModule.GetTrampoline<Symbols::GMOD_LoadBinaryModule>()(L, pFileName); 

	g_pModuleManager.PostLuaModuleLoaded(L, pFileName);

	return retCode;
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

	SourceSDK::ModuleLoader lua_shared_loader("lua_shared");
	Detour::Create(
		&detour_GMOD_LoadBinaryModule, "GMOD_LoadBinaryModule",
		lua_shared_loader.GetModule(), Symbols::GMOD_LoadBinaryModuleSym,
		(void*)hook_GMOD_LoadBinaryModule, 0
	);
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

GarrysMod::Lua::ILuaInterface* Lua::CreateInterface()
{
	GarrysMod::Lua::ILuaInterface* LUA = Lua::CreateLuaInterface(true);
	LUA->Init(Lua::GetLuaGameCallback(), true);
	Lua::CreateLuaData(LUA, true); // Required as everything will use the LuaStateData now.

	return LUA;
}

void Lua::DestroyInterface(GarrysMod::Lua::ILuaInterface* LUA)
{
	LUA->Shutdown();
	Lua::RemoveLuaData(LUA);
	Lua::CloseLuaInterface(LUA);
}

// From LuaJIT, though we can strip it out later
// ToDo: Find out how to untangle these internal dependencies, we / our core system should not depend on modules.
// Update: I don't like the usage of MODULE_EXISTS_LUAJIT though it's the best solution
//         else we'd have to detour our own functions and I don't like the idea of possibly ending up with self modifing code
#if MODULE_EXISTS_LUAJIT
extern RawLua::CDataBridge* GetCDataBridgeFromInterface(GarrysMod::Lua::ILuaInterface* pLua);
#endif
LuaUserData* Lua::GetHolyLibUserData(GarrysMod::Lua::ILuaInterface * LUA, int nStackPos)
{
	lua_State* L = LUA->GetState();
	TValue* val = RawLua::index2adr(L, nStackPos);
	if (!val)
		return nullptr;

	if (!tvisudata(val))
	{
#if MODULE_EXISTS_LUAJIT
		if (tviscdata(val))
		{
			RawLua::CDataBridge* pBridge = GetCDataBridgeFromInterface(LUA);
			if (pBridge->pRegisteredTypes.IsBitSet(cdataV(val)->ctypeid))
				return (LuaUserData*)lj_obj_ptr(G(L), val);
		}
#endif

		return nullptr;
	}

	GCudata* luaData = udataV(val); // Very "safe" I know :3
	if (luaData->udtype >= GarrysMod::Lua::Type::UserData)
	{
		return static_cast<LuaUserData*>(static_cast<void*>(luaData));
	}

	return nullptr;
}

bool Lua::CheckHolyLibType(GarrysMod::Lua::ILuaInterface* LUA, int nStackPos, int nType, LuaUserData** pUserData)
{
	LuaUserData* pData = Lua::GetHolyLibUserData(LUA, nStackPos);

	if (pData && pData->GetType() == nType)
	{
		*pUserData = pData;
		return true;
	}

	*pUserData = nullptr;
	return false;
}

// We need to do some hooking for these since our userdata is "special"
class CLuaInterfaceProxy : public Detouring::ClassProxy<GarrysMod::Lua::ILuaInterface, CLuaInterfaceProxy> {
public:
	CLuaInterfaceProxy(GarrysMod::Lua::ILuaInterface* pLua) {
		if (Detour::CheckValue("initialize", "CLuaInterfaceProxy", Initialize(pLua)))
		{
			Detour::CheckValue("CLuaInterface::SetUserType", Hook(&GarrysMod::Lua::ILuaInterface::SetUserType, &CLuaInterfaceProxy::SetUserType));
			Detour::CheckValue("CLuaInterface::GetUserdata", Hook(&GarrysMod::Lua::ILuaInterface::GetUserdata, &CLuaInterfaceProxy::GetUserdata));
			Detour::CheckValue("CLuaInterface::GetType", Hook(&GarrysMod::Lua::ILuaInterface::GetType, &CLuaInterfaceProxy::GetType));
			Detour::CheckValue("CLuaInterface::IsType", Hook(&GarrysMod::Lua::ILuaInterface::IsType, &CLuaInterfaceProxy::IsType));
		}
	}

	void DeInit()
	{
		UnHook(&GarrysMod::Lua::ILuaInterface::SetUserType);
		UnHook(&GarrysMod::Lua::ILuaInterface::GetUserdata);
		UnHook(&GarrysMod::Lua::ILuaInterface::GetType);
		UnHook(&GarrysMod::Lua::ILuaInterface::IsType);
	}

	int GetUserDataType(int iStackPos)
	{
		lua_State* L = This()->GetState();
		TValue* val = RawLua::index2adr(L, iStackPos);
		if (!val)
			return -1;

		if (!tvisudata(val))
		{
#if MODULE_EXISTS_LUAJIT
			if (tviscdata(val))
			{
				RawLua::CDataBridge* pBridge = GetCDataBridgeFromInterface(This());
				if (pBridge->pRegisteredTypes.IsBitSet(cdataV(val)->ctypeid))
				{
					auto uData = (GarrysMod::Lua::ILuaBase::UserData*)lj_obj_ptr(G(L), val);
					if (uData)
					{
						return uData->type;
					}
				}
			}
#endif

			return -1;
		}

		GCudata* luaData = udataV(val); // Very "safe" I know :3
		if (luaData->udtype >= GarrysMod::Lua::Type::UserData)
		{
			return luaData->udtype;
		}
		
		GarrysMod::Lua::ILuaBase::UserData* uData = (GarrysMod::Lua::ILuaBase::UserData*)uddata(luaData);
		if (!uData)
			return -1;

		return uData->type;
	}

	virtual void* GetUserdata(int iStackPos)
	{
		lua_State* L = This()->GetState();
		TValue* val = RawLua::index2adr(L, iStackPos);
		if (!val)
			return nullptr;

		if (!tvisudata(val))
		{
#if MODULE_EXISTS_LUAJIT
			if (tviscdata(val))
			{
				RawLua::CDataBridge* pBridge = GetCDataBridgeFromInterface(This());
				if (pBridge->pRegisteredTypes.IsBitSet(cdataV(val)->ctypeid))
					return (void*)lj_obj_ptr(G(L), val);
			}
#endif

			return nullptr;
		}

		LuaUserData* luaData = (LuaUserData*)udataV(val); // Very "safe" I know :3
		if (luaData->udtype >= GarrysMod::Lua::Type::UserData)
		{
			return nullptr; // We do NOT support this as we do NOT match the ILuaBase::UserData! (Adding that one byte would fk up alignment)
		}
		
		void* uData = uddata(luaData);
		if (!uData)
			return nullptr;

		return uData;
	}

	virtual void SetUserType(int iStackPos, void* data)
	{
		lua_State* L = This()->GetState();
		TValue* val = RawLua::index2adr(L, iStackPos);
		if (!val)
			return;

		if (!tvisudata(val))
		{
#if MODULE_EXISTS_LUAJIT
			if (tviscdata(val))
			{
				RawLua::CDataBridge* pBridge = GetCDataBridgeFromInterface(This());
				if (pBridge->pRegisteredTypes.IsBitSet(cdataV(val)->ctypeid))
				{
					GarrysMod::Lua::ILuaBase::UserData* uData = (GarrysMod::Lua::ILuaBase::UserData*)lj_obj_ptr(G(L), val);
					if (uData)
					{
						uData->data = data;
					}
				}
			}
#endif

			return;
		}

		LuaUserData* luaData = (LuaUserData*)udataV(val); // Very "safe" I know :3
		if (luaData->udtype >= GarrysMod::Lua::Type::UserData)
		{
			luaData->SetData(data); // Yes, we support this just for sake of my future stupidity when I try to use this on our stuff.
			return;
		}
		
		GarrysMod::Lua::ILuaBase::UserData* uData = (GarrysMod::Lua::ILuaBase::UserData*)uddata(luaData);
		if (!uData)
			return;

		uData->data = data;
	}

	/*
		Removes vprof.
		Why? Because in this case, lua_type is too fast and vprof creates a huge slowdown.
	*/
	virtual int GetType(int iStackPos)
	{
		int type = Util::func_lua_type(This()->GetState(), iStackPos);

		if (type == GarrysMod::Lua::Type::UserData)
		{
			type = GetUserDataType(iStackPos);
		}

		return type == -1 ? GarrysMod::Lua::Type::Nil : type;
	}

	// Fixed with the next update - https://github.com/Facepunch/garrysmod-issues/issues/6418
	virtual bool IsType(int iStackPos, int iType)
	{
		int actualType = Util::func_lua_type(This()->GetState(), iStackPos);

		if (actualType == iType)
			return true;

		if (actualType == GarrysMod::Lua::Type::UserData && iType > GarrysMod::Lua::Type::UserData)
		{
			actualType = GetUserDataType(iStackPos);
			return iType == actualType;
		}

		return false;
	}
};

static std::unordered_set<Lua::StateData*> g_pLuaStates;
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
	data->pLua = LUA;
	data->pProxy = new CLuaInterfaceProxy(LUA);
	
	CLuaInterface* pLua = (CLuaInterface*)LUA;
	LUA->ReferencePush(pLua->m_nLuaErrorReporter);
	data->SetErrorFunc();

	*reinterpret_cast<Lua::StateData**>(pathID + 24) = data;
	g_pLuaStates.insert(data);
	Msg("holylib - Created thread data %p (%s)\n", data, pathID);
}

void Lua::RemoveLuaData(GarrysMod::Lua::ILuaInterface* LUA)
{
	auto data = Lua::GetLuaData(LUA);
	if (!data)
		return;

	g_pLuaStates.erase(data);
	delete data;
	*reinterpret_cast<Lua::StateData**>((char*)LUA->GetPathID() + 24) = NULL;
	Msg("holylib - Removed thread data %p\n", data);
}

const std::unordered_set<Lua::StateData*>& Lua::GetAllLuaData()
{
	return g_pLuaStates;
}

static void LuaCheck(const CCommand& args)
{
	if (!g_Lua)
		return;

	Msg("holylib - Found data %p\n", Lua::GetLuaData(g_Lua));
}
static ConCommand luacheck("holylib_luacheck", LuaCheck, "Temp", 0);

Lua::StateData::~StateData()
{
	for (int i = 0; i < Lua::Internal::pMaxEntries; ++i)
	{
		Lua::ModuleData* pData = pModuelData[i];
		if (pData == NULL)
			continue;

		delete pData;
		pModuelData[i] = NULL;
	}

	if (pProxy)
	{
		pProxy->DeInit();
		delete pProxy;
	}
}