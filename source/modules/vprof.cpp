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
Detouring::Hook detour_Msg; // Yes. I don't like this.
void hook_Msg(PRINTF_FORMAT_STRING const tchar* pMsg, ...)
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

static int pCurrentGamemodeFunction = -1;
static std::map<int, std::string> CallWithArgs_strs;
static Detouring::Hook detour_CLuaGamemode_CallWithArgs;
static void* hook_CLuaGamemode_CallWithArgs(void* funky_srv, int pool)
{
	if (!g_Lua)
		return detour_CLuaGamemode_CallWithArgs.GetTrampoline<Symbols::CLuaGamemode_CallWithArgs>()(funky_srv, pool);

	const char* pStr = nullptr;
	auto it = CallWithArgs_strs.find(pool);
	if (it == CallWithArgs_strs.end())
	{
		CallWithArgs_strs[pool] = "CLuaGamemode::CallWithArgs (" + (std::string)g_Lua->GetPooledString(pool) + ")";
		pStr = CallWithArgs_strs[pool].c_str();
	} else {
		pStr = it->second.c_str();
	}

	pCurrentGamemodeFunction = pool;

	VPROF_BUDGET(pStr, "GMOD");

	return detour_CLuaGamemode_CallWithArgs.GetTrampoline<Symbols::CLuaGamemode_CallWithArgs>()(funky_srv, pool);
}

