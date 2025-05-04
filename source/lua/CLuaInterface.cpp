#include "CLuaInterface.h"
#include "lua.hpp"
#include "../lua/lj_obj.h"
#include "../lua/luajit_rolling.h"
#include "../lua/lauxlib.h"
#include "../lua/lj_obj.h"
#include <regex>
#include "color.h"
#include <filesystem.h>
#include "GarrysMod/Lua/LuaShared.h"
#include "lua/PooledStrings.h"
#include "util.h"

#if defined(__clang__) || defined(__GNUC__)
#define POINT_UNREACHABLE   __builtin_unreachable()
#else
#define POINT_UNREACHABLE   __assume(false)
#endif

static ConVar lua_debugmode("lua_debugmode_interface", "1", 0);
inline void CLuaInterface_DebugPrint(int level, const char* fmt, ...)
{
	if (lua_debugmode.GetInt() < level)
		return;

	va_list args;
	va_list argsCopy;
	va_start(args, fmt);
	va_copy(argsCopy, args);

	int size = vsnprintf(NULL, 0, fmt, args);
	if (size < 0) {
		va_end(args);
		va_end(argsCopy);
		return;
	}

	char* buffer = new char[size + 1];
	vsnprintf(buffer, size + 1, fmt, argsCopy);

	Msg("%s", buffer);

	delete[] buffer;
	va_end(args);
	va_end(argsCopy);
}

#if !HOLYLIB_BUILD_RELEASE
#define LuaDebugPrint CLuaInterface_DebugPrint
#else
#define LuaDebugPrint(...)
#endif

// =================================
// First gmod function
// =================================
std::string g_LastError;
std::vector<lua_Debug*> stackErrors;
GarrysMod::Lua::ILuaGameCallback::CLuaError* ReadStackIntoError(lua_State* L, bool bSimple)
{
	// VPROF ReadStackIntoError GLua

	int level = 0;
	lua_Debug ar;
	GarrysMod::Lua::ILuaGameCallback::CLuaError* lua_error = new GarrysMod::Lua::ILuaGameCallback::CLuaError;
	while (lua_getstack(L, level, &ar)) {
		lua_getinfo(L, "nSl", &ar);

		GarrysMod::Lua::ILuaGameCallback::CLuaError::StackEntry& entry = lua_error->stack.emplace_back();
		entry.source = ar.source ? ar.source : "unknown";
		entry.function = ar.name ? ar.name : "unknown";
		entry.line = ar.currentline;

		++level;
	}

	CLuaInterface* LUA = (CLuaInterface*)L->luabase;
	lua_error->side = LUA->IsClient() ? "client" : (LUA->IsMenu() ? "menu" : "server");

	if ( bSimple )
		return lua_error;

	const char* str = lua_tolstring(L, -1, NULL);
	if (str != NULL) // Setting a std::string to NULL causes a crash
	{
		lua_error->message = str;
	}

	return lua_error;
}

int AdvancedLuaErrorReporter(lua_State *L) 
{
	// VPROF AdvancedLuaErrorReporter GLua

	if (lua_isstring(L, 0)) {
		const char* str = lua_tostring(L, 0);

		g_LastError.assign(str);

		ReadStackIntoError(L, false);

		lua_pushstring(L, g_LastError.c_str());
	}

	return 0;
}

GarrysMod::Lua::ILuaInterface* Lua::CreateLuaInterface(bool bIsServer)
{
	LuaDebugPrint(1, "CreateLuaInterface\n");
	return new CLuaInterface;
}

void Lua::CloseLuaInterface(GarrysMod::Lua::ILuaInterface* LuaInterface)
{
	LuaDebugPrint(1, "CloseLuaInterface\n");
	//LuaInterface->Shutdown(); // This was moved to lua.cpp in holylib
	delete LuaInterface;
}

// =================================
// ILuaBase / CBaseLuaInterface implementation
// =================================

int CLuaInterface::Top()
{
	LuaDebugPrint(3, "CLuaInterface::Top\n");
	return lua_gettop(state);
}

void CLuaInterface::Push(int iStackPos)
{
	LuaDebugPrint(3, "CLuaInterface::Push %i\n", iStackPos);
	lua_pushvalue(state, iStackPos);
}

void CLuaInterface::Pop(int iAmt)
{
	LuaDebugPrint(3, "CLuaInterface::Pop %i\n", iAmt);
	lua_pop(state, iAmt);
#if !HOLYLIB_BUILD_RELEASE
	if (lua_gettop(state) < 0)
	{
#ifdef WIN32
		__debugbreak();
#endif
		::Error("CLuaInterface::Pop -> That was too much :<");
	}
#endif
}

void CLuaInterface::GetTable(int iStackPos)
{
	LuaDebugPrint(3, "CLuaInterface::GetTable %i\n", iStackPos);
	lua_gettable(state, iStackPos);
}

void CLuaInterface::GetField(int iStackPos, const char* strName)
{
	LuaDebugPrint(3, "CLuaInterface::GetField %i, %s\n", iStackPos, strName);
	lua_getfield(state, iStackPos, strName);
}

void CLuaInterface::SetField(int iStackPos, const char* strName)
{
	LuaDebugPrint(3, "CLuaInterface::SetField %i, %s\n", iStackPos, strName);
	lua_setfield(state, iStackPos, strName);
}

void CLuaInterface::CreateTable()
{
	LuaDebugPrint(3, "CLuaInterface::CreateTable\n");
	lua_createtable(state, 0, 0);
}

void CLuaInterface::SetTable(int iStackPos)
{
	LuaDebugPrint(3, "CLuaInterface::SetTable %i\n", iStackPos);
	lua_settable(state, iStackPos);
}

void CLuaInterface::SetMetaTable(int iStackPos)
{
	LuaDebugPrint(3, "CLuaInterface::SetMetaTable %i\n", iStackPos);
	lua_setmetatable(state, iStackPos);
}

bool CLuaInterface::GetMetaTable(int iStackPos)
{
	LuaDebugPrint(3, "CLuaInterface::GetMetaTable %i\n", iStackPos);
	return lua_getmetatable(state, iStackPos);
}

void CLuaInterface::Call(int iArgs, int iResults)
{
	LuaDebugPrint(3, "CLuaInterface::Call %i %i\n", iArgs, iResults);
	lua_call(state, iArgs, iResults);
}

int CLuaInterface::PCall(int iArgs, int iResults, int iErrorFunc)
{
	LuaDebugPrint(2, "CLuaInterface::PCall %i %i %i\n", iArgs, iResults, iErrorFunc);
	//return lua_pcall(state, iArgs, iResults, iErrorFunc);
	int ret = lua_pcall(state, iArgs, iResults, iErrorFunc);
#if !HOLYLIB_BUILD_RELEASE
	if ( ret != 0 )
		LuaDebugPrint(2, "CLuaInterface::PCall went boom. Oh nooo\n");
#endif

	return ret;
}

int CLuaInterface::Equal(int iA, int iB)
{
	LuaDebugPrint(3, "CLuaInterface::Equal %i %i\n", iA, iB);
	return lua_equal(state, iA, iB);
}

