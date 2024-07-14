#include <GarrysMod/FactoryLoader.hpp>
#include <utlsymbol.h>
#include <filesystem.h>

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
		void SetPackedStore( CPackedStoreRefCount *pPackedStore ) { m_pPackedStore = pPackedStore; }
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

#define MAX_FILEPATH 512
class CFileOpenInfo
{
public:
	CFileOpenInfo( void* *pFileSystem, const char *pFileName, const CSearchPath *path, const char *pOptions, int flags, char **ppszResolvedFilename ) : 
		m_pFileSystem( pFileSystem ), m_pFileName( pFileName ), m_pSearchPath( path ), m_pOptions( pOptions ), m_Flags( flags ), m_ppszResolvedFilename( ppszResolvedFilename )
	{
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
	void *m_pFileSystem;

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