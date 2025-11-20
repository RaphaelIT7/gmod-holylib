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

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CFileSystemModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual void Think(bool bSimulating) OVERRIDE;
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaThink(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax) OVERRIDE;
	virtual const char* Name() { return "filesystem"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32; };
	virtual bool SupportsMultipleLuaStates() { return true; };
};

static CFileSystemModule g_pFileSystemModule;
IModule* pFileSystemModule = &g_pFileSystemModule;

static ConVar holylib_filesystem_easydircheck("holylib_filesystem_easydircheck", "0", FCVAR_ARCHIVE, 
	"Checks if the folder CBaseFileSystem::IsDirectory checks has a . in the name after the last /. if so assume it's a file extension.");
static ConVar holylib_filesystem_searchcache("holylib_filesystem_searchcache", "1", FCVAR_ARCHIVE, 
	"If enabled, it will cache the search path a file was located in and if the same file is requested, it will use that search path directly.");
static ConVar holylib_filesystem_optimizedfixpath("holylib_filesystem_optimizedfixpath", "1", FCVAR_ARCHIVE, 
	"If enabled, it will optimize CBaseFilesystem::FixUpPath by caching the BASE_PATH search cache.");
static ConVar holylib_filesystem_earlysearchcache("holylib_filesystem_earlysearchcache", "1", FCVAR_ARCHIVE, 
	"If enabled, it will check early in CBaseFilesystem::OpenForRead if the file is in the search cache.");
static ConVar holylib_filesystem_forcepath("holylib_filesystem_forcepath", "1", FCVAR_ARCHIVE, 
	"If enabled, it will change the paths of some specific files");
static ConVar holylib_filesystem_predictpath("holylib_filesystem_predictpath", "1", FCVAR_ARCHIVE, 
	"If enabled, it will try to predict the path of a file");
static ConVar holylib_filesystem_predictexistance("holylib_filesystem_predictexistance", "0", 0, 
	"If enabled, it will try to predict the path of a file, but if the file doesn't exist in the predicted path, we'll just say it doesn't exist.");
static ConVar holylib_filesystem_splitgamepath("holylib_filesystem_splitgamepath", "1", FCVAR_ARCHIVE, 
	"If enabled, it will create for each content type like models/, materials/ a game path which will be used to find that content.");
static ConVar holylib_filesystem_splitluapath("holylib_filesystem_splitluapath", "0", 0, 
	"If enabled, it will do the same thing holylib_filesystem_splitgamepath does but with lsv. Currently it breaks workshop addons.");
static ConVar holylib_filesystem_splitfallback("holylib_filesystem_splitfallback", "1", FCVAR_ARCHIVE, 
	"If enabled, it will fallback to the original searchpath if the split path failed.");
static ConVar holylib_filesystem_fixgmodpath("holylib_filesystem_fixgmodpath", "1", FCVAR_ARCHIVE, 
	"If enabled, it will fix up weird gamemode paths like sandbox/gamemode/sandbox/gamemode which gmod likes to use.");
static ConVar holylib_filesystem_cachefilehandle("holylib_filesystem_cachefilehandle", "0", 0, 
	"If enabled, it will cache the file handle and return it if needed. This will probably cause issues if you open the same file multiple times.");
static ConVar holylib_filesystem_precachehandle("holylib_filesystem_precachehandle", "1", 0,
	"If enabled, it will try to predict which file it will open next and open the file to keep a handle ready to be opened.");
static ConVar holylib_filesystem_savesearchcache("holylib_filesystem_savesearchcache", "1", FCVAR_ARCHIVE,
	"If enabled, it will write the search cache into a file and restore it when starting, using it to improve performance.");
static ConVar holylib_filesystem_mergesearchcache("holylib_filesystem_mergesearchcache", "1", FCVAR_ARCHIVE,
	"If enabled, when saving the search cache it will not remove old entries and instead keep them even if they were unused this session");
static ConVar holylib_filesystem_skipinvalidluapaths("holylib_filesystem_skipinvalidluapaths", "1", FCVAR_ARCHIVE,
	"If enabled, invalid lua paths like include/include/ will be skipped instantly");
static ConVar holylib_filesystem_tryalternativeluapath("holylib_filesystem_tryalternativeluapath", "1", FCVAR_ARCHIVE,
	"If enabled, if it can't find a file in the search cache, it will remove the first folder and try again as when loading Lua gmod loves to test different folders first");

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
			Util::DestroyThreadPool(pFileSystemPool);
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

