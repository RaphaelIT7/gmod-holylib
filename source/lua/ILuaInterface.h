#ifndef GMOD_ILUAINTERFACE_H
#define GMOD_ILUAINTERFACE_H
#define GARRYSMOD_LUA_LUABASE_H
#include <string>
#include <mathlib/vector.h>
#include <convar.h>
#define GMOD
#include "GarrysMod/Lua/Types.h"
#undef GMOD

class CCommand;
struct lua_Debug;
namespace Bootil
{
	class Buffer;
}

#define LUA_MAX_TEMP_OBJECTS 32

class CLuaInterface;
struct lua_State;
namespace GarrysMod::Lua
{
	typedef int ( *CFunc )( lua_State* L );

	// For use with ILuaBase::PushSpecial
	enum
	{
		SPECIAL_GLOB,       // Global table
		SPECIAL_ENV,        // Environment table
		SPECIAL_REG,        // Registry table
	};

	// Use these when calling ILuaBase::GetField or ILuaBase::SetField for example,
	// instead of pushing the specified table
	enum
	{
		INDEX_GLOBAL = -10002,  // Global table
		INDEX_ENVIRONMENT,      // Environment table
		INDEX_REGISTRY,         // Registry table
	};

	class ILuaBase
	{
	public:
		// You shouldn't need to use this struct
		// Instead, use the UserType functions
		struct UserData
		{
			void* data;
			unsigned char type; // Change me to a uint32 one day
		};

	protected:
		template <class T>
		struct UserData_Value : UserData
		{
			T value;
		};

	public:
		// Returns the amount of values on the stack
		virtual int         Top(void) = 0;

		// Pushes a copy of the value at iStackPos to the top of the stack
		virtual void        Push(int iStackPos) = 0;

		// Pops iAmt values from the top of the stack
		virtual void        Pop(int iAmt = 1) = 0;

		// Pushes table[key] on to the stack
		// table = value at iStackPos
		// key   = value at top of the stack
		// Pops the key from the stack
		virtual void        GetTable(int iStackPos) = 0;

		// Pushes table[key] on to the stack
		// table = value at iStackPos
		// key   = strName
		virtual void        GetField(int iStackPos, const char* strName) = 0;

		// Sets table[key] to the value at the top of the stack
		// table = value at iStackPos
		// key   = strName
		// Pops the value from the stack
		virtual void        SetField(int iStackPos, const char* strName) = 0;

		// Creates a new table and pushes it to the top of the stack
		virtual void        CreateTable() = 0;

		// Sets table[key] to the value at the top of the stack
		// table = value at iStackPos
		// key   = value 2nd to the top of the stack
		// Pops the key and the value from the stack
		virtual void        SetTable(int iStackPos) = 0;

		// Sets the metatable for the value at iStackPos to the value at the top of the stack
		// Pops the value off of the top of the stack
		virtual void        SetMetaTable(int iStackPos) = 0;

		// Pushes the metatable of the value at iStackPos on to the top of the stack
		// Upon failure, returns false and does not push anything
		virtual bool        GetMetaTable(int i) = 0;

		// Calls a function
		// To use it: Push the function on to the stack followed by each argument
		// Pops the function and arguments from the stack, leaves iResults values on the stack
		// If this function errors, any local C values will not have their destructors called!
		virtual void        Call(int iArgs, int iResults) = 0;

		// Similar to Call
		// See: lua_pcall( lua_State*, int, int, int )
		virtual int         PCall(int iArgs, int iResults, int iErrorFunc) = 0;

		// Returns true if the values at iA and iB are equal
		virtual int         Equal(int iA, int iB) = 0;

		// Returns true if the value at iA and iB are equal
		// Does not invoke metamethods
		virtual int         RawEqual(int iA, int iB) = 0;

		// Moves the value at the top of the stack in to iStackPos
		// Any elements above iStackPos are shifted upwards
		virtual void        Insert(int iStackPos) = 0;

		// Removes the value at iStackPos from the stack
		// Any elements above iStackPos are shifted downwards
		virtual void        Remove(int iStackPos) = 0;

		// Allows you to iterate tables similar to pairs(...)
		// See: lua_next( lua_State*, int );
		virtual int         Next(int iStackPos) = 0;

	//#ifndef GMOD_ALLOW_DEPRECATED
	//protected:
	//#endif
		// Deprecated: Use the UserType functions instead of this
		virtual void* NewUserdata(unsigned int iSize) = 0;

	public:
		// Throws an error and ceases execution of the function
		// If this function is called, any local C values will not have their destructors called!
		[[noreturn]]
		virtual void        ThrowError(const char* strError) = 0;

		// Checks that the type of the value at iStackPos is iType
		// Throws and error and ceases execution of the function otherwise
		// If this function errors, any local C values will not have their destructors called!
		virtual void        CheckType(int iStackPos, int iType) = 0;

		// Throws a pretty error message about the given argument
		// If this function is called, any local C values will not have their destructors called!
		[[noreturn]]
		virtual void        ArgError(int iArgNum, const char* strMessage) = 0;

