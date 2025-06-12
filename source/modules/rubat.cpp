#include "LuaInterface.h"
#include "module.h"
#include "lua.h"
#include <vmatrix.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CRubatModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual const char* Name() { return "rubat"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
	virtual bool IsEnabledByDefault() { return false; }; // This has no code at all.
};

static CRubatModule g_pRubatModule;
IModule* pRubatModule = &g_pRubatModule;

class BaseEntity;
class GMOD_ILuaObject
{
public:
	virtual void Set( GarrysMod::Lua::ILuaObject *obj ) = 0;
	virtual void SetFromStack( int i ) = 0;
	virtual void UnReference( ) = 0;

	virtual int GetType( ) = 0;
	virtual const char *GetString( ) = 0;
	virtual float GetFloat( ) = 0;
	virtual int GetInt( ) = 0;
	virtual void *GetUserData( ) = 0;

	virtual void SetMember( const char *name ) = 0;
	virtual void SetMember( const char *name, GarrysMod::Lua::ILuaObject *obj ) = 0;
	virtual void SetMember( const char *name, float val ) = 0;
	virtual void SetMember( const char *name, bool val ) = 0;
	virtual void SetMember( const char *name, const char *val ) = 0;
	virtual void SetMember( const char *name, GarrysMod::Lua::CFunc f ) = 0;

	virtual bool GetMemberBool( const char *name, bool b = true ) = 0;
	virtual int GetMemberInt( const char *name, int i = 0 ) = 0;
	virtual float GetMemberFloat( const char *name, float f = 0.0f ) = 0;
	virtual const char *GetMemberStr( const char *name, const char *s = "" ) = 0;
	virtual void *GetMemberUserData( const char *name, void *u = 0 ) = 0;
	virtual void *GetMemberUserData( float name, void *u = 0 ) = 0;
	virtual GarrysMod::Lua::ILuaObject *GetMember( const char *name, GarrysMod::Lua::ILuaObject *obj ) = 0;
	virtual GarrysMod::Lua::ILuaObject *GetMember( GarrysMod::Lua::ILuaObject *key, GarrysMod::Lua::ILuaObject *obj ) = 0;

	virtual void SetMetaTable( GarrysMod::Lua::ILuaObject *obj ) = 0;
	virtual void SetUserData( void *obj ) = 0;

	virtual void Push( ) = 0;

	virtual bool isNil( ) = 0;
	virtual bool isTable( ) = 0;
	virtual bool isString( ) = 0;
	virtual bool isNumber( ) = 0;
	virtual bool isFunction( ) = 0;
	virtual bool isUserData( ) = 0;

	virtual GarrysMod::Lua::ILuaObject *GetMember( float fKey, GarrysMod::Lua::ILuaObject* obj ) = 0;

	virtual void *Remove_Me_1( const char *name, void * = 0 ) = 0;

	virtual void SetMember( float fKey ) = 0;
	virtual void SetMember( float fKey, GarrysMod::Lua::ILuaObject *obj ) = 0;
	virtual void SetMember( float fKey, float val ) = 0;
	virtual void SetMember( float fKey, bool val ) = 0;
	virtual void SetMember( float fKey, const char *val ) = 0;
	virtual void SetMember( float fKey, GarrysMod::Lua::CFunc f ) = 0;

	virtual const char *GetMemberStr( float name, const char *s = "" ) = 0;

	virtual void SetMember( GarrysMod::Lua::ILuaObject *k, GarrysMod::Lua::ILuaObject *v ) = 0;
	virtual bool GetBool( ) = 0;

	virtual bool PushMemberFast( int iStackPos ) = 0;
	virtual void SetMemberFast( int iKey, int iValue ) = 0;

	virtual void SetFloat( float val ) = 0;
	virtual void SetString( const char *val ) = 0;

	virtual double GetDouble( ) = 0;

	virtual void SetMember_FixKey( const char *, float ) = 0;
	virtual void SetMember_FixKey( const char *, const char * ) = 0;
	virtual void SetMember_FixKey( const char *, GarrysMod::Lua::ILuaObject * ) = 0;
	virtual void SetMember_FixKey( const char *, double ) = 0;
	virtual void SetMember_FixKey( const char *, int ) = 0;

	virtual bool isBool( ) = 0;

	virtual void SetMemberDouble( const char *, double ) = 0;

	virtual void SetMemberNil( const char * ) = 0;
	virtual void SetMemberNil( float ) = 0;

	virtual bool RemoveMe( ) = 0;

	virtual void Init( ) = 0;

	virtual void SetFromGlobal( const char * ) = 0;

	virtual int GetStringLen( unsigned int * ) = 0;

	virtual unsigned int GetMemberUInt( const char *, unsigned int ) = 0;

	virtual void SetMember( const char *, unsigned long long ) = 0;
	virtual void SetMember( const char *, int ) = 0;
	virtual void SetReference( int ) = 0;

	virtual void RemoveMember( const char * ) = 0;
	virtual void RemoveMember( float ) = 0;

	virtual bool MemberIsNil( const char * ) = 0;

	virtual void SetMemberDouble( float, double ) = 0;
	virtual double GetMemberDouble( const char *, double ) = 0;
	// NOTE: All members below do NOT exist in ILuaObjects returned from the menusystem!

	virtual BaseEntity *GetMemberEntity( const char *, BaseEntity * ) = 0;
	virtual void SetMemberEntity( float, BaseEntity * ) = 0;
	virtual void SetMemberEntity( const char *, BaseEntity * ) = 0;
	virtual bool isEntity( ) = 0;
	virtual BaseEntity *GetEntity( ) = 0;
	virtual void SetEntity( BaseEntity * ) = 0;

	virtual void SetMemberVector( const char *, Vector * ) = 0;
	virtual void SetMemberVector( const char *, Vector & ) = 0;
	virtual void SetMemberVector( float, Vector * ) = 0;
	virtual Vector *GetMemberVector( const char *, const Vector * ) = 0;
	virtual Vector *GetMemberVector( int ) = 0;
	virtual Vector *GetVector( ) = 0;
	virtual bool isVector( ) = 0;

	virtual void SetMemberAngle( const char *, QAngle * ) = 0;
	virtual void SetMemberAngle( const char *, QAngle & ) = 0;
	virtual QAngle *GetMemberAngle( const char *, QAngle * ) = 0;
	virtual QAngle *GetAngle( ) = 0;
	virtual bool isAngle( ) = 0;

	virtual void SetMemberMatrix( const char *, VMatrix const * ) = 0;
	virtual void SetMemberMatrix( const char *, VMatrix const & ) = 0;
	virtual void SetMemberMatrix( float, VMatrix const * ) = 0;
	virtual void SetMemberMatrix( int, VMatrix const * ) = 0;

	virtual void SetMemberPhysObject( const char *, IPhysicsObject * ) = 0;
};

void CRubatModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	// Using our own defined class so that the sdk cannot be blamed for differences between platforms.
	GMOD_ILuaObject* pObject = static_cast<GMOD_ILuaObject*>(static_cast<void*>(pLua->NewTemporaryObject()));

	pLua->PushString("Hello World");
	pObject->SetFromStack(-1);
	pLua->Pop(1);

	int iTop = pLua->Top();
	pObject->Push();
	if (iTop == pLua->Top())
	{
		Msg("Failed to push value!\n");
		pLua->PushString("Failed to push!");
	}

	pLua->SetField(GarrysMod::Lua::INDEX_GLOBAL, "ILuaObject_Test");
}