#if SYSTEM_WINDOWS
#define FILEPATH_SLASH "\\"
#define FILEPATH_SLASH_CHAR '\\'
#else
#define FILEPATH_SLASH "/"
#define FILEPATH_SLASH_CHAR '/'
#endif

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

	int iPos = (int)g_pFullFileSystem->Tell(it->second);
	if (iPos != 0)
	{
		if (g_pFileSystemModule.InDebug())
			Msg("holylib - GetFileHandleFromCache: Pos: %u\n", g_pFullFileSystem->Tell(it->second));
		
		g_pFullFileSystem->Seek(it->second, 0, FILESYSTEM_SEEK_HEAD); // Why doesn't it reset?
		
		if (g_pFileSystemModule.InDebug())
			Msg("holylib - GetFileHandleFromCache: Rewind pos: %u\n", g_pFullFileSystem->Tell(it->second));
		
		int iNewPos = (int)g_pFullFileSystem->Tell(it->second);
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

#if SYSTEM_WINDOWS
// Only way on Windows... I hate this so much
CSearchPath *CBaseFileSystem::FindSearchPathByStoreId( int storeId )
{
	FOR_EACH_LL( m_SearchPaths, i )
	{
		CSearchPath& pSearchPath = m_SearchPaths[(unsigned short)i];
		if ( pSearchPath.m_storeId == storeId )
			return &pSearchPath;
	}

	return NULL;
}

static inline CSearchPath* FindSearchPathByStoreId(int iStoreID)
{
	return ((CBaseFileSystem*)g_pFullFileSystem)->FindSearchPathByStoreId(iStoreID);
}
#else
static Symbols::CBaseFileSystem_FindSearchPathByStoreId func_CBaseFileSystem_FindSearchPathByStoreId;
static inline CSearchPath* FindSearchPathByStoreId(int iStoreID)
{
	if (!func_CBaseFileSystem_FindSearchPathByStoreId)
	{
		Warning(PROJECT_NAME ": Failed to get CBaseFileSystem::FindSearchPathByStoreId!\n");
		return NULL;
	}

	return func_CBaseFileSystem_FindSearchPathByStoreId(g_pFullFileSystem, iStoreID);
}
#endif

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

#define SearchCacheVersion 2
// #define MaxSearchCacheEntries (1 << 16) // 64k max files
struct SearchCache {
	unsigned int version = SearchCacheVersion;
	unsigned int usedPaths = 0;
	//SearchCacheEntry paths[MaxSearchCacheEntries];
};

static inline void WriteStringIntoFile(FileHandle_t pHandle, const std::string_view& pValue)
{
	// NOTE: We use unsigned char which is NOT MAX_PATH, though I'm not gonna use 1 byte for just 5 extra length
	unsigned char valueLength = (unsigned char)pValue.length();
	const char* value = pValue.data();
	g_pFullFileSystem->Write(&valueLength, sizeof(valueLength), pHandle);
	g_pFullFileSystem->Write(value, valueLength, pHandle);
}

static inline void WriteStringIntoFile(FileHandle_t pHandle, const char* value, unsigned char valueLength)
{
	// NOTE: We use unsigned char which is NOT MAX_PATH, though I'm not gonna use 1 byte for just 5 extra length
	g_pFullFileSystem->Write(&valueLength, sizeof(valueLength), pHandle);
	g_pFullFileSystem->Write(value, valueLength, pHandle);
}

static std::unordered_map<std::string_view, std::unordered_map<std::string_view, std::string_view>> g_pAbsoluteSearchCache;
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

		if (holylib_filesystem_mergesearchcache.GetBool())
		{
			for (auto& [strPath, cache] : g_pAbsoluteSearchCache)
				searchCache.usedPaths += cache.size();
		}

		g_pFullFileSystem->Write(&searchCache, sizeof(SearchCache), handle);

		if (!holylib_filesystem_mergesearchcache.GetBool())
		{
			char absolutePathBuffer[MAX_PATH];
			for (auto& [strPath, cache] : m_SearchCache)
			{
				for (auto& [strEntry, storeID] : cache)
				{
					WriteStringIntoFile(handle, strPath);
					WriteStringIntoFile(handle, strEntry);

					absolutePathBuffer[0] = '\0';
					g_pFullFileSystem->RelativePathToFullPath(strEntry.data(), strPath.data(), absolutePathBuffer, sizeof(absolutePathBuffer));

					WriteStringIntoFile(handle, absolutePathBuffer, (unsigned char)strlen(absolutePathBuffer));
				}
			}
		} else {
			// Our goal is to write the existing absolute cache now too
			// Above we only write the search cache which only contains all files cached in this sesssion
			// And it discards the previous cache entries which weren't used

			// A COPY that we now fill
			std::unordered_map<std::string_view, std::unordered_map<std::string_view, std::string_view>> pAbsoluteCache = g_pAbsoluteSearchCache;
			std::vector<char*> pTempMemory;
			// For the merge we allocate temporary memory, and since pAbsoluteCache is a copy, it would be deconstructed leaving the pointers leaking

			unsigned int nUsedSearchCachePaths = 0;
			for (auto& [strPath, cache] : m_SearchCache)
				nUsedSearchCachePaths += cache.size();

			pTempMemory.reserve(nUsedSearchCachePaths); // We reserve just for more SPEEED

			char absolutePathBuffer[MAX_PATH];
			for (auto& [strPath, cache] : m_SearchCache)
			{
				for (auto& [strEntry, storeID] : cache)
				{
					absolutePathBuffer[0] = '\0';
					g_pFullFileSystem->RelativePathToFullPath(strEntry.data(), strPath.data(), absolutePathBuffer, sizeof(absolutePathBuffer));

					unsigned char nLength = (unsigned char)strlen(absolutePathBuffer);
					char* pAbsolutePath = new char[nLength + 1];
					pTempMemory.push_back(pAbsolutePath);
					memcpy(pAbsolutePath, absolutePathBuffer, nLength);
					pAbsolutePath[nLength] = '\0';
					pAbsoluteCache[strPath][strEntry] = std::string_view(pAbsolutePath, nLength);
					// We override so that we don't get new entries instead at worse replacing them
				}
			}

			for (auto& [strPath, cache] : pAbsoluteCache)
			{
				for (auto& [relativePath, absolutePath] : cache)
				{
					WriteStringIntoFile(handle, strPath);
					WriteStringIntoFile(handle, relativePath);
					WriteStringIntoFile(handle, absolutePath);
				}
			}

			for (char* pData : pTempMemory)
				delete[] pData;
		}

		g_pFullFileSystem->Close(handle);
		Msg(PROJECT_NAME ": successfully wrote searchcache file (%i)\n", searchCache.usedPaths);
	} else {
		Warning(PROJECT_NAME ": Failed to open searchcache file!\n");
	}
}

inline std::string_view* GetStringFromAbsoluteCache(const char* fileName, const char* pathID)
{
	if (!pathID)
		pathID = nullPath;

	if (!fileName)
		return NULL; // ??? can this even happen?

	auto pathIT = g_pAbsoluteSearchCache.find(pathID);
	if (pathIT == g_pAbsoluteSearchCache.end())
		return NULL;

	auto it = pathIT->second.find(fileName);
	if (it == pathIT->second.end())
		return NULL;

	return &it->second;
}

