#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"

class CCVarsModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual const char* Name() { return "cvars"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; };
};

static CCVarsModule g_pCVarsModule;
IModule* pCVarsModule = &g_pCVarsModule;

LUA_FUNCTION_STATIC(cvars_GetAll)
{
	VPROF_BUDGET("HolyLib(LUA) - cvars.GetAll", VPROF_BUDGETGROUP_HOLYLIB);

	LUA->CreateTable();
		int idx = 0;
#ifdef ARCHITECTURE_X86_64
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
			g_Lua->PushNumber(idx);
			g_Lua->PushUserType((ConVar*)var, GarrysMod::Lua::Type::ConVar);
			g_Lua->RawSet(-3);
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

void CCVarsModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	if (Util::PushTable("cvars"))
	{
		Util::AddFunc(cvars_GetAll, "GetAll");
		Util::AddFunc(cvars_SetValue, "SetValue");
	}
	Util::PopTable();
}

void CCVarsModule::LuaShutdown()
{
	if (Util::PushTable("cvars"))
	{
		Util::RemoveFunc("GetAll");
	}
	Util::PopTable();
}