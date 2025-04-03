#include "filesystem_base.h" // Has to be before symbols.h
#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <map>
#include <vprof.h>
#include <unordered_set>
#include <unordered_map>
#include "color.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CVProfModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "vprof"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32; };
	// NOTE for myself: Linux64 seemingly doesn't have vprof enabled! so don't suppositly add compatbility!
	// Update: Fk my old self, Linux64 is just broken and it crashed because I had the wrong symbols.
};

static ConVar holylib_vprof_exportreport("holylib_vprof_exportreport", "1", 0, "If enabled, vprof results will be dumped into a file in the vprof/ folder");
//static ConVar holylib_vprof_profilecfunc("holylib_vprof_profilecfunc", "0", 0, "If enabled, Lua->C calls will also be profiled.");

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
	if (!DETOUR_ISVALID(detour_Msg))
		return;

	DETOUR_ENABLE(detour_Msg);
}

static void FinishSpew()
{
	if (!DETOUR_ISENABLED(detour_Msg))
		return;

	DETOUR_DISABLE(detour_Msg);
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
			Msg(PROJECT_NAME ": vprof/ is a file? Please delete it or disable vprof_exportreport.\n");
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
		Msg(PROJECT_NAME ": Wrote vprof report into %s\n", filename.c_str());

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

	pCurrentGamemodeFunction = g_Lua->GetPooledString(pool);

	return detour_CLuaGamemode_CallWithArgs.GetTrampoline<Symbols::CLuaGamemode_CallWithArgs>()(funky_srv, pool);
}

//static std::map<std::string_view, std::string> CallWithArgsStr_strs;
static Detouring::Hook detour_CLuaGamemode_CallWithArgsStr;
static void* hook_CLuaGamemode_CallWithArgsStr(void* funky_srv, const char* str) // NOTE: Only used by gameevent.Listen -> CLuaGameEventListener::FireGameEvent
{
	if (!g_Lua)
		return detour_CLuaGamemode_CallWithArgsStr.GetTrampoline<Symbols::CLuaGamemode_CallWithArgsStr>()(funky_srv, str);

	pCurrentGamemodeFunction = str;

	return detour_CLuaGamemode_CallWithArgsStr.GetTrampoline<Symbols::CLuaGamemode_CallWithArgsStr>()(funky_srv, str);
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

static std::unordered_set<std::string> pLuaStrings; // Theses will almost never be freed!
// VPROF doesn't manage the memory of the strings that are used in scopes!
// So we need to make sure that they will always be valid.
// It does manage the strings of counters and budget groups
static const char* AddOrGetString(const char* pString)
{
	std::string strValue = pString;
	auto it = pLuaStrings.find(strValue);
	if (it != pLuaStrings.end())
		return it->c_str();

	pLuaStrings.insert(strValue);
	auto it2 = pLuaStrings.find(strValue); // Get the new std::string to return

	return it2->c_str();
}

#if 0
typedef struct lua_Debug {
  int event;
  const char *name;
  const char *namewhat;
  const char *what;
  const char *source;
  int currentline;
  int nups;
  int linedefined;
  int lastlinedefined;
  char short_src[60];
  int i_ci;
  int nparams;
  int isvararg;
};

static Detouring::Hook detour_lj_BC_FUNCC;
static std::map<int, std::string> lj_strs;
static void* hook_lj_BC_FUNCC(void* arg) // Find out the luajit function later.
{
	lua_Debug debug;
	g_Lua->GetInfo("Sn", &debug);

	if (!debug.name)
		return detour_lj_BC_FUNCC.GetTrampoline<Symbols::lj_BC_FUNCC>()(arg);

	VPROF_BUDGET( AddOrGetString(debug.name), VPROF_BUDGETGROUP_HOLYLIB );

	void* ret = detour_lj_BC_FUNCC.GetTrampoline<Symbols::lj_BC_FUNCC>()(arg);
	return ret;
}
#endif

#if defined(ARCHITECTURE_X86_64) && 0
Detouring::Hook detour_ThreadGetCurrentId;
ThreadId_t hook_ThreadGetCurrentId()
{
	// Downcasting it to a unsigned int to allow this line to work:
	// bool InTargetThread() { return ( m_TargetThreadId == ThreadGetCurrentId() ); }
	// The bug: unsigned int == unsigned long -> This will always return false
	return (unsigned int)detour_ThreadGetCurrentId.GetTrampoline<Symbols::ThreadGetCurrentId_t>()();
}
#endif

void CVProfModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

#ifndef SYSTEM_WINDOWS
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
#endif

#if defined(ARCHITECTURE_X86) && 0
	Detour::Create(
		&detour_lj_BC_FUNCC, "lj_BC_FUNCC",
		server_loader.GetModule(), Symbols::lj_BC_FUNCCSym,
		(void*)hook_lj_BC_FUNCC, m_pID
	);
#endif

#if defined(SYSTEM_WINDOWS) && 0
	// We also add detours for the Client version of thoes functions.
	// Finally we got some sort of lua profiling clientside and new we can know which hook eats all the frames
	if (g_pModuleManager.IsMarkedAsBinaryModule()) // We were marked as a binary module on windows? Then were most likely running on a windows client.
	{
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
	}
#endif

#if ARCHITECTURE_IS_X86_64 && SYSTEM_LINUX
#if 0 // This bug was fixed in Gmod itself -> https://github.com/Facepunch/garrysmod-issues/issues/6019
	Detour::Create(
		&detour_ThreadGetCurrentId, "ThreadGetCurrentId",
		tier0_loader.GetModule(), Symbols::ThreadGetCurrentIdSym,
		(void*)hook_ThreadGetCurrentId, m_pID
	);

	DeclareCurrentThreadIsMainThread(); // Update g_ThreadMainThreadID to fix ThreadInMainThread breaking

	unsigned m_TargetThreadId = ThreadGetCurrentId();
    unsigned long int pthread_Id = ThreadGetCurrentId();
	if (m_TargetThreadId != pthread_Id)
		Warning("[holylib - vprof] Failed to fix ThreadGetCurrentId! (vprof most likely won't work)\n");
#endif

	Detour::Create(
		&detour_Msg, "Msg",
		tier0_loader.GetModule(), Symbol::FromName("Msg"),
		(void*)hook_Msg, m_pID
	);

	if (DETOUR_ISENABLED(detour_Msg))
		DETOUR_DISABLE(detour_Msg);
#endif
}

