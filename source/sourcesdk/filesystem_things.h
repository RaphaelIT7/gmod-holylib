#include <GarrysMod/FactoryLoader.hpp>
#include <utlsymbol.h>
#include <filesystem.h>
#include <utlhashtable.h>
#include <utldict.h>

#if defined( _WIN32 )

#if !defined( _X360 )
	#include <io.h>
	#include <direct.h>
	#include "winlite.h"
#endif
#undef GetCurrentDirectory
#undef GetJob
#undef AddJob

#include "tier0/threadtools.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <malloc.h>
#include <string.h>
#include "tier1/utldict.h"

#elif defined(POSIX)
	#include <unistd.h> // unlink
	#include "sourcesdk/linux_support.h"
	#define INVALID_HANDLE_VALUE (void *)-1

	// undo the prepended "_" 's
	#define _chmod chmod
	#define _stat stat
	#define _alloca alloca
	#define _S_IFDIR S_IFDIR
#endif

class CPackedStoreRefCount;
extern CUtlSymbolTableMT* g_PathIDTable;
class CPathIDInfo
	{
	public:
		CPathIDInfo() : m_bByRequestOnly(), m_pDebugPathID() {}
		const CUtlSymbol& GetPathID() const;
		const char* GetPathIDString() const;
		void SetPathID( CUtlSymbol id );

	public:
		// See MarkPathIDByRequestOnly.
		bool m_bByRequestOnly;

	private:
		CUtlSymbol m_PathID;
		const char *m_pDebugPathID;
	};

class CSearchPath
	{
	public:
							CSearchPath( void );
							~CSearchPath( void );

		const char* GetPathString() const;
		const char* GetDebugString() const;
		
		// Path ID ("game", "mod", "gamebin") accessors.
		const CUtlSymbol& GetPathID() const;
		const char* GetPathIDString() const;

		// Search path (c:\hl2\hl2) accessors.
		void SetPath( CUtlSymbol id );
		const CUtlSymbol& GetPath() const;

		void SetPackFile(void* *pPackFile) { m_pPackFile = pPackFile; }
		void* *GetPackFile() const { return m_pPackFile; }

		#ifdef SUPPORT_PACKED_STORE
		void SetPackedStore( CPackedStoreRefCount *pPackedStore ) { /*m_pPackedStore = pPackedStore;*/ }
		#endif
		void* *GetPackedStore() const { return m_pPackedStore; }

		bool IsMapPath() const;

		int					m_storeId;

		// Used to track if its search 
		CPathIDInfo			*m_pPathIDInfo;

		bool				m_bIsRemotePath;

		bool				m_bIsTrustedForPureServer;

	private:
		CUtlSymbol			m_Path;
		const char			*m_pDebugPath;
		void*			*m_pPackFile;
		void* *m_pPackedStore;
	};

//---------------------------------------------------------

enum FileMode_t
{
	FM_BINARY,
	FM_TEXT
};

abstract_class CStdFilesystemFile
{
public:
	virtual ~CStdFilesystemFile() {}
	virtual void FS_setbufsize(unsigned nBytes) = 0;
	virtual void FS_fclose() = 0;
	virtual void FS_fseek(int64 pos, int seekType) = 0;
	virtual long FS_ftell() = 0;
	virtual int FS_feof() = 0;
	virtual size_t FS_fread(void* dest, size_t destSize, size_t size) = 0;
	virtual size_t FS_fwrite(const void* src, size_t size) = 0;
	virtual bool FS_setmode(FileMode_t mode) = 0;
	virtual size_t FS_vfprintf(const char* fmt, va_list list) = 0;
	virtual int FS_ferror() = 0;
	virtual int FS_fflush() = 0;
	virtual char* FS_fgets(char* dest, int destSize) = 0;
	virtual int FS_GetSectorSize() { return 1; }
};

//---------------------------------------------------------

class CStdioFile : public CStdFilesystemFile
{
public:
	static CStdioFile* FS_fopen(const char* filename, const char* options, int64* size);

	virtual void FS_setbufsize(unsigned nBytes);
	virtual void FS_fclose();
	virtual void FS_fseek(int64 pos, int seekType);
	virtual long FS_ftell();
	virtual int FS_feof();
	virtual size_t FS_fread(void* dest, size_t destSize, size_t size);
	virtual size_t FS_fwrite(const void* src, size_t size);
	virtual bool FS_setmode(FileMode_t mode);
	virtual size_t FS_vfprintf(const char* fmt, va_list list);
	virtual int FS_ferror();
	virtual int FS_fflush();
	virtual char* FS_fgets(char* dest, int destSize);

#if defined( POSIX ) && !defined( _PS3 )
	static CUtlMap< int, CInterlockedInt > m_LockedFDMap;
#endif
public:
	CStdioFile(FILE* pFile, bool bWriteable)
		: m_pFile(pFile), m_bWriteable(bWriteable)
	{
	}

	FILE* m_pFile;
	bool m_bWriteable;
};


#define MAX_FILEPATH 512
class CBaseFileSystem;
class CSearchPathsVisits
	{
	public:
		void Reset()
		{
			m_Visits.RemoveAll();
		}

		bool MarkVisit( const CSearchPath &searchPath )
		{
			if ( m_Visits.Find( searchPath.m_storeId ) == m_Visits.InvalidIndex() )
			{
				MEM_ALLOC_CREDIT();
				m_Visits.AddToTail( searchPath.m_storeId );
				return false;
			}
			return true;
		}

	private:
		CUtlVector<int> m_Visits;	// This is a copy of IDs for the search paths we've visited, so 
	};

