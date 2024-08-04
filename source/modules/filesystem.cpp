#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"
#include "filesystem.h"
#include <sourcesdk/filesystem_things.h>
#include <unordered_map>
#include <vprof.h>
#include <algorithm>
#include <cstring>

class CFileSystemModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual void Think(bool bSimulating) OVERRIDE;
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual const char* Name() { return "filesystem"; };
	virtual int Compatibility() { return LINUX32; };
};

static CFileSystemModule g_pFileSystemModule;
IModule* pFileSystemModule = &g_pFileSystemModule;

static ConVar holylib_filesystem_easydircheck("holylib_filesystem_easydircheck", "0", 0, 
	"Checks if the folder CBaseFileSystem::IsDirectory checks has a . in the name after the last /. if so assume it's a file extension.");
static ConVar holylib_filesystem_searchcache("holylib_filesystem_searchcache", "1", 0, 
	"If enabled, it will cache the search path a file was located in and if the same file is requested, it will use that search path directly.");
static ConVar holylib_filesystem_optimizedfixpath("holylib_filesystem_optimizedfixpath", "1", 0, 
	"If enabled, it will optimize CBaseFilesystem::FixUpPath by caching the BASE_PATH search cache.");
static ConVar holylib_filesystem_earlysearchcache("holylib_filesystem_earlysearchcache", "1", 0, 
	"If enabled, it will check early in CBaseFilesystem::OpenForRead if the file is in the search cache.");
static ConVar holylib_filesystem_forcepath("holylib_filesystem_forcepath", "1", 0, 
	"If enabled, it will change the paths of some specific files");
static ConVar holylib_filesystem_predictpath("holylib_filesystem_predictpath", "1", 0, 
	"If enabled, it will try to predict the path of a file");
static ConVar holylib_filesystem_predictexistance("holylib_filesystem_predictexistance", "1", 0, 
	"If enabled, it will try to predict the path of a file, but if the file doesn't exist in the predicted path, we'll just say it doesn't exist.");
static ConVar holylib_filesystem_splitgamepath("holylib_filesystem_splitgamepath", "1", 0, 
	"If enabled, it will create for each content type like models/, materials/ a game path which will be used to find that content.");
static ConVar holylib_filesystem_splitluapath("holylib_filesystem_splitluapath", "0", 0, 
	"If enabled, it will do the same thing holylib_filesystem_splitgamepath does but with lsv. Currently it breaks workshop addons.");
static ConVar holylib_filesystem_splitfallback("holylib_filesystem_splitfallback", "1", 0, 
	"If enabled, it will fallback to the original searchpath if the split path failed.");
static ConVar holylib_filesystem_fixgmodpath("holylib_filesystem_fixgmodpath", "1", 0, 
	"If enabled, it will fix up weird gamemode paths like sandbox/gamemode/sandbox/gamemode which gmod likes to use.");
static ConVar holylib_filesystem_cachefilehandle("holylib_filesystem_cachefilehandle", "0", 0, 
	"If enabled, it will cache the file handle and return it if needed. This will probably cause issues if you open the same file multiple times.");

static ConVar holylib_filesystem_debug("holylib_filesystem_debug", "0", 0, 
	"If enabled, it will show any change to the search cache.");


static const char* nullPath = "NULL_PATH";
extern void DeleteFileHandle(FileHandle_t handle);
static std::unordered_map<FileHandle_t, std::string_view> m_FileStringCache;
static std::unordered_map<std::string_view, FileHandle_t> m_FileCache;
std::unordered_map<FileHandle_t, float> pFileDeletionList;
void AddFileHandleToCache(std::string_view strFilePath, FileHandle_t pHandle)
{
	char* pFilePath = new char[MAX_PATH];
	V_strncpy(pFilePath, strFilePath.data(), MAX_PATH);

	m_FileCache[pFilePath] = pHandle;
	m_FileStringCache[pHandle] = pFilePath;

	if (holylib_filesystem_debug.GetBool())
		Msg("Added file %s to filehandle cache\n", pFilePath);
}

void RemoveFileHandleFromCache(FileHandle_t pHandle)
{
	auto it = m_FileStringCache.find(pHandle);
	if (it == m_FileStringCache.end())
		return;

	m_FileCache.erase(it->second);
	m_FileStringCache.erase(it);

	if (holylib_filesystem_debug.GetBool())
		Msg("Removed file %s from filehandle cache\n", it->second.data());

	delete it->second.data();
}

FileHandle_t GetFileHandleFromCache(std::string_view strFilePath)
{
	auto it = m_FileCache.find(strFilePath);
	if (it == m_FileCache.end())
	{
		if (holylib_filesystem_debug.GetBool())
			Msg("Failed to find %s in filehandle cache\n", strFilePath.data());

		return NULL;
	}

	auto it2 = pFileDeletionList.find(it->second);
	if (it2 != pFileDeletionList.end())
	{
		pFileDeletionList.erase(it2);
		if (holylib_filesystem_debug.GetBool())
			Msg("GetFileHandleFromCache: Removed handle for deletion! (%p)\n", it->second);
	}

	g_pFullFileSystem->Seek(it->second, 0, FILESYSTEM_SEEK_HEAD);

	return it->second;
}

std::string GetFullPath(const CSearchPath* pSearchPath, const char* strFileName)
{
	char szLowercaseFilename[MAX_PATH];
	V_strcpy_safe(szLowercaseFilename, strFileName);
	V_strlower(szLowercaseFilename);

	std::string pPath = pSearchPath->GetPathString();
	pPath.append(szLowercaseFilename);
	return pPath;
}

static Symbols::CBaseFileSystem_FindSearchPathByStoreId func_CBaseFileSystem_FindSearchPathByStoreId;
static std::unordered_map<std::string_view, std::unordered_map<std::string_view, int>> m_SearchCache;
static void AddFileToSearchCache(const char* pFileName, int path, const char* pathID) // pathID should never be deleted so we don't need to manage that memory.
{
	if (!pathID)
		pathID = nullPath;

	if (holylib_filesystem_debug.GetBool())
		Msg("Added file %s to seach cache (%i, %s)\n", pFileName, path, pathID);

	char* cFileName = new char[MAX_PATH];
	V_strncpy(cFileName, pFileName, MAX_PATH);

	m_SearchCache[pathID][cFileName] = path;
}


static void RemoveFileFromSearchCache(const char* pFileName, const char* pathID)
{
	if (!pathID)
		pathID = nullPath;

	if (holylib_filesystem_debug.GetBool())
		Msg("Removed file %s from seach cache! (%s)\n", pFileName, pathID);

	auto& map = m_SearchCache[pathID];
	auto it = map.find(pFileName);
	if (it == map.end())
		return;

	map.erase(it);
	delete[] it->first.data(); // Allocated in AddFileToSearchCache
}

static CSearchPath* GetPathFromSearchCache(const char* pFileName, const char* pathID)
{
	if (!pathID)
		pathID = nullPath;

	auto& map = m_SearchCache[pathID];
	auto it = map.find(pFileName);
	if (it == map.end())
		return NULL;

	if (holylib_filesystem_debug.GetBool())
		Msg("Getting search path for file %s from cache!\n", pFileName);

	if (!func_CBaseFileSystem_FindSearchPathByStoreId)
	{
		Warning("HolyLib: Failed to get CBaseFileSystem::FindSearchPathByStoreId!\n");
		return NULL;
	}

	return func_CBaseFileSystem_FindSearchPathByStoreId(g_pFullFileSystem, it->second);
}

