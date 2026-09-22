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
#include <shared_mutex>
#include "edict.h"
#include "unordered_stuff.h"
#include "sdk_backports.h"

#include <isteamugc.h>
#include "sourcesdk/baseserver.h"

#include "rengine/filewatcher.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CFileSystemModule : public IModule
{
public:
	void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) override;
	void InitDetour(bool bPreServer) override;
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaThink(GarrysMod::Lua::ILuaInterface* pLua) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void Think(bool bSimulating) override;
	const char* Name() override { return "filesystem"; };
	int Compatibility() override { return LINUX32; };
	bool SupportsMultipleLuaStates() override { return true; };
};

static CFileSystemModule g_pFileSystemModule;
IModule* pFileSystemModule = &g_pFileSystemModule;

#if SYSTEM_WINDOWS
#define FILEPATH_SLASH "\\"
#define FILEPATH_SLASH_CHAR '\\'
#else
#define FILEPATH_SLASH "/"
#define FILEPATH_SLASH_CHAR '/'
#endif

class Addon::FileSystem : public IAddonSystem
{
public:
	const std::string& ModPath() { return m_strModPath; };

private:
	std::map<std::string, std::map<std::string, std::string>> m_NotImportant;
	std::list<std::string> m_NotImportant2;
	std::string m_strModPath;
};

enum class FileCacheEntry : unsigned char
{
	UNKNOWN = 255, // if returned then check disk? (Exists just as a fallback for now)
	INVALID = 0, // Does not exist
	FILE,
	FOLDER,
};


CUtlSymbol CBaseFileSystem::m_GamePathID;
CUtlSymbol CBaseFileSystem::m_BSPPathID;

// VS2022 falsely claims the out buffer may not be null terminated...
static FORCEINLINE void GetFullPath(const CSearchPath* pSearchPath, const char* strFileName, char (&out)[MAX_PATH])
{
	V_strcpy_safe(out, pSearchPath->GetPathString());
	size_t len = strlen(out);
	V_strncpy(out + len, strFileName, sizeof(out) - len);
	V_strlower(out + len);
}

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

static Symbols::CFileSystem_Stdio_FS_FindFirstFile func_CFileSystem_Stdio_FS_FindFirstFile = nullptr;
static Symbols::CFileSystem_Stdio_FS_FindNextFile func_CFileSystem_Stdio_FS_FindNextFile = nullptr;
static Symbols::CFileSystem_Stdio_FS_FindClose func_CFileSystem_Stdio_FS_FindClose = nullptr;

// RaphaelIT7:
// For GMod's scale this will be a lot more complex than REngine...
// Fun :)
class CSearchPath;
class CDiskFileTree : public CRefCounted<CRefCountServiceMT>
{
public:
	void BuildTree( const char *pszRoot );
	FileCacheEntry ContainsPath( const char *pszAbsolutePath );

	void AddPath( const char *pszAbsolutePath, FileCacheEntry type );
	void RemovePath( const char *pszAbsolutePath );
	void RenamePath( const char *pszOldAbsolutePath, const char *pszNewAbsolutePath );

	const auto& GetFileList() const { return m_FileList; }
	void Rebuild();

	std::shared_mutex& GetMutex() { return m_FileListMutex; }

private:
	// bForceScan = true makes the root dir be always scanned
	bool RecursiveTraverse( const char *pszFolderPath, bool bForceScan = false, bool bCreateWatchers = false );

	// We use StringHash & StringEq so that when searching we do not allocate an std::string
	unordered_map<std::string, FileCacheEntry, StringHash, StringEq> m_FileList;
	std::shared_mutex m_FileListMutex;
};

static CDiskFileTree g_DiskFileTree;

static ConVar holylib_filesystem_static("holylib_filesystem_static", "0", FCVAR_ARCHIVE,
	"If enabled, then no file watchers are created as it is assumed at runtime the filesystem won't change externally");

class CFileWatcherSystem : public IFileWatcherSystem
{
public: // IFileWatcherSystem
	// Not important for us
	void OnFileModify(const char* pszFullFilePath) {};

	void OnFileCreated(const char* pszFullFilePath)
	{
		if (g_pFileSystemModule.InDebug())
			Msg(PROJECT_NAME " - filesystem(OnFileCreated): %s\n", pszFullFilePath);

		g_DiskFileTree.AddPath(NormalizePath(pszFullFilePath), FileCacheEntry::FILE);
	}

	void OnFileDeleted(const char* pszFullFilePath)
	{
		if (g_pFileSystemModule.InDebug())
			Msg(PROJECT_NAME " - filesystem(OnFileDeleted): %s\n", pszFullFilePath);

		g_DiskFileTree.RemovePath(NormalizePath(pszFullFilePath));
	}

	void OnFileRenamed(const char* pOldFullFilePath, const char* pszNewFullFilePath)
	{
		if (g_pFileSystemModule.InDebug())
			Msg(PROJECT_NAME " - filesystem(OnFileRenamed): %s -> %s\n", pOldFullFilePath, pszNewFullFilePath);

		g_DiskFileTree.RenamePath(NormalizePath(pOldFullFilePath), NormalizePath(pszNewFullFilePath));
	}

	void OnFolderCreated(const char* pszFullFolderPath)
	{
		if (g_pFileSystemModule.InDebug())
			Msg(PROJECT_NAME " - filesystem(OnFolderCreated): %s\n", pszFullFolderPath);

		const char* pszNormalized = NormalizePath(pszFullFolderPath);
		CreateWatcher(pszNormalized); // First so that when we do a scan, any file added while were scanning is not messed up 

		g_DiskFileTree.AddPath(pszNormalized, FileCacheEntry::FOLDER);
	}

	void OnFolderDeleted(const char* pszFullFolderPath)
	{
		if (g_pFileSystemModule.InDebug())
			Msg(PROJECT_NAME " - filesystem(OnFolderDeleted): %s\n", pszFullFolderPath);

		g_DiskFileTree.RemovePath(NormalizePath(pszFullFolderPath));
	}

	void OnFolderRenamed(const char* pszOldFullFolderPath, const char* pNewFullFolderPath)
	{
		if (g_pFileSystemModule.InDebug())
			Msg(PROJECT_NAME " - filesystem(OnFolderRenamed): %s -> %s\n", pszOldFullFolderPath, pNewFullFolderPath);

		const char* pszNewNormalized = NormalizePath(pNewFullFolderPath);
		g_DiskFileTree.RenamePath(NormalizePath(pszOldFullFolderPath), pszNewNormalized);
		CreateWatcher(pszNewNormalized);
	}

	void RegisterInternalWatcher(CFileWatcher* pWatcher)
	{
		std::lock_guard<std::recursive_mutex> lock(m_WatchersMutex);
		auto it = m_Watchers.find(pWatcher);
		if (it == m_Watchers.end())
			m_Watchers.insert(pWatcher);

		auto folderIt = m_WatcherFolders.find(pWatcher->GetFullFolderPath());
		if (folderIt == m_WatcherFolders.end())
			m_WatcherFolders.insert(pWatcher->GetFullFolderPath());
	}

	void UnregisterInternalWatcher(CFileWatcher* pWatcher)
	{
		auto it = m_Watchers.find(pWatcher);
		if (it != m_Watchers.end())
			m_Watchers.erase(it);

		auto folderIt = m_WatcherFolders.find(pWatcher->GetFullFolderPath());
		if (folderIt != m_WatcherFolders.end())
			m_WatcherFolders.erase(folderIt);

		pWatcher->GiveYummyToLumi();
	}

public:
	void CreateWatcher(const char* pszFullFolderPath)
	{
		if (holylib_filesystem_static.GetBool())
			return;

		{
			std::lock_guard<std::recursive_mutex> lock(m_WatchersMutex);
			if (m_WatcherFolders.find(pszFullFolderPath) == m_WatcherFolders.end())
				return; // We already have a watcher on this folder
		}

		if (g_pFileSystemModule.InDebug())
			Msg(PROJECT_NAME " - filesystem(CreateWatcher): %s\n", pszFullFolderPath);

		char szFolderPath[MAX_PATH];
		V_strncpy(szFolderPath, pszFullFolderPath, sizeof(szFolderPath));
		V_FixSlashes(szFolderPath, '/');
		V_RemoveDotSlashes(szFolderPath);
		V_StripTrailingSlash(szFolderPath);
		V_strlower(szFolderPath);

		new CFileWatcher(szFolderPath);

		char szSearchPath[MAX_PATH];
		V_snprintf(szSearchPath, sizeof(szSearchPath), "%s/*", pszFullFolderPath);

		WIN32_FIND_DATA findData;
		HANDLE hFind = func_CFileSystem_Stdio_FS_FindFirstFile(g_pFullFileSystem, szSearchPath, &findData);
		if (hFind == INVALID_HANDLE_VALUE)
			return;

		do
		{
			if (!V_stricmp(findData.cFileName, ".") || !V_stricmp(findData.cFileName, ".."))
				continue;

			if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				continue;

			char szFullPath[MAX_PATH];
			V_snprintf(szFullPath, sizeof(szFullPath), "%s" CORRECT_PATH_SEPARATOR_S "%s", pszFullFolderPath, findData.cFileName);
			V_FixSlashes(szFullPath, '/');
			V_RemoveDotSlashes(szFullPath);
			V_StripTrailingSlash(szFullPath);
			V_strlower(szFullPath);

			CreateWatcher(szFullPath);
		} while (func_CFileSystem_Stdio_FS_FindNextFile(g_pFullFileSystem, hFind, &findData));

		func_CFileSystem_Stdio_FS_FindClose(g_pFullFileSystem, hFind);
	}

