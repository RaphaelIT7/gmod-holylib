// HOLYLIB_REQUIRES_MODULE=filesystem

// Backport from REngine adjusted for Source

#include "filewatcher.h"
#include "tier0/dbg.h"

#if SYSTEM_WINDOWS
CFileWatcher::CFileWatcher(const char* pFolder)
{
	strncpy(m_strFolder, pFolder, sizeof(m_strFolder));

	ZeroMemory(&m_Overlapped, sizeof(m_Overlapped));
	m_hDirectory = CreateFileA(
		pFolder,
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		nullptr
	);

	if (m_hDirectory == INVALID_HANDLE_VALUE)
	{
		Warning("Failed to watch \"%s\"\n", pFolder);
		GiveYummyToLumi();
		return;
	}

	m_Overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	g_pFileWatcherSystem->RegisterInternalWatcher(this);

	BeginWatch();
}

CFileWatcher::~CFileWatcher()
{
	if (m_hDirectory != INVALID_HANDLE_VALUE)
		CloseHandle(m_hDirectory);

	if (m_Overlapped.hEvent)
		CloseHandle(m_Overlapped.hEvent);
}

void CFileWatcher::BeginWatch()
{
	ResetEvent(m_Overlapped.hEvent);
	ReadDirectoryChangesW(
		m_hDirectory,
		m_Buffer,
		sizeof(m_Buffer),
		// RapahelIT7:
		// No recursion for now. Also if anyone ever tried to use wine- this may fail as wine doesn't seem to properly implement this argument!
		FALSE,
		FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
		nullptr,
		&m_Overlapped,
		nullptr
	);
}

