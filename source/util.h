#pragma once

#include <sourcesdk/ILuaInterface.h>
#include "GarrysMod/Lua/Interface.h"
#include <string>
#include "detours.h"
#include "eiface.h"
#include <GarrysMod/FactoryLoader.hpp>
#include <tier3/tier3.h>

extern IVEngineServer* engine;

extern GarrysMod::Lua::IUpdatedLuaInterface* g_Lua;

class CBaseClient;
namespace Util
{
	extern void StartTable();
	extern void AddFunc(GarrysMod::Lua::CFunc, const char*);
	extern void FinishTable(const char*);

	// Gmod's functions:
	extern CBasePlayer* Get_Player(int iStackPos, bool unknown);
	extern CBaseEntity* Get_Entity(int iStackPos, bool unknown);
	extern void Push_Entity(CBaseEntity* pEnt);

	CBaseClient* GetClientByEdict(edict_t* edict);

	extern void AddDetour(); // We load Gmod's functions in there.
}

// Push functions from modules: 
// ToDo: move this at a later point into a seperate file. Maybe into _modules?
extern void Push_bf_read(bf_read* tbl);

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