	void RunCallbacks()
	{
		std::lock_guard<std::recursive_mutex> lock(m_WatchersMutex);
#if SYSTEM_WINDOWS
		for (auto& pWatcher : m_Watchers)
			pWatcher->CheckForChanges();
#else
		CFileWatcher::CheckForChanges();
#endif

		std::erase_if(m_Watchers, [this](CFileWatcher* pWatcher) {
			bool bFood = pWatcher->FoodForLumi();
			if (bFood)
				UnregisterInternalWatcher(pWatcher);

			return bFood;
		});
	}

	const char* NormalizePath(const char* pszAbsolutePath)
	{
		m_bNextFullPath = !m_bNextFullPath;
		V_strncpy( m_szFullPath[m_bNextFullPath], pszAbsolutePath, sizeof( m_szFullPath[m_bNextFullPath] ) );
		V_FixSlashes( m_szFullPath[m_bNextFullPath], '/' );
		// RaphaelIT7:
		// Somehow... we can have some of those.
		// No we cannot use NormalizeGamePath as the resulting path is wrong... somehow
		V_RemoveDotSlashes( m_szFullPath[m_bNextFullPath] );
		V_StripTrailingSlash( m_szFullPath[m_bNextFullPath] );
		V_strlower( m_szFullPath[m_bNextFullPath] );

		return m_szFullPath[m_bNextFullPath];
	}

private:
	bool m_bNextFullPath = false; // Since we need two buffers
	char m_szFullPath[2][MAX_PATH];
	std::recursive_mutex m_WatchersMutex;
	ankerl::unordered_dense::set<CFileWatcher*> m_Watchers{};
	ankerl::unordered_dense::set<std::string_view> m_WatcherFolders{};
};

static CFileWatcherSystem g_FileWatcherSystem;
IFileWatcherSystem* g_pFileWatcherSystem = &g_FileWatcherSystem;

void CFileSystemModule::Think(bool bSimulating)
{
	g_FileWatcherSystem.RunCallbacks();
}

static void OnSearchCacheChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	if (!((ConVar*)convar)->GetBool())
		return;

	g_DiskFileTree.Rebuild();
}

static ConVar holylib_filesystem_filecache("holylib_filesystem_filecache", "1", FCVAR_ARCHIVE, 
	"If enabled, it will build a file tree and use that for lookups to skip searchpaths.", OnSearchCacheChange);
static ConVar holylib_filesystem_fixgmodpath("holylib_filesystem_fixgmodpath", "1", FCVAR_ARCHIVE, 
	"If enabled, it will fix up weird gamemode paths like sandbox/gamemode/sandbox/gamemode which gmod likes to use.");
static ConVar holylib_filesystem_skipinvalidluapaths("holylib_filesystem_skipinvalidluapaths", "1", FCVAR_ARCHIVE,
	"If enabled, invalid lua paths like include/include/ will be skipped instantly");

void CDiskFileTree::BuildTree( const char *pszRoot )
{
	if ( !V_IsAbsolutePath( pszRoot ) )
		return;

	char szFullPath[MAX_PATH];
	V_strncpy( szFullPath, pszRoot, sizeof( szFullPath ) );
	V_FixSlashes( szFullPath, '/' );
	// RaphaelIT7:
	// Somehow... we can have some of those.
	// No we cannot use NormalizeGamePath as the resulting path is wrong... somehow
	V_RemoveDotSlashes( szFullPath );
	V_StripTrailingSlash( szFullPath );
	V_strlower( szFullPath );

	RecursiveTraverse( pszRoot );
}

FileCacheEntry CDiskFileTree::ContainsPath( const char *pszAbsolutePath )
{
	if ( !holylib_filesystem_filecache.GetBool() )
		return FileCacheEntry::UNKNOWN;

	std::shared_lock<std::shared_mutex> lock( m_FileListMutex );
	auto it = m_FileList.find( pszAbsolutePath );
	if ( it != m_FileList.end() )
		return it->second;

	// RaphaelIT7: BUG! If we print anything we crash due to a stackoverflow in tier0? Something with output!
	//Msg( "Failed to find %s\n", pszAbsolutePath );
	return FileCacheEntry::INVALID;
}

void CDiskFileTree::AddPath( const char *pszAbsolutePath, FileCacheEntry type )
{
	std::unique_lock<std::shared_mutex> lock( m_FileListMutex );
	auto it = m_FileList.find( pszAbsolutePath );
	if ( it == m_FileList.end() )
		m_FileList[pszAbsolutePath] = type;
}

void CDiskFileTree::RemovePath( const char *pszAbsolutePath )
{
	std::unique_lock<std::shared_mutex> lock( m_FileListMutex );
	auto it = m_FileList.find( pszAbsolutePath );
	if ( it != m_FileList.end() )
		m_FileList.erase( it );
}

void CDiskFileTree::RenamePath( const char *pszOldAbsolutePath, const char *pszNewAbsolutePath )
{
	std::unique_lock<std::shared_mutex> lock( m_FileListMutex );
	auto it = m_FileList.find( pszOldAbsolutePath );
	if ( it == m_FileList.end() )
	{
		// Actually let's not Rebuild() since the FileSystem will call this from Rename BUT the FileWatcher may also report it ontop!
		// ToDo: Figure out how we could keep the disk tree & fs in sync without both possibly conflicting
		// 
		//Rebuild(); // ToDo: We could go to disk and check what pszNewAbsolutePath is and what is going on, but this is easier right now (#Lazy)
		return;
	}

	bool bIsFolder = it->second == FileCacheEntry::FOLDER;
	m_FileList[ pszNewAbsolutePath ] = it->second;
	m_FileList.erase( it );

	if ( !bIsFolder )
		return;

	// Expensive...
	// We must update all children too!

	const size_t nOldLength = strlen( pszOldAbsolutePath );
	for ( auto it = m_FileList.begin(); it != m_FileList.end(); )
	{
		const std::string &strPath = it->first;
		if ( strPath.size() <= nOldLength || strPath.compare( 0, nOldLength, pszOldAbsolutePath ) != 0 || strPath[nOldLength] != '/' )
		{
			++it;
			continue;
		}

		auto node = m_FileList.extract( it++ );

		std::string &strNewPath = node.first;
		strNewPath.replace( 0, nOldLength, pszNewAbsolutePath );

		if ( g_pFileSystemModule.InDebug() )
			Msg( PROJECT_NAME " - filesystem(RenamePath - Folder children): %s\n", strNewPath.c_str() );

		m_FileList.insert( std::move( node ) );
	}
}

void CDiskFileTree::Rebuild()
{
#if GMOD_X86_64
	return;
#endif

	if (Util::GetGModVersionNum() < 260718)
		return;

	std::unique_lock<std::shared_mutex> lock(m_FileListMutex);
	m_FileList.clear();
	Addon::FileSystem* m_AddonFileSystem = (Addon::FileSystem*)g_pFullFileSystem->Addons();
	FOR_EACH_LL_(((CBaseFileSystem*)g_pFullFileSystem)->m_SearchPaths, pSearchPath)
	{
		if ( V_IsAbsolutePath( pSearchPath->GetPathString() ) && (m_AddonFileSystem->ModPath().empty() || PathStartsWith( pSearchPath->GetPathString(), m_AddonFileSystem->ModPath().c_str() )) )
			BuildTree( pSearchPath->GetPathString() );
	}
}

