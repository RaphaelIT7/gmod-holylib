#include "../lua/lua.hpp"
#include "../lua/lj_obj.h"
#include "../lua/luajit_rolling.h"
#include "../lua/lauxlib.h"
#include "../lua/lj_state.h"

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
