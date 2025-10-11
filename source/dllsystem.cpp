#include "configsystem.h"
#include "versioninfo.h"
#include "tier0/dbg.h"
#define DLL_TOOLS
#include "detours.h"

/*
	This system serves a simple purpose, any registered DLLs that want to be loaded, will be loaded by us.
	This allows them to go through the same loading order as us, ensuring no issues.
	We DONT do much logic here, we expect the DLLs to register themself as modules using the given IHolyLib interface.

	Example config:

[
	{
		"path": "garrysmod/lua/bin/gmsv_example_linux.dll", // Must be relative to the garrysmod/ directory
		"name": "ExampleDLL", // Name that will be used for debug prints.
		"version": "0.8", // (Optional) a specific version of holylib it might be bound to, the DLL won't be loaded if these don't match.
		"branch": "main", // (Optional) a specific branch of holylib
	},
]
*/

class IHolyLib;
typedef bool (*HolyLib_EntryPoint)(IHolyLib* pHolyLib);
class DLLManager
{
public:
	struct LoadedDLL
	{
		Bootil::BString m_strPath;
		Bootil::BString m_strName;
		Bootil::BString m_strBranch;
		Bootil::BString m_strVersion;
		DLL_Handle m_pHandle = nullptr;
	};

	void LoadDLLs()
	{
		IConfig* pDllConfig = g_pConfigSystem->LoadConfig("garrysmod/holylib/cfg/dlls.json");
		if (!pDllConfig)
			return;

		if (pDllConfig->GetState() == ConfigState::INVALID_JSON)
		{
			Warning(PROJECT_NAME " - core: Failed to load dlls.json!\n- Check if the json is valid or delete the config to let a new one be generated!\n");
			pDllConfig->Destroy(); // config is bad... ewww
			pDllConfig = nullptr;
			return;
		}

		Bootil::Data::Tree& pData = pDllConfig->GetData();
		for (Bootil::Data::Tree& pChild : pData.Children())
		{
			LoadedDLL& pDLL = m_pLoadedDlls.emplace_back();
			pDLL.m_strName = pChild.ChildValue("name", "");
			pDLL.m_strPath = pChild.ChildValue("path", "");
			pDLL.m_strBranch = pChild.ChildValue("branch", HolyLib_GetBranch());
			pDLL.m_strVersion = pChild.ChildValue("version", HolyLib_GetVersion());
			pDLL.m_pHandle = nullptr;

			if (pDLL.m_strPath.length() <= 3 || pDLL.m_strName.length() <= 3)
			{
				Warning(PROJECT_NAME " - DLLManager: not loading \"%s\" since it has no valid path or name!\nCheck your holylib/cfg/dlls.json!", pDLL.m_strName.c_str());
				continue;
			}

			if (pDLL.m_strBranch != HolyLib_GetBranch())
			{
				Warning(PROJECT_NAME " - DLLManager: not loading \"%s\" since it doesn't match our branch!\n", pDLL.m_strName.c_str());
				continue;
			}

			if (pDLL.m_strVersion != HolyLib_GetVersion())
			{
				Warning(PROJECT_NAME " - DLLManager: not loading \"%s\" since it doesn't match our version!\n", pDLL.m_strName.c_str());
				continue;
			}

			pDLL.m_pHandle = DLL_LoadModule(pDLL.m_strPath.c_str(), RTLD_LAZY);
			if (!pDLL.m_pHandle)
			{
				Warning(PROJECT_NAME " - DLLManager: failed to load \"%s\", probably missing a dependency!\n", pDLL.m_strName.c_str());
				continue;
			}

			HolyLib_EntryPoint pEntryPoint = (HolyLib_EntryPoint)DLL_GetAddress(pDLL.m_pHandle, "HolyLib_EntryPoint");
			if (pEntryPoint == NULL)
			{
				Warning(PROJECT_NAME " - DLLManager: failed to load \"%s\", it's missing the entrypoint \"HolyLib_EntryPoint\"!\n", pDLL.m_strName.c_str());
				DLL_UnloadModule(pDLL.m_pHandle);
				pDLL.m_pHandle = nullptr;
				continue;
			}

			extern IHolyLib* g_pHolyLib;
			if (!pEntryPoint(g_pHolyLib))
			{
				Warning(PROJECT_NAME " - DLLManager: failed to load \"%s\", GG!\n", pDLL.m_strName.c_str());
				DLL_UnloadModule(pDLL.m_pHandle);
				pDLL.m_pHandle = nullptr;
				continue;
			}

			Msg(PROJECT_NAME " - DLLManager: Successfully loaded \"%s\"\n", pDLL.m_strName.c_str());
		}
	}

	~DLLManager()
	{
		for (LoadedDLL& pDLL : m_pLoadedDlls)
		{
			if (!pDLL.m_pHandle)
			{
				continue;
			}

			DLL_UnloadModule(pDLL.m_pHandle);
			pDLL.m_pHandle = nullptr;
		}
	}

private:
	std::vector<LoadedDLL> m_pLoadedDlls;
};

static DLLManager g_pDLLManager;

// We cannot just directly load the DLLs inside the constructor since stuff like the config system may have not been constructed yet :sob:
void LoadDLLs() // Called by Util::Load
{
	g_pDLLManager.LoadDLLs();
}