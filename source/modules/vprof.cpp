// We must include them first since else windows starts yapping about private being a macro in STL headers
#include <threadtools.h>
#include <dbg.h>
#define private public
#include <vprof.h>
#undef private
#include <ivprofexport.h>

#include "filesystem_base.h" // Has to be before symbols.h
#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <map>
#include "color.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CVProfModule : public IModule
{
public:
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void InitDetour(bool bPreServer) override;
	const char* Name() override { return "vprof"; };
	int Compatibility() override { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
	bool SupportsMultipleLuaStates() override { return true; };
	// NOTE for myself: Linux64 seemingly doesn't have vprof enabled! so don't suppositly add compatibility!
	// Update: Fk my old self, Linux64 is just broken and it crashed because I had the wrong symbols.
	// Update 2: The issues with vprof were fixed a good while ago on Linux64
};

static ConVar holylib_vprof_exportreport("holylib_vprof_exportreport", "1", FCVAR_ARCHIVE, "If enabled, vprof results will be dumped into a file in the vprof/ folder");
//static ConVar holylib_vprof_profilecfunc("holylib_vprof_profilecfunc", "0", 0, "If enabled, Lua->C calls will also be profiled.");

static CVProfModule g_pVProfModule;
IModule* pVProfModule = &g_pVProfModule;

/*
	Some notes for VPROF:
		- For Nodes it compares the name POINTER not the contents! If no pointer matches a new node is created!
		- VProf crashes when reaching a depth of ~1024
*/

static std::string GetCurrentTime()
{ // Yoink from vprof module
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

static SpewOutputFunc_t originalSpew = nullptr;
static void StartSpew()
{
	originalSpew = GetSpewOutputFunc();
	SpewOutputFunc(VProf_Spew);
}

static void FinishSpew()
{
	SpewOutputFunc(originalSpew);
	originalSpew = nullptr;
}
#else
static Detouring::Hook detour_Msg; // Yes. I don't like this.
static void hook_Msg(PRINTF_FORMAT_STRING const tchar* pMsg, ...)
{
	va_list args;
	va_start(args, pMsg);

	int size = vsnprintf(nullptr, 0, pMsg, args);
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
static void hook_CVProfile_OutputReport(void* pProfile, int type, const tchar* pszStartMode, int budgetGroupID)
{
	if (!holylib_vprof_exportreport.GetBool())
	{
		detour_CVProfile_OutputReport.GetTrampoline<Symbols::CVProfile_OutputReport>()(pProfile, type, pszStartMode, budgetGroupID);
		return;
	}

	StartSpew();

	detour_CVProfile_OutputReport.GetTrampoline<Symbols::CVProfile_OutputReport>()(pProfile, type, pszStartMode, budgetGroupID);

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

static const char* pCurrentGamemodeFunction = nullptr;
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

static unordered_map<std::string_view, std::string> CallFinish_strs;
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
	pCurrentGamemodeFunction = nullptr;

	return detour_CLuaGamemode_CallFinish.GetTrampoline<Symbols::CLuaGamemode_CallFinish>()(funky_srv, pArgs);
}

static unordered_map<uint32_t, std::string> Call_strs;
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

static unordered_map<std::string_view, std::string> CallStr_strs;
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
static unordered_map<std::string_view, std::string> ScriptedEntity_StartFunctionStr_strs;
static Detouring::Hook detour_CScriptedEntity_StartFunctionStr;
static void* hook_CScriptedEntity_StartFunctionStr(void* funky_srv, const char* str) // Only used by GetSoundInterests
{
	if (!g_Lua || !ThreadInMainThread())
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

static unordered_map<int, std::string> CScriptedEntity_StartFunction_strs;
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

static unordered_map<std::string_view, std::string> CScriptedEntity_CallFunctionStr_strs;
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

static unordered_map<int, std::string> CScriptedEntity_CallFunction_strs;
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

#if SYSTEM_WINDOWS
DETOUR_THISCALL_START()
	DETOUR_THISCALL_ADDFUNC3( hook_CVProfile_OutputReport, OutputReport, void*, int, const tchar*, int );
	DETOUR_THISCALL_ADDRETFUNC1( hook_CLuaGamemode_CallWithArgs, void*, CallWithArgs, void*, int );
	DETOUR_THISCALL_ADDRETFUNC1( hook_CLuaGamemode_CallWithArgsStr, void*, CallWithArgsStr, void*, const char* );
	DETOUR_THISCALL_ADDRETFUNC1( hook_CLuaGamemode_CallFinish, void*, CallFinish, void*, int );
	DETOUR_THISCALL_ADDRETFUNC1( hook_CLuaGamemode_Call, void*, CallInt, void*, int );
	DETOUR_THISCALL_ADDRETFUNC1( hook_CLuaGamemode_CallStr, void*, CallStr, void*, const char* );
	DETOUR_THISCALL_ADDRETFUNC1( hook_CScriptedEntity_StartFunctionStr, void*, StartFunctionStr, void*, const char* );
	DETOUR_THISCALL_ADDRETFUNC1( hook_CScriptedEntity_StartFunction, void*, StartFunctionInt, void*, int );
	DETOUR_THISCALL_ADDRETFUNC2( hook_CScriptedEntity_Call, void*, ScriptedCall, void*, int, int );
	DETOUR_THISCALL_ADDRETFUNC1( hook_CScriptedEntity_CallFunctionStr, void*, CallFunctionStr, void*, const char* );
	DETOUR_THISCALL_ADDRETFUNC1( hook_CScriptedEntity_CallFunction, void*, CallFunctionInt, void*, int );
DETOUR_THISCALL_FINISH();
#endif

struct StringHash {
	using is_transparent = void;
	size_t operator()(std::string_view sv) const noexcept {
		return std::hash<std::string_view>{}(sv);
	}
};

struct StringEq {
	using is_transparent = void;
	bool operator()(std::string_view a, std::string_view b) const noexcept {
		return a == b;
	}
};

static unordered_set<std::string, StringHash, StringEq> pLuaStrings; // Theses will almost never be freed!
// VPROF doesn't manage the memory of the strings that are used in scopes!
// So we need to make sure that they will always be valid.
// It does manage the strings of counters and budget groups
static const char* AddOrGetString(const char* pString)
{
	std::string_view strValue = pString;
	auto it = pLuaStrings.find(strValue);
	if (it != pLuaStrings.end())
		return it->c_str();

	auto [newIt, inserted] = pLuaStrings.emplace(strValue);
	return newIt->c_str();
}

#if 0
struct lua_Debug {
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

#if defined(ARCHITECTURE_X86) && defined(SYSTEM_LINUX)
// HACK!
// GMod's libtier0_srv.so is fucked
// All CL2Cache are LOCAL and not GLOBAL (inside .symtab) and do NOT exist inside .dynsym causing the linker to NOT link against them!
// This probably happens because CVProfNode::CVProfNode is inlined and since only the CVProfNode class uses CL2Cache
// And CVProfNode is only created inside CVProfNode::GetSubNode which is defined in tier0 the compiler decided to hide it? I dunno
static Symbols::CL2Cache_Constructor func_CL2Cache_Constructor = nullptr;
CL2Cache::CL2Cache()
{
	if (func_CL2Cache_Constructor)
		func_CL2Cache_Constructor(this);
}

static Symbols::CL2Cache_Deconstructor func_CL2Cache_Deconstructor = nullptr;
CL2Cache::~CL2Cache()
{
	if (func_CL2Cache_Deconstructor)
		func_CL2Cache_Deconstructor(this);
}
#endif

/*
	IMPORTANT:
		Our class MUST not make the engine depend on HolyLib
		We must initialize the normal node BUT can store additional things in our
		BUT we can never expect all nodes to be from HolyLib!
		This design then also allows one to enable/disable this module at runtime without issues :)
*/
static constexpr int NODEID_OFFSET = 1 << 28;
class HolyLib_CVProfNode : public CVProfNode
{
public:
	HolyLib_CVProfNode(const tchar* pszName, int detailLevel, HolyLib_CVProfNode *pParent, const tchar *pBudgetGroupName, int budgetFlags)
		: CVProfNode(pszName, detailLevel, pParent, pBudgetGroupName, budgetFlags)
	{
		m_iUniqueNodeID += NODEID_OFFSET;

		if (pParent->IsHolyLibNode())
			pParent->AddChildNode(this);
	}

	inline bool IsHolyLibNode()
	{
		return m_iUniqueNodeID > NODEID_OFFSET;
	}

	// ONLY use these on HolyLib nodes!

	inline HolyLib_CVProfNode* FindChildNode(const char* name)
	{
		auto it = m_pChildren.find(name);
		if (it != m_pChildren.end())
			return it->second;

		return nullptr;
	}

	inline void AddChildNode(HolyLib_CVProfNode* pChild)
	{
		m_pChildren[pChild->GetName()] = pChild;
	}

	inline size_t ChildCount()
	{
		return m_pChildren.size();
	}

	inline void SetCachedParent(CVProfNode* pParent, int budgetFlagsFilter)
	{
		m_pCachedParent = pParent;
		m_nCachedParentFilter = budgetFlagsFilter;
	}

	inline CVProfNode* GetCachedParent(int budgetFlagsFilter)
	{
		if (budgetFlagsFilter == m_nCachedParentFilter)
			return m_pCachedParent;

		return nullptr;
	}

private:
	unordered_map<const char*, HolyLib_CVProfNode*> m_pChildren;

	CVProfNode* m_pCachedParent = nullptr;
	// If our filter changes our parent may no longer be valid
	// Though this usually only happens on a client when the budget panel is used.
	int m_nCachedParentFilter = 0;
};

static Detouring::Hook detour_CVProfNode_GetSubNode;
static CVProfNode* hook_CVProfNode_GetSubNode(HolyLib_CVProfNode* _this, const tchar* pszName, int detailLevel, const tchar* pBudgetGroupName, int budgetFlags)
{
	if (_this->IsHolyLibNode() && _this->ChildCount() > 10)
	{
		// For wider nodes (one with many children) this will be faster
		HolyLib_CVProfNode* pChild = _this->FindChildNode(pszName);
		if (pChild)
			return pChild;
	} else {
		// Try to find this sub node
		CVProfNode* child = _this->m_pChild;
		while (child) 
		{
			if (child->m_pszName == pszName) 
				return child;

			child = child->m_pSibling;
		}
	}

	// We didn't find it, so add it
	auto * node = new HolyLib_CVProfNode( pszName, detailLevel, _this, pBudgetGroupName, budgetFlags );
	node->m_pSibling = _this->m_pChild;
	_this->m_pChild = node;

	return node;
}

class CVProfExport : public IVProfExport
{
public:
	inline bool CanShowBudgetGroup(int iGroup)
	{
		return (g_VProfCurrentProfile.GetBudgetGroupFlags(iGroup) & m_BudgetFlagsFilter) != 0;
	}

public:
	CUtlVector<float> m_Times;	// Times from the most recent snapshot.
	int m_nListeners;
	int m_BudgetFlagsFilter;	// We can only capture one type of filtered data at a time.
	bool m_bStart;
	bool m_bStop;
};

// CVProfNode::MarkFrame() is also recursive though shouldn't become a performance issue

static bool g_bUsingCVProfExport = false;
// static so that we don't keep freeing & allocating
// We use this also for hook_CVProfNode_MarkFrame to avoid ending up with two static large vectors
static std::vector<CVProfNode*> g_pNodeStack;
static Symbols::VProfRecord_IsPlayingBack func_VProfRecord_IsPlayingBack;
static Detouring::Hook detour_CVProfExport_CalculateBudgetGroupTimes_Recursive;
void hook_CVProfExport_CalculateBudgetGroupTimes_Recursive(CVProfExport* pExport, HolyLib_CVProfNode *pNode);
void hook_CVProfExport_CalculateBudgetGroupTimes_Recursive(CVProfExport* pExport, HolyLib_CVProfNode *pNode)
{
	// RaphaelIT7:
	// for servers this entire function is basically useless and eats performance like hell
	// though I won't just break it as there is a VProfExport001 interface 
	// NOTE: We also don't need to call ClearPrevTime here because first, it's unused, and second MarkFrame overrides it anyways
	if (pExport->m_nListeners <= 0)
	{
		g_bUsingCVProfExport = false;
		return;
	}

	g_bUsingCVProfExport = true;

	// We don't do this recursively anymore as I did encounter a stack overflow crash
	bool bIsNotPlayingBack = func_VProfRecord_IsPlayingBack && !func_VProfRecord_IsPlayingBack();
	g_pNodeStack.push_back(pNode);
	while (!g_pNodeStack.empty())
	{
		pNode = (HolyLib_CVProfNode*)g_pNodeStack.back();
		g_pNodeStack.pop_back();

		if (!pNode)
			continue;

		// If this node's info is filtered out, then put it in its parent's budget group.
		bool bDoParentSearch = true;
		CVProfNode* pTestNode = pNode;
		if (pNode->IsHolyLibNode())
		{
			pTestNode = pNode->GetCachedParent(pExport->m_BudgetFlagsFilter);
			if (pTestNode)
				bDoParentSearch = false;
			else
				pTestNode = pNode;
		}

		while ( bDoParentSearch && pTestNode != g_VProfCurrentProfile.GetRoot() && 
				( !pExport->CanShowBudgetGroup( pTestNode->GetBudgetGroupID() ) || 
					( g_VProfCurrentProfile.GetBudgetGroupFlags( pTestNode->GetBudgetGroupID() ) & BUDGETFLAG_HIDDEN ) != 0 ) )
		{
			pTestNode = pTestNode->GetParent();
		}

		if (!bDoParentSearch)
			pNode->SetCachedParent(pNode, pExport->m_BudgetFlagsFilter);

		int groupID = pTestNode->GetBudgetGroupID();
		if (groupID >= 0 && groupID < MIN(pExport->m_Times.Count(), (intp)IVProfExport::MAX_BUDGETGROUP_TIMES))
			pExport->m_Times[groupID] += pNode->GetPrevTimeLessChildren();
		else
			Assert(false);

		if(pNode->GetSibling())
			g_pNodeStack.push_back((HolyLib_CVProfNode*)pNode->GetSibling());

		if(pNode->GetChild())
			g_pNodeStack.push_back((HolyLib_CVProfNode*)pNode->GetChild());

		if (bIsNotPlayingBack)
			pNode->ClearPrevTime();
	}
}

static Detouring::Hook detour_CVProfNode_MarkFrame;
static void hook_CVProfNode_MarkFrame(CVProfNode* pNode)
{
	g_pNodeStack.push_back(pNode);

	if (g_bUsingCVProfExport)
	{
		// This is quite "expensive" simply due to how much we read-write
		// expensive in quotes since I was testing with 6750000 nodes
		while (!g_pNodeStack.empty())
		{
			pNode = g_pNodeStack.back();
			g_pNodeStack.pop_back();

			pNode->m_nPrevFrameCalls = pNode->m_nCurFrameCalls;
			pNode->m_PrevFrameTime = pNode->m_CurFrameTime;
			pNode->m_iPrevL2CacheMiss = pNode->m_iCurL2CacheMiss;

			pNode->m_nTotalCalls += pNode->m_nCurFrameCalls;
			pNode->m_TotalTime += pNode->m_CurFrameTime;

			if (pNode->m_PeakTime.IsLessThan(pNode->m_CurFrameTime))
				pNode->m_PeakTime = pNode->m_CurFrameTime;

			pNode->m_CurFrameTime.Init();
			pNode->m_nCurFrameCalls = 0;

			pNode->m_iTotalL2CacheMiss += pNode->m_iCurL2CacheMiss;
			pNode->m_iCurL2CacheMiss = 0;

			if (pNode->m_pSibling)
				g_pNodeStack.push_back(pNode->m_pSibling);

			if (pNode->m_pChild)
				g_pNodeStack.push_back(pNode->m_pChild);
		}
	} else {
		// Simple MarkFrame when we don't need all values
		// Stuff like L2Cache, PrevFrame are not set in here
		while (!g_pNodeStack.empty())
		{
			pNode = g_pNodeStack.back();
			g_pNodeStack.pop_back();

			if (pNode->m_pSibling)
				g_pNodeStack.push_back(pNode->m_pSibling);

			// HUGE: This saves a HUGE amount of calls as we skip nodes that weren't touched.
			if (pNode->m_nCurFrameCalls == 0)
				continue;

			pNode->m_nTotalCalls += pNode->m_nCurFrameCalls;
			pNode->m_TotalTime += pNode->m_CurFrameTime;

			if (pNode->m_PeakTime.IsLessThan(pNode->m_CurFrameTime))
				pNode->m_PeakTime = pNode->m_CurFrameTime;

			pNode->m_CurFrameTime.Init();
			pNode->m_nCurFrameCalls = 0;

			if (pNode->m_pChild)
				g_pNodeStack.push_back(pNode->m_pChild);
		}
	}
}

static Detouring::Hook detour_CVProfile_FreeNodes_R;
static void hook_CVProfile_FreeNodes_R(CVProfile* profile, HolyLib_CVProfNode* pNode);
static void hook_CVProfile_FreeNodes_R(CVProfile* profile, HolyLib_CVProfNode* pNode)
{
	CVProfNode *pNext;
	for (CVProfNode *pChild = pNode->GetChild(); pChild; pChild = pNext)
	{
		pNext = pChild->GetSibling();
		hook_CVProfile_FreeNodes_R( profile, (HolyLib_CVProfNode*)pChild );
	}
	
	if ( pNode == profile->GetRoot() )
		pNode->m_pChild = nullptr;
	else {
		// We check and cast properly since CVProfNode::~CVProfNode is NOT a virtual deconstructor!
		if (pNode->IsHolyLibNode())
			delete (HolyLib_CVProfNode*)pNode;
		else
			delete (CVProfNode*)pNode;
	}
}

void CVProfModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	DETOUR_PREPARE_THISCALL();
	SourceSDK::ModuleLoader tier0_loader("tier0");
	Detour::Create(
		&detour_CVProfile_OutputReport, "CVProfile::OutputReport",
		tier0_loader.GetModule(), Symbols::CVProfile_OutputReportSym,
		(void*)DETOUR_THISCALL( hook_CVProfile_OutputReport, OutputReport ), m_pID
	);
	
	SourceSDK::ModuleLoader server_loader("server");
	Detour::Create(
		&detour_CLuaGamemode_CallStr, "CLuaGamemode::Call(const char*)",
		server_loader.GetModule(), Symbols::CLuaGamemode_CallStrSym,
		(void*)DETOUR_THISCALL( hook_CLuaGamemode_CallStr, CallStr ), m_pID
	);

	Detour::Create(
		&detour_CLuaGamemode_Call, "CLuaGamemode::Call(int)",
		server_loader.GetModule(), Symbols::CLuaGamemode_CallSym,
		(void*)DETOUR_THISCALL( hook_CLuaGamemode_Call, CallInt ), m_pID
	);

	Detour::Create(
		&detour_CLuaGamemode_CallFinish, "CLuaGamemode::CallFinish",
		server_loader.GetModule(), Symbols::CLuaGamemode_CallFinishSym,
		(void*)DETOUR_THISCALL( hook_CLuaGamemode_CallFinish, CallFinish ), m_pID
	);

	Detour::Create(
		&detour_CLuaGamemode_CallWithArgsStr, "CLuaGamemode::CallWithArgs(const char*)",
		server_loader.GetModule(), Symbols::CLuaGamemode_CallWithArgsStrSym,
		(void*)DETOUR_THISCALL( hook_CLuaGamemode_CallWithArgsStr, CallWithArgsStr ), m_pID
	);

	Detour::Create(
		&detour_CLuaGamemode_CallWithArgs, "CLuaGamemode::CallWithArgs(int)",
		server_loader.GetModule(), Symbols::CLuaGamemode_CallWithArgsSym,
		(void*)DETOUR_THISCALL( hook_CLuaGamemode_CallWithArgs, CallWithArgs ), m_pID
	);

	Detour::Create(
		&detour_CScriptedEntity_StartFunctionStr, "CScriptedEntity::StartFunction(const char*)",
		server_loader.GetModule(), Symbols::CScriptedEntity_StartFunctionStrSym,
		(void*)DETOUR_THISCALL( hook_CScriptedEntity_StartFunctionStr, StartFunctionStr ), m_pID
	);

	Detour::Create(
		&detour_CScriptedEntity_StartFunction, "CScriptedEntity::StartFunction(int)",
		server_loader.GetModule(), Symbols::CScriptedEntity_StartFunctionSym,
		(void*)DETOUR_THISCALL( hook_CScriptedEntity_StartFunction, StartFunctionInt ), m_pID
	);

	Detour::Create(
		&detour_CScriptedEntity_Call, "CScriptedEntity::Call",
		server_loader.GetModule(), Symbols::CScriptedEntity_CallSym,
		(void*)DETOUR_THISCALL( hook_CScriptedEntity_Call, ScriptedCall ), m_pID
	);

	Detour::Create(
		&detour_CScriptedEntity_CallFunctionStr, "CScriptedEntity::CallFunction(const char*)",
		server_loader.GetModule(), Symbols::CScriptedEntity_CallFunctionStrSym,
		(void*)DETOUR_THISCALL( hook_CScriptedEntity_CallFunctionStr, CallFunctionStr ), m_pID
	);

	Detour::Create(
		&detour_CScriptedEntity_CallFunction, "CScriptedEntity::CallFunction(int)",
		server_loader.GetModule(), Symbols::CScriptedEntity_CallFunctionSym,
		(void*)DETOUR_THISCALL( hook_CScriptedEntity_CallFunction, CallFunctionInt ), m_pID
	);

#if defined(ARCHITECTURE_X86) 
	SourceSDK::ModuleLoader engine_loader("engine");
	Detour::Create(
		&detour_CVProfNode_GetSubNode, "CVProfNode::GetSubNode",
		tier0_loader.GetModule(), Symbols::CVProfNode_GetSubNodeSym,
		(void*)hook_CVProfNode_GetSubNode, m_pID
	);

	Detour::Create(
		&detour_CVProfExport_CalculateBudgetGroupTimes_Recursive, "CVProfExport::CalculateBudgetGroupTimes_Recursive",
		engine_loader.GetModule(), Symbols::CVProfExport_CalculateBudgetGroupTimes_RecursiveSym,
		(void*)hook_CVProfExport_CalculateBudgetGroupTimes_Recursive, m_pID
	);

	Detour::Create(
		&detour_CVProfNode_MarkFrame, "CVProfNode::MarkFrame",
		tier0_loader.GetModule(), Symbols::CVProfNode_MarkFrameSym,
		(void*)hook_CVProfNode_MarkFrame, m_pID
	);

	Detour::Create(
		&detour_CVProfile_FreeNodes_R, "CVProfile::FreeNodes_R",
		tier0_loader.GetModule(), Symbols::CVProfile_FreeNodes_RSym,
		(void*)hook_CVProfile_FreeNodes_R, m_pID
	);

	func_VProfRecord_IsPlayingBack = (Symbols::VProfRecord_IsPlayingBack)Detour::GetFunction(engine_loader.GetModule(), Symbols::VProfRecord_IsPlayingBackSym);
	Detour::CheckFunction((void*)func_VProfRecord_IsPlayingBack, "VProfRecord_IsPlayingBack");

#if defined(SYSTEM_LINUX)
	func_CL2Cache_Constructor = (Symbols::CL2Cache_Constructor)Detour::GetFunction(tier0_loader.GetModule(), Symbols::CL2Cache_ConstructorSym);
	Detour::CheckFunction((void*)func_CL2Cache_Constructor, "CL2Cache::Constructor");

	func_CL2Cache_Deconstructor = (Symbols::CL2Cache_Deconstructor)Detour::GetFunction(tier0_loader.GetModule(), Symbols::CL2Cache_DeconstructorSym);
	Detour::CheckFunction((void*)func_CL2Cache_Deconstructor, "CL2Cache::Deconstructor");
#endif
#endif

#if defined(ARCHITECTURE_X86) && 0
	Detour::Create(
		&detour_lj_BC_FUNCC, "lj_BC_FUNCC",
		server_loader.GetModule(), Symbols::lj_BC_FUNCCSym,
		(void*)hook_lj_BC_FUNCC, m_pID
	);

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
	const char* strName = nullptr;
#if ARCHITECTURE_IS_X86_64
	int* iValue = nullptr;
#else
	int64* iValue = nullptr;
#endif
};

Push_LuaClass(VProfCounter)
Get_LuaClass(VProfCounter, "VProfCounter")

LUA_FUNCTION_STATIC(VProfCounter__tostring)
{
	VProfCounter* pData = Get_VProfCounter(LUA, 1, false);
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
	VProfCounter* pCounter = (VProfCounter*)pStoredData;
	if (pCounter)
		delete pCounter;
)

LUA_FUNCTION_STATIC(VProfCounter_GetName)
{
	VProfCounter* counter = Get_VProfCounter(LUA, 1, true);

	LUA->PushString(counter->strName);
	return 1;
}

LUA_FUNCTION_STATIC(VProfCounter_Set)
{
	VProfCounter* counter = Get_VProfCounter(LUA, 1, true);
	(*counter->iValue) = (int64)LUA->CheckNumber(2);

	return 0;
}

LUA_FUNCTION_STATIC(VProfCounter_Get)
{
	VProfCounter* counter = Get_VProfCounter(LUA, 1, true);
	
	LUA->PushNumber((double)*counter->iValue);
	return 1;
}

LUA_FUNCTION_STATIC(VProfCounter_Increment)
{
	VProfCounter* counter = Get_VProfCounter(LUA, 1, true);

	++(*counter->iValue);
	return 0;
}

LUA_FUNCTION_STATIC(VProfCounter_Decrement)
{
	VProfCounter* counter = Get_VProfCounter(LUA, 1, true);

	--(*counter->iValue);
	return 0;
}

// --------------------------

// ------------- VProf Node --------------

Push_LuaClass(CVProfNode)
Get_LuaClass(CVProfNode, "VProfNode")

LUA_FUNCTION_STATIC(VProfNode__tostring)
{
	CVProfNode* pData = Get_CVProfNode(LUA, 1, false);
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
	CVProfNode* pNode = (CVProfNode*)pStoredData;
	if (pNode)
		delete pNode;
)

LUA_FUNCTION_STATIC(VProfNode_GetName)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	LUA->PushString(node->GetName());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetBudgetGroupID)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	LUA->PushNumber(node->GetBudgetGroupID());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetCurTime)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	LUA->PushNumber(node->GetCurTime());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetCurTimeLessChildren)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	LUA->PushNumber(node->GetCurTimeLessChildren());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetPeakTime)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	LUA->PushNumber(node->GetPeakTime());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetPrevTime)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	LUA->PushNumber(node->GetPrevTime());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetPrevTimeLessChildren)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	LUA->PushNumber(node->GetPrevTimeLessChildren());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetTotalTime)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	LUA->PushNumber(node->GetTotalTime());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetTotalTimeLessChildren)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	LUA->PushNumber(node->GetTotalTimeLessChildren());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetCurCalls)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	LUA->PushNumber(node->GetCurCalls());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetChild)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	Push_CVProfNode(LUA, node->GetChild());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetParent)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	Push_CVProfNode(LUA, node->GetParent());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetSibling)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	Push_CVProfNode(LUA, node->GetSibling());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetPrevSibling)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	Push_CVProfNode(LUA, node->GetPrevSibling());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetL2CacheMisses)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	LUA->PushNumber(node->GetL2CacheMisses());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetPrevL2CacheMissLessChildren)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	LUA->PushNumber(node->GetPrevL2CacheMissLessChildren());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetPrevLoadHitStoreLessChildren)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	LUA->PushNumber(node->GetPrevLoadHitStoreLessChildren());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetTotalCalls)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	LUA->PushNumber(node->GetTotalCalls());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetSubNode)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);
	const char* pName = AddOrGetString(LUA->CheckString(2));
	int iDetailLevel = (int)LUA->CheckNumber(3);
	const char* pBudgetGroup = LUA->CheckString(4);

	Push_CVProfNode(LUA, node->GetSubNode(pName, iDetailLevel, pBudgetGroup));
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_GetClientData)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);

	LUA->PushNumber(node->GetClientData());
	return 1;
}

