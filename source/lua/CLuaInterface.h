#ifndef GMOD_CLUAINTERFACE

#define GMOD_CLUAINTERFACE
#include <cstdarg>
#include "ILuaInterface.h"
#include "GarrysMod/Lua/LuaGameCallback.h"
#include "GarrysMod/Lua/LuaObject.h"
#include <list>

#define GMOD

#ifdef BUILD_GMOD
#ifdef _WIN32
#define WIN32
#else
#define WIN64
#endif
#endif
extern void DebugPrint(int level, const char* fmt, ...);

struct lua_Debug;
extern void GMOD_LoadBinaryModule(lua_State* L, const char*);
extern void GMOD_UnloadBinaryModules(lua_State* L);

extern int g_iTypeNum;
class CLuaInterface : public GarrysMod::Lua::ILuaInterface
{
public:
	virtual int Top(void);
	virtual void Push(int iStackPos);
	virtual void Pop(int iAmt = 1);
	virtual void GetTable(int iStackPos);
	virtual void GetField(int iStackPos, const char* strName);
	virtual void SetField(int iStackPos, const char* strName);
	virtual void CreateTable();
	virtual void SetTable(int iStackPos);
	virtual void SetMetaTable(int iStackPos);
	virtual bool GetMetaTable(int i);
	virtual void Call(int iArgs, int iResults);
	virtual int PCall(int iArgs, int iResults, int iErrorFunc);
	virtual int Equal(int iA, int iB);
	virtual int RawEqual(int iA, int iB);
	virtual void Insert(int iStackPos);
	virtual void Remove(int iStackPos);
	virtual int Next(int iStackPos);
	virtual void* NewUserdata(unsigned int iSize);
	[[noreturn]]
	virtual void ThrowError(const char* strError);
	virtual void CheckType(int iStackPos, int iType);
	[[noreturn]]
	virtual void ArgError(int iArgNum, const char* strMessage);
	virtual void RawGet(int iStackPos);
	virtual void RawSet(int iStackPos);
	virtual const char* GetString(int iStackPos = -1, unsigned int* iOutLen = nullptr);
	virtual double GetNumber(int iStackPos = -1);
	virtual bool GetBool(int iStackPos = -1);
	virtual GarrysMod::Lua::CFunc GetCFunction(int iStackPos = -1);
	virtual void* GetUserdata(int iStackPos = -1);
	virtual void PushNil();
	virtual void PushString(const char* val, unsigned int iLen = 0);
	virtual void PushNumber(double val);
	virtual void PushBool(bool val);
	virtual void PushCFunction(GarrysMod::Lua::CFunc val);
	virtual void PushCClosure(GarrysMod::Lua::CFunc val, int iVars);
	virtual void PushUserdata(void*);
	virtual int ReferenceCreate();
	virtual void ReferenceFree(int i);
	virtual void ReferencePush(int i);
	virtual void PushSpecial(int iType);
	virtual bool IsType(int iStackPos, int iType);
	virtual int GetType(int iStackPos);
	virtual const char* GetTypeName(int iType);
	virtual void CreateMetaTableType(const char* strName, int iType);
	virtual const char* CheckString(int iStackPos = -1);
	virtual double CheckNumber(int iStackPos = -1);
	virtual int ObjLen(int iStackPos = -1);
	virtual const QAngle& GetAngle(int iStackPos = -1);
	virtual const Vector& GetVector(int iStackPos = -1);
	virtual void PushAngle(const QAngle& val);
	virtual void PushVector(const Vector& val);
	virtual void SetState(lua_State* L);
	virtual int CreateMetaTable(const char* strName);
	virtual bool PushMetaTable(int iType);
	virtual void PushUserType(void* data, int iType);
	virtual void SetUserType(int iStackPos, void* data);

public:
	virtual bool Init(GarrysMod::Lua::ILuaGameCallback *, bool);
	virtual void Shutdown();
	virtual void Cycle();
	virtual GarrysMod::Lua::ILuaObject *Global();
	virtual GarrysMod::Lua::ILuaObject *GetObject(int index);
	virtual void PushLuaObject(GarrysMod::Lua::ILuaObject *obj);
	virtual void PushLuaFunction(GarrysMod::Lua::CFunc func);
	virtual void LuaError(const char *err, int index);
	virtual void TypeError(const char *name, int index);
	virtual void CallInternal(int args, int rets);
	virtual void CallInternalNoReturns(int args);
	virtual bool CallInternalGetBool( int args );
	virtual const char *CallInternalGetString( int args );
	virtual bool CallInternalGet( int args, GarrysMod::Lua::ILuaObject *obj );
	virtual void NewGlobalTable( const char *name );
	virtual GarrysMod::Lua::ILuaObject *NewTemporaryObject( );
	virtual bool isUserData( int index );
	virtual GarrysMod::Lua::ILuaObject *GetMetaTableObject( const char *name, int type );
	virtual GarrysMod::Lua::ILuaObject *GetMetaTableObject( int index );
	virtual GarrysMod::Lua::ILuaObject *GetReturn( int index );
	virtual bool IsServer( );
	virtual bool IsClient( );
	virtual bool IsMenu( );
	virtual void DestroyObject( GarrysMod::Lua::ILuaObject *obj );
	virtual GarrysMod::Lua::ILuaObject *CreateObject( );
	virtual void SetMember( GarrysMod::Lua::ILuaObject *table, GarrysMod::Lua::ILuaObject *key, GarrysMod::Lua::ILuaObject *value );
	virtual GarrysMod::Lua::ILuaObject* GetNewTable( );
	virtual void SetMember( GarrysMod::Lua::ILuaObject *table, float key );
	virtual void SetMember( GarrysMod::Lua::ILuaObject *table, float key, GarrysMod::Lua::ILuaObject *value );
	virtual void SetMember( GarrysMod::Lua::ILuaObject *table, const char *key );
	virtual void SetMember( GarrysMod::Lua::ILuaObject *table, const char *key, GarrysMod::Lua::ILuaObject *value );
	virtual void SetType( unsigned char );
	virtual void PushLong( long num );
	virtual int GetFlags( int index );
	virtual bool FindOnObjectsMetaTable( int objIndex, int keyIndex );
	virtual bool FindObjectOnTable( int tableIndex, int keyIndex );
	virtual void SetMemberFast( GarrysMod::Lua::ILuaObject *table, int keyIndex, int valueIndex );
	virtual bool RunString( const char *filename, const char *path, const char *stringToRun, bool run, bool showErrors );
	virtual bool IsEqual( GarrysMod::Lua::ILuaObject *objA, GarrysMod::Lua::ILuaObject *objB );
	virtual void Error( const char *err );
	virtual const char *GetStringOrError( int index );
	virtual bool RunLuaModule( const char *name );
	virtual bool FindAndRunScript( const char *filename, bool run, bool showErrors, const char *stringToRun, bool noReturns );
	virtual void SetPathID( const char *pathID );
	virtual const char *GetPathID( );
	virtual void ErrorNoHalt( const char *fmt, ... );
	virtual void Msg( const char *fmt, ... );
	virtual void PushPath( const char *path );
	virtual void PopPath( );
	virtual const char *GetPath( );
	virtual int GetColor( int index );
	virtual void PushColor( Color color );
	virtual int GetStack( int level, lua_Debug *dbg );
	virtual int GetInfo( const char *what, lua_Debug *dbg );
	virtual const char *GetLocal( lua_Debug *dbg, int n );
	virtual const char *GetUpvalue( int funcIndex, int n );
	virtual bool RunStringEx( const char *filename, const char *path, const char *stringToRun, bool run, bool printErrors, bool dontPushErrors, bool noReturns );
	virtual size_t GetDataString( int index, const char **str );
	virtual void ErrorFromLua( const char *fmt, ... );
	virtual const char *GetCurrentLocation( );
	virtual void MsgColour( const Color &col, const char *fmt, ... );
	virtual void GetCurrentFile( std::string &outStr );
	virtual void CompileString( Bootil::Buffer &dumper, const std::string &stringToCompile );
	virtual bool CallFunctionProtected( int iArgs, int iRets, bool showError );
	virtual void Require( const char *name );
	virtual const char *GetActualTypeName( int type );
	virtual void PreCreateTable( int arrelems, int nonarrelems );
	virtual void PushPooledString( int index );
	virtual const char *GetPooledString( int index );
	virtual int AddThreadedCall( GarrysMod::Lua::ILuaThreadedCall * );
	virtual void AppendStackTrace( char *, unsigned long );
	virtual void *CreateConVar( const char *, const char *, const char *, int );
	virtual void *CreateConCommand( const char *, const char *, int, FnCommandCallback_t, FnCommandCompletionCallback );
	virtual const char* CheckStringOpt( int iStackPos, const char* def );
	virtual double CheckNumberOpt( int iStackPos, double def );
	virtual void RegisterMetaTable( const char* name, GarrysMod::Lua::ILuaObject* obj );

public:
	std::string RunMacros(std::string script);

public:
	inline GarrysMod::Lua::ILuaGameCallback *GetLuaGameCallback() const
	{
		return m_pGameCallback;
	}

