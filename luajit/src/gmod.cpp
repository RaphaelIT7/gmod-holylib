extern "C"
{
	#include "gmod.h"
	#include "lua.h"
}
#include "stdio.h"

namespace std
{
	class string;
}

#define COMMAND_COMPLETION_MAXITEMS		128
#define COMMAND_COMPLETION_ITEM_LENGTH	128
typedef int  ( *FnCommandCompletionCallback )( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] );

class Vector;
class QAngle;
class Color;
class lua_Debug;
class CCommand;
namespace Bootil
{
	class Buffer;
}

namespace GarrysMod
{
    namespace Lua
    {
		class ILuaThreadedCall;
		class ILuaGameCallback;
		class ILuaObject;
        typedef int ( *CFunc )( lua_State* L );

        class ILuaBase
        {
        public:
            struct UserData
            {
                void*         data;
                unsigned char type;
            };

        protected:
            template <class T>
            struct UserData_Value : UserData
            {
                T value;
            };

        public:
            virtual int         Top( void ) = 0;
            virtual void        Push( int iStackPos ) = 0;
            virtual void        Pop( int iAmt = 1 ) = 0;
            virtual void        GetTable( int iStackPos ) = 0;
            virtual void        GetField( int iStackPos, const char* strName ) = 0;
            virtual void        SetField( int iStackPos, const char* strName ) = 0;
            virtual void        CreateTable() = 0;
            virtual void        SetTable( int iStackPos ) = 0;
            virtual void        SetMetaTable( int iStackPos ) = 0;
            virtual bool        GetMetaTable( int i ) = 0;
            virtual void        Call( int iArgs, int iResults ) = 0;
            virtual int         PCall( int iArgs, int iResults, int iErrorFunc ) = 0;
            virtual int         Equal( int iA, int iB ) = 0;
            virtual int         RawEqual( int iA, int iB ) = 0;
            virtual void        Insert( int iStackPos ) = 0;
            virtual void        Remove( int iStackPos ) = 0;
            virtual int         Next( int iStackPos ) = 0;
            virtual void*       NewUserdata( unsigned int iSize ) = 0;
            [[noreturn]]
            virtual void        ThrowError( const char* strError ) = 0;
            virtual void        CheckType( int iStackPos, int iType ) = 0;
            [[noreturn]]
            virtual void        ArgError( int iArgNum, const char* strMessage ) = 0;
            virtual void        RawGet( int iStackPos ) = 0;
            virtual void        RawSet( int iStackPos ) = 0;
            virtual const char* GetString( int iStackPos = -1, unsigned int* iOutLen = nullptr ) = 0;
            virtual double      GetNumber( int iStackPos = -1 ) = 0;
            virtual bool        GetBool( int iStackPos = -1 ) = 0;
            virtual CFunc       GetCFunction( int iStackPos = -1 ) = 0;
            virtual void*       GetUserdata( int iStackPos = -1 ) = 0;
            virtual void        PushNil() = 0;
            virtual void        PushString( const char* val, unsigned int iLen = 0 ) = 0;
            virtual void        PushNumber( double val ) = 0;
            virtual void        PushBool( bool val ) = 0;
            virtual void        PushCFunction( CFunc val ) = 0;
            virtual void        PushCClosure( CFunc val, int iVars ) = 0;
            virtual void        PushUserdata( void* ) = 0;
            virtual int         ReferenceCreate() = 0;
            virtual void        ReferenceFree( int i ) = 0;
            virtual void        ReferencePush( int i ) = 0;
            virtual void        PushSpecial( int iType ) = 0;
            virtual bool        IsType( int iStackPos, int iType ) = 0;
            virtual int         GetType( int iStackPos ) = 0;
            virtual const char* GetTypeName( int iType ) = 0;
            virtual void        CreateMetaTableType( const char* strName, int iType ) = 0;
            virtual const char* CheckString( int iStackPos = -1 ) = 0;
            virtual double      CheckNumber( int iStackPos = -1 ) = 0;
            virtual int         ObjLen( int iStackPos = -1 ) = 0;
            virtual const QAngle& GetAngle( int iStackPos = -1 ) = 0;
            virtual const Vector& GetVector( int iStackPos = -1 ) = 0;
            virtual void        PushAngle( const QAngle& val ) = 0;
            virtual void        PushVector( const Vector& val ) = 0;
            virtual void        SetState( lua_State* L ) = 0;
            virtual int         CreateMetaTable( const char* strName ) = 0;
            virtual bool        PushMetaTable( int iType ) = 0;
            virtual void        PushUserType( void* data, int iType ) = 0;
            virtual void        SetUserType( int iStackPos, void* data ) = 0;
        private:
            lua_State *state;
        };