static std::map<int, std::string> CallFinish_strs;
static Detouring::Hook detour_CLuaGamemode_CallFinish;
static void* hook_CLuaGamemode_CallFinish(void* funky_srv, int pArgs)
{
	if (!g_Lua || pCurrentGamemodeFunction == -1)
		return detour_CLuaGamemode_CallFinish.GetTrampoline<Symbols::CLuaGamemode_CallFinish>()(funky_srv, pArgs);

	if (CallFinish_strs.find(pCurrentGamemodeFunction) == CallFinish_strs.end())
	{
		CallFinish_strs[pCurrentGamemodeFunction] = "CLuaGamemode::CallFinish (" + (std::string)g_Lua->GetPooledString(pCurrentGamemodeFunction) + ")";
	}

	VPROF_BUDGET(CallFinish_strs[pCurrentGamemodeFunction].c_str(), "GMOD");
	pCurrentGamemodeFunction = -1;

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

/*
 * Scripted Entities calls
 * 
 * Notes for myself:
 * - CScriptedEntity::Call(int iArgs, int iRets) - Don't make the same mistake like I did with CallFinish
 * - CScriptedEntity::Call(int iPooledString) - Unlike the function above, we call a function that has no args & return values like Think.
 */
static const char* pCurrentScriptFunction = nullptr;
static std::map<std::string_view, std::string> ScriptedEntity_StartFunction1_strs;
static Detouring::Hook detour_CScriptedEntity_StartFunction1;
static void* hook_CScriptedEntity_StartFunction1(void* funky_srv, const char* str) // Only used by GetSoundInterests
{
	if (!g_Lua)
		return detour_CScriptedEntity_StartFunction1.GetTrampoline<Symbols::CScriptedEntity_StartFunction1>()(funky_srv, str);

	const char* pStr = nullptr;
	auto it = ScriptedEntity_StartFunction1_strs.find(str);
	if (it == ScriptedEntity_StartFunction1_strs.end())
	{
		ScriptedEntity_StartFunction1_strs[str] = "CScriptedEntity::Call (" + (std::string)str + ")"; // Vprof is added in CScriptedEntity::Call(int, int)
		pStr = ScriptedEntity_StartFunction1_strs[str].c_str();
	} else {
		pStr = it->second.c_str();
	}

	pCurrentScriptFunction = pStr;

	return detour_CScriptedEntity_StartFunction1.GetTrampoline<Symbols::CScriptedEntity_StartFunction1>()(funky_srv, str);
}

static std::map<int, std::string> CScriptedEntity_StartFunction2_strs;
static Detouring::Hook detour_CScriptedEntity_StartFunction2;
static void* hook_CScriptedEntity_StartFunction2(void* funky_srv, int pool)
{
	if (!g_Lua)
		return detour_CScriptedEntity_StartFunction2.GetTrampoline<Symbols::CScriptedEntity_StartFunction2>()(funky_srv, pool);

	const char* pStr = nullptr;
	auto it = CScriptedEntity_StartFunction2_strs.find(pool);
	if (it == CScriptedEntity_StartFunction2_strs.end())
	{
		CScriptedEntity_StartFunction2_strs[pool] = "CScriptedEntity::Call (" + (std::string)g_Lua->GetPooledString(pool) + ")";
		pStr = CScriptedEntity_StartFunction2_strs[pool].c_str();
	} else {
		pStr = it->second.c_str();
	}

	pCurrentScriptFunction = pStr;

	return detour_CScriptedEntity_StartFunction2.GetTrampoline<Symbols::CScriptedEntity_StartFunction2>()(funky_srv, pool);
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

static std::map<std::string_view, std::string> CScriptedEntity_CallFunction1_strs;
static Detouring::Hook detour_CScriptedEntity_CallFunction1;
static void* hook_CScriptedEntity_CallFunction1(void* funky_srv, const char* str)
{
	if (!g_Lua)
		return detour_CScriptedEntity_CallFunction1.GetTrampoline<Symbols::CScriptedEntity_CallFunction1>()(funky_srv, str);

	const char* pStr = nullptr;
	auto it = CScriptedEntity_CallFunction1_strs.find(str);
	if (it == CScriptedEntity_CallFunction1_strs.end())
	{
		CScriptedEntity_CallFunction1_strs[str] = "CScriptedEntity::CallFunction (" + (std::string)str + ")";
		pStr = CScriptedEntity_CallFunction1_strs[str].c_str();
	} else {
		pStr = it->second.c_str();
	}

	VPROF_BUDGET(pStr, "GMOD");

	return detour_CScriptedEntity_CallFunction1.GetTrampoline<Symbols::CScriptedEntity_CallFunction1>()(funky_srv, str);
}

static std::map<int, std::string> CScriptedEntity_CallFunction2_strs;
static Detouring::Hook detour_CScriptedEntity_CallFunction2;
static void* hook_CScriptedEntity_CallFunction2(void* funky_srv, int pool)
{
	if (!g_Lua)
		return detour_CScriptedEntity_CallFunction2.GetTrampoline<Symbols::CScriptedEntity_CallFunction2>()(funky_srv, pool);

	const char* pStr = nullptr;
	auto it = CScriptedEntity_CallFunction2_strs.find(pool);
	if (it == CScriptedEntity_CallFunction2_strs.end())
	{
		CScriptedEntity_CallFunction2_strs[pool] = "CScriptedEntity::CallFunction (" + (std::string)g_Lua->GetPooledString(pool) + ")";
		pStr = CScriptedEntity_CallFunction2_strs[pool].c_str();
	} else {
		pStr = it->second.c_str();
	}

	VPROF_BUDGET(pStr, "GMOD");

	return detour_CScriptedEntity_CallFunction2.GetTrampoline<Symbols::CScriptedEntity_CallFunction2>()(funky_srv, pool);
}

#ifdef SYSTEM_WINDOWS
static int pClient_CurrentGamemodeFunction = -1;
static Detouring::Hook detour_Client_CLuaGamemode_CallWithArgs;
static void* hook_Client_CLuaGamemode_CallWithArgs(void* funky_srv, int pool)
{
	if (!g_Lua)
		return detour_Client_CLuaGamemode_CallWithArgs.GetTrampoline<Symbols::CLuaGamemode_CallWithArgs>()(funky_srv, pool);

	const char* pStr = nullptr;
	auto it = CallWithArgs_strs.find(pool);
	if (it == CallWithArgs_strs.end())
	{
		CallWithArgs_strs[pool] = "CLuaGamemode::CallWithArgs (" + (std::string)g_Lua->GetPooledString(pool) + ")";
		pStr = CallWithArgs_strs[pool].c_str();
	} else {
		pStr = it->second.c_str();
	}

	pClient_CurrentGamemodeFunction = pool;

	VPROF_BUDGET(pStr, "GMOD");

	return detour_Client_CLuaGamemode_CallWithArgs.GetTrampoline<Symbols::CLuaGamemode_CallWithArgs>()(funky_srv, pool);
}

static Detouring::Hook detour_Client_CLuaGamemode_CallFinish;
static void* hook_Client_CLuaGamemode_CallFinish(void* funky_srv, int pArgs)
{
	if (!g_Lua || pClient_CurrentGamemodeFunction == -1)
		return detour_CLuaGamemode_CallFinish.GetTrampoline<Symbols::CLuaGamemode_CallFinish>()(funky_srv, pArgs);

	if (CallFinish_strs.find(pClient_CurrentGamemodeFunction) == CallFinish_strs.end())
	{
		CallFinish_strs[pClient_CurrentGamemodeFunction] = "CLuaGamemode::CallFinish (" + (std::string)g_Lua->GetPooledString(pClient_CurrentGamemodeFunction) + ")";
	}

	VPROF_BUDGET(CallFinish_strs[pClient_CurrentGamemodeFunction].c_str(), "GMOD");
	pClient_CurrentGamemodeFunction = -1;

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
static Detouring::Hook detour_Client_CScriptedEntity_StartFunction1;
static void* hook_Client_CScriptedEntity_StartFunction1(void* funky_srv, const char* str) // Only used by GetSoundInterests
{
	if (!g_Lua)
		return detour_Client_CScriptedEntity_StartFunction1.GetTrampoline<Symbols::CScriptedEntity_StartFunction1>()(funky_srv, str);

	const char* pStr = nullptr;
	auto it = ScriptedEntity_StartFunction1_strs.find(str);
	if (it == ScriptedEntity_StartFunction1_strs.end())
	{
		ScriptedEntity_StartFunction1_strs[str] = "CScriptedEntity::Call (" + (std::string)str + ")"; // Vprof is added in CScriptedEntity::Call(int, int)
		pStr = ScriptedEntity_StartFunction1_strs[str].c_str();
	} else {
		pStr = it->second.c_str();
	}

	pClient_CurrentScriptFunction = pStr;

	return detour_Client_CScriptedEntity_StartFunction1.GetTrampoline<Symbols::CScriptedEntity_StartFunction1>()(funky_srv, str);
}

static Detouring::Hook detour_Client_CScriptedEntity_StartFunction2;
static void* hook_Client_CScriptedEntity_StartFunction2(void* funky_srv, int pool)
{
	if (!g_Lua)
		return detour_Client_CScriptedEntity_StartFunction2.GetTrampoline<Symbols::CScriptedEntity_StartFunction2>()(funky_srv, pool);

	const char* pStr = nullptr;
	auto it = CScriptedEntity_StartFunction2_strs.find(pool);
	if (it == CScriptedEntity_StartFunction2_strs.end())
	{
		CScriptedEntity_StartFunction2_strs[pool] = "CScriptedEntity::Call (" + (std::string)g_Lua->GetPooledString(pool) + ")";
		pStr = CScriptedEntity_StartFunction2_strs[pool].c_str();
	} else {
		pStr = it->second.c_str();
	}

	pClient_CurrentScriptFunction = pStr;

	return detour_Client_CScriptedEntity_StartFunction2.GetTrampoline<Symbols::CScriptedEntity_StartFunction2>()(funky_srv, pool);
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

static Detouring::Hook detour_Client_CScriptedEntity_CallFunction1;
static void* hook_Client_CScriptedEntity_CallFunction1(void* funky_srv, const char* str)
{
	if (!g_Lua)
		return detour_Client_CScriptedEntity_CallFunction1.GetTrampoline<Symbols::CScriptedEntity_CallFunction1>()(funky_srv, str);

	const char* pStr = nullptr;
	auto it = CScriptedEntity_CallFunction1_strs.find(str);
	if (it == CScriptedEntity_CallFunction1_strs.end())
	{
		CScriptedEntity_CallFunction1_strs[str] = "CScriptedEntity::CallFunction (" + (std::string)str + ")";
		pStr = CScriptedEntity_CallFunction1_strs[str].c_str();
	} else {
		pStr = it->second.c_str();
	}

	VPROF_BUDGET(pStr, "GMOD");

	return detour_Client_CScriptedEntity_CallFunction1.GetTrampoline<Symbols::CScriptedEntity_CallFunction1>()(funky_srv, str);
}

static Detouring::Hook detour_Client_CScriptedEntity_CallFunction2;
static void* hook_Client_CScriptedEntity_CallFunction2(void* funky_srv, int pool)
{
	if (!g_Lua)
		return detour_Client_CScriptedEntity_CallFunction2.GetTrampoline<Symbols::CScriptedEntity_CallFunction2>()(funky_srv, pool);

	const char* pStr = nullptr;
	auto it = CScriptedEntity_CallFunction2_strs.find(pool);
	if (it == CScriptedEntity_CallFunction2_strs.end())
	{
		CScriptedEntity_CallFunction2_strs[pool] = "CScriptedEntity::CallFunction (" + (std::string)g_Lua->GetPooledString(pool) + ")";
		pStr = CScriptedEntity_CallFunction2_strs[pool].c_str();
	} else {
		pStr = it->second.c_str();
	}

	VPROF_BUDGET(pStr, "GMOD");

	return detour_Client_CScriptedEntity_CallFunction2.GetTrampoline<Symbols::CScriptedEntity_CallFunction2>()(funky_srv, pool);
}
#endif

Detouring::Hook detour_lj_BC_FUNC;
void* hook_lj_BC_FUNC() // Find out the luajit function later.
{
	// ToDo
	return NULL;
}

void CVProfModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }

	SourceSDK::ModuleLoader tier0_loader("tier0");
	Detour::Create(
		&detour_CVProfile_OutputReport, "CVProfile::OutputReport",
		tier0_loader.GetModule(), Symbols::CVProfile_OutputReportSym,
		(void*)hook_CVProfile_OutputReport, m_pID
	);

	SourceSDK::ModuleLoader server_loader("server");
	Detour::Create(
		&detour_CLuaGamemode_Call, "CLuaGamemode::Call",
		server_loader.GetModule(), Symbols::CLuaGamemode_CallSym,
		(void*)hook_CLuaGamemode_Call, m_pID
	);

	Detour::Create(
		&detour_CLuaGamemode_CallFinish, "CLuaGamemode::CallFinish",
		server_loader.GetModule(), Symbols::CLuaGamemode_CallFinishSym,
		(void*)hook_CLuaGamemode_CallFinish, m_pID
	);

	Detour::Create(
		&detour_CLuaGamemode_CallWithArgs, "CLuaGamemode::CallWithArgs",
		server_loader.GetModule(), Symbols::CLuaGamemode_CallWithArgsSym,
		(void*)hook_CLuaGamemode_CallWithArgs, m_pID
	);

	Detour::Create(
		&detour_CScriptedEntity_StartFunction1, "CScriptedEntity::StartFunction1",
		server_loader.GetModule(), Symbols::CScriptedEntity_StartFunction1Sym,
		(void*)hook_CScriptedEntity_StartFunction1, m_pID
	);

	Detour::Create(
		&detour_CScriptedEntity_StartFunction2, "CScriptedEntity::StartFunction2",
		server_loader.GetModule(), Symbols::CScriptedEntity_StartFunction2Sym,
		(void*)hook_CScriptedEntity_StartFunction2, m_pID
	);

	Detour::Create(
		&detour_CScriptedEntity_Call, "CScriptedEntity::Call",
		server_loader.GetModule(), Symbols::CScriptedEntity_CallSym,
		(void*)hook_CScriptedEntity_Call, m_pID
	);

	Detour::Create(
		&detour_CScriptedEntity_CallFunction1, "CScriptedEntity::CallFunction1",
		server_loader.GetModule(), Symbols::CScriptedEntity_CallFunction1Sym,
		(void*)hook_CScriptedEntity_CallFunction1, m_pID
	);

	Detour::Create(
		&detour_CScriptedEntity_CallFunction2, "CScriptedEntity::CallFunction2",
		server_loader.GetModule(), Symbols::CScriptedEntity_CallFunction2Sym,
		(void*)hook_CScriptedEntity_CallFunction2, m_pID
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
		&detour_Client_CScriptedEntity_StartFunction1, "Client - CScriptedEntity::StartFunction1",
		client_loader.GetModule(), Symbols::Client_CScriptedEntity_StartFunction1Sym,
		(void*)hook_Client_CScriptedEntity_StartFunction1, m_pID
	);

	Detour::Create(
		&detour_Client_CScriptedEntity_StartFunction2, "Client - CScriptedEntity::StartFunction2",
		client_loader.GetModule(), Symbols::Client_CScriptedEntity_StartFunction2Sym,
		(void*)hook_Client_CScriptedEntity_StartFunction2, m_pID
	);

	Detour::Create(
		&detour_Client_CScriptedEntity_Call, "Client - CScriptedEntity::Call",
		client_loader.GetModule(), Symbols::Client_CScriptedEntity_CallSym,
		(void*)hook_Client_CScriptedEntity_Call, m_pID
	);

	Detour::Create(
		&detour_Client_CScriptedEntity_CallFunction1, "Client - CScriptedEntity::CallFunction1",
		client_loader.GetModule(), Symbols::Client_CScriptedEntity_CallFunction1Sym,
		(void*)hook_Client_CScriptedEntity_CallFunction1, m_pID
	);

	Detour::Create(
		&detour_Client_CScriptedEntity_CallFunction2, "Client - CScriptedEntity::CallFunction2",
		client_loader.GetModule(), Symbols::Client_CScriptedEntity_CallFunction2Sym,
		(void*)hook_Client_CScriptedEntity_CallFunction2, m_pID
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