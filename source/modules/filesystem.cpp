#include <sourcesdk/filesystem_things.h>
#include "GarrysMod/IGamemodeSystem.h"
#include "GarrysMod/IAddonSystem.h"
#undef Yield
#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include <algorithm>
#include <cstring>
#include "edict.h"

#include <isteamugc.h>
#include "sourcesdk/baseserver.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CFileSystemModule : public IModule
{
public:
	void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) override;
	void InitDetour(bool bPreServer) override;
	void Think(bool bSimulating) override;
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaThink(GarrysMod::Lua::ILuaInterface* pLua) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void Shutdown() override;
	void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax) override;
	const char* Name() override { return "filesystem"; };
	int Compatibility() override { return LINUX32 | WINDOWS32; };
	bool SupportsMultipleLuaStates() override { return true; };
};

static CFileSystemModule g_pFileSystemModule;
IModule* pFileSystemModule = &g_pFileSystemModule;

static ConVar holylib_filesystem_easydircheck("holylib_filesystem_easydircheck", "0", FCVAR_ARCHIVE, 
	"Checks if the folder CBaseFileSystem::IsDirectory checks has a . in the name after the last /. if so assume it's a file extension.");
static ConVar holylib_filesystem_searchcache("holylib_filesystem_searchcache", "1", FCVAR_ARCHIVE, 
	"If enabled, it will cache the search path a file was located in and if the same file is requested, it will use that search path directly.");
static ConVar holylib_filesystem_earlysearchcache("holylib_filesystem_earlysearchcache", "1", FCVAR_ARCHIVE, 
	"If enabled, it will check early in CBaseFilesystem::OpenForRead if the file is in the search cache.");
static ConVar holylib_filesystem_fixgmodpath("holylib_filesystem_fixgmodpath", "1", FCVAR_ARCHIVE, 
	"If enabled, it will fix up weird gamemode paths like sandbox/gamemode/sandbox/gamemode which gmod likes to use.");
static ConVar holylib_filesystem_savesearchcache("holylib_filesystem_savesearchcache", "1", FCVAR_ARCHIVE,
	"If enabled, it will write the search cache into a file and restore it when starting, using it to improve performance.");
static ConVar holylib_filesystem_mergesearchcache("holylib_filesystem_mergesearchcache", "0", FCVAR_ARCHIVE,
	"If enabled, when saving the search cache it will not remove old entries and instead keep them even if they were unused this session");
static ConVar holylib_filesystem_skipinvalidluapaths("holylib_filesystem_skipinvalidluapaths", "1", FCVAR_ARCHIVE,
	"If enabled, invalid lua paths like include/include/ will be skipped instantly");
static ConVar holylib_filesystem_tryalternativeluapath("holylib_filesystem_tryalternativeluapath", "1", FCVAR_ARCHIVE,
	"If enabled, if it can't find a file in the search cache, it will remove the first folder and try again as when loading Lua gmod loves to test different folders first");

// Optimization Idea: When Gmod calls GetFileTime, we could try to get the filehandle in parallel to have it ready when gmod calls it.
// We could also cache every FULL searchpath to not have to look up a file every time.  

static IThreadPool* pFileSystemPool = nullptr;

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
			pFileSystemPool = nullptr;
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
	void* pData = nullptr;
};

#if SYSTEM_WINDOWS
#define FILEPATH_SLASH "\\"
#define FILEPATH_SLASH_CHAR '\\'
#else
#define FILEPATH_SLASH "/"
#define FILEPATH_SLASH_CHAR '/'
#endif

static void DeleteFileHandle(FileHandle_t handle);
static inline CSearchPath* FindSearchPathByStoreId(int iStoreID);

// VS2022 falsely claims the out buffer may not be null terminated...
static FORCEINLINE void GetFullPath(const CSearchPath* pSearchPath, const char* strFileName, char (&out)[MAX_PATH])
{
	V_strcpy_safe(out, pSearchPath->GetPathString());
	size_t len = strlen(out);
	V_strncpy(out + len, strFileName, sizeof(out) - len);
	V_strlower(out + len);
}

/*
	Cached Filesystem
*/

class CachedFileSystem;
class CacheResult
{
public:
	FORCEINLINE const CSearchPath* GetSearchPath()
	{
		return m_pPath;
	}

	// We may have found an entry, doesn't mean it's valid.
	// We also support missing files on disk / to skip checking the disk.
	FORCEINLINE bool IsValid()
	{
		return m_bIsValid;
	}

	FORCEINLINE const char* GetAbsolutePath()
	{
		return m_strAbsolute;
	}

	FORCEINLINE unsigned short GetAbsolutePathLength()
	{
		return m_nAbsoluteLength;
	}

private:
	friend class CachedFileSystem;

	const CSearchPath* m_pPath;
	const char* m_strAbsolute;
	unsigned short m_nAbsoluteLength; // includes NULL terminator!
	bool m_bIsValid;
};
// I don't feel like dealing with many return values soo let's do it the lazy way xd
// Though we must keep it small due to the TLS limits!
static thread_local CacheResult g_pCacheResult;

