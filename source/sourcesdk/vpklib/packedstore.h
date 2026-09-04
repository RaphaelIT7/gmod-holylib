//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef PACKEDSTORE_H
#define PACKEDSTORE_H
#ifdef _WIN32
#pragma once
#endif


#include <tier0/platform.h>
#include <tier0/threadtools.h>
#include <tier0/tslist.h>
#include <tier2/tier2.h>

#include "filesystem.h"
#include "tier1/utlintrusivelist.h"
#include "tier1/utlvector.h"
#include "tier1/utllinkedlist.h"
#include "tier1/UtlSortVector.h"
#include "tier1/utlmap.h"
#include "tier1/checksum_md5.h"

#include "sdk_backports.h"
#include <atomic>

// RaphaelIT7: GMod doesn't use this
// #define VPK_ENABLE_SIGNING

constexpr int k_nVPKDefaultChunkSize = 200 * 1024 * 1024;

class CPackedStore;


struct ChunkHashFraction_t
{
	int m_nPackFileNumber;
	int m_nFileFraction;
	int m_cbChunkLen;
	MD5Value_t m_md5contents;
};

class ChunkHashFractionLess_t
{
public:
	bool Less( const ChunkHashFraction_t& lhs, const ChunkHashFraction_t& rhs, void * )
	{
		if ( lhs.m_nPackFileNumber < rhs.m_nPackFileNumber )
			return true;
		if ( lhs.m_nPackFileNumber > rhs.m_nPackFileNumber )
			return false;

		return lhs.m_nFileFraction < rhs.m_nFileFraction;
	}
};

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

	FORCEINLINE operator bool() const
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

	int Tell() const
	{
		return m_nCurrentFileOffset;
	}

	uint32 GetFileCRCFromHeaderData() const
	{
		uint32 *pCRC = (uint32 *)m_pHeaderData;
		return *pCRC;
	}

	FORCEINLINE void GetPackFileName( OUT_Z_CAP(cchFileNameOut) char *pchFileNameOut, intp cchFileNameOut );

	template<intp outSize>
	FORCEINLINE void GetPackFileName( OUT_Z_ARRAY char (&pchFileNameOut)[outSize] )
	{
		GetPackFileName( pchFileNameOut, outSize );
	}
};

#define MAX_ARCHIVE_FILES_TO_KEEP_OPEN_AT_ONCE 512

#define PACKEDFILE_EXT_HASH_SIZE 15


#ifdef _WIN32
typedef HANDLE PackDataFileHandle_t;
#else
typedef FileHandle_t PackDataFileHandle_t;
#endif

struct FileHandleTracker_t
{
	int m_nFileNumber;
	PackDataFileHandle_t m_hFileHandle;
	int m_nCurOfs;
	CThreadFastMutex m_Mutex;

	FileHandleTracker_t( void )
	{
		m_nFileNumber = -1;
		m_hFileHandle = nullptr;
		m_nCurOfs = 0;
	}
};

enum ePackedStoreAddResultCode
{
	EPADD_NEWFILE,											// the file was added and is new
	EPADD_ADDSAMEFILE,										// the file was already present, and the contents are the same as what you passed.
	EPADD_UPDATEFILE,										// the file was alreayd present and its contents have been updated
	EPADD_ERROR,											// some error has resulted
};

// Describe a file inside of a VPK file.  Is not memory efficient; only used for interface
// purposes and during file building
struct VPKContentFileInfo_t
{
	CUtlString m_sName;
	int m_idxChunk;
	uint32 m_iTotalSize;
	uint32 m_iOffsetInChunk;
	uint32 m_iPreloadSize;
	const void *m_pPreloadData;
	//MD5Value_t m_md5Source; // source content before munging & release optimization.  Used for incremental builds
	uint32 m_crc; // CRC of actual file contents

	/// Size of the data in the chunk file.  (Excludes the preload data size)
	uint32 GetSizeInChunkFile() const
	{
		Assert( m_iTotalSize >= m_iPreloadSize );
		return m_iTotalSize - m_iPreloadSize;
	}

	VPKContentFileInfo_t()
	{
		m_idxChunk = -1;
		m_iTotalSize = 0;
		m_iOffsetInChunk = 0;
		m_iPreloadSize = 0;
		m_crc = 0;
		m_pPreloadData = NULL;
		// BitwiseClear( m_md5Source.bits );
	}
};