static void NukeSearchCache() // NOTE: We actually never nuke it :D
{
	if (holylib_filesystem_debug.GetBool())
		Msg("Search cache got nuked\n");

	m_SearchCache.clear(); // Now causes a memory leak :<
}

static void DumpSearchcacheCmd(const CCommand &args)
{
	//if (args.ArgC() < 1)
	{
		Msg("---- Search cache ----\n");
		for (auto&[strPath, cache] : m_SearchCache)
		{
			Msg("	\"%s\":\n", strPath.data());
			for (auto&[entry, storeID] : cache)
			{
				Msg("		\"%s\": %i\n", entry.data(), storeID);
			}
		}
		Msg("---- End of Search cache ----\n");
	//} else {
	//	if (V_stricmp(args.Arg(1), "file") == 0)
	//	{
			// ToDo: Dump it into a file and print the filename, like with vprof.
	//	}
	}
}
static ConCommand dumpsearchcache("holylib_filesystem_dumpsearchcache", DumpSearchcacheCmd, "Dumps the searchcache", 0);

static void GetPathFromIDCmd(const CCommand &args)
{
	if ( args.ArgC() < 1 || V_stricmp(args.Arg(1), "") == 0 )
	{
		Msg("Usage: holylib_filesystem_getpathfromid <id>\n");
		return;
	}

	if (!func_CBaseFileSystem_FindSearchPathByStoreId)
	{
		Warning("HolyLib: Failed to get CBaseFileSystem::FindSearchPathByStoreId!\n");
		return;
	}

	CSearchPath* path = func_CBaseFileSystem_FindSearchPathByStoreId(g_pFullFileSystem, atoi(args.Arg(1)));
	if (!path)
	{
		Msg("Failed to find CSearchPath :/\n");
		return;
	}

	Msg("Id: %s\n", args.Arg(1));
	Msg("Path %s\n", path->GetPathString()); // Does this crash? idk.
}
static ConCommand getpathfromid("holylib_filesystem_getpathfromid", GetPathFromIDCmd, "prints the path of the given searchpath id", 0);

static void NukeSearchcacheCmd(const CCommand &args)
{
	NukeSearchCache();
}
static ConCommand nukesearchcache("holylib_filesystem_nukesearchcache", NukeSearchcacheCmd, "Nukes the searchcache", 0);

static void DumpFilecacheCmd(const CCommand &args)
{
	Msg("---- FileHandle cache ----\n");
	for (auto&[strPath, handle] : m_FileCache)
	{
		Msg("	\"%s\": %p\n", strPath.data(), handle);
	}
	Msg("---- End of Search cache ----\n");
}
static ConCommand dumpfilecache("holylib_filesystem_dumpfilecache", DumpFilecacheCmd, "Dumps the filecache", 0);

