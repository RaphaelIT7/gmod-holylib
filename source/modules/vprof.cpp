#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <map>
#include <vprof.h>

class CVProfModule : public IModule
{
public:
#ifdef ARCHITECTURE_X86_64
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
#endif
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "vprof"; };
	virtual int Compatibility() { return LINUX32 | WINDOWS32; }; // NOTE for myself: Linux64 seemingly doesn't have vprof enabled! so don't suppositly add compatbility!
};

extern ConVar holylib_sv_stressbots;
static void OnSV_StressBotsChange(IConVar* var, const char *pOldValue, float flOldValue)
{
	ConVar* sv_stressbots = cvar->FindVar("sv_stressbots");
	if (!sv_stressbots) {
		Warning("holylib: Failed to find sv_stressbots!\n");
		return;
	}

	sv_stressbots->SetValue(holylib_sv_stressbots.GetString());
}

// Reminder: Look UP before adding static.
ConVar holylib_sv_stressbots("holylib_sv_stressbots", "0", 0, "Sets sv_stressbots. (sv_stressbots will be available in the next update)", OnSV_StressBotsChange);
static ConVar holylib_vprof_exportreport("holylib_vprof_exportreport", "1", 0, "If enabled, vprof results will be dumped into a file in the vprof/ folder");
static ConVar holylib_vprof_profilelua("holylib_vprof_profilelua", "0", 0);

static CVProfModule g_pVProfModule;
IModule* pVProfModule = &g_pVProfModule;
static std::string GetCurrentTime() { // Yoink from vprof module
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H-%M-%S");

    return ss.str();
}

static std::stringstream ss;
#ifndef ARCHITECTURE_X86_64
static SpewRetval_t VProf_Spew(SpewType_t type, const char *msg)
{
	ss << msg;

	return SPEW_CONTINUE;
}

static SpewOutputFunc_t originalSpew = NULL;
static void StartSpew()
{
	originalSpew = GetSpewOutputFunc();
	SpewOutputFunc(VProf_Spew);
}

static void FinishSpew()
{
	SpewOutputFunc(originalSpew);
	originalSpew = NULL;
}
#else
static Detouring::Hook detour_Msg; // Yes. I don't like this.
static void hook_Msg(PRINTF_FORMAT_STRING const tchar* pMsg, ...)
{
	va_list args;
	va_start(args, pMsg);

	int size = vsnprintf(NULL, 0, pMsg, args);
	if (size < 0) {
		va_end(args);
		return;
	}

	va_end(args);
	va_start(args, pMsg);
	char* buffer = new char[size + 1];
	vsnprintf(buffer, size + 1, pMsg, args);

	ss << buffer;

	delete[] buffer;
	va_end(args);
}

static void StartSpew() // Only enable the hook if we're dumping vprof. We don't want to permanently check for vprof. This would waste performance.
{
	if (!detour_Msg.IsValid())
		return;

	detour_Msg.Enable();
}

static void FinishSpew()
{
	if (!detour_Msg.IsEnabled())
		return;

	detour_Msg.Disable();
}
#endif

static Detouring::Hook detour_CVProfile_OutputReport;
static void hook_CVProfile_OutputReport(void* fancy, int type, const tchar* pszStartMode, int budgetGroupID)
{
	if (!holylib_vprof_exportreport.GetBool())
	{
		detour_CVProfile_OutputReport.GetTrampoline<Symbols::CVProfile_OutputReport>()(fancy, type, pszStartMode, budgetGroupID);
		return;
	}

	StartSpew();

	detour_CVProfile_OutputReport.GetTrampoline<Symbols::CVProfile_OutputReport>()(fancy, type, pszStartMode, budgetGroupID);

	FinishSpew();

	if (!g_pFullFileSystem->IsDirectory("vprof", "MOD"))
	{
		if (g_pFullFileSystem->FileExists("vprof", "MOD"))
		{
			Msg("vprof/ is a file? Please delete it or disable vprof_exportreport.\n");
			return;
		}

		g_pFullFileSystem->CreateDirHierarchy("vprof", "MOD");
	}

	std::string filename = GetCurrentTime();
	filename = "vprof/" + filename + ".txt";
	FileHandle_t fh = g_pFullFileSystem->Open(filename.c_str(), "a+", "MOD");
	if (fh)
	{
		std::string str = ss.str();
		g_pFullFileSystem->Write(str.c_str(), str.length(), fh);  
		Msg("holylib: Wrote vprof report into %s\n", filename.c_str());

		g_pFullFileSystem->Close(fh);
	}

	ss.str("");
}
/*
 * There is no point in adding VPROF to CallWithArgs since it's useless.  
 * It isn't performance intensive since it only pushes the gamemode.Call thingy with the given hook.  
 * The real performance will be shown in CLuaGamemode::CallFinish which should be the focus.  
 */

