extern "C" {
#include "../luajit/src/lua.hpp"
#include "../luajit/src/lj_obj.h"
#include "../luajit/src/luajit_rolling.h"
#include "../luajit/src/lauxlib.h"
#include "../luajit/src/lj_state.h"
#include "../luajit/src/lj_tab.h"
#include "../luajit/src/lj_lib.h"
#include "../luajit/src/lj_gc.h"
#include "../luajit/src/lj_ctype.h"
#include "../luajit/src/lj_cdata.h"
#include "../luajit/src/lj_err.h"
#include "../luajit/src/lj_cparse.h"
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

void* RawLua::GetUserDataOrFFIVar(lua_State* L, int idx, CDataBridge& cDataBridge)
{
	cTValue *o = index2adr(L, idx);
	if (tvisudata(o))
		return uddata(udataV(o));
	else if (tvislightud(o))
		return lightudV(G(L), o);
	else if (tviscdata(o))
		if (cDataBridge.pRegisteredTypes.IsBitSet(cdataV(o)->ctypeid))
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

CTypeID ffi_checkctype(lua_State *L, CTState *cts, TValue* param, const char* pStr)
{
	CPState cp;
	int errcode;
	cp.L = L;
	cp.cts = cts;
	cp.srcname = pStr;
	cp.p = pStr;
	cp.param = param;
	cp.mode = CPARSE_MODE_ABSTRACT|CPARSE_MODE_NOIMPLICIT;
	errcode = lj_cparse(&cp);
	if (errcode)
		lj_err_throw(L, errcode);  /* Propagate errors. */
	
	return cp.val.id;
}

uint32_t RawLua::GetCTypeFromName(lua_State* L, const char* pName, CDataBridge* cDataBridge)
{
	CTState* cts = ctype_cts(L);
	CTypeID pType = ffi_checkctype(L, cts, NULL, pName);
	CType* ct = ctype_raw(cts, pType);
	GCtab* t = cts->miscmap;

	TValue* tv = lj_tab_setinth(L, t, -(int32_t)ctype_typeid(cts, ct));
	if (!tvisnil(tv))
		lj_err_caller(L, LJ_ERR_PROTMT);

	GCtab* mt = lj_lib_checktab(L, 2);
	settabV(L, tv, mt);
	lj_gc_anybarriert(L, t);
	lj_gc_check(L);

	if (!cDataBridge->nHolyLibUserDataGC)
	{
		cDataBridge->nHolyLibUserDataGC = new TValue;

		copyTV(L, cDataBridge->nHolyLibUserDataGC, index2adr(L, 3));

		lua_pushvalue(L, 3);
		cDataBridge->nHolyLibUserDataGCFuncReference = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	return pType;
}

extern RawLua::CDataBridge* GetCDataBridgeFromInterface(GarrysMod::Lua::ILuaInterface* pLua); // From the LuaJIT module
void* RawLua::AllocateCDataOrUserData(GarrysMod::Lua::ILuaInterface* pLua, int nMetaID, int nSize)
{
#if 0 // Disabled since stuff decides to behave funny...
	CDataBridge* cDataBridge = GetCDataBridgeFromInterface(pLua);

	if (cDataBridge && cDataBridge->nHolyLibUserDataTypeID != 0)
	{
		lua_State* L = pLua->GetState();

		lj_gc_check(L);
		GCcdata* cData = lj_cdata_new_(L, cDataBridge->nHolyLibUserDataTypeID, sizeof(LuaUserData));
		setcdataV(L, L->top, cData);
		incr_top(L);

		// Now also set our GC func
		//if (cDataBridge->nHolyLibUserDataGC)
		//	lj_cdata_setfin(L, cData, gcval(cDataBridge->nHolyLibUserDataGC), itype(cDataBridge->nHolyLibUserDataGC));

		// Msg("cdata pointer: %p - %p %u\n", cdataptr(cData), cData, cDataBridge->nHolyLibUserDataTypeID);
		return cdataptr(cData);
	} else
#endif
	{
		return pLua->NewUserdata(nSize);
	}
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