class CSearchPathsIterator
	{
	public:
		CSearchPath *GetFirst();
		CSearchPath *GetNext();

	public:
		CSearchPathsIterator( const  CSearchPathsIterator & );
		void operator=(const CSearchPathsIterator &);
		void CopySearchPaths( const CUtlVector<CSearchPath>	&searchPaths )
		{
			m_SearchPaths = searchPaths;
		}

		int							m_iCurrent;
		CUtlSymbol					m_pathID;
		CUtlVector<CSearchPath> 	m_SearchPaths;
		CSearchPathsVisits			m_visits;
		CSearchPath					m_EmptySearchPath;
		CPathIDInfo					m_EmptyPathIDInfo;
		PathTypeFilter_t			m_PathTypeFilter;
		char						m_Filename[MAX_PATH];	// set for relative names only
		bool						m_bExcluded;
	};

enum FileType_t
{
	FT_NORMAL,
	FT_PACK_BINARY,
	FT_PACK_TEXT,
	FT_MEMORY_BINARY,
	FT_MEMORY_TEXT
};

class CPackedStore;
class CPackedStoreFileHandle
{
public:
	int m_nFileNumber;
	int m_nFileOffset;
	int m_nFileSize;
	int m_nCurrentFileOffset;
	void const *m_pMetaData;
	uint16 m_nMetaDataSize;
	CPackedStore *m_pOwner;
	struct CFileHeaderFixedData *m_pHeaderData;
	uint8 *m_pDirFileNamePtr;								// pointer to basename in dir block

	FORCEINLINE operator bool( void ) const
	{
		return ( m_nFileNumber != -1 );
	}

	FORCEINLINE int Read( void *pOutData, int nNumBytes );

	CPackedStoreFileHandle( void )
	{
		m_nFileNumber = -1;
		m_nFileOffset = -1;
		m_nFileSize = -1;
		m_nCurrentFileOffset = -1;
		m_pMetaData = nullptr;
		m_nMetaDataSize = 0;
		m_pOwner = nullptr;
		m_pHeaderData = nullptr;
		m_pDirFileNamePtr = nullptr;
	}

	int Seek( int nOffset, int nWhence )
	{
		switch( nWhence )
		{
			case SEEK_CUR:
				nOffset = m_nFileOffset + nOffset ;
				break;

			case SEEK_END:
				nOffset = m_nFileSize + nOffset;
				break;
		}
		m_nCurrentFileOffset = MAX( 0, MIN( m_nFileSize, nOffset ) );
		return m_nCurrentFileOffset;
	}

	int Tell( void ) const
	{
		return m_nCurrentFileOffset;
	}

	uint32 GetFileCRCFromHeaderData() const
	{
		uint32 *pCRC = (uint32 *)m_pHeaderData;
		return *pCRC;
	}

	FORCEINLINE void GetPackFileName( char *pchFileNameOut, int cchFileNameOut );

};

class CFileTracker2 : IThreadedFileMD5Processor
{
public:
	CFileTracker2( CBaseFileSystem *pFileSystem ) {}
	~CFileTracker2() {}

	void ShutdownAsync() {}

	void MarkAllCRCsUnverified() {}
	int	GetUnverifiedFileHashes( CUnverifiedFileHash *pFiles, int nMaxFiles ) { return 0; }
	EFileCRCStatus CheckCachedFileHash( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash )
		{ return k_eFileCRCStatus_CantOpenFile; }

	virtual int SubmitThreadedMD5Request( uint8 *pubBuffer, int cubBuffer, int PackFileID, int nPackFileNumber, int nPackFileFraction )
		{ return 0; }
	virtual bool BlockUntilMD5RequestComplete( int iRequest, MD5Value_t *pMd5ValueOut ) { Assert(0); return true; }
	virtual bool IsMD5RequestComplete( int iRequest, MD5Value_t *pMd5ValueOut ) { Assert(0); return true; }

	int NotePackFileOpened( const char *pVPKAbsPath, const char *pPathID, int64 nLength ) { return 0; }
	void NotePackFileAccess( const char *pFilename, const char *pPathID, int iSearchPathStoreId, CPackedStoreFileHandle &VPKHandle ) {}
	void AddFileHashForVPKFile( int nPackFileNumber, int nFileFraction, int cbFileLen, MD5Value_t &md5, CPackedStoreFileHandle &fhandle ) {}

	void NoteFileIgnoredForPureServer( const char *pFilename, const char *pPathID, int iSearchPathStoreId ) {}
	void NoteFileLoadedFromDisk( const char *pFilename, const char *pPathID, int iSearchPathStoreId, FILE *fp, int64 nLength ) {}
	void NoteFileUnloaded( const char *pFilename, const char *pPathID ) {}

	IFileList *GetFilesToUnloadForWhitelistChange( IPureServerWhitelist *pNewWhiteList ) { return NULL; }
};

class CPackFile;
class CPackFileHandle;
class CCompiledKeyValuesReader;
class CFileAsyncReadJob;
class CFileHandle
{
public:
	CFileHandle( CBaseFileSystem* fs );
	virtual ~CFileHandle();

	void	Init( CBaseFileSystem* fs );

	int		GetSectorSize();
	bool	IsOK();
	void	Flush();
	void	SetBufferSize( int nBytes );

	int		Read( void* pBuffer, int nLength );
	int		Read( void* pBuffer, int nDestSize, int nLength );

	int		Write( const void* pBuffer, int nLength );
	int		Seek( int64 nOffset, int nWhence );
	int		Tell();
	int		Size();

	int64 AbsoluteBaseOffset();
	bool	EndOfFile();

#if !defined( _RETAIL )
	char *m_pszTrueFileName;
	char const *Name() const { return m_pszTrueFileName ? m_pszTrueFileName : ""; }

