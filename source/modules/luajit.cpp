#include "module.h"
#define LUAJIT
#include "detours.h"
#include "LuaInterface.h"
#include "lua.h"
#include "lua.hpp"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CLuaJITModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "luajit"; };
	virtual int Compatibility() { return LINUX32; };
	virtual bool IsEnabledByDefault() { return false; };
};

CLuaJITModule g_pLuaJITModule;
IModule* pLuaJITModule = &g_pLuaJITModule;

#define ManualOverride(name, hook) \
Detouring::Hook detour_##name; \
Detour::Create( \
	&detour_##name, #name, \
	lua_shared_loader.GetModule(), Symbol::FromName(#name), \
	(void*)(hook), m_pID \
)

#define Override(name) ManualOverride(name, name)

static bool bOpenLibs = false;
void hook_luaL_openlibs(lua_State* L)
{
	//Msg("luaL_openlibs called!\n");
	luaL_openlibs(L);

	lua_pushcfunction(L, luaopen_ffi);
	lua_pushstring(L, LUA_FFILIBNAME);
	lua_call(L, 1, 1);
	lua_setfield(L, LUA_GLOBALSINDEX, LUA_FFILIBNAME);

	lua_getfield(L, LUA_GLOBALSINDEX, LUA_STRLIBNAME);
	lua_pushcfunction(L, luaopen_string_buffer);
	lua_pushstring(L, LUA_STRLIBNAME ".buffer");
	lua_call(L, 1, 1);
	lua_setfield(L, -2, "buffer");
	lua_pop(L, 1);

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

	lua_getfield(L, LUA_GLOBALSINDEX, "jit");
		lua_getfield(L, LUA_GLOBALSINDEX, "require");
		lua_setfield(L, -2, "require");
	lua_pop(L, 1);

	bOpenLibs = true;
}

/*
	Removes vprof.
	Why? Because in this case, lua_type is too fast and vprof creates a huge slowdown.
*/
static Detouring::Hook detour_CLuaInterface_GetType;
int hook_CLuaInterface_GetType(GarrysMod::Lua::ILuaInterface* pLua, int iStackPos)
{
	int type = lua_type(pLua->GetState(), iStackPos);

	if (type == GarrysMod::Lua::Type::UserData)
	{
		GarrysMod::Lua::ILuaBase::UserData* udata = (GarrysMod::Lua::ILuaBase::UserData*)pLua->GetUserdata(iStackPos);
		if (udata)
			type = udata->type;
	}

	return type == -1 ? GarrysMod::Lua::Type::Nil : type;
}

void CLuaJITModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
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
		lua_getfield(L, -1, "_setlocal");
		lua_setfield(L, -2, "setlocal");

		lua_getfield(L, -1, "_setupvalue");
		lua_setfield(L, -2, "setupvalue");

		lua_getfield(L, -1, "_upvalueid");
		lua_setfield(L, -2, "upvalueid");

		lua_getfield(L, -1, "_upvaluejoin");
		lua_setfield(L, -2, "upvaluejoin");
	}
	lua_pop(L, 1);

	bOpenLibs = false;
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

	Detour::Create(
		&detour_CLuaInterface_GetType, "CLuaInterface::GetType",
		lua_shared_loader.GetModule(), Symbols::CLuaInterface_GetTypeSym,
		(void*)hook_CLuaInterface_GetType, m_pID
	);
}