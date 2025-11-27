#include "module.h"
#include "LuaInterface.h"
#include "lua.h"
#include "detours.h"

#include <unordered_map>

#include "tier0/memdbgon.h"

class CAutoRefreshModule : public IModule
{
public:
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void Shutdown() override;
	void InitDetour(bool bPreServer) override;
	const char* Name() override { return "autorefresh"; };
	int Compatibility() override { return LINUX32; };
	bool IsEnabledByDefault() override { return false; };
};

CAutoRefreshModule g_pAutoRefreshModule;
IModule* pAutoRefreshModule = &g_pAutoRefreshModule;

static IThreadPool* pFileTimePool = nullptr; // Used when checking the file times since the filesystem can be slow.
static void OnFileTimeThreadsChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	if (!pFileTimePool)
		return;

	pFileTimePool->ExecuteAll();
	pFileTimePool->Stop();
	Util::StartThreadPool(pFileTimePool, ((ConVar*)convar)->GetInt());
}

static ConVar autorefresh_threads("holylib_autorefresh_threads", "4", FCVAR_ARCHIVE, "The number of threads to use when checking the file times", true, 1, true, 16, OnFileTimeThreadsChange);

struct FileTimeJob {
	void Init(Bootil::BString& fileName)
	{
		strFileName = fileName;

		nFileTime = g_pFullFileSystem->GetFileTime(strFileName.c_str(), "MOD");
	}

	Bootil::BString strFileName;
	long nFileTime = 0;
	bool bChanged = false;
};
static std::vector<FileTimeJob> pFileTimestamps;
static bool AddFileToAutoRefresh(Bootil::BString pFilename)
{
	for (auto& pFileEntry : pFileTimestamps)
	{
		if (pFileEntry.strFileName == pFilename)
			return false;
	}

	auto newEntry = pFileTimestamps.emplace_back();
	newEntry.Init(pFilename);
	return true;
}

static bool RemoveFileFromAutoRefresh(Bootil::BString pFilename)
{
	for(auto it = pFileTimestamps.begin(); it != pFileTimestamps.end();)
	{
		if (it->strFileName == pFilename)
		{
			pFileTimestamps.erase(it);
			return true;
		}

		it++;
	}

	return false;
}

static void CheckFileTime(FileTimeJob* pJob)
{
	pJob->nFileTime = g_pFullFileSystem->GetFileTime(pJob->strFileName.c_str(), "MOD");
}

static Bootil::File::ChangeMonitor* g_pChangeMonitor = nullptr;
static bool bForceHasChangedCall = false;
static Detouring::Hook detour_Bootil_File_ChangeMonitor_HasChanges;
static bool hook_Bootil_File_ChangeMonitor_HasChanges(Bootil::File::ChangeMonitor* pMonitor)
{
	g_pChangeMonitor = pMonitor; // We cannot just load g_pChangeMonitor since the symbol has an additional fked offset that can change with every gmod build making it unreliable.

	if (bForceHasChangedCall)
		return false;

	return detour_Bootil_File_ChangeMonitor_HasChanges.GetTrampoline<Symbols::Bootil_File_ChangeMonitor_HasChanges>()(pMonitor);
}

static Detouring::Hook detour_Bootil_File_ChangeMonitor_CheckForChanges;
static void hook_Bootil_File_ChangeMonitor_CheckForChanges(Bootil::File::ChangeMonitor* pMonitor)
{
	g_pChangeMonitor = pMonitor;

	detour_Bootil_File_ChangeMonitor_CheckForChanges.GetTrampoline<Symbols::Bootil_File_ChangeMonitor_CheckForChanges>()(pMonitor);

	if (pFileTimestamps.size() > 0)
	{

		if (pFileTimePool)
		{
			for (auto& pFileJob : pFileTimestamps)
			{
				pFileJob.bChanged = false;
				pFileTimePool->QueueCall(&CheckFileTime, &pFileJob);
			}
			pFileTimePool->ExecuteAll();

			for (auto& pFileJob : pFileTimestamps)
			{
				if (pFileJob.bChanged)
				{
					pMonitor->NoteFileChanged(pFileJob.strFileName);
					pFileJob.bChanged = false;
				}
			}
		} else { // Idk why we don't have our threadpool but this will be slower if we got many files.
			for (auto& pFileJob : pFileTimestamps)
			{
				pFileJob.bChanged = false;
				CheckFileTime(&pFileJob);
				pMonitor->NoteFileChanged(pFileJob.strFileName);
				pFileJob.bChanged = false;
			}
		}
	}
}

