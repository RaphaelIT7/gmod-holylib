// HOLYLIB_REQUIRES_MODULE=filesystem
#include "sourcesdk/filesystem_things.h"

// Never called since when setting up we override the VTable
CFileHandle::~CFileHandle()
{
}

extern CUtlSymbolTableMT* g_pPathIDTable;
CSearchPath::CSearchPath( void )
{
	m_Path = g_pPathIDTable->AddString( "" );
	m_pDebugPath = "";

	m_storeId = 0;
	m_pPackFile = nullptr;
	m_pPathIDInfo = nullptr;
	m_bIsRemotePath = false;
	m_pPackedStore = nullptr;
	m_bIsTrustedForPureServer = false;
	m_bIsWorkshop = false;
	m_bTrackDisk = false;
}

CSearchPath::~CSearchPath()
{
	if ( m_pPackFile )
	{	
		m_pPackFile->Release();
	}
	if ( m_pPackedStore )
	{
		m_pPackedStore->Release();
	}
}

CSearchPathsIterator::CSearchPathsIterator( CBaseFileSystem *pFileSystem, const char **ppszFilename, const char *pszPathID, PathTypeFilter_t pathTypeFilter )
	: m_iCurrent( CUtlLinkedList<CSearchPath>::InvalidIndex() ),
	m_PathTypeFilter( pathTypeFilter )
{
	m_Filename[0] = '\0';

	char tempPathID[MAX_PATH];
	if ( *ppszFilename && (*ppszFilename)[0] == '/' && (*ppszFilename)[1] == '/' ) // ONLY '//' (and not '\\') for our special format
	{
		Error(PROJECT_NAME " - filesystem: What??? NYI (%s)\n", ppszFilename);
		// Allow for UNC-type syntax to specify the path ID.
		/// pFileSystem->ParsePathID( *ppszFilename, pszPathID, tempPathID );
	}
	if ( pszPathID )
	{
		m_pathID = g_pPathIDTable->AddString( pszPathID );
	}
	else
	{
		m_pathID = UTL_INVAL_SYMBOL;
	}

	if ( *ppszFilename && !Q_IsAbsolutePath( *ppszFilename ) )
	{
		pFileSystem->FixUpPath ( *ppszFilename, m_Filename, sizeof(m_Filename) );
	}
	else
	{
		// If it's an absolute path, it isn't worth using the paths at all. Simplify
		// client logic by pretending there's a search path of 1
		m_EmptyPathIDInfo.m_bByRequestOnly = false;
		m_EmptySearchPath.m_pPathIDInfo = &m_EmptyPathIDInfo;
		m_EmptySearchPath.SetPath( m_pathID );
		m_EmptySearchPath.m_storeId = -1;
	}
}

CSearchPathsIterator::CSearchPathsIterator( CBaseFileSystem *pFileSystem, const char *pszPathID, PathTypeFilter_t pathTypeFilter )
	: m_iCurrent( CUtlLinkedList<CSearchPath>::InvalidIndex() ),
	m_PathTypeFilter( pathTypeFilter )
{
	if ( pszPathID ) 
	{
		m_pathID = g_pPathIDTable->AddString( pszPathID );
	}
	else
	{
		m_pathID = UTL_INVAL_SYMBOL;
	}

	m_Filename[0] = '\0';
}

void CSearchPath::SetPath( CUtlSymbol id )
{
	m_Path = id;
	m_pDebugPath = g_pPathIDTable->String( m_Path );
}