		class ILuaInterface : public ILuaBase
		{
		public:
			virtual bool Init( ILuaGameCallback *, bool ) = 0;
			virtual void Shutdown( ) = 0;
			virtual void Cycle( ) = 0;
			virtual ILuaObject *Global( ) = 0;
			virtual ILuaObject *GetObject( int index ) = 0;
			virtual void PushLuaObject( ILuaObject *obj ) = 0;
			virtual void PushLuaFunction( CFunc func ) = 0;
			virtual void LuaError( const char *err, int index ) = 0;
			virtual void TypeError( const char *name, int index ) = 0;
			virtual void CallInternal( int args, int rets ) = 0;
			virtual void CallInternalNoReturns( int args ) = 0;
			virtual bool CallInternalGetBool( int args ) = 0;
			virtual const char *CallInternalGetString( int args ) = 0;
			virtual bool CallInternalGet( int args, ILuaObject *obj ) = 0;
			virtual void NewGlobalTable( const char *name ) = 0;
			virtual ILuaObject *NewTemporaryObject( ) = 0;
			virtual bool isUserData( int index ) = 0;
			virtual ILuaObject *GetMetaTableObject( const char *name, int type ) = 0;
			virtual ILuaObject *GetMetaTableObject( int index ) = 0;
			virtual ILuaObject *GetReturn( int index ) = 0;
			virtual bool IsServer( ) = 0;
			virtual bool IsClient( ) = 0;
			virtual bool IsMenu( ) = 0;
			virtual void DestroyObject( ILuaObject *obj ) = 0;
			virtual ILuaObject *CreateObject( ) = 0;
			virtual void SetMember( ILuaObject *table, ILuaObject *key, ILuaObject *value ) = 0;
			virtual void GetNewTable( ) = 0;
			virtual void SetMember( ILuaObject *table, float key ) = 0;
			virtual void SetMember( ILuaObject *table, float key, ILuaObject *value ) = 0;
			virtual void SetMember( ILuaObject *table, const char *key ) = 0;
			virtual void SetMember( ILuaObject *table, const char *key, ILuaObject *value ) = 0;
			virtual void SetType( unsigned char ) = 0;
			virtual void PushLong( long num ) = 0;
			virtual int GetFlags( int index ) = 0;
			virtual bool FindOnObjectsMetaTable( int objIndex, int keyIndex ) = 0;
			virtual bool FindObjectOnTable( int tableIndex, int keyIndex ) = 0;
			virtual void SetMemberFast( ILuaObject *table, int keyIndex, int valueIndex ) = 0;
			virtual bool RunString( const char *filename, const char *path, const char *stringToRun, bool run, bool showErrors ) = 0;
			virtual bool IsEqual( ILuaObject *objA, ILuaObject *objB ) = 0;
			virtual void Error( const char *err ) = 0;
			virtual const char *GetStringOrError( int index ) = 0;
			virtual bool RunLuaModule( const char *name ) = 0;
			virtual bool FindAndRunScript( const char *filename, bool run, bool showErrors, const char *stringToRun, bool noReturns ) = 0;
			virtual void SetPathID( const char *pathID ) = 0;
			virtual const char *GetPathID( ) = 0;
			virtual void ErrorNoHalt( const char *fmt, ... ) = 0;
			virtual void Msg( const char *fmt, ... ) = 0;
			virtual void PushPath( const char *path ) = 0;
			virtual void PopPath( ) = 0;
			virtual const char *GetPath( ) = 0;
			virtual int GetColor( int index ) = 0;
			virtual void PushColor( Color color ) = 0;
			virtual int GetStack( int level, lua_Debug *dbg ) = 0;
			virtual int GetInfo( const char *what, lua_Debug *dbg ) = 0;
			virtual const char *GetLocal( lua_Debug *dbg, int n ) = 0;
			virtual const char *GetUpvalue( int funcIndex, int n ) = 0;
			virtual bool RunStringEx( const char *filename, const char *path, const char *stringToRun, bool run, bool printErrors, bool dontPushErrors, bool noReturns ) = 0;
			virtual size_t GetDataString( int index, const char **str ) = 0;
			virtual void ErrorFromLua( const char *fmt, ... ) = 0;
			virtual const char *GetCurrentLocation( ) = 0;
			virtual void MsgColour( const Color &col, const char *fmt, ... ) = 0;
			virtual void GetCurrentFile( std::string &outStr ) = 0;
			virtual void CompileString( Bootil::Buffer &dumper, const std::string &stringToCompile ) = 0;
			virtual bool CallFunctionProtected( int, int, bool ) = 0;
			virtual void Require( const char *name ) = 0;
			virtual const char *GetActualTypeName( int type ) = 0;
			virtual void PreCreateTable( int arrelems, int nonarrelems ) = 0;
			virtual void PushPooledString( int index ) = 0;
			virtual const char *GetPooledString( int index ) = 0;
			virtual void *AddThreadedCall( ILuaThreadedCall * ) = 0;
			virtual void AppendStackTrace( char *, unsigned long ) = 0;
			virtual void *CreateConVar( const char *, const char *, const char *, int ) = 0;
			virtual void *CreateConCommand( const char *, const char *, int, void ( * )( const CCommand & ), FnCommandCompletionCallback ) = 0;
			virtual const char* CheckStringOpt( int iStackPos, const char* def );
			virtual double CheckNumberOpt( int iStackPos, double def );
			virtual void RegisterMetaTable( const char* name, ILuaObject* obj );
		};
    }
}