// a 1MB chunk of cached VPK data
// For CPackedStoreReadCache
struct CachedVPKRead_t
{
	CachedVPKRead_t()
	{
		m_nPackFileNumber = 0;
		m_nFileFraction = 0;
		m_pubBuffer = NULL;
		m_cubBuffer = 0;
		m_idxLRU = -1;
		m_hMD5RequestHandle= 0;
		m_cFailedHashes = 0;
		BitwiseClear(m_md5Value);
	}
	int m_nPackFileNumber;	// identifier
	int m_nFileFraction;	// identifier
	uint8 *m_pubBuffer;		// data
	int m_cubBuffer;		// data
	int m_idxLRU;			// bookkeeping
	int m_hMD5RequestHandle;// bookkeeping
	int m_cFailedHashes;	// did the MD5 match what it was supposed to?
	MD5Value_t m_md5Value;

	static bool Less( const CachedVPKRead_t& lhs, const CachedVPKRead_t& rhs )
	{
		if ( lhs.m_nPackFileNumber < rhs.m_nPackFileNumber )
			return true;
		if ( lhs.m_nPackFileNumber > rhs.m_nPackFileNumber )
			return false;
		return lhs.m_nFileFraction < rhs.m_nFileFraction;
	}
};


// Read the VPK file in 1MB chunks
// and we hang on to those chunks so we can serve other reads out of the cache
// This sounds great, but is only of secondary importance.
// The primary reason we do this is so that the FileTracker can calculate the 
// MD5 of the 1MB chunks asynchronously in another thread - while we hold
// the chunk in cache - making the MD5 calculation "free"
class CPackedStoreReadCache : public CAlignedNewDelete<16>
{
public:
	CPackedStoreReadCache( IBaseFileSystem *pFS );

	bool ReadCacheLine( FileHandleTracker_t &fHandle, CachedVPKRead_t &cachedVPKRead );
	bool BCanSatisfyFromReadCache( uint8 *pOutData, CPackedStoreFileHandle &handle, FileHandleTracker_t &fHandle, int nDesiredPos, int nNumBytes, int &nRead );
	bool BCanSatisfyFromReadCacheInternal( uint8 *pOutData, CPackedStoreFileHandle &handle, FileHandleTracker_t &fHandle, int nDesiredPos, int nNumBytes, int &nRead );
	bool CheckMd5Result( CachedVPKRead_t &cachedVPKRead );
	int FindBufferToUse();
	void RetryBadCacheLine( CachedVPKRead_t &cachedVPKRead );
	void RetryAllBadCacheLines();


	// cache 64 MiB total
	static constexpr inline int k_nCacheBuffersToKeep = 8;
	static constexpr inline int k_cubCacheBufferSize = 0x00100000; // 1MiB
	static constexpr inline int k_nCacheBufferMask = 0x7FF00000;

	CThreadRWLock m_rwlock;
	CUtlRBTree<CachedVPKRead_t> m_treeCachedVPKRead; // all the reads we have done

	CTSQueue<CachedVPKRead_t> m_queueCachedVPKReadsRetry; // all the reads that have failed
	CUtlLinkedList<CachedVPKRead_t> m_listCachedVPKReadsFailed; // all the reads that have failed

	// current items in the cache
	int m_cItemsInCache;
	unsigned short m_rgCurrentCacheIndex[k_nCacheBuffersToKeep];
	// dimhotepus: atomic_uint -> atomic_ullong as former overflows in 49.7 days.
	std::atomic_ullong m_rgLastUsedTime[k_nCacheBuffersToKeep];

	CPackedStore *m_pPackedStore;
	IBaseFileSystem *m_pFileSystem;
	IThreadedFileMD5Processor *m_pFileTracker;
	// stats
	int m_cubReadFromCache;
	int m_cReadFromCache;
	int m_cDiscardsFromCache;
	int m_cAddedToCache;
	int m_cCacheMiss;
	int m_cubCacheMiss;
	int m_cFileErrors;
	int m_cFileErrorsCorrected;
	int m_cFileResultsDifferent;
};

class CPackedStore : public CAlignedNewDelete<16>
{
public:
	CPackedStore( char const *pFileBasename, char *pszFName, intp fnameSize, IBaseFileSystem *pFS, bool bOpenForWrite = false );

	void RegisterFileTracker( IThreadedFileMD5Processor *pFileTracker ) { m_pFileTracker = pFileTracker; m_PackedStoreReadCache.m_pFileTracker = pFileTracker; }