int CLuaInterface::RawEqual(int iA, int iB)
{
	LuaDebugPrint(3, "CLuaInterface::RawEqual %i %i\n", iA, iB);
	return lua_rawequal(state, iA, iB);
}

void CLuaInterface::Insert(int iStackPos)
{
	LuaDebugPrint(3, "CLuaInterface::Insert %i\n", iStackPos);
	lua_insert(state, iStackPos);
}

void CLuaInterface::Remove(int iStackPos)
{
	LuaDebugPrint(3, "CLuaInterface::Remove %i\n", iStackPos);
	lua_remove(state, iStackPos);
}

int CLuaInterface::Next(int iStackPos)
{
	LuaDebugPrint(3, "CLuaInterface::Next %i\n", iStackPos);
	return lua_next(state, iStackPos);
}

void* CLuaInterface::NewUserdata(unsigned int iSize)
{
	LuaDebugPrint(3, "CLuaInterface::NewUserdata\n");
	return lua_newuserdata(state, iSize);
}

[[noreturn]] void CLuaInterface::ThrowError(const char* strError)
{
	LuaDebugPrint(2, "CLuaInterface::ThrowError %s\n", strError);
	luaL_error(state, "%s", strError);
	POINT_UNREACHABLE;
}

void CLuaInterface::CheckType(int iStackPos, int iType)
{
	LuaDebugPrint(3, "CLuaInterface::CheckType %i %i\n", iStackPos, iType);
	int actualType = GetType(iStackPos);
	if (actualType != iType) {
		const char* expectedType = GetTypeName(iType);
		const char* actualTypeName = GetTypeName(actualType);
		//const char* errorMessage = lua_pushfstring(state, "Expected type %s at stack position %d, but got type %s.", expectedType, iStackPos, actualTypeName);
		lua_error(state);
	}
}

[[noreturn]] void CLuaInterface::ArgError(int iArgNum, const char* strMessage)
{
	LuaDebugPrint(2, "CLuaInterface::ArgError\n");
	luaL_argerror(state, iArgNum, strMessage);
	POINT_UNREACHABLE;
}

void CLuaInterface::RawGet(int iStackPos)
{
	LuaDebugPrint(3, "CLuaInterface::RawGet\n");
	lua_rawget(state, iStackPos);
}

void CLuaInterface::RawSet(int iStackPos)
{
	LuaDebugPrint(3, "CLuaInterface::RawSet\n");
	lua_rawset(state, iStackPos);
}

const char* CLuaInterface::GetString(int iStackPos, unsigned int* iOutLen)
{
	LuaDebugPrint(4, "CLuaInterface::GetString\n");
	return lua_tolstring(state, iStackPos, reinterpret_cast<std::size_t*>(iOutLen));
}

double CLuaInterface::GetNumber(int iStackPos)
{
	LuaDebugPrint(4, "CLuaInterface::GetNumber\n");
	return lua_tonumber(state, iStackPos);
}

bool CLuaInterface::GetBool(int iStackPos)
{
	LuaDebugPrint(4, "CLuaInterface::GetBool\n");
	return lua_toboolean(state, iStackPos);
}

GarrysMod::Lua::CFunc CLuaInterface::GetCFunction(int iStackPos)
{
	LuaDebugPrint(4, "CLuaInterface::GetCFunction\n");
	return lua_tocfunction(state, iStackPos);
}

void* CLuaInterface::GetUserdata(int iStackPos)
{
	LuaDebugPrint(4, "CLuaInterface::GetUserdata %i\n", iStackPos);
	return lua_touserdata(state, iStackPos);
}

void CLuaInterface::PushNil()
{
	LuaDebugPrint(4, "CLuaInterface::PushNil\n");
	lua_pushnil(state);
}

void CLuaInterface::PushString(const char* val, unsigned int iLen)
{
	LuaDebugPrint(4, "CLuaInterface::PushString %s\n", val);
	if (iLen > 0) {
		lua_pushlstring(state, val, iLen);
	} else {
		lua_pushstring(state, val);
	}
}

void CLuaInterface::PushNumber(double val)
{
	LuaDebugPrint(4, "CLuaInterface::PushNumber\n");
	lua_pushnumber(state, val);
}

void CLuaInterface::PushBool(bool val)
{
	LuaDebugPrint(4, "CLuaInterface::PushBool\n");
	lua_pushboolean(state, val);
}

void CLuaInterface::PushCFunction(GarrysMod::Lua::CFunc val)
{
	LuaDebugPrint(4, "CLuaInterface::PushCFunction\n");
	lua_pushcclosure(state, val, 0);
}

void CLuaInterface::PushCClosure(GarrysMod::Lua::CFunc val, int iVars)
{
	LuaDebugPrint(4, "CLuaInterface::PushCClosure\n");
	lua_pushcclosure(state, val, iVars);
}

void CLuaInterface::PushUserdata(void* val)
{
	LuaDebugPrint(4, "CLuaInterface::PushUserdata\n");
	lua_pushlightuserdata(state, val);
}

int CLuaInterface::ReferenceCreate()
{
	LuaDebugPrint(4, "CLuaInterface::ReferenceCreate\n");

	return luaL_ref(state, LUA_REGISTRYINDEX);
}

void CLuaInterface::ReferenceFree(int i)
{
	LuaDebugPrint(4, "CLuaInterface::ReferenceFree\n");
	luaL_unref(state, LUA_REGISTRYINDEX, i);
}

void CLuaInterface::ReferencePush(int i)
{
	LuaDebugPrint(4, "CLuaInterface::ReferencePush\n");
	lua_rawgeti(state, LUA_REGISTRYINDEX, i);
	//LuaDebugPrint(4, "CLuaInterface::ReferencePush pushed a %i\n", GetType(-1));
}

void CLuaInterface::PushSpecial(int iType)
{
	LuaDebugPrint(3, "CLuaInterface::PushSpecial\n");
	switch (iType) {
		case GarrysMod::Lua::SPECIAL_GLOB:
			lua_pushvalue(state, LUA_GLOBALSINDEX);
			break;
		case GarrysMod::Lua::SPECIAL_ENV:
			lua_pushvalue(state, LUA_ENVIRONINDEX);
			break;
		case GarrysMod::Lua::SPECIAL_REG:
			lua_pushvalue(state, LUA_REGISTRYINDEX);
			break;
		default:
			lua_pushnil(state);
			break;
	}
}

bool CLuaInterface::IsType(int iStackPos, int iType)
{
	LuaDebugPrint(4, "CLuaInterface::IsType %i %i\n", iStackPos, iType);
	int actualType = lua_type(state, iStackPos);

	if (actualType == iType)
		return true;

	if (iType > 8)
	{
		GarrysMod::Lua::ILuaBase::UserData* udata = (GarrysMod::Lua::ILuaBase::UserData*)GetUserdata(iStackPos);
		if (udata && udata->type == iType)
		{
			LuaDebugPrint(4, "CLuaInterface::IsType %i %i (%i)\n", iStackPos, iType, udata->type);
			return true;
		}
	}

	LuaDebugPrint(4, "CLuaInterface::IsType %i %i (%i)\n", iStackPos, iType, actualType);

	return false;
}