	void SetName( char const *pName )
	{
		Assert( pName );
		Assert( !m_pszTrueFileName );
		int len = Q_strlen( pName );
		m_pszTrueFileName = new char[len + 1];
		memcpy( m_pszTrueFileName, pName, len + 1 );
	}
#endif

	CPackFileHandle		*m_pPackFileHandle;
#if defined( SUPPORT_PACKED_STORE )
	CPackedStoreFileHandle m_VPKHandle;
#endif
	int64				m_nLength;
	FileType_t			m_type;
	FILE				*m_pFile;

protected:
	CBaseFileSystem		*m_fs;

	enum
	{
		MAGIC = 0x43464861,		// 'CFHa',
		FREE_MAGIC = 0x4672654d	// 'FreM'
	};
	unsigned int	m_nMagic;

	bool IsValid();
};

class CMemoryFileHandle : public CFileHandle
{
public:
	CMemoryFileHandle( CBaseFileSystem* pFS, CMemoryFileBacking* pBacking )
		: CFileHandle( pFS ), m_pBacking( pBacking ), m_nPosition( 0 ) { m_nLength = pBacking->m_nLength; }

	~CMemoryFileHandle() { m_pBacking->Release(); }

	int		Read( void* pBuffer, int nDestSize, int nLength );
	int		Seek( int64 nOffset, int nWhence );
	int		Tell() { return m_nPosition; }
	int		Size() { return (int) m_nLength; }

	CMemoryFileBacking *m_pBacking;
	int m_nPosition;

private:
	CMemoryFileHandle( const CMemoryFileHandle& ); // not defined
	CMemoryFileHandle& operator=( const CMemoryFileHandle& ); // not defined
};