	CPackedStoreFileHandle OpenFile( char const *pFile );
	CPackedStoreFileHandle GetHandleForHashingFiles();

	/// Add/update the given file to the directory.  Does not write any chunk files
	void AddFileToDirectory( const VPKContentFileInfo_t &info );

	/// Remove the specified file from the directory.  Returns true if removed, false if not found
	bool RemoveFileFromDirectory( const char *pszName );

	// GMod
	bool DirectoryEntryExists( const char *pszPath );

	/// Add file, writing file data to the end
	/// of the current chunk
	ePackedStoreAddResultCode AddFile( char const *pFile, uint16 nMetaDataSize, const void *pFileData, uint32 nFullFileSize, bool bMultiChunk, uint32 const *pCrcToUse = NULL );

	// write out the file directory
	void Write( void );

	int ReadData( CPackedStoreFileHandle &handle, void *pOutData, int nNumBytes );

	~CPackedStore( void );

	FORCEINLINE void *DirectoryData( void )
	{
		return m_DirectoryData.Base();
	}

	// Get a list of all the files in the zip You are responsible for freeing the contents of
	// outFilenames (call outFilenames.PurgeAndDeleteElements).
	intp GetFileList( CUtlStringList &outFilenames, bool bFormattedOutput, bool bSortedOutput );

	// Get a list of all files that match the given wildcard string
	intp GetFileList( const char *pWildCard, CUtlStringList &outFilenames, bool bFormattedOutput, bool bSortedOutput );
	
	/// Get a list of all files that match the given wildcard string, fetching all the details
	/// at once
	void GetFileList( const char *pWildcard, CUtlVector<VPKContentFileInfo_t> &outVecResults );

	// Get a list of all directories of the given wildcard
	intp GetFileAndDirLists( const char *pWildCard, CUtlStringList &outDirnames, CUtlStringList &outFilenames, bool bSortedOutput, bool bFullPaths = false );
	intp GetFileAndDirLists( CUtlStringList &outDirnames, CUtlStringList &outFilenames, bool bSortedOutput );

	bool IsEmpty( void ) const;

	/// Hash metadata and chunk files
	void HashEverything();

	/// Hash all chunk files.  Don't forget to rehash the metadata afterwords!
	void HashAllChunkFiles();

	/// Hash all the metadata.  (Everything that's not in the chunk files)
	void HashMetadata();

	/// Re-hash a single chunk file.  Don't forget to rehash the metadata afterwords!
	void HashChunkFile( int iChunkFileIndex );

	bool HashEntirePackFile( CPackedStoreFileHandle &handle, int64 &nFileSize, int nFileFraction, int nFractionSize, FileHash_t &fileHash );
	void ComputeDirectoryHash( MD5Value_t &md5Directory );
	void ComputeChunkHash( MD5Value_t &md5ChunkHashes );
	MD5Value_t &GetDirFileMD5Value() { return m_TotalFileMD5; }
	bool BTestDirectoryHash();
	bool BTestMasterChunkHash();
	CUtlSortVector<ChunkHashFraction_t, ChunkHashFractionLess_t > &AccessPackFileHashes() { return m_vecChunkHashFraction; }
	bool FindFileHashFraction( int nPackFileNumber, int nFileFraction, ChunkHashFraction_t &chunkFileHashFraction );
	void GetPackFileLoadErrorSummary( CUtlString &sErrors );

	void GetPackFileName( CPackedStoreFileHandle &handle, OUT_Z_CAP(cchFileNameOut) char *pchFileNameOut, intp cchFileNameOut ) const;
	template<intp outSize>
	void GetPackFileName( CPackedStoreFileHandle &handle, OUT_Z_ARRAY char (&pchFileNameOut)[outSize] ) const
	{
		return GetPackFileName( handle, pchFileNameOut, outSize );
	}
	void GetDataFileName( OUT_Z_CAP(cchFileNameOut) char *pchFileNameOut, intp cchFileNameOut, intp nFileNumber ) const;
	template<intp outSize>
	void GetDataFileName( OUT_Z_ARRAY char (&pchFileNameOut)[outSize], intp nFileNumber ) const
	{
		return GetDataFileName( pchFileNameOut, outSize, nFileNumber );
	}
	
	char const *BaseName( void )
	{
		return m_pszFileBaseName;
	}

