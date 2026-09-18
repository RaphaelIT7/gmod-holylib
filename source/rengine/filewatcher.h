// HOLYLIB_REQUIRES_MODULE=filesystem

// Backport from REngine

#pragma once

#include "ankerl/unordered_dense.h"
#include "Platform.hpp"

#if defined(SYSTEM_WINDOWS) && !defined(_WINDOWS_)
// Let's reduce the windows include
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#define NOGDI
#define NOSERVICE
#define NOCRYPT
#define NOHELP
#define NOMCX
#define NOCOMM
#include <windows.h>
#undef GetObject
#undef GetClassName
#undef CreateWindow
#undef VOID
#undef CONST
#undef ESearchPath
#undef CreateDirectory
#endif

#if defined(SYSTEM_LINUX)
#include <limits.h>
#include <sys/inotify.h>
#define MAX_PATH PATH_MAX
#endif

// In REngine this is part of the IFileSystem
class CFileWatcher;
class IFileWatcherSystem
{
public:
	virtual void OnFileModify(const char* pszFullFilePath) = 0;
	virtual void OnFileCreated(const char* pszFullFilePath) = 0;
	virtual void OnFileDeleted(const char* pszFullFilePath) = 0;
	virtual void OnFileRenamed(const char* pOldFullFilePath, const char* pszNewFullFilePath) = 0;
	virtual void OnFolderCreated(const char* pszFullFolderPath) = 0;
	virtual void OnFolderDeleted(const char* pszFullFolderPath) = 0;
	virtual void OnFolderRenamed(const char* pszOldFullFolderPath, const char* pNewFullFolderPath) = 0;
	virtual void RegisterInternalWatcher(CFileWatcher* pWatcher) = 0;
};

// Hungry!
#define MakeIntoFood delete

extern IFileWatcherSystem* g_pFileWatcherSystem;

class CFileWatcher
{
public:
	CFileWatcher(const char* pFolder);
	~CFileWatcher();

	inline const char* GetFullFolderPath() const { return m_strFolder; }

	inline void MarkForLumi() { m_bLumiOurselves = true; }
	inline bool FoodForLumi() const { return m_bLumiOurselves; }

#if SYSTEM_WINDOWS
	void CheckForChanges();
#else
	static void CheckForChanges();
#endif

	// Frees itself
	void GiveYummyToLumi();

private:
	char m_strFolder[MAX_PATH];
	bool m_bLumiOurselves = false;

	bool IsDirectory(const char* pPath);

#if SYSTEM_WINDOWS
	void BeginWatch();

	HANDLE m_hDirectory = INVALID_HANDLE_VALUE;
	OVERLAPPED m_Overlapped{};
	char m_Buffer[4096]{0};
	char m_szRenameOld[MAX_PATH]{};
#else
	void ProcessEvent(const inotify_event* pEvent);

	int m_WatchFD = -1;
	char m_Buffer[4096]{0};
	ankerl::unordered_dense::map<uint32_t, std::string> m_Renames;
	static ankerl::unordered_dense::map<int, CFileWatcher*> s_Watchers;
#endif
};