// RaphaelIT7:
// This is expensive! A trade of startup time vs runtime performance
// ToDo: Check out if we can improve memory usage
bool CDiskFileTree::RecursiveTraverse( const char *pszFolderPath, bool bForceScan, bool bCreateWatchers )
{
	// If we have a entry then we already are tracking this one
	if ( !bForceScan && m_FileList.find( pszFolderPath ) != m_FileList.end() )
	{
		// Msg("Skipping already scanned folder %s\n", pszFolderPath);
		return true;
	}

	char szSearchPath[MAX_PATH];
	V_snprintf( szSearchPath, sizeof( szSearchPath ), "%s/*", pszFolderPath );

	WIN32_FIND_DATA findData;
	HANDLE hFind = func_CFileSystem_Stdio_FS_FindFirstFile( g_pFullFileSystem, szSearchPath, &findData );
	if ( hFind == INVALID_HANDLE_VALUE )
	{
		DevWarning(PROJECT_NAME " filesystem: FindFirst failed: '%s' (Permissions are wrong or folder is empty?)\n", szSearchPath);
		return false;
	}

	do
	{
		if ( !V_stricmp( findData.cFileName, "." ) || !V_stricmp( findData.cFileName, ".." ) )
			continue;

		char szFullPath[MAX_PATH];
		V_snprintf( szFullPath, sizeof( szFullPath ), "%s" CORRECT_PATH_SEPARATOR_S "%s", pszFolderPath, findData.cFileName );
		V_FixSlashes( szFullPath, '/' );
		// RaphaelIT7:
		// Somehow... we can have some of those.
		// No we cannot use NormalizeGamePath as the resulting path is wrong... somehow
		V_RemoveDotSlashes( szFullPath );
		V_StripTrailingSlash( szFullPath );
		V_strlower( szFullPath );

		const bool bDirectory = ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;
		if ( bDirectory ) {
			if (bCreateWatchers)
				g_FileWatcherSystem.CreateWatcher( szFullPath );

			if ( RecursiveTraverse( szFullPath ) )
				m_FileList.emplace( szFullPath, FileCacheEntry::FOLDER);
		} else
			m_FileList.emplace( szFullPath, FileCacheEntry::FILE );
	} while ( func_CFileSystem_Stdio_FS_FindNextFile( g_pFullFileSystem, hFind, &findData ) );

	func_CFileSystem_Stdio_FS_FindClose( g_pFullFileSystem, hFind );
	return true;
}

/*
	FileSystem module
*/

// IMPORTANT:
// GMod touched HandleOpenRegularFile and of course put a std::string in there causing an allocation every fking time it's called and a file is missing on disk!
static Detouring::Hook detour_CBaseFileSystem_HandleOpenRegularFile;
static Symbols::Addon_FileHandle_Size func_Addon_FileHandle_Size = nullptr;
static Symbols::CBaseFileSystem_FixUpPath func_CBaseFileSystem_FixUpPath = nullptr;
static Symbols::Addon_FileSystem_GetFileEntry func_Addon_FileSystem_GetFileEntry = nullptr;
static Symbols::CBaseFileSystem_Trace_FOpen func_CBaseFileSystem_Trace_FOpen = nullptr;
static void hook_CBaseFileSystem_HandleOpenRegularFile(CBaseFileSystem* _this, CFileOpenInfo& openInfo, bool bIsAbsolutePath)
{
	if (!func_CBaseFileSystem_FixUpPath)
		return detour_CBaseFileSystem_HandleOpenRegularFile.GetTrampoline<Symbols::CBaseFileSystem_HandleOpenRegularFile>()(_this, openInfo, bIsAbsolutePath);

	openInfo.m_pFileHandle = nullptr;

	// RaphaelIT7: BUG! Source apparently allows materials\\..\\backgrounds why?
	NormalizeGamePath( openInfo.m_AbsolutePath );

	Addon::FileSystem* m_AddonFileSystem = (Addon::FileSystem*)_this->Addons();

	// RaphaelIT7:
	// When mounting the map_pack.bsp we may try when reading it the absolute path! So we must lookup in workshop
	//bool bIsWorkshop = false;
	//if ( bIsAbsolutePath && !m_AddonFileSystem->ModPath().empty() ) // We check for empty as it may have not been set early on!
	//	bIsWorkshop = PathStartsWith( openInfo.m_AbsolutePath, m_AddonFileSystem->ModPath().c_str() );

	// RaphaelIT7:
	// We must use a different approach in HolyLib
	// as storing a field like m_bIsWorkshop in CSearchPath is highly unreliable for whatever stupid memory issue

	// GMod
	//if ( openInfo.m_pSearchPath && openInfo.m_pSearchPath->m_bIsWorkshop || bIsWorkshop )
	if ( !m_AddonFileSystem->ModPath().empty() && PathStartsWith( openInfo.m_AbsolutePath, m_AddonFileSystem->ModPath().c_str() ) )
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
		// Msg("Workshop lookup %s (%s)\n", openInfo.m_AbsolutePath, pHandle ? "true" : "false");
		return;
	}

	// RaphaelIT7:
	// We do not use the cache for absolute paths!
	// Absolute paths are used by GMod for example when mounting a gma
	// The path will be somewhere in the steamapps/workshop/4000 which we did not scan (and never will)
	FileCacheEntry eCacheEntry = FileCacheEntry::UNKNOWN;
	if ( !bIsAbsolutePath )
	{
		eCacheEntry = g_DiskFileTree.ContainsPath( openInfo.m_AbsolutePath );

		// openInfo.m_pFileName is a mess due to \\..\\ not yet being normalized!
		// eCacheEntry = openInfo.m_pSearchPath->ContainsPath( openInfo.m_pFileName );
		if ( eCacheEntry != FileCacheEntry::FILE && eCacheEntry != FileCacheEntry::UNKNOWN )
			return;
	}

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
		/*if ( eCacheEntry != FileCacheEntry::FILE && eCacheEntry != FileCacheEntry::UNKNOWN )
		{
			Warning(PROJECT_NAME " - filesystem: File exists on disk yet we said no? (%s)\n", openInfo.m_AbsolutePath);
			if (openInfo.m_pSearchPath)
			{
				Warning("Path: %s\n", openInfo.m_pSearchPath->GetPathString());
				Warning("ID: %s\n", openInfo.m_pSearchPath->GetPathIDString());
			}
		}*/

		openInfo.m_pFileHandle = new CFileHandle(_this);
		openInfo.m_pFileHandle->m_pFile = fp;
		openInfo.m_pFileHandle->m_type = FT_NORMAL;
		openInfo.m_pFileHandle->m_nLength = size;

		openInfo.SetResolvedFilename( openInfo.m_AbsolutePath );
		// Msg( "Opened file %s\n", openInfo.m_AbsolutePath );
		
		// LogFileOpen( "Loose", openInfo.m_pFileName, openInfo.m_AbsolutePath );

		// GMod - Returns on hit
		return;
	}

	// Msg( "Failed to open file %s\n", openInfo.m_AbsolutePath );

	// RaphaelIT7: If this happens then the file was removed from disk and we didn't know yet
	g_DiskFileTree.RemovePath( openInfo.m_AbsolutePath );
}

// RaphaelIT7: A special flag to mark the workshop/ path
#define PATH_FLAG_ISWORKSHOP (1<<9)

static Symbols::CFileHandle_Constructor func_CFileHandle_Constructor = nullptr;
CFileHandle::CFileHandle(CBaseFileSystem* fs)
{
	// Our Layout matches GMod so this should have no side effects :3
	func_CFileHandle_Constructor(this, fs);
}

static CSearchPath* g_pLastCreatedSearchPath = nullptr;
static Detouring::Hook detour_CBaseFileSystem_NewSearchPath;
CSearchPath* hook_CBaseFileSystem_NewSearchPath(void* _this, int addType)
{
	// RaphaelIT7:
	// We MUST do addType & 0x1FF as GMod uses a vague mask where we can easily corrupt the priority group if we don't clear the custom flag bits
	// This is since GMod only skips bit 8 when getting the priority group but they don't skip the bits after...
	// GMod uses 0xFFFFFEFF when it should be using 0xFE
	CSearchPath* pPath = (CSearchPath*)detour_CBaseFileSystem_NewSearchPath.GetTrampoline<Symbols::CBaseFileSystem_NewSearchPath>()(_this, addType & 0x1FF);
	pPath->m_bIsWorkshop = false;
	pPath->m_bTrackDisk = false;
	g_pLastCreatedSearchPath = pPath;

	g_pFullFileSystem = (CBaseFileSystem*)_this;

	return pPath;
}

