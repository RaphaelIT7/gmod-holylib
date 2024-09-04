//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Model loading / unloading interface
//
// $NoKeywords: $
//===========================================================================//

//#include "render_pch.h"
#include "common.h"
#include "modelloader.h"
#include "sysexternal.h"
#include "cmd.h"
#include "istudiorender.h"
#include "engine/ivmodelinfo.h"
//#include "draw.h"
#include "zone.h"
#include "edict.h"
#include "cmodel_engine.h"
//#include "cdll_engine_int.h"
#include "iscratchpad3d.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/materialsystem_config.h"
#include "gl_rsurf.h"
#include "video/ivideoservices.h"
#include "materialsystem/itexture.h"
#include "Overlay.h"
#include "utldict.h"
#include "mempool.h"
//#include "r_decal.h"
//#include "l_studio.h"
//#include "gl_drawlights.h"
#include "tier0/icommandline.h"
//#include "MapReslistGenerator.h"
#ifndef SWDS
#include "vgui_baseui_interface.h"
#endif
#include "engine/ivmodelrender.h"
//#include "host.h"
#include "datacache/idatacache.h"
//#include "sys_dll.h"
#include "datacache/imdlcache.h"
//#include "gl_cvars.h"
#include "vphysics_interface.h"
#include "filesystem/IQueuedLoader.h"
#include "tier2/tier2.h"
#include "lightcache.h"
#include "lumpfiles.h"
#include "tier2/fileutils.h"
#include "UtlSortVector.h"
#include "utlhashtable.h"
#include "tier1/lzmaDecoder.h"
#include "eiface.h"
//#include "server.h"
#include "ifilelist.h"
//#include "LoadScreenUpdate.h"
#include "optimize.h"
#include "networkstringtable.h"
#include "tier1/callqueue.h"

#include "gl_model_private.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Lump files are patches for a shipped map
// List of lump files found when map was loaded. Each entry is the lump file index for that lump id.
struct lumpfiles_t
{
	FileHandle_t		file;
	int					lumpfileindex;
	lumpfileheader_t	header;
};
static lumpfiles_t s_MapLumpFiles[HEADER_LUMPS];

