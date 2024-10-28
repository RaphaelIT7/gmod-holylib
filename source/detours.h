#pragma once

#define TIER0_DLL_EXPORT
#include <GarrysMod/FactoryLoader.hpp>
#include <scanning/symbolfinder.hpp>
#include <GarrysMod/Symbol.hpp>
#include <detouring/hook.hpp>
#include "Platform.hpp"
#include "tier0/dbg.h"
#include <vector>

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

	inline bool CheckValue(const char* msg, const char* name, bool ret)
	{
		if (!ret) {
			Msg("holylib: Failed to %s %s!\n", msg, name);
			return false;
		}

		return true;
	}

	inline bool CheckValue(const char* name, bool ret)
	{
		return CheckValue("get function", name, ret);
	}

	inline bool CheckFunction(void* func, const char* name)
	{
		return CheckValue("get function", name, func != nullptr);
	}

	extern void* GetFunction(void* module, Symbol symbol);
	extern void Create(Detouring::Hook* hook, const char* name, void* module, Symbol symbol, void* func, unsigned int category);
	extern void Remove(unsigned int category); // 0 = All
	extern void ReportLeak();
	extern unsigned int g_pCurrentCategory;

	extern SymbolFinder symfinder;
	template<class T>
	inline T* ResolveSymbol(
		SourceSDK::FactoryLoader& loader, const Symbol& symbol
	)
	{
		if (symbol.type == Symbol::Type::None)
			return nullptr;

	#if defined SYSTEM_WINDOWS
		auto iface = reinterpret_cast<T**>(symfinder.Resolve(
			loader.GetModule(), symbol.name.c_str(), symbol.length
		));
		return iface != nullptr ? *iface : nullptr;
	#elif defined SYSTEM_POSIX
		return reinterpret_cast<T*>(symfinder.Resolve(
			loader.GetModule(), symbol.name.c_str(), symbol.length
		));
	#endif
	}

#ifdef SYSTEM_LINUX
#if ARCHITECTURE_IS_X86
#define DETOUR_SYMBOL_ID 0
#else
#define DETOUR_SYMBOL_ID 1
#endif
#else
#if ARCHITECTURE_IS_X86
#define DETOUR_SYMBOL_ID 2
#else
#define DETOUR_SYMBOL_ID 3
#endif
#endif

	template<class T>
	inline T* ResolveSymbol(
		SourceSDK::FactoryLoader& loader, const std::vector<Symbol>& symbols
	)
	{
	#if DETOUR_SYMBOL_ID != 0
		if ((symbols.size()-1) < DETOUR_SYMBOL_ID)
			return NULL;
	#endif

	#if defined SYSTEM_WINDOWS
		auto iface = reinterpret_cast<T**>(symfinder.Resolve(
			loader.GetModule(), symbols[DETOUR_SYMBOL_ID].name.c_str(), symbols[DETOUR_SYMBOL_ID].length
		));
		return iface != nullptr ? *iface : nullptr;
	#elif defined SYSTEM_POSIX
		return reinterpret_cast<T*>(symfinder.Resolve(
			loader.GetModule(), symbols[DETOUR_SYMBOL_ID].name.c_str(), symbols[DETOUR_SYMBOL_ID].length
		));
	#endif
	}

	inline void* GetFunction(void* module, std::vector<Symbol> symbols)
	{
#if DETOUR_SYMBOL_ID != 0
		if ((symbols.size()-1) < DETOUR_SYMBOL_ID)
			return NULL;
#endif

		return GetFunction(module, symbols[DETOUR_SYMBOL_ID]);
	}

	inline void Create(Detouring::Hook* hook, const char* name, void* module, std::vector<Symbol> symbols, void* func, unsigned int category)
	{
#if DETOUR_SYMBOL_ID != 0
		if ((symbols.size()-1) < DETOUR_SYMBOL_ID)
		{
			CheckFunction(nullptr, name);
			return;
		}
#endif

		Create(hook, name, module, symbols[DETOUR_SYMBOL_ID], func, category);
	}
}