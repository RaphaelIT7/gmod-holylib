#include "../lua/lua.hpp"
#include "../lua/lj_obj.h"
#include "../lua/luajit_rolling.h"
#include "CLuaInterface.h"
#include <filesystem.h>
#include "iluashared.h"
#include "lua/PooledStrings.h"
#include "util.h"
#include "Bootil/Bootil.h"
#include "color.h"
#include "lua.h"

#if defined(__clang__) || defined(__GNUC__)
#define POINT_UNREACHABLE   __builtin_unreachable()
#else
#define POINT_UNREACHABLE   __assume(false)
#endif

// Another shit workaround since the vtable is off on Windows.
// And until I get a response from Rubat I give up trying to properly fix the vtable.
// This project already has enouth shitty workarounds and I hate to keep having to add some just because some updates breaks shit without the changes being announced.
class CSimpleLuaObject;
inline CSimpleLuaObject* ToSimpleObject(GarrysMod::Lua::ILuaObject* pObj);
class CSimpleLuaObject
{
public:
	inline void Init(GarrysMod::Lua::ILuaInterface* pLua)
	{
		m_pLua = pLua;
		m_bUserData = false;
		m_reference = -1;
		m_iLUA_TYPE = -1;
	}

	inline void Push(GarrysMod::Lua::ILuaInterface* pLua)
	{
		if (!m_pLua)
		{
			Init(pLua);
		}

		if (m_reference != -1)
			m_pLua->ReferencePush(m_reference);
		else
			m_pLua->PushNil();
	}

	inline void SetFromStack(int iStackPos, GarrysMod::Lua::ILuaInterface* pLua)
	{
		if (!m_pLua)
		{
			Init(pLua);
		}

		m_pLua->Push(iStackPos);

		UnReference();
		m_iLUA_TYPE = m_pLua->GetType( -1 );
		m_bUserData = m_iLUA_TYPE > 7;

		m_reference = m_pLua->ReferenceCreate();
	}

	inline int GetType()
	{
		if (m_reference != -1)
		{
			m_pLua->ReferencePush(m_reference);
			int type = m_pLua->GetType(-1);
			m_pLua->Pop(1);
			return type;
		} else {
			return GarrysMod::Lua::Type::Nil;
		}
	}

	inline bool isTable()
	{
		return GetType() == GarrysMod::Lua::Type::Table;
	}

	inline void UnReference()
	{
		m_pLua->ReferenceFree(m_reference);
		m_reference = -1;
	}

	inline void SetMember(const char* name, const char* val)
	{
		if (m_reference != -1)
		{
			m_pLua->ReferencePush(m_reference);
			if (m_pLua->IsType(-1, GarrysMod::Lua::Type::Table))
			{
				m_pLua->PushString(val);
				m_pLua->SetField(-2, name);
			}
			m_pLua->Pop(1);
		}
	}

	inline int GetMemberInt(const char* name, int b)
	{
		int val = b;
		if (m_reference != -1)
		{
			m_pLua->ReferencePush(m_reference);
			if (m_pLua->IsType(-1, GarrysMod::Lua::Type::Table))
			{
				m_pLua->GetField(-1, name);
				if (m_pLua->IsType(-1, GarrysMod::Lua::Type::Number)) {
					val = (int)m_pLua->GetNumber(-1);
				}
				m_pLua->Pop(1);
			}
			m_pLua->Pop(1);
		}

		return val;
	}

	inline void SetMember(const char* name, int val)
	{
		if (m_reference != -1)
		{
			m_pLua->ReferencePush(m_reference);
			if (m_pLua->IsType(-1, GarrysMod::Lua::Type::Table))
			{
				m_pLua->PushNumber(val);
				m_pLua->SetField(-2, name);
			}
			m_pLua->Pop(1);
		}
	}

	inline void SetMetaTable(GarrysMod::Lua::ILuaObject* meta, GarrysMod::Lua::ILuaInterface* pLua)
	{
		Push(pLua);
		ToSimpleObject(meta)->Push(pLua);
		m_pLua->SetMetaTable( -2 );
	}

public:
	void* m_pVTABLE; // Vtable offset as else our member variables are all invalid.
	bool m_bUserData;
	int m_iLUA_TYPE;
	int m_reference;
	GarrysMod::Lua::ILuaInterface* m_pLua;
};

inline CSimpleLuaObject* ToSimpleObject(GarrysMod::Lua::ILuaObject* pObj)
{
	return static_cast<CSimpleLuaObject*>(static_cast<void*>(pObj));
}

static ConVar lua_debugmode("lua_debugmode_interface", "0", 0);
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
GarrysMod::Lua::ILuaGameCallback::CLuaError* ReadStackIntoError(lua_State* L)
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

	const char* str = lua_tolstring(L, -1, NULL);
	if (str != NULL) // Setting a std::string to NULL causes a crash. Don't care.
		lua_error->message = str;

	CLuaInterface* LUA = (CLuaInterface*)L->luabase;
	lua_error->side = LUA->IsClient() ? "client" : (LUA->IsMenu() ? "menu" : "server");

	return lua_error;
}

