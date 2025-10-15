#pragma once

#include "Bootil/Bootil.h"

namespace Bootil
{
	namespace File
	{
		/*

		- Declare

		ChangeMonitor mychangemonitor;


		- Set up

		mychangemonitor.WatchFolder( "blah/blah/", true );


		- In a loop somewhere safe

		if ( mychangemonitor.HasChanges() )
		{
			BString strFileChanged = GetChange();

			- do something here about strFileChanged being changed
		}

		*/

		class BOOTIL_EXPORT ChangeMonitor
		{
			public:

				ChangeMonitor();
				~ChangeMonitor();

				bool WatchFolder(const BString& strFolder, bool bWatchSubtree = false/*, const std::vector<BString>& strSubFolders = {}*/);
				void Stop();

				bool HasChanges();
				BString GetChange();

				const BString & FolderName() { return m_strFolderName; }

				bool AddFolderToWatch(const BString& strFolder, bool bWatchSubtree = false);
				bool RemoveFolderFromWatch(const BString& strFolder, bool bWatchSubtree = false);

			public: // private? Naah

				void NoteFileChanged( const BString & strName );
				void CheckForChanges();
				void StartWatch();

#ifdef __linux__
				int                 m_inotify;
#endif
				void* 				m_dirHandles;
				BString				m_strFolderName;
				std::list<BString>	m_Changes;
				bool				m_bWatchSubtree;
		};
	}
}