LUA_FUNCTION_STATIC(VProfNode_MarkFrame)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);
	node->MarkFrame();

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_ClearPrevTime)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);
	node->ClearPrevTime();

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_Pause)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);
	node->Pause();

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_Reset)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);
	node->Reset();

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_ResetPeak)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);
	node->ResetPeak();

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_Resume)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);
	node->Resume();

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_SetBudgetGroupID)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);
	node->SetBudgetGroupID((int)LUA->CheckNumber(2));

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_SetCurFrameTime)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);
	node->SetCurFrameTime((unsigned long)LUA->CheckNumber(2));

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_SetClientData)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);
	node->SetClientData((int)LUA->CheckNumber(2));

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_EnterScope)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);
	node->EnterScope();

	return 0;
}

LUA_FUNCTION_STATIC(VProfNode_ExitScope)
{
	CVProfNode* node = Get_CVProfNode(LUA, 1, true);
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
#ifndef WIN32
	CounterGroup_t group = (CounterGroup_t)LUA->CheckNumberOpt(2, COUNTER_GROUP_DEFAULT);
#endif

	LuaUserData* pUserData = PushInlined_VProfCounter(LUA);
	VProfCounter* counter = (VProfCounter*)pUserData->GetData();

	counter->strName = pName;
#ifndef WIN32
	counter->iValue = g_VProfCurrentProfile.FindOrCreateCounter(pName, group);
#endif

	return 1;
}