/*
	ToDo:
		The new filesystem should learn and save behavior files to know what to do.
		1) Find most accessed folders and in all paths
		   -> Using this it then should be able to add search paths like CONTENT_MATERIAL without them being hard coded
		2) Prioritize most accessed files
		   -> If a file is frequently accessed it should be able to notice and somehow put it earlier in the cache
		3) Misses on disk
		   -> We should be able to avoid disk lookups when we know it does not exist
		   -> CHECK: This must work with autorefresh! Maybe hook into it to clear cache valid flag when autorefresh hits?
		3) Override search paths for lookups when possible
		   -> If a file is always on disk and for example always looked up with "GAME" when it's the stale.txt
		   -> it should be able to figure it out and by itself decide to override the path to "MOD_WRITE" or whatever path is the best to look it up
		   -> Best case: It doesn't override path though instead just directly opens it and returns the handle avoiding further filesystem work entirely
		4) Disk Saves
		   -> It should save onto disk what order files are accessed at and we must also dump search paths and priorities to never break expected order!
		   -> It should also save a filesystem state dictating stuff like which search paths should be created on startup and so on
		5) Assume static files
		   -> We can assume that files from GMod only change when garrysmod.ver changed allowing us to partially implement a static filesystem across reboots
		6) Priorities
		   -> We shoul store and account for priorities- if a new search path is added to the head of a gamepath it should take priority!

	Important:
		It should always assume that on server startup the filesystem may have entirely changed
		So never assume that a file may be missing on disk without having checked once!

		We could though cache VPC files and we could cache GMod version and have a convar like _staticfilesystem?
		Needs more thought on how to handle this!

	Design choices:
		No usage of std::string- instead we use std::string_view which is performance and memory wise way better.
		We just have to manage the memory ourselves right which is easyyyy (I'll hate myself in a year)
*/

static const char* nullPath = "NULL_PATH";
class CachedFileSystem
{
public:
	~CachedFileSystem()
	{
	
	}

	// This is the main call- we aren't iterating search paths yet.
	bool FindCacheEntry(const char* pFileName, const char* pGamePath)
	{
		if (!pGamePath)
			pGamePath = nullPath;

		std::shared_lock<std::shared_mutex> lock(m_CacheMutex);
		FileEntry* pFileEntry = GetFileEntry(pFileName, pGamePath);
		if (!pFileEntry)
			return false;

		pFileEntry->m_nAccessCount.fetch_add(1);
		g_pCacheResult.m_bIsValid = pFileEntry->m_bIsValid.load();
		g_pCacheResult.m_strAbsolute = pFileEntry->m_strAbsolutePath.data();

		// + 1 for null terminator!
		g_pCacheResult.m_nAbsoluteLength = (unsigned short)pFileEntry->m_strAbsolutePath.length() + 1;
		return true;
	}

	// This is the smaller one- we check for this specific search path
	bool FindCacheEntry(const char* pFileName, const CSearchPath* pPath)
	{
		if (!pPath)
			return false;

		std::shared_lock<std::shared_mutex> lock(m_CacheMutex);
		
		auto foundEntry = m_Cache.find(pPath->m_storeId);
		if (foundEntry == m_Cache.end())
			return false;

		CacheEntry* pCacheEntry = foundEntry->second;
		FileEntry* pFileEntry = pCacheEntry->FindFile(pFileName);
		if (!pFileEntry)
			return false;

		pFileEntry->m_nAccessCount.fetch_add(1);
		g_pCacheResult.m_bIsValid = pFileEntry->m_bIsValid.load();
		g_pCacheResult.m_pPath = pPath;
		return true;
	}

	void AddFileToCache(const char* pFileName, const CSearchPath* pPath, const char* pOptions)
	{
		if (!pPath)
			return;

		std::unique_lock<std::shared_mutex> lock(m_CacheMutex);
		CacheEntry* pCacheEntry;
		auto foundEntry = m_Cache.find(pPath->m_storeId);
		if (foundEntry == m_Cache.end())
		{
			pCacheEntry = new CacheEntry;
			m_Cache[pPath->m_storeId] = pCacheEntry;
		} else
			pCacheEntry = foundEntry->second;

		if (pOptions && *pOptions == 'w')
			pCacheEntry->MarkWrite();

		std::string_view permanentFileName;
		FileEntry* pFileEntry = pCacheEntry->AddFile(pFileName, &permanentFileName);
		
		char pBuffer[MAX_PATH];
		GetFullPath(pPath, pFileName, pBuffer);
		pFileEntry->SetAbsolutePath(pBuffer);

		unordered_map<std::string_view, FileEntry*>* pFileMap;
		const char* pGamePath = pPath->GetPathIDString();
		auto gamePathIT = m_GamePathCache.find(pGamePath);
		if (gamePathIT == m_GamePathCache.end())
			pFileMap = &m_GamePathCache.emplace(pGamePath, unordered_map<std::string_view, FileEntry*>()).first->second;
		else
			pFileMap = &gamePathIT->second;

		auto fileMapIT = pFileMap->find(permanentFileName);
		if (fileMapIT == pFileMap->end())
			(*pFileMap)[permanentFileName] = pFileEntry;

		Msg("Added file %s to cache (%s)\n", pFileName, pBuffer);

		// ToDo: Let the search paths fight for priority!
	}

	// Call this when you encounter a faulty cache result!
	void InvalidateFileFromCache(const char* pFileName, const CSearchPath* pPath)
	{
		if (!pPath)
			return;

		std::unique_lock<std::shared_mutex> lock(m_CacheMutex);
		CacheEntry* pCacheEntry;
		auto foundEntry = m_Cache.find(pPath->m_storeId);
		if (foundEntry == m_Cache.end())
		{
			pCacheEntry = new CacheEntry;
			m_Cache[pPath->m_storeId] = pCacheEntry;
		} else
			pCacheEntry = foundEntry->second;

		std::string_view permanentFileName;
		FileEntry* pFileEntry = pCacheEntry->AddFile(pFileName, &permanentFileName);
		pFileEntry->MarkMissing();
		// Invalid entries aren't added here to m_GamePathCache as this is specific to this search path.
		// There may be more, MarkFileMissing will be the one to add it if it's missing everywhere.
		// Update:
		// I need to think more, it would be too hacky to add it inside MarkFileMissing
		// So for now, we do it here

		unordered_map<std::string_view, FileEntry*>* pFileMap;
		const char* pGamePath = pPath->GetPathIDString();
		auto gamePathIT = m_GamePathCache.find(pGamePath);
		if (gamePathIT == m_GamePathCache.end())
			pFileMap = &m_GamePathCache.emplace(pGamePath, unordered_map<std::string_view, FileEntry*>()).first->second;
		else
			pFileMap = &gamePathIT->second;

		auto fileMapIT = pFileMap->find(permanentFileName);
		if (fileMapIT == pFileMap->end())
			(*pFileMap)[permanentFileName] = pFileEntry;
	}

