#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"
#include "filesystem.h"
#include <sourcesdk/filesystem_things.h>
#include <unordered_map>
#include <vprof.h>

class CFileSystemModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn);
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour(bool bPreServer);
	virtual void Think(bool bSimulating);
	virtual void Shutdown();
	virtual const char* Name() { return "filesystem"; };
};

CFileSystemModule g_pFileSystemModule;
IModule* pFileSystemModule = &g_pFileSystemModule;

ConVar holylib_filesystem_easydircheck("holylib_filesystem_easydircheck", "0", 0, 
	"Checks if the folder CBaseFileSystem::IsDirectory checks has a . in the name after the last /. if so assume it's a file extension.");
ConVar holylib_filesystem_searchcache("holylib_filesystem_searchcache", "1", 0, 
	"If enabled, it will cache the search path a file was located in and if the same file is requested, it will use that search path directly.");
ConVar holylib_filesystem_optimizedfixpath("holylib_filesystem_optimizedfixpath", "1", 0, 
	"If enabled, it will optimize CBaseFilesystem::FixUpPath by caching the BASE_PATH search cache.");

ConVar holylib_filesystem_debug("holylib_filesystem_debug", "0", 0, 
	"If enabled, it will show any change to the search cache.");

Symbols::CBaseFileSystem_FindSearchPathByStoreId func_CBaseFileSystem_FindSearchPathByStoreId;
std::unordered_map<std::string, int> m_SearchCache;
void AddFileToSearchCache(const char* pFileName, int path)
{
	if (holylib_filesystem_debug.GetBool())
		Msg("Added file %s to seach cache (%i)\n", pFileName, path);

	m_SearchCache[pFileName] = path;
}


void RemoveFileFromSearchCache(const char* pFileName)
{
	if (holylib_filesystem_debug.GetBool())
		Msg("Removed file %s from seach cache!\n", pFileName);

	m_SearchCache.erase(pFileName);
}

CSearchPath* GetPathFromSearchCache(const char* pFileName)
{
	auto it = m_SearchCache.find(pFileName);
	if (it == m_SearchCache.end())
		return NULL;

	if (holylib_filesystem_debug.GetBool())
		Msg("Getting search path for file %s from cache!\n", pFileName);

	return func_CBaseFileSystem_FindSearchPathByStoreId(g_pFullFileSystem, it->second);
}

void NukeSearchCache()
{
	if (holylib_filesystem_debug.GetBool())
		Msg("Search cache got nuked\n");

	m_SearchCache.clear();
}