static const char* pCurrentGamemodeFunction = NULL;
//static std::map<int, std::string> CallWithArgs_strs;
static Detouring::Hook detour_CLuaGamemode_CallWithArgs;
static void* hook_CLuaGamemode_CallWithArgs(void* funky_srv, int pool)
{
	if (!g_Lua)
		return detour_CLuaGamemode_CallWithArgs.GetTrampoline<Symbols::CLuaGamemode_CallWithArgs>()(funky_srv, pool);

	/*const char* pStr = nullptr;
	auto it = CallWithArgs_strs.find(pool);
	if (it == CallWithArgs_strs.end())
	{
		CallWithArgs_strs[pool] = "CLuaGamemode::CallWithArgs (" + (std::string)g_Lua->GetPooledString(pool) + ")";
		pStr = CallWithArgs_strs[pool].c_str();
	} else {
		pStr = it->second.c_str();
	}*/

	pCurrentGamemodeFunction = g_Lua->GetPooledString(pool);

	//VPROF_BUDGET(pStr, "GMOD");

	return detour_CLuaGamemode_CallWithArgs.GetTrampoline<Symbols::CLuaGamemode_CallWithArgs>()(funky_srv, pool);
}

//static std::map<std::string_view, std::string> CallWithArgsStr_strs;
static Detouring::Hook detour_CLuaGamemode_CallWithArgsStr;
static void* hook_CLuaGamemode_CallWithArgsStr(void* funky_srv, const char* str) // NOTE: Only used by gameevent.Listen -> CLuaGameEventListener::FireGameEvent
{
	if (!g_Lua)
		return detour_CLuaGamemode_CallWithArgs.GetTrampoline<Symbols::CLuaGamemode_CallWithArgsStr>()(funky_srv, str);

	/*const char* pStr = nullptr;
	auto it = CallWithArgsStr_strs.find(str);
	if (it == CallWithArgsStr_strs.end())
	{
		CallWithArgsStr_strs[str] = "CLuaGamemode::CallWithArgs (" + (std::string)str + ")";
		pStr = CallWithArgsStr_strs[str].c_str();
	} else {
		pStr = it->second.c_str();
	}*/

	pCurrentGamemodeFunction = str;

	//VPROF_BUDGET(pStr, "GMOD");

	return detour_CLuaGamemode_CallWithArgs.GetTrampoline<Symbols::CLuaGamemode_CallWithArgsStr>()(funky_srv, str);
}

static std::map<std::string_view, std::string> CallFinish_strs;
static Detouring::Hook detour_CLuaGamemode_CallFinish;
static void* hook_CLuaGamemode_CallFinish(void* funky_srv, int pArgs)
{
	if (!g_Lua || !pCurrentGamemodeFunction)
		return detour_CLuaGamemode_CallFinish.GetTrampoline<Symbols::CLuaGamemode_CallFinish>()(funky_srv, pArgs);

	const char* pStr = nullptr;
	auto it = CallFinish_strs.find(pCurrentGamemodeFunction);
	if (it == CallFinish_strs.end())
	{
		CallFinish_strs[pCurrentGamemodeFunction] = "CLuaGamemode::CallFinish (" + (std::string)pCurrentGamemodeFunction + ")";
		pStr = CallFinish_strs[pCurrentGamemodeFunction].c_str();
	} else {
		pStr = it->second.c_str();
	}

	VPROF_BUDGET(pStr, "GMOD");
	pCurrentGamemodeFunction = NULL;

	return detour_CLuaGamemode_CallFinish.GetTrampoline<Symbols::CLuaGamemode_CallFinish>()(funky_srv, pArgs);
}

