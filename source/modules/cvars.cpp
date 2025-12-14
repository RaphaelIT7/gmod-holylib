#include "LuaInterface.h"
#include "module.h"
#include "detours.h"
#include "lua.h"
#include "ankerl/unordered_dense.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CCVarsModule : public IModule
{
public:
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
#if ARCHITECTURE_IS_X86
	void InitDetour(bool bServerInit) override;
	void Shutdown() override;
#endif
	const char* Name() override { return "cvars"; };
	int Compatibility() override { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
	bool SupportsMultipleLuaStates() override { return true; };
};

static CCVarsModule g_pCVarsModule;
IModule* pCVarsModule = &g_pCVarsModule;

#if ARCHITECTURE_IS_X86
struct CaseInsensitiveHash {
    size_t operator()(const std::string_view& s) const noexcept {
        size_t h = 0;
        for (char c : s) {
            char lower = (c >= 'A' && c <= 'Z') ? c + 32 : c;
            h = h * 31 + lower;
        }
        return h;
    }
};

struct CaseInsensitiveEqual {
    bool operator()(const std::string_view& a, const std::string_view& b) const noexcept {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            char ca = (a[i] >= 'A' && a[i] <= 'Z') ? a[i] + 32 : a[i];
            char cb = (b[i] >= 'A' && b[i] <= 'Z') ? b[i] + 32 : b[i];
            if (ca != cb) return false;
        }
        return true;
    }
};

static ankerl::unordered_dense::map<std::string_view, ConCommandBase*, CaseInsensitiveHash, CaseInsensitiveEqual> g_pCommandBaseNames;

/*
 * BUG: The Source engine uses Q_stricmp -> V_stricmp which is case insensitive, so we need to account for that.
 */
inline void AddCommandBaseName(ConCommandBase* variable);
inline ConCommandBase* FindCommandBaseName(const char* name);
inline void AddCommandBaseName(ConCommandBase* variable)
{
	g_pCommandBaseNames.try_emplace(variable->GetName(), variable);

	/*
	 * BUG: For some reason, children convars are NEVER registered.
	 * The Engine is forgetting to register them properly and seemingly relies on the logic that children convars are automatically added, which doesn't seem to be intentional?
	 */
	variable = variable->GetNext();
	if (variable && FindCommandBaseName(variable->GetName()) == nullptr)
		AddCommandBaseName(variable);
}

inline void RemoveCommandBaseName(const ConCommandBase* variable)
{
	auto it = g_pCommandBaseNames.find(variable->GetName());
	if (it != g_pCommandBaseNames.end())
		g_pCommandBaseNames.erase(it);
}

inline ConCommandBase* FindCommandBaseName(const char* name)
{
	auto it = g_pCommandBaseNames.find(name);
	if (it != g_pCommandBaseNames.end())
		return it->second;

	return nullptr;
}

Detouring::Hook detour_CCvar_RegisterConCommand;
void hook_CCvar_RegisterConCommand(ICvar* pCVar, ConCommandBase* variable)
{
	if (g_pCVarsModule.InDebug())
		Msg(PROJECT_NAME ": About to register \"%s\" convar\n", variable->GetName());

	// Internally calls FindCommandBase so expect a single "Failed to find xyz" message when debugging
	detour_CCvar_RegisterConCommand.GetTrampoline<Symbols::CCvar_RegisterConCommand>()(pCVar, variable);

	if (!variable->GetNext())
	{
		if (g_pCVarsModule.InDebug())
			Msg(PROJECT_NAME ": failed to register \"%s\" convar\n", variable->GetName());

		return; // Failed to register
	}

	AddCommandBaseName(variable);

	if (g_pCVarsModule.InDebug())
		Msg(PROJECT_NAME ": registered \"%s\" convar\n", variable->GetName());
}

Detouring::Hook detour_CCvar_UnregisterConCommand;
void hook_CCvar_UnregisterConCommand(ICvar* pCVar, ConCommandBase* pCommandToRemove)
{
	detour_CCvar_UnregisterConCommand.GetTrampoline<Symbols::CCvar_UnregisterConCommand>()(pCVar, pCommandToRemove);

	if (pCommandToRemove->GetNext())
	{
		if (g_pCVarsModule.InDebug())
			Msg(PROJECT_NAME ": failed to unregister \"%s\" convar\n", pCommandToRemove->GetName());

		return; // Failed to unregister.
	}

	RemoveCommandBaseName(pCommandToRemove);

	if (g_pCVarsModule.InDebug())
		Msg(PROJECT_NAME ": unregistered \"%s\" convar\n", pCommandToRemove->GetName());
}