		// Pushes table[key] on to the stack
		// table = value at iStackPos
		// key   = value at top of the stack
		// Does not invoke metamethods
		virtual void        RawGet(int iStackPos) = 0;

		// Sets table[key] to the value at the top of the stack
		// table = value at iStackPos
		// key   = value 2nd to the top of the stack
		// Pops the key and the value from the stack
		// Does not invoke metamethods
		virtual void        RawSet(int iStackPos) = 0;

		// Returns the string at iStackPos. iOutLen is set to the length of the string if it is not NULL
		// If the value at iStackPos is a number, it will be converted in to a string
		// Returns NULL upon failure
		virtual const char* GetString(int iStackPos = -1, unsigned int* iOutLen = nullptr) = 0;

		// Returns the number at iStackPos
		// Returns 0 upon failure
		virtual double      GetNumber(int iStackPos = -1) = 0;

		// Returns the boolean at iStackPos
		// Returns false upon failure
		virtual bool        GetBool(int iStackPos = -1) = 0;

		// Returns the C-Function at iStackPos
		// returns NULL upon failure
		virtual CFunc       GetCFunction(int iStackPos = -1) = 0;

	//#if !defined( GMOD_ALLOW_DEPRECATED ) && !defined( GMOD_ALLOW_LIGHTUSERDATA )
	//protected:
	//#endif
		// Deprecated: You should probably be using the UserType functions instead of this
		virtual void* GetUserdata(int iStackPos = -1) = 0;

	public:
		// Pushes a nil value on to the stack
		virtual void        PushNil() = 0;

		// Pushes the given string on to the stack
		// If iLen is 0, strlen will be used to determine the string's length
		virtual void        PushString(const char* val, unsigned int iLen = 0) = 0;

		// Pushes the given double on to the stack
		virtual void        PushNumber(double val) = 0;

		// Pushes the given bobolean on to the stack
		virtual void        PushBool(bool val) = 0;

		// Pushes the given C-Function on to the stack
		virtual void        PushCFunction(CFunc val) = 0;

		// Pushes the given C-Function on to the stack with upvalues
		// See: GetUpvalueIndex()
		virtual void        PushCClosure(CFunc val, int iVars) = 0;


	//#if !defined( GMOD_ALLOW_DEPRECATED ) && !defined( GMOD_ALLOW_LIGHTUSERDATA )
	//protected:
	//#endif
		// Deprecated: Don't use light userdata in GMod
		virtual void        PushUserdata(void*) = 0;

	public:
		// Allows for values to be stored by reference for later use
		// Make sure you call ReferenceFree when you are done with a reference
		virtual int         ReferenceCreate() = 0;
		virtual void        ReferenceFree(int i) = 0;
		virtual void        ReferencePush(int i) = 0;

		// Push a special value onto the top of the stack (see SPECIAL_* enums)
		virtual void        PushSpecial(int iType) = 0;

		// Returns true if the value at iStackPos is of type iType
		// See: Types.h
		virtual bool        IsType(int iStackPos, int iType) = 0;

		// Returns the type of the value at iStackPos
		// See: Types.h
		virtual int         GetType(int iStackPos) = 0;

		// Returns the name associated with the given type ID
		// See: Types.h
		// Note: GetTypeName does not work with user-created types
		virtual const char* GetTypeName(int iType) = 0;

	//#ifndef GMOD_ALLOW_DEPRECATED
	//protected:
	//#endif
		// Deprecated: Use CreateMetaTable
		virtual void        CreateMetaTableType(const char* strName, int iType) = 0;

	public:
		// Like Get* but throws errors and returns if they're not of the expected type
		// If these functions error, any local C values will not have their destructors called!
		virtual const char* CheckString(int iStackPos = -1) = 0;
		virtual double      CheckNumber(int iStackPos = -1) = 0;

		// Returns the length of the object at iStackPos
		// Works for: strings, tables, userdata
		virtual int         ObjLen(int iStackPos = -1) = 0;

		// Returns the angle at iStackPos
		virtual const QAngle& GetAngle(int iStackPos = -1) = 0;

		// Returns the vector at iStackPos
		virtual const Vector& GetVector(int iStackPos = -1) = 0;

		// Pushes the given angle to the top of the stack
		virtual void        PushAngle(const QAngle& val) = 0;

		// Pushes the given vector to the top of the stack
		virtual void        PushVector(const Vector& val) = 0;

		// Sets the lua_State to be used by the ILuaBase implementation
		// You don't need to use this if you use the LUA_FUNCTION macro
		virtual void        SetState(lua_State* L) = 0;

		// Pushes the metatable associated with the given type name
		// Returns the type ID to use for this type
		// If the type doesn't currently exist, it will be created
		virtual int         CreateMetaTable(const char* strName) = 0;

		// Pushes the metatable associated with the given type
		virtual bool        PushMetaTable(int iType) = 0;

		// Creates a new UserData of type iType that references the given data
		virtual void        PushUserType(void* data, int iType) = 0;

		// Sets the data pointer of the UserType at iStackPos
		// You can use this to invalidate a UserType by passing NULL
		virtual void        SetUserType(int iStackPos, void* data) = 0;