int CLuaInterface::GetType(int iStackPos)
{
	LuaDebugPrint(4, "CLuaInterface::GetType %i %i\n", iStackPos, lua_type(state, iStackPos));
	int type = lua_type(state, iStackPos);

	if (type == GarrysMod::Lua::Type::UserData)
	{
		GarrysMod::Lua::ILuaBase::UserData* udata = (GarrysMod::Lua::ILuaBase::UserData*)GetUserdata(iStackPos);
		if (udata)
		{
			type = udata->type;
		}
	}

	LuaDebugPrint(4, "CLuaInterface::GetType %i %i (%i)\n", iStackPos, lua_type(state, iStackPos), type);

	return type == -1 ? GarrysMod::Lua::Type::Nil : type;
}

const char* CLuaInterface::GetTypeName(int iType)
{
	LuaDebugPrint(1, "CLuaInterface::GetTypeName %i\n", iType);
	if (iType < 0) {
		return "none";
	} else {
		const char* pName = GetActualTypeName(iType);
		if (!pName) {
			return "unknown";
		} else {
			return pName;
		}
	}
}

void CLuaInterface::CreateMetaTableType(const char* strName, int iType)
{
	LuaDebugPrint(2, "CLuaInterface::CreateMetaTableType\n");
	V_strncpy(m_strTypes[iType - GarrysMod::Lua::Type::Type_Count], strName, sizeof(m_strTypes[iType - GarrysMod::Lua::Type::Type_Count]));
	luaL_newmetatable_type(state, strName, iType);

	if (iType > 254) {
		return;
	}
}

const char* CLuaInterface::CheckString(int iStackPos)
{
	LuaDebugPrint(4, "CLuaInterface::CheckString %i %s\n", iStackPos, luaL_checklstring(state, iStackPos, NULL));

	return luaL_checklstring(state, iStackPos, NULL);
}

double CLuaInterface::CheckNumber(int iStackPos)
{
	LuaDebugPrint(4, "CLuaInterface::CheckNumber %i\n", iStackPos);
	return luaL_checknumber(state, iStackPos);
}

int CLuaInterface::ObjLen(int iStackPos)
{
	LuaDebugPrint(3, "CLuaInterface::ObjLen\n");
	return lua_objlen(state, iStackPos);
}

// ------------------------------------------
// ToDo: Recheck every function below!
// ------------------------------------------

static const QAngle ang_invalid(FLT_MAX, FLT_MAX, FLT_MAX);
const QAngle& CLuaInterface::GetAngle(int iStackPos)
{
	LuaDebugPrint(2, "CLuaInterface::GetAngle\n");
	GarrysMod::Lua::ILuaBase::UserData* udata = (GarrysMod::Lua::ILuaBase::UserData*)GetUserdata(iStackPos);
	if (!udata || !udata->data || udata->type != GarrysMod::Lua::Type::Angle)
		return ang_invalid;

	return *(QAngle*)udata->data;
}

static const Vector vec_invalid(FLT_MAX, FLT_MAX, FLT_MAX);
const Vector& CLuaInterface::GetVector(int iStackPos)
{
	LuaDebugPrint(2, "CLuaInterface::GetVector\n");
	GarrysMod::Lua::ILuaBase::UserData* udata = (GarrysMod::Lua::ILuaBase::UserData*)GetUserdata(iStackPos);
	if (!udata || !udata->data || udata->type != GarrysMod::Lua::Type::Vector)
		return vec_invalid;

	return *(Vector*)udata->data;
}

void CLuaInterface::PushAngle(const QAngle& val)
{
	LuaDebugPrint(2, "CLuaInterface::PushAngle\n");
	
	GarrysMod::Lua::ILuaBase::UserData* udata = (GarrysMod::Lua::ILuaBase::UserData*)NewUserdata(20); // Should we use PushUserType?
	*(QAngle*)udata->data = val;
	udata->type = GarrysMod::Lua::Type::Angle;

	if (PushMetaTable(GarrysMod::Lua::Type::Angle))
		SetMetaTable(-2);
}

void CLuaInterface::PushVector(const Vector& val)
{
	LuaDebugPrint(2, "CLuaInterface::PushVector\n");

	GarrysMod::Lua::ILuaBase::UserData* udata = (GarrysMod::Lua::ILuaBase::UserData*)NewUserdata(20);
	*(Vector*)udata->data = val;
	udata->type = GarrysMod::Lua::Type::Vector;

	if (PushMetaTable(GarrysMod::Lua::Type::Vector))
		SetMetaTable(-2);
}

void CLuaInterface::SetState(lua_State* L)
{
	LuaDebugPrint(3, "CLuaInterface::SetState\n");
	state = L;
}

int CLuaInterface::CreateMetaTable(const char* strName) // Return value is probably a bool? Update: NO! It returns a int -> metaID
{
	LuaDebugPrint(2, "CLuaInterface::CreateMetaTable\n");
	//luaL_newmetatable_type(state, strName, -1);

	int ref = -1;
	PushSpecial(GarrysMod::Lua::SPECIAL_REG);
		GetField(-1, strName);
		if (IsType(-1, GarrysMod::Lua::Type::Table))
		{
			ref = ReferenceCreate();
		} else {
			Pop(1);
		}
	Pop(1);

	if (ref != -1)
	{
		ReferencePush(ref);
		ReferenceFree(ref);
		lua_getfield(state, -1, "MetaID");
		int metaID = (int)lua_tonumber(state, -1);
		lua_pop(state, 1);
		return metaID;
	} else {
		luaL_newmetatable_type(state, strName, ++m_iTypeNum); // Missing this logic in lua-shared, CreateMetaTable creates it if it's missing.
		V_strncpy(m_strTypes[m_iTypeNum - GarrysMod::Lua::Type::Type_Count], strName, sizeof(m_strTypes[m_iTypeNum - GarrysMod::Lua::Type::Type_Count]));
		return m_iTypeNum;
	}
}

bool CLuaInterface::PushMetaTable(int iType)
{
	LuaDebugPrint(2, "CLuaInterface::PushMetaTable %i\n", iType);
	bool bFound = false;
	const char* type = GetActualTypeName(iType);
	PushSpecial(GarrysMod::Lua::SPECIAL_REG);
		GetField(-1, type);
		bFound = IsType(-1, GarrysMod::Lua::Type::Table);
	Remove(-2);

	if (bFound)
		return true;

	Pop(1);

	::Msg("I failed u :< %i\n", iType);

	return false;
}

void CLuaInterface::PushUserType(void* data, int iType)
{
	LuaDebugPrint(2, "CLuaInterface::PushUserType %i\n", iType);

	GarrysMod::Lua::ILuaBase::UserData* udata = (GarrysMod::Lua::ILuaBase::UserData*)NewUserdata(8);
	udata->data = data;
	udata->type = (unsigned char)iType;

	if (PushMetaTable(iType))
	{
		SetMetaTable(-2);
	} else {
		::Msg("Failed to find Metatable for %i!\n", iType);
	}
}