static std::map<int, std::string> Call_strs;
static Detouring::Hook detour_CLuaGamemode_Call;
static void* hook_CLuaGamemode_Call(void* funky_srv, int pool)
{
	if (!g_Lua)
		return detour_CLuaGamemode_Call.GetTrampoline<Symbols::CLuaGamemode_Call>()(funky_srv, pool);

	const char* pStr = nullptr;
	auto it = Call_strs.find(pool);
	if (it == Call_strs.end())
	{
		Call_strs[pool] = "CLuaGamemode::Call (" + (std::string)g_Lua->GetPooledString(pool) + ")";
		pStr = Call_strs[pool].c_str();
	} else {
		pStr = it->second.c_str();
	}

	VPROF_BUDGET(pStr, "GMOD");

	return detour_CLuaGamemode_Call.GetTrampoline<Symbols::CLuaGamemode_Call>()(funky_srv, pool);
}

static std::map<std::string_view, std::string> CallStr_strs;
static Detouring::Hook detour_CLuaGamemode_CallStr;
static void* hook_CLuaGamemode_CallStr(void* funky_srv, const char* str)
{
	if (!g_Lua)
		return detour_CLuaGamemode_CallStr.GetTrampoline<Symbols::CLuaGamemode_CallStr>()(funky_srv, str);

	const char* pStr = nullptr;
	auto it = CallStr_strs.find(str);
	if (it == CallStr_strs.end())
	{
		CallStr_strs[str] = "CLuaGamemode::Call (" + (std::string)str + ")";
		pStr = CallStr_strs[str].c_str();
	} else {
		pStr = it->second.c_str();
	}

	VPROF_BUDGET(pStr, "GMOD");

	return detour_CLuaGamemode_CallStr.GetTrampoline<Symbols::CLuaGamemode_CallStr>()(funky_srv, str);
}

/*
 * Scripted Entities calls
 * 
 * Notes for myself:
 * - CScriptedEntity::Call(int iArgs, int iRets) - Don't make the same mistake like I did with CallFinish
 * - CScriptedEntity::Call(int iPooledString) - Unlike the function above, we call a function that has no args & return values like Think.
 */
static const char* pCurrentScriptFunction = nullptr;
static std::map<std::string_view, std::string> ScriptedEntity_StartFunctionStr_strs;
static Detouring::Hook detour_CScriptedEntity_StartFunctionStr;
static void* hook_CScriptedEntity_StartFunctionStr(void* funky_srv, const char* str) // Only used by GetSoundInterests
{
	if (!g_Lua)
		return detour_CScriptedEntity_StartFunctionStr.GetTrampoline<Symbols::CScriptedEntity_StartFunctionStr>()(funky_srv, str);

	const char* pStr = nullptr;
	auto it = ScriptedEntity_StartFunctionStr_strs.find(str);
	if (it == ScriptedEntity_StartFunctionStr_strs.end())
	{
		ScriptedEntity_StartFunctionStr_strs[str] = "CScriptedEntity::Call (" + (std::string)str + ")"; // Vprof is added in CScriptedEntity::Call(int, int)
		pStr = ScriptedEntity_StartFunctionStr_strs[str].c_str();
	} else {
		pStr = it->second.c_str();
	}

	pCurrentScriptFunction = pStr;

	return detour_CScriptedEntity_StartFunctionStr.GetTrampoline<Symbols::CScriptedEntity_StartFunctionStr>()(funky_srv, str);
}

static std::map<int, std::string> CScriptedEntity_StartFunction_strs;
static Detouring::Hook detour_CScriptedEntity_StartFunction;
static void* hook_CScriptedEntity_StartFunction(void* funky_srv, int pool)
{
	if (!g_Lua)
		return detour_CScriptedEntity_StartFunction.GetTrampoline<Symbols::CScriptedEntity_StartFunction>()(funky_srv, pool);

	const char* pStr = nullptr;
	auto it = CScriptedEntity_StartFunction_strs.find(pool);
	if (it == CScriptedEntity_StartFunction_strs.end())
	{
		CScriptedEntity_StartFunction_strs[pool] = "CScriptedEntity::Call (" + (std::string)g_Lua->GetPooledString(pool) + ")";
		pStr = CScriptedEntity_StartFunction_strs[pool].c_str();
	} else {
		pStr = it->second.c_str();
	}

	pCurrentScriptFunction = pStr;

	return detour_CScriptedEntity_StartFunction.GetTrampoline<Symbols::CScriptedEntity_StartFunction>()(funky_srv, pool);
}

