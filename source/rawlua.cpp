extern "C" {
#include "../luajit/src/lua.hpp"
#include "../luajit/src/lj_obj.h"
#include "../luajit/src/luajit_rolling.h"
#include "../luajit/src/lauxlib.h"
#include "../luajit/src/lj_state.h"
#include "../luajit/src/lj_tab.h"
#include "../luajit/src/lj_lib.h"
#include "../luajit/src/lj_gc.h"
}

#include "lua.h"

TValue* RawLua::index2adr(lua_State* L, int idx)
{
	return lua_index2adr(L, idx); // We exposed it in LuaJIT so that we don't have to implement it ourself again.
}

TValue* RawLua::CopyTValue(lua_State* L, TValue* o)
{
	TValue* newO = new TValue;
	copyTV(L, newO, o);

	return newO;
}

void RawLua::DestroyTValue(TValue* o)
{
	delete o;
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

void* RawLua::GetUserDataOrFFIVar(lua_State* L, int idx, CBitVec<USHRT_MAX>& cDataTypes)
{
	cTValue *o = index2adr(L, idx);
	if (tvisudata(o))
		return uddata(udataV(o));
	else if (tvislightud(o))
		return lightudV(G(L), o);
	else if (tviscdata(o))
		if (cDataTypes.IsBitSet(cdataV(o)->ctypeid))
			return (void*)lj_obj_ptr(G(L), o); // won't mind the const void* -> void* it'll be fine
		else {
			Msg("cdata type %i is NOT registered!\n", cdataV(o)->ctypeid);
			return NULL;
		}
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