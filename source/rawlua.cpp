extern "C" {
#include "../lua/lua.hpp"
#include "../lua/lj_obj.h"
#include "../lua/luajit_rolling.h"
#include "../lua/lauxlib.h"
#include "../lua/lj_state.h"
#include "../lua/lj_tab.h"
#include "../lua/lj_lib.h"
#include "../lua/lj_gc.h"
}

#include "lua.h"

TValue* RawLua::index2adr(lua_State* L, int idx)
{
	if (idx > 0) {
		TValue *o = L->base + (idx - 1);
		return o < L->top ? o : niltv(L);
	} else if (idx > LUA_REGISTRYINDEX) {
		lj_checkapi(idx != 0 && -idx <= L->top - L->base,
			"bad stack slot %d", idx);
		return L->top + idx;
	} else if (idx == LUA_GLOBALSINDEX) {
		TValue *o = &G(L)->tmptv;
		settabV(L, o, tabref(L->env));
		return o;
	} else if (idx == LUA_REGISTRYINDEX) {
		return registry(L);
	} else {
		GCfunc *fn = curr_func(L);
		lj_checkapi(fn->c.gct == ~LJ_TFUNC && !isluafunc(fn),
			"calling frame is not a C function");
		if (idx == LUA_ENVIRONINDEX) {
			TValue *o = &G(L)->tmptv;
			settabV(L, o, tabref(fn->c.env));
			return o;
		} else {
			idx = LUA_GLOBALSINDEX - idx;
			return idx <= fn->c.nupvalues ? &fn->c.upvalue[idx-1] : niltv(L);
		}
	}
}

TValue* RawLua::CopyTValue(lua_State* L, TValue* o)
{
	TValue* newO = new TValue;
	copyTV(L, newO, o);

	return newO;
}

void RawLua::PushTValue(lua_State* L, TValue* o)
{
	lua_pushnil(L);
	copyTV(L, L->top - 1, o);
}

void RawLua::SetReadOnly(TValue* o, bool readOnly)
{
	GCtab* pTable = tabV(o);
	lj_tab_setreadonly(pTable, readOnly);
}

void* RawLua::GetUserDataOrFFIVar(lua_State* L, int idx, bool cDataTypes[USHRT_MAX])
{
	cTValue *o = index2adr(L, idx);
	if (tvisudata(o))
		return uddata(udataV(o));
	else if (tvislightud(o))
		return lightudV(G(L), o);
	else if (tviscdata(o))
		if (cDataTypes[cdataV(o)->ctypeid])
			return (void*)lj_obj_ptr(G(L), o); // won't mind the const void* -> void* it'll be fine
		else
			return NULL;
	else
		return NULL;
}

uint16_t RawLua::GetCDataType(lua_State* L, int idx)
{
	cTValue *o = index2adr(L, idx);
	if (tviscdata(o))
		return cdataV(o)->ctypeid;
	
	return -1;
}

int table_setreadonly(lua_State* L)
{
	GCtab *t = lj_lib_checktab(L, 1);
	int readOnly = lua_toboolean(L, 2);
  
	lj_tab_setreadonly(t, readOnly);
	return 0;
}

int table_isreadonly(lua_State* L)
{
	GCtab *t = lj_lib_checktab(L, 1);
  
	lua_pushboolean(L, lj_tab_isreadonly(t));
	return 1;
}

int func_setdebugblocked(lua_State* L)
{
	GCfunc *func = lj_lib_checkfunc(L, 1);
  
	markblockdebug(func->c);
	return 0;
}

int func_isdebugblocked(lua_State* L)
{
	GCfunc *func = lj_lib_checkfunc(L, 1);
  
	lua_pushboolean(L, isblockdebug(func->c));
	return 1;
}