static void ClearAbsoluteSearchCache()
{
	VPROF_BUDGET("HolyLib - ClearAbsoluteSearchCache", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	for (auto& [key, val] : g_pAbsoluteSearchCache)
	{
		delete[] key.data(); // Free the memory.
		for (auto& [key2, val2] : val)
		{
			delete[] key2.data();
			delete[] val2.data();
		}
	}

	g_pAbsoluteSearchCache.clear();
}

static inline const char* ReadStringFromFile(FileHandle_t pHandle, unsigned char* nLength)
{
	g_pFullFileSystem->Read(nLength, sizeof(*nLength), pHandle);

	char* pValue = new char[*nLength + 1];
	g_pFullFileSystem->Read(pValue, *nLength, pHandle);
	pValue[*nLength] = '\0';

	return pValue;
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
			g_pFullFileSystem->Close(handle);
			Warning(PROJECT_NAME " - ReadSearchCache: Searchcache version didnt match  (File: %i, Current %i)\n", searchCache.version, SearchCacheVersion);
			return;
		}

		g_pAbsoluteSearchCache.reserve(searchCache.usedPaths);

		for (unsigned int i = 0; i < searchCache.usedPaths; ++i)
		{
			// PathID
			unsigned char pathIDLength;
			const char* pathID = ReadStringFromFile(handle, &pathIDLength);

			// relative file path
			unsigned char pathLength;
			const char* path = ReadStringFromFile(handle, &pathLength);

			// full file path
			unsigned char absolutePathLength;
			const char* absolutePath = ReadStringFromFile(handle, &absolutePathLength);
			
			std::string_view pathIDStr(pathID, pathIDLength); // NOTE: We have to manually free it later
			std::string_view pathStr(path, pathLength); // NOTE: We have to manually free it later
			std::string_view absolutePathStr(absolutePath, absolutePathLength); // NOTE: We have to manually free it later
			g_pAbsoluteSearchCache[pathIDStr][pathStr] = absolutePathStr;
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
		Msg("	\"%s\":\n", key.data());
		for (auto&[relativeFilePath, fullFilePath] : val)
		{
			Msg("		\"%s\": %s\n", relativeFilePath.data(), fullFilePath.data());
		}
	}
}
static ConCommand dumpabsolutesearchcache("holylib_filesystem_dumpabsolutesearchcache", DumpSearchCacheCmd, "Dumps the absolute search cache", 0);

static bool bShutdown = false;
static bool bFileSystemInited = false;

static void InitFileSystem(IFileSystem* pFileSystem)
{		
	if (!pFileSystem || bShutdown || bFileSystemInited) // We refuse to init when this is called when it shouldn't. If it crashes, then give me a stacktrace to fix it.
		return;

	g_pFullFileSystem = pFileSystem;
	bFileSystemInited = true;

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

	bool bIsAbsolute = V_IsAbsolutePath(pFileName);
	if (!bIsAbsolute)
	{
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
	}

	long time = detour_CBaseFileSystem_FastFileTime.GetTrampoline<Symbols::CBaseFileSystem_FastFileTime>()(filesystem, path, pFileName);

	if (time != 0L && !bIsAbsolute)
		AddFileToSearchCache(pFileName, path->m_storeId, path->GetPathIDString());

	return time;
}

static bool is_file(const char *path) {
	const char *last_slash = strrchr(path, FILEPATH_SLASH_CHAR);
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
	size_t firstSlash = fileName.find_first_of(FILEPATH_SLASH_CHAR);
	if (firstSlash == std::string::npos)
		return false;

	size_t lastSlash = fileName.find_last_of(FILEPATH_SLASH_CHAR);
	if (lastSlash == std::string::npos || firstSlash == lastSlash)
		return false;

	return true;
}