static Detouring::Hook detour_CScriptedEntity_Call;
static void* hook_CScriptedEntity_Call(void* funky_srv, int iArgs, int iRets)
{
	if (!g_Lua || !pCurrentScriptFunction)
		return detour_CScriptedEntity_Call.GetTrampoline<Symbols::CScriptedEntity_Call>()(funky_srv, iArgs, iRets);

	VPROF_BUDGET(pCurrentScriptFunction, "GMOD");
	pCurrentScriptFunction = nullptr;

	return detour_CScriptedEntity_Call.GetTrampoline<Symbols::CScriptedEntity_Call>()(funky_srv, iArgs, iRets);
}

static std::map<std::string_view, std::string> CScriptedEntity_CallFunctionStr_strs;
static Detouring::Hook detour_CScriptedEntity_CallFunctionStr;
static void* hook_CScriptedEntity_CallFunctionStr(void* funky_srv, const char* str)
{
	if (!g_Lua)
		return detour_CScriptedEntity_CallFunctionStr.GetTrampoline<Symbols::CScriptedEntity_CallFunctionStr>()(funky_srv, str);

	const char* pStr = nullptr;
	auto it = CScriptedEntity_CallFunctionStr_strs.find(str);
	if (it == CScriptedEntity_CallFunctionStr_strs.end())
	{
		CScriptedEntity_CallFunctionStr_strs[str] = "CScriptedEntity::CallFunction (" + (std::string)str + ")";
		pStr = CScriptedEntity_CallFunctionStr_strs[str].c_str();
	} else {
		pStr = it->second.c_str();
	}

	VPROF_BUDGET(pStr, "GMOD");

	return detour_CScriptedEntity_CallFunctionStr.GetTrampoline<Symbols::CScriptedEntity_CallFunctionStr>()(funky_srv, str);
}

static std::map<int, std::string> CScriptedEntity_CallFunction_strs;
static Detouring::Hook detour_CScriptedEntity_CallFunction;
static void* hook_CScriptedEntity_CallFunction(void* funky_srv, int pool)
{
	if (!g_Lua)
		return detour_CScriptedEntity_CallFunction.GetTrampoline<Symbols::CScriptedEntity_CallFunction>()(funky_srv, pool);

	const char* pStr = nullptr;
	auto it = CScriptedEntity_CallFunction_strs.find(pool);
	if (it == CScriptedEntity_CallFunction_strs.end())
	{
		CScriptedEntity_CallFunction_strs[pool] = "CScriptedEntity::CallFunction (" + (std::string)g_Lua->GetPooledString(pool) + ")";
		pStr = CScriptedEntity_CallFunction_strs[pool].c_str();
	} else {
		pStr = it->second.c_str();
	}

	VPROF_BUDGET(pStr, "GMOD");

	return detour_CScriptedEntity_CallFunction.GetTrampoline<Symbols::CScriptedEntity_CallFunction>()(funky_srv, pool);
}

#ifdef SYSTEM_WINDOWS
static int pClient_CurrentGamemodeFunction = -1;
static Detouring::Hook detour_Client_CLuaGamemode_CallWithArgs;
static void* hook_Client_CLuaGamemode_CallWithArgs(void* funky_srv, int pool)
{
	if (!g_Lua)
		return detour_Client_CLuaGamemode_CallWithArgs.GetTrampoline<Symbols::CLuaGamemode_CallWithArgs>()(funky_srv, pool);

	/*const char* pStr = nullptr;
	auto it = CallWithArgs_strs.find(pool);
	if (it == CallWithArgs_strs.end())
	{
		CallWithArgs_strs[pool] = "CLuaGamemode::CallWithArgs (" + (std::string)g_Lua->GetPooledString(pool) + ")";
		pStr = CallWithArgs_strs[pool].c_str();
	} else {
		pStr = it->second.c_str();
	}*/

	pClient_CurrentGamemodeFunction = pool;

	//VPROF_BUDGET(pStr, "GMOD");

	return detour_Client_CLuaGamemode_CallWithArgs.GetTrampoline<Symbols::CLuaGamemode_CallWithArgs>()(funky_srv, pool);
}

