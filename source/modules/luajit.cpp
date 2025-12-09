#include "module.h"
#define LUAJIT
#define DISABLE_GMODJIT
#include "detours.h"
#include <detouring/classproxy.hpp>
#include "LuaInterface.h"
#include "lua.h"
#include "Bootil/Bootil.h"
#include "bitvec.h"
#include "scripts/VectorFFI.h"
#include "scripts/AngleFFI.h"
#include "scripts/HolyLibUserDataFFI.h"
#include "scripts/VoiceDataFFI.h"

extern "C"
{
	#include "../luajit/src/lj_gc.h"
	#include "../luajit/src/lj_tab.h"
}

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CLuaJITModule : public IModule
{
public:
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void PostLuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void InitDetour(bool bPreServer) override;
	const char* Name() override { return "luajit"; };
	int Compatibility() override { return LINUX32 | LINUX64; };
	bool IsEnabledByDefault() override { return false; };
	void OnConfigLoad(Bootil::Data::Tree& pConfig) override
	{
		m_bAllowFFI = pConfig.EnsureChildVar<bool>("enableFFI", m_bAllowFFI);
		m_bKeepRemovedDebugFunctions = pConfig.EnsureChildVar<bool>("keepRemovedDebugFunctions", m_bKeepRemovedDebugFunctions);
	};

public:
	bool m_bAllowFFI = false;
	bool m_bKeepRemovedDebugFunctions = false;
	bool m_bIsEnabled = false;
};

CLuaJITModule g_pLuaJITModule;
IModule* pLuaJITModule = &g_pLuaJITModule;

//class CLuaInterfaceProxy;
class LuaJITModuleData : public Lua::ModuleData
{
public:
	//std::unique_ptr<CLuaInterfaceProxy> pLuaInterfaceProxy;
};

LUA_GetModuleData(LuaJITModuleData, g_pLuaJITModule, LuaJIT)

#define ManualOverride(name, hook) \
static Detouring::Hook detour_##name; \
Detour::Create( \
	&detour_##name, #name, \
	lua_shared_loader.GetModule(), Symbol::FromName(#name), \
	(void*)(hook), m_pID \
)

#define Override(name) ManualOverride(name, name)

static thread_local int overrideFFIReference = -1;
static int getffi(lua_State* L)
{
	if (!g_pLuaJITModule.m_bAllowFFI && overrideFFIReference == -1)
		return 0;

	if (overrideFFIReference != -1)
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, overrideFFIReference);
	} else {
		lua_getfield(L, LUA_GLOBALSINDEX, "ffi");
	}
	return 1;
}

LUA_FUNCTION_STATIC(markFFITypeAsValidUserData)
{
	if (!g_pLuaJITModule.m_bAllowFFI && overrideFFIReference == -1)
		return 0;

	Lua::StateData* pData = Lua::GetLuaData(LUA);
	if (!pData)
		return 0;

	unsigned char nMetaID = (unsigned char)LUA->CheckNumber(1);
	uint16_t cTypeID = RawLua::GetCDataType(LUA->GetState(), 2);

	Lua::GetLuaData(LUA)->GetCDataBridge().RegisterType(nMetaID, cTypeID);
	return 0;
}

LUA_FUNCTION_STATIC(registerCreateHolyLibCDataFunc)
{
	if (!g_pLuaJITModule.m_bAllowFFI && overrideFFIReference == -1) // ONLY allowed inside the internal scripts
		return 0;

	LUA->CheckType(2, GarrysMod::Lua::Type::Table);
	LUA->CheckType(3, GarrysMod::Lua::Type::Function);

	Lua::StateData* pData = Lua::GetLuaData(LUA);
	if (!pData)
		return 0;

	uint32_t nTypeID = RawLua::GetCTypeFromName(LUA->GetState(), LUA->CheckString(1), &pData->GetCDataBridge());
	pData->GetCDataBridge().RegisterHolyLibUserData(nTypeID);
	return 0;
}

LUA_FUNCTION_STATIC(getMetaTableByID)
{
	if (!g_pLuaJITModule.m_bAllowFFI && overrideFFIReference == -1)
		return 0;

	return LUA->PushMetaTable((int)LUA->CheckNumber(1)) ? 1 : 0;
}