static std::unordered_set<Bootil::BString> pBlacklistedFiles = {};
static Detouring::Hook detour_GarrysMod_AutoRefresh_HandleChange_Lua;
static bool hook_GarrysMod_AutoRefresh_HandleChange_Lua(const std::string* pfileRelPath, const std::string* pfileName, const std::string* pfileExt)
{
	auto trampoline = detour_GarrysMod_AutoRefresh_HandleChange_Lua.GetTrampoline<Symbols::GarrysMod_AutoRefresh_HandleChange_Lua>();
	if (!g_Lua || !pfileRelPath || !pfileName || !pfileExt)
		return trampoline(pfileRelPath, pfileName, pfileExt);

	// Is this really needed? Yes it is since AutoRefresh::Cycle calls all HandleChange functions expecting them to first check the file extension... Why...
	if (std::string(pfileExt->substr(0, 3)) != "lua")
		return trampoline(pfileRelPath, pfileName, pfileExt);

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

	if (pBlacklistedFiles.find(*const_cast<Bootil::BString*>(pfileRelPath)) != pBlacklistedFiles.end())
	{
		bDenyRefresh = true;
	}

	if (bDenyRefresh)
		return true;

	bool originalResult = trampoline(pfileRelPath, pfileName, pfileExt);

	if (Lua::PushHook("HolyLib:PostLuaAutoRefresh"))
	{
		g_Lua->PushString(pfileRelPath->c_str());
		g_Lua->PushString(pfileName->c_str());

		g_Lua->CallFunctionProtected(3, 0, true);
	}

	return originalResult;
};

LUA_FUNCTION_STATIC(autorefresh_AddFileToRefresh)
{
	const char* fileName = LUA->CheckString(1);

	LUA->PushBool(AddFileToAutoRefresh(fileName));
	return 1;
}

LUA_FUNCTION_STATIC(autorefresh_RemoveFileFromRefresh)
{
	const char* fileName = LUA->CheckString(1);

	LUA->PushBool(RemoveFileFromAutoRefresh(fileName));
	return 1;
}

LUA_FUNCTION_STATIC(autorefresh_AddFolderToRefresh)
{
	if (!g_pChangeMonitor)
		LUA->ThrowError("Failed to get g_pChangeMonitor!");

	const char* folderName = LUA->CheckString(1);
	bool bRecursive = LUA->GetBool(2);

	LUA->PushBool(g_pChangeMonitor->AddFolderToWatch(folderName, bRecursive));
	return 1;
}

LUA_FUNCTION_STATIC(autorefresh_RemoveFolderFromRefresh)
{
	if (!g_pChangeMonitor)
		LUA->ThrowError("Failed to get g_pChangeMonitor!");

	const char* folderName = LUA->CheckString(1);
	bool bRecursive = LUA->GetBool(2);

	LUA->PushBool(g_pChangeMonitor->RemoveFolderFromWatch(folderName, bRecursive));
	return 1;
}

static Symbols::GarrysMod_AutoRefresh_Init func_GarrysMod_AutoRefresh_Init = nullptr;
LUA_FUNCTION_STATIC(autorefresh_RefreshFolders)
{
	if (!func_GarrysMod_AutoRefresh_Init)
		LUA->ThrowError("Failed to load function GarrysMod::AutoRefresh::Init");

	func_GarrysMod_AutoRefresh_Init();
	return 0;
}

LUA_FUNCTION_STATIC(autorefresh_BlockFileForRefresh)
{
	const char* pFilePath = LUA->CheckString(1);
	bool bBlock = LUA->GetBool(2);

	if (bBlock)
	{
		if (pBlacklistedFiles.find(pFilePath) != pBlacklistedFiles.end())
		{
			LUA->PushBool(false);
			return 1;
		}

		pBlacklistedFiles.insert(pFilePath);
	} else {
		auto it = pBlacklistedFiles.find(pFilePath);
		if (it == pBlacklistedFiles.end())
		{
			LUA->PushBool(false);
			return 1;
		}

		pBlacklistedFiles.erase(it);
	}

	LUA->PushBool(true);
	return 1;
}