void CLuaInterface::SetUserType(int iStackPos, void* data)
{
	LuaDebugPrint(2, "CLuaInterface::SetUserType\n");
	
	GarrysMod::Lua::ILuaBase::UserData* udata = (GarrysMod::Lua::ILuaBase::UserData*)GetUserdata(iStackPos);
	if (udata)
		udata->data = data;
}

// =================================
// ILuaInterface implementations
// =================================

int LuaPanic(lua_State* lua)
{
	LuaDebugPrint(1, "CLuaInterface::LuaPanic\n");

	Error("Lua Panic! Something went horribly wrong!\n%s", lua_tolstring(lua, -1, 0));
	return 0;
}

bool CLuaInterface::Init( GarrysMod::Lua::ILuaGameCallback* callback, bool bIsServer )
{
	LuaDebugPrint(1, "CLuaInterface::Init Server: %s\n", bIsServer ? "Yes" : "No");
	m_pGameCallback = callback;
	memset(&m_sPathID, 0, sizeof(m_sPathID)); // NULL out pathID properly, gmod doesn't seem to do this.
	memset(&m_strTypes, 0, sizeof(m_strTypes));

	state = luaL_newstate();
	luaL_openlibs(state);

	state->luabase = this;
	SetState(state);

	lua_atpanic(state, LuaPanic);

	lua_pushcclosure(state, AdvancedLuaErrorReporter, 0);
	lua_setglobal(state, "AdvancedLuaErrorReporter");

	{
		luaJIT_setmode(state, -1, LUAJIT_MODE_WRAPCFUNC|LUAJIT_MODE_ON);

		// ToDo: Find out how to check the FPU precision
		// Warning("Lua detected bad FPU precision! Prepare for weirdness!");
	}

	DoStackCheck();

	NewGlobalTable("");

	DoStackCheck();

	lua_createtable(state, (sizeof(g_PooledStrings) / 4), 0);
	
	int idx = 0;
	for(const char* str : g_PooledStrings)
	{
		PushString(str);
		lua_rawseti(state, -2, ++idx);
	}

	m_iStringPoolReference = ReferenceCreate();

	DoStackCheck();

	// lua_getfield(state, "debug");

	// lua_pushnil(state);
	// lua_setfield(state, -2, "setlocal");

	// lua_pushnil(state);
	// lua_setfield(state, -2, "setupvalue");

	// lua_pushnil(state);
	// lua_setfield(state, -2, "upvalueid");

	// lua_pushnil(state);
	// lua_setfield(state, -2, "upvaluejoin");

	// lua_pop(state, 1);

	return true;
}

void CLuaInterface::Shutdown()
{
	LuaDebugPrint(1, "CLuaInterface::Shutdown\n");
	GMOD_UnloadBinaryModules(state);

	lua_close(state);
}

void CLuaInterface::Cycle()
{
	LuaDebugPrint(3, "CLuaInterface::Cycle\n");
	// iLastTimeCheck

	DoStackCheck();

	RunThreadedCalls();
}

void CLuaInterface::RunThreadedCalls()
{
	LuaDebugPrint(3, "CLuaInterface::RunThreadedCalls\n");
	for (GarrysMod::Lua::ILuaThreadedCall* call : m_pThreadedCalls)
	{
		call->Init();
	}

	for (GarrysMod::Lua::ILuaThreadedCall* call : m_pThreadedCalls)
	{
		call->Run(this);
	}

	m_pThreadedCalls.clear();
}

int CLuaInterface::AddThreadedCall(GarrysMod::Lua::ILuaThreadedCall* call)
{
	LuaDebugPrint(1, "CLuaInterface::AddThreadedCall What called this?\n");

	m_pThreadedCalls.push_back(call);

	return m_pThreadedCalls.size();
}

GarrysMod::Lua::ILuaObject* CLuaInterface::Global()
{
	LuaDebugPrint(4, "CLuaInterface::Global\n");
	Error("CLuaInterface::Global NOT IMPLEMENTED");
	return NULL;
}

GarrysMod::Lua::ILuaObject* CLuaInterface::GetObject(int index)
{
	LuaDebugPrint(4, "CLuaInterface::GetObject\n");
	Error("CLuaInterface::GetObject NOT IMPLEMENTED");
	return NULL;
}

void CLuaInterface::PushLuaObject(GarrysMod::Lua::ILuaObject* obj)
{
	LuaDebugPrint(4, "CLuaInterface::PushLuaObject\n");
	Error("CLuaInterface::PushLuaObject NOT IMPLEMENTED!\n");
	// Don't call obj->Push as we are the one supposed to push it.
}

void CLuaInterface::PushLuaFunction(GarrysMod::Lua::CFunc func)
{
	LuaDebugPrint(4, "CLuaInterface::PushLuaFunction\n");
	lua_pushcclosure(state, func, 0);
}

void CLuaInterface::LuaError(const char* str, int iStackPos)
{
	LuaDebugPrint(4, "CLuaInterface::LuaError %s %i\n", str, iStackPos);
	
	if (iStackPos != -1)
	{
		luaL_argerror(state, iStackPos, str);
	} else {
		LuaDebugPrint(1, "CLuaInterface::LuaError_IMPORTANT %s %i\n", str, iStackPos);
	}
}

void CLuaInterface::TypeError(const char* str, int iStackPos)
{
	LuaDebugPrint(2, "CLuaInterface::TypeError %s %i\n", str, iStackPos);
	luaL_typerror(state, iStackPos, str);
}

void CLuaInterface::CallInternal(int args, int rets)
{
	LuaDebugPrint(2, "CLuaInterface::CallInternal %i %i\n", args, rets);
	
	CallFunctionProtected(args, rets, true);
}

void CLuaInterface::CallInternalNoReturns(int args)
{
	LuaDebugPrint(2, "CLuaInterface::CallInternalNoReturns %i\n", args);
	
	CallFunctionProtected(args, 0, true);
}

bool CLuaInterface::CallInternalGetBool(int args)
{
	LuaDebugPrint(2, "CLuaInterface::CallInternalGetBool %i\n", args);
	
	bool ret = false;
	if (CallFunctionProtected(args, 1, 1)) {
		ret = GetBool(-1);
		Pop(1);
	}

	return ret;
}

const char* CLuaInterface::CallInternalGetString(int args)
{
	LuaDebugPrint(2, "CLuaInterface::CallInternalGetString %i\n", args);

	const char* ret = NULL;
	if (CallFunctionProtected(args, 1, 1)) {
		ret = GetString(-1);
		Pop(1);
	}

	return ret;
}

bool CLuaInterface::CallInternalGet(int args, GarrysMod::Lua::ILuaObject* obj)
{
	LuaDebugPrint(2, "CLuaInterface::CallInternalGet %i\n", args);
	Error("CLuaInterface::CallInternalGet NOT IMPLEMENTED!\n");
	return false;
}

void CLuaInterface::NewGlobalTable(const char* name)
{
	LuaDebugPrint(1, "CLuaInterface::NewGlobalTable %s\n", name);

	//CreateTable();
	PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	m_iGlobalReference = ReferenceCreate();
	//SetField(LUA_GLOBALSINDEX, name);
}