static bool bOpenLibs = false;
static void hook_luaL_openlibs(lua_State* L)
{
	luaL_openlibs(L);

	if (g_pLuaJITModule.m_bAllowFFI)
	{
		lua_pushcfunction(L, luaopen_ffi);
		lua_pushstring(L, LUA_FFILIBNAME);
		lua_call(L, 1, 1);
		lua_setfield(L, LUA_GLOBALSINDEX, LUA_FFILIBNAME);
	}

	lua_getfield(L, LUA_GLOBALSINDEX, LUA_STRLIBNAME);
	lua_pushcfunction(L, luaopen_string_buffer);
	lua_pushstring(L, LUA_STRLIBNAME ".buffer");
	lua_call(L, 1, 1);
	lua_setfield(L, -2, "buffer");
	lua_pop(L, 1);

	lua_pushboolean(L, true);
	lua_setfield(L, LUA_REGISTRYINDEX, "HOLYLIB_LUAJIT");

	if (g_pLuaJITModule.m_bKeepRemovedDebugFunctions)
	{
		/*
		 * Save funny debug functions.
		 */
		lua_getfield(L, LUA_GLOBALSINDEX, "debug");
		if (lua_istable(L, -1))
		{
			lua_getfield(L, -1, "setlocal");
			lua_setfield(L, -2, "_setlocal");

			lua_getfield(L, -1, "setupvalue");
			lua_setfield(L, -2, "_setupvalue");

			lua_getfield(L, -1, "upvalueid");
			lua_setfield(L, -2, "_upvalueid");

			lua_getfield(L, -1, "upvaluejoin");
			lua_setfield(L, -2, "_upvaluejoin");
		}
		lua_pop(L, 1);
	}

	lua_getfield(L, LUA_GLOBALSINDEX, "jit");
		if (g_pLuaJITModule.m_bAllowFFI) // if we allow ffi, then we will also keep the original require so scripts can use jit.require("ffi")
		{
			lua_getfield(L, LUA_GLOBALSINDEX, "require");
			lua_setfield(L, -2, "require");
		}
		lua_pushcfunction(L, getffi);
		lua_setfield(L, -2, "getffi");

		lua_pushcfunction(L, markFFITypeAsValidUserData);
		lua_setfield(L, -2, "markFFITypeAsValidUserData");

		lua_pushcfunction(L, registerCreateHolyLibCDataFunc);
		lua_setfield(L, -2, "registerCreateHolyLibCDataFunc");

		lua_pushcfunction(L, getMetaTableByID);
		lua_setfield(L, -2, "getMetaTableByID");

		lua_pushcfunction(L, luaopen_jit_profile);
		lua_call(L, 0, 1);
		lua_setfield(L, -2, "profile");
	lua_pop(L, 1);

	bOpenLibs = true;
}

/*
ToDo: Redo this entire class and move all LuaJIT specific stuff from lua.cpp into here to seperate shit from CORE code!
class CLuaInterfaceProxy : public Detouring::ClassProxy<GarrysMod::Lua::ILuaInterface, CLuaInterfaceProxy> {
public:
	CLuaInterfaceProxy(GarrysMod::Lua::ILuaInterface* env) {
		if (Detour::CheckValue("initialize", "CLuaInterfaceProxy", Initialize(env)))
		{
			Detour::CheckValue("CLuaInterface::GetUserdata", Hook(&GarrysMod::Lua::ILuaInterface::GetUserdata, &CLuaInterfaceProxy::GetUserdata));
		}
	}

	void DeInit()
	{
		UnHook(&GarrysMod::Lua::ILuaInterface::GetUserdata);
	}

	GarrysMod::Lua::ILuaBase::UserData* GetUserdata(int iStackPos)
	{
		bool bFutureVar;
		return (GarrysMod::Lua::ILuaBase::UserData*)RawLua::GetUserDataOrFFIVar(This()->GetState(), iStackPos, GetLuaJITLuaData(This())->pBridge, &bFutureVar);
	}
};*/

void CLuaJITModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	//GetLuaJITLuaData(pLua)->pLuaInterfaceProxy->DeInit();
}