static Detouring::Hook detour_Client_CLuaGamemode_CallFinish;
static void* hook_Client_CLuaGamemode_CallFinish(void* funky_srv, int pArgs)
{
	if (!g_Lua || !pCurrentGamemodeFunction)
		return detour_Client_CLuaGamemode_CallFinish.GetTrampoline<Symbols::CLuaGamemode_CallFinish>()(funky_srv, pArgs);

	const char* pStr = nullptr;
	auto it = CallFinish_strs.find(pCurrentGamemodeFunction);
	if (it == CallFinish_strs.end())
	{
		CallFinish_strs[pCurrentGamemodeFunction] = "CLuaGamemode::CallFinish (" + (std::string)pCurrentGamemodeFunction + ")";
		pStr = CallFinish_strs[pCurrentGamemodeFunction].c_str();
	} else {
		pStr = it->second.c_str();
	}

	VPROF_BUDGET(pStr, "GMOD");
	pCurrentGamemodeFunction = NULL;

	return detour_Client_CLuaGamemode_CallFinish.GetTrampoline<Symbols::CLuaGamemode_CallFinish>()(funky_srv, pArgs);
}

static Detouring::Hook detour_Client_CLuaGamemode_Call;
static void* hook_Client_CLuaGamemode_Call(void* funky_srv, int pool)
{
	if (!g_Lua)
		return detour_Client_CLuaGamemode_Call.GetTrampoline<Symbols::CLuaGamemode_Call>()(funky_srv, pool);

	const char* pStr = nullptr;
	auto it = Call_strs.find(pool);
	if (it == Call_strs.end())
	{
		Call_strs[pool] = "CLuaGamemode::Call (" + (std::string)g_Lua->GetPooledString(pool) + ")";
		pStr = Call_strs[pool].c_str();
	} else {
		pStr = it->second.c_str();
	}

	VPROF_BUDGET(pStr, "GMOD");

	return detour_Client_CLuaGamemode_Call.GetTrampoline<Symbols::CLuaGamemode_Call>()(funky_srv, pool);
}

static const char* pClient_CurrentScriptFunction = nullptr;
static Detouring::Hook detour_Client_CScriptedEntity_StartFunctionStr;
static void* hook_Client_CScriptedEntity_StartFunctionStr(void* funky_srv, const char* str) // Only used by GetSoundInterests
{
	if (!g_Lua)
		return detour_Client_CScriptedEntity_StartFunctionStr.GetTrampoline<Symbols::CScriptedEntity_StartFunctionStr>()(funky_srv, str);

	const char* pStr = nullptr;
	auto it = ScriptedEntity_StartFunctionStr_strs.find(str);
	if (it == ScriptedEntity_StartFunctionStr_strs.end())
	{
		ScriptedEntity_StartFunctionStr_strs[str] = "CScriptedEntity::Call (" + (std::string)str + ")"; // Vprof is added in CScriptedEntity::Call(int, int)
		pStr = ScriptedEntity_StartFunctionStr_strs[str].c_str();
	} else {
		pStr = it->second.c_str();
	}

	pClient_CurrentScriptFunction = pStr;

	return detour_Client_CScriptedEntity_StartFunctionStr.GetTrampoline<Symbols::CScriptedEntity_StartFunctionStr>()(funky_srv, str);
}

static Detouring::Hook detour_Client_CScriptedEntity_StartFunction;
static void* hook_Client_CScriptedEntity_StartFunction(void* funky_srv, int pool)
{
	if (!g_Lua)
		return detour_Client_CScriptedEntity_StartFunction.GetTrampoline<Symbols::CScriptedEntity_StartFunction>()(funky_srv, pool);

	const char* pStr = nullptr;
	auto it = CScriptedEntity_StartFunction_strs.find(pool);
	if (it == CScriptedEntity_StartFunction_strs.end())
	{
		CScriptedEntity_StartFunction_strs[pool] = "CScriptedEntity::Call (" + (std::string)g_Lua->GetPooledString(pool) + ")";
		pStr = CScriptedEntity_StartFunction_strs[pool].c_str();
	} else {
		pStr = it->second.c_str();
	}

	pClient_CurrentScriptFunction = pStr;

	return detour_Client_CScriptedEntity_StartFunction.GetTrampoline<Symbols::CScriptedEntity_StartFunction>()(funky_srv, pool);
}