LUA_FUNCTION_STATIC(vprof_GetCounter)
{
	int index = (int)LUA->CheckNumber(1);

	LuaUserData* pUserData = PushInlined_VProfCounter(LUA);
	VProfCounter* counter = (VProfCounter*)pUserData->GetData();

	counter->strName = g_VProfCurrentProfile.GetCounterName(index);

#ifndef WIN32
	if (counter->strName)
	{
		counter->iValue = g_VProfCurrentProfile.FindOrCreateCounter(counter->strName);

		Push_VProfCounter(LUA, counter);
	} else {
		LUA->PushNil();
	}
#else
	LUA->PushNil();
#endif
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
	Push_CVProfNode(LUA, g_VProfCurrentProfile.GetRoot());
	return 1;
}

LUA_FUNCTION_STATIC(vprof_GetCurrentNode)
{
	Push_CVProfNode(LUA, g_VProfCurrentProfile.GetCurrentNode());
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

// ToDo:
//   1) Maybe we should consider to add a block to depth to avoid a crash?
//   2) We should probably hook into CLuaInterface::CallFunctionProtected to then restore vprof in case of a lua error
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

	Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::VProfCounter, pLua->CreateMetaTable("VProfCounter"));
		Util::AddFunc(pLua, VProfCounter__tostring, "__tostring");
		Util::AddFunc(pLua, VProfCounter__index, "__index");
		Util::AddFunc(pLua, VProfCounter__newindex, "__newindex");
		Util::AddFunc(pLua, VProfCounter__gc, "__gc");
		LUA_REGISTER_JIT(pLua, VProfCounter_GetTable, "GetTable");
		Util::AddFunc(pLua, VProfCounter_Set, "Set");
		Util::AddFunc(pLua, VProfCounter_Get, "Get");
		Util::AddFunc(pLua, VProfCounter_Increment, "Increment");
		Util::AddFunc(pLua, VProfCounter_Decrement, "Decrement");
		Util::AddFunc(pLua, VProfCounter_GetName, "GetName");
	pLua->Pop(1);

	Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::CVProfNode, pLua->CreateMetaTable("VProfNode"));
		Util::AddFunc(pLua, VProfNode__tostring, "__tostring");
		Util::AddFunc(pLua, CVProfNode__index, "__index");
		Util::AddFunc(pLua, CVProfNode__newindex, "__newindex");
		Util::AddFunc(pLua, CVProfNode__gc, "__gc");
		LUA_REGISTER_JIT(pLua, CVProfNode_GetTable, "GetTable");
		Util::AddFunc(pLua, VProfNode_GetName, "GetName");
		Util::AddFunc(pLua, VProfNode_GetBudgetGroupID, "GetBudgetGroupID");
		Util::AddFunc(pLua, VProfNode_GetCurTime, "GetCurTime");
		Util::AddFunc(pLua, VProfNode_GetCurTimeLessChildren, "GetCurTimeLessChildren");
		Util::AddFunc(pLua, VProfNode_GetPeakTime, "GetPeakTime");
		Util::AddFunc(pLua, VProfNode_GetPrevTime, "GetPrevTime");
		Util::AddFunc(pLua, VProfNode_GetPrevTimeLessChildren, "GetPrevTimeLessChildren");
		Util::AddFunc(pLua, VProfNode_GetTotalTime, "GetTotalTime");
		Util::AddFunc(pLua, VProfNode_GetTotalTimeLessChildren, "GetTotalTimeLessChildren");
		Util::AddFunc(pLua, VProfNode_GetCurCalls, "GetCurCalls");
		Util::AddFunc(pLua, VProfNode_GetChild, "GetChild");
		Util::AddFunc(pLua, VProfNode_GetParent, "GetParent");
		Util::AddFunc(pLua, VProfNode_GetSibling, "GetSibling");
		Util::AddFunc(pLua, VProfNode_GetPrevSibling, "GetPrevSibling");
		Util::AddFunc(pLua, VProfNode_GetL2CacheMisses, "GetL2CacheMisses");
		Util::AddFunc(pLua, VProfNode_GetPrevL2CacheMissLessChildren, "GetPrevL2CacheMissLessChildren");
		Util::AddFunc(pLua, VProfNode_GetPrevLoadHitStoreLessChildren, "GetPrevLoadHitStoreLessChildren");
		Util::AddFunc(pLua, VProfNode_GetTotalCalls, "GetTotalCalls");
		Util::AddFunc(pLua, VProfNode_GetSubNode, "GetSubNode");
		Util::AddFunc(pLua, VProfNode_GetClientData, "GetClientData");
		Util::AddFunc(pLua, VProfNode_MarkFrame, "MarkFrame");
		Util::AddFunc(pLua, VProfNode_ClearPrevTime, "ClearPrevTime");
		Util::AddFunc(pLua, VProfNode_Pause, "Pause");
		Util::AddFunc(pLua, VProfNode_Reset, "Reset");
		Util::AddFunc(pLua, VProfNode_ResetPeak, "ResetPeak");
		Util::AddFunc(pLua, VProfNode_Resume, "Resume");
		Util::AddFunc(pLua, VProfNode_SetBudgetGroupID, "SetBudgetGroupID");
		Util::AddFunc(pLua, VProfNode_SetCurFrameTime, "SetCurFrameTime");
		Util::AddFunc(pLua, VProfNode_SetClientData, "SetClientData");
		Util::AddFunc(pLua, VProfNode_EnterScope, "EnterScope");
		Util::AddFunc(pLua, VProfNode_ExitScope, "ExitScope");
	pLua->Pop(1);

	Util::StartTable(pLua);
		Util::AddFunc(pLua, vprof_Start, "Start");
		Util::AddFunc(pLua, vprof_Stop, "Stop");
		Util::AddFunc(pLua, vprof_AtRoot, "AtRoot");
		Util::AddFunc(pLua, vprof_FindOrCreateCounter, "FindOrCreateCounter");
		Util::AddFunc(pLua, vprof_GetCounter, "GetCounter");
		Util::AddFunc(pLua, vprof_GetNumCounters, "GetNumCounters");
		Util::AddFunc(pLua, vprof_ResetCounters, "ResetCounters");
		Util::AddFunc(pLua, vprof_GetTimeLastFrame, "GetTimeLastFrame");
		Util::AddFunc(pLua, vprof_GetPeakFrameTime, "GetPeakFrameTime");
		Util::AddFunc(pLua, vprof_GetTotalTimeSampled, "GetTotalTimeSampled");
		Util::AddFunc(pLua, vprof_GetDetailLevel, "GetDetailLevel");
		Util::AddFunc(pLua, vprof_NumFramesSampled, "NumFramesSampled");
		Util::AddFunc(pLua, vprof_HideBudgetGroup, "HideBudgetGroup");
		Util::AddFunc(pLua, vprof_GetNumBudgetGroups, "GetNumBudgetGroups");
		Util::AddFunc(pLua, vprof_BudgetGroupNameToBudgetGroupID, "BudgetGroupNameToBudgetGroupID");
		Util::AddFunc(pLua, vprof_GetBudgetGroupName, "GetBudgetGroupName");
		Util::AddFunc(pLua, vprof_GetBudgetGroupFlags, "GetBudgetGroupFlags");
		Util::AddFunc(pLua, vprof_GetBudgetGroupColor, "GetBudgetGroupColor");
		Util::AddFunc(pLua, vprof_GetRoot, "GetRoot");
		Util::AddFunc(pLua, vprof_GetCurrentNode, "GetCurrentNode");
		Util::AddFunc(pLua, vprof_IsEnabled, "IsEnabled");
		Util::AddFunc(pLua, vprof_MarkFrame, "MarkFrame");
		Util::AddFunc(pLua, vprof_EnterScope, "EnterScope");
		Util::AddFunc(pLua, vprof_ExitScope, "ExitScope");
		Util::AddFunc(pLua, vprof_Pause, "Pause");
		Util::AddFunc(pLua, vprof_Resume, "Resume");
		Util::AddFunc(pLua, vprof_Reset, "Reset");
		Util::AddFunc(pLua, vprof_ResetPeaks, "ResetPeaks");
		Util::AddFunc(pLua, vprof_Term, "Term");

		Util::AddValue(pLua, COUNTER_GROUP_DEFAULT, "COUNTER_GROUP_DEFAULT");
		Util::AddValue(pLua, COUNTER_GROUP_NO_RESET, "COUNTER_GROUP_NO_RESET");
		Util::AddValue(pLua, COUNTER_GROUP_TEXTURE_GLOBAL, "COUNTER_GROUP_TEXTURE_GLOBAL");
		Util::AddValue(pLua, COUNTER_GROUP_TEXTURE_PER_FRAME, "COUNTER_GROUP_TEXTURE_PER_FRAME");
		Util::AddValue(pLua, COUNTER_GROUP_TELEMETRY, "COUNTER_GROUP_TELEMETRY");
	Util::FinishTable(pLua, "vprof");
}

void CVProfModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "vprof");
}