Detouring::Hook detour_CBaseFileSystem_FindFileInSearchPath;
FileHandle_t* hook_CBaseFileSystem_FindFileInSearchPath(void* filesystem, CFileOpenInfo &openInfo)
{
	if (!holylib_filesystem_searchcache.GetBool())
		return detour_CBaseFileSystem_FindFileInSearchPath.GetTrampoline<Symbols::CBaseFileSystem_FindFileInSearchPath>()(filesystem, openInfo);

	CSearchPath* cachePath = GetPathFromSearchCache(openInfo.m_pFileName);
	if (cachePath)
	{
		VPROF_BUDGET("HolyLib::FileSystem::Cache-CBaseFileSystem::FindFile", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

		const CSearchPath* origPath = openInfo.m_pSearchPath;
		openInfo.m_pSearchPath = cachePath;
		FileHandle_t* file = detour_CBaseFileSystem_FindFileInSearchPath.GetTrampoline<Symbols::CBaseFileSystem_FindFileInSearchPath>()(filesystem, openInfo);
		if (file)
			return file;

		openInfo.m_pSearchPath = origPath;
		RemoveFileFromSearchCache(openInfo.m_pFileName);
	} else {
		if (holylib_filesystem_debug.GetBool())
			Msg("FindFileInSearchPath: Failed to find cachePath! (%s)\n", openInfo.m_pFileName);
	}

	FileHandle_t* file = detour_CBaseFileSystem_FindFileInSearchPath.GetTrampoline<Symbols::CBaseFileSystem_FindFileInSearchPath>()(filesystem, openInfo);

	if (file)
		AddFileToSearchCache(openInfo.m_pFileName, openInfo.m_pSearchPath->m_storeId);

	return file;
}

Detouring::Hook detour_CBaseFileSystem_FastFileTime;
long hook_CBaseFileSystem_FastFileTime(void* filesystem, const CSearchPath* path, const char* pFileName)
{
	if (!holylib_filesystem_searchcache.GetBool())
		return detour_CBaseFileSystem_FastFileTime.GetTrampoline<Symbols::CBaseFileSystem_FastFileTime>()(filesystem, path, pFileName);

	CSearchPath* cachePath = GetPathFromSearchCache(pFileName);
	if (cachePath)
	{
		VPROF_BUDGET("HolyLib::FileSystem::Cache-CBaseFileSystem::FastFileTime", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

		long time = detour_CBaseFileSystem_FastFileTime.GetTrampoline<Symbols::CBaseFileSystem_FastFileTime>()(filesystem, cachePath, pFileName);
		if (time != 0L)
			return time;

		RemoveFileFromSearchCache(pFileName);
	} else {
		if (holylib_filesystem_debug.GetBool())
			Msg("FastFileTime: Failed to find cachePath! (%s)\n", pFileName);
	}

	long time = detour_CBaseFileSystem_FastFileTime.GetTrampoline<Symbols::CBaseFileSystem_FastFileTime>()(filesystem, path, pFileName);

	if (time != 0L)
		AddFileToSearchCache(pFileName, path->m_storeId);

	return time;
}

bool is_file(const char *path) {
    const char *last_slash = strrchr(path, '/');
    const char *last_dot = strrchr(path, '.');

    return last_dot != NULL && (last_slash == NULL || last_dot > last_slash);
}

Detouring::Hook detour_CBaseFileSystem_IsDirectory;
bool hook_CBaseFileSystem_IsDirectory(void* filesystem, const char* pFileName, const char* pPathID)
{
	if (holylib_filesystem_easydircheck.GetBool() && is_file(pFileName))
		return false;

	return detour_CBaseFileSystem_IsDirectory.GetTrampoline<Symbols::CBaseFileSystem_IsDirectory>()(filesystem, pFileName, pPathID);
}

int pBaseLength = 0;
char pBaseDir[MAX_PATH];
Detouring::Hook detour_CBaseFileSystem_FixUpPath;
bool hook_CBaseFileSystem_FixUpPath(IFileSystem* filesystem, const char *pFileName, char *pFixedUpFileName, int sizeFixedUpFileName)
{
	if (!holylib_filesystem_optimizedfixpath.GetBool())
		return detour_CBaseFileSystem_FixUpPath.GetTrampoline<Symbols::CBaseFileSystem_FixUpPath>()(filesystem, pFileName, pFixedUpFileName, sizeFixedUpFileName);

	VPROF_BUDGET("HolyLib - CBaseFileSystem::FixUpPath", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

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

		if (!pBaseLength || pBaseLength < 3)
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

void CFileSystemModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
}

void CFileSystemModule::LuaInit(bool bServerInit)
{
	if (!bServerInit)
	{

	}
}

void CFileSystemModule::LuaShutdown()
{
}

void CFileSystemModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }

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

	func_CBaseFileSystem_FindSearchPathByStoreId = (Symbols::CBaseFileSystem_FindSearchPathByStoreId)Detour::GetFunction(dedicated_loader.GetModule(), Symbols::CBaseFileSystem_FindSearchPathByStoreIdSym);
	Detour::CheckFunction(func_CBaseFileSystem_FindSearchPathByStoreId, "CBaseFileSystem::FindSearchPathByStoreId");
}

void CFileSystemModule::Think(bool simulating)
{
}

void CFileSystemModule::Shutdown()
{
	Detour::Remove(m_pID);
}