using namespace GarrysMod::Lua;
extern "C" void lua_init_stack_gmod(lua_State* L1, lua_State* L)
{
    if (L && L != L1)
	{
		L1->luabase = L->luabase;
		if (L->luabase)
			((ILuaBase*)L->luabase)->SetState(L);
	}
}

extern "C" void GMOD_LuaPrint(const char* str, lua_State* L) // Should be how gmod does it
{
	if (!L->luabase) // except for this, gmod doesn't do this, but we do making testing jit less of a pain
	{
		printf(str);
		return;
	}

	((ILuaInterface*)L->luabase)->Msg("%s", str);
}

struct UserData {
	void* data;
	unsigned char type;
};

extern "C" void GMOD_LuaCreateEmptyUserdata(lua_State* L)
{
	//ILuaBase::UserData* udata = (ILuaBase::UserData*)((ILuaBase*)L->luabase)->NewUserdata(sizeof(ILuaBase::UserData)); // Gmod uses CLuaInterface::PushUserType(NULL, 7) Instead
	//udata->data = nullptr;
	//udata->type = 7; // 7 = Userdata

	//return udata;

	if (!L->luabase) // just to make testing easier. Gmod doesn't have this.
	{
		UserData* pData = (UserData*)lua_newuserdata(L, sizeof(ILuaBase::UserData));
		pData->data = nullptr;
		pData->type = 7;
		return;
	}
	

	((ILuaBase*)L->luabase)->PushUserType(NULL, 7);
}