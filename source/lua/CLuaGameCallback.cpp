#include "CLuaInterface.h"
#include <GarrysMod/Lua/LuaGameCallback.h>
#include "color.h"
#include <sstream>
#include <string>
#include "Platform.hpp"
#include "lua.h"

#if !HOLYLIB_BUILD_RELEASE && SYSTEM_WINDOWS
__declspec(dllexport) class [[nodiscard]] CLuaObject : GarrysMod::Lua::ILuaObject // For IDA as I need a vtable and its members to use when comparing stuff
{
public:
	virtual void Set( ILuaObject *obj );
	virtual void SetFromStack( int i );
	virtual void UnReference( );

	virtual int GetType( );
	virtual const char *GetString( );
	virtual float GetFloat( );
	virtual int GetInt( );
	virtual void *GetUserData( );

	virtual void SetMember( const char *name );
	virtual void SetMember( const char *name, ILuaObject *obj );
	virtual void SetMember( const char *name, float val );
	virtual void SetMember( const char *name, bool val );
	virtual void SetMember( const char *name, const char *val );
	virtual void SetMember( const char *name, GarrysMod::Lua::CFunc f );

	virtual bool GetMemberBool( const char *name, bool b = true );
	virtual int GetMemberInt( const char *name, int i = 0 );
	virtual float GetMemberFloat( const char *name, float f = 0.0f );
	virtual const char *GetMemberStr( const char *name, const char *s = "" );
	virtual void *GetMemberUserData( const char *name, void *u = 0 );
	virtual void *GetMemberUserData( float name, void *u = 0 );
	virtual ILuaObject *GetMember( const char *name, ILuaObject *obj );
	virtual ILuaObject *GetMember( ILuaObject *key, ILuaObject *obj );

	virtual void SetMetaTable( ILuaObject *obj );
	virtual void SetUserData( void *obj );

	virtual void Push( );

	virtual bool isNil( );
	virtual bool isTable( );
	virtual bool isString( );
	virtual bool isNumber( );
	virtual bool isFunction( );
	virtual bool isUserData( );

	virtual ILuaObject *GetMember( float fKey, ILuaObject* obj );

	virtual void *Remove_Me_1( const char *name, void * = 0 );

	virtual void SetMember( float fKey );
	virtual void SetMember( float fKey, ILuaObject *obj );
	virtual void SetMember( float fKey, float val );
	virtual void SetMember( float fKey, bool val );
	virtual void SetMember( float fKey, const char *val );
	virtual void SetMember( float fKey, GarrysMod::Lua::CFunc f );

	virtual const char *GetMemberStr( float name, const char *s = "" );

	virtual void SetMember( ILuaObject *k, ILuaObject *v );
	virtual bool GetBool( );

	virtual bool PushMemberFast( int iStackPos );
	virtual void SetMemberFast( int iKey, int iValue );

	virtual void SetFloat( float val );
	virtual void SetString( const char *val );

	virtual double GetDouble( );

	virtual void SetMember_FixKey( const char *, float );
	virtual void SetMember_FixKey( const char *, const char * );
	virtual void SetMember_FixKey( const char *, ILuaObject * );
	virtual void SetMember_FixKey( const char *, double );
	virtual void SetMember_FixKey( const char *, int );

	virtual bool isBool( );

	virtual void SetMemberDouble( const char *, double );

	virtual void SetMemberNil( const char * );
	virtual void SetMemberNil( float );

	virtual bool RemoveMe( );

	virtual void Init( );

	virtual void SetFromGlobal( const char * );

	virtual int GetStringLen( unsigned int * );

	virtual unsigned int GetMemberUInt( const char *, unsigned int );

	virtual void SetMember( const char *, unsigned long long );
	virtual void SetMember( const char *, int );
	virtual void SetReference( int );

	virtual void RemoveMember( const char * );
	virtual void RemoveMember( float );

	virtual bool MemberIsNil( const char * );

	virtual void SetMemberDouble( float, double );
	virtual double GetMemberDouble( const char *, double );

	virtual BaseEntity *GetMemberEntity( const char *, BaseEntity * );
	virtual void SetMemberEntity( float, BaseEntity * );
	virtual void SetMemberEntity( const char *, BaseEntity * );
	virtual bool isEntity( );
	virtual BaseEntity *GetEntity( );
	virtual void SetEntity( BaseEntity * );

	virtual void SetMemberVector( const char *, Vector * );
	virtual void SetMemberVector( const char *, Vector & );
	virtual void SetMemberVector( float, Vector * );
	virtual Vector *GetMemberVector( const char *, const Vector * );
	virtual Vector *GetMemberVector( int );
	virtual Vector *GetVector( );
	virtual bool isVector( );

	virtual void SetMemberAngle( const char *, QAngle * );
	virtual void SetMemberAngle( const char *, QAngle & );
	virtual QAngle *GetMemberAngle( const char *, QAngle * );
	virtual QAngle *GetAngle( );
	virtual bool isAngle( );

	virtual void SetMemberMatrix( const char *, VMatrix const * );
	virtual void SetMemberMatrix( const char *, VMatrix const & );
	virtual void SetMemberMatrix( float, VMatrix const * );
	virtual void SetMemberMatrix( int, VMatrix const * );

	virtual void SetMemberPhysObject( const char *, IPhysicsObject * );
public:
	void Init(GarrysMod::Lua::ILuaBase*);
protected:
	bool m_bUserData;
	int m_iLUA_TYPE;
	int m_reference;
	GarrysMod::Lua::ILuaBase* m_pLua;
protected:
	int m_metatable; // Verify: This real? Why did I add it? Where did this come from? Magic
};
#endif