static Detouring::Hook detour_CBaseFileSystem_FindFileInSearchPath;
static FileHandle_t hook_CBaseFileSystem_FindFileInSearchPath(void* filesystem, CFileOpenInfo &openInfo)
{
	if (!holylib_filesystem_searchcache.GetBool())
		return detour_CBaseFileSystem_FindFileInSearchPath.GetTrampoline<Symbols::CBaseFileSystem_FindFileInSearchPath>()(filesystem, openInfo);

	if (!g_pFullFileSystem)
		g_pFullFileSystem = (IFileSystem*)filesystem;

	VPROF_BUDGET("HolyLib - CBaseFileSystem::FindFile", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	CSearchPath* cachePath = GetPathFromSearchCache(openInfo.m_pFileName, openInfo.m_pSearchPath->GetPathIDString());
	if (cachePath)
	{
		VPROF_BUDGET("HolyLib - CBaseFileSystem::FindFile - Cache", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

		if (holylib_filesystem_cachefilehandle.GetBool())
		{
			FileHandle_t cacheFile = GetFileHandleFromCache(GetFullPath(cachePath, openInfo.m_pFileName));
			if (cacheFile)
				return cacheFile;
		}

		const CSearchPath* origPath = openInfo.m_pSearchPath;
		openInfo.m_pSearchPath = cachePath;
		FileHandle_t file = detour_CBaseFileSystem_FindFileInSearchPath.GetTrampoline<Symbols::CBaseFileSystem_FindFileInSearchPath>()(filesystem, openInfo);
		if (file)
		{
			if (holylib_filesystem_cachefilehandle.GetBool())
				AddFileHandleToCache(GetFullPath(openInfo.m_pSearchPath, openInfo.m_pFileName), file);

			return file;
		}

		openInfo.m_pSearchPath = origPath;
		RemoveFileFromSearchCache(openInfo.m_pFileName, openInfo.m_pSearchPath->GetPathIDString());
	} else {
		if (holylib_filesystem_cachefilehandle.GetBool())
		{
			FileHandle_t cacheFile = GetFileHandleFromCache(GetFullPath(openInfo.m_pSearchPath, openInfo.m_pFileName));
			if (cacheFile)
				return cacheFile;
		}

		if (holylib_filesystem_debug.GetBool())
			Msg("FindFileInSearchPath: Failed to find cachePath! (%s)\n", openInfo.m_pFileName);
	}

	FileHandle_t file = detour_CBaseFileSystem_FindFileInSearchPath.GetTrampoline<Symbols::CBaseFileSystem_FindFileInSearchPath>()(filesystem, openInfo);

	if (file)
	{
		AddFileToSearchCache(openInfo.m_pFileName, openInfo.m_pSearchPath->m_storeId, openInfo.m_pSearchPath->GetPathIDString());
		if (holylib_filesystem_cachefilehandle.GetBool())
			AddFileHandleToCache(GetFullPath(openInfo.m_pSearchPath, openInfo.m_pFileName), file);
	}

	return file;
}

static Detouring::Hook detour_CBaseFileSystem_FastFileTime;
static long hook_CBaseFileSystem_FastFileTime(void* filesystem, const CSearchPath* path, const char* pFileName)
{
	if (!holylib_filesystem_searchcache.GetBool())
		return detour_CBaseFileSystem_FastFileTime.GetTrampoline<Symbols::CBaseFileSystem_FastFileTime>()(filesystem, path, pFileName);

	if (!g_pFullFileSystem)
		g_pFullFileSystem = (IFileSystem*)filesystem;

	VPROF_BUDGET("HolyLib - CBaseFileSystem::FastFileTime", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	CSearchPath* cachePath = GetPathFromSearchCache(pFileName, path->GetPathIDString());
	if (cachePath)
	{
		VPROF_BUDGET("HolyLib - CBaseFileSystem::FastFileTime - Cache", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

		long time = detour_CBaseFileSystem_FastFileTime.GetTrampoline<Symbols::CBaseFileSystem_FastFileTime>()(filesystem, cachePath, pFileName);
		if (time != 0L)
			return time;

		RemoveFileFromSearchCache(pFileName, path->GetPathIDString());
	} else {
		if (holylib_filesystem_debug.GetBool())
			Msg("FastFileTime: Failed to find cachePath! (%s)\n", pFileName);
	}

	long time = detour_CBaseFileSystem_FastFileTime.GetTrampoline<Symbols::CBaseFileSystem_FastFileTime>()(filesystem, path, pFileName);

	if (time != 0L)
		AddFileToSearchCache(pFileName, path->m_storeId, path->GetPathIDString());

	return time;
}

static bool is_file(const char *path) {
	const char *last_slash = strrchr(path, '/');
	const char *last_dot = strrchr(path, '.');

	return last_dot != NULL && (last_slash == NULL || last_dot > last_slash);
}

static Detouring::Hook detour_CBaseFileSystem_IsDirectory;
static bool hook_CBaseFileSystem_IsDirectory(void* filesystem, const char* pFileName, const char* pPathID)
{
	if (holylib_filesystem_easydircheck.GetBool() && is_file(pFileName))
		return false;

	return detour_CBaseFileSystem_IsDirectory.GetTrampoline<Symbols::CBaseFileSystem_IsDirectory>()(filesystem, pFileName, pPathID);
}

static int pBaseLength = 0;
static char pBaseDir[MAX_PATH];
static Detouring::Hook detour_CBaseFileSystem_FixUpPath;
static bool hook_CBaseFileSystem_FixUpPath(IFileSystem* filesystem, const char *pFileName, char *pFixedUpFileName, int sizeFixedUpFileName)
{
	if (!holylib_filesystem_optimizedfixpath.GetBool())
		return detour_CBaseFileSystem_FixUpPath.GetTrampoline<Symbols::CBaseFileSystem_FixUpPath>()(filesystem, pFileName, pFixedUpFileName, sizeFixedUpFileName);

	VPROF_BUDGET("HolyLib - CBaseFileSystem::FixUpPath", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	if (!g_pFullFileSystem)
		g_pFullFileSystem = (IFileSystem*)filesystem;

	V_strncpy( pFixedUpFileName, pFileName, sizeFixedUpFileName );
	V_FixSlashes( pFixedUpFileName, CORRECT_PATH_SEPARATOR );
//	V_RemoveDotSlashes( pFixedUpFileName, CORRECT_PATH_SEPARATOR, true );
	V_FixDoubleSlashes( pFixedUpFileName );

	if ( !V_IsAbsolutePath( pFixedUpFileName ) )
	{
		V_strlower( pFixedUpFileName );
	}
	else 
	{
		// What changed here?
		// Gmod calls GetSearchPath every time FixUpPath is called.
		// Now, we only call it once and then reuse the value, since the BASE_PATH shouldn't change at runtime.

		if ( pBaseLength < 3 )
			pBaseLength = filesystem->GetSearchPath( "BASE_PATH", true, pBaseDir, sizeof( pBaseDir ) );

		if ( pBaseLength )
		{
			if ( *pBaseDir && (pBaseLength+1 < V_strlen( pFixedUpFileName ) ) && (0 != V_strncmp( pBaseDir, pFixedUpFileName, pBaseLength ) )  )
			{
				V_strlower( &pFixedUpFileName[pBaseLength-1] );
			}
		}
		
	}

	return true;
}

static std::string_view nukeFileExtension(const std::string_view& fileName) {
	size_t lastDotPos = fileName.find_last_of('.');
	if (lastDotPos == std::string::npos) 
		return fileName;

	return fileName.substr(0, lastDotPos);
}

static std::string_view getFileExtension(const std::string_view& fileName) {
	size_t lastDotPos = fileName.find_last_of('.');
	if (lastDotPos == std::string::npos || lastDotPos == fileName.length() - 1)
		return "";

	return fileName.substr(lastDotPos + 1);
}

static const char* GetOverridePath(const char* pFileName, const char* pathID)
{
	if (!holylib_filesystem_splitgamepath.GetBool())
		return NULL;

	std::string_view strFileName = pFileName;
	if (strFileName.rfind("materials/") == 0)
		return "CONTENT_MATERIALS";

	if (strFileName.rfind("models/") == 0)
		return "CONTENT_MODELS";

	if (strFileName.rfind("sound/") == 0)
		return "CONTENT_SOUNDS";

	if (strFileName.rfind("maps/") == 0)
		return "CONTENT_MAPS";

	if (strFileName.rfind("resource/") == 0)
		return "CONTENT_RESOURCE";

	if (strFileName.rfind("scripts/") == 0)
		return "CONTENT_SCRIPTS";

	if (strFileName.rfind("cfg/") == 0)
		return "CONTENT_CONFIGS";

	if (strFileName.rfind("gamemodes/") == 0)
		return "LUA_GAMEMODES";

	if (strFileName.rfind("lua/includes/") == 0)
		return "LUA_INCLUDES";

	if (pathID && (V_stricmp(pathID, "lsv") == 0 || V_stricmp(pathID, "GAME") == 0) && holylib_filesystem_splitluapath.GetBool())
	{
		if (strFileName.rfind("sandbox/") == 0)
			return "LUA_GAMEMODE_SANDBOX";

		if (strFileName.rfind("effects/") == 0)
			return "LUA_EFFECTS";

		if (strFileName.rfind("entities/") == 0)
			return "LUA_ENTITIES";

		if (strFileName.rfind("weapons/") == 0)
			return "LUA_WEAPONS";

		if (strFileName.rfind("lua/derma/") == 0)
			return "LUA_DERMA";

		if (strFileName.rfind("lua/drive/") == 0)
			return "LUA_DRIVE";

		if (strFileName.rfind("lua/entities/") == 0)
			return "LUA_LUA_ENTITIES";

		if (strFileName.rfind("vgui/") == 0)
			return "LUA_VGUI";

		if (strFileName.rfind("postprocess/") == 0)
			return "LUA_POSTPROCESS";

		if (strFileName.rfind("matproxy/") == 0)
			return "LUA_MATPROXY";

		if (strFileName.rfind("autorun/") == 0)
			return "LUA_AUTORUN";
	}

	return NULL;
}

static std::unordered_map<std::string_view, std::string_view> g_pOverridePaths;
void AddOveridePath(const char* pFileName, const char* pPathID)
{
	char* cFileName = new char[MAX_PATH];
	V_strncpy(cFileName, pFileName, MAX_PATH);

	char* cPathID = new char[MAX_PATH];
	V_strncpy(cPathID, pPathID, MAX_PATH);

	g_pOverridePaths[cFileName] = cPathID;
}

static Detouring::Hook detour_CBaseFileSystem_OpenForRead;
static FileHandle_t hook_CBaseFileSystem_OpenForRead(CBaseFileSystem* filesystem, const char *pFileNameT, const char *pOptions, unsigned flags, const char *pathID, char **ppszResolvedFilename)
{
	VPROF_BUDGET("HolyLib - CBaseFileSystem::OpenForRead", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	char pFileNameBuff[MAX_PATH];
	const char *pFileName = pFileNameBuff;

	hook_CBaseFileSystem_FixUpPath(filesystem, pFileNameT, pFileNameBuff, sizeof(pFileNameBuff));

	bool splitPath = false;
	const char* origPath = pathID;
	const char* newPath = GetOverridePath(pFileName, pathID);
	if (newPath)
	{
		if (holylib_filesystem_debug.GetBool())
			Msg("OpenForRead: Found split path! switching (%s, %s)\n", pathID, newPath);

		pathID = newPath;
		splitPath = true;
	}

	if (holylib_filesystem_forcepath.GetBool())
	{
		newPath = NULL;
		std::string_view strFileName = pFileName;

		auto it = g_pOverridePaths.find(strFileName);
		if (it != g_pOverridePaths.end())
			newPath = it->second.data();

		if (newPath)
		{
			FileHandle_t handle = detour_CBaseFileSystem_OpenForRead.GetTrampoline<Symbols::CBaseFileSystem_OpenForRead>()(filesystem, pFileNameT, pOptions, flags, pathID, ppszResolvedFilename);
			if (handle) {
				if (holylib_filesystem_debug.GetBool())
					Msg("OpenForRead: Found file in forced path! (%s, %s, %s)\n", pFileNameT, pathID, newPath);

				return handle;
			} else {
				if (holylib_filesystem_debug.GetBool())
					Msg("OpenForRead: Failed to find file in forced path! (%s, %s, %s)\n", pFileNameT, pathID, newPath);
			}
		} else {
			if (holylib_filesystem_debug.GetBool())
				Msg("OpenForRead: File is not in overridePaths (%s, %s)\n", pFileNameT, pathID);
		}
	}

	if (holylib_filesystem_predictpath.GetBool() || holylib_filesystem_predictexistance.GetBool())
	{
		CSearchPath* path = NULL;
		std::string_view strFileName = pFileNameT;
		std::string_view extension = getFileExtension(strFileName);

		bool isModel = false;
		if (extension == "vvd" || extension == "vtx" || extension == "phy" || extension == "ani")
			isModel = true;

		if (isModel)
		{
			std::string mdlPath = nukeFileExtension(strFileName).data(); // Since we modify the string, I don't think we need to switch to std::string_view
			if (extension == "vtx")
				mdlPath = nukeFileExtension(mdlPath); // "dx90.vtx" -> "dx90" -> ""

			path = GetPathFromSearchCache((mdlPath + ".mdl").c_str(), pathID);
		}

		if (path)
		{
			CFileOpenInfo openInfo( filesystem, pFileName, NULL, pOptions, flags, ppszResolvedFilename );
			openInfo.m_pSearchPath = path;
			FileHandle_t file = hook_CBaseFileSystem_FindFileInSearchPath(filesystem, openInfo);
			if (file) {
				if (holylib_filesystem_debug.GetBool())
					Msg("OpenForRead: Found file in predicted path! (%s, %s)\n", pFileNameT, pathID);

				if (holylib_filesystem_cachefilehandle.GetBool())
					AddFileHandleToCache(GetFullPath(openInfo.m_pSearchPath, openInfo.m_pFileName), file);

				return file;
			} else {
				if (holylib_filesystem_debug.GetBool())
					Msg("OpenForRead: Failed to predict file path! (%s, %s)\n", pFileNameT, pathID);

				if (holylib_filesystem_predictexistance.GetBool())
				{
					if (holylib_filesystem_debug.GetBool())
						Msg("OpenForRead: predicted path failed. Let's say it doesn't exist.\n");

					return NULL;
				}
			}
		} else {
			if (holylib_filesystem_debug.GetBool())
				Msg("OpenForRead: Not predicting it! (%s, %s, %s)\n", pFileNameT, pathID, extension.data());
		}
	}

	if (!holylib_filesystem_earlysearchcache.GetBool())
		return detour_CBaseFileSystem_OpenForRead.GetTrampoline<Symbols::CBaseFileSystem_OpenForRead>()(filesystem, pFileNameT, pOptions, flags, pathID, ppszResolvedFilename);

	if ( V_IsAbsolutePath( pFileName ) )
		return detour_CBaseFileSystem_OpenForRead.GetTrampoline<Symbols::CBaseFileSystem_OpenForRead>()(filesystem, pFileNameT, pOptions, flags, pathID, ppszResolvedFilename);

	// So we now got the issue that CBaseFileSystem::CSearchPathsIterator::CSearchPathsIterator is way too slow.
	// so we need to skip it. We do this by checking if we got the file in the search cache before we call the OpenForRead function.
	CSearchPath* cachePath = GetPathFromSearchCache(pFileName, pathID);
	if (cachePath)
	{
		VPROF_BUDGET("HolyLib - SearchCache::OpenForRead", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

		if (holylib_filesystem_cachefilehandle.GetBool())
		{
			FileHandle_t cacheFile = GetFileHandleFromCache(GetFullPath(cachePath, pFileName));
			if (cacheFile)
				return cacheFile;
		}

		CFileOpenInfo openInfo( filesystem, pFileName, NULL, pOptions, flags, ppszResolvedFilename );
		openInfo.m_pSearchPath = cachePath;
		FileHandle_t file = detour_CBaseFileSystem_FindFileInSearchPath.GetTrampoline<Symbols::CBaseFileSystem_FindFileInSearchPath>()(filesystem, openInfo);
		if (file)
		{
			if (holylib_filesystem_cachefilehandle.GetBool())
				AddFileHandleToCache(GetFullPath(openInfo.m_pSearchPath, openInfo.m_pFileName), file);

			return file;
		}

		//RemoveFileFromSearchCache(pFileNameT);
	} else {
		if (holylib_filesystem_debug.GetBool())
			Msg("OpenForRead: Failed to find cachePath! (%s)\n", pFileName);
	}
	
	if (splitPath && holylib_filesystem_splitfallback.GetBool())
	{
		FileHandle_t file = detour_CBaseFileSystem_OpenForRead.GetTrampoline<Symbols::CBaseFileSystem_OpenForRead>()(filesystem, pFileNameT, pOptions, flags, pathID, ppszResolvedFilename);
		if (file)
			return file;

		// ToDo: Find out why map content isn't found properly.
		if (holylib_filesystem_debug.GetBool())
			Msg("OpenForRead: Failed to find file in splitPath! Failling back to original. This is slow! (%s)\n", pFileName);

		pathID = origPath;
	}

	return detour_CBaseFileSystem_OpenForRead.GetTrampoline<Symbols::CBaseFileSystem_OpenForRead>()(filesystem, pFileNameT, pOptions, flags, pathID, ppszResolvedFilename);
}

/*
 * GMOD first calls GetFileTime and then OpenForRead, so we need to make changes for lua in GetFileTime.
 */

#if 0
static std::string replaceString(std::string str, const std::string_view& from, const std::string_view& to)
{
	size_t startPos = str.find(from);
	if (startPos != std::string::npos)
		str.replace(startPos, from.length(), to);

	return str;
}
#endif

namespace IGamemodeSystem
{
	struct UpdatedInformation
	{
		bool exists;
		bool menusystem;
#ifndef WIN32
		const char* title;
		const char* name;
		const char* maps;
		const char* basename;
		const char* category;
#else
		std::string title;
		std::string name;
		std::string maps;
		std::string basename;
		std::string category;
#endif
		uint64_t workshopid;
	};
}

/*
 * GMOD Likes to use paths like "sandbox/gamemode/spawnmenu/sandbox/gamemode/spawnmenu/".
 * This wastes performance, so we fix them up to be "sandbox/gamemode/spawnmenu/"
 */
static std::string_view fixGamemodePath(IFileSystem* filesystem, std::string_view path)
{
	std::string_view activeGamemode = ((const IGamemodeSystem::UpdatedInformation&)filesystem->Gamemodes()->Active()).name;
	if (activeGamemode.empty())
		return path;

	if (path.rfind("gamemodes/") == 0)
		return path;

	std::string searchStr = "/";
	searchStr.append(activeGamemode);
	searchStr.append("/gamemode/"); // Final string should be /[Active Gamemode]/gamemode/
	size_t pos = path.find(searchStr);
	if (pos == std::string::npos)
		return path;

	if (holylib_filesystem_debug.GetBool())
		Msg("fixGamemodePath: Fixed up path. (%s -> %s)\n", path.data(), path.substr(pos + 1).data());

	return path.substr(pos + 1);
}

static Detouring::Hook detour_CBaseFileSystem_GetFileTime;
static long hook_CBaseFileSystem_GetFileTime(IFileSystem* filesystem, const char *pFileName, const char *pPathID)
{
	VPROF_BUDGET("HolyLib - CBaseFileSystem::GetFileTime", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);
	
	const char* origPath = pPathID;
	const char* newPath = GetOverridePath(pFileName, pPathID);
	if (newPath)
	{
		if (holylib_filesystem_debug.GetBool())
			Msg("GetFileTime: Found split path! switching (%s, %s)\n", pPathID, newPath);

		pPathID = newPath;
	}

	std::string_view strFileName = pFileName; // Workaround for now.
	if (origPath && V_stricmp(origPath, "lsv") == 0 && holylib_filesystem_fixgmodpath.GetBool()) // Some weird things happen in the lsv path.  
	{
		strFileName = fixGamemodePath(filesystem, strFileName);
		//strFileName = replaceString(strFileName, "includes/includes/", "includes/"); // What causes this?
	}
	pFileName = strFileName.data();

	if (holylib_filesystem_forcepath.GetBool())
	{
		newPath = NULL;

		if (!newPath && strFileName.rfind("gamemodes/base") == 0)
			newPath = "MOD_WRITE";

		if (!newPath && strFileName.rfind("gamemodes/sandbox") == 0)
			newPath = "MOD_WRITE";

		if (!newPath && strFileName.rfind("gamemodes/terrortown") == 0)
			newPath = "MOD_WRITE";

		if (newPath)
		{
			long time = detour_CBaseFileSystem_GetFileTime.GetTrampoline<Symbols::CBaseFileSystem_GetFileTime>()(filesystem, pFileName, pPathID);
			if (time != 0L) {
				if (holylib_filesystem_debug.GetBool())
					Msg("GetFileTime: Found file in forced path! (%s, %s, %s)\n", pFileName, pPathID, newPath);
				return time;
			} else
				if (holylib_filesystem_debug.GetBool())
					Msg("GetFileTime: Failed to find file in forced path! (%s, %s, %s)\n", pFileName, pPathID, newPath);
		} else {
			if (holylib_filesystem_debug.GetBool())
				Msg("GetFileTime: File is not in overridePaths (%s, %s)\n", pFileName, pPathID);
		}
	}

	return detour_CBaseFileSystem_GetFileTime.GetTrampoline<Symbols::CBaseFileSystem_GetFileTime>()(filesystem, pFileName, pPathID);
}

static bool gBlockRemoveAllMapPaths = false;
static Detouring::Hook detour_CBaseFileSystem_RemoveAllMapSearchPaths;
static void hook_CBaseFileSystem_RemoveAllMapSearchPaths(IFileSystem* filesystem)
{
	if (gBlockRemoveAllMapPaths)
		return;

	detour_CBaseFileSystem_RemoveAllMapSearchPaths.GetTrampoline<Symbols::CBaseFileSystem_RemoveAllMapSearchPaths>()(filesystem);
}

static std::string_view getVPKFile(const std::string_view& fileName) {
	size_t lastThingyPos = fileName.find_last_of('/');
	size_t lastDotPos = fileName.find_last_of('.');

	return fileName.substr(lastThingyPos + 1, lastDotPos - lastThingyPos - 1);
}

static Detouring::Hook detour_CBaseFileSystem_AddSearchPath;
static void hook_CBaseFileSystem_AddSearchPath(IFileSystem* filesystem, const char *pPath, const char *pathID, SearchPathAdd_t addType)
{
	VPROF_BUDGET("HolyLib - CBaseFileSystem::AddSearchPath", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, pathID, addType);

	std::string_view extension = getFileExtension(pPath);
	/*if (extension == "bsp") {
		const char* pPathID = "__TEMP_MAP_PATH";
		gBlockRemoveAllMapPaths = true;
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, pPathID, addType);

		if (filesystem->IsDirectory("materials/", pPathID))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_MATERIALS", addType);

		if (filesystem->IsDirectory("models/", pPathID))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_MODELS", addType);
	
		if (filesystem->IsDirectory("sound/", pPathID))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_SOUNDS", addType);
	
		if (filesystem->IsDirectory("maps/", pPathID))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_MAPS", addType);
	
		if (filesystem->IsDirectory("resource/", pPathID))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_RESOURCE", addType);

		if (filesystem->IsDirectory("scripts/", pPathID))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_SCRIPTS", addType);

		if (filesystem->IsDirectory("cfg/", pPathID))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_CONFIGS", addType);
		
		gBlockRemoveAllMapPaths = false;
		filesystem->RemoveSearchPath(pPath, pPathID);
	}*/

	std::string strPath = pPath;
	if (V_stricmp(pathID, "GAME") == 0)
	{
		if (filesystem->IsDirectory((strPath + "/materials").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_MATERIALS", addType);

		if (filesystem->IsDirectory((strPath + "/models").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_MODELS", addType);
	
		if (filesystem->IsDirectory((strPath + "/sound").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_SOUNDS", addType);
	
		if (filesystem->IsDirectory((strPath + "/maps").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_MAPS", addType);
	
		if (filesystem->IsDirectory((strPath + "/resource").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_RESOURCE", addType);

		if (filesystem->IsDirectory((strPath + "/scripts").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_SCRIPTS", addType);

		if (filesystem->IsDirectory((strPath + "/cfg").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_CONFIGS", addType);

		if (filesystem->IsDirectory((strPath + "/gamemodes").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_GAMEMODES", addType);

		if (filesystem->IsDirectory((strPath + "/lua/includes").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_INCLUDES", addType);
	}
	
	if(V_stricmp(pathID, "lsv") == 0 || V_stricmp(pathID, "GAME") == 0)
	{
		if (filesystem->IsDirectory((strPath + "/sandbox").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_GAMEMODE_SANDBOX", addType);

		if (filesystem->IsDirectory((strPath + "/effects").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_EFFECTS", addType);
	
		if (filesystem->IsDirectory((strPath + "/entities").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_ENTITIES", addType);

		if (filesystem->IsDirectory((strPath + "/weapons").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_WEAPONS", addType);

		if (filesystem->IsDirectory((strPath + "/lua/derma").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_DERMA", addType);

		if (filesystem->IsDirectory((strPath + "/lua/drive").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_DRIVE", addType);

		if (filesystem->IsDirectory((strPath + "/lua/entities").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_LUA_ENTITIES", addType);

		if (filesystem->IsDirectory((strPath + "/vgui").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_VGUI", addType);

		if (filesystem->IsDirectory((strPath + "/postprocess").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_POSTPROCESS", addType);

		if (filesystem->IsDirectory((strPath + "/matproxy").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_MATPROXY", addType);

		if (filesystem->IsDirectory((strPath + "/autorun").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_AUTORUN", addType);
	}

	if (holylib_filesystem_debug.GetBool())
		Msg("Added Searchpath: %s %s %i\n", pPath, pathID, (int)addType);
}

static Detouring::Hook detour_CBaseFileSystem_AddVPKFile;
static void hook_CBaseFileSystem_AddVPKFile(IFileSystem* filesystem, const char *pPath, const char *pathID, SearchPathAdd_t addType)
{
	VPROF_BUDGET("HolyLib - CBaseFileSystem::AddVPKFile", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, pathID, addType);

	if (V_stricmp(pathID, "GAME") == 0)
	{
		std::string_view vpkPath = getVPKFile(pPath);
		detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, vpkPath.data(), addType);

		if (filesystem->IsDirectory("materials/"), vpkPath.data())
			detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, "CONTENT_MATERIALS", addType);

		if (filesystem->IsDirectory("models/"), vpkPath.data())
			detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, "CONTENT_MODELS", addType);
	
		if (filesystem->IsDirectory("sound/"), vpkPath.data())
			detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, "CONTENT_SOUNDS", addType);
	
		if (filesystem->IsDirectory("maps/"), vpkPath.data())
			detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, "CONTENT_MAPS", addType);
	
		if (filesystem->IsDirectory("resource/"), vpkPath.data())
			detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, "CONTENT_RESOURCE", addType);

		if (filesystem->IsDirectory("scripts/"), vpkPath.data())
			detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, "CONTENT_SCRIPTS", addType);

		if (filesystem->IsDirectory("cfg/"), vpkPath.data())
			detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, "CONTENT_CONFIGS", addType);

		if (filesystem->IsDirectory("gamemodes/"), vpkPath.data())
			detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, "LUA_GAMEMODES", addType);

		if (filesystem->IsDirectory("lua/includes/"), vpkPath.data())
			detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, "LUA_INCLUDES", addType);
	
		filesystem->RemoveSearchPath(pPath, vpkPath.data());
	}

	if (holylib_filesystem_debug.GetBool())
		Msg("Added vpk: %s %s %i\n", pPath, pathID, (int)addType);
}

#define FILE_HANDLE_DELETION_DELAY 5 // 5 sec
static Detouring::Hook detour_CBaseFileSystem_Close;
void DeleteFileHandle(FileHandle_t handle)
{
	detour_CBaseFileSystem_Close.GetTrampoline<Symbols::CBaseFileSystem_Close>()(g_pFullFileSystem, handle);
}

extern CGlobalVars* gpGlobals;
static void hook_CBaseFileSystem_Close(IFileSystem* filesystem, FileHandle_t file)
{
	VPROF_BUDGET("HolyLib - CBaseFileSystem::Close", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	if (holylib_filesystem_cachefilehandle.GetBool())
	{
		auto it = pFileDeletionList.find(file);
		if (it == pFileDeletionList.end()) // File is being used again.
			pFileDeletionList[file] = gpGlobals->curtime + FILE_HANDLE_DELETION_DELAY;
		else
			it->second = FILE_HANDLE_DELETION_DELAY;

		if (holylib_filesystem_debug.GetBool())
			Msg("CBaseFileSystem::Close: Marked handle for deletion! (%p)\n", file);

		return;
	}

	detour_CBaseFileSystem_Close.GetTrampoline<Symbols::CBaseFileSystem_Close>()(filesystem, file);
}

extern void FileAsyncReadThink();
void CFileSystemModule::Think(bool bSimulating)
{
	FileAsyncReadThink();

	if (!holylib_filesystem_cachefilehandle.GetBool())
		return;

	std::vector<FileHandle_t> pDeletionList;
	for (auto& [file, time] : pFileDeletionList)
	{
		if (gpGlobals->curtime > time)
		{
			pDeletionList.push_back(file);
			if (holylib_filesystem_debug.GetBool())
				Msg("FileThread: Preparing filehandle for deletion! (%p, %f)\n", file, time);
		}
	}

	if (pDeletionList.size() > 0)
	{
		for (FileHandle_t handle : pDeletionList)
		{
			auto it = pFileDeletionList.find(handle);
			if (it == pFileDeletionList.end()) // File is being used again.
				continue;

			pFileDeletionList.erase(it);

			RemoveFileHandleFromCache(handle); // Remove & Free the memory of the string.
		}

		for (FileHandle_t handle : pDeletionList) // We delete them outside the mutex to not block the main thread.
		{
			if (holylib_filesystem_debug.GetBool())
				Msg("FileThread: Deleted handle! (%p)\n", handle);

			DeleteFileHandle(handle);
		}
		pDeletionList.clear();
	}
}

void CFileSystemModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	// We use MOD_WRITE because it doesn't have additional junk search paths.
	AddOveridePath("cfg/server.cfg", "MOD_WRITE");
	AddOveridePath("cfg/banned_ip.cfg", "MOD_WRITE");
	AddOveridePath("cfg/banned_user.cfg", "MOD_WRITE");
	AddOveridePath("cfg/skill2.cfg", "MOD_WRITE");
	AddOveridePath("cfg/game.cfg", "MOD_WRITE");
	AddOveridePath("cfg/trusted_keys_base.txt", "MOD_WRITE");
	AddOveridePath("cfg/pure_server_minimal.txt", "MOD_WRITE");
	AddOveridePath("cfg/skill_manifest.cfg", "MOD_WRITE");
	AddOveridePath("cfg/skill.cfg", "MOD_WRITE");
	AddOveridePath("cfg/mapcycle.txt", "MOD_WRITE");

	AddOveridePath("stale.txt", "MOD_WRITE");
	AddOveridePath("garrysmod.ver", "MOD_WRITE");
	AddOveridePath("scripts/actbusy.txt", "MOD_WRITE");
	AddOveridePath("modelsounds.cache", "MOD_WRITE");
	AddOveridePath("lua/send.txt", "MOD_WRITE");

	AddOveridePath("resource/serverevents.res", "MOD_WRITE");
	AddOveridePath("resource/gameevents.res", "MOD_WRITE");
	AddOveridePath("resource/modevents.res", "MOD_WRITE");
	AddOveridePath("resource/hltvevents.res", "MOD_WRITE");

	//AddOveridePath("scripts/voicecommands.txt", "MOD_WRITE");
	//AddOveridePath("scripts/propdata.txt", "MOD_WRITE");
	//AddOveridePath("scripts/propdata/base.txt", "MOD_WRITE");
	//AddOveridePath("scripts/propdata/cs.txt", "MOD_WRITE");
	//AddOveridePath("scripts/propdata/l4d.txt", "MOD_WRITE");
	//AddOveridePath("scripts/propdata/phx.txt", "MOD_WRITE");

	if ( pBaseLength < 3 )
		pBaseLength = g_pFullFileSystem->GetSearchPath( "BASE_PATH", true, pBaseDir, sizeof( pBaseDir ) );

	std::string workshopDir = pBaseDir;
		workshopDir = workshopDir + "garrysmod/workshop";

	if (!detour_CBaseFileSystem_AddSearchPath.IsValid())
	{
		Msg("holylib: CBaseFileSystem::AddSearchPath detour is invalid?\n");
		return;
	}

	// NOTE: Check the thing below again
	if (g_pFullFileSystem->IsDirectory("materials/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_MATERIALS", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("models/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_MODELS", SearchPathAdd_t::PATH_ADD_TO_TAIL);
	
	if (g_pFullFileSystem->IsDirectory("sound/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_SOUNDS", SearchPathAdd_t::PATH_ADD_TO_TAIL);
	
	if (g_pFullFileSystem->IsDirectory("maps/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_MAPS", SearchPathAdd_t::PATH_ADD_TO_TAIL);
	
	if (g_pFullFileSystem->IsDirectory("resource/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_RESOURCE", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("scripts/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_SCRIPTS", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("cfg/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_CONFIGS", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("gamemodes/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_GAMEMODES", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("lua/includes/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_INCLUDES", SearchPathAdd_t::PATH_ADD_TO_TAIL);
	
	if (g_pFullFileSystem->IsDirectory("sandbox/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_GAMEMODE_SANDBOX", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("effects/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_EFFECTS", SearchPathAdd_t::PATH_ADD_TO_TAIL);
	
	if (g_pFullFileSystem->IsDirectory("entities/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_ENTITIES", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("weapons/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_WEAPONS", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("lua/derma/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_DERMA", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("lua/drive/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_DRIVE", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("lua/entities/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_LUA_ENTITIES", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("vgui/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_VGUI", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("postprocess/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_POSTPROCESS", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("matproxy/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_MATPROXY", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("autorun/", "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_AUTORUN", SearchPathAdd_t::PATH_ADD_TO_TAIL);
	
	if (holylib_filesystem_debug.GetBool())
		Msg("Updated workshop path. (%s)\n", workshopDir.c_str());
}

inline const char* CPathIDInfo::GetPathIDString() const
{
	return m_pDebugPathID; // This should remove the requirement for g_pPathIDTable.
}

inline const char* CSearchPath::GetPathIDString() const
{
	return m_pPathIDInfo->GetPathIDString();
}

static Symbols::CBaseFileSystem_CSearchPath_GetDebugString func_CBaseFileSystem_CSearchPath_GetDebugString;
inline const char* CSearchPath::GetPathString() const
{
	return func_CBaseFileSystem_CSearchPath_GetDebugString((void*)this);
}

void CFileSystemModule::InitDetour(bool bPreServer)
{
	if ( !bPreServer ) { return; }

	SourceSDK::ModuleLoader dedicated_loader("dedicated_srv");
	Detour::Create(
		&detour_CBaseFileSystem_FindFileInSearchPath, "CBaseFileSystem::FindFileInSearchPath",
		dedicated_loader.GetModule(), Symbols::CBaseFileSystem_FindFileInSearchPathSym,
		(void*)hook_CBaseFileSystem_FindFileInSearchPath, m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_IsDirectory, "CBaseFileSystem::IsDirectory",
		dedicated_loader.GetModule(), Symbols::CBaseFileSystem_IsDirectorySym,
		(void*)hook_CBaseFileSystem_IsDirectory, m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_FastFileTime, "CBaseFileSystem::FastFileTime",
		dedicated_loader.GetModule(), Symbols::CBaseFileSystem_FastFileTimeSym,
		(void*)hook_CBaseFileSystem_FastFileTime, m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_FixUpPath, "CBaseFileSystem::FixUpPath",
		dedicated_loader.GetModule(), Symbols::CBaseFileSystem_FixUpPathSym,
		(void*)hook_CBaseFileSystem_FixUpPath, m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_OpenForRead, "CBaseFileSystem::OpenForRead",
		dedicated_loader.GetModule(), Symbols::CBaseFileSystem_OpenForReadSym,
		(void*)hook_CBaseFileSystem_OpenForRead, m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_GetFileTime, "CBaseFileSystem::GetFileTime",
		dedicated_loader.GetModule(), Symbols::CBaseFileSystem_GetFileTimeSym,
		(void*)hook_CBaseFileSystem_GetFileTime, m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_AddSearchPath, "CBaseFileSystem::AddSearchPath",
		dedicated_loader.GetModule(), Symbols::CBaseFileSystem_AddSearchPathSym,
		(void*)hook_CBaseFileSystem_AddSearchPath, m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_AddVPKFile, "CBaseFileSystem::AddVPKFile",
		dedicated_loader.GetModule(), Symbols::CBaseFileSystem_AddVPKFileSym,
		(void*)hook_CBaseFileSystem_AddVPKFile, m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_RemoveAllMapSearchPaths, "CBaseFileSystem::RemoveAllMapSearchPaths",
		dedicated_loader.GetModule(), Symbols::CBaseFileSystem_RemoveAllMapSearchPathsSym,
		(void*)hook_CBaseFileSystem_RemoveAllMapSearchPaths, m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_Close, "CBaseFileSystem::Close",
		dedicated_loader.GetModule(), Symbols::CBaseFileSystem_CloseSym,
		(void*)hook_CBaseFileSystem_Close, m_pID
	);

	func_CBaseFileSystem_FindSearchPathByStoreId = (Symbols::CBaseFileSystem_FindSearchPathByStoreId)Detour::GetFunction(dedicated_loader.GetModule(), Symbols::CBaseFileSystem_FindSearchPathByStoreIdSym);
	Detour::CheckFunction((void*)func_CBaseFileSystem_FindSearchPathByStoreId, "CBaseFileSystem::FindSearchPathByStoreId");

	func_CBaseFileSystem_CSearchPath_GetDebugString = (Symbols::CBaseFileSystem_CSearchPath_GetDebugString)Detour::GetFunction(dedicated_loader.GetModule(), Symbols::CBaseFileSystem_CSearchPath_GetDebugStringSym);
	Detour::CheckFunction((void*)func_CBaseFileSystem_CSearchPath_GetDebugString, "CBaseFileSystem::CSearchPath::GetDebugString");
}

/*
 *
 *	LUA API
 *
 */

struct IAsyncFile
{
	~IAsyncFile()
	{
		if ( content )
			delete[] content;
	}

	FileAsyncRequest_t* req;
	int callback;
	int nBytesRead;
	int status;
	const char* content = NULL;
};

std::vector<IAsyncFile*> asyncCallback;
void AsyncCallback(const FileAsyncRequest_t &request, int nBytesRead, FSAsyncStatus_t err)
{
	IAsyncFile* async = (IAsyncFile*)request.pContext;
	if (async)
	{
		async->nBytesRead = nBytesRead;
		async->status = err;
		char* content = new char[nBytesRead + 1];
		std::memcpy(static_cast<void*>(content), request.pData, nBytesRead);
		content[nBytesRead] = '\0';
		async->content = content;
		asyncCallback.push_back(async);
	} else {
		Msg("[Luathreaded] file.AsyncRead Invalid request? (%s, %s)\n", request.pszFilename, request.pszPathID);
	}
}

LUA_FUNCTION_STATIC(filesystem_AsyncRead)
{
	const char* fileName = LUA->CheckString(1);
	const char* gamePath = LUA->CheckString(2);
	LUA->CheckType(3, GarrysMod::Lua::Type::Function);
	LUA->Push(3);
	int reference = LUA->ReferenceCreate();
	LUA->Pop();
	bool sync = LUA->GetBool(4);

	FileAsyncRequest_t* request = new FileAsyncRequest_t;
	request->pszFilename = fileName;
	request->pszPathID = gamePath;
	request->pfnCallback = AsyncCallback;
	request->flags = sync ? FSASYNC_FLAGS_SYNC : 0;

	IAsyncFile* file = new IAsyncFile;
	file->callback = reference;
	file->req = request;

	request->pContext = file;

	LUA->PushNumber(g_pFullFileSystem->AsyncReadMultiple(request, 1));

	return 1;
}

void FileAsyncReadThink()
{
	std::vector<IAsyncFile*> files;
	for(IAsyncFile* file : asyncCallback) {
		g_Lua->ReferencePush(file->callback);
		g_Lua->PushString(file->req->pszFilename);
		g_Lua->PushString(file->req->pszPathID);
		g_Lua->PushNumber(file->status);
		g_Lua->PushString(file->content);
		g_Lua->CallFunctionProtected(4, 0, true);
		g_Lua->ReferenceFree(file->callback);
		files.push_back(file);
	}

	asyncCallback.clear();
}

LUA_FUNCTION_STATIC(filesystem_CreateDir)
{
	g_pFullFileSystem->CreateDirHierarchy(LUA->CheckString(1), "DATA");

	return 0;
}

LUA_FUNCTION_STATIC(filesystem_Delete)
{
	g_pFullFileSystem->RemoveFile(LUA->CheckString(1), "DATA");

	return 0;
}

LUA_FUNCTION_STATIC(filesystem_Exists)
{
	LUA->PushBool(g_pFullFileSystem->FileExists(LUA->CheckString(1), LUA->CheckString(2)));

	return 1;
}

std::string extractDirectoryPath(const std::string& filepath) {
    size_t lastSlashPos = filepath.find_last_of('/');
    if (lastSlashPos != std::string::npos) {
        return filepath.substr(0, lastSlashPos + 1);
    } else {
        return "";
    }
}

std::vector<std::string> SortByDate(std::vector<std::string> files, const char* filepath, const char* path, bool ascending)
{
	std::string str_filepath = extractDirectoryPath((std::string)filepath);
	std::unordered_map<std::string, long> dates;
	for (std::string file : files) {
		dates[file] = g_pFullFileSystem->GetFileTime((str_filepath + file).c_str(), path);
	}

	std::sort(files.begin(), files.end(), [&dates](const std::string& a, const std::string& b) {
        return dates[a] < dates[b];
    });

	if (!ascending) {
        std::reverse(files.begin(), files.end());
    }

	return files;
}

LUA_FUNCTION_STATIC(filesystem_Find)
{
	std::vector<std::string> files;
	std::vector<std::string> folders;

	const char* filepath = LUA->CheckString(1);
	const char* path = LUA->CheckString(2);
	const char* sorting = LUA->CheckString(3);

	FileFindHandle_t findHandle;
	const char *pFilename = g_pFullFileSystem->FindFirstEx(filepath, path, &findHandle);
	while (pFilename)
	{
		if (g_pFullFileSystem->IsDirectory(((std::string)filepath + pFilename).c_str(), path)) {
			folders.push_back(pFilename);
		} else {
			files.push_back(pFilename);
		}

		pFilename = g_pFullFileSystem->FindNext(findHandle);
	}
	g_pFullFileSystem->FindClose(findHandle);

	if (files.size() > 0) {
		if (strcmp(sorting, "namedesc") == 0) { // sort the files descending by name.
			std::sort(files.begin(), files.end(), std::greater<std::string>());
			std::sort(folders.begin(), folders.end(), std::greater<std::string>());
		} else if (strcmp(sorting, "dateasc") == 0) { // sort the files ascending by date.
			SortByDate(files, filepath, path, true);
			SortByDate(folders, filepath, path, true);
		} else if (strcmp(sorting, "datedesc") == 0) { // sort the files descending by date.
			SortByDate(files, filepath, path, false);
			SortByDate(folders, filepath, path, false);
		} else { // Fallback to default: nameasc | sort the files ascending by name.
			std::sort(files.begin(), files.end());
			std::sort(folders.begin(), folders.end());
		}

		LUA->CreateTable();

		int i = 0;
		for (std::string file : files)
		{
			++i;
			LUA->PushString(file.c_str());
			LUA->SetField(-2, std::to_string(i).c_str());
		}
	} else {
		LUA->PushNil();
	}

	if (folders.size() > 0) {
		LUA->CreateTable();

		int i = 0;
		for (std::string folder : folders)
		{
			++i;
			LUA->PushString(folder.c_str());
			LUA->SetField(-2, std::to_string(i).c_str());
		}
	} else {
		LUA->PushNil();
	}

	return 2;
}

LUA_FUNCTION_STATIC(filesystem_IsDir)
{
	LUA->PushBool(g_pFullFileSystem->IsDirectory(LUA->CheckString(1), LUA->CheckString(2)));

	return 1;
}

LUA_FUNCTION_STATIC(filesystem_Open)
{
	const char* filename = LUA->CheckString(1);
	const char* fileMode = LUA->CheckString(2);
	const char* path = g_Lua->CheckStringOpt(3, "GAME");

	FileHandle_t fh = g_pFullFileSystem->Open(filename, fileMode, path);
	if (fh)
		g_Lua->PushUserType(fh, GarrysMod::Lua::Type::File); // Gmod uses a class Lua::File which it pushes. What does it contain?
	else
		g_Lua->PushNil();

	return 1;
}

LUA_FUNCTION_STATIC(filesystem_Rename)
{
	const char* original = LUA->CheckString(1);
	const char* newname = LUA->CheckString(2);

	LUA->PushBool(g_pFullFileSystem->RenameFile(original, newname, "DATA"));

	return 1;
}

LUA_FUNCTION_STATIC(filesystem_Size)
{
	const char* path = LUA->GetString(2);
	if (path == NULL)
		path = "GAME";

	LUA->PushNumber(g_pFullFileSystem->Size(LUA->CheckString(1), path));

	return 1;
}

LUA_FUNCTION_STATIC(filesystem_Time)
{
	const char* path = LUA->GetString(2);
	if (path == NULL)
		path = "GAME";

	LUA->PushNumber(g_pFullFileSystem->GetFileTime(LUA->CheckString(1), path));

	return 1;
}

// Gmod's filesystem functions have some weird stuff in them that makes them noticeably slower :/
void CFileSystemModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	Util::StartTable();
		Util::AddFunc(filesystem_AsyncRead, "AsyncRead");
		Util::AddFunc(filesystem_CreateDir, "CreateDir");
		Util::AddFunc(filesystem_Delete, "Delete");
		Util::AddFunc(filesystem_Exists, "Exists");
		Util::AddFunc(filesystem_Find, "Find");
		Util::AddFunc(filesystem_IsDir, "IsDir");
		Util::AddFunc(filesystem_Open, "Open");
		Util::AddFunc(filesystem_Rename, "Rename");
		Util::AddFunc(filesystem_Size, "Size");
		Util::AddFunc(filesystem_Time, "Time");
	Util::FinishTable("filesystem");
}

void CFileSystemModule::LuaShutdown()
{
	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		g_Lua->PushNil();
		g_Lua->SetField(-2, "filesystem");
	g_Lua->Pop(1);
}