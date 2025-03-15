#include <sourcesdk/filesystem_things.h>
#undef Yield
#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <unordered_set>
#include "edict.h"

class CFileSystemModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual void Think(bool bSimulating) OVERRIDE;
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax) OVERRIDE;
	virtual const char* Name() { return "filesystem"; };
	virtual int Compatibility() { return LINUX32 | WINDOWS32; };
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
static ConVar holylib_filesystem_predictexistance("holylib_filesystem_predictexistance", "0", 0, 
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
static ConVar holylib_filesystem_precachehandle("holylib_filesystem_precachehandle", "1", 0,
	"If enabled, it will try to predict which file it will open next and open the file to keep a handle ready to be opened.");
static ConVar holylib_filesystem_savesearchcache("holylib_filesystem_savesearchcache", "1", 0,
	"If enabled, it will write the search cache into a file and restore it when starting, using it to improve performance.");


// Optimization Idea: When Gmod calls GetFileTime, we could try to get the filehandle in parallel to have it ready when gmod calls it.
// We could also cache every FULL searchpath to not have to look up a file every time.  

static IThreadPool* pFileSystemPool = NULL;

static void OnThreadsChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	ConVar* pConVar = (ConVar*)convar;
	if (pConVar->GetInt() > 0 && !pFileSystemPool)
		pFileSystemPool = V_CreateThreadPool();

	if (pFileSystemPool)
	{
		pFileSystemPool->ExecuteAll();
		pFileSystemPool->Stop();

		if (pConVar->GetInt() <= 0)
		{
			V_DestroyThreadPool(pFileSystemPool);
			pFileSystemPool = NULL;
		} else {
			Util::StartThreadPool(pFileSystemPool, pConVar->GetInt());
		}
	}
}

static ConVar holylib_filesystem_threads("holylib_filesystem_threads", "0", 0,
	"The number of threads the filesystem is allowed to use. Set to 0 to disable it", OnThreadsChange);

struct FilesystemJob
{
	std::string fileName;
	std::string gamePath;
	void* pData = NULL;
};

static const char* nullPath = "NULL_PATH";
extern void DeleteFileHandle(FileHandle_t handle);
static std::unordered_map<FileHandle_t, std::string_view> m_FileStringCache;
static std::unordered_map<std::string_view, FileHandle_t> m_FileCache;
static std::unordered_set<FileHandle_t> m_WriteFileHandle;
static std::unordered_set<std::string> m_PredictionCheck;
std::unordered_map<FileHandle_t, float> pFileDeletionList;
void AddFileHandleToCache(std::string_view strFilePath, FileHandle_t pHandle)
{
	char* pFilePath = new char[MAX_PATH];
	V_strncpy(pFilePath, strFilePath.data(), MAX_PATH);

	m_FileCache[pFilePath] = pHandle;
	m_FileStringCache[pHandle] = pFilePath;

	if (g_pFileSystemModule.InDebug())
		Msg("holylib - AddFileHandleToCache: Added file %s to filehandle cache\n", pFilePath);
}

void RemoveFileHandleFromCache(FileHandle_t pHandle)
{
	auto it = m_FileStringCache.find(pHandle);
	if (it == m_FileStringCache.end())
		return;

	m_FileCache.erase(it->second);
	m_FileStringCache.erase(it);

	if (g_pFileSystemModule.InDebug())
		Msg("holylib - RemoveFileHandleFromCache: Removed file %s from filehandle cache\n", it->second.data());

	delete[] it->second.data();
}

static void ClearFileHandleSearchCache()
{
	for (auto& [key, val] : m_FileCache)
	{
		delete[] key.data();
		// ToDo: Also free handles
	}

	m_FileCache.clear();
	m_FileStringCache.clear();
}

extern CGlobalVars* gpGlobals;
FileHandle_t GetFileHandleFromCache(std::string_view strFilePath)
{
	auto it = m_FileCache.find(strFilePath);
	if (it == m_FileCache.end())
	{
		if (g_pFileSystemModule.InDebug())
			Msg("holylib - GetFileHandleFromCache: Failed to find %s in filehandle cache\n", strFilePath.data());

		return NULL;
	}

	auto it2 = pFileDeletionList.find(it->second);
	if (it2 != pFileDeletionList.end())
	{
		pFileDeletionList.erase(it2);
		if (g_pFileSystemModule.InDebug())
			Msg("holylib - GetFileHandleFromCache: Removed handle for deletion! (%p)\n", it->second);
	} else {
		Msg("holylib - GetFileHandleFromCache: File wasn't marked for deletion? (%p)\n", it->second); // This could mean that were actively using it.
	}

	int iPos = g_pFullFileSystem->Tell(it->second);
	if (iPos != 0)
	{
		if (g_pFileSystemModule.InDebug())
			Msg("holylib - GetFileHandleFromCache: Pos: %llu\n", g_pFullFileSystem->Tell(it->second));
		
		g_pFullFileSystem->Seek(it->second, 0, FILESYSTEM_SEEK_HEAD); // Why doesn't it reset?
		
		if (g_pFileSystemModule.InDebug())
			Msg("holylib - GetFileHandleFromCache: Rewind pos: %llu\n", g_pFullFileSystem->Tell(it->second));
		
		int iNewPos = g_pFullFileSystem->Tell(it->second);
		if (iNewPos != 0)
		{
			if (g_pFileSystemModule.InDebug())
				Msg("holylib - GetFileHandleFromCache: Failed to reset pointer!\n");
			
			pFileDeletionList[it->second] = gpGlobals->curtime; // Force delete. it's broken
			return NULL;
		}
		// BUG: .bsp files seem to have funny behavior :/
		// BUG2: We need to account for rb and wb since wb can't read and rb can't write.  
		// How will we account for that? were gonna need to get the CFileHandle class
	}

	return it->second;
}

static Symbols::CBaseFileSystem_FindSearchPathByStoreId func_CBaseFileSystem_FindSearchPathByStoreId;
inline CSearchPath* FindSearchPathByStoreId(int iStoreID)
{
	if (!func_CBaseFileSystem_FindSearchPathByStoreId)
	{
		Warning("HolyLib: Failed to get CBaseFileSystem::FindSearchPathByStoreId!\n");
		return NULL;
	}

	return func_CBaseFileSystem_FindSearchPathByStoreId(g_pFullFileSystem, iStoreID);
}

std::string GetFullPath(const CSearchPath* pSearchPath, const char* strFileName) // ToDo: Possibly switch to string_view?
{
	char szLowercaseFilename[MAX_PATH];
	V_strcpy_safe(szLowercaseFilename, strFileName);
	V_strlower(szLowercaseFilename);

	std::string pPath = pSearchPath->GetPathString();
	pPath.append(szLowercaseFilename);
	return pPath;
}

static std::unordered_map<std::string_view, std::unordered_map<std::string_view, int>> m_SearchCache;
static void ClearFileSearchCache()
{
	for (auto& [key, valMap] : m_SearchCache)
	{
		for (auto& [val, _] : valMap)
		{
			delete[] val.data();
		}
	}

	m_SearchCache.clear();
}

static void AddFileToSearchCache(const char* pFileName, int path, const char* pathID) // pathID should never be deleted so we don't need to manage that memory.
{
	if (!pathID)
		pathID = nullPath;

	if (!pFileName)
		return;

	if (g_pFileSystemModule.InDebug())
		Msg("holylib - AddFileToSearchCache: Added file %s to seach cache (%i, %s)\n", pFileName, path, pathID);

	char* cFileName = new char[MAX_PATH];
	V_strncpy(cFileName, pFileName, MAX_PATH);

	m_SearchCache[pathID][cFileName] = path;
}


