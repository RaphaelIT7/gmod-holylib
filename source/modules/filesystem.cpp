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
#include "unordered_stuff.h"

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

#if SYSTEM_WINDOWS
#define FILEPATH_SLASH "\\"
#define FILEPATH_SLASH_CHAR '\\'
#else
#define FILEPATH_SLASH "/"
#define FILEPATH_SLASH_CHAR '/'
#endif

enum class FileCacheEntry : unsigned char
{
	UNKNOWN = 255, // if returned then check disk? (Exists just as a fallback for now)
	INVALID = 0, // Does not exist
	FILE,
	FOLDER,
};

// RaphaelIT7:
// For GMod's scale this will be a lot more complex than REngine...
// Fun :)
class CSearchPath;
class CDiskFileTree : public CRefCounted<CRefCountServiceMT>
{
public:
	void BuildTree( const char *pszRoot );
	FileCacheEntry ContainsPath( const char *pszAbsolutePath ) const;

	void AddPath( const char *pszAbsolutePath, FileCacheEntry type );
	void RemovePath( const char *pszAbsolutePath );
	void RenamePath( const char *pszOldAbsolutePath, const char *pszNewAbsolutePath );

private:
	void RecursiveTraverse( const char *pszFolderPath );

	// We use StringHash & StringEq so that when searching we do not allocate an std::string
	unordered_map<std::string, FileCacheEntry, StringHash, StringEq> m_FileList;
};

void CDiskFileTree::BuildTree( const char *pszRoot )
{
	if ( V_IsAbsolutePath( pszRoot ) )
	{
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
}

FileCacheEntry CDiskFileTree::ContainsPath( const char *pszAbsolutePath ) const
{
	if ( !holylib_filesystem_searchcache.GetBool() )
		return FileCacheEntry::UNKNOWN;

	auto it = m_FileList.find( pszAbsolutePath );
	if ( it != m_FileList.end() )
		return it->second;

	// RaphaelIT7: BUG! If we print anything we crash due to a stackoverflow in tier0? Something with output!
	//Msg( "Failed to find %s\n", pszAbsolutePath );
	return FileCacheEntry::INVALID;
}

// RaphaelIT7 (ToDo): We need a shared mutex!
void CDiskFileTree::AddPath( const char *pszAbsolutePath, FileCacheEntry type )
{
	auto it = m_FileList.find( pszAbsolutePath );
	if ( it == m_FileList.end() )
		m_FileList[pszAbsolutePath] = type;
}

void CDiskFileTree::RemovePath( const char *pszAbsolutePath )
{
	auto it = m_FileList.find( pszAbsolutePath );
	if ( it != m_FileList.end() )
		m_FileList.erase( it );
}

void CDiskFileTree::RenamePath( const char *pszOldAbsolutePath, const char *pszNewAbsolutePath )
{
	auto it = m_FileList.find( pszOldAbsolutePath );
	if ( it == m_FileList.end() )
		return; // Lies! ToDo: How should we handle this?

	m_FileList[ pszNewAbsolutePath ] = it->second;
	m_FileList.erase( it );
}

static Symbols::CFileSystem_Stdio_FS_FindFirstFile func_CFileSystem_Stdio_FS_FindFirstFile = nullptr;
static Symbols::CFileSystem_Stdio_FS_FindNextFile func_CFileSystem_Stdio_FS_FindNextFile = nullptr;
static Symbols::CFileSystem_Stdio_FS_FindClose func_CFileSystem_Stdio_FS_FindClose = nullptr;
// RaphaelIT7:
// This is expensive! A trade of startup time vs runtime performance
// ToDo: Check out if we can improve memory usage
void CDiskFileTree::RecursiveTraverse( const char *pszFolderPath )
{
	// If we have a entry then we already are tracking this one
	if ( m_FileList.find( pszFolderPath ) != m_FileList.end() )
		return;

	char szSearchPath[MAX_PATH];
	V_snprintf( szSearchPath, sizeof( szSearchPath ), "%s/*", pszFolderPath );

	WIN32_FIND_DATA findData;
	HANDLE hFind = func_CFileSystem_Stdio_FS_FindFirstFile( g_pFullFileSystem, szSearchPath, &findData );
	if ( hFind == INVALID_HANDLE_VALUE )
		return;

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
		if ( bDirectory )
			RecursiveTraverse( szFullPath );

		m_FileList.emplace( szFullPath, bDirectory ? FileCacheEntry::FOLDER : FileCacheEntry::FILE );
	} while ( func_CFileSystem_Stdio_FS_FindNextFile( g_pFullFileSystem, hFind, &findData ) );

	func_CFileSystem_Stdio_FS_FindClose( g_pFullFileSystem, hFind );
}

static CDiskFileTree g_pDiskFileTree;

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