static void AddSeperatorAndFixPath( char *str )
{
	char *lastChar = &str[strlen( str ) - 1];
	if( *lastChar != CORRECT_PATH_SEPARATOR && *lastChar != INCORRECT_PATH_SEPARATOR )
	{
		lastChar[1] = CORRECT_PATH_SEPARATOR;
		lastChar[2] = '\0';
	}
	V_FixSlashes( str );
}

static Detouring::Hook detour_CBaseFileSystem_AddSearchPathInternal;
void hook_CBaseFileSystem_AddSearchPathInternal(CBaseFileSystem* _this, const char *pPath, const char *pathID, SearchPathAdd_t addType, bool bAddPackFiles)
{
	g_pFullFileSystem = _this;

	detour_CBaseFileSystem_AddSearchPathInternal.GetTrampoline<Symbols::CBaseFileSystem_AddSearchPathInternal>()(_this, pPath, pathID, addType, bAddPackFiles);

	// Skip the only paths where we do not care
	if (V_stristr( pPath, ".bsp" ) || V_stristr( pPath, ".vpk" ))
		return;

	char newPath[ MAX_FILEPATH ];
	if ( Q_isempty( pPath ) )
	{
		newPath[0] = newPath[1] = '\0';
	}
	else
	{
		if ( IsX360() || Q_IsAbsolutePath( pPath ) )
		{
			V_strcpy_safe( newPath, pPath );
		}
		else
		{
			V_MakeAbsolutePath( newPath, sizeof(newPath), pPath );
		}
#ifdef _WIN32
		Q_strlower( newPath );
#endif
		AddSeperatorAndFixPath( newPath );
	}

	Addon::FileSystem* m_AddonFileSystem = (Addon::FileSystem*)_this->Addons();
	/*if ( !m_AddonFileSystem->ModPath().empty() ) // We check for empty as it may have not been set early on!
	{
		if ( PathStartsWith( newPath, m_AddonFileSystem->ModPath().c_str() ) )
		{
			DevMsg( PROJECT_NAME " - filesystem: Marked %s a workshop path\n", newPath );
			g_pLastCreatedSearchPath->m_bIsWorkshop = true;
		}
	}*/

	if ( V_IsAbsolutePath( g_pLastCreatedSearchPath->GetPathString() ) && (m_AddonFileSystem->ModPath().empty() || PathStartsWith( g_pLastCreatedSearchPath->GetPathString(), m_AddonFileSystem->ModPath().c_str() )) )
	{
		if (V_stricmp(g_pLastCreatedSearchPath->GetPathIDString(), "BASE_PATH") == 0)
			g_FileWatcherSystem.CreateWatcher( g_pLastCreatedSearchPath->GetPathString() );

		std::unique_lock<std::shared_mutex> lock(g_DiskFileTree.GetMutex());
		g_DiskFileTree.BuildTree( g_pLastCreatedSearchPath->GetPathString() );
	}
}

static void DumpFileTree(const CCommand &args)
{
	Msg("Filelist:\n");
	for (auto& [key, val] : g_DiskFileTree.GetFileList())
		Msg("\t%s (%i)\n", key.c_str(), (int)val);
}
static ConCommand dumpfiletree("holylib_filesystem_dumpfiletree", DumpFileTree, "Dumps the filetree", 0);

// Future note: When using an absolute path the search path should not be a packed file! And it doesn't matter what search path it is! Just not a pack!
static Detouring::Hook detour_CBaseFileSystem_FastFileTime;
static Symbols::CFileSystem_Stdio_FS_stat func_CFileSystem_Stdio_FS_stat = nullptr;
static Symbols::Addon_FileSystem_GetFileSize func_Addon_FileSystem_GetFileSize = nullptr;
static long hook_CBaseFileSystem_FastFileTime(CBaseFileSystem* _this, const CSearchPath* path, const char* pFileName)
{
	if (!func_CFileSystem_Stdio_FS_stat || !func_Addon_FileSystem_GetFileSize)
		return detour_CBaseFileSystem_FastFileTime.GetTrampoline<Symbols::CBaseFileSystem_FastFileTime>()(_this, path, pFileName);

	struct _stat buf;

	if ( path->GetPackFile() )
	{
		// If we found the file:
		if ( path->GetPackFile()->ContainsFile( pFileName ) )
		{
			return path->GetPackFile()->m_lPackFileTime;
		}
	}
#ifdef SUPPORT_PACKED_STORE
	else if ( path->GetPackedStore() )
	{
		// Hm, should we support this in some way?
		return 0L;
	}
#endif
	else
	{
		// Is it an absolute path?
		char pTmpFileName[ MAX_FILEPATH ]; 
		
		if ( V_IsAbsolutePath( pFileName ) )
		{
			V_strcpy_safe( pTmpFileName, pFileName );
		}
		else
		{
			ComposeSearchPath( pTmpFileName, sizeof( pTmpFileName ), path->GetPathString(), pFileName );
		}

		V_FixSlashes( pTmpFileName );
		// GMod
		//if ( path->m_bIsWorkshop )
		Addon::FileSystem* m_AddonFileSystem = (Addon::FileSystem*)_this->Addons();
		if ( !m_AddonFileSystem->ModPath().empty() && PathStartsWith( pTmpFileName, m_AddonFileSystem->ModPath().c_str() ) )
		{
			int64 iSize = func_Addon_FileSystem_GetFileSize( m_AddonFileSystem, pTmpFileName );
			if ( iSize >= 0 )
				return 1L;

			// RaphaelIT7:
			// Do not lookup on disk.
			return 0L;
		}

		// RaphaelIT7: We force lower for consistency!
		V_strlower( pTmpFileName );
		NormalizeGamePath( pTmpFileName );
		FileCacheEntry eCacheEntry = g_DiskFileTree.ContainsPath( pTmpFileName );

		// RaphaelIT7: We check == INVALID since FS_stat works on both file and folder so we must allow both!
		if ( eCacheEntry == FileCacheEntry::INVALID )
		{
			// RaphaelIT7: Debugging
			//if ( FS_stat( pTmpFileName, &buf ) != -1 )
			//	__debugbreak();

			return 0L;
		}

		if ( func_CFileSystem_Stdio_FS_stat( g_pFullFileSystem, pTmpFileName, &buf, nullptr ) != -1 )
		{
			return buf.st_mtime;
		}
#ifdef LINUX
		char caseFixedName[ MAX_PATH ];
		if ( findFileInDirCaseInsensitive_safe( pTmpFileName, caseFixedName ) &&
			 func_CFileSystem_Stdio_FS_stat( g_pFullFileSystem, caseFixedName, &buf, nullptr ) != -1 )
		{
			return buf.st_mtime;
		}
#endif

		g_DiskFileTree.RemovePath( pTmpFileName );
	}

	return ( 0L );
}

