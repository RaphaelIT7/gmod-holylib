#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

/*
	This Library exists just for fun, wanna mess with the engine? Here are some functions just to do that.
	You got any obsurd idea? Tell me about it :)
*/

class CUnHolyLibModule : public IModule
{
public:
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	const char* Name() override { return "unholylib"; };
	int Compatibility() override { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
};

static CUnHolyLibModule g_pUnHolyLibModule;
IModule* pUnHolyLibModule = &g_pUnHolyLibModule;

extern CGlobalVars* gpGlobals;
LUA_FUNCTION_STATIC(unholylib_SetCurTime)
{
	double curTime = LUA->CheckNumber(1);

	if (!gpGlobals)
		LUA->ThrowError("Failed to load gpGlobals!");

	gpGlobals->curtime = curTime;
	return 0;
}

LUA_FUNCTION_STATIC(unholylib_SetEntIndex)
{
	CBaseEntity* pEntity = Util::Get_Entity(LUA, 1, true);
	int newIndex = (int)LUA->CheckNumber(2);
	if (newIndex < -1 || newIndex >= MAX_EDICTS)
		LUA->ArgError(2, "Index is too large!");

	edict_t* pCurrentEdict = pEntity->edict();
	if (pCurrentEdict)
		pEntity->NetworkProp()->DetachEdict();

	if (newIndex == -1) // Now it's an server-only entity
		return 0;

	edict_t* pEdict = Util::engineserver->PEntityOfEntIndex(newIndex);
	if (!pEdict)
		LUA->ArgError(2, "Failed to find an edict_t for the given index?");

	if (pEdict->IsFree())
	{
		Util::engineserver->CreateEdict(newIndex); // We take it :3
		pEntity->NetworkProp()->AttachEdict(pEdict);
	} else { // GG time to do funnies. Let's swap them
		CServerNetworkProperty* pIndexEnt = static_cast<CServerNetworkProperty*>(pEdict->GetNetworkable());
		if (!pIndexEnt)
			LUA->ThrowError("New index is used yet also it has no CServerNetworkProperty?");

		pIndexEnt->DetachEdict();
		if (pCurrentEdict)
		{
			Util::engineserver->CreateEdict(pCurrentEdict->m_EdictIndex); // DetachEdict above freed it!
			pIndexEnt->AttachEdict(pCurrentEdict);
		}

		if (pEntity->NetworkProp())
		{
			Util::engineserver->CreateEdict(pEdict->m_EdictIndex);
			pEntity->NetworkProp()->AttachEdict(pEdict);
		}
	}

	return 0;
}

void CUnHolyLibModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (!g_pModuleManager.IsUnsafeCodeEnabled())
	{
		Warning(PROJECT_NAME " - unholylib: Tried to use an unsafe library while -holylib_allowunsafe is not active!\n");
		return;
	}

	Util::StartTable(pLua);
		Util::AddFunc(pLua, unholylib_SetCurTime, "SetCurTime");
		Util::AddFunc(pLua, unholylib_SetEntIndex, "SetEntIndex");
	Util::FinishTable(pLua, "unholylib");
}

void CUnHolyLibModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "unholylib");
}