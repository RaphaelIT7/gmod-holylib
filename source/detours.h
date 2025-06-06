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

#ifdef DLL_TOOLS
#ifdef WIN32
#include <Windows.h>
#undef GetObject
#undef GetClassName
#define DLL_Handle HMODULE
#define DLL_LoadModule(name, _) LoadLibrary(name)
#define DLL_UnloadModule(handle) FreeLibrary((DLL_Handle)handle)
#define DLL_GetAddress(handle, name) GetProcAddress((DLL_Handle)handle, name)
#else
#include <dlfcn.h>
#define DLL_Handle void*
#define DLL_LoadModule(name, type) dlopen(name, type)
#define DLL_UnloadModule(handle) dlclose(handle)
#define DLL_GetAddress(handle, name) dlsym(handle, name)
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
	extern unsigned int g_pCurrentCategory;

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
#if ARCHITECTURE_IS_X86
#define DLL_EXTENSION "_srv.so"
#define DETOUR_SYMBOL_ID 0
#else
#define DLL_EXTENSION ".so"
#define DETOUR_SYMBOL_ID 1
#endif
#else
#define DLL_EXTENSION ".dll"
#if ARCHITECTURE_IS_X86
#define DETOUR_SYMBOL_ID 2
#else
#define DETOUR_SYMBOL_ID 3
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