static CSearchPath* g_pLastCreatedSearchPath = nullptr;
static Detouring::Hook detour_CBaseFileSystem_NewSearchPath;
CSearchPath* hook_CBaseFileSystem_NewSearchPath(void* _this, int addType)
{
	// RaphaelIT7:
	// We MUST do addType & 0x1FF as GMod uses a vague mask where we can easily corrupt the priority group if we don't clear the custom flag bits
	// This is since GMod only skips bit 8 when getting the priority group but they don't skip the bits after...
	// GMod uses 0xFFFFFEFF when it should be using 0xFE
	CSearchPath* pPath = (CSearchPath*)detour_CBaseFileSystem_NewSearchPath.GetTrampoline<Symbols::CBaseFileSystem_NewSearchPath>()(_this, addType & 0x1FF);
	pPath->m_bIsWorkshop = (addType & PATH_FLAG_ISWORKSHOP) != 0;
	g_pLastCreatedSearchPath = pPath;

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
	if ( !m_AddonFileSystem->ModPath().empty() ) // We check for empty as it may have not been set early on!
	{
		if ( PathStartsWith( newPath, m_AddonFileSystem->ModPath().c_str() ) )
			g_pLastCreatedSearchPath->m_bIsWorkshop = true;
	}

	if ( V_IsAbsolutePath( g_pLastCreatedSearchPath->GetPathString() ) )
		g_pDiskFileTree.BuildTree( g_pLastCreatedSearchPath->GetPathString() );
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

static Detouring::Hook detour_CBaseFileSystem_RelativePathToFullPath;
static Symbols::Addon_FileSystem_ResolveFile func_Addon_FileSystem_ResolveFile = nullptr;
static const char* hook_CBaseFileSystem_RelativePathToFullPath( CBaseFileSystem* _this, const char *pFileName, const char *pPathID, char *pDest, int maxLenInChars, PathTypeFilter_t pathFilter, PathTypeQuery_t *pPathType )
{
	struct _stat buf;

	if ( pPathType )
	{
		*pPathType = PATH_IS_NORMAL;
	}

	// Convert filename to lowercase.  All files in the
	// game logical filesystem must be accessed by lowercase name
	char szLowercaseFilename[ MAX_PATH ];
	func_CBaseFileSystem_FixUpPath( _this, pFileName, szLowercaseFilename, sizeof(szLowercaseFilename) );
	pFileName = szLowercaseFilename;

	// RaphaelIT7: Just to avoid weird issues
	NormalizeGamePath( szLowercaseFilename );

	// Fill in the default in case it's not found...
	V_strncpy( pDest, pFileName, maxLenInChars );

// @FD This is arbitrary and seems broken.  If the caller needs this filter, they should
//     request it with the flag themselves.  As it is, I cannot search all the file paths
//     for a file using this function because there is no option that says, "No, really, I
//     mean ALL SEARCH PATHS."  The current problem I'm trying to fix is that sounds are not
//     working if they are in the BSP.  I wrote code that assumed that I could just ask for
//     the absolute path of a file, since we are able to open files with these absolute
//     filenames, and that each particular filesystem call wouldn't have its own individual
//     quirks.
//	if ( IsPC() && pathFilter == FILTER_NONE )
//	{
//		// X360TBD: PC legacy behavior never returned pack paths
//		// do legacy behavior to ensure naive callers don't break
//		pathFilter = FILTER_CULLPACK;
//	}
	

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
		if ( pSearchPath->m_bIsWorkshop )
		{
			Addon::FileSystem* m_AddonFileSystem = (Addon::FileSystem*)_this->Addons();
			std::string strFullFileName = func_Addon_FileSystem_ResolveFile( m_AddonFileSystem, pTmpFileName );
			if ( !strFullFileName.empty() )
			{
				V_strncpy( pDest, strFullFileName.c_str(), maxLenInChars );
				return pDest;
			}
		}

		// RaphaelIT7: We force lower for consistency!
		V_strlower( pTmpFileName );
		FileCacheEntry eCacheEntry = g_pDiskFileTree.ContainsPath( pTmpFileName );

		// RaphaelIT7: We check == INVALID since FS_stat works on both file and folder so we must allow both!
		if ( eCacheEntry == FileCacheEntry::INVALID )
		{
			// RaphaelIT7: Debugging
			//if ( FS_stat( pTmpFileName, &buf ) != -1 )
			//	__debugbreak();

			continue;
		}

		if ( ((CBaseFileSystem*)g_pFullFileSystem)->FS_stat( pTmpFileName, &buf ) != -1 )
		{
			V_strncpy( pDest, pTmpFileName, maxLenInChars );
			if ( pPathType && pSearchPath->m_bIsRemotePath )
			{
				*pPathType |= PATH_IS_REMOTE;
			}
			return pDest;
		}
	}

	// not found
	return nullptr;
}

void CFileSystemModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	bShutdown = false;

	int pBaseLength = 0;
	char pBaseDir[MAX_PATH];
	if ( pBaseLength < 3 )
		pBaseLength = g_pFullFileSystem->GetSearchPath( "BASE_PATH", true, pBaseDir, sizeof( pBaseDir ) );

	std::string workshopDir = pBaseDir;
	workshopDir.append("garrysmod" FILEPATH_SLASH "workshop");

	if (g_pFullFileSystem != nullptr)
		InitFileSystem(g_pFullFileSystem);

	Addon::FileSystem* m_AddonFileSystem = (Addon::FileSystem*)g_pFullFileSystem->Addons();
	FOR_EACH_LL_(((CBaseFileSystem*)g_pFullFileSystem)->m_SearchPaths, pSearchPath)
	{
		if ( !m_AddonFileSystem->ModPath().empty() ) // We check for empty as it may have not been set early on!
		{
			if ( PathStartsWith( pSearchPath->GetPathString(), m_AddonFileSystem->ModPath().c_str() ) )
				pSearchPath->m_bIsWorkshop = true;
		}
	}

	if (g_pFileSystemModule.InDebug())
		Msg("Updated workshop path. (%s)\n", workshopDir.c_str());
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
	DETOUR_THISCALL_ADDRETFUNC1( hook_CBaseFileSystem_FindFileInSearchPath, FileHandle_t, FindFileInSearchPath, CBaseFileSystem*, CFileOpenInfo& );
	DETOUR_THISCALL_ADDRETFUNC2( hook_CBaseFileSystem_IsDirectory, bool, IsDirectory, CBaseFileSystem*, const char*, const char* );
	DETOUR_THISCALL_ADDRETFUNC2( hook_CBaseFileSystem_FastFileTime, long, FastFileTime, CBaseFileSystem*, const CSearchPath*, const char* );
	DETOUR_THISCALL_ADDRETFUNC2( hook_CBaseFileSystem_GetFileTime, long, GetFileTime, CBaseFileSystem*, const char*, const char* );
	DETOUR_THISCALL_ADDFUNC2( hook_CBaseFileSystem_HandleOpenRegularFile, HandleOpenRegularFile, CBaseFileSystem*, CFileOpenInfo&, bool);
	DETOUR_THISCALL_ADDFUNC1( hook_CBaseFileSystem_NewSearchPath, NewSearchPath, CBaseFileSystem*, int );
	DETOUR_THISCALL_ADDFUNC4( hook_CBaseFileSystem_AddSearchPathInternal, AddSearchPathInternal, CBaseFileSystem*, const char*, const char*, SearchPathAdd_t, bool );
DETOUR_THISCALL_FINISH();
#endif

void CFileSystemModule::InitDetour(bool bPreServer)
{
	if (!bPreServer)
		return;

	bShutdown = false;

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

	func_CBaseFileSystem_Trace_FOpen = (Symbols::CBaseFileSystem_Trace_FOpen)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::CBaseFileSystem_Trace_FOpenSym);
	Detour::CheckFunction((void*)func_CBaseFileSystem_Trace_FOpen, "CBaseFileSystem::Trace_FOpen");

	func_Addon_FileHandle_Size = (Symbols::Addon_FileHandle_Size)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::Addon_FileHandle_SizeSym);
	Detour::CheckFunction((void*)func_Addon_FileHandle_Size, "Addon::FileHandle::Size");

	func_Addon_FileSystem_GetFileEntry = (Symbols::Addon_FileSystem_GetFileEntry)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::Addon_FileSystem_GetFileEntrySym);
	Detour::CheckFunction((void*)func_Addon_FileSystem_GetFileEntry, "Addon::FileSystem::GetFileEntry");

	func_Addon_FileSystem_ResolveFile = (Symbols::Addon_FileSystem_ResolveFile)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::Addon_FileSystem_ResolveFileSym);
	Detour::CheckFunction((void*)func_Addon_FileSystem_ResolveFile, "Addon::FileSystem::ResolveFile");

	func_CFileSystem_Stdio_FS_FindFirstFile = (Symbols::CFileSystem_Stdio_FS_FindFirstFile)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::CFileSystem_Stdio_FS_FindFirstFileSym);
	Detour::CheckFunction((void*)func_CFileSystem_Stdio_FS_FindFirstFile, "CFileSystem_Stdio::FS_FindFirstFile");

	func_CFileSystem_Stdio_FS_FindNextFile = (Symbols::CFileSystem_Stdio_FS_FindNextFile)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::CFileSystem_Stdio_FS_FindNextFileSym);
	Detour::CheckFunction((void*)func_CFileSystem_Stdio_FS_FindNextFile, "CFileSystem_Stdio::FS_FindNextFile");

	func_CFileSystem_Stdio_FS_FindClose = (Symbols::CFileSystem_Stdio_FS_FindClose)Detour::GetFunction(filesystem_loader.GetModule(), Symbols::CFileSystem_Stdio_FS_FindCloseSym);
	Detour::CheckFunction((void*)func_CFileSystem_Stdio_FS_FindClose, "CFileSystem_Stdio::FS_FindClose");

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
	bShutdown = true;
}