static Detouring::Hook detour_Client_CScriptedEntity_Call;
static void* hook_Client_CScriptedEntity_Call(void* funky_srv, int iArgs, int iRets)
{
	if (!g_Lua || !pClient_CurrentScriptFunction)
		return detour_Client_CScriptedEntity_Call.GetTrampoline<Symbols::CScriptedEntity_Call>()(funky_srv, iArgs, iRets);

	VPROF_BUDGET(pClient_CurrentScriptFunction, "GMOD");
	pClient_CurrentScriptFunction = nullptr;

	return detour_Client_CScriptedEntity_Call.GetTrampoline<Symbols::CScriptedEntity_Call>()(funky_srv, iArgs, iRets);
}

static Detouring::Hook detour_Client_CScriptedEntity_CallFunctionStr;
static void* hook_Client_CScriptedEntity_CallFunctionStr(void* funky_srv, const char* str)
{
	if (!g_Lua)
		return detour_Client_CScriptedEntity_CallFunctionStr.GetTrampoline<Symbols::CScriptedEntity_CallFunctionStr>()(funky_srv, str);

	const char* pStr = nullptr;
	auto it = CScriptedEntity_CallFunctionStr_strs.find(str);
	if (it == CScriptedEntity_CallFunctionStr_strs.end())
	{
		CScriptedEntity_CallFunctionStr_strs[str] = "CScriptedEntity::CallFunction (" + (std::string)str + ")";
		pStr = CScriptedEntity_CallFunctionStr_strs[str].c_str();
	} else {
		pStr = it->second.c_str();
	}

	VPROF_BUDGET(pStr, "GMOD");

	return detour_Client_CScriptedEntity_CallFunctionStr.GetTrampoline<Symbols::CScriptedEntity_CallFunctionStr>()(funky_srv, str);
}

static Detouring::Hook detour_Client_CScriptedEntity_CallFunction;
static void* hook_Client_CScriptedEntity_CallFunction(void* funky_srv, int pool)
{
	if (!g_Lua)
		return detour_Client_CScriptedEntity_CallFunction.GetTrampoline<Symbols::CScriptedEntity_CallFunction>()(funky_srv, pool);

	const char* pStr = nullptr;
	auto it = CScriptedEntity_CallFunction_strs.find(pool);
	if (it == CScriptedEntity_CallFunction_strs.end())
	{
		CScriptedEntity_CallFunction_strs[pool] = "CScriptedEntity::CallFunction (" + (std::string)g_Lua->GetPooledString(pool) + ")";
		pStr = CScriptedEntity_CallFunction_strs[pool].c_str();
	} else {
		pStr = it->second.c_str();
	}

	VPROF_BUDGET(pStr, "GMOD");

	return detour_Client_CScriptedEntity_CallFunction.GetTrampoline<Symbols::CScriptedEntity_CallFunction>()(funky_srv, pool);
}
#endif

/*static Detouring::Hook detour_lj_BC_FUNC;
static void* hook_lj_BC_FUNC() // Find out the luajit function later.
{
	// ToDo
	return NULL;
}*/

void CVProfModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::ModuleLoader tier0_loader("tier0");
	Detour::Create(
		&detour_CVProfile_OutputReport, "CVProfile::OutputReport",
		tier0_loader.GetModule(), Symbols::CVProfile_OutputReportSym,
		(void*)hook_CVProfile_OutputReport, m_pID
	);

	SourceSDK::ModuleLoader server_loader("server");
	Detour::Create(
		&detour_CLuaGamemode_CallStr, "CLuaGamemode::Call(const char*)",
		server_loader.GetModule(), Symbols::CLuaGamemode_CallStrSym,
		(void*)hook_CLuaGamemode_CallStr, m_pID
	);

	Detour::Create(
		&detour_CLuaGamemode_Call, "CLuaGamemode::Call(int)",
		server_loader.GetModule(), Symbols::CLuaGamemode_CallSym,
		(void*)hook_CLuaGamemode_Call, m_pID
	);

	Detour::Create(
		&detour_CLuaGamemode_CallFinish, "CLuaGamemode::CallFinish",
		server_loader.GetModule(), Symbols::CLuaGamemode_CallFinishSym,
		(void*)hook_CLuaGamemode_CallFinish, m_pID
	);

	Detour::Create(
		&detour_CLuaGamemode_CallWithArgsStr, "CLuaGamemode::CallWithArgs(const char*)",
		server_loader.GetModule(), Symbols::CLuaGamemode_CallWithArgsStrSym,
		(void*)hook_CLuaGamemode_CallWithArgsStr, m_pID
	);

	Detour::Create(
		&detour_CLuaGamemode_CallWithArgs, "CLuaGamemode::CallWithArgs(int)",
		server_loader.GetModule(), Symbols::CLuaGamemode_CallWithArgsSym,
		(void*)hook_CLuaGamemode_CallWithArgs, m_pID
	);

	Detour::Create(
		&detour_CScriptedEntity_StartFunctionStr, "CScriptedEntity::StartFunction(const char*)",
		server_loader.GetModule(), Symbols::CScriptedEntity_StartFunctionStrSym,
		(void*)hook_CScriptedEntity_StartFunctionStr, m_pID
	);

	Detour::Create(
		&detour_CScriptedEntity_StartFunction, "CScriptedEntity::StartFunction(int)",
		server_loader.GetModule(), Symbols::CScriptedEntity_StartFunctionSym,
		(void*)hook_CScriptedEntity_StartFunction, m_pID
	);

	Detour::Create(
		&detour_CScriptedEntity_Call, "CScriptedEntity::Call",
		server_loader.GetModule(), Symbols::CScriptedEntity_CallSym,
		(void*)hook_CScriptedEntity_Call, m_pID
	);

	Detour::Create(
		&detour_CScriptedEntity_CallFunctionStr, "CScriptedEntity::CallFunction(const char*)",
		server_loader.GetModule(), Symbols::CScriptedEntity_CallFunctionStrSym,
		(void*)hook_CScriptedEntity_CallFunctionStr, m_pID
	);

	Detour::Create(
		&detour_CScriptedEntity_CallFunction, "CScriptedEntity::CallFunction(int)",
		server_loader.GetModule(), Symbols::CScriptedEntity_CallFunctionSym,
		(void*)hook_CScriptedEntity_CallFunction, m_pID
	);

#ifdef SYSTEM_WINDOWS
	// We also add detours for the Client version of thoes functions.
	// Finally we got some sort of lua profiling clientside and new we can know which hook eats all the frames

	SourceSDK::ModuleLoader client_loader("client");
	Detour::Create(
		&detour_Client_CLuaGamemode_Call, "Client - CLuaGamemode::Call",
		client_loader.GetModule(), Symbols::Client_CLuaGamemode_CallSym,
		(void*)hook_Client_CLuaGamemode_Call, m_pID
	);

	Detour::Create(
		&detour_Client_CLuaGamemode_CallFinish, "Client - CLuaGamemode::CallFinish",
		client_loader.GetModule(), Symbols::Client_CLuaGamemode_CallFinishSym,
		(void*)hook_Client_CLuaGamemode_CallFinish, m_pID
	);

	Detour::Create(
		&detour_Client_CLuaGamemode_CallWithArgs, "Client - CLuaGamemode::CallWithArgs",
		client_loader.GetModule(), Symbols::Client_CLuaGamemode_CallWithArgsSym,
		(void*)hook_Client_CLuaGamemode_CallWithArgs, m_pID
	);

	Detour::Create(
		&detour_Client_CScriptedEntity_StartFunctionStr, "Client - CScriptedEntity::StartFunction1(const char*)",
		client_loader.GetModule(), Symbols::Client_CScriptedEntity_StartFunctionStrSym,
		(void*)hook_Client_CScriptedEntity_StartFunctionStr, m_pID
	);

	Detour::Create(
		&detour_Client_CScriptedEntity_StartFunction, "Client - CScriptedEntity::StartFunction2(int)",
		client_loader.GetModule(), Symbols::Client_CScriptedEntity_StartFunctionSym,
		(void*)hook_Client_CScriptedEntity_StartFunction, m_pID
	);

	Detour::Create(
		&detour_Client_CScriptedEntity_Call, "Client - CScriptedEntity::Call",
		client_loader.GetModule(), Symbols::Client_CScriptedEntity_CallSym,
		(void*)hook_Client_CScriptedEntity_Call, m_pID
	);

	Detour::Create(
		&detour_Client_CScriptedEntity_CallFunctionStr, "Client - CScriptedEntity::CallFunction(const char*)",
		client_loader.GetModule(), Symbols::Client_CScriptedEntity_CallFunctionStrSym,
		(void*)hook_Client_CScriptedEntity_CallFunctionStr, m_pID
	);

	Detour::Create(
		&detour_Client_CScriptedEntity_CallFunction, "Client - CScriptedEntity::CallFunction2(int)",
		client_loader.GetModule(), Symbols::Client_CScriptedEntity_CallFunctionSym,
		(void*)hook_Client_CScriptedEntity_CallFunction, m_pID
	);