	inline void SetLuaGameCallback( GarrysMod::Lua::ILuaGameCallback *callback )
	{
		m_pGameCallback = callback;
	}

private:
	char* m_sCurrentPath = new char[32]; // not how gmod does it :/
	int m_iPushedPaths = 0;
	const char* m_sLastPath = NULL;
	std::list<GarrysMod::Lua::ILuaThreadedCall*> m_pThreadedCalls;
	unsigned char m_iRealm = -1; // CLIENT = 0, SERVER = 1, MENU = 2
	GarrysMod::Lua::ILuaGameCallback* m_pGameCallback = nullptr;
	char m_sPathID[32] = "LuaMenu"; // lsv, lsc or LuaMenu
	int m_iGlobalReference = -1;
	int m_iStringPoolReference = -1;
	std::list<char*> m_pPaths;

public:
	void RunThreadedCalls();
	inline void DoStackCheck() {
		DebugPrint(2, "Top: %i\n", Top());
	}
};

// Some functions declared inside CLuaInterface_cpp
//extern GarrysMod::Lua::ILuaGameCallback::CLuaError* ReadStackIntoError(lua_State* L);
//extern int AdvancedLuaErrorReporter(lua_State* L);

extern GarrysMod::Lua::ILuaInterface* CreateLuaInterface(bool bIsServer);
extern void CloseLuaInterface(GarrysMod::Lua::ILuaInterface*);
#endif