bool CFileWatcher::IsDirectory(const char* pPath)
{
	DWORD attr = GetFileAttributesA(pPath);
	return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

void CFileWatcher::CheckForChanges()
{
	if (m_hDirectory == INVALID_HANDLE_VALUE || FoodForLumi())
		return;

	DWORD bytesReturned = 0;
	if (!GetOverlappedResult(m_hDirectory, &m_Overlapped, &bytesReturned, FALSE))
	{
		DWORD err = GetLastError();
		if (err == ERROR_IO_INCOMPLETE)
			return;

		if (err == ERROR_NOTIFY_ENUM_DIR)
		{
			Warning("[CFileWatcher::CheckForChanges] Failed to watch folder \"%s\" and were now desync with disk!\n", m_strFolder);
			// ToDo: We should just re-trigger CSearchPath::RecursiveDiskTraversal probably
			return;
		}

		if (err == ERROR_ACCESS_DENIED || err == ERROR_INVALID_HANDLE)
		{
			MarkForLumi();
			return;
		}

		BeginWatch();
		return;
	}

	char szOldFileName[MAX_PATH]{0};
	char* pCurrent = reinterpret_cast<char*>(m_Buffer);
	while (true)
	{
		FILE_NOTIFY_INFORMATION* pInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(pCurrent);
		char szFileName[MAX_PATH];
		int length = WideCharToMultiByte(
			CP_UTF8,
			0,
			pInfo->FileName,
			pInfo->FileNameLength / sizeof(WCHAR),
			szFileName,
			MAX_PATH - 1,
			nullptr,
			nullptr
		);

		szFileName[length] = '\0';

		// Very simple de-duplication
		// it is not perfect but it kinda works for most of the files
		if (strncmp(szOldFileName, szFileName, MAX_PATH) == 0)
		{
			if (pInfo->NextEntryOffset == 0)
				break;

			pCurrent += pInfo->NextEntryOffset;
			continue;
		}

		char pszFullPath[MAX_PATH];
		snprintf(pszFullPath, sizeof(pszFullPath), "%s%s", m_strFolder, szFileName);
		strncpy(szOldFileName, szFileName, MAX_PATH);

		switch (pInfo->Action)
		{
		case FILE_ACTION_ADDED:
			if (IsDirectory(pszFullPath))
				g_pFileWatcherSystem->OnFolderCreated(pszFullPath);
			else
				g_pFileWatcherSystem->OnFileCreated(pszFullPath);
			break;
		case FILE_ACTION_REMOVED:
			g_pFileWatcherSystem->OnFileDeleted(pszFullPath);
			break;
		case FILE_ACTION_MODIFIED:
			g_pFileWatcherSystem->OnFileModify(pszFullPath);
			break;
		case FILE_ACTION_RENAMED_OLD_NAME:
			strncpy(m_szRenameOld, pszFullPath, sizeof(m_szRenameOld));
			break;
		case FILE_ACTION_RENAMED_NEW_NAME:
			if (IsDirectory(pszFullPath))
				g_pFileWatcherSystem->OnFolderRenamed(m_szRenameOld, pszFullPath);
			else
				g_pFileWatcherSystem->OnFileRenamed(m_szRenameOld, pszFullPath);
			break;
		}

		if (pInfo->NextEntryOffset == 0)
			break;

		pCurrent += pInfo->NextEntryOffset;
	}

	BeginWatch();
}
#else
class CInotifySystem
{
public:
	CInotifySystem()
	{
		m_InotifyFD = inotify_init1(IN_NONBLOCK);
		if (m_InotifyFD == -1)
			Warning("inotify_init1 failed\n");
	}

	~CInotifySystem()
	{
		if (m_InotifyFD != -1)
			close(m_InotifyFD);
	}

	int GetFD() const
	{
		return m_InotifyFD;
	}

	bool IsValid() const
	{
		return m_InotifyFD != -1;
	}

private:
	int m_InotifyFD = -1;
};

static CInotifySystem g_Notify;

ankerl::unordered_dense::map<int, CFileWatcher*> CFileWatcher::s_Watchers;
CFileWatcher::CFileWatcher(const char* pFolder)
{
	strncpy(m_strFolder, pFolder, sizeof(m_strFolder));

	m_WatchFD = inotify_add_watch(
		g_Notify.GetFD(),
		pFolder,
		IN_MODIFY | IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB | IN_DELETE_SELF | IN_IGNORED
	);

	if (m_WatchFD == -1)
	{
		Warning("Failed to watch \"%s\"\n", pFolder);
		GiveYummyToLumi();
		return;
	}

	s_Watchers[m_WatchFD] = this;
	g_pFileWatcherSystem->RegisterInternalWatcher(this);
}


CFileWatcher::~CFileWatcher()
{
	if (m_WatchFD != -1)
	{
		s_Watchers.erase(m_WatchFD);
		inotify_rm_watch(g_Notify.GetFD(), m_WatchFD);
	}
}

void CFileWatcher::CheckForChanges()
{
	const int inotifyFD = g_Notify.GetFD();
	if (inotifyFD == -1)
		return;

	char buffer[64 * 1024];
	for (;;)
	{
		const int length = read(inotifyFD, buffer, sizeof(buffer));
		if (length <= 0)
			return;

		int offset = 0;
		while (offset < length)
		{
			auto* pEvent = reinterpret_cast<inotify_event*>(buffer + offset);
			auto it = s_Watchers.find(pEvent->wd);
			if (it != s_Watchers.end())
				it->second->ProcessEvent(pEvent);

			offset += sizeof(inotify_event) + pEvent->len;
		}
	}
}

void CFileWatcher::ProcessEvent(const inotify_event* pEvent)
{
	char pszFullPath[PATH_MAX];
	if (pEvent->len > 0 && pEvent->name[0] != '\0')
		snprintf(pszFullPath, sizeof(pszFullPath), "%s/%s", m_strFolder, pEvent->name);
	else
		strncpy(pszFullPath, m_strFolder, sizeof(pszFullPath));

	if (pEvent->mask & IN_CREATE)
	{
		if (pEvent->mask & IN_ISDIR)
			g_pFileWatcherSystem->OnFolderCreated(pszFullPath);
		else
			g_pFileWatcherSystem->OnFileCreated(pszFullPath);
	}

	if (pEvent->mask & IN_DELETE)
	{
		if (pEvent->mask & IN_ISDIR)
			g_pFileWatcherSystem->OnFolderDeleted(pszFullPath);
		else
			g_pFileWatcherSystem->OnFileDeleted(pszFullPath);
	}

	if (pEvent->mask & IN_MODIFY)
	{
		if (!(pEvent->mask & IN_ISDIR))
			g_pFileWatcherSystem->OnFileModify(pszFullPath);
	}

	if (pEvent->mask & IN_MOVED_FROM)
		m_Renames[pEvent->cookie] = pszFullPath;

	if (pEvent->mask & IN_MOVED_TO)
	{
		auto it = m_Renames.find(pEvent->cookie);
		if (it != m_Renames.end())
		{
			if (pEvent->mask & IN_ISDIR)
				g_pFileWatcherSystem->OnFolderRenamed(it->second.c_str(), pszFullPath);
			else
				g_pFileWatcherSystem->OnFileRenamed(it->second.c_str(), pszFullPath);

			m_Renames.erase(it);
		}
		else
		{
			// Moved here from outside this watched tree.
			if (pEvent->mask & IN_ISDIR)
				g_pFileWatcherSystem->OnFolderCreated(pszFullPath);
			else
				g_pFileWatcherSystem->OnFileCreated(pszFullPath);
		}
	}

	if (pEvent->mask & IN_DELETE_SELF)
		g_pFileWatcherSystem->OnFolderDeleted(m_strFolder);

	if (pEvent->mask & IN_IGNORED)
		MarkForLumi();
}
#endif

void CFileWatcher::GiveYummyToLumi()
{
	MakeIntoFood this;
}