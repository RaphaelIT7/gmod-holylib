#pragma once

#include <GarrysMod/Lua/LuaInterface.h>
#include <GarrysMod/Symbol.hpp>
#include <detouring/hook.hpp>
#include <scanning/symbolfinder.hpp>
#include <vector>

class CBaseEntity;
class CBasePlayer;
class IClient;

namespace Symbols
{
	//---------------------------------------------------------------------------------
	// Purpose: All Base Symbols
	//---------------------------------------------------------------------------------
	typedef bool (*InitLuaClasses)(GarrysMod::Lua::ILuaInterface*);
	const Symbol InitLuaClassesSym = Symbol::FromName("_Z14InitLuaClassesP13ILuaInterface");

	typedef CBasePlayer* (*Get_Player)(int, bool);
	const Symbol Get_PlayerSym = Symbol::FromName("_Z10Get_Playerib");

	typedef void (*Push_Entity)(CBaseEntity*);
	const Symbol Push_EntitySym = Symbol::FromName("_Z11Push_EntityP11CBaseEntity");

	//---------------------------------------------------------------------------------
	// Purpose: All other Symbols
	//---------------------------------------------------------------------------------
	typedef bool (*CServerGameDLL_ShouldHideServer)();
	const Symbol CServerGameDLL_ShouldHideServerSym = Symbol::FromName("_ZN14CServerGameDLL16ShouldHideServerEv");

	const Symbol s_GameSystemsSym = Symbol::FromName("_ZL13s_GameSystems");

	typedef void* (*CGameClient_ProcessGMod_ClientToServer)(IClient*, void*);
	const Symbol CGameClient_ProcessGMod_ClientToServerSym = Symbol::FromName("_ZN11CGameClient26ProcessGMod_ClientToServerEP23CLC_GMod_ClientToServer");
}

//---------------------------------------------------------------------------------
// Purpose: Detour functions
//---------------------------------------------------------------------------------
namespace Detour
{
	inline bool CheckValue(const char* msg, const char* name, bool ret)
	{
		if (!ret) {
			Msg("[holylib] Failed to %s %s!\n", msg, name);
			return false;
		}

		return true;
	}

	inline bool CheckValue(const char* name, bool ret)
	{
		return CheckValue("get function", name, ret);
	}

	template<class T>
	bool CheckFunction(T func, const char* name)
	{
		return CheckValue("get function", name, func != nullptr);
	}

	extern void* GetFunction(void* module, Symbol symbol);
	extern void Create(Detouring::Hook* hook, const char* name, void* module, Symbol symbol, void* func, unsigned int category);
	extern void Remove(unsigned int category); // 0 = All
	extern unsigned int g_pCurrentCategory = 0;
}