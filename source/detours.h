#pragma once

#ifndef LUAJIT
#define TIER0_DLL_EXPORT
#endif
#include <GarrysMod/FactoryLoader.hpp>
#include <scanning/symbolfinder.hpp>
#include <GarrysMod/Symbol.hpp>
#include <detouring/hook.hpp>
#include "Platform.hpp"
#include "tier0/dbg.h"
#include <vector>
#include <unordered_set>
#include <unordered_map>

#ifdef DLL_TOOLS
#ifdef WIN32
#include <Windows.h>
#undef GetObject
#undef GetClassName
#define DLL_Handle HMODULE
#define DLL_LoadModule(name, _) LoadLibrary(name)
#define DLL_UnloadModule(handle) FreeLibrary((DLL_Handle)handle)
#define DLL_GetAddress(handle, name) GetProcAddress((DLL_Handle)handle, name)
#define DLL_LASTERROR "LINUXONLY"
#else
#include <dlfcn.h>
#define DLL_Handle void*
#define DLL_LoadModule(name, type) dlopen(name, type)
#define DLL_UnloadModule(handle) dlclose(handle)
#define DLL_GetAddress(handle, name) dlsym(handle, name)
#define DLL_LASTERROR dlerror()
#endif
#endif

//---------------------------------------------------------------------------------
// Purpose: Detour functions
//---------------------------------------------------------------------------------
namespace Detour
{
#define DETOUR_ISVALID( detour ) detour .IsValid()

#define DETOUR_ISENABLED( detour ) detour .IsEnabled()

#define DETOUR_ENABLE( detour ) detour .Enable()

#define DETOUR_DISABLE( detour ) detour .Disable()

// Below one is unused for now.
#define DETOUR_GETFUNC( detour, type ) \
	detour .GetTrampoline< type >()

#define DETOUR_CREATE( loader, name, funcName ) \
	Detour::Create( \
		&detour_##name, funcName, \
		loader##.GetModule(), Symbols::##name##Sym, \
		(void*)hook_##name, m_pID \
	)

#if SYSTEM_WINDOWS
/*
	__thiscall is funky, the compiler LOVES to screw up registers causing invalid/corrupted arguments
	and deny __thiscall directly as you are REQUIRED to only use it on a non-static method
	so to workaround this, you can use these to let your hook be called properly by having a wrapper around it.

	How this works:
	
	First you call 
		DETOUR_THISCALL_START()
	This will setup a static class into which then our wrapper functions are added.

	Then use
		DETOUR_THISCALL_ADDFUNC1( hook_Bla_ExampleHook, ExampleHook, CExampleClass*, const char* );
	Which will translate to 
		virtual void ExampleHook(const char* __arg1) {
			realHook((CExampleClass*)this, __arg1); // this will always be your CExampleClass* given as the first argument to the hook
		};

	You can continue adding more, when your done, use
		DETOUR_THISCALL_FINISH()
	Which will close the class and setup a static g_pThisCallConversionWorkaround variable.

	Now, when you do your Detour::Create calls, instead of just passing your hook directly, you instead use
		DETOUR_THISCALL(hook_Bla_ExampleHook, ExampleHook) -- First is your normal hook, second is the name you used when calling AddFunc

	Detour::Create(
		&detour_CBaseFileSystem_GetFileTime, "CBaseFileSystem::GetFileTime",
		filesystem_loader.GetModule(), Symbols::CBaseFileSystem_GetFileTimeSym,
		(void*)DETOUR_THISCALL(hook_CBaseFileSystem_GetFileTime, GetFileTime), <------- Here you just use it
		m_pID
	);
*/

// Uses an anonymous namespace since else the compile LOVES to merge variables/vtable into one global class breaking everything and overriving vtable entries
#define DETOUR_THISCALL_START() \
namespace { \
class ThisCallWorkaround \
{ \
public:

#define DETOUR_THISCALL_FINISH() \
}; \
static ThisCallWorkaround g_pThisCallConversionWorkaround; \
}

#define DETOUR_THISCALL_ADDFUNC0( realHook, name, thisType ) \
virtual void name() { realHook((thisType)this); }; \
byte m_##name = 0;

#define DETOUR_THISCALL_ADDFUNC1( realHook, name, thisType, arg1 ) \
virtual void name(arg1 __arg1) { realHook((thisType)this, __arg1); };\
byte m_##name = 0;

#define DETOUR_THISCALL_ADDFUNC2( realHook, name, thisType, arg1, arg2 ) \
virtual void name(arg1 __arg1, arg2 __arg2) { realHook((thisType)this, __arg1, __arg2); };\
byte m_##name = 0;

#define DETOUR_THISCALL_ADDFUNC3( realHook, name, thisType, arg1, arg2, arg3 ) \
virtual void name(arg1 __arg1, arg2 __arg2, arg3 __arg3) { realHook((thisType)this, __arg1, __arg2, __arg3); };\
byte m_##name = 0;

#define DETOUR_THISCALL_ADDFUNC4( realHook, name, thisType, arg1, arg2, arg3, arg4 ) \
virtual void name(arg1 __arg1, arg2 __arg2, arg3 __arg3, arg4 __arg4) { realHook((thisType)this, __arg1, __arg2, __arg3, __arg4); };\
byte m_##name = 0;

#define DETOUR_THISCALL_ADDRETFUNC0( realHook, returnValue, name, thisType ) \
virtual returnValue name() { return realHook((thisType)this); };\
byte m_##name = 0;

#define DETOUR_THISCALL_ADDRETFUNC1( realHook, returnValue, name, thisType, arg1 ) \
virtual returnValue name(arg1 __arg1) { return realHook((thisType)this, __arg1); };\
byte m_##name = 0;

#define DETOUR_THISCALL_ADDRETFUNC2( realHook, returnValue, name, thisType, arg1, arg2 ) \
virtual returnValue name(arg1 __arg1, arg2 __arg2) { return realHook((thisType)this, __arg1, __arg2); };\
byte m_##name = 0;

#define DETOUR_THISCALL_ADDRETFUNC3( realHook, returnValue, name, thisType, arg1, arg2, arg3 ) \
virtual returnValue name(arg1 __arg1, arg2 __arg2, arg3 __arg3) { return realHook((thisType)this, __arg1, __arg2, __arg3); };\
byte m_##name = 0;

#define DETOUR_THISCALL_ADDRETFUNC4( realHook, returnValue, name, thisType, arg1, arg2, arg3, arg4 ) \
virtual returnValue name(arg1 __arg1, arg2 __arg2, arg3 __arg3, arg4 __arg4) { return realHook((thisType)this, __arg1, __arg2, __arg3, __arg4); };\
byte m_##name = 0;

#define DETOUR_THISCALL_ADDRETFUNC5( realHook, returnValue, name, thisType, arg1, arg2, arg3, arg4, arg5 ) \
virtual returnValue name(arg1 __arg1, arg2 __arg2, arg3 __arg3, arg4 __arg4, arg5 __arg5) { return realHook((thisType)this, __arg1, __arg2, __arg3, __arg4, __arg5); };\
byte m_##name = 0;

#define DETOUR_PREPARE_THISCALL() void** pWorkaroundVTable = *(void***)&g_pThisCallConversionWorkaround
/*
	HACK
	Since we cannot just get the index of a vtable function
	I decided to use member variables as its straight forward and easy to get their offset
	And since we create a virtual function and a variable together we can be 100% sure that the variable offset will match the vtable offset.
	Since the vtable is always first, we also remove sizeof(void*) as thats the 4/8 bytes of the vtable.
*/
#define DETOUR_THISCALL( linuxHook, windowsHook ) pWorkaroundVTable[((size_t)&(((ThisCallWorkaround*)0)->m_##windowsHook)) - sizeof(void*)]
#else
#define DETOUR_PREPARE_THISCALL( varName )
#define DETOUR_THISCALL( linuxHook, windowsHook ) linuxHook
#endif