abstract_class CBaseFileSystem : public CTier1AppSystem< IFileSystem >
{
	friend class CPackFileHandle;
	friend class CZipPackFileHandle;
	friend class CPackFile;
	friend class CZipPackFile;
	friend class CFileHandle;
	friend class CFileTracker;
	friend class CFileTracker2;
	friend class CFileOpenInfo;

	typedef CTier1AppSystem< IFileSystem > BaseClass;

public:
	CBaseFileSystem();
	~CBaseFileSystem();

	// Methods of IAppSystem
	virtual void				*QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t		Init();
	virtual void				Shutdown();

	void						InitAsync();
	void						ShutdownAsync();

	void						ParsePathID( const char* &pFilename, const char* &pPathID, char tempPathID[MAX_PATH] );

	// file handling
	virtual FileHandle_t		Open( const char *pFileName, const char *pOptions, const char *pathID );
	virtual FileHandle_t		OpenEx( const char *pFileName, const char *pOptions, unsigned flags = 0, const char *pathID = 0, char **ppszResolvedFilename = NULL );
	virtual void				Close( FileHandle_t );
	virtual void				Seek( FileHandle_t file, int pos, FileSystemSeek_t method );
	virtual unsigned int		Tell( FileHandle_t file );
	virtual unsigned int		Size( FileHandle_t file );
	virtual unsigned int		Size( const char *pFileName, const char *pPathID );

	virtual void				SetBufferSize( FileHandle_t file, unsigned nBytes );
	virtual bool				IsOk( FileHandle_t file );
	virtual void				Flush( FileHandle_t file );
	virtual bool				Precache( const char *pFileName, const char *pPathID );
	virtual bool				EndOfFile( FileHandle_t file );
 
	virtual int					Read( void *pOutput, int size, FileHandle_t file );
	virtual int					ReadEx( void* pOutput, int sizeDest, int size, FileHandle_t file );
	virtual int					Write( void const* pInput, int size, FileHandle_t file );
	virtual char				*ReadLine( char *pOutput, int maxChars, FileHandle_t file );
	virtual int					FPrintf( FileHandle_t file, PRINTF_FORMAT_STRING const char *pFormat, ... ) FMTFUNCTION( 3, 4 );

	// Reads/writes files to utlbuffers
	virtual bool				ReadFile( const char *pFileName, const char *pPath, CUtlBuffer &buf, int nMaxBytes, int nStartingByte, FSAllocFunc_t pfnAlloc = NULL );
	virtual bool				WriteFile( const char *pFileName, const char *pPath, CUtlBuffer &buf );
	virtual bool				UnzipFile( const char *pFileName, const char *pPath, const char *pDestination );
	virtual int					ReadFileEx( const char *pFileName, const char *pPath, void **ppBuf, bool bNullTerminate, bool bOptimalAlloc, int nMaxBytes = 0, int nStartingByte = 0, FSAllocFunc_t pfnAlloc = NULL );
	virtual bool				ReadToBuffer( FileHandle_t hFile, CUtlBuffer &buf, int nMaxBytes = 0, FSAllocFunc_t pfnAlloc = NULL );

	// Optimal buffer
	bool						GetOptimalIOConstraints( FileHandle_t hFile, unsigned *pOffsetAlign, unsigned *pSizeAlign, unsigned *pBufferAlign );
	void						*AllocOptimalReadBuffer( FileHandle_t hFile, unsigned nSize, unsigned nOffset )	{ return malloc( nSize ); }
	void						FreeOptimalReadBuffer( void *p ) { free( p ); }

	// Gets the current working directory
	virtual bool				GetCurrentDirectory( char* pDirectory, int maxlen );

	// this isn't implementable on STEAM as is.
	virtual void				CreateDirHierarchy( const char *path, const char *pathID );

	// returns true if the file is a directory
	virtual bool				IsDirectory( const char *pFileName, const char *pathID );

	// path info
	virtual const char			*GetLocalPath( const char *pFileName, OUT_Z_CAP(maxLenInChars) char *pDest, int maxLenInChars );
	virtual bool				FullPathToRelativePath( const char *pFullpath, OUT_Z_CAP(maxLenInChars) char *pDest, int maxLenInChars );
#ifndef BUILD_GMOD
	virtual bool				GetCaseCorrectFullPath_Ptr( const char *pFullPath, OUT_Z_CAP(maxLenInChars) char *pDest, int maxLenInChars );
#endif

	// removes a file from disk
	virtual void				RemoveFile( char const* pRelativePath, const char *pathID );

	// Remove all search paths (including write path?)
	virtual void				RemoveAllSearchPaths( void );

	// Purpose: Removes all search paths for a given pathID, such as all "GAME" paths.
	virtual void				RemoveSearchPaths( const char *pathID );

	// STUFF FROM IFileSystem
	// Add paths in priority order (mod dir, game dir, ....)
	// Can also add pak files (errr, NOT YET!)
	virtual void				AddSearchPath( const char *pPath, const char *pathID, SearchPathAdd_t addType );
	virtual bool				RemoveSearchPath( const char *pPath, const char *pathID );
	virtual void				PrintSearchPaths( void );

	virtual void				MarkPathIDByRequestOnly( const char *pPathID, bool bRequestOnly );

	virtual bool				FileExists( const char *pFileName, const char *pPathID = NULL );
	virtual long				GetFileTime( const char *pFileName, const char *pPathID = NULL );
	virtual bool				IsFileWritable( char const *pFileName, const char *pPathID = NULL );
	virtual bool				SetFileWritable( char const *pFileName, bool writable, const char *pPathID = 0 );
	virtual void				FileTimeToString( char *pString, int maxChars, long fileTime );
	
	virtual const char			*FindFirst( const char *pWildCard, FileFindHandle_t *pHandle );
	virtual const char			*FindFirstEx( const char *pWildCard, const char *pPathID, FileFindHandle_t *pHandle );
	virtual const char			*FindNext( FileFindHandle_t handle );
	virtual bool				FindIsDirectory( FileFindHandle_t handle );
	virtual void				FindClose( FileFindHandle_t handle );

	virtual void				PrintOpenedFiles( void );
	virtual void				SetWarningFunc( void (*pfnWarning)( PRINTF_FORMAT_STRING const char *fmt, ... ) );
	virtual void				SetWarningLevel( FileWarningLevel_t level );
	virtual void				AddLoggingFunc( FileSystemLoggingFunc_t logFunc );
	virtual void				RemoveLoggingFunc( FileSystemLoggingFunc_t logFunc );
	virtual bool				RenameFile( char const *pOldPath, char const *pNewPath, const char *pathID );

	virtual void				GetLocalCopy( const char *pFileName );

	virtual  bool				FixUpPath( const char *pFileName, char *pFixedUpFileName, int sizeFixedUpFileName );

	virtual FileNameHandle_t	FindOrAddFileName( char const *pFileName );
	virtual FileNameHandle_t	FindFileName( char const *pFileName );
	virtual bool				String( const FileNameHandle_t& handle, char *buf, int buflen );
	virtual int					GetPathIndex( const FileNameHandle_t &handle );
	long						GetPathTime( const char *pFileName, const char *pPathID );
	
	virtual void				EnableWhitelistFileTracking( bool bEnable, bool bCacheAllVPKHashes, bool bRecalculateAndCheckHashes );
	virtual void				RegisterFileWhitelist( IPureServerWhitelist *pWhiteList, IFileList **ppFilesToReload ) OVERRIDE;
	virtual	void				MarkAllCRCsUnverified();
	virtual void				CacheFileCRCs( const char *pPathname, ECacheCRCType eType, IFileList *pFilter );
	//void						CacheFileCRCs_R( const char *pPathname, ECacheCRCType eType, IFileList *pFilter, CUtlDict<int,int> &searchPathNames );
	virtual EFileCRCStatus		CheckCachedFileHash( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash );
	virtual int					GetUnverifiedFileHashes( CUnverifiedFileHash *pFiles, int nMaxFiles );
	virtual int					GetWhitelistSpewFlags();
	virtual void				SetWhitelistSpewFlags( int flags );
	virtual void				InstallDirtyDiskReportFunc( FSDirtyDiskReportFunc_t func );

	// Low-level file caching
	virtual FileCacheHandle_t CreateFileCache();
	virtual void AddFilesToFileCache( FileCacheHandle_t cacheId, const char **ppFileNames, int nFileNames, const char *pPathID );
	virtual bool IsFileCacheFileLoaded( FileCacheHandle_t cacheId, const char* pFileName );
	virtual bool IsFileCacheLoaded( FileCacheHandle_t cacheId );
	virtual void DestroyFileCache( FileCacheHandle_t cacheId );

	virtual void				CacheAllVPKFileHashes( bool bCacheAllVPKHashes, bool bRecalculateAndCheckHashes );
	virtual bool				CheckVPKFileHash( int PackFileID, int nPackFileNumber, int nFileFraction, MD5Value_t &md5Value );
	virtual void				NotifyFileUnloaded( const char *pszFilename, const char *pPathId ) OVERRIDE;

	// Returns the file system statistics retreived by the implementation.  Returns NULL if not supported.
	virtual const FileSystemStatistics *GetFilesystemStatistics();
	
	// Load dlls
	virtual CSysModule 			*LoadModule( const char *pFileName, const char *pPathID, bool bValidatedDllOnly );
	virtual void				UnloadModule( CSysModule *pModule );

	//--------------------------------------------------------
	// asynchronous file loading
	//--------------------------------------------------------
	virtual FSAsyncStatus_t		AsyncReadMultiple( const FileAsyncRequest_t *pRequests, int nRequests, FSAsyncControl_t *pControls );
	virtual FSAsyncStatus_t		AsyncReadMultipleCreditAlloc( const FileAsyncRequest_t *pRequests, int nRequests, const char *pszFile, int line, FSAsyncControl_t *phControls = NULL );
	virtual FSAsyncStatus_t		AsyncFinish( FSAsyncControl_t hControl, bool wait );
	virtual FSAsyncStatus_t		AsyncGetResult( FSAsyncControl_t hControl, void **ppData, int *pSize );
	virtual FSAsyncStatus_t		AsyncAbort( FSAsyncControl_t hControl );
	virtual FSAsyncStatus_t		AsyncStatus( FSAsyncControl_t hControl );
	virtual FSAsyncStatus_t		AsyncSetPriority(FSAsyncControl_t hControl, int newPriority);
	virtual FSAsyncStatus_t		AsyncFlush();
	virtual FSAsyncStatus_t		AsyncAppend(const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, FSAsyncControl_t *pControl) { return AsyncWrite( pFileName, pSrc, nSrcBytes, bFreeMemory, true, pControl); }
	virtual FSAsyncStatus_t		AsyncWrite(const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, bool bAppend, FSAsyncControl_t *pControl);
	virtual FSAsyncStatus_t		AsyncWriteFile(const char *pFileName, const CUtlBuffer *pSrc, int nSrcBytes, bool bFreeMemory, bool bAppend, FSAsyncControl_t *pControl);
	virtual FSAsyncStatus_t		AsyncAppendFile(const char *pDestFileName, const char *pSrcFileName, FSAsyncControl_t *pControl);
	virtual void				AsyncFinishAll( int iToPriority = INT_MIN );
	virtual void				AsyncFinishAllWrites();
	virtual bool				AsyncSuspend();
	virtual bool				AsyncResume();

	virtual void				AsyncAddRef( FSAsyncControl_t hControl );
	virtual void				AsyncRelease( FSAsyncControl_t hControl );
	virtual FSAsyncStatus_t		AsyncBeginRead( const char *pszFile, FSAsyncFile_t *phFile );
	virtual FSAsyncStatus_t		AsyncEndRead( FSAsyncFile_t hFile );
	virtual void				AsyncAddFetcher( IAsyncFileFetch *pFetcher );
	virtual void				AsyncRemoveFetcher( IAsyncFileFetch *pFetcher );

	//--------------------------------------------------------
	// pack files
	//--------------------------------------------------------
	bool						AddPackFile( const char *pFileName, const char *pathID );
	bool						AddPackFileFromPath( const char *pPath, const char *pakfile, bool bCheckForAppendedPack, const char *pathID );

	// converts a partial path into a full path
	// can be filtered to restrict path types and can provide info about resolved path
	virtual const char			*RelativePathToFullPath( const char *pFileName, const char *pPathID, OUT_Z_CAP(maxLenInChars) char *pDest, int maxLenInChars, PathTypeFilter_t pathFilter = FILTER_NONE, PathTypeQuery_t *pPathType = NULL );

	// Returns the search path, each path is separated by ;s. Returns the length of the string returned
	virtual int					GetSearchPath( const char *pathID, bool bGetPackFiles, OUT_Z_CAP(maxLenInChars) char *pDest, int maxLenInChars );

#if defined( TRACK_BLOCKING_IO )
	virtual void				EnableBlockingFileAccessTracking( bool state );
	virtual bool				IsBlockingFileAccessEnabled() const;
	virtual IBlockingFileItemList *RetrieveBlockingFileAccessInfo();

	virtual void				RecordBlockingFileAccess( bool synchronous, const FileBlockingItem& item );

	virtual bool				SetAllowSynchronousLogging( bool state );
#endif

	virtual bool				GetFileTypeForFullPath( char const *pFullPath, wchar_t *buf, size_t bufSizeInBytes );

	virtual void				BeginMapAccess();
	virtual void				EndMapAccess();
	virtual bool				FullPathToRelativePathEx( const char *pFullpath, const char *pPathId, OUT_Z_CAP(maxLenInChars) char *pDest, int maxLenInChars );

	FSAsyncStatus_t				SyncRead( const FileAsyncRequest_t &request );
	FSAsyncStatus_t				SyncWrite(const char *pszFilename, const void *pSrc, int nSrcBytes, bool bFreeMemory, bool bAppend );
	FSAsyncStatus_t				SyncAppendFile(const char *pAppendToFileName, const char *pAppendFromFileName );
	FSAsyncStatus_t				SyncGetFileSize( const FileAsyncRequest_t &request );
	void						DoAsyncCallback( const FileAsyncRequest_t &request, void *pData, int nBytesRead, FSAsyncStatus_t result );

	void						SetupPreloadData();
	void						DiscardPreloadData();

	virtual void				LoadCompiledKeyValues( KeyValuesPreloadType_t type, char const *archiveFile );

	// If the "PreloadedData" hasn't been purged, then this'll try and instance the KeyValues using the fast path of compiled keyvalues loaded during startup.
	// Otherwise, it'll just fall through to the regular KeyValues loading routines
	virtual KeyValues			*LoadKeyValues( KeyValuesPreloadType_t type, char const *filename, char const *pPathID = 0 );
	virtual bool				LoadKeyValues( KeyValues& head, KeyValuesPreloadType_t type, char const *filename, char const *pPathID = 0 );
	virtual bool				ExtractRootKeyName( KeyValuesPreloadType_t type, char *outbuf, size_t bufsize, char const *filename, char const *pPathID = 0 );

	virtual DVDMode_t			GetDVDMode() { return m_DVDMode; }

	FSDirtyDiskReportFunc_t		GetDirtyDiskReportFunc() { return m_DirtyDiskReportFunc; }

	//-----------------------------------------------------------------------------
	// MemoryFile cache implementation
	//-----------------------------------------------------------------------------
	class CFileCacheObject;

	// XXX For now, we assume that all path IDs are "GAME", never cache files
	// outside of the game search path, and preferentially return those files
	// whenever anyone searches for a match even if an on-disk file in another
	// folder would have been found first in a traditional search. extending
	// the memory cache to cover non-game files isn't necessary right now, but
	// should just be a matter of defining a more complex key type. (henryg)

	// Register a CMemoryFileBacking; must balance with UnregisterMemoryFile.
	// Returns false and outputs an ref-bumped pointer to the existing entry
	// if the same file has already been registered by someone else; this must
	// be Unregistered to maintain the balance.
	virtual bool RegisterMemoryFile( CMemoryFileBacking *pFile, CMemoryFileBacking **ppExistingFileWithRef );

	// Unregister a CMemoryFileBacking; must balance with RegisterMemoryFile.
	virtual void UnregisterMemoryFile( CMemoryFileBacking *pFile );

	//------------------------------------
	// Synchronous path for file operations
	//------------------------------------
	class CPathIDInfo
	{
	public:
		CPathIDInfo() : m_bByRequestOnly(), m_pDebugPathID() {}
		const CUtlSymbol& GetPathID() const;
		const char* GetPathIDString() const;
		void SetPathID( CUtlSymbol id );

	public:
		// See MarkPathIDByRequestOnly.
		bool m_bByRequestOnly;

	private:
		CUtlSymbol m_PathID;
		const char *m_pDebugPathID;
	};

	////////////////////////////////////////////////
	// IMPLEMENTATION DETAILS FOR CBaseFileSystem //
	////////////////////////////////////////////////

	class CSearchPath
	{
	public:
							CSearchPath( void );
							~CSearchPath( void );

		const char* GetPathString() const;
		const char* GetDebugString() const;
		
		// Path ID ("game", "mod", "gamebin") accessors.
		const CUtlSymbol& GetPathID() const;
		const char* GetPathIDString() const;

		// Search path (c:\hl2\hl2) accessors.
		void SetPath( CUtlSymbol id );
		const CUtlSymbol& GetPath() const;

		void SetPackFile(CPackFile *pPackFile) { m_pPackFile = pPackFile; }
		CPackFile *GetPackFile() const { return m_pPackFile; }

		#ifdef SUPPORT_PACKED_STORE
		void SetPackedStore( CPackedStoreRefCount *pPackedStore ) { /*m_pPackedStore = pPackedStore;*/ }
		#endif
		CPackedStoreRefCount *GetPackedStore() const { return m_pPackedStore; }

		bool IsMapPath() const;

		int					m_storeId;

		// Used to track if its search 
		CPathIDInfo			*m_pPathIDInfo;

		bool				m_bIsRemotePath;

		bool				m_bIsTrustedForPureServer;

	private:
		CUtlSymbol			m_Path;
		const char			*m_pDebugPath;
		CPackFile			*m_pPackFile;
		CPackedStoreRefCount *m_pPackedStore;
	};

	class CSearchPathsVisits
	{
	public:
		void Reset()
		{
			m_Visits.RemoveAll();
		}

		bool MarkVisit( const CSearchPath &searchPath )
		{
			if ( m_Visits.Find( searchPath.m_storeId ) == m_Visits.InvalidIndex() )
			{
				MEM_ALLOC_CREDIT();
				m_Visits.AddToTail( searchPath.m_storeId );
				return false;
			}
			return true;
		}

	private:
		CUtlVector<int> m_Visits;	// This is a copy of IDs for the search paths we've visited, so 
	};

	class CSearchPathsIterator
	{
	public:
		CSearchPath *GetFirst();
		CSearchPath *GetNext();

	private:
		CSearchPathsIterator( const  CSearchPathsIterator & );
		void operator=(const CSearchPathsIterator &);
		void CopySearchPaths( const CUtlVector<CSearchPath>	&searchPaths );

		int							m_iCurrent;
		CUtlSymbol					m_pathID;
		CUtlVector<CSearchPath> 	m_SearchPaths;
		CSearchPathsVisits			m_visits;
		CSearchPath					m_EmptySearchPath;
		CPathIDInfo					m_EmptyPathIDInfo;
		PathTypeFilter_t			m_PathTypeFilter;
		char						m_Filename[MAX_PATH];	// set for relative names only
	};

	friend class CSearchPathsIterator;

	struct FindData_t
	{
		WIN32_FIND_DATA		findData;
		int					currentSearchPathID;
		CUtlVector<char>	wildCardString;
		HANDLE				findHandle;
		CSearchPathsVisits	m_VisitedSearchPaths;	// This is a copy of IDs for the search paths we've visited, so avoids searching duplicate paths.
		int					m_CurrentStoreID;		// CSearchPath::m_storeId of the current search path.

		CUtlSymbol			m_FilterPathID;			// What path ID are we looking at? Ignore all others. (Only set by FindFirstEx).

		CUtlDict<int,int>	m_VisitedFiles;			// We go through the search paths in priority order, and we use this to make sure
													// that we don't return the same file more than once.
		CUtlStringList		m_fileMatchesFromVPKOrPak;
		CUtlStringList		m_dirMatchesFromVPKOrPak;
	};

	friend class CSearchPath;

	IPureServerWhitelist	*m_pPureServerWhitelist;
	int					m_WhitelistSpewFlags; // Combination of WHITELIST_SPEW_ flags.

	// logging functions
	CUtlVector< FileSystemLoggingFunc_t > m_LogFuncs;

	CThreadMutex m_SearchPathsMutex;
	CUtlVector< CSearchPath > m_SearchPaths;
	CUtlVector<CPathIDInfo*> m_PathIDInfos;
	CUtlLinkedList<FindData_t> m_FindData;

	CSearchPath *FindSearchPathByStoreId( int storeId );

	int m_iMapLoad;

	// Global list of pack file handles
	CUtlVector<CPackFile *> m_ZipFiles;

	FILE *m_pLogFile;
	bool m_bOutputDebugString;

	IThreadPool *	m_pThreadPool;
	CThreadFastMutex m_AsyncCallbackMutex;

	// Statistics:
	FileSystemStatistics m_Stats;

#if defined( TRACK_BLOCKING_IO )
	CBlockingFileItemList	*m_pBlockingItems;
	bool					m_bBlockingFileAccessReportingEnabled;
	bool					m_bAllowSynchronousLogging;

	friend class			CBlockingFileItemList;
	friend class			CAutoBlockReporter;
#endif

	CFileTracker2	m_FileTracker2;

public:
	//----------------------------------------------------------------------------
	// Purpose: Functions implementing basic file system behavior.
	//----------------------------------------------------------------------------
	virtual FILE *FS_fopen( const char *filename, const char *options, unsigned flags, int64 *size ) = 0;
	virtual void FS_setbufsize( FILE *fp, unsigned nBytes ) = 0;
	virtual void FS_fclose( FILE *fp ) = 0;
	virtual void FS_fseek( FILE *fp, int64 pos, int seekType ) = 0;
	virtual long FS_ftell( FILE *fp ) = 0;
	virtual int FS_feof( FILE *fp ) = 0;
	size_t FS_fread( void *dest, size_t size, FILE *fp ) { return FS_fread( dest, (size_t)-1, size, fp ); }
	virtual size_t FS_fread( void *dest, size_t destSize, size_t size, FILE *fp ) = 0;
    virtual size_t FS_fwrite( const void *src, size_t size, FILE *fp ) = 0;
	virtual bool FS_setmode( FILE *fp, FileMode_t mode ) { return false; }
	virtual size_t FS_vfprintf( FILE *fp, const char *fmt, va_list list ) = 0;
	virtual int FS_ferror( FILE *fp ) = 0;
	virtual int FS_fflush( FILE *fp ) = 0;
	virtual char *FS_fgets( char *dest, int destSize, FILE *fp ) = 0;
	virtual int FS_stat( const char *path, struct _stat *buf, bool *pbLoadedFromSteamCache=NULL ) = 0;
	virtual int FS_chmod( const char *path, int pmode ) = 0;
	virtual HANDLE FS_FindFirstFile( const char *findname, WIN32_FIND_DATA *dat) = 0;
	virtual bool FS_FindNextFile(HANDLE handle, WIN32_FIND_DATA *dat) = 0;
	virtual bool FS_FindClose(HANDLE handle) = 0;
	virtual int FS_GetSectorSize( FILE * ) { return 1; }

#if defined( TRACK_BLOCKING_IO )
	void BlockingFileAccess_EnterCriticalSection();
	void BlockingFileAccess_LeaveCriticalSection();

	CThreadMutex m_BlockingFileMutex;

#endif

	void GetFileNameForHandle( FileHandle_t handle, char *buf, size_t buflen );

public:
	//-----------------------------------------------------------------------------
	// Purpose: For tracking unclosed files
	// NOTE:  The symbol table could take up memory that we don't want to eat here.
	// In that case, we shouldn't store them in a table, or we should store them as locally allocates stings
	//  so we can control the size
	//-----------------------------------------------------------------------------
	class COpenedFile
	{
	public:
					COpenedFile( void );
					~COpenedFile( void );

					COpenedFile( const COpenedFile& src );

		bool operator==( const COpenedFile& src ) const;

		void		SetName( char const *name );
		char const	*GetName( void );

		FILE		*m_pFile;
		char		*m_pName;
	};

	CThreadFastMutex m_MemoryFileMutex;
	CUtlHashtable< const char*, CMemoryFileBacking* > m_MemoryFileHash;


	//CUtlRBTree< COpenedFile, int > m_OpenedFiles;
	CThreadMutex m_OpenedFilesMutex;
	CUtlVector <COpenedFile>	m_OpenedFiles;

	static bool OpenedFileLessFunc( COpenedFile const& src1, COpenedFile const& src2 );

	FileWarningLevel_t			m_fwLevel;
	void						(*m_pfnWarning)( PRINTF_FORMAT_STRING const char *fmt, ... );

	FILE						*Trace_FOpen( const char *filename, const char *options, unsigned flags, int64 *size );
	void						Trace_FClose( FILE *fp );
	void						Trace_FRead( int size, FILE* file );
	void						Trace_FWrite( int size, FILE* file );

	void						Trace_DumpUnclosedFiles( void );

public:
	void						LogAccessToFile( char const *accesstype, char const *fullpath, char const *options );
	void						Warning( FileWarningLevel_t level, PRINTF_FORMAT_STRING const char *fmt, ... );
	FileHandle_t				FindFileInSearchPath( CFileOpenInfo &openInfo ); // Is normally a protected function.
#ifdef BUILD_GMOD
	IGet*	GetIGet() { return m_pIGet; }; // Doesn't exist in Gmod
#endif

public:
	// Note: if pFoundStoreID is passed in, then it will set that to the CSearchPath::m_storeId value of the search path it found the file in.
	const char*					FindFirstHelper( const char *pWildCard, const char *pPathID, FileFindHandle_t *pHandle, int *pFoundStoreID );
	bool						FindNextFileHelper( FindData_t *pFindData, int *pFoundStoreID );
	bool						FindNextFileInVPKOrPakHelper( FindData_t *pFindData );

	void						RemoveAllMapSearchPaths( void );
	void						AddMapPackFile( const char *pPath, const char *pPathID, SearchPathAdd_t addType );
	void						AddPackFiles( const char *pPath, const CUtlSymbol &pathID, SearchPathAdd_t addType );
	bool						PreparePackFile( CPackFile &packfile, int offsetofpackinmetafile, int64 filelen );
	void						AddVPKFile( const char *pPath, const char *pPathID, SearchPathAdd_t addType );
	bool						RemoveVPKFile( const char *pPath, const char *pPathID );

	void						HandleOpenRegularFile( CFileOpenInfo &openInfo, bool bIsAbsolutePath );

	long						FastFileTime( const CSearchPath *path, const char *pFileName );

	const char					*GetWritePath( const char *pFilename, const char *pathID );

	// Computes a full write path
	void						ComputeFullWritePath( char* pDest, int maxlen, const char *pWritePathID, char const *pRelativePath );

	void						AddSearchPathInternal( const char *pPath, const char *pathID, SearchPathAdd_t addType, bool bAddPackFiles );

	// Opens a file for read or write
	FileHandle_t OpenForRead( const char *pFileName, const char *pOptions, unsigned flags, const char *pathID, char **ppszResolvedFilename = NULL );
	FileHandle_t OpenForWrite( const char *pFileName, const char *pOptions, const char *pathID );
	CSearchPath *FindWritePath( const char *pFilename, const char *pathID );

	// Helper function for fs_log file logging
	void LogFileAccess( const char *pFullFileName );
	bool LookupKeyValuesRootKeyName( char const *filename, char const *pPathID, char *rootName, size_t bufsize );
	void UnloadCompiledKeyValues();

	// If bByRequestOnly is -1, then it will default to false if it doesn't already exist, and it 
	// won't change it if it does already exist. Otherwise, it will be set to the value of bByRequestOnly.
	CPathIDInfo*				FindOrAddPathIDInfo( const CUtlSymbol &id, int bByRequestOnly );
	static bool					FilterByPathID( const CSearchPath *pSearchPath, const CUtlSymbol &pathID );

	// Global/shared filename/path table
	CUtlFilenameSymbolTable		m_FileNames;

	int				m_WhitelistFileTrackingEnabled;	// -1 if unset, 0 if disabled (single player), 1 if enabled (multiplayer).
	FSDirtyDiskReportFunc_t m_DirtyDiskReportFunc;

	void	SetSearchPathIsTrustedSource( CSearchPath *pPath );

	struct CompiledKeyValuesPreloaders_t
	{
		CompiledKeyValuesPreloaders_t() :
			m_CacheFile( 0 ),
			m_pReader( 0 )
		{
		}
		FileNameHandle_t			m_CacheFile;
		CCompiledKeyValuesReader	*m_pReader;
	};

	CompiledKeyValuesPreloaders_t	m_PreloadData[ NUM_PRELOAD_TYPES ];

	static CUtlSymbol			m_GamePathID;
	static CUtlSymbol			m_BSPPathID;

	static DVDMode_t			m_DVDMode;

	// Pack exclude paths are strictly for 360 to allow holes in search paths and pack files
	// which fall through to support new or dynamic data on the host pc.
	static CUtlVector< FileNameHandle_t > m_ExcludePaths;

	/// List of installed hooks to intercept async file operations
	CUtlVector< IAsyncFileFetch * > m_vecAsyncFetchers;

	/// List of active async jobs being serviced by customer fetchers
	CUtlVector< CFileAsyncReadJob * > m_vecAsyncCustomFetchJobs;

	/// Remove a custom fetch job from the list (and release our reference)
	friend class CFileAsyncReadJob;
	void RemoveAsyncCustomFetchJob( CFileAsyncReadJob *pJob );

	char m_pBaseDir[MAX_PATH];
	int m_pBaseLength;
};