//-----------------------------------------------------------------------------
// Globals used by the CMapLoadHelper
//-----------------------------------------------------------------------------
static dheader_t		s_MapHeader;
static FileHandle_t		s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
static char				s_szLoadName[128];
static char				s_szMapName[128];
static worldbrushdata_t* s_pMap = NULL;
static int				s_nMapLoadRecursion = 0;
static CUtlBuffer		s_MapBuffer;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mapfile - 
//			lumpToLoad - 
//-----------------------------------------------------------------------------
CMapLoadHelper::CMapLoadHelper(int lumpToLoad)
{
	if (lumpToLoad < 0 || lumpToLoad >= HEADER_LUMPS)
	{
		Error("Can't load lump %i, range is 0 to %i!!!", lumpToLoad, HEADER_LUMPS - 1);
	}

	m_nLumpID = lumpToLoad;
	m_nLumpSize = 0;
	m_nLumpOffset = -1;
	m_pData = NULL;
	m_pRawData = NULL;
	m_pUncompressedData = NULL;

	// Load raw lump from disk
	lump_t* lump = &s_MapHeader.lumps[lumpToLoad];
	Assert(lump);

	m_nLumpSize = lump->filelen;
	m_nLumpOffset = lump->fileofs;
	m_nLumpVersion = lump->version;

	FileHandle_t fileToUse = s_MapFileHandle;

	// If we have a lump file for this lump, use it instead
	if (IsPC() && s_MapLumpFiles[lumpToLoad].file != FILESYSTEM_INVALID_HANDLE)
	{
		fileToUse = s_MapLumpFiles[lumpToLoad].file;
		m_nLumpSize = s_MapLumpFiles[lumpToLoad].header.lumpLength;
		m_nLumpOffset = s_MapLumpFiles[lumpToLoad].header.lumpOffset;
		m_nLumpVersion = s_MapLumpFiles[lumpToLoad].header.lumpVersion;

		// Store off the lump file name
		GenerateLumpFileName(s_szLoadName, m_szLumpFilename, MAX_PATH, s_MapLumpFiles[lumpToLoad].lumpfileindex);
	}

	if (!m_nLumpSize)
	{
		// this lump has no data
		return;
	}

	if (s_MapBuffer.Base())
	{
		// bsp is in memory
		m_pData = (unsigned char*)s_MapBuffer.Base() + m_nLumpOffset;
	}
	else
	{
		if (s_MapFileHandle == FILESYSTEM_INVALID_HANDLE)
		{
			Error("Can't load map from invalid handle!!!");
		}

		unsigned nOffsetAlign, nSizeAlign, nBufferAlign;
		g_pFullFileSystem->GetOptimalIOConstraints(fileToUse, &nOffsetAlign, &nSizeAlign, &nBufferAlign);

		bool bTryOptimal = (m_nLumpOffset % 4 == 0); // Don't return badly aligned data
		unsigned int alignedOffset = m_nLumpOffset;
		unsigned int alignedBytesToRead = ((m_nLumpSize) ? m_nLumpSize : 1);

		if (bTryOptimal)
		{
			alignedOffset = AlignValue((alignedOffset - nOffsetAlign) + 1, nOffsetAlign);
			alignedBytesToRead = AlignValue((m_nLumpOffset - alignedOffset) + alignedBytesToRead, nSizeAlign);
		}

		m_pRawData = (byte*)g_pFullFileSystem->AllocOptimalReadBuffer(fileToUse, alignedBytesToRead, alignedOffset);
		if (!m_pRawData && m_nLumpSize)
		{
			Error("Can't load lump %i, allocation of %i bytes failed!!!", lumpToLoad, m_nLumpSize + 1);
		}

		if (m_nLumpSize)
		{
			g_pFullFileSystem->Seek(fileToUse, alignedOffset, FILESYSTEM_SEEK_HEAD);
			g_pFullFileSystem->ReadEx(m_pRawData, alignedBytesToRead, alignedBytesToRead, fileToUse);
			m_pData = m_pRawData + (m_nLumpOffset - alignedOffset);
		}
	}

	if (lump->uncompressedSize != 0)
	{
		// Handle compressed lump -- users of the class see the uncompressed data
		CLZMA lzma;
		AssertMsg(lzma.IsCompressed(m_pData),
			"Lump claims to be compressed but is not recognized as LZMA");

		m_nLumpSize = lzma.GetActualSize(m_pData);
		AssertMsg(lump->uncompressedSize == m_nLumpSize,
			"Lump header disagrees with lzma header for compressed lump");

		m_pUncompressedData = (unsigned char*)malloc(m_nLumpSize);
		lzma.Uncompress(m_pData, m_pUncompressedData);

		m_pData = m_pUncompressedData;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapLoadHelper::~CMapLoadHelper(void)
{
	if (m_pUncompressedData)
	{
		free(m_pUncompressedData);
	}

	if (m_pRawData)
	{
		g_pFullFileSystem->FreeOptimalReadBuffer(m_pRawData);
	}
}

//-----------------------------------------------------------------------------
// Returns the size of a particular lump without loading it...
//-----------------------------------------------------------------------------
int CMapLoadHelper::LumpSize(int lumpId)
{
	// If we have a lump file for this lump, return its length instead
	if (IsPC() && s_MapLumpFiles[lumpId].file != FILESYSTEM_INVALID_HANDLE)
	{
		return s_MapLumpFiles[lumpId].header.lumpLength;
	}

	lump_t* pLump = &s_MapHeader.lumps[lumpId];
	Assert(pLump);

	// all knowledge of compression is private, they expect and get the original size
	int originalSize = s_MapHeader.lumps[lumpId].uncompressedSize;
	if (originalSize != 0)
	{
		return originalSize;
	}

	return pLump->filelen;
}

//-----------------------------------------------------------------------------
// Loads one element in a lump.
//-----------------------------------------------------------------------------
void CMapLoadHelper::LoadLumpElement(int nElemIndex, int nElemSize, void* pData)
{
	if (!nElemSize || !m_nLumpSize)
	{
		return;
	}

	// supply from memory
	if (nElemIndex * nElemSize + nElemSize <= m_nLumpSize)
	{
		V_memcpy(pData, m_pData + nElemIndex * nElemSize, nElemSize);
	}
	else
	{
		// out of range
		Assert(0);
	}
}

//-----------------------------------------------------------------------------
// Loads one element in a lump.
//-----------------------------------------------------------------------------
void CMapLoadHelper::LoadLumpData(int offset, int size, void* pData)
{
	if (!size || !m_nLumpSize)
	{
		return;
	}

	if (offset + size <= m_nLumpSize)
	{
		V_memcpy(pData, m_pData + offset, size);
	}
	else
	{
		// out of range
		Assert(0);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char* CMapLoadHelper::GetMapName(void)
{
	return s_szMapName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
char* CMapLoadHelper::GetLoadName(void)
{
	// If we have a custom lump file for the lump this helper 
	// is loading, return it instead.
	if (IsPC() && s_MapLumpFiles[m_nLumpID].file != FILESYSTEM_INVALID_HANDLE)
	{
		return m_szLumpFilename;
	}

	return s_szLoadName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : byte
//-----------------------------------------------------------------------------
byte* CMapLoadHelper::LumpBase(void)
{
	return m_pData;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CMapLoadHelper::LumpSize()
{
	return m_nLumpSize;
}

int	CMapLoadHelper::LumpVersion() const
{
	return m_nLumpVersion;
}

int CMapLoadHelper::LumpOffset()
{
	return m_nLumpOffset;
}

//-----------------------------------------------------------------------------
// Setup a BSP loading context, maintains a ref count.	
//-----------------------------------------------------------------------------
extern CGlobalVars* gpGlobals;
void CMapLoadHelper::Init(model_t* pMapModel, const char* loadname)
{
	if (++s_nMapLoadRecursion > 1)
	{
		return;
	}

	s_pMap = NULL;
	s_szLoadName[0] = 0;
	s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
	V_memset(&s_MapHeader, 0, sizeof(s_MapHeader));
	V_memset(&s_MapLumpFiles, 0, sizeof(s_MapLumpFiles));

	if (!pMapModel)
	{
		V_strcpy_safe(s_szMapName, loadname);
	}
	else
	{
		V_strcpy_safe(s_szMapName, pMapModel->strName);
	}

	s_MapFileHandle = g_pFullFileSystem->OpenEx(s_szMapName, "rb", IsX360() ? FSOPEN_NEVERINPACK : 0, IsX360() ? "GAME" : NULL);
	if (s_MapFileHandle == FILESYSTEM_INVALID_HANDLE)
	{
		Error("CMapLoadHelper::Init, unable to open %s\n", s_szMapName);
		return;
	}

	g_pFullFileSystem->Read(&s_MapHeader, sizeof(dheader_t), s_MapFileHandle);
	if (s_MapHeader.ident != IDBSPHEADER)
	{
		g_pFullFileSystem->Close(s_MapFileHandle);
		s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
		Error("CMapLoadHelper::Init, map %s has wrong identifier\n", s_szMapName);
		return;
	}

	if (s_MapHeader.version < MINBSPVERSION || s_MapHeader.version > BSPVERSION)
	{
		g_pFullFileSystem->Close(s_MapFileHandle);
		s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
		Error("CMapLoadHelper::Init, map %s has wrong version (%i when expecting %i)\n", s_szMapName,
			s_MapHeader.version, BSPVERSION);
		return;
	}

	V_strcpy_safe(s_szLoadName, loadname);

	// Store map version, but only do it once so that the communication between the engine and Hammer isn't broken. The map version
	// is incremented whenever a Hammer to Engine session is established so resetting the global map version each time causes a problem.
	if (0 == gpGlobals->mapversion)
	{
		//g_ServerGlobalVariables.mapversion = s_MapHeader.mapRevision;
	}

#ifndef SWDS
	InitDLightGlobals(s_MapHeader.version);
#endif

	//s_pMap = &g_ModelLoader.m_worldBrushData;

#if 0
	// XXX(johns): There are security issues with this system currently. sv_pure doesn't handle unexpected/mismatched
	//             lumps, so players can create lumps for maps not using them to wallhack/etc.. Currently unused,
	//             disabling until we have time to make a proper security pass.
	if (IsPC())
	{
		// Now find and open our lump files, and create the master list of them.
		for (int iIndex = 0; iIndex < MAX_LUMPFILES; iIndex++)
		{
			lumpfileheader_t lumpHeader;
			char lumpfilename[MAX_PATH];

			GenerateLumpFileName(s_szMapName, lumpfilename, MAX_PATH, iIndex);
			if (!g_pFileSystem->FileExists(lumpfilename))
				break;

			// Open the lump file
			FileHandle_t lumpFile = g_pFileSystem->Open(lumpfilename, "rb");
			if (lumpFile == FILESYSTEM_INVALID_HANDLE)
			{
				Host_Error("CMapLoadHelper::Init, failed to load lump file %s\n", lumpfilename);
				return;
			}

			// Read the lump header
			memset(&lumpHeader, 0, sizeof(lumpHeader));
			g_pFileSystem->Read(&lumpHeader, sizeof(lumpfileheader_t), lumpFile);

			if (lumpHeader.lumpID >= 0 && lumpHeader.lumpID < HEADER_LUMPS)
			{
				// We may find multiple lump files for the same lump ID. If so,
				// close the earlier lump file, because the later one overwrites it.
				if (s_MapLumpFiles[lumpHeader.lumpID].file != FILESYSTEM_INVALID_HANDLE)
				{
					g_pFileSystem->Close(s_MapLumpFiles[lumpHeader.lumpID].file);
				}

				s_MapLumpFiles[lumpHeader.lumpID].file = lumpFile;
				s_MapLumpFiles[lumpHeader.lumpID].lumpfileindex = iIndex;
				memcpy(&(s_MapLumpFiles[lumpHeader.lumpID].header), &lumpHeader, sizeof(lumpHeader));
			}
			else
			{
				Warning("Found invalid lump file '%s'. Lump Id: %d\n", lumpfilename, lumpHeader.lumpID);
			}
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Shutdown a BSP loading context.
//-----------------------------------------------------------------------------
void CMapLoadHelper::Shutdown(void)
{
	if (--s_nMapLoadRecursion > 0)
	{
		return;
	}

	if (s_MapFileHandle != FILESYSTEM_INVALID_HANDLE)
	{
		g_pFullFileSystem->Close(s_MapFileHandle);
		s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
	}

	if (IsPC())
	{
		// Close our open lump files
		for (int i = 0; i < HEADER_LUMPS; i++)
		{
			if (s_MapLumpFiles[i].file != FILESYSTEM_INVALID_HANDLE)
			{
				g_pFullFileSystem->Close(s_MapLumpFiles[i].file);
			}
		}
		V_memset(&s_MapLumpFiles, 0, sizeof(s_MapLumpFiles));
	}

	s_szLoadName[0] = 0;
	V_memset(&s_MapHeader, 0, sizeof(s_MapHeader));
	s_pMap = NULL;

	// discard from memory
	if (s_MapBuffer.Base())
	{
		free(s_MapBuffer.Base());
		s_MapBuffer.SetExternalBuffer(NULL, 0, 0);
	}
}