// ------------- VProf Counter --------------

struct VProfCounter
{
	const char* strName;
#if ARCHITECTURE_IS_X86_64
	int* iValue;
#else
	int64* iValue;
#endif
};
static int VProfCounter_TypeID = -1;
Push_LuaClass(VProfCounter, VProfCounter_TypeID)
Get_LuaClass(VProfCounter, VProfCounter_TypeID, "VProfCounter")

LUA_FUNCTION_STATIC(VProfCounter__tostring)
{
	VProfCounter* pData = Get_VProfCounter(1, false);
	if (!pData)
	{
		LUA->PushString("VProfCounter [NULL]");
		return 1;
	}

	char szBuf[64] = {};
	V_snprintf(szBuf, sizeof(szBuf), "VProfCounter [%s][%lld]", pData->strName, (long long)(*pData->iValue));
	LUA->PushString(szBuf);
	return 1;
}

Default__index(VProfCounter);
Default__newindex(VProfCounter);
Default__GetTable(VProfCounter);
Default__gc(VProfCounter,
	VProfCounter* pCounter = (VProfCounter*)pData->GetData();
	if (pCounter)
		delete pCounter;
)

LUA_FUNCTION_STATIC(VProfCounter_GetName)
{
	VProfCounter* counter = Get_VProfCounter(1, true);

	LUA->PushString(counter->strName);
	return 1;
}

LUA_FUNCTION_STATIC(VProfCounter_Set)
{
	VProfCounter* counter = Get_VProfCounter(1, true);
	(*counter->iValue) = (int64)LUA->CheckNumber(2);

	return 0;
}

LUA_FUNCTION_STATIC(VProfCounter_Get)
{
	VProfCounter* counter = Get_VProfCounter(1, true);
	
	LUA->PushNumber((double)*counter->iValue);
	return 1;
}

LUA_FUNCTION_STATIC(VProfCounter_Increment)
{
	VProfCounter* counter = Get_VProfCounter(1, true);

	++(*counter->iValue);
	return 0;
}

LUA_FUNCTION_STATIC(VProfCounter_Decrement)
{
	VProfCounter* counter = Get_VProfCounter(1, true);

	--(*counter->iValue);
	return 0;
}

// --------------------------

// ------------- VProf Node --------------

static int VProfNode_TypeID = -1;
Push_LuaClass(CVProfNode, VProfNode_TypeID)
Get_LuaClass(CVProfNode, VProfNode_TypeID, "VProfNode")