static Detouring::Hook detour_CBaseFileSystem_IsDirectory;
static Symbols::Addon_FileSystem_IsDirectory func_Addon_FileSystem_IsDirectory = nullptr;
static Symbols::CPackedStore_DirectoryEntryExists func_CPackedStore_DirectoryEntryExists = nullptr;
static bool hook_CBaseFileSystem_IsDirectory(CBaseFileSystem* _this, const char* pFileName, const char* pathID)
{
	if (!func_CFileSystem_Stdio_FS_stat || !func_CPackedStore_DirectoryEntryExists || !func_Addon_FileSystem_IsDirectory)
		return detour_CBaseFileSystem_IsDirectory.GetTrampoline<Symbols::CBaseFileSystem_IsDirectory>()(_this, pFileName, pathID);

	// Allow for UNC-type syntax to specify the path ID.
	struct	_stat buf;

	char pTempBuf[MAX_PATH];
	V_strcpy_safe( pTempBuf, pFileName );
	V_StripTrailingSlash( pTempBuf );
	pFileName = pTempBuf;

	// RaphaelIT7: Just to avoid weird issues
	NormalizeGamePath( pTempBuf );

	char tempPathID[MAX_PATH] = {0};
	// ParsePathID( pFileName, pathID, tempPathID );
	if ( V_IsAbsolutePath( pFileName ) )
	{
		if ( func_CFileSystem_Stdio_FS_stat( g_pFullFileSystem, pFileName, &buf, nullptr ) != -1 )
		{
			if ( buf.st_mode & _S_IFDIR )
				return true;
		}
		return false;
	}

	CSearchPathsIterator iter( _this, &pFileName, pathID, FILTER_CULLPACK );
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != nullptr; pSearchPath = iter.GetNext() )
	{
#ifdef SUPPORT_PACKED_STORE
		if ( pSearchPath->GetPackedStore() )
		{
			// GMod
			// ASM shows a +8 but I have no idea why
			if ( func_CPackedStore_DirectoryEntryExists( (void*)((char*)pSearchPath->GetPackedStore() + 8), pFileName ) )
				return true;
		}
		else
#endif // SUPPORT_PACKED_STORE
		{
			char pTmpFileName[ MAX_FILEPATH ];
			ComposeSearchPath( pTmpFileName, sizeof( pTmpFileName ), pSearchPath->GetPathString(), pFileName );
			V_FixSlashes( pTmpFileName );

			// GMod
			//if ( pSearchPath->m_bIsWorkshop )
			Addon::FileSystem* m_AddonFileSystem = (Addon::FileSystem*)_this->Addons();
			if (!m_AddonFileSystem->ModPath().empty() && PathStartsWith( pTmpFileName, m_AddonFileSystem->ModPath().c_str() ))
			{
				if ( func_Addon_FileSystem_IsDirectory( m_AddonFileSystem, pTmpFileName ) )
					return true;
			}
			else
			{
				// RaphaelIT7: We force lower for consistency!
				V_strlower( pTmpFileName );
				FileCacheEntry eCacheEntry = g_DiskFileTree.ContainsPath( pTmpFileName );

				// RaphaelIT7: We check == INVALID since FS_stat works on both file and folder so we must allow both!
				if ( eCacheEntry != FileCacheEntry::FOLDER && eCacheEntry != FileCacheEntry::UNKNOWN )
				{
					// RaphaelIT7: Debugging
					//if ( FS_stat( pTmpFileName, &buf ) != -1 )
					//	__debugbreak();

					continue;
				}

				// RaphaelIT7:
				// We can just return true since it's said to be a folder?
				// Verify: Lets be certain first before we truly just skip the disk check!
				// return true;
				if ( func_CFileSystem_Stdio_FS_stat( g_pFullFileSystem, pTmpFileName, &buf, nullptr ) != -1 )
				{
					if ( buf.st_mode & _S_IFDIR )
						return true;
				} else {
					// RaphaelIT7: As fallback since apparently it's no longer a folder?
					g_DiskFileTree.RemovePath( pTmpFileName );
				}
			}
		}
	}
	return false;
}

/*
 * This is the OpenForRead implementation but faster.
 */
