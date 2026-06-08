#include "lua.h"

// Due to lua_State layout having changed lua_State::base is no longer in the same place in HolyLib's version
TValue *GModLua::index2adr(lua_State *L, int idx)
{
  if (idx > 0) {
    TValue *o = L->base + (idx - 1);
    return o < L->top ? o : niltv(L);
  } else if (idx > LUA_REGISTRYINDEX) {
    return L->top + idx;
  } else if (idx == LUA_GLOBALSINDEX) {
    TValue *o = &G(L)->tmptv;
    settabV(L, o, tabref(L->env));
    return o;
  } else if (idx == LUA_REGISTRYINDEX) {
    return registry(L);
  } else {
    GCfunc *fn = curr_func(L);
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

TValue* GModLua::FastIndex2Addr(lua_State* L, int nStackPos)
{
  TValue *o = L->base + (nStackPos - 1);
  return o < L->top ? o : niltv(L);
}

TValue* GModLua::LuaTop(lua_State* L)
{
  return L->top;
}

TValue* GModLua::LuaIncrTop(lua_State* L)
{
  return L->top++;
}

TValue* GModLua::LuaBase(lua_State* L)
{
  return L->base;
}

TValue* GModLua::GlobalJITBase(lua_State * L)
{
    return tvref(G(L)->jit_base);
}