static void RemoveFileFromSearchCache(const char* pFileName, const char* pathID)
{
	if (!pathID)
		pathID = nullPath;

	if (!pFileName)
		return;

	if (g_pFileSystemModule.InDebug())
		Msg("holylib - RemoveFileFromSearchCache: Removed file %s from seach cache! (%s)\n", pFileName, pathID);

	auto mapIt = m_SearchCache.find(pathID);
	if (mapIt == m_SearchCache.end())
		return;
	
	auto& map = mapIt->second;
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

	if (!pFileName)
		return NULL; // ??? can this even happen?

	auto mapIt = m_SearchCache.find(pathID);
	if (mapIt == m_SearchCache.end())
		return NULL;
	
	auto& map = mapIt->second;
	auto it = map.find(pFileName);
	if (it == map.end())
		return NULL; // We should add a debug print to see if we make a mistake somewhere

	if (g_pFileSystemModule.InDebug())
		Msg("holylib - GetPathFromSearchCache: Getting search path for file %s from cache!\n", pFileName);

	return FindSearchPathByStoreId(it->second);
}

static void NukeSearchCache() // NOTE: We actually never nuke it :D
{
	if (g_pFileSystemModule.InDebug())
		Msg("holylib - NukeSearchCache: Search cache got nuked\n");

	m_SearchCache.clear(); // Now causes a memory leak :D (Should I try to solve it? naaaaaa :^)
}

/*struct SearchCacheEntry
{
	unsigned char pathLength;
	char* path;
	unsigned char absolutePathLength;
	char* absolutePath;
};*/

#define SearchCacheVersion 1
#define MaxSearchCacheEntries (1 << 16) // 64k max files
struct SearchCache {
	unsigned int version = SearchCacheVersion;
	unsigned int usedPaths = 0;
	//SearchCacheEntry paths[MaxSearchCacheEntries];
};

static void WriteSearchCache()
{
	VPROF_BUDGET("HolyLib - WriteSearchCache", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);
	if (!holylib_filesystem_savesearchcache.GetBool())
		return;

	FileHandle_t handle = g_pFullFileSystem->Open("holylib_searchcache.dat", "wb", "MOD_WRITE");
	if (handle)
	{
		SearchCache searchCache;
		for (auto& [strPath, cache] : m_SearchCache)
			searchCache.usedPaths += cache.size();

		g_pFullFileSystem->Write(&searchCache, sizeof(SearchCache), handle);

		char absolutePath[MAX_PATH];
		for (auto& [strPath, cache] : m_SearchCache)
		{
			for (auto& [strEntry, storeID] : cache)
			{
				//V_strcpy(entry.pathID, strPath.data());
				unsigned char pathLength = (unsigned char)strEntry.length();
				const char* path = strEntry.data();
				g_pFullFileSystem->Write(&pathLength, sizeof(pathLength), handle);
				g_pFullFileSystem->Write(path, pathLength, handle);

				memset(absolutePath, 0, sizeof(absolutePath));
				g_pFullFileSystem->RelativePathToFullPath(path, strPath.data(), absolutePath, sizeof(absolutePath));

				unsigned char absolutePathLength = (unsigned char)strlen(absolutePath);
				g_pFullFileSystem->Write(&absolutePathLength, sizeof(absolutePathLength), handle);
				g_pFullFileSystem->Write(absolutePath, absolutePathLength, handle);
			}
		}

		g_pFullFileSystem->Close(handle);
		Msg("holylib: successfully wrote searchcache file (%i)\n", searchCache.usedPaths);
	} else {
		Warning("holylib: Failed to open searchcache file!\n");
	}
}

static std::unordered_map<std::string_view, std::string_view> g_pAbsoluteSearchCache;
inline std::string_view* GetStringFromAbsoluteCache(std::string_view fileName)
{
	auto it = g_pAbsoluteSearchCache.find(fileName);
	if (it == g_pAbsoluteSearchCache.end())
		return NULL;

	return &it->second;
}

static void ClearAbsoluteSearchCache()
{
	VPROF_BUDGET("HolyLib - ClearAbsoluteSearchCache", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	for (auto& [key, val] : g_pAbsoluteSearchCache)
	{
		delete[] key.data(); // Free the memory.
		delete[] val.data();
	}

	g_pAbsoluteSearchCache.clear();
}

static void ReadSearchCache()
{
	VPROF_BUDGET("HolyLib - ReadSearchCache", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);
	ClearAbsoluteSearchCache();

	FileHandle_t handle = g_pFullFileSystem->Open("holylib_searchcache.dat", "rb", "MOD_WRITE");
	if (handle)
	{
		SearchCache searchCache;
		g_pFullFileSystem->Read(&searchCache, sizeof(SearchCache), handle);
		if (searchCache.version != SearchCacheVersion)
		{
			Warning("holylib - ReadSearchCache: Searchcache version didnt match  (File: %i, Current %i)\n", searchCache.version, SearchCacheVersion);
			return;
		}

		g_pAbsoluteSearchCache.reserve(searchCache.usedPaths);

		for (unsigned int i = 0; i < searchCache.usedPaths; ++i)
		{
			unsigned char pathLength;
			g_pFullFileSystem->Read(&pathLength, sizeof(pathLength), handle);

			char* path = new char[pathLength + 1];
			g_pFullFileSystem->Read(path, pathLength, handle);
			path[pathLength] = '\0'; // We null terminate it to keep it nice since things like Msg woukd print it with additional junk / random memory.

			unsigned char absolutePathLength;
			g_pFullFileSystem->Read(&absolutePathLength, sizeof(absolutePathLength), handle);

			char* absolutePath = new char[absolutePathLength + 1];
			g_pFullFileSystem->Read(absolutePath, absolutePathLength, handle);
			absolutePath[absolutePathLength] = '\0';

			std::string_view pathStr = path; // NOTE: We have to manually free it later
			std::string_view absolutePathStr = absolutePath; // NOTE: We have to manually free it later
			g_pAbsoluteSearchCache[pathStr] = absolutePathStr;
		}

		g_pFullFileSystem->Close(handle);

		/*for (auto& [key, val] : g_pAbsoluteSearchCache)
		{
			Msg("Key: %s\nValue: %s\n", key.data(), val.data());
		}*/

		//if (g_pFileSystemModule.InDebug())
		Msg("holylib - filesystem: Loaded searchcache file (%i)\n", searchCache.usedPaths);
	}
	else {
		if (g_pFileSystemModule.InDebug())
			Msg("holylib - filesystem: Failed to find searchcache file\n");
	}
}

void CFileSystemModule::ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
	if (pFileSystemPool)
		pFileSystemPool->QueueCall(WriteSearchCache);
	else
		WriteSearchCache();
}

static void DumpSearchcacheCmd(const CCommand &args)
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
}
static ConCommand dumpsearchcache("holylib_filesystem_dumpsearchcache", DumpSearchcacheCmd, "Dumps the searchcache", 0);