LUA_FUNCTION_STATIC(VProfNode__tostring)
{
	CVProfNode* pData = Get_CVProfNode(1, false);
	if (!pData)
	{
		LUA->PushString("VProfNode [NULL]");
		return 1;
	}

	char szBuf[64] = {};
	V_snprintf(szBuf, sizeof(szBuf), "VProfNode [%s]", pData->GetName());
	LUA->PushString(szBuf);
	return 1;
}

Default__index(CVProfNode);
Default__newindex(CVProfNode);
Default__GetTable(CVProfNode);
Default__gc(CVProfNode,
	CVProfNode* pNode = (CVProfNode*)pData->GetData();
	if (pNode)
		delete pNode;
)

LUA_FUNCTION_STATIC(VProfNode_GetName)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	LUA->PushString(node->GetName());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetBudgetGroupID)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	LUA->PushNumber(node->GetBudgetGroupID());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetCurTime)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	LUA->PushNumber(node->GetCurTime());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetCurTimeLessChildren)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	LUA->PushNumber(node->GetCurTimeLessChildren());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetPeakTime)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	LUA->PushNumber(node->GetPeakTime());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetPrevTime)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	LUA->PushNumber(node->GetPrevTime());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetPrevTimeLessChildren)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	LUA->PushNumber(node->GetPrevTimeLessChildren());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetTotalTime)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	LUA->PushNumber(node->GetTotalTime());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetTotalTimeLessChildren)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	LUA->PushNumber(node->GetTotalTimeLessChildren());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetCurCalls)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	LUA->PushNumber(node->GetCurCalls());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetChild)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	Push_CVProfNode(node->GetChild());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetParent)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	Push_CVProfNode(node->GetParent());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetSibling)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	Push_CVProfNode(node->GetSibling());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetPrevSibling)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	Push_CVProfNode(node->GetPrevSibling());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetL2CacheMisses)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	LUA->PushNumber(node->GetL2CacheMisses());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetPrevL2CacheMissLessChildren)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	LUA->PushNumber(node->GetPrevL2CacheMissLessChildren());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetPrevLoadHitStoreLessChildren)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	LUA->PushNumber(node->GetPrevLoadHitStoreLessChildren());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetTotalCalls)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	LUA->PushNumber(node->GetTotalCalls());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetSubNode)
{
	CVProfNode* node = Get_CVProfNode(1, true);
	const char* pName = AddOrGetString(LUA->CheckString(2));
	int iDetailLevel = (int)LUA->CheckNumber(3);
	const char* pBudgetGroup = LUA->CheckString(4);

	Push_CVProfNode(node->GetSubNode(pName, iDetailLevel, pBudgetGroup));
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetClientData)
{
	CVProfNode* node = Get_CVProfNode(1, true);

	LUA->PushNumber(node->GetClientData());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_MarkFrame)
{
	CVProfNode* node = Get_CVProfNode(1, true);
	node->MarkFrame();

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_ClearPrevTime)
{
	CVProfNode* node = Get_CVProfNode(1, true);
	node->ClearPrevTime();

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_Pause)
{
	CVProfNode* node = Get_CVProfNode(1, true);
	node->Pause();

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_Reset)
{
	CVProfNode* node = Get_CVProfNode(1, true);
	node->Reset();

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_ResetPeak)
{
	CVProfNode* node = Get_CVProfNode(1, true);
	node->ResetPeak();

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_Resume)
{
	CVProfNode* node = Get_CVProfNode(1, true);
	node->Resume();

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_SetBudgetGroupID)
{
	CVProfNode* node = Get_CVProfNode(1, true);
	node->SetBudgetGroupID((int)LUA->CheckNumber(2));

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_SetCurFrameTime)
{
	CVProfNode* node = Get_CVProfNode(1, true);
	node->SetCurFrameTime((unsigned long)LUA->CheckNumber(2));

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_SetClientData)
{
	CVProfNode* node = Get_CVProfNode(1, true);
	node->SetClientData((int)LUA->CheckNumber(2));

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_EnterScope)
{
	CVProfNode* node = Get_CVProfNode(1, true);
	node->EnterScope();

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_ExitScope)
{
	CVProfNode* node = Get_CVProfNode(1, true);
	node->ExitScope();

	return 0;
}

// --------------------------

LUA_FUNCTION_STATIC(vprof_Start)
{
	g_VProfCurrentProfile.Start();

	return 0;
}

LUA_FUNCTION_STATIC(vprof_Stop)
{
	g_VProfCurrentProfile.Stop();

	return 0;
}