	void MarkFileMissing(const char* pFileName, const char* pGamePath)
	{
		if (!pGamePath)
			pGamePath = nullPath;

		std::unique_lock<std::shared_mutex> lock(m_CacheMutex);
		FileEntry* pFileEntry = GetFileEntry(pFileName, pGamePath);
		if (!pFileEntry)
			return;

		pFileEntry->MarkMissing();
		Msg("Marked file missing %s - %s\n", pFileName, pGamePath);
	}

	bool ShouldCloseHandle(FileHandle_t pHandle)
	{
		return true;
	}

	void Shutdown()
	{
	
	}

	void Init()
	{
	
	}

	// Main Thread Think
	void MainThink()
	{
	
	}

	void Nuke()
	{
		std::unique_lock<std::shared_mutex> lock(m_CacheMutex);
		m_Cache.clear();
	}

	void Dump()
	{
		std::shared_lock<std::shared_mutex> lock(m_CacheMutex);
		Msg("Game Cache:\n");
		for (auto& [gamePath, fileList] : m_GamePathCache)
		{
			Msg("-> \"%s\":\n", gamePath.data());
			for (auto& [fileName, fileEntry] : fileList)
				Msg("	\"%s\" -> \"%s\"\n", fileName.data(), fileEntry->m_bIsValid.load() ? fileEntry->m_strAbsolutePath.data() : "MISSING");
		}
	}

private:
	static constexpr const char* pInvalidAbsolutePath = "";
	// We got one expectation here... A FileEntry is never deleted.
	struct FileEntry
	{
		// If false then the file is known to not exist.
		std::atomic<bool> m_bIsValid{true};
		std::atomic<size_t> m_nAccessCount{0};
		const std::string_view m_strAbsolutePath = pInvalidAbsolutePath;

		~FileEntry()
		{
			if (m_strAbsolutePath.data() != pInvalidAbsolutePath)
				delete[] m_strAbsolutePath.data();
		}

		void SetAbsolutePath(const std::string_view pFileName)
		{
			// Once it was set it should never change!
			if (m_strAbsolutePath.data() != pInvalidAbsolutePath)
				return;

			const size_t nFileNameLength = pFileName.length() + 1;
			char* pFile = new char[nFileNameLength];
			V_strncpy(pFile, pFileName.data(), nFileNameLength);

			//if (m_strAbsolutePath.data() != pInvalidAbsolutePath)
			//	delete[] m_strAbsolutePath.data();

			// We only made m_strAbsolutePath const so that no other place will touch it!
			*((std::string_view*)&m_strAbsolutePath) = std::string_view(pFile, pFileName.length());
		}

		FORCEINLINE void MarkMissing()
		{
			m_bIsValid.store(false);
		}

		FORCEINLINE void MarkExisting()
		{
			m_bIsValid.store(true);
		}
	};

	// A Entry always belongs to a search path!
	class CacheEntry
	{
	public:
		~CacheEntry()
		{
			for (auto& [pFile, pFileEntry] : m_pFiles)
				delete[] pFile.data();
		}

		FORCEINLINE FileEntry* FindFile(std::string_view pFileName)
		{
			auto it = m_pFiles.find(pFileName);
			if (it != m_pFiles.end())
				return it->second;

			return nullptr;
		}

		// pOutput = new std::string_view containing a pointer to a newly allocated const char* that won't be freed
		FORCEINLINE FileEntry* AddFile(std::string_view pFileName, std::string_view* pOutput)
		{
			auto it = m_pFiles.find(pFileName);
			if (it != m_pFiles.end())
			{
				*pOutput = it->first;
				return it->second;
			}

			const size_t nFileNameLength = pFileName.length() + 1;
			char* pFile = new char[nFileNameLength];
			V_strncpy(pFile, pFileName.data(), nFileNameLength);

			// IMPORTANT: We do not include the null terminator in std::string_view!
			*pOutput = std::string_view(pFile, pFileName.length());

			FileEntry* pEntry = new FileEntry;
			m_pFiles[*pOutput] = pEntry;
			return pEntry;
		}

		FORCEINLINE const unordered_map<std::string_view, FileEntry*>& GetFiles()
		{
			return m_pFiles;
		}

		FORCEINLINE void MarkWrite()
		{
			m_bCanWrite = true;
		}

	private:
		std::string_view m_strGamePath;
		unordered_map<std::string_view, FileEntry*> m_pFiles;
		bool m_bCanWrite = false;
	};

	FORCEINLINE FileEntry* GetFileEntry(const char* pFileName, const char* pGamePath)
	{
		auto fileList = m_GamePathCache.find(pGamePath);
		if (fileList == m_GamePathCache.end())
			return nullptr;

		auto fileEntry = fileList->second.find(pFileName);
		if (fileEntry == fileList->second.end())
			return nullptr;
		
		return fileEntry->second;
	}

