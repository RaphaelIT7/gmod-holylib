#ifndef GMOD_ILUAINTERFACE
#define GMOD_ILUAINTERFACE

#include <cstdarg>
#include <GarrysMod/Lua/LuaBase.h>
#include <GarrysMod/Lua/LuaObject.h>
#include <GarrysMod/Lua/LuaGameCallback.h>
#include <convar.h>

// NOTE: This is a custom class that somewhat works works with Gmod? (Gmod's real class is currently unknown) soonTM
// Update: This class now works 100% with Gmod :D
class ILuaThreadedCall {
public:
    virtual void Init() = 0;
    virtual void Run(GarrysMod::Lua::ILuaBase*) = 0;
};

struct lua_Debug;
namespace Bootil
{
	class Buffer;
}

namespace GarrysMod::Lua
{
	class IUpdatedLuaInterface;
};

class GarrysMod::Lua::IUpdatedLuaInterface : public GarrysMod::Lua::ILuaBase
{
public:
	virtual bool Init( GarrysMod::Lua::ILuaGameCallback *, bool ) = 0;
	virtual void Shutdown( ) = 0;
	virtual void Cycle( ) = 0;
	virtual GarrysMod::Lua::ILuaObject *Global( ) = 0;
	virtual GarrysMod::Lua::ILuaObject *GetObject( int index ) = 0;
	virtual void PushLuaObject( GarrysMod::Lua::ILuaObject *obj ) = 0;
	virtual void PushLuaFunction( GarrysMod::Lua::CFunc func ) = 0;
	virtual void LuaError( const char *err, int index ) = 0;
	virtual void TypeError( const char *name, int index ) = 0;
	virtual void CallInternal( int args, int rets ) = 0;
	virtual void CallInternalNoReturns( int args ) = 0;
	virtual bool CallInternalGetBool( int args ) = 0;
	virtual const char *CallInternalGetString( int args ) = 0;
	virtual bool CallInternalGet( int args, GarrysMod::Lua::ILuaObject *obj ) = 0;
	virtual void NewGlobalTable( const char *name ) = 0;
	virtual GarrysMod::Lua::ILuaObject *NewTemporaryObject( ) = 0;
	virtual bool isUserData( int index ) = 0;
	virtual GarrysMod::Lua::ILuaObject *GetMetaTableObject( const char *name, int type ) = 0;
	virtual GarrysMod::Lua::ILuaObject *GetMetaTableObject( int index ) = 0;
	virtual GarrysMod::Lua::ILuaObject *GetReturn( int index ) = 0;
	virtual bool IsServer( ) = 0;
	virtual bool IsClient( ) = 0;
	virtual bool IsMenu( ) = 0;
	virtual void DestroyObject( GarrysMod::Lua::ILuaObject *obj ) = 0;
	virtual GarrysMod::Lua::ILuaObject *CreateObject( ) = 0;
	virtual void SetMember( GarrysMod::Lua::ILuaObject *table, GarrysMod::Lua::ILuaObject *key, GarrysMod::Lua::ILuaObject *value ) = 0;
	virtual GarrysMod::Lua::ILuaObject* GetNewTable( ) = 0;
	virtual void SetMember( GarrysMod::Lua::ILuaObject *table, float key ) = 0;
	virtual void SetMember( GarrysMod::Lua::ILuaObject *table, float key, GarrysMod::Lua::ILuaObject *value ) = 0;
	virtual void SetMember( GarrysMod::Lua::ILuaObject *table, const char *key ) = 0;
	virtual void SetMember( GarrysMod::Lua::ILuaObject *table, const char *key, GarrysMod::Lua::ILuaObject *value ) = 0;
	virtual void SetType( unsigned char ) = 0;
	virtual void PushLong( long num ) = 0;
	virtual int GetFlags( int index ) = 0;
	virtual bool FindOnObjectsMetaTable( int objIndex, int keyIndex ) = 0;
	virtual bool FindObjectOnTable( int tableIndex, int keyIndex ) = 0;
	virtual void SetMemberFast( GarrysMod::Lua::ILuaObject *table, int keyIndex, int valueIndex ) = 0;
	virtual bool RunString( const char *filename, const char *path, const char *stringToRun, bool run, bool showErrors ) = 0;
	virtual bool IsEqual( GarrysMod::Lua::ILuaObject *objA, GarrysMod::Lua::ILuaObject *objB ) = 0;
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
	virtual int AddThreadedCall( ILuaThreadedCall * ) = 0;
	virtual void AppendStackTrace( char *, unsigned long ) = 0;
	virtual void *CreateConVar( const char *, const char *, const char *, int ) = 0;
	virtual void *CreateConCommand( const char *, const char *, int, void ( * )( const CCommand & ), int ( * )( const char *, char ( * )[128] ) ) = 0;
	virtual const char* CheckStringOpt( int iStackPos, const char* def ) = 0;
	virtual double CheckNumberOpt( int iStackPos, double def ) = 0;
	virtual void RegisterMetaTable( const char* name, GarrysMod::Lua::ILuaObject* obj ) = 0;
};
#endif