	inline bool CheckValue(const char* strMsg, const char* strName, bool bRet)
	{
		if (!bRet) {
			Warning(PROJECT_NAME ": Failed to %s %s!\n", strMsg, strName);
			return false;
		}

		return true;
	}

	inline bool CheckValue(const char* strName, bool bRet)
	{
		return CheckValue("get function", strName, bRet);
	}

	inline bool CheckFunction(void* pFunc, const char* strName)
	{
		return CheckValue("get function", strName, pFunc != nullptr);
	}

	extern void* GetFunction(void* pModule, Symbol pSymbol);
	extern void Create(Detouring::Hook* pHook, const char* strName, void* pModule, Symbol pSymbol, void* pHookFunc, unsigned int category = 0);
	extern void Remove(unsigned int category); // 0 = All
	extern void ReportLeak();
	extern const std::unordered_set<std::string>& GetDisabledDetours();
	extern const std::unordered_set<std::string>& GetFailedDetours();
	extern const std::unordered_map<std::string, unsigned int>& GetLoadedDetours();

	extern SymbolFinder symfinder;
	template<class T>
	inline T* ResolveSymbol(
		SourceSDK::FactoryLoader& pLoader, const Symbol& pSymbol
	)
	{
		if (pSymbol.type == Symbol::Type::None)
			return nullptr;

	#if defined SYSTEM_WINDOWS
		auto iface = reinterpret_cast<T**>(symfinder.Resolve(
			pLoader.GetModule(), pSymbol.name.c_str(), pSymbol.length
		));
		return iface != nullptr ? *iface : nullptr;
	#elif defined SYSTEM_POSIX
		return reinterpret_cast<T*>(symfinder.Resolve(
			pLoader.GetModule(), pSymbol.name.c_str(), pSymbol.length
		));
	#endif
	}

#ifdef SYSTEM_LINUX
#define DLL_PREEXTENSION "lib"
#define LIBRARY_EXTENSION ".so"
#if ARCHITECTURE_IS_X86
#define DLL_EXTENSION "_srv.so"
#define DETOUR_SYMBOL_ID 0
#define MODULE_EXTENSION "linux64"
#else
#define DLL_EXTENSION ".so"
#define DETOUR_SYMBOL_ID 1
#define MODULE_EXTENSION "linux"
#endif
#else
#define DLL_PREEXTENSION ""
#define DLL_EXTENSION ".dll"
#define LIBRARY_EXTENSION ".dll"
#if ARCHITECTURE_IS_X86
#define DETOUR_SYMBOL_ID 2
#define MODULE_EXTENSION "win32"
#else
#define DETOUR_SYMBOL_ID 3
#define MODULE_EXTENSION "win64"
#endif
#endif

