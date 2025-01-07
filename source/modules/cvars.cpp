#include "LuaInterface.h"
#include "module.h"
#include "detours.h"
#include "lua.h"

class CCVarsModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
#if ARCHITECTURE_IS_X86
	virtual void InitDetour(bool bServerInit) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
#endif
	virtual const char* Name() { return "cvars"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
};

static CCVarsModule g_pCVarsModule;
IModule* pCVarsModule = &g_pCVarsModule;

#if ARCHITECTURE_IS_X86
static std::unordered_map<std::string_view, ConCommandBase*> g_pConCommandNames;
Detouring::Hook detour_CCvar_RegisterConCommand;
void hook_CCvar_RegisterConCommand(ICvar* pCVar, ConCommandBase* variable)
{
	detour_CCvar_RegisterConCommand.GetTrampoline<Symbols::CCvar_RegisterConCommand>()(pCVar, variable);

	if (!variable->GetNext())
		return; // Failed to register

	g_pConCommandNames[variable->GetName()] = variable;
}

Detouring::Hook detour_CCvar_UnregisterConCommand;
void hook_CCvar_UnregisterConCommand(ICvar* pCVar, ConCommandBase* pCommandToRemove)
{
	detour_CCvar_UnregisterConCommand.GetTrampoline<Symbols::CCvar_RegisterConCommand>()(pCVar, pCommandToRemove);

	if (pCommandToRemove->GetNext())
		return; // Failed to unregister.

	auto it = g_pConCommandNames.find(pCommandToRemove->GetName());
	if (it != g_pConCommandNames.end())
		g_pConCommandNames.erase(it);
}

Detouring::Hook detour_CCvar_UnregisterConCommands;
void hook_CCvar_UnregisterConCommands(ICvar* pCVar, CVarDLLIdentifier_t id)
{
#if ARCHITECTURE_IS_X86_64
	ICvar::Iterator it(g_pCVar);
	it.SetFirst();
	const ConCommandBase *pCommand = it.Get();
	for (; pCommand; it.Next())
	{
		pCommand = it.Get();
#else
	for (const ConCommandBase *pCommand = g_pCVar->GetCommands(); pCommand; pCommand = pCommand->GetNext())
	{
#endif
		if (pCommand->GetDLLIdentifier() == id)
		{
			auto it = g_pConCommandNames.find(pCommand->GetName());
			if (it != g_pConCommandNames.end())
				g_pConCommandNames.erase(it);
		}
	}

	detour_CCvar_UnregisterConCommands.GetTrampoline<Symbols::CCvar_UnregisterConCommands>()(pCVar, id);
}

Detouring::Hook detour_CCvar_FindCommandBaseConst;
const ConCommandBase* hook_CCvar_FindCommandBaseConst(ICvar* pCVar, const char* name)
{
	auto it = g_pConCommandNames.find( name );
	if (it != g_pConCommandNames.end())
		return it->second;

	if (g_pCVarsModule.InDebug())
		Msg("holylib: Failed to find %s convar!\n", name);

	return NULL;
}

Detouring::Hook detour_CCvar_FindCommandBase;
ConCommandBase* hook_CCvar_FindCommandBase(ICvar* pCVar, const char* name)
{
	auto it = g_pConCommandNames.find( name );
	if (it != g_pConCommandNames.end())
		return it->second;

	if (g_pCVarsModule.InDebug())
		Msg("holylib: Failed to find %s convar!\n", name);

	return NULL;
}
#endif

LUA_FUNCTION_STATIC(cvars_GetAll)
{
	VPROF_BUDGET("HolyLib(LUA) - cvars.GetAll", VPROF_BUDGETGROUP_HOLYLIB);

	LUA->CreateTable();
		int idx = 0;
#if ARCHITECTURE_IS_X86_64
		ICvar::Iterator it(g_pCVar);
		it.SetFirst();
		const ConCommandBase *var = it.Get();
		for (; var; it.Next())
		{
			var = it.Get();
#else
		for (const ConCommandBase *var = g_pCVar->GetCommands(); var; var = var->GetNext())
		{
#endif
			if (var->IsCommand())
				continue;

			LUA->PushUserType((ConVar*)var, GarrysMod::Lua::Type::ConVar);
			Util::RawSetI(-2, ++idx);
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

// ToDo: Port over find optimization later
void CCVarsModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	Util::StartTable();
		Util::AddFunc(cvars_GetAll, "GetAll");
		Util::AddFunc(cvars_SetValue, "SetValue");
		Util::AddFunc(cvars_Unregister, "Unregister");
	Util::FinishTable("cvar");
}

#if ARCHITECTURE_IS_X86
void CCVarsModule::InitDetour(bool bServerInit)
{
	if (!bServerInit)
		return;

	SourceSDK::FactoryLoader vstdlib_loader("vstdlib");
	Detour::Create(
		&detour_CCvar_RegisterConCommand, "CCvar::RegisterConCommand",
		vstdlib_loader.GetModule(), Symbols::CCvar_RegisterConCommandSym,
		(void*)hook_CCvar_RegisterConCommand, m_pID
	);

	Detour::Create(
		&detour_CCvar_UnregisterConCommand, "CCvar::UnregisterConCommand",
		vstdlib_loader.GetModule(), Symbols::CCvar_UnregisterConCommandSym,
		(void*)hook_CCvar_UnregisterConCommand, m_pID
	);

	Detour::Create(
		&detour_CCvar_UnregisterConCommands, "CCvar::UnregisterConCommands",
		vstdlib_loader.GetModule(), Symbols::CCvar_UnregisterConCommandsSym,
		(void*)hook_CCvar_UnregisterConCommands, m_pID
	);

	Detour::Create(
		&detour_CCvar_FindCommandBaseConst, "CCvar::FindCommandBase(const)",
		vstdlib_loader.GetModule(), Symbols::CCvar_FindCommandBaseConstSym,
		(void*)hook_CCvar_FindCommandBaseConst, m_pID
	);

	Detour::Create(
		&detour_CCvar_FindCommandBase, "CCvar::FindCommandBase",
		vstdlib_loader.GetModule(), Symbols::CCvar_FindCommandBaseSym,
		(void*)hook_CCvar_FindCommandBase, m_pID
	);

	ICvar* pCVar = vstdlib_loader.GetInterface<ICvar>(CVAR_INTERFACE_VERSION); // g_pCVar isn't initialized yet since tiers didn't connect yet.
	for (ConCommandBase *pCommand = pCVar->GetCommands(); pCommand; pCommand = pCommand->GetNext())
		g_pConCommandNames.try_emplace(pCommand->GetName(), pCommand);
}

void CCVarsModule::Shutdown()
{
	g_pConCommandNames.clear();
}
#endif

void CCVarsModule::LuaShutdown()
{
	Util::NukeTable("cvar");
}