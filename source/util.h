#pragma once

#include "GarrysMod/Lua/Interface.h"
#include <string>
#include "detours.h"
#include "eiface.h"
#include <GarrysMod/FactoryLoader.hpp>
#include <tier3/tier3.h>

extern IVEngineServer* engine;

extern CreateInterfaceFn g_interfaceFactory;
extern CreateInterfaceFn g_gameServerFactory;

extern GarrysMod::Lua::ILuaInterface* g_Lua;

namespace Util
{
	extern void StartTable();
	extern void AddFunc(GarrysMod::Lua::CFunc, const char*);
	extern void FinishTable(const char*);

	// Gmod's functions:
	extern CBasePlayer* Get_Player(int iStackPos, bool unknown);
	extern void Push_Entity(CBaseEntity* pEnt);

	extern void AddDetour(); // We load Gmod's functions in there.
}

#include <scanning/symbolfinder.hpp>
static SymbolFinder symbol_finder;
template<class T>
static inline T* ResolveSymbol(
	SourceSDK::FactoryLoader& loader, const Symbol& symbol
)
{
	if (symbol.type == Symbol::Type::None)
		return nullptr;

#if defined SYSTEM_WINDOWS

	auto iface = reinterpret_cast<T**>(symbol_finder.Resolve(
		loader.GetModule(), symbol.name.c_str(), symbol.length
	));
	return iface != nullptr ? *iface : nullptr;

#elif defined SYSTEM_POSIX

	return reinterpret_cast<T*>(symbol_finder.Resolve(
		loader.GetModule(), symbol.name.c_str(), symbol.length
	));

#endif

}

template<class T>
static inline T* ResolveSymbols(
	SourceSDK::FactoryLoader& loader, const std::vector<Symbol>& symbols
)
{
	T* iface_pointer = nullptr;
	for (const auto& symbol : symbols)
	{
		iface_pointer = ResolveSymbol<T>(loader, symbol);
		if (iface_pointer != nullptr)
			break;
	}

	return iface_pointer;
}