static const char* GetSplitPath(const char* pFileName, const char* pathID)
{
	if (!holylib_filesystem_splitgamepath.GetBool())
		return NULL;

	// We only enable split path for anything on the GAME path.
	if (!pathID || V_stricmp("GAME", pathID) != 0)
		return NULL;

	std::string_view strFileName = pFileName;
	std::size_t pos = strFileName.find_first_of(FILEPATH_SLASH_CHAR);
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
		if (strFileName.rfind("lua" FILEPATH_SLASH "includes" FILEPATH_SLASH) == 0)
			return "LUA_INCLUDES";

		if (strFileName.rfind("sandbox" FILEPATH_SLASH) == 0)
			return "LUA_GAMEMODE_SANDBOX";

		if (strFileName.rfind("effects" FILEPATH_SLASH) == 0)
			return "LUA_EFFECTS";

		if (strFileName.rfind("entities" FILEPATH_SLASH) == 0)
			return "LUA_ENTITIES";

		if (strFileName.rfind("weapons" FILEPATH_SLASH) == 0)
			return "LUA_WEAPONS";

		if (strFileName.rfind("lua" FILEPATH_SLASH "derma" FILEPATH_SLASH) == 0)
			return "LUA_DERMA";

		if (strFileName.rfind("lua" FILEPATH_SLASH "drive" FILEPATH_SLASH) == 0)
			return "LUA_DRIVE";

		if (strFileName.rfind("lua" FILEPATH_SLASH "entities" FILEPATH_SLASH) == 0)
			return "LUA_LUA_ENTITIES"; // Why LUA_LUA?!?

		if (strFileName.rfind("vgui" FILEPATH_SLASH) == 0)
			return "LUA_VGUI";

		if (strFileName.rfind("postprocess" FILEPATH_SLASH) == 0)
			return "LUA_POSTPROCESS";

		if (strFileName.rfind("matproxy" FILEPATH_SLASH) == 0)
			return "LUA_MATPROXY";

		if (strFileName.rfind("autorun" FILEPATH_SLASH) == 0)
			return "LUA_AUTORUN";
	}
	
	return NULL;
	*/
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
static Symbols::CBaseFileSystem_FixUpPath func_CBaseFileSystem_FixUpPath;
FileHandle_t hook_CBaseFileSystem_OpenForRead(CBaseFileSystem* filesystem, const char *pFileNameT, const char *pOptions, unsigned flags, const char *pathID, char **ppszResolvedFilename)
{
	VPROF_BUDGET("HolyLib - CBaseFileSystem::OpenForRead", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	char pFileNameBuff[MAX_PATH];
	const char *pFileName = pFileNameBuff;

	func_CBaseFileSystem_FixUpPath(filesystem, pFileNameT, pFileNameBuff, sizeof(pFileNameBuff));

	if (holylib_filesystem_savesearchcache.GetBool())
	{
		std::string_view* absoluteStr = GetStringFromAbsoluteCache(pFileName, pathID);
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
	const char* newPath = GetSplitPath(pFileName, pathID);
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
			std::string_view* absoluteStr = GetStringFromAbsoluteCache(pFileName, pathID);
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
static std::string_view fixGamemodePath(std::string_view path)
{
	// BUG: I have no idea why... previously we passed filesystem as an agument
	// that somehow corrupted itself, using g_pFullFileSystem though goes completely fine???

	// Just debug stuff... The one line does these three things at once
	//Gamemode::System* pGamemodeSystem = g_pFullFileSystem->Gamemodes();
	//const IGamemodeSystem::UpdatedInformation& pActiveGamemode = (const IGamemodeSystem::UpdatedInformation&)pGamemodeSystem->Active();
	//std::string_view activeGamemode = pActiveGamemode.name;
	std::string_view activeGamemode = ((const IGamemodeSystem::UpdatedInformation&)g_pFullFileSystem->Gamemodes()->Active()).name;
	if (activeGamemode.empty())
		return path;

	if (path.rfind("gamemodes" FILEPATH_SLASH) == 0)
		return path;

	std::string searchStr = FILEPATH_SLASH;
	searchStr.append(activeGamemode);
	searchStr.append(FILEPATH_SLASH "gamemode" FILEPATH_SLASH); // Final string should be /[Active Gamemode]/gamemode/
	size_t pos = path.find(searchStr);
	if (pos == std::string::npos)
		return path;

	if (g_pFileSystemModule.InDebug())
		Msg("fixGamemodePath: Fixed up path. (%s -> %s)\n", path.data(), path.substr(pos + 1).data());

	return path.substr(pos + 1);
}

static Detouring::Hook detour_CBaseFileSystem_GetFileTime;
static long hook_CBaseFileSystem_GetFileTime(IFileSystem* filesystem, const char *pFileNameT, const char *pPathID)
{
	VPROF_BUDGET("HolyLib - CBaseFileSystem::GetFileTime", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	// Fixes GetFileTime missing the caches since entries have different slashes
	char pFileNameBuff[MAX_PATH];
	const char *pFileName = pFileNameBuff;

	func_CBaseFileSystem_FixUpPath(filesystem, pFileNameT, pFileNameBuff, sizeof(pFileNameBuff));
	
	bool bSplitPath = false;
	const char* origPath = pPathID;
	const char* newPath = GetSplitPath(pFileName, pPathID);
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
		strFileName = fixGamemodePath(strFileName);
	}
	pFileName = strFileName.data();

	if (holylib_filesystem_skipinvalidluapaths.GetBool())
	{
		if (strFileName.rfind("include" FILEPATH_SLASH "include" FILEPATH_SLASH) == 0)
			return 0L;
	}

	if (holylib_filesystem_forcepath.GetBool())
	{
		newPath = NULL;

		if (!newPath && strFileName.rfind("gamemodes" FILEPATH_SLASH "base") == 0)
			newPath = "MOD_WRITE";

		if (!newPath && strFileName.rfind("gamemodes" FILEPATH_SLASH "sandbox") == 0)
			newPath = "MOD_WRITE";

		if (!newPath && strFileName.rfind("gamemodes" FILEPATH_SLASH "terrortown") == 0)
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
		std::string_view* absoluteStr = GetStringFromAbsoluteCache(pFileName, pPathID);
		if (!absoluteStr && holylib_filesystem_tryalternativeluapath.GetBool())
		{
			size_t nPos = strFileName.find_first_of(FILEPATH_SLASH_CHAR);
			if (nPos != std::string_view::npos)
			{
				std::string_view altFileName = strFileName.substr(nPos + 1);
				if (g_pFileSystemModule.InDebug())
					Msg("holylib - GetFileTime: Trying alternative absolute path (%s -> %s)\n", strFileName.data(), altFileName.data());

				absoluteStr = GetStringFromAbsoluteCache(altFileName.data(), pPathID);
			}
		}

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

	if (!pPath) // :BRUH:
		return;

	// Below is not dead code. It's code to try to solve the map contents but it currently doesn't work.
	/*std::string_view extension = getFileExtension(pPath);
	if (extension == "bsp") {
		const char* pPathID = "__TEMP_MAP_PATH";
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, pPathID, addType);

		if (filesystem->IsDirectory("materials" FILEPATH_SLASH, pPathID))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_MATERIALS", addType);

		if (filesystem->IsDirectory("models" FILEPATH_SLASH, pPathID))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_MODELS", addType);
	
		if (filesystem->IsDirectory("sound" FILEPATH_SLASH, pPathID))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_SOUNDS", addType);
	
		if (filesystem->IsDirectory("maps" FILEPATH_SLASH, pPathID))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_MAPS", addType);
	
		if (filesystem->IsDirectory("resource" FILEPATH_SLASH, pPathID))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_RESOURCE", addType);

		if (filesystem->IsDirectory("scripts" FILEPATH_SLASH, pPathID))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_SCRIPTS", addType);

		if (filesystem->IsDirectory("cfg" FILEPATH_SLASH, pPathID))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_CONFIGS", addType);
		
		filesystem->RemoveSearchPath(pPath, pPathID);
	}*/

	std::string strPath = pPath;
	if (V_stricmp(pathID, "GAME") == 0)
	{
		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "materials").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_MATERIALS", addType);

		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "models").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_MODELS", addType);
	
		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "sound").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_SOUNDS", addType);
	
		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "maps").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_MAPS", addType);
	
		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "resource").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_RESOURCE", addType);

		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "scripts").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_SCRIPTS", addType);

		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "cfg").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "CONTENT_CONFIGS", addType);

		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "gamemodes").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_GAMEMODES", addType);

		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "lua" FILEPATH_SLASH "includes").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_INCLUDES", addType);
	}
	
	if(V_stricmp(pathID, "lsv") == 0 || V_stricmp(pathID, "GAME") == 0)
	{
		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "sandbox").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_GAMEMODE_SANDBOX", addType);

		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "effects").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_EFFECTS", addType);
	
		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "entities").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_ENTITIES", addType);

		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "weapons").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_WEAPONS", addType);

		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "lua" FILEPATH_SLASH "derma").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_DERMA", addType);

		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "lua" FILEPATH_SLASH "drive").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_DRIVE", addType);

		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "lua" FILEPATH_SLASH "entities").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_LUA_ENTITIES", addType);

		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "vgui").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_VGUI", addType);

		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "postprocess").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_POSTPROCESS", addType);

		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "matproxy").c_str()))
			detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(filesystem, pPath, "LUA_MATPROXY", addType);

		if (filesystem->IsDirectory((strPath + FILEPATH_SLASH "autorun").c_str()))
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

	if (!pPath)
		return;

	if (V_stricmp(pathID, "GAME") == 0)
	{
		std::string_view vpkPath = getVPKFile(pPath);
		detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, vpkPath.data(), addType);

		if (filesystem->IsDirectory("materials" FILEPATH_SLASH), vpkPath.data())
			detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, "CONTENT_MATERIALS", addType);

		if (filesystem->IsDirectory("models" FILEPATH_SLASH), vpkPath.data())
			detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, "CONTENT_MODELS", addType);
	
		if (filesystem->IsDirectory("sound" FILEPATH_SLASH), vpkPath.data())
			detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, "CONTENT_SOUNDS", addType);
	
		if (filesystem->IsDirectory("maps" FILEPATH_SLASH), vpkPath.data())
			detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, "CONTENT_MAPS", addType);
	
		if (filesystem->IsDirectory("resource" FILEPATH_SLASH), vpkPath.data())
			detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, "CONTENT_RESOURCE", addType);

		if (filesystem->IsDirectory("scripts" FILEPATH_SLASH), vpkPath.data())
			detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, "CONTENT_SCRIPTS", addType);

		if (filesystem->IsDirectory("cfg" FILEPATH_SLASH), vpkPath.data())
			detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, "CONTENT_CONFIGS", addType);

		if (filesystem->IsDirectory("gamemodes" FILEPATH_SLASH), vpkPath.data())
			detour_CBaseFileSystem_AddVPKFile.GetTrampoline<Symbols::CBaseFileSystem_AddVPKFile>()(filesystem, pPath, "LUA_GAMEMODES", addType);

		if (filesystem->IsDirectory("lua" FILEPATH_SLASH "includes" FILEPATH_SLASH), vpkPath.data())
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