GarrysMod::Lua::ILuaObject* CLuaInterface::NewTemporaryObject()
{
	LuaDebugPrint(2, "CLuaInterface::NewTemporaryObject\n");

	Error("CLuaInterface::NewTemporaryObject NOT IMPLEMENTED");
	return NULL;
}

bool CLuaInterface::isUserData(int iStackPos)
{
	LuaDebugPrint(4, "CLuaInterface::isUserData %i %s\n", iStackPos, lua_type(state, iStackPos) == GarrysMod::Lua::Type::UserData ? "Yes" : "No");

	return lua_type(state, iStackPos) == GarrysMod::Lua::Type::UserData;
}

GarrysMod::Lua::ILuaObject* CLuaInterface::GetMetaTableObject(const char* name, int type)
{
	LuaDebugPrint(2, "CLuaInterface::GetMetaTableObject %s, %i\n", name, type);

	Error("CLuaInterface::GetMetaTableObject NOT IMPLEMENTED");
	return NULL;
}

GarrysMod::Lua::ILuaObject* CLuaInterface::GetMetaTableObject(int iStackPos)
{
	LuaDebugPrint(2, "CLuaInterface::GetMetaTableObject\n");

	Error("CLuaInterface::GetMetaTableObject NOT IMPLEMENTED");
	return NULL;
}

GarrysMod::Lua::ILuaObject* CLuaInterface::GetReturn(int iStackPos)
{
	LuaDebugPrint(2, "CLuaInterface::GetReturn\n");
	Error("CLuaInterface::GetReturn NOT IMPLEMENTED");
	return nullptr;
}

bool CLuaInterface::IsServer()
{
	LuaDebugPrint(3, "CLuaInterface::IsServer\n");

	return m_iRealm == GarrysMod::Lua::State::SERVER;
}

bool CLuaInterface::IsClient()
{
	LuaDebugPrint(3, "CLuaInterface::IsClient\n");

	return m_iRealm == GarrysMod::Lua::State::CLIENT;
}

bool CLuaInterface::IsMenu()
{
	LuaDebugPrint(3, "CLuaInterface::IsMenu\n");

	return m_iRealm == GarrysMod::Lua::State::MENU;
}

void CLuaInterface::DestroyObject(GarrysMod::Lua::ILuaObject* obj)
{
	LuaDebugPrint(4, "CLuaInterface::DestroyObject\n");
	Error("CLuaInterface::DestroyObject NOT IMPLEMENTED!\n");
}

GarrysMod::Lua::ILuaObject* CLuaInterface::CreateObject()
{
	LuaDebugPrint(4, "CLuaInterface::CreateObject\n");

	Error("CLuaInterface::CreateObject NOT IMPLEMENTED!\n");
	return NULL;
}

void CLuaInterface::SetMember(GarrysMod::Lua::ILuaObject* obj, GarrysMod::Lua::ILuaObject* key, GarrysMod::Lua::ILuaObject* value)
{
	LuaDebugPrint(3, "CLuaInterface::SetMember 1\n");
	Error("CLuaInterface::SetMember(1) NOT IMPLEMENTED!\n");
}

GarrysMod::Lua::ILuaObject* CLuaInterface::GetNewTable()
{
	LuaDebugPrint(2, "CLuaInterface::GetNewTable\n");

	Error("CLuaInterface::GetNewTable NOT IMPLEMENTED!\n");
	return NULL;
}

void CLuaInterface::SetMember(GarrysMod::Lua::ILuaObject* obj, float key)
{
	LuaDebugPrint(3, "CLuaInterface::SetMember 2\n");
	Error("CLuaInterface::SetMember(2) NOT IMPLEMENTED!\n");
}

void CLuaInterface::SetMember(GarrysMod::Lua::ILuaObject* obj, float key, GarrysMod::Lua::ILuaObject* value)
{
	LuaDebugPrint(3, "CLuaInterface::SetMember 3 %f\n", key);
	Error("CLuaInterface::SetMember(3) NOT IMPLEMENTED!\n");
}

void CLuaInterface::SetMember(GarrysMod::Lua::ILuaObject* obj, const char* key)
{
	LuaDebugPrint(3, "CLuaInterface::SetMember 4 %s\n", key);
	Error("CLuaInterface::SetMember(4) NOT IMPLEMENTED!\n");
}

void CLuaInterface::SetMember(GarrysMod::Lua::ILuaObject* obj, const char* key, GarrysMod::Lua::ILuaObject* value)
{
	LuaDebugPrint(3, "CLuaInterface::SetMember 5 %s\n", key);
	Error("CLuaInterface::SetMember(5) NOT IMPLEMENTED!\n");
}

void CLuaInterface::SetType(unsigned char realm)
{
	LuaDebugPrint(1, "CLuaInterface::SetType %u\n", realm);
	m_iRealm = realm;
}

void CLuaInterface::PushLong(long number)
{
	LuaDebugPrint(4, "CLuaInterface::PushLong\n");
	lua_pushnumber(state, number);
}

int CLuaInterface::GetFlags(int iStackPos)
{
	LuaDebugPrint(1, "CLuaInterface::GetFlags %i %i\n", iStackPos, GetType(iStackPos));

	return (int)GetNumber(iStackPos); // ToDo: Verify this
}

bool CLuaInterface::FindOnObjectsMetaTable(int iStackPos, int keyIndex)
{
	LuaDebugPrint(2, "CLuaInterface::FindOnObjectsMetaTable %i %i %s\n", iStackPos, keyIndex, lua_tolstring(state, keyIndex, NULL));

	if (lua_getmetatable(state, iStackPos) == 1)
	{
		lua_pushvalue(state, keyIndex);
		lua_gettable(state, -2);
		if (lua_type(state, -1) == GarrysMod::Lua::Type::Nil)
		{
			Pop(2);
		} else {
			Remove(-2);
			LuaDebugPrint(2, "CLuaInterface::FindOnObjectsMetaTable GOOD\n");
			return true;
		}
	}

	LuaDebugPrint(2, "CLuaInterface::FindOnObjectsMetaTable BAD\n");
	return false;
}

bool CLuaInterface::FindObjectOnTable(int iStackPos, int keyIndex)
{
	LuaDebugPrint(2, "CLuaInterface::FindObjectOnTable\n");
	if (IsType(iStackPos, GarrysMod::Lua::Type::Table))
	{
		lua_pushvalue(state, iStackPos);
		lua_pushvalue(state, keyIndex);
		lua_gettable(state, -2);
		lua_remove(state, -2);
		if (GetType(-1) != GarrysMod::Lua::Type::Nil)
		{
			return true;
		} else {
			lua_remove(state, -1);
		}
	} else {
		LuaDebugPrint(2, "CLuaInterface::FindObjectOnTable NOT A TABLE!\n");
	}

	return false;
}

void CLuaInterface::SetMemberFast(GarrysMod::Lua::ILuaObject* obj, int keyIndex, int valueIndex)
{
	LuaDebugPrint(3, "CLuaInterface::SetMemberFast %i %i %s\n", keyIndex, valueIndex, GetType(keyIndex) == GarrysMod::Lua::Type::String ? GetString(keyIndex) : "");
	Error("CLuaInterface::SetMemberFast NOT IMPLEMENTED!\n");
}

