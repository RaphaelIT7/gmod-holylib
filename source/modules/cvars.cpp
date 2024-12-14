#include "LuaInterface.h"
#include "module.h"
#include "lua.h"

class CCVarsModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual const char* Name() { return "cvars"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
};

static CCVarsModule g_pCVarsModule;
IModule* pCVarsModule = &g_pCVarsModule;

LUA_FUNCTION_STATIC(cvars_GetAll)
{
	VPROF_BUDGET("HolyLib(LUA) - cvars.GetAll", VPROF_BUDGETGROUP_HOLYLIB);

	LUA->CreateTable();
		int idx = 0;
#if ARCHITECTURE_IS_X86_64
		ICvar::Iterator it(g_pCVar);
		it.SetFirst();
		const ConCommandBase *var = it.Get();
		for ( ; var ; it.Next())
		{
			var = it.Get();
#else
		for ( const ConCommandBase *var = g_pCVar->GetCommands() ; var ; var = var->GetNext())
		{
#endif
			if ( var->IsCommand() )
				continue;

			++idx;
			LUA->PushNumber(idx);
			LUA->PushUserType((ConVar*)var, GarrysMod::Lua::Type::ConVar);
			LUA->RawSet(-3);
		}

	return 1;
}

LUA_FUNCTION_STATIC(cvars_SetValue)
{
	const char* pName = LUA->CheckString(1);
	ConVar* pConVar = g_pCVar->FindVar(pName);
	if (!pConVar)
		return 0;

	pConVar->SetValue(LUA->CheckString(2));
	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION_STATIC(cvars_Unregister)
{
	ConCommandBase* pConVar;
	if (LUA->IsType(1, GarrysMod::Lua::Type::String))
		pConVar = g_pCVar->FindCommandBase(LUA->GetString(1));
	else
		pConVar = Get_ConVar(1, true);

	if (!pConVar)
		LUA->ThrowError("Failed to find ConVar/ConCommand!");
	
	g_pCVar->UnregisterConCommand(pConVar);
	return 0;
}

void CCVarsModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Util::StartTable(pLua);
		Util::AddFunc(pLua, cvars_GetAll, "GetAll");
		Util::AddFunc(pLua, cvars_SetValue, "SetValue");
		Util::AddFunc(pLua, cvars_Unregister, "Unregister");
	Util::FinishTable(pLua, "cvar");
}

void CCVarsModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "cvar");
}