static Symbols::GarrysMod_AutoRefresh_Cycle func_GarrysMod_AutoRefresh_Cycle = nullptr;
void CAutoRefreshModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	if (!pFileTimePool)
	{
		pFileTimePool = V_CreateThreadPool();
		Util::StartThreadPool(pFileTimePool, autorefresh_threads.GetInt());
	}

	if (!g_pChangeMonitor && func_GarrysMod_AutoRefresh_Cycle)
	{
		bForceHasChangedCall = true;
		func_GarrysMod_AutoRefresh_Cycle();
		bForceHasChangedCall = false;
	}

	Util::StartTable(pLua);
		Util::AddFunc(pLua, autorefresh_AddFileToRefresh, "AddFileToRefresh");
		Util::AddFunc(pLua, autorefresh_RemoveFileFromRefresh, "RemoveFileFromRefresh");
		Util::AddFunc(pLua, autorefresh_AddFolderToRefresh, "AddFolderToRefresh");
		Util::AddFunc(pLua, autorefresh_RemoveFolderFromRefresh, "RemoveFolderFromRefresh");
		Util::AddFunc(pLua, autorefresh_BlockFileForRefresh, "BlockFileForRefresh");
		Util::AddFunc(pLua, autorefresh_RefreshFolders, "RefreshFolders");
	Util::FinishTable(pLua, "autorefresh");
}

void CAutoRefreshModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "autorefresh");
}

void CAutoRefreshModule::Shutdown()
{
	if (pFileTimePool)
	{
		Util::DestroyThreadPool(pFileTimePool);
		pFileTimePool = nullptr;
	}
}

#if SYSTEM_WINDOWS
DETOUR_THISCALL_START()
	DETOUR_THISCALL_ADDFUNC0( hook_Bootil_File_ChangeMonitor_CheckForChanges, CheckForChanges, Bootil::File::ChangeMonitor* );
	DETOUR_THISCALL_ADDRETFUNC0( hook_Bootil_File_ChangeMonitor_HasChanges, bool, HasChanges, Bootil::File::ChangeMonitor* );
DETOUR_THISCALL_FINISH();
#endif

void CAutoRefreshModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::FactoryLoader server_loader("server");
	Detour::Create(
		&detour_GarrysMod_AutoRefresh_HandleChange_Lua, "GarrysMod::AutoRefresh::HandleChange_Lua",
		server_loader.GetModule(), Symbols::GarrysMod_AutoRefresh_HandleChange_LuaSym,
		(void*)hook_GarrysMod_AutoRefresh_HandleChange_Lua, m_pID
	);

	DETOUR_PREPARE_THISCALL();
	Detour::Create(
		&detour_Bootil_File_ChangeMonitor_CheckForChanges, "Bootil::File::ChangeMonitor::CheckForChanges",
		server_loader.GetModule(), Symbols::Bootil_File_ChangeMonitor_CheckForChangesSym,
		(void*)DETOUR_THISCALL(hook_Bootil_File_ChangeMonitor_CheckForChanges, CheckForChanges), m_pID
	);

	Detour::Create(
		&detour_Bootil_File_ChangeMonitor_HasChanges, "Bootil::File::ChangeMonitor::HasChanges",
		server_loader.GetModule(), Symbols::Bootil_File_ChangeMonitor_HasChangesSym,
		(void*)DETOUR_THISCALL(hook_Bootil_File_ChangeMonitor_HasChanges, HasChanges), m_pID
	);

	func_GarrysMod_AutoRefresh_Init = (Symbols::GarrysMod_AutoRefresh_Init)Detour::GetFunction(server_loader.GetModule(), Symbols::GarrysMod_AutoRefresh_InitSym);
	Detour::CheckFunction((void*)func_GarrysMod_AutoRefresh_Init, "GarrysMod::AutoRefresh::Init");

	func_GarrysMod_AutoRefresh_Cycle = (Symbols::GarrysMod_AutoRefresh_Cycle)Detour::GetFunction(server_loader.GetModule(), Symbols::GarrysMod_AutoRefresh_CycleSym);
	Detour::CheckFunction((void*)func_GarrysMod_AutoRefresh_Cycle, "GarrysMod::AutoRefresh::Cycle");
}