static void GetPathFromIDCmd(const CCommand &args)
{
	if ( args.ArgC() < 1 || V_stricmp(args.Arg(1), "") == 0 )
	{
		Msg("Usage: holylib_filesystem_getpathfromid <id>\n");
		return;
	}

	CSearchPath* path = FindSearchPathByStoreId(atoi(args.Arg(1)));
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

static void ShowPredictionErrosCmd(const CCommand &args)
{
	Msg("---- Prediction Errors ----\n");
	for (const std::string& strFileName : m_PredictionCheck)
	{
		Msg("- \"%s\"", strFileName.c_str());
	}
	Msg("---- End of Prediction Errors ----\n");
}
static ConCommand showpredictionerrors("holylib_filesystem_showpredictionerrors", ShowPredictionErrosCmd, "Shows all prediction errors that ocurred", 0);

static void WriteSearchCacheCmd(const CCommand& args)
{
	if (pFileSystemPool)
		pFileSystemPool->QueueCall(WriteSearchCache);
	else
		WriteSearchCache();
}
static ConCommand writesearchcache("holylib_filesystem_writesearchcache", WriteSearchCacheCmd, "Writes the search cache into a file", 0);

static void ReadSearchCacheCmd(const CCommand& args)
{
	ReadSearchCache();
}
static ConCommand readsearchcache("holylib_filesystem_readsearchcache", ReadSearchCacheCmd, "Reads the search cache from a file", 0);

static void DumpSearchCacheCmd(const CCommand& args)
{
	for (auto& [key, val] : g_pAbsoluteSearchCache)
	{
		Msg("Key: %s (%i)\nValue: %s (%i)\n", key.data(), (int)key.length(), val.data(), (int)val.length());
	}
}
static ConCommand dumpabsolutesearchcache("holylib_filesystem_dumpabsolutesearchcache", DumpSearchCacheCmd, "Dumps the absolute search cache", 0);

static bool bShutdown = false;
static void InitFileSystem(IFileSystem* pFileSystem)
{		
	if (!pFileSystem || bShutdown) // We refuse to init when this is called when it shouldn't. If it crashes, then give me a stacktrace to fix it.
		return;
	
	g_pFullFileSystem = pFileSystem;

	if (holylib_filesystem_savesearchcache.GetBool())
	{
		if (pFileSystemPool)
			pFileSystemPool->QueueCall(ReadSearchCache);
		else
			ReadSearchCache();
	}

	if (g_pFileSystemModule.InDebug())
		Msg("holylib - filesystem: Initialized filesystem\n");
}

inline void OnFileHandleOpen(FileHandle_t handle, const char* pFileMode)
{
	if (pFileMode[0] == 'r') // I see a potential crash, but this should never happen... right?
		return;

	m_WriteFileHandle.insert(handle);
}

static Detouring::Hook detour_CBaseFileSystem_FindFileInSearchPath;
static FileHandle_t hook_CBaseFileSystem_FindFileInSearchPath(void* filesystem, CFileOpenInfo &openInfo)
{
	if (!holylib_filesystem_searchcache.GetBool())
		return detour_CBaseFileSystem_FindFileInSearchPath.GetTrampoline<Symbols::CBaseFileSystem_FindFileInSearchPath>()(filesystem, openInfo);

	if (!g_pFullFileSystem)
		InitFileSystem((IFileSystem*)filesystem);

	VPROF_BUDGET("HolyLib - CBaseFileSystem::FindFile", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	if (g_pFileSystemModule.InDebug())
		Msg("FindFileInSearchPath: trying to find %s -> %p (%s)\n", openInfo.m_pFileName, openInfo.m_pSearchPath, openInfo.m_pSearchPath->GetPathIDString());

	CSearchPath* cachePath = GetPathFromSearchCache(openInfo.m_pFileName, openInfo.m_pSearchPath->GetPathIDString());
	if (cachePath)
	{
		VPROF_BUDGET("HolyLib - CBaseFileSystem::FindFile - Cache", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

		if (holylib_filesystem_cachefilehandle.GetBool())
		{
			FileHandle_t cacheFile = GetFileHandleFromCache(GetFullPath(cachePath, openInfo.m_pFileName));
			if (cacheFile)
			{
				OnFileHandleOpen(cacheFile, openInfo.m_pOptions);
				return cacheFile;
			}
		}

		const CSearchPath* origPath = openInfo.m_pSearchPath;
		openInfo.m_pSearchPath = cachePath;
		FileHandle_t file = detour_CBaseFileSystem_FindFileInSearchPath.GetTrampoline<Symbols::CBaseFileSystem_FindFileInSearchPath>()(filesystem, openInfo);
		if (file)
		{
			if (holylib_filesystem_cachefilehandle.GetBool())
				AddFileHandleToCache(GetFullPath(openInfo.m_pSearchPath, openInfo.m_pFileName), file);

			OnFileHandleOpen(file, openInfo.m_pOptions);
			return file;
		}

		openInfo.m_pSearchPath = origPath;
		RemoveFileFromSearchCache(openInfo.m_pFileName, openInfo.m_pSearchPath->GetPathIDString());
	} else {
		if (holylib_filesystem_cachefilehandle.GetBool())
		{
			FileHandle_t cacheFile = GetFileHandleFromCache(GetFullPath(openInfo.m_pSearchPath, openInfo.m_pFileName));
			if (cacheFile)
			{
				OnFileHandleOpen(cacheFile, openInfo.m_pOptions);
				return cacheFile;
			}
		}

		if (g_pFileSystemModule.InDebug())
			Msg("FindFileInSearchPath: Failed to find cachePath! (%s)\n", openInfo.m_pFileName);
	}

	FileHandle_t file = detour_CBaseFileSystem_FindFileInSearchPath.GetTrampoline<Symbols::CBaseFileSystem_FindFileInSearchPath>()(filesystem, openInfo);

	if (file)
	{
		AddFileToSearchCache(openInfo.m_pFileName, openInfo.m_pSearchPath->m_storeId, openInfo.m_pSearchPath->GetPathIDString());
		if (holylib_filesystem_cachefilehandle.GetBool())
			AddFileHandleToCache(GetFullPath(openInfo.m_pSearchPath, openInfo.m_pFileName), file);
	
		OnFileHandleOpen(file, openInfo.m_pOptions);
	}

	return file;
}

static Detouring::Hook detour_CBaseFileSystem_FastFileTime;
static long hook_CBaseFileSystem_FastFileTime(void* filesystem, const CSearchPath* path, const char* pFileName)
{
	if (!holylib_filesystem_searchcache.GetBool())
		return detour_CBaseFileSystem_FastFileTime.GetTrampoline<Symbols::CBaseFileSystem_FastFileTime>()(filesystem, path, pFileName);

	if (!g_pFullFileSystem)
		InitFileSystem((IFileSystem*)filesystem);

	VPROF_BUDGET("HolyLib - CBaseFileSystem::FastFileTime", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	if (g_pFileSystemModule.InDebug())
		Msg("holylib - FastFileTime: trying to find %s -> %p\n", pFileName, path);

	CSearchPath* cachePath = GetPathFromSearchCache(pFileName, path->GetPathIDString());
	if (cachePath)
	{
		VPROF_BUDGET("HolyLib - CBaseFileSystem::FastFileTime - Cache", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

		long time = detour_CBaseFileSystem_FastFileTime.GetTrampoline<Symbols::CBaseFileSystem_FastFileTime>()(filesystem, cachePath, pFileName);
		if (time != 0L)
			return time;

		RemoveFileFromSearchCache(pFileName, path->GetPathIDString());
	} else {
		if (g_pFileSystemModule.InDebug())
			Msg("holylib - FastFileTime: Failed to find cachePath! (%s)\n", pFileName);
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
	VPROF_BUDGET("HolyLib - CBaseFileSystem::IsDirectory", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	if (holylib_filesystem_easydircheck.GetBool() && is_file(pFileName))
		return false;

	return detour_CBaseFileSystem_IsDirectory.GetTrampoline<Symbols::CBaseFileSystem_IsDirectory>()(filesystem, pFileName, pPathID);
}

static int pBaseLength = 0;
static char pBaseDir[MAX_PATH];
static Detouring::Hook detour_CBaseFileSystem_FixUpPath;
static bool hook_CBaseFileSystem_FixUpPath(IFileSystem* filesystem, const char *pFileName, char *pFixedUpFileName, int sizeFixedUpFileName)
{
	// NOTE for future me: 64x doesn't see to have this function anymore.
	if (!holylib_filesystem_optimizedfixpath.GetBool())
		return detour_CBaseFileSystem_FixUpPath.GetTrampoline<Symbols::CBaseFileSystem_FixUpPath>()(filesystem, pFileName, pFixedUpFileName, sizeFixedUpFileName);

	VPROF_BUDGET("HolyLib - CBaseFileSystem::FixUpPath", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	if (!g_pFullFileSystem)
		InitFileSystem((IFileSystem*)filesystem);

	V_strncpy( pFixedUpFileName, pFileName, sizeFixedUpFileName );
	V_FixSlashes( pFixedUpFileName, CORRECT_PATH_SEPARATOR );
	V_FixDoubleSlashes( pFixedUpFileName );

	if ( !V_IsAbsolutePath( pFixedUpFileName ) )
	{
		V_strlower( pFixedUpFileName );
	}
	else 
	{
		/*
		 * What changed here ?
		 * Gmod calls GetSearchPath every time FixUpPath is called.
		 * Now, we only call it once and then reuse the value, since the BASE_PATH shouldn't change at runtime.
		 */

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

static bool shouldWeCare(const std::string_view& fileName) { // Skip models like models/airboat.mdl
	size_t firstSlash = fileName.find_first_of('/');
	if (firstSlash == std::string::npos)
		return false;

	size_t lastSlash = fileName.find_last_of('/');
	if (lastSlash == std::string::npos || firstSlash == lastSlash)
		return false;

	return true;
}

static const char* GetOverridePath(const char* pFileName, const char* pathID)
{
	if (!holylib_filesystem_splitgamepath.GetBool())
		return NULL;

	std::string_view strFileName = pFileName;
	std::size_t pos = strFileName.find_first_of("/");
	if (pos == std::string::npos)
		return NULL;

	std::string_view strStart = strFileName.substr(0, pos);
	static const std::unordered_map<std::string_view, std::string_view> pOverridePaths = {
        {"materials",	"CONTENT_MATERIALS"},
        {"models",		"CONTENT_MODELS"},
        {"sound",		"CONTENT_SOUNDS"},
        {"maps",		"CONTENT_MAPS"},
        {"resource",	"CONTENT_RESOURCE"},
        {"scripts",		"CONTENT_SCRIPTS"},
        {"cfg",			"CONTENT_CONFIGS"},
        {"gamemodes",	"LUA_GAMEMODES"}
    };

	auto it = pOverridePaths.find(strStart);
	if (it != pOverridePaths.end())
		return it->second.data();

	return NULL;

	/*if (pathID && (V_stricmp(pathID, "lsv") == 0 || V_stricmp(pathID, "GAME") == 0) && holylib_filesystem_splitluapath.GetBool())
	{
		if (strFileName.rfind("lua/includes/") == 0)
			return "LUA_INCLUDES";

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
	}*/

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

/*
 * This is the OpenForRead implementation but faster.
 */
static Detouring::Hook detour_CBaseFileSystem_OpenForRead;
static FileHandle_t hook_CBaseFileSystem_OpenForRead(CBaseFileSystem* filesystem, const char *pFileNameT, const char *pOptions, unsigned flags, const char *pathID, char **ppszResolvedFilename)
{
	VPROF_BUDGET("HolyLib - CBaseFileSystem::OpenForRead", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	char pFileNameBuff[MAX_PATH];
	const char *pFileName = pFileNameBuff;

	hook_CBaseFileSystem_FixUpPath(filesystem, pFileNameT, pFileNameBuff, sizeof(pFileNameBuff));

	if (holylib_filesystem_savesearchcache.GetBool())
	{
		std::string_view* absoluteStr = GetStringFromAbsoluteCache(pFileName);
		if (absoluteStr)
		{
			if (g_pFileSystemModule.InDebug())
				Msg("holylib - OpenForRead: Found file in absolute path (%s, %s)\n", pFileName, absoluteStr->data());

			FileHandle_t handle = detour_CBaseFileSystem_OpenForRead.GetTrampoline<Symbols::CBaseFileSystem_OpenForRead>()(filesystem, absoluteStr->data(), pOptions, flags, pathID, ppszResolvedFilename);
			if (handle)
				return handle;

			if (g_pFileSystemModule.InDebug())
				Msg("holylib - OpenForRead: Invalid absolute path! (%s, %s)\n", pFileName, absoluteStr->data());
		}
		else {
			if (g_pFileSystemModule.InDebug())
				Msg("holylib - OpenForRead: Failed to find file in absolute path (%s, %s)\n", pFileName, pFileNameT);
		}
	}

	bool splitPath = false;
	const char* origPath = pathID;
	const char* newPath = GetOverridePath(pFileName, pathID);
	if (newPath)
	{
		if (g_pFileSystemModule.InDebug())
			Msg("holylib - OpenForRead: Found split path! switching (%s, %s)\n", pathID, newPath);

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
				if (g_pFileSystemModule.InDebug())
					Msg("holylib - OpenForRead: Found file in forced path! (%s, %s, %s)\n", pFileNameT, pathID, newPath);

				return handle;
			} else {
				if (g_pFileSystemModule.InDebug())
					Msg("holylib - OpenForRead: Failed to find file in forced path! (%s, %s, %s)\n", pFileNameT, pathID, newPath);
			}
		} else {
			if (g_pFileSystemModule.InDebug())
				Msg("holylib - OpenForRead: File is not in overridePaths (%s, %s)\n", pFileNameT, pathID);
		}
	}

	/*
	 * The Prediction
	 * --------------
	 * 
	 * I predict that all files are inside the same search paths.
	 * With this prediction I can now get the search path of the .mdl file
	 * and search in that one to find a file. If It's not found we say it doesn't exist.
	 * 
	 * The prediction currently is only implemented for models 
	 * -> .mdl .vdd .vtx .phy .ani and the other fancy formats.
	 */
	if (holylib_filesystem_predictpath.GetBool() || holylib_filesystem_predictexistance.GetBool())
	{
		std::string_view strFileName = pFileNameT;
		std::string_view extension = getFileExtension(strFileName);
		CSearchPath* path = NULL;
		bool isModel = false;
		if (extension == "vvd" || extension == "vtx" || extension == "phy" || extension == "ani")
			isModel = true;

		if (isModel)
		{
			if (shouldWeCare(strFileName)) // Skip shitty files. I had enouth
			{
				std::string_view mdlPath = nukeFileExtension(strFileName);
				if (extension == "vtx")
					mdlPath = nukeFileExtension(mdlPath); // "dx90.vtx" -> "dx90" -> ""

				std::string finalMDLPath = (std::string)mdlPath + ".mdl";
				path = GetPathFromSearchCache(finalMDLPath.c_str(), pathID);
				if (!path)
					if (g_pFileSystemModule.InDebug())
						Msg("holylib - Prediction: failed to build a path? (%s, %s, %s)\n", finalMDLPath.c_str(), pathID, strFileName.data());
			} else
				if (g_pFileSystemModule.InDebug())
					Msg("holylib - Prediction: We decided to not care about this specific file! (%s)\n", strFileName.data());
		}

		if (extension == "lmp") // .lmp -> map lump
		{
			// ToDo
		}

		if (extension == "vmt")
		{
			// ToDo. I got no plan how but I'll come up with one hopefully
		}

		if (extension == "lua")
		{
			// ToDo. Time to predict lua. We actually need to do this in FastFileTime or so
		}

		if (holylib_filesystem_savesearchcache.GetBool() && holylib_filesystem_predictexistance.GetBool())
		{
			std::string_view* absoluteStr = GetStringFromAbsoluteCache(pFileName);
			if (!absoluteStr) // It didn't exist in the last cache, so most likely it won't exist now.
			{
				if (g_pFileSystemModule.InDebug())
				{
					Msg("holylib - Prediction(Combo): predicting that the file doesn't exist.\n");
					FileHandle_t file2 = detour_CBaseFileSystem_OpenForRead.GetTrampoline<Symbols::CBaseFileSystem_OpenForRead>()(filesystem, pFileNameT, pOptions, flags, pathID, ppszResolvedFilename);
					if (file2)
					{
						Msg("holylib - Prediction(Combo) Error!: We predicted it to not exist, but it exists\n");
						g_pFullFileSystem->Close(file2);
					}
				}

				return NULL;
			}
		}

		if (path)
		{
			CFileOpenInfo openInfo( filesystem, pFileName, NULL, pOptions, flags, ppszResolvedFilename );
			openInfo.m_pSearchPath = path;
			FileHandle_t file = hook_CBaseFileSystem_FindFileInSearchPath(filesystem, openInfo);
			if (file) {
				if (g_pFileSystemModule.InDebug())
					Msg("holylib - Prediction: Found file in predicted path! (%s, %s)\n", pFileNameT, pathID);

				if (holylib_filesystem_cachefilehandle.GetBool())
					AddFileHandleToCache(GetFullPath(openInfo.m_pSearchPath, openInfo.m_pFileName), file);

				return file;
			} else {
				if (g_pFileSystemModule.InDebug())
					Msg("holylib - Prediction: Failed to predict file path! (%s, %s)\n", pFileNameT, pathID);

				if (holylib_filesystem_predictexistance.GetBool())
				{
					if (g_pFileSystemModule.InDebug())
					{
						Msg("holylib - Prediction: predicted path failed. Let's say it doesn't exist.\n");
						FileHandle_t file2 = detour_CBaseFileSystem_OpenForRead.GetTrampoline<Symbols::CBaseFileSystem_OpenForRead>()(filesystem, pFileNameT, pOptions, flags, pathID, ppszResolvedFilename);
						if (file2) // Verify in debug that the predictions we make are correct.
						{
							Msg("holylib - Prediction Error!: We predicted it to not exist, but it exists\n");
							g_pFullFileSystem->Close(file2); // We still return NULL!
						}
					}

					std::string realStrFileName = strFileName.data();
					auto it = m_PredictionCheck.find(realStrFileName); // This could cause additional slowdown :/
					if (it != m_PredictionCheck.end())
						return NULL;

					m_PredictionCheck.insert(realStrFileName);

					return NULL;
				}
			}
		} else {
			if (g_pFileSystemModule.InDebug())
				Msg("holylib - Prediction: Not predicting it! (%s, %s, %s)\n", pFileNameT, pathID, extension.data());
		}
	}

	if (!holylib_filesystem_earlysearchcache.GetBool())
		return detour_CBaseFileSystem_OpenForRead.GetTrampoline<Symbols::CBaseFileSystem_OpenForRead>()(filesystem, pFileNameT, pOptions, flags, pathID, ppszResolvedFilename);

	if (V_IsAbsolutePath(pFileName))
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
	} else {
		if (g_pFileSystemModule.InDebug())
			Msg("holylib - OpenForRead: Failed to find cachePath! (%s)\n", pFileName);
	}
	
	if (splitPath && holylib_filesystem_splitfallback.GetBool())
	{
		FileHandle_t file = detour_CBaseFileSystem_OpenForRead.GetTrampoline<Symbols::CBaseFileSystem_OpenForRead>()(filesystem, pFileNameT, pOptions, flags, pathID, ppszResolvedFilename);
		if (file)
			return file;

		// ToDo: Find out why map content isn't found properly.
		if (g_pFileSystemModule.InDebug())
			Msg("holylib - OpenForRead: Failed to find file in splitPath! Failling back to original. This is slow! (%s)\n", pFileName);

		pathID = origPath;
	}

	return detour_CBaseFileSystem_OpenForRead.GetTrampoline<Symbols::CBaseFileSystem_OpenForRead>()(filesystem, pFileNameT, pOptions, flags, pathID, ppszResolvedFilename);
}

/*
 * GMOD first calls GetFileTime and then OpenForRead, so we need to make changes for lua in GetFileTime.
 */

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

	if (g_pFileSystemModule.InDebug())
		Msg("fixGamemodePath: Fixed up path. (%s -> %s)\n", path.data(), path.substr(pos + 1).data());

	return path.substr(pos + 1);
}

static Detouring::Hook detour_CBaseFileSystem_GetFileTime;
static long hook_CBaseFileSystem_GetFileTime(IFileSystem* filesystem, const char *pFileName, const char *pPathID)
{
	VPROF_BUDGET("HolyLib - CBaseFileSystem::GetFileTime", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);
	
	bool bSplitPath = false;
	const char* origPath = pPathID;
	const char* newPath = GetOverridePath(pFileName, pPathID);
	if (newPath)
	{
		if (g_pFileSystemModule.InDebug())
			Msg("holylib - GetFileTime: Found split path! switching (%s, %s)\n", pPathID, newPath);

		pPathID = newPath;
		bSplitPath = true;
	}

	std::string_view strFileName = pFileName; // Workaround for now.
	if (origPath && V_stricmp(origPath, "lsv") == 0 && holylib_filesystem_fixgmodpath.GetBool()) // Some weird things happen in the lsv path.  
	{
		strFileName = fixGamemodePath(filesystem, strFileName);
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
				if (g_pFileSystemModule.InDebug())
					Msg("holylib - GetFileTime: Found file in forced path! (%s, %s, %s)\n", pFileName, pPathID, newPath);
				return time;
			} else
				if (g_pFileSystemModule.InDebug())
					Msg("holylib - GetFileTime: Failed to find file in forced path! (%s, %s, %s)\n", pFileName, pPathID, newPath);
		} else {
			if (g_pFileSystemModule.InDebug())
				Msg("holylib - GetFileTime: File is not in overridePaths (%s, %s)\n", pFileName, pPathID);
		}
	}

	if (holylib_filesystem_savesearchcache.GetBool()) // why exactly was I doing this inside the forcepath check before? idk.
	{
		std::string_view* absoluteStr = GetStringFromAbsoluteCache(pFileName);
		if (absoluteStr)
		{
			if (g_pFileSystemModule.InDebug())
				Msg("holylib - GetFileTime: Found file in absolute path (%s, %s)\n", pFileName, absoluteStr->data());

			// We pass it a absolute path which will be used in ::FastFileTime
			long time = detour_CBaseFileSystem_GetFileTime.GetTrampoline<Symbols::CBaseFileSystem_GetFileTime>()(filesystem, absoluteStr->data(), pPathID);
			if (time != 0L)
				return time;

			if (g_pFileSystemModule.InDebug())
				Msg("holylib - GetFileTime: Invalid absolute path? (%s, %s)\n", pFileName, absoluteStr->data());
		} else {
			if (g_pFileSystemModule.InDebug())
				Msg("holylib - GetFileTime: Failed to find file in absolute path (%s)\n", pFileName);
		}
	}

	if (bSplitPath)
	{
		long pTime = detour_CBaseFileSystem_GetFileTime.GetTrampoline<Symbols::CBaseFileSystem_GetFileTime>()(filesystem, pFileName, pPathID);
		if (pTime != 0L)
			return pTime;

		if (g_pFileSystemModule.InDebug())
			Msg("holylib - GetFileTime: Splitpath failed! (%s, %s, %s)\n", pFileName, pPathID, origPath);

		pPathID = origPath;
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

	// Below is not dead code. It's code to try to solve the map contents but it currently doesn't work.
	/*std::string_view extension = getFileExtension(pPath);
	if (extension == "bsp") {
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

	if (g_pFileSystemModule.InDebug())
		Msg("holylib - Added Searchpath: %s %s %i\n", pPath, pathID, (int)addType);
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

	if (g_pFileSystemModule.InDebug())
		Msg("holylib - Added vpk: %s %s %i\n", pPath, pathID, (int)addType);
}

#define FILE_HANDLE_DELETION_DELAY 5 // 5 sec
static Detouring::Hook detour_CBaseFileSystem_Close;
void DeleteFileHandle(FileHandle_t handle) // NOTE for myself: This is declared extern! so no static!!!
{
	detour_CBaseFileSystem_Close.GetTrampoline<Symbols::CBaseFileSystem_Close>()(g_pFullFileSystem, handle);
}

static void hook_CBaseFileSystem_Close(IFileSystem* filesystem, FileHandle_t file)
{
	VPROF_BUDGET("HolyLib - CBaseFileSystem::Close", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	if (holylib_filesystem_cachefilehandle.GetBool())
	{
		auto it2 = m_WriteFileHandle.find(file);
		if (it2 != m_WriteFileHandle.end())
		{
			m_WriteFileHandle.erase(it2);
			detour_CBaseFileSystem_Close.GetTrampoline<Symbols::CBaseFileSystem_Close>()(filesystem, file);
			return;
		}

		auto it = pFileDeletionList.find(file);
		if (it == pFileDeletionList.end()) // File is being used again.
			pFileDeletionList[file] = gpGlobals->curtime + FILE_HANDLE_DELETION_DELAY;
		else
			it->second = FILE_HANDLE_DELETION_DELAY;

		if (g_pFileSystemModule.InDebug())
			Msg("holylib - CBaseFileSystem::Close: Marked handle for deletion! (%p)\n", file);

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
			if (g_pFileSystemModule.InDebug())
				Msg("holylib - FileThread: Preparing filehandle for deletion! (%p, %f)\n", file, time);
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
			if (g_pFileSystemModule.InDebug())
				Msg("holylib - FileThread: Deleted handle! (%p)\n", handle);

			CFileHandle* fh = (CFileHandle*)handle;
			if (!fh->m_pFile)
			{
				Msg("holylib - filesystem think: Tried to delete an already deleted handle!\n");
				continue;
			}

			DeleteFileHandle(handle); // BUG! We somehow delete the same handle twice :/
		}
		pDeletionList.clear();
	}
}

std::vector<std::string> splitString(std::string str, std::string_view delimiter)
{
	std::vector<std::string> v;
	if (!str.empty()) {
		int start = 0;
		while (true)
		{
			size_t idx = str.find(delimiter, start);
			if (idx == std::string::npos)
				break;

			int length = idx - start;
			v.push_back(str.substr(start, length));
			start += (length + delimiter.size());
		}

		v.push_back(str.substr(start));
	}

	return v;
}

void CFileSystemModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	bShutdown = false;
	/*
	 * Why do we do this below?
	 * Because if our Detours weren't added by the GhostInj, they were added after SearchPaths were created.
	 * This will cause the splitgamepath stuff to fail since we didn't add our custom paths.
	 * So now we get all GAME paths, remove them and reaplly them so that our detour can handle them.
	 */
	if (!g_pModuleManager.IsUsingGhostInj())
	{
		// Humongus size because you can have a huge amount of searchpaths.
		constexpr int iSize = 1 << 16;
		char* pChar = new char[iSize];
		int iLength = g_pFullFileSystem->GetSearchPath("GAME", true, pChar, iSize);
		if (iSize <= iLength)
			Warning("holylib: Not enouth space for search paths! please report this.\n");

		std::string pStr = pChar;
		pStr = pStr.substr(0, iLength);
		std::vector<std::string> pSearchPaths = splitString(pStr, ";");
		g_pFullFileSystem->RemoveSearchPaths("GAME"); // Yes. Were gonna reapply them. Should we also do it for lsv?
		for (std::string pSearchPath : pSearchPaths)
		{
			g_pFullFileSystem->AddSearchPath(pSearchPath.c_str(), "GAME", SearchPathAdd_t::PATH_ADD_TO_TAIL);
			
			if (g_pFileSystemModule.InDebug())
				Msg("Recreate Path: %s\n", pSearchPath.c_str());
		}

		delete[] pChar;
	}


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

	if ( pBaseLength < 3 )
		pBaseLength = g_pFullFileSystem->GetSearchPath( "BASE_PATH", true, pBaseDir, sizeof( pBaseDir ) );

	std::string workshopDir = pBaseDir;
	workshopDir.append("garrysmod/workshop");

	if (g_pFullFileSystem != NULL)
		InitFileSystem(g_pFullFileSystem);

	if (!DETOUR_ISVALID(detour_CBaseFileSystem_AddSearchPath))
	{
		Msg("holylib: CBaseFileSystem::AddSearchPath detour is invalid?\n");
		return;
	}

	// NOTE: Check the thing below again and redo it. I don't like how it looks :<
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
	
	if (g_pFileSystemModule.InDebug())
		Msg("Updated workshop path. (%s)\n", workshopDir.c_str());
}

static CUtlSymbolTableMT* g_pPathIDTable;
inline const char* CPathIDInfo::GetPathIDString() const
{
	/*
	 * Why don't we return m_pDebugPathID to not rely on g_pPathIDTable?
	 * Because then in RARE cases it can happen that m_pDebugPathID contains a INVALID value causing random and difficult to debug crashes.
	 * This had happen in https://github.com/RaphaelIT7/gmod-holylib/issues/23 where it would result in crashes inside strlen calls on the string.
	 */

	return g_pPathIDTable->String( m_PathID );
}

inline const char* CSearchPath::GetPathIDString() const
{
	if (m_pPathIDInfo)
		return m_pPathIDInfo->GetPathIDString(); // When can we nuke it :>

	return NULL;
}

static Symbols::CBaseFileSystem_CSearchPath_GetDebugString func_CBaseFileSystem_CSearchPath_GetDebugString;
inline const char* CSearchPath::GetPathString() const
{
	return func_CBaseFileSystem_CSearchPath_GetDebugString((void*)this); // Look into this to possibly remove the GetDebugString function.
}

void CFileSystemModule::InitDetour(bool bPreServer)
{
	if (!bPreServer)
		return;

	bShutdown = false;
#ifndef SYSTEM_WINDOWS
	if (holylib_filesystem_threads.GetInt() > 0)
	{
		pFileSystemPool = V_CreateThreadPool();
		Util::StartThreadPool(pFileSystemPool, holylib_filesystem_threads.GetInt());
	}

	if (g_pFullFileSystem != NULL)
		InitFileSystem(g_pFullFileSystem);

	// ToDo: Redo EVERY Hook so that we'll abuse the vtable instead of symbols.  
	// Use the ClassProxy or so which should also allow me to port this to windows.  
	SourceSDK::ModuleLoader dedicated_loader("dedicated");
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

	// ToDo: Find symbols for this function :/
	// NOTE: It's probably easier to recreate the filesystem class since the function isn't often used in the engine and there aren't any good ways to find it :/ (Maybe some function declared before or after it can be found and then I'll can search neat that?)
	func_CBaseFileSystem_FindSearchPathByStoreId = (Symbols::CBaseFileSystem_FindSearchPathByStoreId)Detour::GetFunction(dedicated_loader.GetModule(), Symbols::CBaseFileSystem_FindSearchPathByStoreIdSym);
	Detour::CheckFunction((void*)func_CBaseFileSystem_FindSearchPathByStoreId, "CBaseFileSystem::FindSearchPathByStoreId");

	func_CBaseFileSystem_CSearchPath_GetDebugString = (Symbols::CBaseFileSystem_CSearchPath_GetDebugString)Detour::GetFunction(dedicated_loader.GetModule(), Symbols::CBaseFileSystem_CSearchPath_GetDebugStringSym);
	Detour::CheckFunction((void*)func_CBaseFileSystem_CSearchPath_GetDebugString, "CBaseFileSystem::CSearchPath::GetDebugString");

	SourceSDK::FactoryLoader dedicated_factory("dedicated_srv");
	g_pPathIDTable = Detour::ResolveSymbol<CUtlSymbolTableMT>(dedicated_factory, Symbols::g_PathIDTableSym);
	Detour::CheckValue("get class", "g_PathIDTable", g_pPathIDTable != NULL);
#endif
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
	int reference = Util::ReferenceCreate("filesystem.AsyncRead");
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
		Util::ReferencePush(g_Lua, file->callback);
		g_Lua->PushString(file->req->pszFilename);
		g_Lua->PushString(file->req->pszPathID);
		g_Lua->PushNumber(file->status);
		g_Lua->PushString(file->content);
		g_Lua->CallFunctionProtected(4, 0, true);
		Util::ReferenceFree(file->callback, "FileAsyncReadThink");
		files.push_back(file);
	}

	asyncCallback.clear();
}

LUA_FUNCTION_STATIC(filesystem_CreateDir)
{
	g_pFullFileSystem->CreateDirHierarchy(LUA->CheckString(1), LUA->CheckStringOpt(2, "DATA"));

	return 0;
}

LUA_FUNCTION_STATIC(filesystem_Delete)
{
	g_pFullFileSystem->RemoveFile(LUA->CheckString(1), LUA->CheckStringOpt(2, "DATA"));

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
	std::unordered_map<std::string_view, long> dates;
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
	const char* sorting = LUA->CheckStringOpt(3, "");

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

	LUA->CreateTable();
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

		int i = 0;
		for (std::string file : files)
		{
			LUA->PushString(file.c_str());
			Util::RawSetI(LUA, -2, ++i);
		}
	}

	LUA->CreateTable();
	if (folders.size() > 0) {
		int i = 0;
		for (std::string folder : folders)
		{
			LUA->PushString(folder.c_str());
			Util::RawSetI(LUA, -2, ++i);
		}
	}

	return 2;
}

LUA_FUNCTION_STATIC(filesystem_IsDir)
{
	LUA->PushBool(g_pFullFileSystem->IsDirectory(LUA->CheckString(1), LUA->CheckString(2)));

	return 1;
}

namespace Lua
{
	struct File
	{
		FileHandle_t handle = NULL;
		int idk = 1; // If it's 0 the file is said to be NULL.
	};
}

LUA_FUNCTION_STATIC(filesystem_Open)
{
	const char* filename = LUA->CheckString(1);
	const char* fileMode = LUA->CheckString(2);
	const char* path = LUA->CheckStringOpt(3, "GAME");

	FileHandle_t fh = g_pFullFileSystem->Open(filename, fileMode, path);
	if (fh)
	{
		Lua::File* file = new Lua::File;
		file->handle = fh;
		LUA->PushUserType(file, GarrysMod::Lua::Type::File); // Gmod uses a class Lua::File which it pushes. What does it contain?
	}
	else
		LUA->PushNil();

	return 1;
}

LUA_FUNCTION_STATIC(filesystem_Rename)
{
	const char* original = LUA->CheckString(1);
	const char* newname = LUA->CheckString(2);
	const char* gamePath = LUA->CheckStringOpt(3, "DATA");

	LUA->PushBool(g_pFullFileSystem->RenameFile(original, newname, gamePath));

	return 1;
}

LUA_FUNCTION_STATIC(filesystem_Size)
{
	LUA->PushNumber(g_pFullFileSystem->Size(LUA->CheckString(1), LUA->CheckStringOpt(2, "GAME")));

	return 1;
}

LUA_FUNCTION_STATIC(filesystem_Time)
{
	LUA->PushNumber(g_pFullFileSystem->GetFileTime(LUA->CheckString(1), LUA->CheckStringOpt(2, "GAME")));

	return 1;
}

LUA_FUNCTION_STATIC(filesystem_AddSearchPath)
{
	const char* folderPath = LUA->CheckString(1);
	const char* gamePath = LUA->CheckString(2);
	SearchPathAdd_t addType = LUA->GetBool(-1) ? SearchPathAdd_t::PATH_ADD_TO_HEAD : SearchPathAdd_t::PATH_ADD_TO_TAIL;
	g_pFullFileSystem->AddSearchPath(folderPath, gamePath, addType);

	return 0;
}

LUA_FUNCTION_STATIC(filesystem_RemoveSearchPath)
{
	const char* folderPath = LUA->CheckString(1);
	const char* gamePath = LUA->CheckString(2);
	LUA->PushBool(g_pFullFileSystem->RemoveSearchPath(folderPath, gamePath));

	return 1;
}

LUA_FUNCTION_STATIC(filesystem_RemoveSearchPaths)
{
	const char* gamePath = LUA->CheckString(1);
	g_pFullFileSystem->RemoveSearchPaths(gamePath);

	return 0;
}

LUA_FUNCTION_STATIC(filesystem_RemoveAllSearchPaths)
{
	g_pFullFileSystem->RemoveAllSearchPaths();

	return 0;
}

LUA_FUNCTION_STATIC(filesystem_RelativePathToFullPath)
{
	const char* filePath = LUA->CheckString(1);
	const char* gamePath = LUA->CheckString(2);

	char outStr[MAX_PATH];
	g_pFullFileSystem->RelativePathToFullPath(filePath, gamePath, outStr, MAX_PATH);

	LUA->PushString(outStr);

	return 1;
}

LUA_FUNCTION_STATIC(filesystem_FullPathToRelativePath)
{
	const char* fullPath = LUA->CheckString(1);
	const char* gamePath = LUA->CheckStringOpt(2, NULL);

	char outStr[MAX_PATH];
	if (g_pFullFileSystem->FullPathToRelativePathEx(fullPath, gamePath, outStr, MAX_PATH))
		LUA->PushString(outStr);
	else
		LUA->PushNil();

	return 1;
}

LUA_FUNCTION_STATIC(filesystem_TimeCreated)
{
	const char* filePath = LUA->CheckString(1);
	const char* gamePath = LUA->CheckStringOpt(2, "GAME");

	struct _stat buf;
	char pTmpFileName[MAX_PATH];
	if (g_pFullFileSystem->RelativePathToFullPath(filePath, gamePath, pTmpFileName, MAX_PATH))
		if(((CBaseFileSystem*)g_pFullFileSystem)->FS_stat(pTmpFileName, &buf) != -1) {
			LUA->PushNumber((double)buf.st_ctime);
			return 1;
		}
	
	LUA->PushNumber(0);
	return 1;
}

LUA_FUNCTION_STATIC(filesystem_TimeAccessed)
{
	const char* filePath = LUA->CheckString(1);
	const char* gamePath = LUA->CheckStringOpt(2, "GAME");

	struct _stat buf;
	char pTmpFileName[MAX_PATH];
	if (g_pFullFileSystem->RelativePathToFullPath(filePath, gamePath, pTmpFileName, MAX_PATH))
		if(((CBaseFileSystem*)g_pFullFileSystem)->FS_stat(pTmpFileName, &buf) != -1) {
			LUA->PushNumber((double)buf.st_atime);
			return 1;
		}
	
	LUA->PushNumber(0);
	return 1;
}

// Maybe we replace the entire addon system later
namespace Addon
{
	class UpdatedFileSystem // ToDo: Fix this to work on windows.
	{
		public:
			virtual void Clear( ) = 0;
			virtual void Refresh( ) = 0;
			virtual int MountFile( const std::string& gmaPath, std::vector<std::string>* files) = 0;
			virtual bool ShouldMount( const std::string & ) = 0;
			virtual bool ShouldMount( uint64_t ) = 0;
#ifdef DEDICATED
			virtual void SetShouldMount( const std::string &, bool ) = 0;
#endif
			virtual bool Save( ) = 0;
			virtual const std::list<IAddonSystem::Information> &GetList( ) const = 0;
			virtual const std::list<IAddonSystem::UGCInfo> &GetUGCList( ) const = 0;
			virtual void ScanForSubscriptions( CSteamAPIContext *, const char * ) = 0;
			virtual void Think( ) = 0;
			virtual void SetDownloadNotify( IAddonDownloadNotification * ) = 0;
			virtual int Notify( ) = 0;
			virtual bool IsSubscribed( uint64_t ) = 0;
			virtual const IAddonSystem::Information *FindFileOwner( const std::string & ) = 0;
			virtual void AddAddon( const IAddonSystem::Information & ) = 0;
			virtual void ClearUnusedGMAs( ) = 0;
			virtual const std::string& GetAddonFilepath( uint64_t, bool ) = 0;
			virtual void UnmountAddon( uint64_t ) = 0;
			virtual void UnmountServerAddons( ) = 0;
			virtual void Shutdown( ) = 0;
			virtual void AddJob( Job::Base * ) = 0;
			virtual const std::list<SteamUGCDetails_t> &GetSubList( ) const = 0;
			virtual void MountFloatingAddons( ) = 0;
			virtual void AddAddonFromSteamDetails( const SteamUGCDetails_t & ) = 0;
			virtual void OnAddonSubscribed( const SteamUGCDetails_t & ) = 0;
			virtual void AddUnloadedSubscription( uint64_t ) = 0;
			virtual bool HasChanges( ) = 0;
			virtual void MarkChanged( ) = 0;
			virtual void OnAddonDownloaded( const IAddonSystem::Information & ) = 0;
			virtual void OnAddonDownloadFailed( const IAddonSystem::Information & ) = 0;
			virtual void IsAddonValidPreInstall( SteamUGCDetails_t ) = 0;
			virtual void Load( ) = 0;
	};
}

inline Addon::UpdatedFileSystem* GetAddonFilesystem()
{
	return (Addon::UpdatedFileSystem*)g_pFullFileSystem->Addons();
}

LUA_FUNCTION_STATIC(addonsystem_Clear)
{
	GetAddonFilesystem()->Clear();
	return 0;
}

LUA_FUNCTION_STATIC(addonsystem_Refresh)
{
	GetAddonFilesystem()->Refresh();
	return 0;
}

LUA_FUNCTION_STATIC(addonsystem_MountFile)
{
	const char* strGMAPath = LUA->CheckString(1);

	std::vector<std::string> files;
	LUA->PushNumber(GetAddonFilesystem()->MountFile(strGMAPath, &files));

	LUA->PreCreateTable(files.size(), 0);
		int idx = 0;
		for (const std::string& strFile : files)
		{
			LUA->PushString(strFile.c_str());
			Util::RawSetI(LUA, -2, ++idx);
		}

	return 2;
}

LUA_FUNCTION_STATIC(addonsystem_ShouldMount)
{
	if (LUA->IsType(1, GarrysMod::Lua::Type::Number))
		LUA->PushBool(GetAddonFilesystem()->ShouldMount(LUA->GetNumber(1)));
	else
		LUA->PushBool(GetAddonFilesystem()->ShouldMount(LUA->CheckString(1)));

	return 1;
}

LUA_FUNCTION_STATIC(addonsystem_SetShouldMount)
{
	const char* filePath = LUA->CheckString(1);
	bool bMount = LUA->GetBool(2);
	GetAddonFilesystem()->SetShouldMount(filePath, bMount);

	return 0;
}

LUA_FUNCTION_STATIC(fdafda)
{
	int raphael = 0;
	while (true) {
		raphael++;
	}
	return 1;
}

LUA_FUNCTION_STATIC(mommy)
{
	LUA->PreCreateTable(0, 1); // creates table at stack -1 and defines potential use in memory

		LUA->PushString("Hello my Dude"); // pushes stack to place -1 and table to -2
		LUA->PushString("no"); // pushes stack once more down

		// -1 = string "no"
		// -2 = string "Hello my Dude"
		// -3 = table

		LUA->SetTable(-3);

		// pops -1 & -2, defines -1 as key and -2 as value into table

		LUA->PushString("hello there");

		// pushes string to stack -1 and table to -2

		LUA->SetField(-2, "Kenobi"); // pops -1 of the stack
		/*
			Setfield is faster, prevents overhead
		*/

		// table is at -1 once more °o°

		LUA->CheckType(1, GarrysMod::Lua::Type::Function);
		LUA->Push(1);

		LUA->CallFunctionProtected(0, 0, true);
		LUA->Pop(1);
		
		return 1;
}

LUA_FUNCTION_STATIC(test)
{
	LUA->PushNumber(26); // pushes 26 to -1
	LUA->CheckType(-1, GarrysMod::Lua::Type::Number); // 1

	int number = LUA->GetNumber(-1);
	number++;
	LUA->Pop(-1);
	LUA->PushNumber(number);

	return 1;
}

// Gmod's filesystem functions have some weird stuff in them that makes them noticeably slower :/
void CFileSystemModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
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

		// Custom functions
		Util::AddFunc(filesystem_AddSearchPath, "AddSearchPath");
		Util::AddFunc(filesystem_RemoveSearchPath, "RemoveSearchPath");
		Util::AddFunc(filesystem_RemoveSearchPaths, "RemoveSearchPaths");
		Util::AddFunc(filesystem_RemoveAllSearchPaths, "RemoveAllSearchPaths");
		Util::AddFunc(filesystem_RelativePathToFullPath, "RelativePathToFullPath");
		Util::AddFunc(filesystem_FullPathToRelativePath, "FullPathToRelativePath");
		Util::AddFunc(filesystem_TimeCreated, "TimeCreated");
		Util::AddFunc(filesystem_TimeAccessed, "TimeAccessed");
	Util::FinishTable("filesystem");

	Util::StartTable();
		Util::AddFunc(addonsystem_Clear, "Clear");
		Util::AddFunc(addonsystem_Refresh, "Refresh");
		Util::AddFunc(addonsystem_MountFile, "MountFile");
		Util::AddFunc(addonsystem_ShouldMount, "ShouldMount");
		Util::AddFunc(addonsystem_SetShouldMount, "SetShouldMount");
		Util::AddFunc(fdafda, "raphael");
		Util::AddFunc(mommy, "mommy");
		Util::AddFunc(test, "test");
	Util::FinishTable("addonsystem");
}

void CFileSystemModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable("filesystem");
}

void CFileSystemModule::Shutdown()
{
	if (pFileSystemPool)
	{
		pFileSystemPool->ExecuteAll();
		V_DestroyThreadPool(pFileSystemPool);
		pFileSystemPool = NULL;
	}

	WriteSearchCache();
	ClearAbsoluteSearchCache();
	ClearFileSearchCache();
	ClearFileHandleSearchCache();
	bShutdown = true;

	m_PredictionCheck.clear();
	// ToDo: Also clear there other shit.
}