Detouring::Hook detour_CCvar_UnregisterConCommands;
void hook_CCvar_UnregisterConCommands(ICvar* pCVar, CVarDLLIdentifier_t id)
{
#if ARCHITECTURE_IS_X86_64
	ICvar::Iterator iter(pCVar);
	for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
	{
		ConCommandBase* pCommand = iter.Get();
#else
	for (const ConCommandBase* pCommand = pCVar->GetCommands(); pCommand; pCommand = pCommand->GetNext())
	{
#endif
		if (pCommand->GetDLLIdentifier() == id)
		{
			RemoveCommandBaseName(pCommand);

			if (g_pCVarsModule.InDebug())
				Msg(PROJECT_NAME ": Unregistered \"%s\" convars\n", pCommand->GetName());
		}
	}

	detour_CCvar_UnregisterConCommands.GetTrampoline<Symbols::CCvar_UnregisterConCommands>()(pCVar, id);
}

Detouring::Hook detour_CCvar_FindCommandBaseConst;
const ConCommandBase* hook_CCvar_FindCommandBaseConst(ICvar* pCVar, const char* name)
{
	ConCommandBase* pVar = FindCommandBaseName(name);

	if (!pVar && g_pCVarsModule.InDebug())
		Msg(PROJECT_NAME ": Failed to find \"%s\" convar!\n", name);

	return pVar;
}

Detouring::Hook detour_CCvar_FindCommandBase;
ConCommandBase* hook_CCvar_FindCommandBase(ICvar* pCVar, const char* name)
{
	ConCommandBase* pVar = FindCommandBaseName(name);

	if (!pVar && g_pCVarsModule.InDebug())
		Msg(PROJECT_NAME ": Failed to find \"%s\" convar!\n", name);

	return pVar;
}
#endif

static constexpr int nConVarPreAllocSize = 5000;
LUA_FUNCTION_STATIC(cvars_GetAll)
{
	VPROF_BUDGET("HolyLib(LUA) - cvars.GetAll", VPROF_BUDGETGROUP_HOLYLIB);

	LUA->PreCreateTable(nConVarPreAllocSize, 0);
		int idx = 0;
#if ARCHITECTURE_IS_X86_64
		ICvar::Iterator iter(g_pCVar);

		for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
		{
			ConCommandBase* var = iter.Get();
#else
		for (const ConCommandBase* var = g_pCVar->GetCommands(); var; var = var->GetNext())
		{
#endif
			if (var->IsCommand())
				continue;

			LUA->PushUserType((ConVar*)var, GarrysMod::Lua::Type::ConVar);

			Util::RawSetI(LUA, -2, ++idx);
		}

	return 1;
}

LUA_FUNCTION_STATIC(cvars_SetValue)
{
	Util::DoUnsafeCodeCheck(LUA);

	const char* pName = LUA->CheckString(1);
	ConVar* pConVar = g_pCVar->FindVar(pName);
	if (!pConVar)
	{
		LUA->PushBool(false);
		return 1;
	}

	pConVar->SetValue(LUA->CheckString(2));
	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION_STATIC(cvars_Unregister)
{
	Util::DoUnsafeCodeCheck(LUA);

	ConCommandBase* pConVar;
	if (LUA->IsType(1, GarrysMod::Lua::Type::String))
		pConVar = g_pCVar->FindCommandBase(LUA->GetString(1));
	else
		pConVar = Get_ConVar(LUA, 1, true);

	if (!pConVar)
		LUA->ThrowError("Failed to find ConVar/ConCommand!");
	
	g_pCVar->UnregisterConCommand(pConVar);
	return 0;
}

LUA_FUNCTION_STATIC(cvars_Find)
{
	const char* strName = LUA->CheckString(1);

	IConVar* pConVar = g_pCVar->FindVar(strName);
	if (!pConVar)
	{
		LUA->PushNil();
		return 1;
	}

	LUA->PushUserType((ConVar*)pConVar, GarrysMod::Lua::Type::ConVar);
	return 1;
}

void CCVarsModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Util::StartTable(pLua);
		Util::AddFunc(pLua, cvars_GetAll, "GetAll");
		Util::AddFunc(pLua, cvars_SetValue, "SetValue");
		Util::AddFunc(pLua, cvars_Unregister, "Unregister");
		Util::AddFunc(pLua, cvars_Find, "Find");
	Util::FinishTable(pLua, "cvar");
}

#if ARCHITECTURE_IS_X86
void CCVarsModule::InitDetour(bool bServerInit)
{
	if (!bServerInit)
		return;

	// These detours are obsolete once the new gmod update is out, see: https://github.com/Facepunch/garrysmod-requests/issues/2600
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

	g_pCommandBaseNames.reserve(nConVarPreAllocSize);

	ICvar* pCVar = vstdlib_loader.GetInterface<ICvar>(CVAR_INTERFACE_VERSION); // g_pCVar isn't initialized yet since tiers didn't connect yet.
#if ARCHITECTURE_IS_X86_64
		ICvar::Iterator iter(pCVar);
		for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
		{
			ConCommandBase* pCommand = iter.Get();
#else
		for (ConCommandBase* pCommand = pCVar->GetCommands(); pCommand; pCommand = pCommand->GetNext())
		{
#endif
			AddCommandBaseName(pCommand);
		}
}

void CCVarsModule::Shutdown()
{
	g_pCommandBaseNames.clear();
}
#endif

void CCVarsModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "cvar");
}