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
	virtual int Compatibility() { return LINUX32; };
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

static CVProfModule g_pVProfModule;
IModule* pVProfModule = &g_pVProfModule;
#ifndef ARCHITECTURE_X86_64
static std::string GetCurrentTime() { // Yoink from vprof module
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H-%M-%S");

    return ss.str();
}
#endif

static std::stringstream ss;
#ifndef ARCHITECTURE_X86_64
static SpewRetval_t VProf_Spew(SpewType_t type, const char *msg)
{
	ss << msg;

	return SPEW_CONTINUE;
}
#endif

static Detouring::Hook detour_CVProfile_OutputReport;
static void hook_CVProfile_OutputReport(void* fancy, int type, const tchar* pszStartMode, int budgetGroupID)
{
#ifdef ARCHITECTURE_X86_64
	detour_CVProfile_OutputReport.GetTrampoline<Symbols::CVProfile_OutputReport>()(fancy, type, pszStartMode, budgetGroupID);
#else
	if (!holylib_vprof_exportreport.GetBool())
	{
		detour_CVProfile_OutputReport.GetTrampoline<Symbols::CVProfile_OutputReport>()(fancy, type, pszStartMode, budgetGroupID);
		return;
	}

	SpewOutputFunc_t originalSpew = GetSpewOutputFunc();
	SpewOutputFunc(VProf_Spew);

	detour_CVProfile_OutputReport.GetTrampoline<Symbols::CVProfile_OutputReport>()(fancy, type, pszStartMode, budgetGroupID);

	SpewOutputFunc(originalSpew);

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
#endif
}

static std::map<int, std::string> CallFinish_strs;
static Detouring::Hook detour_CLuaGamemode_CallFinish;
static void* hook_CLuaGamemode_CallFinish(void* funky_srv, int pool)
{
	if (!g_Lua)
		return detour_CLuaGamemode_CallFinish.GetTrampoline<Symbols::CLuaGamemode_CallFinish>()(funky_srv, pool);

	if (CallFinish_strs.find(pool) == CallFinish_strs.end())
	{
		CallFinish_strs[pool] = "CLuaGamemode::CallFinish (" + (std::string)g_Lua->GetPooledString(pool) + ")";
	}

	VPROF_BUDGET(CallFinish_strs[pool].c_str(), "GMOD");

	return detour_CLuaGamemode_CallFinish.GetTrampoline<Symbols::CLuaGamemode_CallFinish>()(funky_srv, pool);
}

static std::map<int, std::string> CallWithArgs_strs;
static Detouring::Hook detour_CLuaGamemode_CallWithArgs;
static void* hook_CLuaGamemode_CallWithArgs(void* funky_srv, int pool)
{
	if (!g_Lua)
		return detour_CLuaGamemode_CallWithArgs.GetTrampoline<Symbols::CLuaGamemode_CallWithArgs>()(funky_srv, pool);

	if (CallWithArgs_strs.find(pool) == CallWithArgs_strs.end())
	{
		CallWithArgs_strs[pool] = "CLuaGamemode::CallWithArgs (" + (std::string)g_Lua->GetPooledString(pool) + ")";
	}

	VPROF_BUDGET(CallWithArgs_strs[pool].c_str(), "GMOD");

	return detour_CLuaGamemode_CallWithArgs.GetTrampoline<Symbols::CLuaGamemode_CallWithArgs>()(funky_srv, pool);
}

static std::map<int, std::string> Call_strs;
static Detouring::Hook detour_CLuaGamemode_Call;
static void* hook_CLuaGamemode_Call(void* funky_srv, int pool)
{
	if (!g_Lua)
		return detour_CLuaGamemode_Call.GetTrampoline<Symbols::CLuaGamemode_Call>()(funky_srv, pool);

	if (Call_strs.find(pool) == Call_strs.end())
	{
		Call_strs[pool] = "CLuaGamemode::Call (" + (std::string)g_Lua->GetPooledString(pool) + ")";
	}

	VPROF_BUDGET(Call_strs[pool].c_str(), "GMOD");

	return detour_CLuaGamemode_Call.GetTrampoline<Symbols::CLuaGamemode_Call>()(funky_srv, pool);
}

void CVProfModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }

	SourceSDK::ModuleLoader tier0_loader("libtier0");
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
}