	template<class T>
	inline T* ResolveSymbol(
		SourceSDK::FactoryLoader& pLoader, const std::vector<Symbol>& pSymbols
	)
	{
	#if DETOUR_SYMBOL_ID != 0
		if ((pSymbols.size()-1) < DETOUR_SYMBOL_ID)
			return NULL;
	#endif

	#if defined SYSTEM_WINDOWS
		auto iface = reinterpret_cast<T**>(symfinder.Resolve(
			pLoader.GetModule(), pSymbols[DETOUR_SYMBOL_ID].name.c_str(), pSymbols[DETOUR_SYMBOL_ID].length
		));
		return iface != nullptr ? *iface : nullptr;
	#elif defined SYSTEM_POSIX
		return reinterpret_cast<T*>(symfinder.Resolve(
			pLoader.GetModule(), pSymbols[DETOUR_SYMBOL_ID].name.c_str(), pSymbols[DETOUR_SYMBOL_ID].length
		));
	#endif
	}

	template<class T>
	inline T* ResolveSymbolFromLea(void* pModule, const std::vector<Symbol>& pSymbols)
	{
	#if DETOUR_SYMBOL_ID != 0
		if ((pSymbols.size()-1) < DETOUR_SYMBOL_ID)
			return NULL;
	#endif

		void* matchAddr = GetFunction(pModule, pSymbols[DETOUR_SYMBOL_ID]);
		if (matchAddr == NULL)
		{
			Warning(PROJECT_NAME ": Failed to get matchAddr!\n");
			return NULL;
		}

	#if defined(SYSTEM_WINDOWS)
		uint8_t* ip = reinterpret_cast<uint8_t*>((char*)(matchAddr) + pSymbols[DETOUR_SYMBOL_ID].offset);
	#else
		uint8_t* ip = reinterpret_cast<uint8_t*>(matchAddr + pSymbols[DETOUR_SYMBOL_ID].offset);
	#endif

		//
		if (ip[0] == 0x48) {
			const size_t instrLen = 7;
			int32_t disp = *reinterpret_cast<int32_t*>(ip + 3); // disp32 at offset 3
			uint8_t* next = ip + instrLen;                      // RIP after the instruction
			void* gEntListAddr = next + disp;                   // final address = next + disp32

		#if defined SYSTEM_WINDOWS
			auto iface = reinterpret_cast<T**>(gEntListAddr);
			return iface != nullptr ? *iface : nullptr;
		#elif defined SYSTEM_POSIX
			return reinterpret_cast<T*>(gEntListAddr);
		#endif
		}

		Warning(PROJECT_NAME ": Failed to match LEA bytes!\n");
		return NULL;
	}

	inline void* GetFunction(void* pModule, std::vector<Symbol> pSymbols)
	{
#if DETOUR_SYMBOL_ID != 0
		if ((pSymbols.size()-1) < DETOUR_SYMBOL_ID)
			return NULL;
#endif

		return GetFunction(pModule, pSymbols[DETOUR_SYMBOL_ID]);
	}

	inline void Create(Detouring::Hook* pHook, const char* strName, void* pModule, std::vector<Symbol> pSymbols, void* pHookFunc, unsigned int category = 0)
	{
#if DETOUR_SYMBOL_ID != 0
		if ((pSymbols.size()-1) < DETOUR_SYMBOL_ID)
		{
			CheckFunction(nullptr, strName);
			return;
		}
#endif

		Create(pHook, strName, pModule, pSymbols[DETOUR_SYMBOL_ID], pHookFunc, category);
	}
}