LUA_FUNCTION_STATIC(vprof_AtRoot)
{
	LUA->PushBool(g_VProfCurrentProfile.AtRoot());

	return 1;
}

LUA_FUNCTION_STATIC(vprof_FindOrCreateCounter)
{
	const char* pName = AddOrGetString(LUA->CheckString(1)); // Just to make sure
	CounterGroup_t group = (CounterGroup_t)LUA->CheckNumberOpt(2, COUNTER_GROUP_DEFAULT);

	VProfCounter* counter = new VProfCounter;
	counter->strName = pName;
	counter->iValue = g_VProfCurrentProfile.FindOrCreateCounter(pName, group);

	Push_VProfCounter(counter);
	return 1;
}

LUA_FUNCTION_STATIC(vprof_GetCounter)
{
	int index = (int)LUA->CheckNumber(1);

	VProfCounter* counter = new VProfCounter;
	counter->strName = g_VProfCurrentProfile.GetCounterName(index);
	if (!counter->strName)
	{
		LUA->PushNil();
		return 1; // Not a valid counter. LIAR
	}
	
	counter->iValue = g_VProfCurrentProfile.FindOrCreateCounter(counter->strName);

	Push_VProfCounter(counter);
	return 1;
}

LUA_FUNCTION_STATIC(vprof_GetNumCounters)
{
	LUA->PushNumber(g_VProfCurrentProfile.GetNumCounters());
	return 1;
}

LUA_FUNCTION_STATIC(vprof_ResetCounters)
{
	CounterGroup_t group = (CounterGroup_t)LUA->CheckNumberOpt(2, COUNTER_GROUP_DEFAULT);

	g_VProfCurrentProfile.ResetCounters(group);
	return 0;
}

LUA_FUNCTION_STATIC(vprof_GetTimeLastFrame)
{
	LUA->PushNumber(g_VProfCurrentProfile.GetTimeLastFrame());
	return 1;
}

LUA_FUNCTION_STATIC(vprof_GetPeakFrameTime)
{
	LUA->PushNumber(g_VProfCurrentProfile.GetPeakFrameTime());
	return 1;
}

LUA_FUNCTION_STATIC(vprof_GetTotalTimeSampled)
{
	LUA->PushNumber(g_VProfCurrentProfile.GetTotalTimeSampled());
	return 1;
}

LUA_FUNCTION_STATIC(vprof_GetDetailLevel)
{
	LUA->PushNumber(g_VProfCurrentProfile.GetDetailLevel());
	return 1;
}

LUA_FUNCTION_STATIC(vprof_NumFramesSampled)
{
	LUA->PushNumber(g_VProfCurrentProfile.NumFramesSampled());
	return 1;
}

LUA_FUNCTION_STATIC(vprof_HideBudgetGroup)
{
	const char* pName = AddOrGetString(LUA->CheckString(1));
	bool bHide = LUA->GetBool(2);
	g_VProfCurrentProfile.HideBudgetGroup(pName, bHide);

	return 0;
}

LUA_FUNCTION_STATIC(vprof_GetNumBudgetGroups)
{
	LUA->PushNumber(g_VProfCurrentProfile.GetNumBudgetGroups());
	return 1;
}

LUA_FUNCTION_STATIC(vprof_BudgetGroupNameToBudgetGroupID)
{
	const char* pName = LUA->CheckString(1);

	LUA->PushNumber(g_VProfCurrentProfile.BudgetGroupNameToBudgetGroupID(pName));
	return 1;
}

LUA_FUNCTION_STATIC(vprof_GetBudgetGroupName)
{
	int index = (int)LUA->CheckNumber(1);

	LUA->PushString(g_VProfCurrentProfile.GetBudgetGroupName(index));
	return 1;
}

LUA_FUNCTION_STATIC(vprof_GetBudgetGroupFlags)
{
	int index = (int)LUA->CheckNumber(1);

	LUA->PushNumber(g_VProfCurrentProfile.GetBudgetGroupFlags(index));
	return 1;
}

LUA_FUNCTION_STATIC(vprof_GetBudgetGroupColor)
{
	int index = (int)LUA->CheckNumber(1);

	int r, g, b, a;
	g_VProfCurrentProfile.GetBudgetGroupColor(index,r, g, b, a);

	LUA->PushColor(Color(r, g, b, a));
	return 1;
}

LUA_FUNCTION_STATIC(vprof_GetRoot)
{
	Push_CVProfNode(g_VProfCurrentProfile.GetRoot());
	return 1;
}

