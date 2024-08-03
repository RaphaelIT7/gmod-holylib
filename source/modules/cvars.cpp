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

void CCVarsModule::LuaInit(bool bServerInit)
{
	if (!bServerInit)
	{
		g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
			g_Lua->GetField(-1, "cvars");
				if (g_Lua->IsType(-1, GarrysMod::Lua::Type::Table))
				{
					Util::AddFunc(cvars_GetAll, "GetAll");
				}
		g_Lua->Pop(2);
	}
}

void CCVarsModule::LuaShutdown()
{
	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		g_Lua->GetField(-1, "cvars");
			if (g_Lua->IsType(-1, GarrysMod::Lua::Type::Table))
			{
				g_Lua->PushNil();
				g_Lua->SetField(-2, "GetAll");
			}
	g_Lua->Pop(2);
}