bool CLuaInterface::RunString(const char* filename, const char* path, const char* stringToRun, bool run, bool showErrors)
{
	LuaDebugPrint(2, "CLuaInterface::RunString\n");
	return RunStringEx(filename, path, stringToRun, run, showErrors, true, true);
}

bool CLuaInterface::IsEqual(GarrysMod::Lua::ILuaObject* objA, GarrysMod::Lua::ILuaObject* objB)
{
	LuaDebugPrint(2, "CLuaInterface::IsEqual\n");

	Error("CLuaInterface::IsEqual NOT IMPLEMENTED!\n");
	return false;
}

void CLuaInterface::Error(const char* err)
{
	LuaDebugPrint(2, "CLuaInterface::Error\n");
	luaL_error(state, "%s", err);
}

const char* CLuaInterface::GetStringOrError(int index)
{
	LuaDebugPrint(3, "CLuaInterface::GetStringOrError\n");
	const char* string = lua_tolstring(state, index, NULL);
	if (string == NULL)
	{
		Error("You betraid me"); // ToDo: This should probably be an Arg error
	}

	return string;
}

bool CLuaInterface::RunLuaModule(const char* name)
{
	LuaDebugPrint(2, "CLuaInterface::RunLuaModule\n");
	// ToDo
	char* dest = new char[511];
	V_snprintf(dest, 511, "includes/modules/%s.lua", name);

	// NOTE: Why does it use !MODULE ?
	bool found = FindAndRunScript(dest, true, true, "", true);

	delete[] dest;

	return found;
}

std::string ToPath(std::string path)
{
	size_t lastSeparatorPos = path.find_last_of("/\\");

	if (lastSeparatorPos != std::string::npos) {
		return path.substr(0, lastSeparatorPos + 1);
	}

	return path;
}

bool CLuaInterface::FindAndRunScript(const char *filename, bool run, bool showErrors, const char *stringToRun, bool noReturns)
{
	LuaDebugPrint(2, "CLuaInterface::FindAndRunScript %s, %s, %s, %s, %s\n", filename, run ? "Yes" : "No", showErrors ? "Yes" : "No", stringToRun, noReturns ? "Yes" : "No");

	//if (true)
	//	return false;

	//bool bDataTable = ((std::string)filename).rfind("!lua", 0) == 0;
	Error("CLuaInterface::FindAndRunScript NOT IMPLEMENTED!\n");

	return false;
}

void CLuaInterface::SetPathID(const char* pathID)
{
	LuaDebugPrint(1, "CLuaInterface::SetPathID %s\n", pathID);
	V_strncpy(m_sPathID, pathID, sizeof(m_sPathID));
}

const char* CLuaInterface::GetPathID()
{
	LuaDebugPrint(2, "CLuaInterface::GetPathID\n");

	return m_sPathID;
}

void CLuaInterface::ErrorNoHalt( const char* fmt, ... )
{
	LuaDebugPrint(2, "CLuaInterface::ErrorNoHalt %s\n", fmt);

	GarrysMod::Lua::ILuaGameCallback::CLuaError* error = ReadStackIntoError(state, true);

	va_list args;
	va_start(args, fmt);

	int size = vsnprintf(NULL, 0, fmt, args);
	if (size < 0) {
		va_end(args);
		return;
	}

	char* buffer = new char[size + 1];
	vsnprintf(buffer, size + 1, fmt, args);
	buffer[size] = '\0';

	error->message = buffer;
	va_end(args);

	if (m_pGameCallback)
	{
		m_pGameCallback->LuaError(error);
	} else {
		Msg("An error ocurred! %s\n", error->message.c_str());
	}

	delete[] buffer;
	delete error; // Deconstuctor will delete our buffer
}

void CLuaInterface::Msg( const char* fmt, ... )
{
	LuaDebugPrint(2, "CLuaInterface::Msg %s\n", fmt);

	va_list args;
	va_start(args, fmt);

	char* buffer = new char[4096];
	V_vsnprintf(buffer, 4096, fmt, args);

	va_end(args);

	m_pGameCallback->Msg(buffer, false);

	delete[] buffer;
}

void CLuaInterface::PushPath( const char* path )
{
	LuaDebugPrint(2, "CLuaInterface::PushPath %s\n", path);
	char* str = new char[strlen(path)];
	V_strncpy( str, path, strlen(path) );
	str[ strlen(path) - 1 ] = '\0'; // nuke the last /
	m_sCurrentPath = str;
	m_pPaths.push_back( str );
	++m_iPushedPaths;
}

void CLuaInterface::PopPath()
{
	LuaDebugPrint(2, "CLuaInterface::PopPath\n");
	char* str = m_pPaths.back();
	delete[] str;
	m_pPaths.pop_back();

	--m_iPushedPaths;
	if ( m_iPushedPaths > 0 )
		m_sCurrentPath = m_pPaths.back();
	else
		m_sCurrentPath = NULL;
}

const char* CLuaInterface::GetPath()
{
	LuaDebugPrint(2, "CLuaInterface::GetPath\n");

	if ( m_iPushedPaths <= 0 )
		return NULL;

	LuaDebugPrint(2, "CLuaInterface::GetPath %s\n", (const char*)m_pPaths.back());
	return m_pPaths.back();
}

int CLuaInterface::GetColor(int iStackPos) // Probably returns the StackPos
{
	LuaDebugPrint(2, "CLuaInterface::GetColor\n");

	return 0;
}

void CLuaInterface::PushColor(Color color)
{
	LuaDebugPrint(2, "CLuaInterface::PushColor\n");
	
	CreateTable();
		PushNumber( color.r() );
		SetField( -2, "r" );

		PushNumber( color.g() );
		SetField( -2, "g" );

		PushNumber( color.b() );
		SetField( -2, "b" );

		PushNumber( color.a() );
		SetField( -2, "a" );

	if ( CreateMetaTable( "Color" ) == 1 )
		SetMetaTable( -1 );
}

int CLuaInterface::GetStack(int level, lua_Debug* dbg)
{
	LuaDebugPrint(2, "CLuaInterface::GetStack %i\n", level);

	return lua_getstack(state, level, dbg);
}

int CLuaInterface::GetInfo(const char* what, lua_Debug* dbg)
{
	LuaDebugPrint(2, "CLuaInterface::GetInfo %s\n", what);

	return lua_getinfo(state, what, dbg);
}

const char* CLuaInterface::GetLocal(lua_Debug* dbg, int n)
{
	LuaDebugPrint(2, "CLuaInterface::GetLocal %i\n", n);

	return lua_getlocal(state, dbg, n);
}

const char* CLuaInterface::GetUpvalue(int funcIndex, int n)
{
	LuaDebugPrint(2, "CLuaInterface::GetUpvalue %i %i\n", funcIndex, n);

	return lua_getupvalue(state, funcIndex, n);
}