static Detouring::Hook detour_CBaseFileSystem_OpenForRead;
FileHandle_t hook_CBaseFileSystem_OpenForRead(CBaseFileSystem* _this, const char *pFileNameT, const char *pOptions, unsigned flags, const char *pathID, char **ppszResolvedFilename)
{
	if (!func_CBaseFileSystem_FixUpPath)
		return detour_CBaseFileSystem_OpenForRead.GetTrampoline<Symbols::CBaseFileSystem_OpenForRead>()(_this, pFileNameT, pOptions, flags, pathID, ppszResolvedFilename);

	VPROF_BUDGET("HolyLib - CBaseFileSystem::OpenForRead", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	char pFileNameBuff[MAX_PATH];
	const char *pFileName = pFileNameBuff;

	func_CBaseFileSystem_FixUpPath(_this, pFileNameT, pFileNameBuff, sizeof(pFileNameBuff));

	FileHandle_t fh = detour_CBaseFileSystem_OpenForRead.GetTrampoline<Symbols::CBaseFileSystem_OpenForRead>()(_this, pFileNameT, pOptions, flags, pathID, ppszResolvedFilename);

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
static long hook_CBaseFileSystem_GetFileTime(IFileSystem* _this, const char *pFileNameT, const char *pPathID)
{
	if (!func_CBaseFileSystem_FixUpPath)
		return detour_CBaseFileSystem_GetFileTime.GetTrampoline<Symbols::CBaseFileSystem_GetFileTime>()(_this, pFileNameT, pPathID);

	VPROF_BUDGET("HolyLib - CBaseFileSystem::GetFileTime", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	// Fixes GetFileTime missing the caches since entries have different slashes
	char pFileNameBuff[MAX_PATH];
	const char *pFileName = pFileNameBuff;

	func_CBaseFileSystem_FixUpPath(_this, pFileNameT, pFileNameBuff, sizeof(pFileNameBuff));

	std::string_view strFileName = pFileName; // Workaround for now.
	if (pPathID && V_stricmp(pPathID, "lsv") == 0 && holylib_filesystem_fixgmodpath.GetBool()) // Some weird things happen in the lsv path.  
		strFileName = fixGamemodePath(strFileName);

	pFileName = strFileName.data();
	if (holylib_filesystem_skipinvalidluapaths.GetBool())
	{
		if (strFileName.rfind("include" FILEPATH_SLASH "include" FILEPATH_SLASH) == 0)
			return 0L;
	}

	return detour_CBaseFileSystem_GetFileTime.GetTrampoline<Symbols::CBaseFileSystem_GetFileTime>()(_this, pFileName, pPathID);
}

static Detouring::Hook detour_CBaseFileSystem_RelativePathToFullPath;
static Symbols::Addon_FileSystem_ResolveFile func_Addon_FileSystem_ResolveFile = nullptr;
static const char* hook_CBaseFileSystem_RelativePathToFullPath( CBaseFileSystem* _this, const char *pFileName, const char *pPathID, char *pDest, int maxLenInChars, PathTypeFilter_t pathFilter, PathTypeQuery_t *pPathType )
{
	//if (!func_CBaseFileSystem_FixUpPath || !func_Addon_FileSystem_ResolveFile || !func_CFileSystem_Stdio_FS_stat)
	//	return detour_CBaseFileSystem_RelativePathToFullPath.GetTrampoline<Symbols::CBaseFileSystem_RelativePathToFullPath>()(_this, pFileName, pPathID, pDest, maxLenInChars, pathFilter, pPathType);

	struct _stat buf;
	if ( pPathType )
		*pPathType = PATH_IS_NORMAL;

	// Convert filename to lowercase.  All files in the
	// game logical filesystem must be accessed by lowercase name
	char szLowercaseFilename[ MAX_PATH ];
	func_CBaseFileSystem_FixUpPath( _this, pFileName, szLowercaseFilename, sizeof(szLowercaseFilename) );
	pFileName = szLowercaseFilename;

	// RaphaelIT7: Just to avoid weird issues
	NormalizeGamePath( szLowercaseFilename );

	// Fill in the default in case it's not found...
	V_strncpy( pDest, pFileName, maxLenInChars );

	CSearchPathsIterator iter( _this, &pFileName, pPathID, pathFilter );
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != nullptr; pSearchPath = iter.GetNext() )
	{
		CPackFile *pPack = pSearchPath->GetPackFile();
		if ( pPack )
		{
			if ( pPack->ContainsFile( pFileName ) )
			{
				if ( pPathType )
				{
					if ( pPack->m_bIsMapPath )
					{
						*pPathType |= PATH_IS_MAPPACKFILE;
					}
					else
					{
						*pPathType |= PATH_IS_PACKFILE;
					}
					if ( pSearchPath->m_bIsRemotePath )
					{
						*pPathType |= PATH_IS_REMOTE;
					}
				}

				// form an encoded absolute path that can be decoded by our FS as pak based
				const char *pszPackName = pPack->m_ZipName.String();
				intp len = V_strlen( pszPackName );
				intp nTotalLen = len + 1 + V_strlen( pFileName );
				if ( nTotalLen >= maxLenInChars )
				{
					::Warning( "File %s was found in %s, but resulting abs filename won't fit in callers buffer of %d bytes\n",
						pFileName, pszPackName, maxLenInChars );
					Assert( false );
					return nullptr;
				}

				V_strncpy( pDest, pszPackName, maxLenInChars );
				V_AppendSlash( pDest, maxLenInChars );
				V_strncat( pDest, pFileName, maxLenInChars ); 
				Assert( V_strlen( pDest ) == nTotalLen );
				return pDest;
			}

			continue;
		}

		// Found in VPK?
#ifdef SUPPORT_PACKED_STORE
			CPackedStore *pVPK = pSearchPath->GetPackedStore();
			if ( pVPK )
			{
				CPackedStoreFileHandle vpkHandle = pVPK->OpenFile( pFileName );
				if ( vpkHandle )
				{
					const char *pszVpkName = vpkHandle.m_pOwner->FullPathName();
					Assert( V_GetFileExtension( pszVpkName ) != nullptr );

					intp len = V_strlen( pszVpkName );
					intp nTotalLen = len + 1 + V_strlen( pFileName );
					if ( nTotalLen >= maxLenInChars )
					{
						::Warning( "File %s was found in %s, but resulting abs filename won't fit in callers buffer of %d bytes\n",
							pFileName, pszVpkName, maxLenInChars );
						Assert( false );
						return nullptr;
					}

					V_strncpy( pDest, pszVpkName, maxLenInChars );
					V_AppendSlash( pDest, maxLenInChars );
					V_strncat( pDest, pFileName, maxLenInChars );
					V_FixSlashes( pDest );
					return pDest;
				}
				continue;
			}
#endif

		char pTmpFileName[ MAX_FILEPATH ];
		ComposeSearchPath( pTmpFileName, sizeof( pTmpFileName ), pSearchPath->GetPathString(), pFileName );
		V_FixSlashes( pTmpFileName );

		// GMod
		// if ( pSearchPath->m_bIsWorkshop )
		Addon::FileSystem* m_AddonFileSystem = (Addon::FileSystem*)_this->Addons();
		if ( !m_AddonFileSystem->ModPath().empty() && PathStartsWith( pTmpFileName, m_AddonFileSystem->ModPath().c_str() ) )
		{
			std::string strFullFileName = func_Addon_FileSystem_ResolveFile( m_AddonFileSystem, pTmpFileName );
			if ( !strFullFileName.empty() )
			{
				V_strncpy( pDest, strFullFileName.c_str(), maxLenInChars );
				return pDest;
			}
		}

		// RaphaelIT7: We force lower for consistency!
		V_strlower( pTmpFileName );
		FileCacheEntry eCacheEntry = g_DiskFileTree.ContainsPath( pTmpFileName );

		// RaphaelIT7: We check == INVALID since FS_stat works on both file and folder so we must allow both!
		if ( eCacheEntry == FileCacheEntry::INVALID )
		{
			// RaphaelIT7: Debugging
			//if ( FS_stat( pTmpFileName, &buf ) != -1 )
			//	__debugbreak();

			continue;
		}

		if ( func_CFileSystem_Stdio_FS_stat( _this, pTmpFileName, &buf, nullptr ) != -1 )
		{
			V_strncpy( pDest, pTmpFileName, maxLenInChars );
			if ( pPathType && pSearchPath->m_bIsRemotePath )
			{
				*pPathType |= PATH_IS_REMOTE;
			}
			return pDest;
		} else {
			g_DiskFileTree.RemovePath( pTmpFileName );
		}
	}

	// not found
	return nullptr;
}

static Symbols::CBaseFileSystem_GetWritePath func_CBaseFileSystem_GetWritePath = nullptr;
inline void ComputeFullWritePath( CBaseFileSystem* _this, char* pDest, int maxlen, const char *pRelativePath, const char *pWritePathID )
{
	Q_strncpy( pDest, func_CBaseFileSystem_GetWritePath( _this, pRelativePath, pWritePathID ), maxlen );
	Q_strncat( pDest, pRelativePath, maxlen, COPY_ALL_CHARACTERS );
	Q_FixSlashes( pDest );
}

static Detouring::Hook detour_CBaseFileSystem_OpenForWrite;
static FileHandle_t hook_CBaseFileSystem_OpenForWrite( CBaseFileSystem* _this, const char *pFileName, const char *pOptions, const char *pathID )
{
	if (!func_CBaseFileSystem_GetWritePath)
		return detour_CBaseFileSystem_OpenForWrite.GetTrampoline<Symbols::CBaseFileSystem_OpenForWrite>()(_this, pFileName, pOptions, pathID);

	FileHandle_t hFileHandle = detour_CBaseFileSystem_OpenForWrite.GetTrampoline<Symbols::CBaseFileSystem_OpenForWrite>()(_this, pFileName, pOptions, pathID);
	
	const char *pTmpFileName;
	char szScratchFileName[MAX_PATH];
	if ( V_IsAbsolutePath( pFileName ) )
	{
		pTmpFileName = pFileName;
	}
	else
	{
		ComputeFullWritePath( _this, szScratchFileName, sizeof( szScratchFileName ), pFileName, pathID );
		pTmpFileName = szScratchFileName; 
	}
	
	if (hFileHandle)
		g_DiskFileTree.AddPath( pTmpFileName, FileCacheEntry::FILE );

	return hFileHandle;
}

static Detouring::Hook detour_CBaseFileSystem_CreateDirHierarchy;
void hook_CBaseFileSystem_CreateDirHierarchy( CBaseFileSystem* _this, const char *pRelativePathT, const char *pathID )
{
	if (!func_CBaseFileSystem_FixUpPath || !func_CBaseFileSystem_GetWritePath)
	{
		detour_CBaseFileSystem_CreateDirHierarchy.GetTrampoline<Symbols::CBaseFileSystem_CreateDirHierarchy>()(_this, pRelativePathT, pathID);
		return;
	}

	char pRelativePathBuff[ MAX_PATH ];
	const char *pRelativePath = pRelativePathBuff;

	func_CBaseFileSystem_FixUpPath( _this, pRelativePathT, pRelativePathBuff, sizeof( pRelativePathBuff ) );

	char szScratchFileName[MAX_PATH];
	if ( !V_IsAbsolutePath( pRelativePath ) )
	{
		Assert( pathID );

		ComputeFullWritePath( _this, szScratchFileName, sizeof( szScratchFileName ), pRelativePath, pathID );
	}
	else
	{
		V_strcpy_safe( szScratchFileName, pRelativePath );
	}

	intp len = V_strlen( szScratchFileName ) + 1;
	char *end = szScratchFileName + len;
	char *s = szScratchFileName;
	while ( s < end )
	{
		if ( *s == CORRECT_PATH_SEPARATOR && s != szScratchFileName && ( IsLinux() || *( s - 1 ) != ':' ) )
		{
			*s = '\0';

#if defined( _WIN32 )
			if ( _mkdir( szScratchFileName ) && errno != EEXIST )
#elif defined( POSIX )
			if ( mkdir( szScratchFileName, S_IRWXU |  S_IRGRP |  S_IROTH ) && errno != EEXIST )// owner has rwx, rest have r
#endif
			{
				::Warning( "Unable to create file or directory '%s' in hierarchy '%s': %s.\n",
					szScratchFileName,
					pRelativePathT,
					std::generic_category().message(errno).c_str() );
			} else {
				g_DiskFileTree.AddPath( szScratchFileName, FileCacheEntry::FOLDER );
			}

			*s = CORRECT_PATH_SEPARATOR;
		}

		s++;
	}

#if defined( _WIN32 )
	if ( _mkdir( szScratchFileName ) && errno != EEXIST )
#elif defined( POSIX )
	if ( mkdir( szScratchFileName, S_IRWXU |  S_IRGRP |  S_IROTH ) && errno != EEXIST )
#endif
	{
		::Warning( "Unable to create file '%s' in hierarchy '%s': %s.\n",
			szScratchFileName,
			pRelativePathT,
			std::generic_category().message(errno).c_str() );
	} else {
		g_DiskFileTree.AddPath( szScratchFileName, FileCacheEntry::FOLDER );
	}
}

static Detouring::Hook detour_CBaseFileSystem_RenameFile;
bool hook_CBaseFileSystem_RenameFile( CBaseFileSystem* _this, char const *pOldPath, char const *pNewPath, const char *pathID )
{
	if (!func_CBaseFileSystem_FixUpPath || !func_CBaseFileSystem_GetWritePath)
		return detour_CBaseFileSystem_RenameFile.GetTrampoline<Symbols::CBaseFileSystem_RenameFile>()(_this, pOldPath, pNewPath, pathID);

	// Allow for UNC-type syntax to specify the path ID.
	char pPathIdCopy[MAX_PATH];
	const char *pOldPathId = pathID;
	if ( pathID )
	{
		V_strcpy_safe( pPathIdCopy, pathID );
		pOldPathId = pPathIdCopy;
	}

	char pNewFileName[ MAX_PATH ];
	char szScratchFileName[MAX_PATH];

	// The source file may be in a fallback directory, so just resolve the actual path, don't assume pathid...
	_this->RelativePathToFullPath( pOldPath, pOldPathId, szScratchFileName, sizeof(szScratchFileName) );

	// Figure out the dest path
	if ( !V_IsAbsolutePath( pNewPath ) )
		ComputeFullWritePath( _this, pNewFileName, sizeof( pNewFileName ), pNewPath, pathID );
	else
		V_strcpy_safe( pNewFileName, pNewPath );

	// RaphaelIT7: We force lower for consistency!
	V_strlower( pNewFileName );
	NormalizeGamePath( pNewFileName );
	V_strlower( szScratchFileName );
	NormalizeGamePath( szScratchFileName );

	// Make sure the directory exitsts, too
	char pPathOnly[ MAX_PATH ];
	V_strcpy_safe( pPathOnly, pNewFileName );
	V_StripFilename( pPathOnly );
	hook_CBaseFileSystem_CreateDirHierarchy( _this, pPathOnly, pathID );

	// Now copy the file over.
	if ( rename( szScratchFileName, pNewFileName ) )
	{
		::Warning( "Unable to rename file '%s' to '%s': %s.\n",
			szScratchFileName,
			pNewFileName,
			std::generic_category().message(errno).c_str() );
		return false;
	} else
		g_DiskFileTree.RenamePath( szScratchFileName, pNewFileName );

	return true;
}

void CFileSystemModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	if (Util::GetGModVersionNum() < 260718)
	{
		Warning(PROJECT_NAME " - filesystem: This GMod version is not supported!\n");
		return;
	}

	Addon::FileSystem* m_AddonFileSystem = (Addon::FileSystem*)g_pFullFileSystem->Addons();
	FOR_EACH_LL_(((CBaseFileSystem*)g_pFullFileSystem)->m_SearchPaths, pSearchPath)
	{
		if ( !m_AddonFileSystem->ModPath().empty() ) // We check for empty as it may have not been set early on!
		{
			if ( PathStartsWith( pSearchPath->GetPathString(), m_AddonFileSystem->ModPath().c_str() ) )
			{
				DevMsg(PROJECT_NAME " - filesystem: Init marked path %s as workshop\n", pSearchPath->GetPathString());
				pSearchPath->m_bIsWorkshop = true;
				continue;
			}
		}

		if ( V_IsAbsolutePath( pSearchPath->GetPathString() ) )
			g_DiskFileTree.BuildTree( pSearchPath->GetPathString() );
	}
}

CUtlSymbolTableMT* g_pPathIDTable;
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
	DETOUR_THISCALL_ADDRETFUNC2( hook_CBaseFileSystem_IsDirectory, bool, IsDirectory, CBaseFileSystem*, const char*, const char* );
	DETOUR_THISCALL_ADDRETFUNC2( hook_CBaseFileSystem_FastFileTime, long, FastFileTime, CBaseFileSystem*, const CSearchPath*, const char* );
	DETOUR_THISCALL_ADDRETFUNC2( hook_CBaseFileSystem_GetFileTime, long, GetFileTime, CBaseFileSystem*, const char*, const char* );
	DETOUR_THISCALL_ADDRETFUNC3( hook_CBaseFileSystem_RenameFile, bool, RenameFile, CBaseFileSystem*, const char*, const char*, const char*);
	DETOUR_THISCALL_ADDFUNC2( hook_CBaseFileSystem_HandleOpenRegularFile, HandleOpenRegularFile, CBaseFileSystem*, CFileOpenInfo&, bool);
	DETOUR_THISCALL_ADDFUNC1( hook_CBaseFileSystem_NewSearchPath, NewSearchPath, CBaseFileSystem*, int );
	DETOUR_THISCALL_ADDFUNC4( hook_CBaseFileSystem_AddSearchPathInternal, AddSearchPathInternal, CBaseFileSystem*, const char*, const char*, SearchPathAdd_t, bool );
	DETOUR_THISCALL_ADDFUNC3( hook_CBaseFileSystem_OpenForWrite, OpenForWrite, CBaseFileSystem*, const char*, const char*, const char*);
	DETOUR_THISCALL_ADDFUNC2( hook_CBaseFileSystem_CreateDirHierarchy, CreateDirHierarchy, CBaseFileSystem*, const char*, const char*);
DETOUR_THISCALL_FINISH();
#endif

void CFileSystemModule::InitDetour(bool bPreServer)
{
	if (!bPreServer)
		return;

	if (Util::GetGModVersionNum() < 260718)
	{
		Warning(PROJECT_NAME " - filesystem: This GMod version is not supported!\n");
		return;
	}

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
	/*Detour::Create(
		&detour_CBaseFileSystem_OpenForRead, "CBaseFileSystem::OpenForRead",
		filesystem_loader.GetModule(), Symbols::CBaseFileSystem_OpenForReadSym,
		(void*)DETOUR_THISCALL(hook_CBaseFileSystem_OpenForRead, OpenForRead), m_pID
	);*/

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
		&detour_CBaseFileSystem_HandleOpenRegularFile, "CBaseFileSystem::HandleOpenRegularFile",
		filesystem_loader.GetModule(), Symbols::CBaseFileSystem_HandleOpenRegularFileSym,
		(void*)DETOUR_THISCALL(hook_CBaseFileSystem_HandleOpenRegularFile, HandleOpenRegularFile), m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_NewSearchPath, "CBaseFileSystem::NewSearchPath",
		filesystem_loader.GetModule(), Symbols::CBaseFileSystem_NewSearchPathSym,
		(void*)DETOUR_THISCALL(hook_CBaseFileSystem_NewSearchPath, NewSearchPath), m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_AddSearchPathInternal, "CBaseFileSystem::AddSearchPathInternal",
		filesystem_loader.GetModule(), Symbols::CBaseFileSystem_AddSearchPathInternalSym,
		(void*)DETOUR_THISCALL(hook_CBaseFileSystem_AddSearchPathInternal, AddSearchPathInternal), m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_OpenForWrite, "CBaseFileSystem::OpenForWrite",
		filesystem_loader.GetModule(), Symbols::CBaseFileSystem_OpenForWriteSym,
		(void*)DETOUR_THISCALL(hook_CBaseFileSystem_OpenForWrite, OpenForWrite), m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_RenameFile, "CBaseFileSystem::RenameFile",
		filesystem_loader.GetModule(), Symbols::CBaseFileSystem_RenameFileSym,
		(void*)DETOUR_THISCALL(hook_CBaseFileSystem_RenameFile, RenameFile), m_pID
	);

	Detour::Create(
		&detour_CBaseFileSystem_CreateDirHierarchy, "CBaseFileSystem::CreateDirHierarchy",
		filesystem_loader.GetModule(), Symbols::CBaseFileSystem_CreateDirHierarchySym,
		(void*)DETOUR_THISCALL(hook_CBaseFileSystem_CreateDirHierarchy, CreateDirHierarchy), m_pID
	);

	func_CBaseFileSystem_CSearchPath_GetDebugString = (Symbols::CBaseFileSystem_CSearchPath_GetDebugString)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::CBaseFileSystem_CSearchPath_GetDebugStringSym);
	Detour::CheckFunction((void*)func_CBaseFileSystem_CSearchPath_GetDebugString, "CBaseFileSystem::CSearchPath::GetDebugString");

	func_CBaseFileSystem_FixUpPath = (Symbols::CBaseFileSystem_FixUpPath)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::CBaseFileSystem_FixUpPathSym);
	Detour::CheckFunction((void*)func_CBaseFileSystem_FixUpPath, "CBaseFileSystem::FixUpPath");

	func_CBaseFileSystem_Trace_FOpen = (Symbols::CBaseFileSystem_Trace_FOpen)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::CBaseFileSystem_Trace_FOpenSym);
	Detour::CheckFunction((void*)func_CBaseFileSystem_Trace_FOpen, "CBaseFileSystem::Trace_FOpen");

	func_Addon_FileHandle_Size = (Symbols::Addon_FileHandle_Size)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::Addon_FileHandle_SizeSym);
	Detour::CheckFunction((void*)func_Addon_FileHandle_Size, "Addon::FileHandle::Size");

	func_Addon_FileSystem_GetFileEntry = (Symbols::Addon_FileSystem_GetFileEntry)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::Addon_FileSystem_GetFileEntrySym);
	Detour::CheckFunction((void*)func_Addon_FileSystem_GetFileEntry, "Addon::FileSystem::GetFileEntry");

	func_Addon_FileSystem_ResolveFile = (Symbols::Addon_FileSystem_ResolveFile)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::Addon_FileSystem_ResolveFileSym);
	Detour::CheckFunction((void*)func_Addon_FileSystem_ResolveFile, "Addon::FileSystem::ResolveFile");

	func_Addon_FileSystem_GetFileSize = (Symbols::Addon_FileSystem_GetFileSize)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::Addon_FileSystem_GetFileSizeSym);
	Detour::CheckFunction((void*)func_Addon_FileSystem_GetFileSize, "Addon::FileSystem::GetFileSize");

	func_Addon_FileSystem_IsDirectory = (Symbols::Addon_FileSystem_IsDirectory)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::Addon_FileSystem_IsDirectorySym);
	Detour::CheckFunction((void*)func_Addon_FileSystem_IsDirectory, "Addon::FileSystem::IsDirectory");

	func_CFileSystem_Stdio_FS_FindFirstFile = (Symbols::CFileSystem_Stdio_FS_FindFirstFile)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::CFileSystem_Stdio_FS_FindFirstFileSym);
	Detour::CheckFunction((void*)func_CFileSystem_Stdio_FS_FindFirstFile, "CFileSystem_Stdio::FS_FindFirstFile");

	func_CFileSystem_Stdio_FS_FindNextFile = (Symbols::CFileSystem_Stdio_FS_FindNextFile)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::CFileSystem_Stdio_FS_FindNextFileSym);
	Detour::CheckFunction((void*)func_CFileSystem_Stdio_FS_FindNextFile, "CFileSystem_Stdio::FS_FindNextFile");

	func_CFileSystem_Stdio_FS_FindClose = (Symbols::CFileSystem_Stdio_FS_FindClose)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::CFileSystem_Stdio_FS_FindCloseSym);
	Detour::CheckFunction((void*)func_CFileSystem_Stdio_FS_FindClose, "CFileSystem_Stdio::FS_FindClose");

	func_CFileSystem_Stdio_FS_stat = (Symbols::CFileSystem_Stdio_FS_stat)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::CFileSystem_Stdio_FS_statSym);
	Detour::CheckFunction((void*)func_CFileSystem_Stdio_FS_stat, "CFileSystem_Stdio::FS_stat");

	func_CBaseFileSystem_GetWritePath = (Symbols::CBaseFileSystem_GetWritePath)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::CBaseFileSystem_GetWritePathSym);
	Detour::CheckFunction((void*)func_CBaseFileSystem_GetWritePath, "CBaseFileSystem::GetWritePath");

	func_CPackedStore_DirectoryEntryExists = (Symbols::CPackedStore_DirectoryEntryExists)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::CPackedStore_DirectoryEntryExistsSym);
	Detour::CheckFunction((void*)func_CPackedStore_DirectoryEntryExists, "CPackedStore::DirectoryEntryExists");

	func_CFileHandle_Constructor = (Symbols::CFileHandle_Constructor)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::CFileHandle_ConstructorSym);
	Detour::CheckFunction((void*)func_CFileHandle_Constructor, "CFileHandle::CFileHandle");