class CLuaGameCallback : public GarrysMod::Lua::ILuaGameCallback
{
public:
	virtual GarrysMod::Lua::ILuaObject* CreateLuaObject();
	virtual void DestroyLuaObject(GarrysMod::Lua::ILuaObject* pObject);
	virtual void ErrorPrint(const char* error, bool print);
	virtual void Msg(const char* msg, bool unknown);
	virtual void MsgColour(const char* msg, const Color& color);
	virtual void LuaError(const CLuaError* error);
	virtual void InterfaceCreated(GarrysMod::Lua::ILuaInterface* iface);
};

static CLuaGameCallback g_pLuaGameCallback;

GarrysMod::Lua::ILuaGameCallback* Lua::GetLuaGameCallback()
{
	return &g_pLuaGameCallback;
}

#ifdef CLIENT_DLL
Color col_msg(255, 241, 122, 200);
Color col_error(255, 221, 102, 255);
#elif defined(MENUSYSTEM)
Color col_msg(100, 220, 100, 200);
Color col_error(120, 220, 100, 255);
#else
Color col_msg(156, 241, 255, 200);
Color col_error(136, 221, 255, 255);
#endif

/*void UTLVarArgs(char* buffer, const char* format, ...) {
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, strlen(buffer), format, args);
	va_end(args);
}*/

// Time for yet ANOTHER shitty workaround.
// This should be considered a warcrime at this point.
struct UnholyVTable
{
	// I think it only has like 50 virtual functions, but we take up more space just to be safe.
	void* entry[128] = {NULL}; // Hopefully this should be enouth for years to come.
};
UnholyVTable g_pGlobalObjctVTable;
class UnholyLuaObject
{
public:
	void* m_pVTABLE = &g_pGlobalObjctVTable; // Vtable offset as else our member variables are all invalid.
	bool m_bUserData;
	int m_iLUA_TYPE;
	int m_reference;
	GarrysMod::Lua::ILuaInterface* m_pLua;
};
void SetupUnHolyVTableForThisShit(GarrysMod::Lua::ILuaInterface* pLua)
{
	GarrysMod::Lua::ILuaObject* pObject = pLua->NewTemporaryObject();
	memcpy(&g_pGlobalObjctVTable, *(void**)pObject, sizeof(UnholyVTable));
	// Yes, we just copy the entire thing.
	// This is cursed, but it allows us to update the vtable without having to change every single ILuaObject created before this function was called.
	// By default all vtable functions are NULL since they shouldn't be used that early anyways.
}

GarrysMod::Lua::ILuaObject* CLuaGameCallback::CreateLuaObject()
{
	return static_cast<GarrysMod::Lua::ILuaObject*>(static_cast<void*>(new UnholyLuaObject()));
}

void CLuaGameCallback::DestroyLuaObject(GarrysMod::Lua::ILuaObject* pObject)
{
	UnholyLuaObject* pUnholyObject = static_cast<UnholyLuaObject*>(static_cast<void*>(pObject));
	if (pUnholyObject->m_pVTABLE == &g_pGlobalObjctVTable) // Ensure its one of our unholy instances.
	{
		delete pUnholyObject;
	} else {
		if (!g_Lua)
			Error("CLuaGameCallback::DestroyLuaObject called while no valid lua instance existed!\n");

		g_Lua->DestroyObject(pObject);
	}
}

void CLuaGameCallback::ErrorPrint(const char* error, bool print)
{
	// Write into the lua_errors_server.txt if error logging is enabled.

	Color ErrorColor = col_error; // ToDo: Change this later and finish this function.
	ConColorMsg(ErrorColor, "%s\n", error);
}

void CLuaGameCallback::Msg(const char* msg, bool unknown)
{
	MsgColour(msg, col_msg);
}

void CLuaGameCallback::MsgColour(const char* msg, const Color& color)
{
	ConColorMsg(color, "%s", msg);
}

void CLuaGameCallback::LuaError(const CLuaError* error)
{
	std::stringstream str;

	str << "[ERROR] ";
	str << error->message;
	str << "\n";

	int i = 0;
	for (CLuaError::StackEntry entry : error->stack)
	{
		++i;
		for (int j=-1;j<i;++j)
		{
			str << " ";
		}

		str << i;
		str << ". ";
		str << entry.function;
		str << " - ";
		str << entry.source;
		str << ":";
		str << entry.line;
		str << "\n";
	}

	MsgColour(str.str().c_str(), col_error);
}

void CLuaGameCallback::InterfaceCreated(GarrysMod::Lua::ILuaInterface* iface) {} // Unused

static CLuaGameCallback gamecallback;
GarrysMod::Lua::ILuaGameCallback* g_LuaCallback = &gamecallback;