bool CLuaInterface::RunStringEx(const char *filename, const char *path, const char *stringToRun, bool run, bool printErrors, bool dontPushErrors, bool noReturns)
{
	LuaDebugPrint(1, "CLuaInterface::RunStringEx %s, %s\n", filename, path);

	std::string code = RunMacros(stringToRun);
	int res = luaL_loadbuffer(state, code.c_str(), code.length(), filename);
	if (res != 0)
	{
		GarrysMod::Lua::ILuaGameCallback::CLuaError* error = ReadStackIntoError(state, false);
		if (dontPushErrors)
			Pop(1);

		if (printErrors)
		{
			if (m_pGameCallback)
			{
				m_pGameCallback->LuaError(error);
			} else {
				Msg("An error ocurred! %s\n", error->message.c_str());
			}
		}

		delete error;

		return false;
	} else {
		return CallFunctionProtected(0, 0, printErrors);
	}
}

size_t CLuaInterface::GetDataString(int index, const char **str)
{
	LuaDebugPrint(2, "CLuaInterface::GetDataString\n");
	// ToDo

	Error("CLuaInterface::GetDataString is not implemented!\n");

	return 0;
}

void CLuaInterface::ErrorFromLua(const char *fmt, ...)
{
	LuaDebugPrint(2, "CLuaInterface::ErrorFromLua %s\n", fmt);
	GarrysMod::Lua::ILuaGameCallback::CLuaError* error = ReadStackIntoError(state, true);

	va_list args;
	va_start(args, fmt);

	int size = vsnprintf(NULL, 0, fmt, args);
	if (size < 0) {
		va_end(args);
		return;
	}

	char* buffer = new char[size + 1];
	vsnprintf(buffer, size + 1, fmt, args);
	buffer[size] = '\0';

	error->message = buffer;
	va_end(args);
	
	/* NOTE: This is already done in ReadStackIntoError
	
	const char* realm;
	switch(m_iRealm)
	{
		case 0:
			realm = "client";
			break;
		case 1:
			realm = "server";
			break;
		case 2:
			realm = "menu";
			break;
		default:
			realm = "unknown";
			break;
	}
	error->side = realm;*/

/*#ifndef WIN32
	Msg("Error Message: %s\n", error->message);
	Msg("Error Side: %s\n", error->side);

	for ( GarrysMod::Lua::ILuaGameCallback::CLuaError::StackEntry entry : error->stack )
	{
		Msg("Error Stack function: %s\n", entry.function);
		Msg("Error Stack source: %s\n", entry.source);
		Msg("Error Stack line: %i\n", entry.line);
	}
#endif*/

	if (m_pGameCallback)
	{
		m_pGameCallback->LuaError(error);
	}
	else {
		Msg("An error ocurred! %s\n", error->message.c_str());
	}

	delete[] buffer;
	delete error; // Deconstuctor will delete our buffer
}

const char* CLuaInterface::GetCurrentLocation()
{
	LuaDebugPrint(2, "CLuaInterface::GetCurrentLocation\n");
	
	lua_Debug ar;
	lua_getstack(state, 1, &ar);
	lua_getinfo(state, "Sl", &ar);
	if (ar.source && strcmp(ar.what, "C") != 0)
	{
		static char strOutput[511];
		V_snprintf( strOutput, 511, "%s (line %i)", ar.source, ar.currentline );

		LuaDebugPrint(2, "CLuaInterface::GetCurrentLocation %s\n", strOutput);
		return strOutput;
	}

	return "<nowhere>";
}

void CLuaInterface::MsgColour(const Color& col, const char* fmt, ...)
{
	LuaDebugPrint(2, "CLuaInterface::MsgColour\n");

	va_list args;
	va_start(args, fmt);

	int size = vsnprintf(NULL, 0, fmt, args);
	if (size < 0) {
		va_end(args);
		return;
	}

	char* buffer = new char[size + 1];
	vsnprintf(buffer, size + 1, fmt, args);

	m_pGameCallback->MsgColour(buffer, col);

	delete[] buffer;
	va_end(args);
}

void CLuaInterface::GetCurrentFile(std::string &outStr)
{
	LuaDebugPrint(1, "CLuaInterface::GetCurrentFile\n");

	lua_Debug ar;
	int level = 0;
	while (lua_getstack(state, level, &ar) != 0)
	{
		lua_getinfo(state, "S", &ar);
		if (ar.source && strcmp(ar.what, "C") != 0)
		{
			outStr.assign(ar.source);
			LuaDebugPrint(1, "CLuaInterface::GetCurrentFile %s\n", ar.source);
			return;
		}
		++level;
	}

	outStr = "!UNKNOWN";
	LuaDebugPrint(1, "CLuaInterface::GetCurrentFile %s\n", "!UNKNOWN (How dare you)");
}

void CLuaInterface::CompileString(Bootil::Buffer& dumper, const std::string& stringToCompile)
{
	LuaDebugPrint(2, "CLuaInterface::CompileString\n");
	// ToDo

	Error("CLuaInterface::CompileString is not implemented!");
}

bool CLuaInterface::CallFunctionProtected(int iArgs, int iRets, bool showError)
{
	LuaDebugPrint(2, "CLuaInterface::CallFunctionProtected %i %i %s\n", iArgs, iRets, showError ? "Yes" : "no");

	/*for (int i=1;i <= Top(); ++i)
	{
		LuaDebugPrint(4, "Stack: %i, Type: %i\n", i, GetType(i));
	}*/

	if (GetType(-(iArgs + 1)) != GarrysMod::Lua::Type::Function)
	{
		Warning("[CLuaInterface::CallFunctionProtected] You betraid me. This is not a function :<\n");
		return false;
	}

	int ret = PCall(iArgs, iRets, 0);
	if (ret != 0)
	{
		GarrysMod::Lua::ILuaGameCallback::CLuaError* error = ReadStackIntoError(state, false);
		if (showError)
		{
			if (m_pGameCallback)
			{
				m_pGameCallback->LuaError(error);
			} else {
				Msg("An error ocurred! %s\n", error->message.c_str());
			}
		}
		delete error;
		Pop(1);
	}

	return ret == 0;
}

void CLuaInterface::Require(const char* cname)
{
	LuaDebugPrint(2, "CLuaInterface::Require %s\n", cname);

	std::string name = cname;
	name = (IsClient() ? "gmcl_" : "gmsv_") + name + "_";

#ifdef SYSTEM_MACOSX
	name = name + "osx";
#else
	#ifdef SYSTEM_WINDOWS
		name = name + "win";
	#else
		name = name + "linux";
	#endif

	#ifdef ARCHITECTURE_X86_64
		name = name + "64";
	#else
		#ifdef SYSTEM_WINDOWS
			name = name + "32";
		#endif
	#endif
#endif
	name = name + ".dll";


	std::string path = (std::string)"lua/bin/" + name;
	if (g_pFullFileSystem->FileExists(path.c_str(), "MOD"))
	{
		char dllpath[512];
		g_pFullFileSystem->RelativePathToFullPath(path.c_str(), "MOD", dllpath, sizeof(dllpath));
		GMOD_LoadBinaryModule(state, dllpath);
		//delete[] dllpath; //NOTE: Delete this and it will cause a critical error? probably because the value is still in use by Lua
	} else {
		RunLuaModule(cname);
	}
}