int AdvancedLuaErrorReporter(lua_State *L) 
{
	// VPROF AdvancedLuaErrorReporter GLua

	if (lua_isstring(L, 0)) {
		const char* str = lua_tostring(L, 0);

		g_LastError.assign(str);

		ReadStackIntoError(L);

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
	// ToDo: Add all fancy operations and delete ILuaGameCallback? (Should we really delete it? No.)
	// LuaInterface->Shutdown(); // BUG: Gmod doesn't do this in here... right?
	// NOTE: We alreay call Shutdown from Lua::DestroyInterface so if your not careful you might call it twice and crash.

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

bool CLuaInterface::SetMetaTable(int iStackPos)
{
	LuaDebugPrint(3, "CLuaInterface::SetMetaTable %i\n", iStackPos);
	return lua_setmetatable(state, iStackPos);
}

bool CLuaInterface::GetMetaTable(int iStackPos)
{
	LuaDebugPrint(3, "CLuaInterface::GetMetaTable %i\n", iStackPos);
	return lua_getmetatable(state, iStackPos);
}

void CLuaInterface::Call(int iArgs, int iResults)
{
	LuaDebugPrint(3, "CLuaInterface::Call %i %i\n", iArgs, iResults);
	lua_State* currentState = state;
	lua_call(currentState, iArgs, iResults);
	SetState(currentState); // done for some reason, idk. Probably done in case the state somehow changes like if you call a coroutine?
}

int CLuaInterface::PCall(int iArgs, int iResults, int iErrorFunc)
{
	LuaDebugPrint(2, "CLuaInterface::PCall %i %i %i\n", iArgs, iResults, iErrorFunc);
	lua_State* pCurrentState = state;
	int ret = lua_pcall(pCurrentState, iArgs, iResults, iErrorFunc);
	SetState(pCurrentState);
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

GarrysMod::Lua::ILuaBase::UserData* CLuaInterface::NewUserdata(unsigned int iSize)
{
	LuaDebugPrint(3, "CLuaInterface::NewUserdata\n");
	return (UserData*)lua_newuserdata(state, iSize);
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
		TypeError(GetTypeName(iType), iStackPos);
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

	size_t length;
	const char* pString = lua_tolstring(state, iStackPos, &length);
	if (iOutLen)
		*iOutLen = length;

	return pString;
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

GarrysMod::Lua::ILuaBase::UserData* CLuaInterface::GetUserdata(int iStackPos)
{
	LuaDebugPrint(4, "CLuaInterface::GetUserdata %i\n", iStackPos);
	return (ILuaBase::UserData*)lua_touserdata(state, iStackPos);
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

void CLuaInterface::PushUserdata(GarrysMod::Lua::ILuaBase::UserData* val)
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

	if (actualType == 7 && iType > 7) // Check for 7 since gmod doesn't know that all type values were shifted by 1 because of a new type in luajit.
		return iType == GetUserdata(iStackPos)->type; // Don't need to accout for type shift since this shouldn't be affected.

	return false;
}

int CLuaInterface::GetType(int iStackPos) // WHY DOES THIS USE A SWITCH BLOCK IN GMOD >:(
{
	LuaDebugPrint(4, "CLuaInterface::GetType %i %i\n", iStackPos, lua_type(state, iStackPos));
	int type = lua_type(state, iStackPos);

	if (type == GarrysMod::Lua::Type::UserData)
	{
		ILuaBase::UserData* udata = GetUserdata(iStackPos);
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
	if (iType < 0)
		return "none";

	constexpr int typeCount = sizeof(GarrysMod::Lua::Type::Name) / sizeof(const char*);
	if (iType <= typeCount)
		return GarrysMod::Lua::Type::Name[iType];

	return "unknown";
}

void CLuaInterface::CreateMetaTableType(const char* strName, int iType)
{
	LuaDebugPrint(1, "CLuaInterface::CreateMetaTableType(%s, %i)\n", strName, iType);
	int ret = luaL_newmetatable_type(state, strName, iType);
	if (ret && iType <= 254)
	{
		GarrysMod::Lua::ILuaObject* pObject = m_pMetaTables[iType];
		if (!pObject)
		{
			pObject = CreateObject();
			m_pMetaTables[iType] = pObject;
		}
		ToSimpleObject(pObject)->SetFromStack(-1, this);
	}

	//return ret;
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

static QAngle angle_fallback = QAngle(0, 0, 0);
const QAngle& CLuaInterface::GetAngle(int iStackPos)
{
	LuaDebugPrint(2, "CLuaInterface::GetAngle\n");
	ILuaBase::UserData* udata = GetUserdata(iStackPos);
	if (!udata || !udata->data || udata->type != GarrysMod::Lua::Type::Angle)
		return angle_fallback;

	return *(QAngle*)udata->data;
}

static Vector vector_fallback = Vector(0, 0, 0);
const Vector& CLuaInterface::GetVector(int iStackPos)
{
	LuaDebugPrint(2, "CLuaInterface::GetVector\n");
	ILuaBase::UserData* udata = GetUserdata(iStackPos);
	if (!udata || !udata->data || udata->type != GarrysMod::Lua::Type::Vector)
		return vector_fallback;

	return *(Vector*)udata->data;
}

void CLuaInterface::PushAngle(const QAngle& val)
{
	LuaDebugPrint(2, "CLuaInterface::PushAngle\n");
	
	ILuaBase::UserData* udata = NewUserdata(20); // Should we use PushUserType?
	*(QAngle*)udata->data = val;
	udata->type = GarrysMod::Lua::Type::Angle;

	if (PushMetaTable(GarrysMod::Lua::Type::Angle))
		SetMetaTable(-2);
}

void CLuaInterface::PushVector(const Vector& val)
{
	LuaDebugPrint(2, "CLuaInterface::PushVector\n");

	ILuaBase::UserData* udata = NewUserdata(20);
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
	LuaDebugPrint(1, "CLuaInterface::CreateMetaTable\n");
	//luaL_newmetatable_type(state, strName, -1);

	int ref = -1;
	GetField(LUA_REGISTRYINDEX, strName);
	if (IsType(-1, GarrysMod::Lua::Type::Table))
	{
		ref = ReferenceCreate();
	} else {
		Pop(1);
	}

	if (ref != -1)
	{
		ReferencePush(ref);
		ReferenceFree(ref);
		lua_getfield(state, -1, "MetaID");
		int metaID = (int)lua_tonumber(state, -1);
		lua_pop(state, 1);
		return metaID;
	} else {
		// Missing this logic in lua-shared, CreateMetaTable creates it if it's missing, just as its name would imply.
		luaL_newmetatable_type(state, strName, ++m_iMetaTableIDCounter);
		return m_iMetaTableIDCounter;
	}

	return 0;
}

bool CLuaInterface::PushMetaTable(int iType)
{
	LuaDebugPrint(2, "CLuaInterface::PushMetaTable %i\n", iType);
	if (iType <= 254)
	{
		GarrysMod::Lua::ILuaObject* pMetaObject = m_pMetaTables[iType];
		if (pMetaObject)
		{
			ToSimpleObject(pMetaObject)->Push(this);
			return true;
		}
	}

	return false;
}

void CLuaInterface::PushUserType(void* data, int iType)
{
	LuaDebugPrint(2, "CLuaInterface::PushUserType %i\n", iType);

	UserData* udata = NewUserdata(8);
	udata->data = data;
	udata->type = (unsigned char)iType;

	if (PushMetaTable(iType))
		SetMetaTable(-2);
}

void CLuaInterface::SetUserType(int iStackPos, void* data)
{
	LuaDebugPrint(2, "CLuaInterface::SetUserType\n");
	
	ILuaBase::UserData* udata = GetUserdata(iStackPos);
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
	POINT_UNREACHABLE;
}

bool CLuaInterface::Init( GarrysMod::Lua::ILuaGameCallback* callback, bool bIsServer )
{
	LuaDebugPrint(1, "CLuaInterface::Init Server: %s\n", bIsServer ? "Yes" : "No");
	m_pGameCallback = callback;
	m_pGlobal = CreateObject();

	for (int i=0; i<LUA_MAX_TEMP_OBJECTS;++i)
	{
		m_TempObjects[i] = CreateObject();
	}

	m_iMetaTableIDCounter = GarrysMod::Lua::Type::Type_Count;
	for (int i=0; i<=254; ++i)
	{
		m_pMetaTables[i] = NULL;
	}
	m_iCurrentTempObject = 0;

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

	int reference = -1;
	lua_getfield(state, LUA_GLOBALSINDEX, "require"); // Keep the original require function
	if (IsType(-1, GarrysMod::Lua::Type::Function))
	{
		reference = ReferenceCreate();
	} else {
		Pop(1);
	}

	DoStackCheck();

	lua_pushvalue(state, LUA_GLOBALSINDEX);
	ToSimpleObject(m_pGlobal)->SetFromStack(-1, this);
	Pop(1);

	if (reference != -1)
	{
		ReferencePush(reference);
		SetMember(Global(), "requiree");
		ReferenceFree(reference);
	}

	LuaDebugPrint(3, "Table? %s\n", ToSimpleObject(m_pGlobal)->isTable() ? "Yes" : "No");

	DoStackCheck();

	constexpr int pooledStrings = sizeof(g_PooledStrings) / sizeof(const char*);
	lua_createtable(state, pooledStrings, 0);
	
	int idx = 0;
	for(const char* str : g_PooledStrings)
	{
		++idx;
		PushNumber(idx);
		PushString(str);
		SetTable(-3);
	}

	m_pStringPool = CreateObject();
	ToSimpleObject(m_pStringPool)->SetFromStack(-1, this);
	Pop(1);

	DoStackCheck();


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
	if (!state)
	{
		Warning(PROJECT_NAME " - CLuaInterface::Shutdown called while having no lua state! Was it already called once?\n");
		return;
	}

	GMOD_UnloadBinaryModules(state);

	lua_close(state);
	state = NULL;
}

static int iLastTimeCheck = 0;
void CLuaInterface::Cycle()
{
	LuaDebugPrint(3, "CLuaInterface::Cycle\n");

	iLastTimeCheck = 0;
	// someotherValue = 0;
	// m_ProtectedFunctionReturns = NULL; // Why would we want this? Sounds like a possible memory leak.
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
	return m_pGlobal;
}

GarrysMod::Lua::ILuaObject* CLuaInterface::GetObject(int index)
{
	LuaDebugPrint(4, "CLuaInterface::GetObject\n");
	GarrysMod::Lua::ILuaObject* obj = CreateObject();
	ToSimpleObject(obj)->SetFromStack(index, this);

	return obj;
}

void CLuaInterface::PushLuaObject(GarrysMod::Lua::ILuaObject* obj)
{
	LuaDebugPrint(4, "CLuaInterface::PushLuaObject\n");
	if (obj)
	{
		ToSimpleObject(obj)->Push(this);
	} else {
		PushNil();
	}
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
		ErrorNoHalt("%s", str);
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
	if (!ThreadInMainThread())
		Error("Calling Lua function in a thread other than main!");

	if (rets > 4)
		Error("[CLuaInterface::Call] Expecting more returns than possible\n");

	for (int i=0; i<3; ++i)
	{
		m_ProtectedFunctionReturns[i] = nullptr;
	}

	if (IsType(-(args + 1), GarrysMod::Lua::Type::Function))
	{
		if (CallFunctionProtected(args, rets, true))
		{
			for (int i=0; i<rets; ++i)
			{
				GarrysMod::Lua::ILuaObject* obj = NewTemporaryObject();
				ToSimpleObject(obj)->SetFromStack(-1, this);
				m_ProtectedFunctionReturns[i] = obj;
				Pop(1);
			}
		}
	} else {
		Error("Lua tried to call non functions");
	}
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
	if (CallFunctionProtected(args, 1, 1)) {
		ToSimpleObject(obj)->SetFromStack(-1, this);
		Pop(1);
		return true;
	}

	return false;
}

void CLuaInterface::NewGlobalTable(const char* name)
{
	LuaDebugPrint(1, "CLuaInterface::NewGlobalTable %s\n", name);

	lua_createtable(state, 0, 0);
	lua_setfield(state, LUA_GLOBALSINDEX, name);
}

GarrysMod::Lua::ILuaObject* CLuaInterface::NewTemporaryObject()
{
	LuaDebugPrint(2, "CLuaInterface::NewTemporaryObject\n");

	++m_iCurrentTempObject;
	if (m_iCurrentTempObject >= LUA_MAX_TEMP_OBJECTS)
		m_iCurrentTempObject = 0;

	GarrysMod::Lua::ILuaObject* obj = m_TempObjects[m_iCurrentTempObject];
	if (obj)
	{
		ToSimpleObject(obj)->UnReference();
	} else {
		obj = CreateObject();
		m_TempObjects[m_iCurrentTempObject] = obj;
	}

	return obj;
}

bool CLuaInterface::isUserData(int iStackPos)
{
	LuaDebugPrint(4, "CLuaInterface::isUserData %i %s\n", iStackPos, lua_type(state, iStackPos) == GarrysMod::Lua::Type::UserData ? "Yes" : "No");

	return lua_type(state, iStackPos) == GarrysMod::Lua::Type::UserData;
}

GarrysMod::Lua::ILuaObject* CLuaInterface::GetMetaTableObject(const char* name, int type)
{
	LuaDebugPrint(2, "CLuaInterface::GetMetaTableObject %s, %i\n", name, type);

	lua_getfield(state, LUA_REGISTRYINDEX, name);
	if (GetType(-1) != 5)
	{
		Pop(1);
		if (type != -1)
		{
			CreateMetaTableType(name, type);
			lua_getfield(state, LUA_REGISTRYINDEX, name);
		} else {
			return NULL;
		}
	}

	GarrysMod::Lua::ILuaObject* obj = NewTemporaryObject();
	ToSimpleObject(obj)->SetFromStack(-1, this);
	Pop(1);
	return obj;
}

GarrysMod::Lua::ILuaObject* CLuaInterface::GetMetaTableObject(int iStackPos)
{
	LuaDebugPrint(2, "CLuaInterface::GetMetaTableObject\n");

	if (lua_getmetatable(state, iStackPos))
	{
		GarrysMod::Lua::ILuaObject* obj = NewTemporaryObject();
		ToSimpleObject(obj)->SetFromStack(-1, this);
		Pop(1);
		return obj;
	}

	return NULL;
}

GarrysMod::Lua::ILuaObject* CLuaInterface::GetReturn(int iStackPos)
{
	LuaDebugPrint(2, "CLuaInterface::GetReturn\n");
	
	int idx = abs(iStackPos);
	if (idx >= 0 && idx < 4)
	{
		if ( m_ProtectedFunctionReturns[idx] == NULL)
		{
			LuaDebugPrint(1, "CLuaInterface::GetReturn We could crash! (null object!)\n");
#ifdef WIN32
			__debugbreak();
#endif
		}

		return m_ProtectedFunctionReturns[idx];
	}

	LuaDebugPrint(1, "CLuaInterface::GetReturn We could crash! (invalid idx!)\n");
#ifdef WIN32
	__debugbreak();
#endif
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
	m_pGameCallback->DestroyLuaObject(obj);
}

GarrysMod::Lua::ILuaObject* CLuaInterface::CreateObject()
{
	LuaDebugPrint(4, "CLuaInterface::CreateObject\n");
	return m_pGameCallback->CreateLuaObject();
}

void CLuaInterface::SetMember(GarrysMod::Lua::ILuaObject* obj, GarrysMod::Lua::ILuaObject* key, GarrysMod::Lua::ILuaObject* value)
{
	LuaDebugPrint(3, "CLuaInterface::SetMember 1\n");
	ToSimpleObject(obj)->Push(this);
	ToSimpleObject(key)->Push(this);
	if (value)
		ToSimpleObject(value)->Push(this);
	else
		lua_pushnil(state);
	SetTable(-3);
	Pop(1);
}

GarrysMod::Lua::ILuaObject* CLuaInterface::GetNewTable()
{
	LuaDebugPrint(2, "CLuaInterface::GetNewTable\n");
	
	CreateTable();
	GarrysMod::Lua::ILuaObject* obj = CreateObject();
	ToSimpleObject(obj)->SetFromStack(-1, this);
	return obj;
}

void CLuaInterface::SetMember(GarrysMod::Lua::ILuaObject* obj, float key)
{
	LuaDebugPrint(3, "CLuaInterface::SetMember 2\n");
	ToSimpleObject(obj)->Push(this);
	PushNumber(key);
	lua_pushvalue(state, -3);
	SetTable(-3);
	Pop(2);
}

void CLuaInterface::SetMember(GarrysMod::Lua::ILuaObject* obj, float key, GarrysMod::Lua::ILuaObject* value)
{
	LuaDebugPrint(3, "CLuaInterface::SetMember 3 %f\n", key);
	ToSimpleObject(obj)->Push(this);
	PushNumber(key);
	if (value)
		ToSimpleObject(value)->Push(this);
	else
		lua_pushnil(state);
	SetTable(-3);
	Pop(1);
}

void CLuaInterface::SetMember(GarrysMod::Lua::ILuaObject* obj, const char* key)
{
	LuaDebugPrint(3, "CLuaInterface::SetMember 4 %s\n", key);
	ToSimpleObject(obj)->Push(this);
	PushString(key);
	lua_pushvalue(state, -3);
	SetTable(-3);
	Pop(2);
}

void CLuaInterface::SetMember(GarrysMod::Lua::ILuaObject* obj, const char* key, GarrysMod::Lua::ILuaObject* value)
{
	LuaDebugPrint(3, "CLuaInterface::SetMember 5 %s\n", key);
	ToSimpleObject(obj)->Push(this);
	PushString(key);
	if (value)
		ToSimpleObject(value)->Push(this);
	else
		lua_pushnil(state);
	SetTable(-3);
	Pop(1);
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

	if (!lua_getmetatable(state, iStackPos))
		return false;

	lua_pushvalue(state, keyIndex);
	GetTable(-2);
	if (lua_type(state, -1))
		return true;

	return false;
}

bool CLuaInterface::FindObjectOnTable(int iStackPos, int keyIndex)
{
	LuaDebugPrint(2, "CLuaInterface::FindObjectOnTable\n");
	lua_pushvalue(state, iStackPos);
	lua_pushvalue(state, keyIndex);

	return lua_type(state, -1) != 0;
}

void CLuaInterface::SetMemberFast(GarrysMod::Lua::ILuaObject* obj, int keyIndex, int valueIndex)
{
	LuaDebugPrint(3, "CLuaInterface::SetMemberFast %i %i %s\n", keyIndex, valueIndex, GetType(keyIndex) == GarrysMod::Lua::Type::String ? GetString(keyIndex) : "");
	CSimpleLuaObject* pObj = ToSimpleObject(obj);
	if (pObj->isTable() || pObj->GetType() == GarrysMod::Lua::Type::Table)
	{
		ReferencePush(pObj->m_reference);
		Push(keyIndex);
		Push(valueIndex);
		SetTable(-3);
		Pop(1);
	}
}

bool CLuaInterface::RunString(const char* filename, const char* path, const char* stringToRun, bool run, bool showErrors)
{
	LuaDebugPrint(2, "CLuaInterface::RunString\n");
	return RunStringEx(filename, path, stringToRun, run, showErrors, true, true);
}

bool CLuaInterface::IsEqual(GarrysMod::Lua::ILuaObject* objA, GarrysMod::Lua::ILuaObject* objB)
{
	LuaDebugPrint(2, "CLuaInterface::IsEqual\n");
	
	ToSimpleObject(objA)->Push(this);
	ToSimpleObject(objB)->Push(this);
	bool ret = Equal(-1, -2);
	Pop(2);

	return ret;
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

	std::string resultPath = path;
	if (lastSeparatorPos != std::string::npos)
	{
		resultPath = path.substr(0, lastSeparatorPos + 1);
	}

	if ( resultPath.find( "lua/" ) == 0 )
		resultPath.erase( 0, 4 );

	if ( resultPath.find( "gamemodes/" ) == 0 )
		resultPath.erase( 0, 10 );

    if (resultPath.rfind("addons/", 0) == 0) // ToDo: I think we can remove this again.
	{
		size_t first = path.find('/', 7);
		if (first != std::string::npos)
		{
			size_t second = path.find('/', first + 1);
			if (second != std::string::npos)
			{
				resultPath = resultPath.substr(second + 1);
			}
		}
	}

	return resultPath;
}

bool CLuaInterface::FindAndRunScript(const char *filename, bool run, bool showErrors, const char *stringToRun, bool noReturns)
{
	LuaDebugPrint(2, "CLuaInterface::FindAndRunScript %s, %s, %s, %s, %s\n", filename, run ? "Yes" : "No", showErrors ? "Yes" : "No", stringToRun, noReturns ? "Yes" : "No");

	bool bDataTable = ((std::string)filename).rfind("!lua", 0) == 0;
	GarrysMod::Lua::ILuaShared* shared = Lua::GetShared();
	std::string filePath = filename;
	if (GetPath())
	{
		std::string currentPath = GetPath();
		if (!currentPath.empty() && currentPath.back() != '/')
		{
			currentPath.append("/");
		}

		if ( filePath.rfind( currentPath, 0 ) != 0 )
		{
			filePath = GetPath();
			filePath.append( "/" );
			filePath.append( filename );
		}

		if ( filePath.find( "lua/" ) == 0 )
			filePath.erase( 0, 4 );

		if ( filePath.find( "gamemodes/" ) == 0 )
			filePath.erase( 0, 10 );
	}

	GarrysMod::Lua::LuaFile* file = shared->LoadFile(filePath.c_str(), m_sPathID, bDataTable, true);
	if (!file)
	{
		filePath = filename;
		file = shared->LoadFile(filename, m_sPathID, bDataTable, true);
	}

	bool ret = false;
	if (file)
	{
		PushPath(ToPath(filePath).c_str());
		ret = RunStringEx(filePath.c_str(), filePath.c_str(), file->GetContents(), true, showErrors, true, noReturns);
		PopPath();
	}

	if ( !file )
	{
		LuaDebugPrint( 1, "Failed to find Script %s!\n", filename );
	}

	return ret;
}

void CLuaInterface::SetPathID(const char* pathID)
{
	LuaDebugPrint(1, "CLuaInterface::SetPathID %s\n", pathID);
	V_strncpy(m_sPathID, pathID, sizeof(m_sPathID));
}

const char* CLuaInterface::GetPathID()
{
	LuaDebugPrint(5, "CLuaInterface::GetPathID\n");

	return m_sPathID;
}

void CLuaInterface::ErrorNoHalt( const char* fmt, ... )
{
	LuaDebugPrint(2, "CLuaInterface::ErrorNoHalt %s\n", fmt);

	GarrysMod::Lua::ILuaGameCallback::CLuaError* error = ReadStackIntoError(state);

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

	m_pGameCallback->LuaError(error);

	delete error; // Deconstuctor will delete our buffer.
	delete buffer; // Update: I'm a idiot, it won't since a std::string copies it
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

	int r, g, b, a = 0;
	CSimpleLuaObject* pObject = ToSimpleObject(GetObject(iStackPos));
	if (!pObject)
		return 0xFFFFFFFF;

	r = pObject->GetMemberInt("r", 255);
	g = pObject->GetMemberInt("g", 255);
	b = pObject->GetMemberInt("b", 255);
	a = pObject->GetMemberInt("a", 255);

	return (a << 24) | (r << 16) | (g << 8) | b;
}

GarrysMod::Lua::ILuaObject* CLuaInterface::PushColor(Color color)
{
	LuaDebugPrint(2, "CLuaInterface::PushColor\n");
	
	GarrysMod::Lua::ILuaObject* pOrigObject = CreateObject();
	CSimpleLuaObject* pObject = ToSimpleObject(pOrigObject);
	pObject->SetMember("r", color.r());
	pObject->SetMember("g", color.g());
	pObject->SetMember("b", color.b());
	pObject->SetMember("a", color.a());
	GarrysMod::Lua::ILuaObject* pMetaTable = GetMetaTableObject("Color", -1);
	if (pMetaTable)
		pObject->SetMetaTable(pMetaTable, this);

	pObject->Push(this);
	return pOrigObject;
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
	LuaDebugPrint(2, "CLuaInterface::RunStringEx %s, %s\n", filename, path);

	std::string code = RunMacros(stringToRun);
	int res = luaL_loadbuffer(state, code.c_str(), code.length(), filename);
	if (res != 0)
	{
		GarrysMod::Lua::ILuaGameCallback::CLuaError* err = ReadStackIntoError(state);
		if (dontPushErrors)
			Pop(1);

		if (printErrors)
			m_pGameCallback->LuaError(err);

		delete err;

		return false;
	} else {
		return CallFunctionProtected(0, 0, printErrors);
	}
}

size_t CLuaInterface::GetDataString(int iStackPos, const char **pOutput)
{
	LuaDebugPrint(2, "CLuaInterface::GetDataString\n");
	
	size_t length = 0;
	*pOutput = NULL;
	const char* pString = lua_tolstring(state, iStackPos, &length);
	if (!pString)
		return 0;

	*pOutput = pString;
	return length;
}

static char cMessageBuffer[4096]; // this is NOT thread safe! Anyways :3
void CLuaInterface::ErrorFromLua(const char *fmt, ...)
{
	LuaDebugPrint(2, "CLuaInterface::ErrorFromLua %s\n", fmt);
	GarrysMod::Lua::ILuaGameCallback::CLuaError* error = ReadStackIntoError(state);

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

	m_pGameCallback->LuaError(error);

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
		static char strOutput[512];
		V_snprintf( strOutput, sizeof(strOutput), "%s (line %i)", ar.source, ar.currentline );

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
	V_vsnprintf(cMessageBuffer, sizeof(cMessageBuffer), fmt, args);
	va_end(args);

	m_pGameCallback->MsgColour(cMessageBuffer, col);
}

void CLuaInterface::GetCurrentFile(std::string &outStr)
{
	LuaDebugPrint(2, "CLuaInterface::GetCurrentFile\n");

	lua_Debug ar;
	int level = 0;
	while (lua_getstack(state, level, &ar) != 0)
	{
		lua_getinfo(state, "S", &ar);
		if (ar.source && strcmp(ar.what, "C") != 0)
		{
			outStr.assign(ar.source);
			LuaDebugPrint(2, "CLuaInterface::GetCurrentFile %s\n", ar.source);
			return;
		}
		++level;
	}

	outStr = "!UNKNOWN";
	LuaDebugPrint(2, "CLuaInterface::GetCurrentFile %s\n", "!UNKNOWN (How dare you)");
}

int WriteToBuffer(lua_State* pState, const void *pData, size_t iSize, void* pBuffer)
{
	((Bootil::AutoBuffer*)pBuffer)->Write(pData, iSize);
	return 0;
}

bool CLuaInterface::CompileString(Bootil::Buffer& dumper, const std::string& stringToCompile)
{
	LuaDebugPrint(2, "CLuaInterface::CompileString\n");
	
	int loadResult = luaL_loadbufferx(state, stringToCompile.c_str(), stringToCompile.size(), "", "t");
	if (loadResult != 0)
	{
		Pop(1);
		return 0;
	}

	bool success = lua_dump(state, WriteToBuffer, &dumper) == 0;
	Pop(1);

	return success;
}

bool CLuaInterface::CallFunctionProtected(int iArgs, int iRets, bool showError)
{
	LuaDebugPrint(2, "CLuaInterface::CallFunctionProtected %i %i %s\n", iArgs, iRets, showError ? "Yes" : "no");

	/*for (int i=1;i <= Top(); ++i)
	{
		LuaDebugPrint(4, "Stack: %i, GarrysMod::Lua::Type::: %i\n", i, GetType(i));
	}*/

	if (GetType(-(iArgs + 1)) != GarrysMod::Lua::Type::Function)
	{
		Warning("[CLuaInterface::CallFunctionProtected] You betraid me. This is not a function :<\n");
		return false;
	}

	int ret = PCall(iArgs, iRets, 0);
	if (ret != 0)
	{
		GarrysMod::Lua::ILuaGameCallback::CLuaError* err = ReadStackIntoError(state);
		if (showError)
			m_pGameCallback->LuaError(err);

		delete err;
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

const char* CLuaInterface::GetActualTypeName(int typeID)
{
	LuaDebugPrint(4, "CLuaInterface::GetActualTypeName (%i)\n", typeID);

	return luaL_typename(state, typeID);
}

void CLuaInterface::PreCreateTable(int arrelems, int nonarrelems)
{
	LuaDebugPrint(4, "CLuaInterface::PreCreateTable %i %i\n", arrelems, nonarrelems);
	lua_createtable(state, arrelems, nonarrelems);
}

void CLuaInterface::PushPooledString(int index)
{
	LuaDebugPrint(2, "CLuaInterface::PushPooledString %i %s\n", index, g_PooledStrings[index]);
	
	ToSimpleObject(m_pStringPool)->Push(this);
	lua_rawgeti(state, -1, index + 1);
	lua_remove(state, -2);
}

const char* CLuaInterface::GetPooledString(int index)
{
	LuaDebugPrint(2, "CLuaInterface::GetPooledString\n");

	return g_PooledStrings[index];
}

void CLuaInterface::AppendStackTrace(char* pOutput, unsigned int iOutputLength)
{
	LuaDebugPrint(2, "CLuaInterface::AppendStackTrace\n");
	
	if (!state)
	{
		V_strncat(pOutput, "   Lua State = NULL\n\n", iOutputLength, -1);
		return;
	}

	lua_Debug ar;
	int iStackLevel = 0;
	while (lua_getstack(state, iStackLevel, &ar))
	{
		lua_getinfo(state, "Slnu", &ar);

		char lineBuffer[256] = {0};
		V_snprintf(lineBuffer, sizeof(lineBuffer), "%d. %s - %s:%d\n", iStackLevel, ar.name, ar.short_src, ar.currentline);
		for (int i = 0; i <= iStackLevel; ++i)
			V_strncat(pOutput, "  ", iOutputLength, -1);

		V_strncat(pOutput, lineBuffer, iOutputLength, -1);

		if (++iStackLevel == 17)
			break;
	}

	if (iStackLevel == 0)
		V_strncat(pOutput, "\t*Not in Lua call OR Lua has panicked*\n", iOutputLength, -1);

	V_strncat(pOutput, "\n", iOutputLength, -1);
}

#ifndef FCVAR_LUA_CLIENT // 64x fun
static constexpr int FCVAR_LUA_CLIENT = (1 << 18);
static constexpr int FCVAR_LUA_SERVER = (1 << 19);
#endif
int CLuaInterface::FilterConVarFlags(int& flags)
{
	flags &= ~(FCVAR_GAMEDLL | FCVAR_CLIENTDLL | FCVAR_LUA_CLIENT); // Check if FCVAR_RELEASE is added on 64x

	if (IsServer())
	{
		flags |= FCVAR_GAMEDLL | FCVAR_LUA_SERVER;
	}

	if (IsClient())
	{
		flags |= FCVAR_CLIENTDLL | FCVAR_SERVER_CAN_EXECUTE | FCVAR_LUA_CLIENT;
	}

	if (IsMenu())
	{
		flags &= ~FCVAR_ARCHIVE;
	}

	return IsMenu();
}

void* CLuaInterface::CreateConVar(const char* name, const char* defaultValue, const char* helpString, int flags)
{
	LuaDebugPrint(2, "CLuaInterface::CreateConVar\n");

	FilterConVarFlags(flags);

	Warning("holylib - CLuaInterface::CreateConVar was NOT implemented yet!\n"); // This function requires changes to work properly to support multiple states
	return NULL;// LuaConVars()->CreateConVar(name, defaultValue, helpString, flags);
}

void* CLuaInterface::CreateConCommand(const char* name, const char* helpString, int flags, FnCommandCallback_t callback, FnCommandCompletionCallback completionFunc)
{
	LuaDebugPrint(2, "CLuaInterface::CreateConCommand\n");

	FilterConVarFlags(flags);
	if (IsServer())
		flags |= FCVAR_CLIENTCMD_CAN_EXECUTE;

	Warning("holylib - CLuaInterface::CreateConCommand was NOT implemented yet!\n");
	return NULL; //return LuaConVars()->CreateConCommand(name, helpString, flags, callback, completionFunc);
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

	Bootil::String::Util::FindAndReplace(code, "DEFINE_BASECLASS", "local BaseClass = baseclass.Get");

	return code;
}

int CLuaInterface::RegisterMetaTable( const char* name, GarrysMod::Lua::ILuaObject* metaObj )
{
	lua_getfield(state, LUA_REGISTRYINDEX, name);
	if (GetType(-1) == 0)
	{
		Pop(1);
		int metaID = m_iMetaTableIDCounter++;
		CSimpleLuaObject* pMetaObject = ToSimpleObject(metaObj);
		pMetaObject->SetMember("MetaID", metaID); // Gmod uses SetMember_FixKey
		pMetaObject->SetMember("MetaName", name);

		pMetaObject->Push(this);
		lua_setfield(state, LUA_REGISTRYINDEX, name);
		lua_getfield(state, LUA_REGISTRYINDEX, name);
	}

	if (GetType(-1) == GarrysMod::Lua::Type::Table)
	{
		lua_getfield(state, -1, "MetaID");
		int id = (int)lua_tonumber(state, -1);
		lua_settop(state, -2);

		return id;
	}

	return -1;
}

#define DLL_TOOLS
#include "detours.h"
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

	void* hDll = DLL_LoadModule(name, RTLD_LAZY);
	if (hDll == NULL) {
		lua_pushliteral(L, "Failed to load dll!");
		lua_error(L);
		return;
	}

	GarrysMod::Lua::CFunc gmod13_open = (GarrysMod::Lua::CFunc)DLL_GetAddress(hDll, "gmod13_open");
	if (gmod13_open == NULL) {
		lua_pushliteral(L, "Failed to get gmod13_open!");
		lua_error(L);
		DLL_UnloadModule(hDll);
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
		GarrysMod::Lua::CFunc gmod13_close = (GarrysMod::Lua::CFunc)DLL_GetAddress(udata->data, "gmod13_close");
		if (gmod13_close != NULL) {
			lua_pushcclosure(L, gmod13_close, 0);
			lua_call(L, 0, 0);
		}

		DLL_UnloadModule(udata->data);
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