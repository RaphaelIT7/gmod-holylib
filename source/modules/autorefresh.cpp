#include "module.h"
#include "LuaInterface.h"
#include "lua.h"
#include "detours.h"

#include <unordered_map>

#include "tier0/memdbgon.h"

class CAutoRefreshModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "autorefresh"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; };
};

CAutoRefreshModule g_pAutoRefreshModule;
IModule* pAutoRefreshModule = &g_pAutoRefreshModule;

static std::unordered_map<std::string, bool> blockedLuaFilesMap = {};
LUA_FUNCTION_STATIC(DenyLuaAutoRefresh)
{
	LUA->CheckType(1, GarrysMod::Lua::Type::String);
	LUA->CheckType(2, GarrysMod::Lua::Type::Bool);

	const char* inputFilePath = LUA->GetString(1);
	bool blockStatus = LUA->GetBool(2);
	char normalizedPath[260];
	V_FixupPathName(normalizedPath, sizeof(normalizedPath), inputFilePath);
	blockedLuaFilesMap.insert_or_assign(std::string(normalizedPath), blockStatus);

	return 0;
}

static Detouring::Hook detour_CAutoRefresh_HandleChange_Lua;
static bool hook_CAutoRefresh_HandleChange_Lua(const std::string* pfileRelPath, const std::string* pfileName, const std::string* pfileExt)
{
	auto trampoline = detour_CAutoRefresh_HandleChange_Lua.GetTrampoline<Symbols::GarrysMod_AutoRefresh_HandleChange_Lua>();
	if (!g_Lua || !pfileRelPath || !pfileName || !pfileExt)
	{
		return trampoline(pfileRelPath, pfileName, pfileExt);
	}

	if (std::string(pfileExt->substr(0, 3)) != "lua")
	{
		return trampoline(pfileRelPath, pfileName, pfileExt);
	}

	bool bDenyRefresh = false;
	if (Lua::PushHook("HolyLib:PreLuaAutoRefresh"))
	{
		g_Lua->PushString(pfileRelPath->c_str());
		g_Lua->PushString(pfileName->c_str());

		if (g_Lua->CallFunctionProtected(3, 1, true))
		{
			bDenyRefresh = g_Lua->GetBool(-1);
			g_Lua->Pop(1);
		}
	}

	if (!blockedLuaFilesMap.empty() && !bDenyRefresh)
	{
		char fullPath[260];
		V_ComposeFileName(pfileRelPath->c_str(), pfileName->c_str(), fullPath, sizeof(fullPath));
		V_SetExtension(fullPath, "lua", sizeof(fullPath));
		if (auto fileSearch = blockedLuaFilesMap.find(fullPath); fileSearch != blockedLuaFilesMap.end())
		{
			bDenyRefresh = fileSearch->second;
		}
	}

	if (bDenyRefresh)
	{
		return true;
	}

	bool originalResult = trampoline(pfileRelPath, pfileName, pfileExt);

	if (Lua::PushHook("HolyLib:PostLuaAutoRefresh"))
	{
		g_Lua->PushString(pfileRelPath->c_str());
		g_Lua->PushString(pfileName->c_str());

		g_Lua->CallFunctionProtected(3, 0, true);
	}

	return originalResult;
};

void CAutoRefreshModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Util::StartTable(pLua);
		Util::AddFunc(pLua, DenyLuaAutoRefresh, "DenyLuaAutoRefresh");
	Util::FinishTable(pLua, "autorefresh");
}

void CAutoRefreshModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "autorefresh");
}

void CAutoRefreshModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::FactoryLoader server_loader("server");
	Detour::Create(
		&detour_CAutoRefresh_HandleChange_Lua, "CAutoRefresh_HandleChange_Lua",
		server_loader.GetModule(), Symbols::GarrysMod_AutoRefresh_HandleChange_LuaSym,
		(void*)hook_CAutoRefresh_HandleChange_Lua, m_pID
	);
}