	// key = CSearchPath::m_storeId
	unordered_map<int, CacheEntry*> m_Cache;

	// key = game path, set = all files found!
	// Important! CacheEntry & the key of unordered_map share memory!
	unordered_map<std::string_view, unordered_map<std::string_view, FileEntry*>> m_GamePathCache;

	std::shared_mutex m_CacheMutex;
};
static CachedFileSystem g_pCachedFileSystem;

// RaphaelIT7:
// Hack! When comparing openInfo.m_AbsolutePath against m_AddonFileSystem.ModPath() we may differ in slashes!
static bool PathStartsWith( const char *pszPath, const char *pszPrefix )
{
	if ( !pszPath || !pszPrefix )
		return false;

	while ( *pszPrefix )
	{
		char a = *pszPath++;
		char b = *pszPrefix++;
		if ( a == '\\' )
			a = '/';

		if ( b == '\\' )
			b = '/';

		if ( tolower( static_cast<unsigned char>( a ) ) != tolower( static_cast<unsigned char>( b ) ) )
			return false;
	}

	return true;
}

// RaphaelIT7:
// Try to deal with bs paths like materials\\..\\backgrounds
// V_RemoveDotSlashes exists BUT it doesn't do both seperators unlike this one
static void NormalizeGamePath( char *pszPath )
{
    char* src = pszPath;
    char* dst = pszPath;
    while ( *src )
    {
        if ( src[0] == '.' && ( src[1] == '\\' || src[1] == '/' ) )
        {
            src += 2;
            continue;
        }

        if ( src[0] == '.' && src[1] == '.' && ( src[2] == '\\' || src[2] == '/' ) )
        {
            // Remove previous component.
            if ( dst > pszPath )
            {
                --dst;

                while ( dst > pszPath && dst[-1] != '\\' && dst[-1] != '/' )
                    --dst;
            }

            src += 3;
            continue;
        }

        *dst++ = *src++;
    }

    *dst = '\0';
}

// RaphaelIT7:
// Using printf( "%s%s" ) is a real waste! So we replact it with this
// This only now becomes a bit noticeable in performance results thanks to our cache making lookups fast already
inline void ComposeSearchPath( char *pOut, size_t nOutSize, const char *pSearchPath, const char *pFileName )
{
	V_strncpy( pOut, pSearchPath, nOutSize );
	V_strcat( pOut, pFileName, nOutSize );
}

/*
	FileSystem module
*/

// IMPORTANT BUG!
// Source is missing a mutex for FindSearchPathByStoreId as m_SearchPathsMutex is never locked!
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

	return nullptr;
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
		static size_t pYappingCounter = 0;
		if (++pYappingCounter < 100)
			Warning(PROJECT_NAME ": Failed to get CBaseFileSystem::FindSearchPathByStoreId!\n");
		
		return nullptr;
	}

	return func_CBaseFileSystem_FindSearchPathByStoreId(g_pFullFileSystem, iStoreID);
}
#endif

class Addon::FileSystem : IAddonSystem
{
public:
	const std::string& ModPath() { return m_strModPath; };

private:
	std::map<std::string, std::map<std::string, std::string>> m_NotImportant;
	std::list<std::string> m_NotImportant2;
	std::string m_strModPath;
};

