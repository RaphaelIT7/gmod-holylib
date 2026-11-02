#include "lua.h"
#include "detours.h"

// Previously was in the Util namespace...
// finally moved this out of there which I wanted to do for a loooooong time
static Symbols::lua_pcall func_lua_pcall = nullptr;
static Symbols::lua_call func_lua_call = nullptr;
static Symbols::lua_gc func_lua_gc = nullptr;
static Symbols::lua_setallocf func_lua_setallocf = nullptr;
static Symbols::lua_setfenv func_lua_setfenv = nullptr;
static Symbols::lua_type func_lua_type = nullptr;
static Symbols::luaL_checklstring func_luaL_checklstring = nullptr;
static Symbols::lua_rawseti func_lua_rawseti = nullptr;
static Symbols::lua_rawgeti func_lua_rawgeti = nullptr;
static Symbols::lua_pushvfstring func_lua_pushvfstring = nullptr;
static Symbols::lua_error func_lua_error = nullptr;

// These can be NULL. Why? Because on 64x all the names are mangled making shit far more difficult...
static Symbols::lj_tab_new func_lj_tab_new = nullptr;
static Symbols::lj_gc_barrierf func_lj_gc_barrierf = nullptr;
static Symbols::lj_tab_get func_lj_tab_get = nullptr;
static bool g_bHasInternals = false;

// Why did I never make a macro like this in detours.h :bruh:
// RaphaelIT7 (ToDo): On next cleanup move this to detours.h as a more general Detour_ macro to use in lots of places
#define LoadFunction(name) \
	func_##name = (Symbols::name)Detour::GetFunction(lua_shared_loader.GetModule(), Symbols::name##Sym); \
	Detour::CheckFunction((void*)func_##name, #name)

void GModLua::Init()
{
	SourceSDK::ModuleLoader lua_shared_loader("lua_shared");

	LoadFunction(lua_pcall);
	LoadFunction(lua_call);
	LoadFunction(lua_gc);
	LoadFunction(lua_setallocf);
	LoadFunction(lua_setfenv);
	LoadFunction(lua_type);
	LoadFunction(luaL_checklstring);
	LoadFunction(lua_rawseti);
	LoadFunction(lua_rawgeti);
	LoadFunction(lua_pushvfstring);
	LoadFunction(lua_error);

	LoadFunction(lj_tab_new);
	LoadFunction(lj_gc_barrierf);
	LoadFunction(lj_tab_get);
	g_bHasInternals = func_lj_tab_new && func_lj_gc_barrierf && func_lj_tab_get;

	if (
		!func_lua_pcall || !func_lua_call || !func_lua_gc || !func_lua_setallocf || !func_lua_setfenv || !func_lua_type ||
		!func_luaL_checklstring || !func_lua_rawseti || !func_lua_rawgeti || !func_lua_pushvfstring || !func_lua_error
		)
	{
		// This is like the ONLY dependency we have on symbols that without we cannot function.
		Error(PROJECT_NAME " - core: Failed to load an important symbol which we utterly depend on.\n");
	}
}

#undef LoadFunction

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

TValue* GModLua::GlobalJITBase(lua_State* L)
{
    return tvref(G(L)->jit_base);
}

lua_State* GModLua::MainState(lua_State* L)
{
    return mainthread(G(L));
}

const char* GModLua::GetGCStrData(GCstr* str)
{
  return strdata(str);
}

size_t GModLua::GetGCStrLength(GCstr* str)
{
  return str->len;
}

int GModLua::PCall(lua_State* L, int nArgs, int nRets, int nErrorFunc)
{
  return func_lua_pcall(L, nArgs, nRets, nErrorFunc);
}

void GModLua::Call(lua_State* L, int nArgs, int nRets)
{
  func_lua_call(L, nArgs, nRets);
}

int GModLua::GC(lua_State* L, int nWhat, int nData)
{
  return func_lua_gc(L, nWhat, nData);
}

void GModLua::SetAllocF(lua_State* L, Symbols::lua_allocf pAlloc, void* pUserData)
{
  func_lua_setallocf(L, pAlloc, pUserData);
}

int GModLua::SetFEnv(lua_State* L, int nStackPos)
{
  return func_lua_setfenv(L, nStackPos);
}

int GModLua::Type(lua_State* L, int nStackPos)
{
  return func_lua_type(L, nStackPos);
}

const char* GModLua::CheckLString(lua_State* L, int nStackPos, size_t* nOutLength)
{
  return func_luaL_checklstring(L, nStackPos, nOutLength);
}

void GModLua::RawSetI(lua_State* L, int nStackPos, int nValue)
{
  func_lua_rawseti(L, nStackPos, nValue);
}

void GModLua::RawGetI(lua_State* L, int nStackPos, int nValue)
{
  func_lua_rawgeti(L, nStackPos, nValue);
}

const char* GModLua::PushVFString(lua_State* L, const char* pFormat, va_list pArgs)
{
  return func_lua_pushvfstring(L, pFormat, pArgs);
}

int GModLua::LuaError(lua_State* L)
{
  return func_lua_error(L);
}

bool GModLua::HasInternals()
{
  return g_bHasInternals;
}

GCtab* GModLua::TabNew(lua_State* L, uint32_t nArray, uint32_t nRec)
{
  return func_lj_tab_new(L, nArray, nRec);
}

void GModLua::GCBarrierF(global_State* g, void* pObject, void* pValue)
{
  func_lj_gc_barrierf(g, pObject, pValue);
}

void* GModLua::TabGet(lua_State* L, void* pTable, void* pKey)
{
  return func_lj_tab_get(L, pTable, pKey);
}