#if defined(ARCHITECTURE_X86) && defined(SYSTEM_LINUX)
	g_pPathIDTable = Detour::ResolveSymbol<CUtlSymbolTableMT>(filesystem_loader, Symbols::g_PathIDTableSym);
#else
	g_pPathIDTable = Detour::ResolveSymbolWithOffset<CUtlSymbolTableMT>(filesystem_loader.GetModule(), Symbols::g_PathIDTableSym);
#endif
	Detour::CheckValue("get class", "g_PathIDTable", g_pPathIDTable != nullptr);

	if (g_pPathIDTable)
	{
		CBaseFileSystem::m_BSPPathID = g_pPathIDTable->AddString( "BSP" );
		CBaseFileSystem::m_GamePathID = g_pPathIDTable->AddString( "GAME" );
	}
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
	std::string strFileName;
	std::string strPathID;
	GarrysMod::Lua::ILuaInterface* luaState;
};

class LuaFileSystemModuleData : public Lua::ModuleData
{
public:
	std::mutex callbacksMutex;
	std::queue<IAsyncFile*> callbacks;
};

LUA_GetModuleData(LuaFileSystemModuleData, g_pFileSystemModule, Filesystem);

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

		if (!Lua::IsValidLuaState(async->luaState))
			return; // Just to be sure as I don't trust the filesystem

		Lua::ScopedThreadAccess threadAccess;
		auto pData = GetFilesystemLuaData(async->luaState);
		if (!pData)
			return;

		std::lock_guard<std::mutex> lock(pData->callbacksMutex);
		pData->callbacks.push(async);
	} else
		Msg(PROJECT_NAME " - filesystem: file.AsyncRead Invalid request? (%s, %s)\n", request.pszFilename, request.pszPathID);
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
	request->pfnCallback = AsyncCallback;
	request->flags = sync ? FSASYNC_FLAGS_SYNC : 0;

	IAsyncFile* file = new IAsyncFile;
	file->callback = reference;
	file->req = request;
	file->strFileName = fileName;
	file->strPathID = gamePath;
	file->luaState = LUA;

	request->pszFilename = file->strFileName.c_str();
	request->pszPathID = file->strPathID.c_str();
	request->pContext = file;

	LUA->PushNumber(g_pFullFileSystem->AsyncReadMultiple(request, 1));
	return 1;
}