	char const *FullPathName( void )
	{
		return m_pszFullPathName;
	}

	void SetWriteChunkSize( int nWriteChunkSize )
	{
		m_nWriteChunkSize = nWriteChunkSize;
	}

	int GetWriteChunkSize() const { return m_nWriteChunkSize; }

	int GetHighestChunkFileIndex() const { return m_nHighestChunkFileIndex; }

	void DiscardChunkHashes( intp iChunkFileIndex );

	const CUtlVector<uint8> &GetSignaturePublicKey() const { return m_SignaturePublicKey; }
	const CUtlVector<uint8> &GetSignature() const { return m_Signature; }

#ifdef VPK_ENABLE_SIGNING
	enum ESignatureCheckResult
	{
		eSignatureCheckResult_NotSigned,
		eSignatureCheckResult_WrongKey,
		eSignatureCheckResult_Failed, // IO error, etc
		eSignatureCheckResult_InvalidSignature,
		eSignatureCheckResult_ValidSignature,
	};
	ESignatureCheckResult CheckSignature( intp nSignatureSize, const void *pSignature ) const;

	void SetKeysForSigning( intp nPrivateKeySize, const void *pPrivateKeyData, intp nPublicKeySize, const void *pPublicKeyData );
#endif

	void SetUseDirFile() { m_bUseDirFile = true; }

	int m_PackFileID;
private:
	char m_pszFileBaseName[MAX_PATH];
	char m_pszFullPathName[MAX_PATH];
	int m_nDirectoryDataSize;
	int m_nWriteChunkSize;
	bool m_bUseDirFile;

	IBaseFileSystem *m_pFileSystem;
	IThreadedFileMD5Processor *m_pFileTracker;
	CThreadFastMutex m_Mutex;
	
	CPackedStoreReadCache m_PackedStoreReadCache;

	CUtlIntrusiveList<class CFileExtensionData> m_pExtensionData[PACKEDFILE_EXT_HASH_SIZE];

	CUtlVector<uint8> m_DirectoryData;
	CUtlBlockVector<uint8> m_EmbeddedChunkData;

	CUtlSortVector<ChunkHashFraction_t, ChunkHashFractionLess_t > m_vecChunkHashFraction;
	bool BFileContainedHashes() { return m_vecChunkHashFraction.Count() > 0; }
	// these are valid if BFileContainedHashes() is true
	MD5Value_t m_DirectoryMD5;
	MD5Value_t m_ChunkHashesMD5;
	MD5Value_t m_TotalFileMD5;

	int m_nHighestChunkFileIndex;

	/// The private key that will be used to sign the directory file.
	/// This will be empty for unsigned VPK's, or if we don't know the
	/// private key.
	CUtlVector<uint8> m_SignaturePrivateKey;

	/// The public key in the VPK.
	CUtlVector<uint8> m_SignaturePublicKey;

	/// The signature that was read / computed
	CUtlVector<uint8> m_Signature;

	/// The number of bytes in the dir file that were signed
	uint32 m_nSizeOfSignedData;

	FileHandleTracker_t m_FileHandles[MAX_ARCHIVE_FILES_TO_KEEP_OPEN_AT_ONCE];
	
	void Init( void );

	struct CFileHeaderFixedData *FindFileEntry( 
		char const *pDirname, char const *pBaseName, char const *pExtension,
		uint8 **pExtBaseOut = NULL, uint8 **pNameBaseOut = NULL );

	void BuildHashTables( void );

	FileHandleTracker_t &GetFileHandle( int nFileNumber );

	// For cache-ing directory and contents data
	CUtlStringList m_directoryList; // The index of this list of directories...
	CUtlMap<intp, CUtlStringList*, intp> m_dirContents; // ...is the key to this map of filenames
	void BuildFindFirstCache();

	bool InternalRemoveFileFromDirectory( const char *pszName );

	friend class CPackedStoreReadCache;
};

FORCEINLINE int CPackedStoreFileHandle::Read( void *pOutData, int nNumBytes )
{
	return m_pOwner->ReadData( *this, pOutData, nNumBytes );
}

FORCEINLINE void CPackedStoreFileHandle::GetPackFileName( OUT_Z_CAP(cchFileNameOut) char *pchFileNameOut, intp cchFileNameOut )
{
	m_pOwner->GetPackFileName( *this, pchFileNameOut, cchFileNameOut );
}


#endif // packedtsore_h