// IMPORTANT:
// GMod touched HandleOpenRegularFile and of course put a std::string in there causing an allocation every fking time it's called and a file is missing on disk!
static Detouring::Hook detour_CBaseFileSystem_HandleOpenRegularFile;
static Symbols::Addon_FileHandle_Size func_Addon_FileHandle_Size = nullptr;
static Symbols::Addon_FileSystem_GetFileEntry func_Addon_FileSystem_GetFileEntry = nullptr;
static Symbols::CBaseFileSystem_Trace_FOpen func_CBaseFileSystem_Trace_FOpen = nullptr;
static void hook_CBaseFileSystem_HandleOpenRegularFile(CBaseFileSystem* _this, CFileOpenInfo& openInfo, bool bIsAbsolutePath)
{
	openInfo.m_pFileHandle = nullptr;

	Addon::FileSystem* m_AddonFileSystem = (Addon::FileSystem*)_this->Addons();

	// RaphaelIT7:
	// When mounting the map_pack.bsp we may try when reading it the absolute path! So we must lookup in workshop
	bool bIsWorkshop = false;
	if ( bIsAbsolutePath && !m_AddonFileSystem->ModPath().empty() ) // We check for empty as it may have not been set early on!
		bIsWorkshop = PathStartsWith( openInfo.m_AbsolutePath, m_AddonFileSystem->ModPath().c_str() );

	// GMod
	if ( openInfo.m_pSearchPath && openInfo.m_pSearchPath->m_bIsWorkshop || bIsWorkshop )
	{
		Addon::FileHandle *pHandle = (Addon::FileHandle*)func_Addon_FileSystem_GetFileEntry( m_AddonFileSystem, openInfo.m_AbsolutePath );
		if ( pHandle )
		{
			openInfo.m_pFileHandle = new CFileHandle( _this );
			openInfo.m_pFileHandle->m_pAddonFileHandle = pHandle;
			openInfo.m_pFileHandle->m_type = FT_NORMAL;
			openInfo.m_pFileHandle->m_nLength = func_Addon_FileHandle_Size(pHandle);

			openInfo.SetResolvedFilename( openInfo.m_AbsolutePath );
		}

		// RaphaelIT7: We avoid disk lookup for workshop/ as we expect it to not exist anyways
		return;
	}

	// RaphaelIT7:
	// We do not use the cache for absolute paths!
	// Absolute paths are used by GMod for example when mounting a gma
	// The path will be somewhere in the steamapps/workshop/4000 which we did not scan (and never will)
	/*if ( !bIsAbsolutePath )
	{
		FileCacheEntry eCacheEntry = g_pBaseFileSystem->m_DiskFileTree.ContainsPath( openInfo.m_AbsolutePath );

		// openInfo.m_pFileName is a mess due to \\..\\ not yet being normalized!
		// eCacheEntry = openInfo.m_pSearchPath->ContainsPath( openInfo.m_pFileName );
		if ( eCacheEntry != FileCacheEntry::FILE && eCacheEntry != FileCacheEntry::UNKNOWN )
			return;
	}*/

	int64 size;
	FILE *fp = (FILE*)func_CBaseFileSystem_Trace_FOpen( _this, openInfo.m_AbsolutePath, openInfo.m_pOptions, openInfo.m_Flags, &size );
	if ( fp )
	{
		/*if ( m_pLogFile )
		{
			LogFileAccess( openInfo.m_AbsolutePath );
		}

		if ( m_bOutputDebugString )
		{
			// dimhotepus: Use Plat_DebugString everywhere.
			Plat_DebugString( "fs_debug: " );
			Plat_DebugString( openInfo.m_AbsolutePath );
			Plat_DebugString( "\n" );
		}*/

		// RaphaelIT7: Debugging
		/* if ( eCacheEntry != FileCacheEntry::FILE && eCacheEntry != FileCacheEntry::UNKNOWN )
		{
			__debugbreak();
			openInfo.m_pSearchPath->ContainsPath( openInfo.m_pFileName );
			__debugbreak();
		}*/

		openInfo.m_pFileHandle = new CFileHandle(_this);
		openInfo.m_pFileHandle->m_pFile = fp;
		openInfo.m_pFileHandle->m_type = FT_NORMAL;
		openInfo.m_pFileHandle->m_nLength = size;

		openInfo.SetResolvedFilename( openInfo.m_AbsolutePath );
		
		// LogFileOpen( "Loose", openInfo.m_pFileName, openInfo.m_AbsolutePath );

		// GMod - Returns on hit
		return;
	}

	// RaphaelIT7: If this happens then the file was removed from disk and we didn't know yet
	// g_pBaseFileSystem->m_DiskFileTree.RemovePath( openInfo.m_AbsolutePath );
}

// RaphaelIT7: A special flag to mark the workshop/ path
#define PATH_FLAG_ISWORKSHOP (1<<9)

static Symbols::CFileHandle_Constructor func_Constructor = nullptr;
CFileHandle::CFileHandle(CBaseFileSystem* fs)
{
	// Our Layout matches GMod so this should have no side effects :3
	func_Constructor(this, fs);
}

static Detouring::Hook detour_CBaseFileSystem_NewSearchPath;
CSearchPath* hook_CBaseFileSystem_NewSearchPath(void* _this, int addType)
{
	// RaphaelIT7:
	// We MUST do addType & 0x1FF as GMod uses a vague mask where we can easily corrupt the priority group if we don't clear the custom flag bits
	// This is since GMod only skips bit 8 when getting the priority group but they don't skip the bits after...
	// GMod uses 0xFFFFFEFF when it should be using 0xFE
	CSearchPath* pPath = (CSearchPath*)detour_CBaseFileSystem_NewSearchPath.GetTrampoline<Symbols::CBaseFileSystem_NewSearchPath>()(_this, addType & 0x1FF);
	pPath->m_bIsWorkshop = (addType & PATH_FLAG_ISWORKSHOP) != 0;

	return pPath;
}

void CFileSystemModule::ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
}

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
	g_pCachedFileSystem.Nuke();
}
static ConCommand nukesearchcache("holylib_filesystem_nukesearchcache", NukeSearchcacheCmd, "Nukes the searchcache", 0);

static void DumpCacheCmd(const CCommand &args)
{
	Msg("---- FileHandle cache ----\n");
	g_pCachedFileSystem.Dump();
	Msg("---- End of Search cache ----\n");
}
static ConCommand dumpfilecache("holylib_filesystem_dumpcache", DumpCacheCmd, "Dumps the filecache", 0);

static bool bShutdown = false;

static void InitFileSystem(IFileSystem* pFileSystem)
{		
	if (!pFileSystem || bShutdown) // We refuse to init when this is called when it shouldn't. If it crashes, then give me a stacktrace to fix it.
		return;

	g_pFullFileSystem = pFileSystem;

	if (g_pFileSystemModule.InDebug())
		Msg("holylib - filesystem: Initialized filesystem\n");
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

	return detour_CBaseFileSystem_FindFileInSearchPath.GetTrampoline<Symbols::CBaseFileSystem_FindFileInSearchPath>()(filesystem, openInfo);
}

// Future note: When using an absolute path the search path should not be a packed file! And it doesn't matter what search path it is! Just not a pack!
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
	}

	long time = detour_CBaseFileSystem_FastFileTime.GetTrampoline<Symbols::CBaseFileSystem_FastFileTime>()(filesystem, path, pFileName);
	return time;
}

static FORCEINLINE bool is_file(const char *path)
{
	const char *last_slash = strrchr(path, FILEPATH_SLASH_CHAR);
	const char *last_dot = strrchr(path, '.');

	return last_dot != nullptr && (last_slash == nullptr || last_dot > last_slash);
}