extern int table_setreadonly(lua_State* L);
extern int table_isreadonly(lua_State* L);
extern int func_setdebugblocked(lua_State* L);
extern int func_isdebugblocked(lua_State* L);
void CLuaJITModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit || pLua != g_Lua) // Don't init for non-gmod states
		return;

	if (!bOpenLibs)
	{
		Error(PROJECT_NAME ": LuaJIT didn't work for some magical reason!\n");
		// This is better than a random crash.
		// ToDo: Actually fix the bug
		// NOTE: We error because further up the GetType function will else cause a crash so if we fail its better to error out than to continue & crash
		return;
	}

	lua_State* L = pLua->GetState();
	lua_getfield(L, LUA_GLOBALSINDEX, "debug");
	if (lua_istable(L, -1))
	{
		if (g_pLuaJITModule.m_bKeepRemovedDebugFunctions)
		{
			lua_getfield(L, -1, "_setlocal");
			lua_setfield(L, -2, "setlocal");

			lua_getfield(L, -1, "_setupvalue");
			lua_setfield(L, -2, "setupvalue");

			lua_getfield(L, -1, "_upvalueid");
			lua_setfield(L, -2, "upvalueid");

			lua_getfield(L, -1, "_upvaluejoin");
			lua_setfield(L, -2, "upvaluejoin");
		}

		lua_pushcfunction(L, table_setreadonly);
		lua_setfield(L, -2, "setreadonly");

		lua_pushcfunction(L, table_isreadonly);
		lua_setfield(L, -2, "isreadonly");

		lua_pushcfunction(L, func_setdebugblocked);
		lua_setfield(L, -2, "setblocked");

		lua_pushcfunction(L, func_isdebugblocked);
		lua_setfield(L, -2, "isblocked");
	}
	lua_pop(L, 1);

	LuaJITModuleData* pData = new LuaJITModuleData;
	//pData->pLuaInterfaceProxy = std::make_unique<CLuaInterfaceProxy>(pLua);
	Lua::GetLuaData(pLua)->SetModuleData(m_pID, pData);

	bOpenLibs = false;
}

// We load the FFI scripts after LuaInit allowing them to properly work AFTER all new Lua types were registered.
void CLuaJITModule::PostLuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit || pLua != g_Lua) // Don't init for non-gmod states
		return;

	lua_State* L = pLua->GetState();
	if (!g_pLuaJITModule.m_bAllowFFI)
	{
		lua_pushcfunction(L, luaopen_ffi);
		lua_pushstring(L, LUA_FFILIBNAME);
		lua_call(L, 1, 1);
		overrideFFIReference = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	if (!pLua->RunStringEx("HolyLib:HolyLibUserDataFFI.lua", "", luaHolyLibUserDataFFI, true, true, true, true))
	{
		Warning(PROJECT_NAME " - luajit: Failed to load HolyLibUserData script!\n");
	}

	if (!pLua->RunStringEx("HolyLib:VectorFFI.lua", "", luaVectorFFI, true, true, true, true))
	{
		Warning(PROJECT_NAME " - luajit: Failed to load VectorFFI script!\n");
	}

	if (!pLua->RunStringEx("HolyLib:AngleFFI.lua", "", luaAngleFFI, true, true, true, true))
	{
		Warning(PROJECT_NAME " - luajit: Failed to load AngleFFI script!\n");
	}

	if (!pLua->RunStringEx("HolyLib:VoiceDataFFI.lua", "", luaVoiceDataFFI, true, true, true, true))
	{
		Warning(PROJECT_NAME " - luajit: Failed to load VoiceDataFFI script!\n");
	}

	pLua->PushNil();
	pLua->SetField(GarrysMod::Lua::INDEX_GLOBAL, "__HOLYLIB_FFI"); // A table that the script could have used to share data between them.

	// Remove FFI again.
	if (overrideFFIReference != -1)
	{
		luaL_unref(L, LUA_REGISTRYINDEX, overrideFFIReference);
		overrideFFIReference = -1;
	}
}

void CLuaJITModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::ModuleLoader lua_shared_loader("lua_shared");
	//Override(luaJIT_version_2_0_4);
	Override(luaJIT_setmode);
	Override(luaL_addlstring);
	Override(luaL_addstring);
	Override(luaL_addvalue);
	Override(luaL_argerror);
	Override(luaL_buffinit);
	Override(luaL_callmeta);
	Override(luaL_checkany);
	Override(luaL_checkinteger);
	Override(luaL_checklstring);
	Override(luaL_checknumber);
	Override(luaL_checkoption);
	Override(luaL_checkstack);
	Override(luaL_checktype);
	Override(luaL_checkudata);
	Override(luaL_error);
	Override(luaL_execresult);
	Override(luaL_fileresult);
	Override(luaL_findtable);
	Override(luaL_getmetafield);
	Override(luaL_gsub);
	Override(luaL_loadbuffer);
	Override(luaL_loadbufferx);
	Override(luaL_loadfile);
	Override(luaL_loadfilex);
	Override(luaL_loadstring);
	Override(luaL_newmetatable);
	Override(luaL_newmetatable_type);
	Override(luaL_newstate);
	Override(luaL_openlib);
	ManualOverride(luaL_openlibs, hook_luaL_openlibs); // Gmod calls luaL_openlibs
	Override(luaL_optinteger);
	Override(luaL_optlstring);
	Override(luaL_optnumber);
	Override(luaL_prepbuffer);
	Override(luaL_pushresult);
	Override(luaL_ref);
	Override(luaL_register);
	Override(luaL_traceback);
	Override(luaL_typerror);
	Override(luaL_unref);
	Override(luaL_where);
	Override(lua_atpanic);
	Override(lua_call);
	Override(lua_checkstack);
	Override(lua_close);
	Override(lua_concat);
	Override(lua_cpcall);
	Override(lua_createtable);
	Override(lua_dump);
	Override(lua_equal);
	Override(lua_error);
	Override(lua_gc);
	Override(lua_getallocf);
	Override(lua_getfenv);
	Override(lua_getfield);
	Override(lua_gethook);
	Override(lua_gethookcount);
	Override(lua_gethookmask);
	Override(lua_getinfo);
	Override(lua_getlocal);
	Override(lua_getmetatable);
	Override(lua_getstack);
	Override(lua_gettable);
	Override(lua_gettop);
	Override(lua_getupvalue);
	Override(lua_insert);
	Override(lua_iscfunction);
	Override(lua_isnumber);
	Override(lua_isstring);
	Override(lua_isuserdata);
	Override(lua_lessthan);
	Override(lua_load);
	Override(lua_loadx);
	Override(lua_newstate);
	Override(lua_newthread);
	Override(lua_newuserdata);
	Override(lua_next);
	Override(lua_objlen);
	Override(lua_pcall);
	Override(lua_pushboolean);
	Override(lua_pushcclosure);
	Override(lua_pushfstring);
	Override(lua_pushinteger);
	Override(lua_pushlightuserdata);
	Override(lua_pushlstring);
	Override(lua_pushnil);
	Override(lua_pushnumber);
	Override(lua_pushstring);
	Override(lua_pushthread);
	Override(lua_pushvalue);
	Override(lua_pushvfstring);
	Override(lua_rawequal);
	Override(lua_rawget);
	Override(lua_rawgeti);
	Override(lua_rawset);
	Override(lua_rawseti);
	Override(lua_remove);
	Override(lua_replace);
	Override(lua_resume);
	Override(lua_setallocf);
	Override(lua_setfenv);
	Override(lua_setfield);
	Override(lua_sethook);
	Override(lua_setlocal);
	Override(lua_setmetatable);
	Override(lua_settable);
	Override(lua_settop);
	Override(lua_setupvalue);
	Override(lua_status);
	Override(lua_toboolean);
	Override(lua_tocfunction);
	Override(lua_tointeger);
	Override(lua_tolstring);
	Override(lua_tonumber);
	Override(lua_topointer);
	Override(lua_tothread);
	Override(lua_touserdata);
	Override(lua_type);
	Override(lua_typename);
	Override(lua_upvalueid);
	Override(lua_upvaluejoin);
	Override(lua_xmove);
	Override(lua_yield);
	Override(luaopen_base);
	Override(luaopen_bit);
	Override(luaopen_debug);
	Override(luaopen_jit);
	Override(luaopen_math);
	Override(luaopen_os);
	Override(luaopen_package);
	Override(luaopen_string);
	Override(luaopen_table);

	Util::func_lua_rawgeti = &lua_rawgeti;
	Util::func_lua_rawseti = &lua_rawseti;
	Util::func_lj_tab_new = &lj_tab_new;
	Util::func_lua_setfenv = &lua_setfenv;
	Util::func_lua_touserdata = &lua_touserdata;
	Util::func_lua_type = &lua_type;
	Util::func_luaL_checklstring = &luaL_checklstring;
	Util::func_lua_pcall = &lua_pcall;
	Util::func_lua_insert = &lua_insert;
	Util::func_lua_toboolean = &lua_toboolean;
	Util::func_lj_gc_barrierf = (Symbols::lj_gc_barrierf)&lj_gc_barrierf;
	Util::func_lj_tab_get = (Symbols::lj_tab_get)&lj_tab_get;

	m_bIsEnabled = true;
}