		// Returns the data of the UserType at iStackPos if it is of the given type
		template <class T>
		T* GetUserType(int iStackPos, int iType)
		{
			auto* ud = static_cast<UserData*>(GetUserdata(iStackPos));

			if (ud == nullptr || ud->data == nullptr || ud->type != iType)
				return nullptr;

			return static_cast<T*>(ud->data);
		}

		// Creates a new UserData of type iType with an instance of T
		// If your class is complex/has complex members which handle memory,
		// you might need a __gc method to clean these, as Lua won't handle them
		template <typename T>
		T* NewUserType(int iType)
		{
			UserData* ud = static_cast<UserData*>(NewUserdata(sizeof(UserData) + sizeof(T)));
			if (ud == nullptr)
				return nullptr;

			T* data = reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(ud) + sizeof(UserData));
			ud->data = new(data) T;
			ud->type = static_cast<unsigned char>(iType);

			return data;
		}

		// Creates a new UserData with your own data embedded within it
		template <class T>
		void PushUserType_Value(const T& val, int iType)
		{
			using UserData_T = UserData_Value<T>;

			// The UserData allocated by CLuaInterface is only guaranteed to have a data alignment of 8
			static_assert(std::alignment_of<UserData_T>::value <= 8,
				"PushUserType_Value given type with unsupported alignment requirement");

			// Don't give this function objects that can't be trivially destructed
			// You could ignore this limitation if you implement object destruction in `__gc`
			static_assert(std::is_trivially_destructible<UserData_T>::value,
				"PushUserType_Value given type that is not trivially destructible");

			auto* ud = static_cast<UserData_T*>(NewUserdata(sizeof(UserData_T)));
			ud->data = new(&ud->value) T(val);
			ud->type = iType;

			// Set the metatable
			if (PushMetaTable(iType)) SetMetaTable(-2);
		}

		// Gets the internal lua_State
		inline lua_State* GetState() const
		{
			return state;
		}

		// Gets the environment table of the value at the given index
		/*inline void GetFEnv(int iStackPos)
		{
			lua_getfenv(state, iStackPos);
		}

		// Sets the environment table of the value at the given index
		inline int SetFEnv(int iStackPos)
		{
			return lua_setfenv(state, iStackPos);
		}

		// Pushes a formatted string onto the stack
		inline const char* PushFormattedString(const char* fmt, va_list args)
		{
			return lua_pushvfstring(state, fmt, args);
		}*/

		// Pushes a formatted string onto the stack
		inline const char* PushFormattedString(const char* fmt, ...)
		{
			va_list args;
			va_start(args, fmt);
			const char* res = PushFormattedString(fmt, args);
			va_end(args);
			return res;
		}

		// Throws an error (uses the value at the top of the stack)
		//[[noreturn]]
		/*inline void Error()
		{
			lua_error(state);

			// Should never be reached since 'lua_error' never returns.
			std::abort();
		}*/

		// Throws an error (pushes a formatted string onto the stack and uses it)
		[[noreturn]]
		inline void FormattedError(const char* fmt, ...)
		{
			va_list args;
			va_start(args, fmt);
			PushFormattedString(fmt, args);
			va_end(args);
			//Error();
		}

		// Throws an error related to type differences
		/*[[noreturn]]
		inline void TypeError(int iStackPos, const char* tname)
		{
			luaL_typerror(state, iStackPos, tname);

			// Should never be reached since 'luaL_typerror' never returns.
			std::abort();
		}*/

		// Converts the value at the given index to a generic C pointer (void*)
		/*inline const void* GetPointer(int iStackPos)
		{
			return lua_topointer(state, iStackPos);
		}*/

		// Calls a metamethod on the object at iStackPos
		/*inline int CallMeta(int iStackPos, const char* e)
		{
			return luaL_callmeta(state, iStackPos, e);
		}*/

		// Produces the pseudo-index of an upvalue at iPos
		static inline int GetUpvalueIndex(int iPos)
		{
			return static_cast<int>(INDEX_GLOBAL) - iPos;
		}

		// Get information about the interpreter runtime stack
		/*inline int GetStack(int level, lua_Debug* ar)
		{
			return lua_getstack(state, level, ar);
		}

		// Returns information about a specific function or function invocation
		inline int GetInfo(const char* what, lua_Debug* ar)
		{
			return lua_getinfo(state, what, ar);
		}*/

	public: // I do not care anymore. compile!
		//friend class CLuaInterface;

		lua_State* state;
	};

	// NOTE: This is a custom class that somewhat works works with Gmod? (Gmod's real class is currently unknown) soonTM
	// Update: This class now works 100% with Gmod :D
	class ILuaThreadedCall {
	public:
		virtual void Init() = 0;
		virtual void Run(ILuaBase*) = 0;
	};

	class ILuaObject;
	class ILuaGameCallback;
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
		virtual ILuaObject* GetNewTable( ) = 0;
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
		virtual bool CallFunctionProtected( int iArgs, int iRets, bool bError ) = 0;
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
		virtual void RegisterMetaTable( const char* name, ILuaObject* obj ) = 0;

		// Not in gmod? Anyways.
		virtual ~ILuaInterface() {}
	};
}
#endif