void FileAsyncReadThink(GarrysMod::Lua::ILuaInterface* pLua)
{
	VPROF_BUDGET("HolyLib - FileAsyncReadThink", VPROF_BUDGETGROUP_HOLYLIB);

	auto pData = GetFilesystemLuaData(pLua);
	if (!pData)
		return;
	
	while (true)
	{
		IAsyncFile* file = nullptr;
		{
			std::lock_guard<std::mutex> lock(pData->callbacksMutex);
			if (pData->callbacks.empty())
				break;

			file = pData->callbacks.front();
			pData->callbacks.pop();
		}

		Lua::ReferencePush(pLua, file->callback);
		pLua->PushString(file->req->pszFilename);
		pLua->PushString(file->req->pszPathID);
		pLua->PushNumber(file->status);
		pLua->PushString(file->content);
		pLua->CallFunctionProtected(4, 0, true);
		Util::ReferenceFree(pLua, file->callback, "FileAsyncReadThink");

		delete file->req;
		delete file;
	}
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
			Lua::RawSetI(LUA, -2, ++i);
		}
	}

	LUA->CreateTable();
	if (folders.size() > 0) {
		int i = 0;
		for (std::string folder : folders)
		{
			LUA->PushString(folder.c_str());
			Lua::RawSetI(LUA, -2, ++i);
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

	if (!func_CFileSystem_Stdio_FS_stat)
		LUA->ThrowError("Failed to load CFileSystem_Stdio::FS_stat");

	struct _stat buf;
	char pTmpFileName[MAX_PATH];
	if (g_pFullFileSystem->RelativePathToFullPath(filePath, gamePath, pTmpFileName, MAX_PATH))
	{
		if(func_CFileSystem_Stdio_FS_stat( g_pFullFileSystem, pTmpFileName, &buf, nullptr) != -1)
		{
			LUA->PushNumber((double)buf.st_ctime);
			return 1;
		}
	}
	
	LUA->PushNumber(0);
	return 1;
}

LUA_FUNCTION_STATIC(filesystem_TimeAccessed)
{
	const char* filePath = LUA->CheckString(1);
	const char* gamePath = LUA->CheckStringOpt(2, "GAME");
	
	if (!func_CFileSystem_Stdio_FS_stat)
		LUA->ThrowError("Failed to load CFileSystem_Stdio::FS_stat");

	struct _stat buf;
	char pTmpFileName[MAX_PATH];
	if (g_pFullFileSystem->RelativePathToFullPath(filePath, gamePath, pTmpFileName, MAX_PATH))
	{
		if(func_CFileSystem_Stdio_FS_stat( g_pFullFileSystem, pTmpFileName, &buf, nullptr) != -1)
		{
			LUA->PushNumber((double)buf.st_atime);
			return 1;
		}
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
			Lua::RawSetI(LUA, -2, ++idx);
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

	Lua::GetLuaData(pLua)->SetModuleData(m_pID, new LuaFileSystemModuleData);

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
	g_pFullFileSystem->AsyncFinishAll(); // So that we can finish all Read Think calls
	FileAsyncReadThink(pLua);

	Util::NukeTable(pLua, "filesystem");
}