#endif

#ifdef ARCHITECTURE_X86_64
	Detour::Create(
		&detour_Msg, "Msg",
		tier0_loader.GetModule(), Symbol::FromName("Msg"),
		(void*)hook_Msg, m_pID
	);

	if (detour_Msg.IsEnabled())
		detour_Msg.Disable();
#endif
}

#ifdef ARCHITECTURE_X86_64
class PLATFORM_CLASS CCVProfile 
{
public:
#ifdef VPROF_VTUNE_GROUP
	bool		m_bVTuneGroupEnabled;
	int			m_nVTuneGroupID;
	int			m_GroupIDStack[MAX_GROUP_STACK_DEPTH];
	int			m_GroupIDStackDepth;
#endif
	int 		m_enabled;
	bool		m_fAtRoot; // tracked for efficiency of the "not profiling" case
	CVProfNode *m_pCurNode;
	CVProfNode	m_Root;
	int			m_nFrames;
	int			m_ProfileDetailLevel;
	int			m_pausedEnabledDepth;

	class CBudgetGroup
	{
	public:
		tchar *m_pName;
		int m_BudgetFlags;
	};
	
	CBudgetGroup	*m_pBudgetGroups;
	int			m_nBudgetGroupNamesAllocated;
	int			m_nBudgetGroupNames;
	void		(*m_pNumBudgetGroupsChangedCallBack)(void);

	// Performance monitoring events.
	bool		m_bPMEInit;
	bool		m_bPMEEnabled;

	int64 m_Counters[MAXCOUNTERS];
	char m_CounterGroups[MAXCOUNTERS]; // (These are CounterGroup_t's).
	tchar *m_CounterNames[MAXCOUNTERS];
	int m_NumCounters;

	unsigned m_TargetThreadId;
};

void CVProfModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	CCVProfile* prof = (CCVProfile*)&g_VProfCurrentProfile;
	Msg("m_fAtRoot: %s\n", prof->m_fAtRoot ? "true" : "false");
	Msg("m_nFrames: %i\n", prof->m_nFrames);
	Msg("m_ProfileDetailLevel: %i\n", prof->m_ProfileDetailLevel);
	Msg("m_pausedEnabledDepth: %i\n", prof->m_pausedEnabledDepth);
	Msg("m_nBudgetGroupNamesAllocated: %i\n", prof->m_nBudgetGroupNamesAllocated);
	Msg("m_nBudgetGroupNames: %i\n", prof->m_nBudgetGroupNames);
	Msg("m_bPMEInit: %s\n", prof->m_bPMEInit ? "true" : "false");
	Msg("m_bPMEEnabled: %s\n", prof->m_bPMEEnabled ? "true" : "false");
	Msg("m_NumCounters: %i\n", prof->m_NumCounters);
	prof->m_ProfileDetailLevel = 4;
}

void CVProfModule::Shutdown()
{
	g_VProfCurrentProfile.Stop();
}
#endif