void CFileSystemModule::Think(bool bSimulating)
{
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
			Warning(PROJECT_NAME ": Not enouth space for search paths! please report this.\n");

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
	AddOveridePath("cfg" FILEPATH_SLASH "server.cfg", "MOD_WRITE");
	AddOveridePath("cfg" FILEPATH_SLASH "banned_ip.cfg", "MOD_WRITE");
	AddOveridePath("cfg" FILEPATH_SLASH "banned_user.cfg", "MOD_WRITE");
	AddOveridePath("cfg" FILEPATH_SLASH "skill2.cfg", "MOD_WRITE");
	AddOveridePath("cfg" FILEPATH_SLASH "game.cfg", "MOD_WRITE");
	AddOveridePath("cfg" FILEPATH_SLASH "trusted_keys_base.txt", "MOD_WRITE");
	AddOveridePath("cfg" FILEPATH_SLASH "pure_server_minimal.txt", "MOD_WRITE");
	AddOveridePath("cfg" FILEPATH_SLASH "skill_manifest.cfg", "MOD_WRITE");
	AddOveridePath("cfg" FILEPATH_SLASH "skill.cfg", "MOD_WRITE");
	AddOveridePath("cfg" FILEPATH_SLASH "mapcycle.txt", "MOD_WRITE");

	AddOveridePath("stale.txt", "MOD_WRITE");
	AddOveridePath("garrysmod.ver", "MOD_WRITE");
	AddOveridePath("scripts" FILEPATH_SLASH "actbusy.txt", "MOD_WRITE");
	AddOveridePath("modelsounds.cache", "MOD_WRITE");
	AddOveridePath("lua" FILEPATH_SLASH "send.txt", "MOD_WRITE");

	AddOveridePath("resource" FILEPATH_SLASH "serverevents.res", "MOD_WRITE");
	AddOveridePath("resource" FILEPATH_SLASH "gameevents.res", "MOD_WRITE");
	AddOveridePath("resource" FILEPATH_SLASH "modevents.res", "MOD_WRITE");
	AddOveridePath("resource" FILEPATH_SLASH "hltvevents.res", "MOD_WRITE");

	int pBaseLength = 0;
	char pBaseDir[MAX_PATH];
	if ( pBaseLength < 3 )
		pBaseLength = g_pFullFileSystem->GetSearchPath( "BASE_PATH", true, pBaseDir, sizeof( pBaseDir ) );

	std::string workshopDir = pBaseDir;
	workshopDir.append("garrysmod" FILEPATH_SLASH "workshop");

	if (g_pFullFileSystem != NULL)
		InitFileSystem(g_pFullFileSystem);

	if (!DETOUR_ISVALID(detour_CBaseFileSystem_AddSearchPath))
	{
		Msg(PROJECT_NAME ": CBaseFileSystem::AddSearchPath detour is invalid?\n");
		return;
	}

	// NOTE: Check the thing below again and redo it. I don't like how it looks :<
	if (g_pFullFileSystem->IsDirectory("materials" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_MATERIALS", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("models" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_MODELS", SearchPathAdd_t::PATH_ADD_TO_TAIL);
	
	if (g_pFullFileSystem->IsDirectory("sound" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_SOUNDS", SearchPathAdd_t::PATH_ADD_TO_TAIL);
	
	if (g_pFullFileSystem->IsDirectory("maps" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_MAPS", SearchPathAdd_t::PATH_ADD_TO_TAIL);
	
	if (g_pFullFileSystem->IsDirectory("resource" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_RESOURCE", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("scripts" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_SCRIPTS", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("cfg" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_CONFIGS", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("gamemodes" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_GAMEMODES", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("lua" FILEPATH_SLASH "includes" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_INCLUDES", SearchPathAdd_t::PATH_ADD_TO_TAIL);
	
	if (g_pFullFileSystem->IsDirectory("sandbox" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_GAMEMODE_SANDBOX", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("effects" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_EFFECTS", SearchPathAdd_t::PATH_ADD_TO_TAIL);
	
	if (g_pFullFileSystem->IsDirectory("entities" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_ENTITIES", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("weapons" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_WEAPONS", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("lua" FILEPATH_SLASH "derma" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_DERMA", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("lua" FILEPATH_SLASH "drive" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_DRIVE", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("lua" FILEPATH_SLASH "entities" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_LUA_ENTITIES", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("vgui" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_VGUI", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("postprocess" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_POSTPROCESS", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("matproxy" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_MATPROXY", SearchPathAdd_t::PATH_ADD_TO_TAIL);

	if (g_pFullFileSystem->IsDirectory("autorun" FILEPATH_SLASH, "workshop"))
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

	if (!g_pPathIDTable)
		return NULL;

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
	if (!func_CBaseFileSystem_CSearchPath_GetDebugString)
		return NULL;

	return func_CBaseFileSystem_CSearchPath_GetDebugString((void*)this); // Look into this to possibly remove the GetDebugString function.
}

#if SYSTEM_WINDOWS
DETOUR_THISCALL_START()
	DETOUR_THISCALL_ADDRETFUNC5( hook_CBaseFileSystem_OpenForRead, FileHandle_t, OpenForRead, CBaseFileSystem*, const char*, const char*, unsigned, const char*, char** );
	DETOUR_THISCALL_ADDRETFUNC1( hook_CBaseFileSystem_FindFileInSearchPath, FileHandle_t, FindFileInSearchPath, CBaseFileSystem*, CFileOpenInfo& );
	DETOUR_THISCALL_ADDRETFUNC2( hook_CBaseFileSystem_IsDirectory, bool, IsDirectory, CBaseFileSystem*, const char*, const char* );
	DETOUR_THISCALL_ADDRETFUNC2( hook_CBaseFileSystem_FastFileTime, long, FastFileTime, CBaseFileSystem*, const CSearchPath*, const char* );
	DETOUR_THISCALL_ADDRETFUNC2( hook_CBaseFileSystem_GetFileTime, long, GetFileTime, CBaseFileSystem*, const char*, const char* );
	DETOUR_THISCALL_ADDFUNC1( hook_CBaseFileSystem_Close, Close, CBaseFileSystem*, FileHandle_t );
	DETOUR_THISCALL_ADDFUNC3( hook_CBaseFileSystem_AddSearchPath, AddSearchPath, CBaseFileSystem*, const char*, const char*, SearchPathAdd_t );
	DETOUR_THISCALL_ADDFUNC3( hook_CBaseFileSystem_AddVPKFile, AddVPKFile, CBaseFileSystem*, const char*, const char*, SearchPathAdd_t );
DETOUR_THISCALL_FINISH();
#endif

void CFileSystemModule::InitDetour(bool bPreServer)
{
	if (!bPreServer)
		return;

	bShutdown = false;
	if (holylib_filesystem_threads.GetInt() > 0)
	{
		pFileSystemPool = V_CreateThreadPool();
		Util::StartThreadPool(pFileSystemPool, holylib_filesystem_threads.GetInt());
	}

	if (g_pFullFileSystem != NULL)
		InitFileSystem(g_pFullFileSystem);

	// ToDo: Redo EVERY Hook so that we'll abuse the vtable instead of symbols.  
	// Use the ClassProxy or so which should also allow me to port this to windows.
#if SYSTEM_WINDOWS
	SourceSDK::FactoryLoader filesystem_loader("filesystem_stdio");
#else
	SourceSDK::FactoryLoader filesystem_loader("dedicated");
#endif
	

	// A total abomination to get the vtable so that we can pass the functions to use as hooks
	// I hate and absolutely love that this actually works
	DETOUR_PREPARE_THISCALL();
	Detour::Create(
		&detour_CBaseFileSystem_OpenForRead, "CBaseFileSystem::OpenForRead",
		filesystem_loader.GetModule(), Symbols::CBaseFileSystem_OpenForReadSym,
		(void*)DETOUR_THISCALL(hook_CBaseFileSystem_OpenForRead, OpenForRead), m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_FindFileInSearchPath, "CBaseFileSystem::FindFileInSearchPath",
		filesystem_loader.GetModule(), Symbols::CBaseFileSystem_FindFileInSearchPathSym,
		(void*)DETOUR_THISCALL(hook_CBaseFileSystem_FindFileInSearchPath, FindFileInSearchPath), m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_IsDirectory, "CBaseFileSystem::IsDirectory",
		filesystem_loader.GetModule(), Symbols::CBaseFileSystem_IsDirectorySym,
		(void*)DETOUR_THISCALL(hook_CBaseFileSystem_IsDirectory, IsDirectory), m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_FastFileTime, "CBaseFileSystem::FastFileTime",
		filesystem_loader.GetModule(), Symbols::CBaseFileSystem_FastFileTimeSym,
		(void*)DETOUR_THISCALL(hook_CBaseFileSystem_FastFileTime, FastFileTime), m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_GetFileTime, "CBaseFileSystem::GetFileTime",
		filesystem_loader.GetModule(), Symbols::CBaseFileSystem_GetFileTimeSym,
		(void*)DETOUR_THISCALL(hook_CBaseFileSystem_GetFileTime, GetFileTime), m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_Close, "CBaseFileSystem::Close",
		filesystem_loader.GetModule(), Symbols::CBaseFileSystem_CloseSym,
		(void*)DETOUR_THISCALL(hook_CBaseFileSystem_Close, Close), m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_AddSearchPath, "CBaseFileSystem::AddSearchPath",
		filesystem_loader.GetModule(), Symbols::CBaseFileSystem_AddSearchPathSym,
		(void*)DETOUR_THISCALL(hook_CBaseFileSystem_AddSearchPath, AddSearchPath), m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_AddVPKFile, "CBaseFileSystem::AddVPKFile",
		filesystem_loader.GetModule(), Symbols::CBaseFileSystem_AddVPKFileSym,
		(void*)DETOUR_THISCALL(hook_CBaseFileSystem_AddVPKFile, AddVPKFile), m_pID
	);

#if SYSTEM_LINUX
	// ToDo: Find symbols for this function :/
	// NOTE: It's probably easier to recreate the filesystem class since the function isn't often used in the engine and there aren't any good ways to find it :/ (Maybe some function declared before or after it can be found and then I'll can search neat that?)
	func_CBaseFileSystem_FindSearchPathByStoreId = (Symbols::CBaseFileSystem_FindSearchPathByStoreId)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::CBaseFileSystem_FindSearchPathByStoreIdSym);
	Detour::CheckFunction((void*)func_CBaseFileSystem_FindSearchPathByStoreId, "CBaseFileSystem::FindSearchPathByStoreId");
#endif

	func_CBaseFileSystem_CSearchPath_GetDebugString = (Symbols::CBaseFileSystem_CSearchPath_GetDebugString)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::CBaseFileSystem_CSearchPath_GetDebugStringSym);
	Detour::CheckFunction((void*)func_CBaseFileSystem_CSearchPath_GetDebugString, "CBaseFileSystem::CSearchPath::GetDebugString");

	func_CBaseFileSystem_FixUpPath = (Symbols::CBaseFileSystem_FixUpPath)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::CBaseFileSystem_FixUpPathSym);
	Detour::CheckFunction((void*)func_CBaseFileSystem_FixUpPath, "CBaseFileSystem::FixUpPath");

#if ARCHITECTURE_IS_X86
	g_pPathIDTable = Detour::ResolveSymbol<CUtlSymbolTableMT>(filesystem_loader, Symbols::g_PathIDTableSym);
#else
	g_pPathIDTable = Detour::ResolveSymbolFromLea<CUtlSymbolTableMT>(filesystem_loader.GetModule(), Symbols::g_PathIDTableSym);
#endif
	Detour::CheckValue("get class", "g_PathIDTable", g_pPathIDTable != NULL);
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
	Util::DoUnsafeCodeCheck(LUA);
	// We don't have GMods file whitelist/blacklist and if you call this just use file.Open

	const char* fileName = LUA->CheckString(1);
	const char* gamePath = LUA->CheckString(2);
	LUA->CheckType(3, GarrysMod::Lua::Type::Function);
	LUA->Push(3);
	int reference = Util::ReferenceCreate(LUA, "filesystem.AsyncRead");
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

void FileAsyncReadThink(GarrysMod::Lua::ILuaInterface* pLua)
{
	std::vector<IAsyncFile*> files;
	for(IAsyncFile* file : asyncCallback) {
		Util::ReferencePush(pLua, file->callback);
		pLua->PushString(file->req->pszFilename);
		pLua->PushString(file->req->pszPathID);
		pLua->PushNumber(file->status);
		pLua->PushString(file->content);
		pLua->CallFunctionProtected(4, 0, true);
		Util::ReferenceFree(pLua, file->callback, "FileAsyncReadThink");
		files.push_back(file);
	}

	asyncCallback.clear();
}

LUA_FUNCTION_STATIC(filesystem_CreateDir)
{
	g_pFullFileSystem->CreateDirHierarchy(LUA->CheckString(1), 
		g_pModuleManager.IsUnsafeCodeEnabled() ? LUA->CheckStringOpt(2, "DATA") : "DATA" // Force "DATA" path if unsafe is diabled
	);

	return 0;
}

LUA_FUNCTION_STATIC(filesystem_Delete)
{
	g_pFullFileSystem->RemoveFile(LUA->CheckString(1),
		g_pModuleManager.IsUnsafeCodeEnabled() ? LUA->CheckStringOpt(2, "DATA") : "DATA" // Force "DATA" path if unsafe is diabled
	);

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
	const char* gamePath = LUA->CheckString(2);
	const char* sorting = LUA->CheckStringOpt(3, "");

	FileFindHandle_t findHandle;
	const char *pFilename = g_pFullFileSystem->FindFirstEx(filepath, gamePath, &findHandle);
	while (pFilename)
	{
		if (g_pFullFileSystem->IsDirectory(((std::string)filepath + pFilename).c_str(), gamePath)) {
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
			SortByDate(files, filepath, gamePath, true);
			SortByDate(folders, filepath, gamePath, true);
		} else if (strcmp(sorting, "datedesc") == 0) { // sort the files descending by date.
			SortByDate(files, filepath, gamePath, false);
			SortByDate(folders, filepath, gamePath, false);
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
	Util::DoUnsafeCodeCheck(LUA);
	// We don't have GMods file whitelist/blacklist and if you call this just use file.Open

	const char* filename = LUA->CheckString(1);
	const char* fileMode = LUA->CheckString(2);
	const char* gamePath = LUA->CheckStringOpt(3, "GAME");

	FileHandle_t fh = g_pFullFileSystem->Open(filename, fileMode, gamePath);
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

	if (!g_pModuleManager.IsUnsafeCodeEnabled())
		gamePath = "DATA"; // Force "DATA" path if unsafe is diabled

	LUA->PushBool(g_pFullFileSystem->RenameFile(original, newname, gamePath));

	return 1;
}

LUA_FUNCTION_STATIC(filesystem_Size)
{
	LUA->PushNumber((double)g_pFullFileSystem->Size(LUA->CheckString(1), LUA->CheckStringOpt(2, "GAME")));

	return 1;
}

LUA_FUNCTION_STATIC(filesystem_Time)
{
	LUA->PushNumber(g_pFullFileSystem->GetFileTime(LUA->CheckString(1), LUA->CheckStringOpt(2, "GAME")));

	return 1;
}

LUA_FUNCTION_STATIC(filesystem_AddSearchPath)
{
	Util::DoUnsafeCodeCheck(LUA);

	const char* folderPath = LUA->CheckString(1);
	const char* gamePath = LUA->CheckString(2);
	SearchPathAdd_t addType = LUA->GetBool(-1) ? SearchPathAdd_t::PATH_ADD_TO_HEAD : SearchPathAdd_t::PATH_ADD_TO_TAIL;
	g_pFullFileSystem->AddSearchPath(folderPath, gamePath, addType);

	return 0;
}

LUA_FUNCTION_STATIC(filesystem_RemoveSearchPath)
{
	Util::DoUnsafeCodeCheck(LUA);

	const char* folderPath = LUA->CheckString(1);
	const char* gamePath = LUA->CheckString(2);
	LUA->PushBool(g_pFullFileSystem->RemoveSearchPath(folderPath, gamePath));

	return 1;
}

LUA_FUNCTION_STATIC(filesystem_RemoveSearchPaths)
{
	Util::DoUnsafeCodeCheck(LUA);

	const char* gamePath = LUA->CheckString(1);
	g_pFullFileSystem->RemoveSearchPaths(gamePath);

	return 0;
}

LUA_FUNCTION_STATIC(filesystem_RemoveAllSearchPaths)
{
	Util::DoUnsafeCodeCheck(LUA);

	g_pFullFileSystem->RemoveAllSearchPaths();

	return 0;
}

LUA_FUNCTION_STATIC(filesystem_RelativePathToFullPath)
{
	Util::DoUnsafeCodeCheck(LUA);

	const char* filePath = LUA->CheckString(1);
	const char* gamePath = LUA->CheckString(2);

	char outStr[MAX_PATH];
	g_pFullFileSystem->RelativePathToFullPath(filePath, gamePath, outStr, MAX_PATH);

	LUA->PushString(outStr);

	return 1;
}

LUA_FUNCTION_STATIC(filesystem_FullPathToRelativePath)
{
	Util::DoUnsafeCodeCheck(LUA);

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

inline Addon::FileSystem* GetAddonFilesystem()
{
	return g_pFullFileSystem->Addons();
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
	//const char* strGMAPath = LUA->CheckString(1);

	std::vector<std::string> files;
	//LUA->PushNumber(GetAddonFilesystem()->MountFile(strGMAPath, &files, 0, 0, !?));

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
	const char* workshopID64 = LUA->CheckString(1);
	uint64 workshopID = strtoull(workshopID64, NULL, 0);
	LUA->PushBool(GetAddonFilesystem()->ShouldMount(workshopID));

	return 1;
}

LUA_FUNCTION_STATIC(addonsystem_SetShouldMount)
{
	const char* workshopID64 = LUA->CheckString(1);
	uint64 workshopID = strtoull(workshopID64, NULL, 0);
	bool bMount = LUA->GetBool(2);
	GetAddonFilesystem()->SetShouldMount(workshopID, bMount);

	return 0;
}

// Gmod's filesystem functions have some weird stuff in them that makes them noticeably slower :/
void CFileSystemModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Util::StartTable(pLua);
		Util::AddFunc(pLua, filesystem_AsyncRead, "AsyncRead");
		Util::AddFunc(pLua, filesystem_CreateDir, "CreateDir");
		Util::AddFunc(pLua, filesystem_Delete, "Delete");
		Util::AddFunc(pLua, filesystem_Exists, "Exists");
		Util::AddFunc(pLua, filesystem_Find, "Find");
		Util::AddFunc(pLua, filesystem_IsDir, "IsDir");
		Util::AddFunc(pLua, filesystem_Open, "Open");
		Util::AddFunc(pLua, filesystem_Rename, "Rename");
		Util::AddFunc(pLua, filesystem_Size, "Size");
		Util::AddFunc(pLua, filesystem_Time, "Time");

		// Custom functions
		Util::AddFunc(pLua, filesystem_AddSearchPath, "AddSearchPath");
		Util::AddFunc(pLua, filesystem_RemoveSearchPath, "RemoveSearchPath");
		Util::AddFunc(pLua, filesystem_RemoveSearchPaths, "RemoveSearchPaths");
		Util::AddFunc(pLua, filesystem_RemoveAllSearchPaths, "RemoveAllSearchPaths");
		Util::AddFunc(pLua, filesystem_RelativePathToFullPath, "RelativePathToFullPath");
		Util::AddFunc(pLua, filesystem_FullPathToRelativePath, "FullPathToRelativePath");
		Util::AddFunc(pLua, filesystem_TimeCreated, "TimeCreated");
		Util::AddFunc(pLua, filesystem_TimeAccessed, "TimeAccessed");
	Util::FinishTable(pLua, "filesystem");

	Util::StartTable(pLua);
		Util::AddFunc(pLua, addonsystem_Clear, "Clear");
		Util::AddFunc(pLua, addonsystem_Refresh, "Refresh");
		Util::AddFunc(pLua, addonsystem_MountFile, "MountFile");
		Util::AddFunc(pLua, addonsystem_ShouldMount, "ShouldMount");
		Util::AddFunc(pLua, addonsystem_SetShouldMount, "SetShouldMount");
	Util::FinishTable(pLua, "addonsystem");
}

void CFileSystemModule::LuaThink(GarrysMod::Lua::ILuaInterface* pLua)
{
	FileAsyncReadThink(pLua);
}

void CFileSystemModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "filesystem");
}

void CFileSystemModule::Shutdown()
{
	if (pFileSystemPool)
	{
		pFileSystemPool->ExecuteAll();
		Util::DestroyThreadPool(pFileSystemPool);
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