const char* CLuaInterface::GetActualTypeName(int type)
{
	LuaDebugPrint(4, "CLuaInterface::GetActualTypeName\n");

	//lua_typename(state, lua_type(state, type));

	return (type < GarrysMod::Lua::Type::Type_Count) ? GarrysMod::Lua::Type::Name[type] : m_strTypes[type - GarrysMod::Lua::Type::Type_Count];
}

void CLuaInterface::PreCreateTable(int arrelems, int nonarrelems)
{
	LuaDebugPrint(4, "CLuaInterface::PreCreateTable %i %i\n", arrelems, nonarrelems);
	lua_createtable(state, arrelems, nonarrelems);
}

void CLuaInterface::PushPooledString(int index)
{
	LuaDebugPrint(2, "CLuaInterface::PushPooledString %i %s\n", index, g_PooledStrings[index]);
	
	ReferencePush(m_iStringPoolReference);
	PushNumber(index+1); // LUA starts at 1 so we add 1
	GetTable(-2);
	Remove(-2);

	LuaDebugPrint(2, "CLuaInterface::PushPooledString %i %s %s\n", index, g_PooledStrings[index], GetString(-1));
}

const char* CLuaInterface::GetPooledString(int index)
{
	LuaDebugPrint(2, "CLuaInterface::GetPooledString\n");

	return g_PooledStrings[index];
}

void CLuaInterface::AppendStackTrace(char *, unsigned long)
{
	LuaDebugPrint(2, "CLuaInterface::AppendStackTrace\n");
	// ToDo

	Error("CLuaInterface::AppendStackTrace is not implemented!");
}

void* CLuaInterface::CreateConVar(const char* name, const char* defaultValue, const char* helpString, int flags)
{
	LuaDebugPrint(2, "CLuaInterface::CreateConVar\n");

	/*if ( IsServer() )
		flags |= FCVAR_LUA_SERVER;
	else
		flags |= FCVAR_LUA_CLIENT;*/

	Error("CLuaInterface::CreateConVar NOT IMPLEMENTED!\n");
	return NULL;//LuaConVars()->CreateConVar(name, defaultValue, helpString, flags);
}

void* CLuaInterface::CreateConCommand(const char* name, const char* helpString, int flags, FnCommandCallback_t callback, FnCommandCompletionCallback completionFunc)
{
	LuaDebugPrint(2, "CLuaInterface::CreateConCommand\n");

	Error("CLuaInterface::CreateConCommand NOT IMPLEMENTED!\n");
	return NULL;//LuaConVars()->CreateConCommand(name, helpString, flags, callback, completionFunc);
}

const char* CLuaInterface::CheckStringOpt( int iStackPos, const char* def )
{
	LuaDebugPrint(4, "CLuaInterface::CheckStringOpt %i %s\n", iStackPos, def);

	return luaL_optlstring(state, iStackPos, def, NULL);
}

double CLuaInterface::CheckNumberOpt( int iStackPos, double def )
{
	LuaDebugPrint(4, "CLuaInterface::CheckNumberOpt %i %f\n", iStackPos, def);

	return luaL_optnumber(state, iStackPos, def);
}

std::string CLuaInterface::RunMacros(std::string code)
{
	LuaDebugPrint(2, "CLuaInterface::RunMacros\n");

	code = std::regex_replace(code, std::regex("DEFINE_BASECLASS"), "local BaseClass = baseclass.Get");

	return code;
}

void CLuaInterface::RegisterMetaTable( const char* name, GarrysMod::Lua::ILuaObject* obj )
{
	Error("CLuaInterface::RegisterMetaTable NOT IMPLEMENTED!\n");
}

#ifdef WIN32
#include <Windows.h>
#define Handle HMODULE
#define LoadModule(name, _) LoadLibrary(name)
#define UnloadModule(handle) FreeLibrary((Handle)handle)
#define GetAddress(handle, name) GetProcAddress((Handle)handle, name)
#else
#include <dlfcn.h>
#define Handle void*
#define LoadModule(name, type) dlopen(name, type)
#define UnloadModule(handle) dlclose(handle)
#define GetAddress(handle, name) dlsym(handle, name)
#endif

void GMOD_LoadBinaryModule(lua_State* L, const char* name)
{
	lua_pushfstring(L, "LOADLIB: %s", name);
	GarrysMod::Lua::ILuaBase::UserData* udata = (GarrysMod::Lua::ILuaBase::UserData*)lua_newuserdata(L, sizeof(GarrysMod::Lua::ILuaBase::UserData));
	udata->data = nullptr;
	udata->type = GarrysMod::Lua::Type::UserData;

	lua_pushvalue(L, LUA_REGISTRYINDEX);
	lua_getfield(L, -1, "_LOADLIB");
	lua_setmetatable(L, -3);

	lua_pushvalue(L, -3);
	lua_pushvalue(L, -3);
	lua_settable(L, -3);
	//lua_pop(L, 1);

	void* hDll = LoadModule(name, RTLD_LAZY);
	if (hDll == NULL) {
		lua_pushliteral(L, "Failed to load dll!");
		lua_error(L);
		return;
	}

	GarrysMod::Lua::CFunc gmod13_open = (GarrysMod::Lua::CFunc)GetAddress(hDll, "gmod13_open");
	if (gmod13_open == NULL) {
		lua_pushliteral(L, "Failed to get gmod13_open!");
		lua_error(L);
		UnloadModule(hDll);
		return;
	}

	udata->data = hDll;

	lua_pushcclosure(L, gmod13_open, 0);
	lua_call(L, 0, 0);
}

void GMOD_UnloadBinaryModule(lua_State* L, const char* module, GarrysMod::Lua::ILuaBase::UserData* udata)
{
	if (udata->data != NULL)
	{
		GarrysMod::Lua::CFunc gmod13_close = (GarrysMod::Lua::CFunc)GetAddress(udata->data, "gmod13_close");
		if (gmod13_close != NULL) {
			lua_pushcclosure(L, gmod13_close, 0);
			lua_call(L, 0, 0);
		}

		UnloadModule(udata->data);
	}

	lua_pushvalue(L, LUA_REGISTRYINDEX);
	lua_pushnil(L);
	lua_setfield(L, -2, module);
	lua_pop(L, 1);
}

void GMOD_UnloadBinaryModules(lua_State* L)
{
	lua_pushvalue(L, LUA_REGISTRYINDEX);
	lua_pushnil(L);

	while (lua_next(L, -2) != 0) {
		if(lua_type(L, -2) == GarrysMod::Lua::Type::String && lua_type(L, -1) == GarrysMod::Lua::Type::UserData)
		{
			const char* module = lua_tolstring(L, -2, NULL);
			if (strncmp(module, "LOADLIB: ", 8) == 0) // Why are we doing it like this (I forgot)
			{
				Msg("Unloading %s\n", module);
				GMOD_UnloadBinaryModule(L, module, (GarrysMod::Lua::ILuaBase::UserData*)lua_touserdata(L, -1));
			}
		}

		lua_pop(L, 1);
	}

	lua_pop(L, 1);
}