class CFileOpenInfo
{
public:
	CFileOpenInfo( CBaseFileSystem *pFileSystem, const char *pFileName, const CSearchPath *path, const char *pOptions, int flags, char **ppszResolvedFilename ) : 
		m_pFileSystem( pFileSystem ), m_ppszResolvedFilename( ppszResolvedFilename ), m_pFileName( pFileName ), m_pSearchPath( path ), m_pOptions( pOptions ), m_Flags( flags )
	{
		m_pFileHandle = NULL;
		m_ePureFileClass = ePureServerFileClass_Any;
		if ( m_pFileSystem->m_pPureServerWhitelist )
		{
			m_ePureFileClass = m_pFileSystem->m_pPureServerWhitelist->GetFileClass( pFileName );
		}

		if ( m_ppszResolvedFilename )
			*m_ppszResolvedFilename = NULL;
		m_pPackFile = NULL;
		m_pVPKFile = NULL;
		m_AbsolutePath[0] = '\0';
	}
	
	~CFileOpenInfo()
	{
	}
	
	void SetAbsolutePath( const char *pFormat, ... )
	{
	}
	
	void SetResolvedFilename( const char *pStr )
	{
	}

	void HandleFileCRCTracking( const char *pRelativeFileName )
	{
	}

public:
	CBaseFileSystem* m_pFileSystem;

	// These are output parameters.
	void *m_pFileHandle;
	char **m_ppszResolvedFilename;

	void *m_pPackFile;
	void *m_pVPKFile;

	const char *m_pFileName;
	const CSearchPath *m_pSearchPath;
	const char *m_pOptions;
	int m_Flags;

	EPureServerFileClass m_ePureFileClass;

	char m_AbsolutePath[MAX_FILEPATH];	// This is set 
};