static Detouring::Hook detour_CBaseFileSystem_IsDirectory;
static bool hook_CBaseFileSystem_IsDirectory(void* filesystem, const char* pFileName, const char* pPathID)
{
	VPROF_BUDGET("HolyLib - CBaseFileSystem::IsDirectory", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	if (holylib_filesystem_easydircheck.GetBool() && is_file(pFileName))
		return false;

	return detour_CBaseFileSystem_IsDirectory.GetTrampoline<Symbols::CBaseFileSystem_IsDirectory>()(filesystem, pFileName, pPathID);
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

	FileHandle_t fh = detour_CBaseFileSystem_OpenForRead.GetTrampoline<Symbols::CBaseFileSystem_OpenForRead>()(filesystem, pFileNameT, pOptions, flags, pathID, ppszResolvedFilename);

	return fh;
}

/*
 * GMod first calls GetFileTime and then OpenForRead, so we need to make changes for Lua in GetFileTime.
 */

/*
 * GMOD Likes to use paths like "sandbox/gamemode/spawnmenu/sandbox/gamemode/spawnmenu/".
 * This wastes performance, so we fix them up to be "sandbox/gamemode/spawnmenu/"
 */
static std::string_view fixGamemodePath(std::string_view path)
{
	// BUG: I have no idea why... previously we passed filesystem as an argument
	// that somehow corrupted itself, using g_pFullFileSystem though goes completely fine???

	// Just debug stuff... The one line does these three things at once
	//Gamemode::System* pGamemodeSystem = g_pFullFileSystem->Gamemodes();
	//const IGamemodeSystem::UpdatedInformation& pActiveGamemode = (const IGamemodeSystem::UpdatedInformation&)pGamemodeSystem->Active();
	//std::string_view activeGamemode = pActiveGamemode.name;
	std::string_view activeGamemode = g_pFullFileSystem->Gamemodes()->Active().name;
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

	std::string_view strFileName = pFileName; // Workaround for now.
	if (pPathID && V_stricmp(pPathID, "lsv") == 0 && holylib_filesystem_fixgmodpath.GetBool()) // Some weird things happen in the lsv path.  
		strFileName = fixGamemodePath(strFileName);

	pFileName = strFileName.data();
	if (holylib_filesystem_skipinvalidluapaths.GetBool())
	{
		if (strFileName.rfind("include" FILEPATH_SLASH "include" FILEPATH_SLASH) == 0)
			return 0L;
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

static Detouring::Hook detour_CBaseFileSystem_Close;
static void DeleteFileHandle(FileHandle_t handle)
{
	detour_CBaseFileSystem_Close.GetTrampoline<Symbols::CBaseFileSystem_Close>()(g_pFullFileSystem, handle);
}

static void hook_CBaseFileSystem_Close(IFileSystem* filesystem, FileHandle_t file)
{
	VPROF_BUDGET("HolyLib - CBaseFileSystem::Close", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	if (!g_pCachedFileSystem.ShouldCloseHandle(file))
		return;

	detour_CBaseFileSystem_Close.GetTrampoline<Symbols::CBaseFileSystem_Close>()(filesystem, file);
}

void CFileSystemModule::Think(bool bSimulating)
{
	g_pCachedFileSystem.MainThink();
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
		// RaphaelIT7:
		// BUG! We are completely nuking any special flags like VPKHack, PriorityGroup!
		// We should NOT be recreating them at all!

		// Humongus size because you can have a huge amount of searchpaths.
		/*
		constexpr int iSize = 1 << 16;
		char* pChar = new char[iSize];
		CBaseFileSystem* pSystem = (CBaseFileSystem*)g_pFullFileSystem;
		int iLength = pSystem->GetSearchPath("GAME", true, pChar, iSize);
		if (iSize <= iLength)
			Warning(PROJECT_NAME ": Not enough space for search paths! please report this.\n");

		std::string pStr = pChar;
		pStr = pStr.substr(0, iLength);
		std::vector<std::string> pSearchPaths = splitString(pStr, ";");
		pSystem->RemoveSearchPaths("GAME"); // Yes. Were gonna reapply them. Should we also do it for lsv?
		for (std::string pSearchPath : pSearchPaths)
		{
			pSystem->AddSearchPath(pSearchPath.c_str(), "GAME", PATH_ADD_TO_TAIL);
			
			if (g_pFileSystemModule.InDebug())
				Msg("Recreate Path: %s\n", pSearchPath.c_str());
		}

		delete[] pChar;
		*/
	}

	int pBaseLength = 0;
	char pBaseDir[MAX_PATH];
	if ( pBaseLength < 3 )
		pBaseLength = g_pFullFileSystem->GetSearchPath( "BASE_PATH", true, pBaseDir, sizeof( pBaseDir ) );

	std::string workshopDir = pBaseDir;
	workshopDir.append("garrysmod" FILEPATH_SLASH "workshop");

	if (g_pFullFileSystem != nullptr)
		InitFileSystem(g_pFullFileSystem);

	if (!DETOUR_ISVALID(detour_CBaseFileSystem_AddSearchPath))
	{
		Msg(PROJECT_NAME ": CBaseFileSystem::AddSearchPath detour is invalid?\n");
		return;
	}

	// NOTE: Check the thing below again and redo it. I don't like how it looks :<
	if (g_pFullFileSystem->IsDirectory("materials" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_MATERIALS", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );

	if (g_pFullFileSystem->IsDirectory("models" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_MODELS", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );
	
	if (g_pFullFileSystem->IsDirectory("sound" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_SOUNDS", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );
	
	if (g_pFullFileSystem->IsDirectory("maps" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_MAPS", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );
	
	if (g_pFullFileSystem->IsDirectory("resource" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_RESOURCE", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );

	if (g_pFullFileSystem->IsDirectory("scripts" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_SCRIPTS", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );

	if (g_pFullFileSystem->IsDirectory("cfg" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "CONTENT_CONFIGS", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );

	if (g_pFullFileSystem->IsDirectory("gamemodes" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_GAMEMODES", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );

	if (g_pFullFileSystem->IsDirectory("lua" FILEPATH_SLASH "includes" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_INCLUDES", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );
	
	if (g_pFullFileSystem->IsDirectory("sandbox" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_GAMEMODE_SANDBOX", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );

	if (g_pFullFileSystem->IsDirectory("effects" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_EFFECTS", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );
	
	if (g_pFullFileSystem->IsDirectory("entities" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_ENTITIES", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );

	if (g_pFullFileSystem->IsDirectory("weapons" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_WEAPONS", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );

	if (g_pFullFileSystem->IsDirectory("lua" FILEPATH_SLASH "derma" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_DERMA", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );

	if (g_pFullFileSystem->IsDirectory("lua" FILEPATH_SLASH "drive" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_DRIVE", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );

	if (g_pFullFileSystem->IsDirectory("lua" FILEPATH_SLASH "entities" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_LUA_ENTITIES", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );

	if (g_pFullFileSystem->IsDirectory("vgui" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_VGUI", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );

	if (g_pFullFileSystem->IsDirectory("postprocess" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_POSTPROCESS", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );

	if (g_pFullFileSystem->IsDirectory("matproxy" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_MATPROXY", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );

	if (g_pFullFileSystem->IsDirectory("autorun" FILEPATH_SLASH, "workshop"))
		detour_CBaseFileSystem_AddSearchPath.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPath>()(g_pFullFileSystem, workshopDir.c_str(), "LUA_AUTORUN", PRIORITY_GROUP_TAIL( GN_GMODCORE ) );
	
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
		return nullptr;

	return g_pPathIDTable->String( m_PathID );
}

inline const char* CSearchPath::GetPathIDString() const
{
	if (Util::GetGModVersionNum() < 260718)
	{
		if (((CSearchPathOld*)this)->m_pPathIDInfo)
			return ((CSearchPathOld*)this)->m_pPathIDInfo->GetPathIDString();

		return nullptr;
	}

	if (m_pPathIDInfo)
		return m_pPathIDInfo->GetPathIDString(); // When can we nuke it :>

	return nullptr;
}

static Symbols::CBaseFileSystem_CSearchPath_GetDebugString func_CBaseFileSystem_CSearchPath_GetDebugString;
inline const char* CSearchPath::GetPathString() const
{
	if (!func_CBaseFileSystem_CSearchPath_GetDebugString)
		return nullptr;

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
	DETOUR_THISCALL_ADDFUNC2( hook_CBaseFileSystem_HandleOpenRegularFile, HandleOpenRegularFile, CBaseFileSystem*, CFileOpenInfo&, bool);
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

	if (g_pFullFileSystem != nullptr)
		InitFileSystem(g_pFullFileSystem);

	// ToDo: Redo EVERY Hook so that we'll abuse the vtable instead of symbols.  
	// Use the ClassProxy or so which should also allow me to port this to windows.
	#if defined( NOT_DEDICATED )
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

	Detour::Create(
		&detour_CBaseFileSystem_HandleOpenRegularFile, "CBaseFileSystem::HandleOpenRegularFile",
		filesystem_loader.GetModule(), Symbols::CBaseFileSystem_HandleOpenRegularFileSym,
		(void*)DETOUR_THISCALL(hook_CBaseFileSystem_HandleOpenRegularFile, HandleOpenRegularFile), m_pID
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

#if defined(ARCHITECTURE_X86) && defined(SYSTEM_LINUX)
	g_pPathIDTable = Detour::ResolveSymbol<CUtlSymbolTableMT>(filesystem_loader, Symbols::g_PathIDTableSym);
#else
	g_pPathIDTable = Detour::ResolveSymbolWithOffset<CUtlSymbolTableMT>(filesystem_loader.GetModule(), Symbols::g_PathIDTableSym);
#endif
	Detour::CheckValue("get class", "g_PathIDTable", g_pPathIDTable != nullptr);
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
	const char* content = nullptr;
};

std::vector<IAsyncFile*> asyncCallback;
void AsyncCallback(const FileAsyncRequest_t &request, int nBytesRead, FSAsyncStatus_t err)
{
	IAsyncFile* async = (IAsyncFile*)request.pContext;
	if (async)
	{
		async->nBytesRead = nBytesRead;
		async->status = err;
		int nContentLength = nBytesRead > 0 && request.pData ? nBytesRead : 0;
		char* content = new char[nContentLength + 1];
		if (nContentLength > 0)
			std::memcpy(static_cast<void*>(content), request.pData, nContentLength);
		content[nContentLength] = '\0';
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
	for(IAsyncFile* file : asyncCallback)
	{
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
		g_pModuleManager.IsUnsafeCodeEnabled() ? LUA->CheckStringOpt(2, "DATA") : "DATA" // Force "DATA" path if unsafe is disabled
	);

	return 0;
}

LUA_FUNCTION_STATIC(filesystem_Delete)
{
	g_pFullFileSystem->RemoveFile(LUA->CheckString(1),
		g_pModuleManager.IsUnsafeCodeEnabled() ? LUA->CheckStringOpt(2, "DATA") : "DATA" // Force "DATA" path if unsafe is disabled
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
	if (lastSlashPos != std::string::npos)
		return filepath.substr(0, lastSlashPos + 1);
	else
		return "";
}

std::vector<std::string> SortByDate(std::vector<std::string> files, const char* filepath, const char* path, bool ascending)
{
	std::string str_filepath = extractDirectoryPath((std::string)filepath);
	unordered_map<std::string_view, long> dates;
	for (std::string file : files) {
		dates[file] = g_pFullFileSystem->GetFileTime((str_filepath + file).c_str(), path);
	}

	std::sort(files.begin(), files.end(), [&dates](const std::string& a, const std::string& b) {
		return dates[a] < dates[b];
	});

	if (!ascending)
		std::reverse(files.begin(), files.end());

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
			files = SortByDate(files, filepath, gamePath, true);
			folders = SortByDate(folders, filepath, gamePath, true);
		} else if (strcmp(sorting, "datedesc") == 0) { // sort the files descending by date.
			files = SortByDate(files, filepath, gamePath, false);
			folders = SortByDate(folders, filepath, gamePath, false);
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
		FileHandle_t handle = nullptr;
		int idk = 1; // If it's 0 the file is said to be nullptr.
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
		gamePath = "DATA"; // Force "DATA" path if unsafe is disabled

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

	// The Source Filesystem does not lock on the main thread that often!
	// Soo the assumption is that the main thread is the only one modifying search paths!
	if (!ThreadInMainThread())
		LUA->ThrowError("Thread must be on the main thread due to filesystem assumptions!");

	const char* folderPath = LUA->CheckString(1);
	const char* gamePath = LUA->CheckString(2);
	SearchPathAdd_t addType = LUA->GetBool(-1) ? PATH_ADD_TO_HEAD : PATH_ADD_TO_TAIL;
	g_pFullFileSystem->AddSearchPath(folderPath, gamePath, addType);

	return 0;
}

LUA_FUNCTION_STATIC(filesystem_RemoveSearchPath)
{
	Util::DoUnsafeCodeCheck(LUA);

	if (!ThreadInMainThread())
		LUA->ThrowError("Thread must be on the main thread due to filesystem assumptions!");

	const char* folderPath = LUA->CheckString(1);
	const char* gamePath = LUA->CheckString(2);
	LUA->PushBool(g_pFullFileSystem->RemoveSearchPath(folderPath, gamePath));

	return 1;
}

LUA_FUNCTION_STATIC(filesystem_RemoveSearchPaths)
{
	Util::DoUnsafeCodeCheck(LUA);

	if (!ThreadInMainThread())
		LUA->ThrowError("Thread must be on the main thread due to filesystem assumptions!");

	const char* gamePath = LUA->CheckString(1);
	g_pFullFileSystem->RemoveSearchPaths(gamePath);

	return 0;
}

LUA_FUNCTION_STATIC(filesystem_RemoveAllSearchPaths)
{
	Util::DoUnsafeCodeCheck(LUA);

	if (!ThreadInMainThread())
		LUA->ThrowError("Thread must be on the main thread due to filesystem assumptions!");

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
	const char* gamePath = LUA->CheckStringOpt(2, nullptr);

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

inline IAddonSystem* GetAddonFilesystem()
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
	const char* strGMAPath = LUA->CheckString(1);

	std::vector<std::string> files;
	// 2? Seems to be what game.MountGMA uses
	LUA->PushNumber(GetAddonFilesystem()->MountFile(strGMAPath, &files, 0, 0, IAddonSystem::AddonSource(2)));

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
	uint64 workshopID = strtoull(workshopID64, nullptr, 0);
	LUA->PushBool(GetAddonFilesystem()->ShouldMount(workshopID));

	return 1;
}

LUA_FUNCTION_STATIC(addonsystem_SetShouldMount)
{
	const char* workshopID64 = LUA->CheckString(1);
	uint64 workshopID = strtoull(workshopID64, nullptr, 0);
	bool bMount = LUA->GetBool(2);
	GetAddonFilesystem()->SetShouldMount(workshopID, bMount);

	return 0;
}

// IMPORTANT! Old content stays mounted!
LUA_FUNCTION_STATIC(addonsystem_ChangeCollection)
{
	const char* pszCollectionID = LUA->CheckString(1);
	ConVarRef host_workshop_collection("host_workshop_collection");
	if (host_workshop_collection.IsValid())
		host_workshop_collection.SetValue(pszCollectionID);

	uint64_t wsid = strtoull(pszCollectionID, nullptr, 0);
	GetAddonFilesystem()->ScanForSubscriptions(pszCollectionID, wsid != 0);

	return 0;
}

LUA_FUNCTION_STATIC(addonsystem_Subscribe)
{
	const char* pszWSID = LUA->CheckString(1);
	uint64_t wsid = strtoull(pszWSID, nullptr, 0);

	SteamUGC()->SubscribeItem(wsid);
	GetAddonFilesystem()->AddUnloadedSubscription(wsid);

	if (Util::server)
	{
		CBaseServer* pServer = (CBaseServer*)Util::server;
		bool bIsDedicated = pServer->m_bIsDedicated;
		pServer->m_bIsDedicated = false;
		// We must fool the Addon::FileSystem as it calls IGet::IsDedicatedServer which does sv.IsDedicated which returns m_bIsDedicated :3

		// now it should be able to do Task::GetSubscriptions & Task::DownloadAddons
		GetAddonFilesystem()->ScanForSubscriptions("", false);

		pServer->m_bIsDedicated = bIsDedicated;
	}

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
		Util::AddFunc(pLua, addonsystem_ChangeCollection, "ChangeCollection");
		Util::AddFunc(pLua, addonsystem_Subscribe, "Subscribe");
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
		pFileSystemPool = nullptr;
	}

	g_pCachedFileSystem.Shutdown();

	bShutdown = true;
}