LUA_FUNCTION_STATIC(vprof_GetCurrentNode)
{
	Push_CVProfNode(g_VProfCurrentProfile.GetCurrentNode());
	return 1;
}

LUA_FUNCTION_STATIC(vprof_IsEnabled)
{
	LUA->PushBool(g_VProfCurrentProfile.IsEnabled());
	return 1;
}

LUA_FUNCTION_STATIC(vprof_MarkFrame)
{
	g_VProfCurrentProfile.MarkFrame();
	return 0;
}

LUA_FUNCTION_STATIC(vprof_EnterScope)
{
	const char* pName = AddOrGetString(LUA->CheckString(1));
	int iDetailLevel = (int)LUA->CheckNumber(2);
	const char* pBudgetGroupName = LUA->CheckString(3);

	g_VProfCurrentProfile.EnterScope(pName, iDetailLevel, pBudgetGroupName, false);
	return 0;
}

LUA_FUNCTION_STATIC(vprof_ExitScope)
{
	g_VProfCurrentProfile.ExitScope();
	return 0;
}

LUA_FUNCTION_STATIC(vprof_Pause)
{
	g_VProfCurrentProfile.Pause();
	return 0;
}

LUA_FUNCTION_STATIC(vprof_Resume)
{
	g_VProfCurrentProfile.Resume();
	return 0;
}

LUA_FUNCTION_STATIC(vprof_Reset)
{
	g_VProfCurrentProfile.Reset();
	return 0;
}

LUA_FUNCTION_STATIC(vprof_ResetPeaks)
{
	g_VProfCurrentProfile.ResetPeaks();
	return 0;
}

LUA_FUNCTION_STATIC(vprof_Term)
{
	pLuaStrings.clear(); // The only time we clear it.
	g_VProfCurrentProfile.Term();
	return 0;
}

void CVProfModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	VProfCounter_TypeID = g_Lua->CreateMetaTable("VProfCounter");
		Util::AddFunc(VProfCounter__tostring, "__tostring");
		Util::AddFunc(VProfCounter__index, "__index");
		Util::AddFunc(VProfCounter__newindex, "__newindex");
		Util::AddFunc(VProfCounter__gc, "__gc");
		Util::AddFunc(VProfCounter_GetTable, "GetTable");
		Util::AddFunc(VProfCounter_Set, "Set");
		Util::AddFunc(VProfCounter_Get, "Get");
		Util::AddFunc(VProfCounter_Increment, "Increment");
		Util::AddFunc(VProfCounter_Decrement, "Decrement");
		Util::AddFunc(VProfCounter_GetName, "GetName");
	g_Lua->Pop(1);

	VProfNode_TypeID = g_Lua->CreateMetaTable("VProfNode");
		Util::AddFunc(VProfNode__tostring, "__tostring");
		Util::AddFunc(CVProfNode__index, "__index");
		Util::AddFunc(CVProfNode__newindex, "__newindex");
		Util::AddFunc(CVProfNode__gc, "__gc");
		Util::AddFunc(CVProfNode_GetTable, "GetTable");
		Util::AddFunc(VProfNode_GetName, "GetName");
		Util::AddFunc(VProfNode_GetBudgetGroupID, "GetBudgetGroupID");
		Util::AddFunc(VProfNode_GetCurTime, "GetCurTime");
		Util::AddFunc(VProfNode_GetCurTimeLessChildren, "GetCurTimeLessChildren");
		Util::AddFunc(VProfNode_GetPeakTime, "GetPeakTime");
		Util::AddFunc(VProfNode_GetPrevTime, "GetPrevTime");
		Util::AddFunc(VProfNode_GetPrevTimeLessChildren, "GetPrevTimeLessChildren");
		Util::AddFunc(VProfNode_GetTotalTime, "GetTotalTime");
		Util::AddFunc(VProfNode_GetTotalTimeLessChildren, "GetTotalTimeLessChildren");
		Util::AddFunc(VProfNode_GetCurCalls, "GetCurCalls");
		Util::AddFunc(VProfNode_GetChild, "GetChild");
		Util::AddFunc(VProfNode_GetParent, "GetParent");
		Util::AddFunc(VProfNode_GetSibling, "GetSibling");
		Util::AddFunc(VProfNode_GetPrevSibling, "GetPrevSibling");
		Util::AddFunc(VProfNode_GetL2CacheMisses, "GetL2CacheMisses");
		Util::AddFunc(VProfNode_GetPrevL2CacheMissLessChildren, "GetPrevL2CacheMissLessChildren");
		Util::AddFunc(VProfNode_GetPrevLoadHitStoreLessChildren, "GetPrevLoadHitStoreLessChildren");
		Util::AddFunc(VProfNode_GetTotalCalls, "GetTotalCalls");
		Util::AddFunc(VProfNode_GetSubNode, "GetSubNode");
		Util::AddFunc(VProfNode_GetClientData, "GetClientData");
		Util::AddFunc(VProfNode_MarkFrame, "MarkFrame");
		Util::AddFunc(VProfNode_ClearPrevTime, "ClearPrevTime");
		Util::AddFunc(VProfNode_Pause, "Pause");
		Util::AddFunc(VProfNode_Reset, "Reset");
		Util::AddFunc(VProfNode_ResetPeak, "ResetPeak");
		Util::AddFunc(VProfNode_Resume, "Resume");
		Util::AddFunc(VProfNode_SetBudgetGroupID, "SetBudgetGroupID");
		Util::AddFunc(VProfNode_SetCurFrameTime, "SetCurFrameTime");
		Util::AddFunc(VProfNode_SetClientData, "SetClientData");
		Util::AddFunc(VProfNode_EnterScope, "EnterScope");
		Util::AddFunc(VProfNode_ExitScope, "ExitScope");
	g_Lua->Pop(1);

	Util::StartTable();
		Util::AddFunc(vprof_Start, "Start");
		Util::AddFunc(vprof_Stop, "Stop");
		Util::AddFunc(vprof_AtRoot, "AtRoot");
		Util::AddFunc(vprof_FindOrCreateCounter, "FindOrCreateCounter");
		Util::AddFunc(vprof_GetCounter, "GetCounter");
		Util::AddFunc(vprof_GetNumCounters, "GetNumCounters");
		Util::AddFunc(vprof_ResetCounters, "ResetCounters");
		Util::AddFunc(vprof_GetTimeLastFrame, "GetTimeLastFrame");
		Util::AddFunc(vprof_GetPeakFrameTime, "GetPeakFrameTime");
		Util::AddFunc(vprof_GetTotalTimeSampled, "GetTotalTimeSampled");
		Util::AddFunc(vprof_GetDetailLevel, "GetDetailLevel");
		Util::AddFunc(vprof_NumFramesSampled, "NumFramesSampled");
		Util::AddFunc(vprof_HideBudgetGroup, "HideBudgetGroup");
		Util::AddFunc(vprof_GetNumBudgetGroups, "GetNumBudgetGroups");
		Util::AddFunc(vprof_BudgetGroupNameToBudgetGroupID, "BudgetGroupNameToBudgetGroupID");
		Util::AddFunc(vprof_GetBudgetGroupName, "GetBudgetGroupName");
		Util::AddFunc(vprof_GetBudgetGroupFlags, "GetBudgetGroupFlags");
		Util::AddFunc(vprof_GetBudgetGroupColor, "GetBudgetGroupColor");
		Util::AddFunc(vprof_GetRoot, "GetRoot");
		Util::AddFunc(vprof_GetCurrentNode, "GetCurrentNode");
		Util::AddFunc(vprof_IsEnabled, "IsEnabled");
		Util::AddFunc(vprof_MarkFrame, "MarkFrame");
		Util::AddFunc(vprof_EnterScope, "EnterScope");
		Util::AddFunc(vprof_ExitScope, "ExitScope");
		Util::AddFunc(vprof_Pause, "Pause");
		Util::AddFunc(vprof_Resume, "Resume");
		Util::AddFunc(vprof_Reset, "Reset");
		Util::AddFunc(vprof_ResetPeaks, "ResetPeaks");
		Util::AddFunc(vprof_Term, "Term");

		Util::AddValue(COUNTER_GROUP_DEFAULT, "COUNTER_GROUP_DEFAULT");
		Util::AddValue(COUNTER_GROUP_NO_RESET, "COUNTER_GROUP_NO_RESET");
		Util::AddValue(COUNTER_GROUP_TEXTURE_GLOBAL, "COUNTER_GROUP_TEXTURE_GLOBAL");
		Util::AddValue(COUNTER_GROUP_TEXTURE_PER_FRAME, "COUNTER_GROUP_TEXTURE_PER_FRAME");
		Util::AddValue(COUNTER_GROUP_TELEMETRY, "COUNTER_GROUP_TELEMETRY");
	Util::FinishTable("vprof");
}

void CVProfModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable("vprof");
}