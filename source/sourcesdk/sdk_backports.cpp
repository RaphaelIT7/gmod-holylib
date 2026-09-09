#include "sdk_backports.h"

#include "tier0/platform.h"
#include "tier0/basetypes.h"
#include "tier0/dbg.h"

#include "../utils/lzma/C/7zTypes.h"
#include "../utils/lzma/C/LzmaEnc.h"
#include "../utils/lzma/C/LzmaDec.h"

// Ugly define to let us forward declare the anonymous-struct-typedef that is CLzmaDec in the header.
#ifndef CLzmaDec_t
#define CLzmaDec_t CLzmaDec
#endif
#include "tier1/lzmaDecoder.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Allocator to pass to LZMA functions
static void *SzAlloc(ISzAllocPtr p, size_t size) { return malloc(size); }
static void SzFree(ISzAllocPtr p, void *address) { free(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

//-----------------------------------------------------------------------------
// Uncompress a buffer, Returns the uncompressed size. Caller must provide an
// adequate sized output buffer or memory corruption will occur.
//-----------------------------------------------------------------------------
/* static */
size_t CLZMAExtra::Uncompress( void *pInput, OUT_BYTECAP(outSize) void *pOutput, size_t outSize )
{
	auto *pHeader = static_cast<lzma_header_t *>(pInput);
	if ( pHeader->id != LZMA_ID )
	{
		// not ours
		return 0;
	}

	// These are in/out variables
	SizeT outProcessed = pHeader->actualSize;
	if ( outSize < outProcessed )
	{
		Warning( "LZMA Decompression buffer size %zu is lower than needed (%zu).\n", outSize, outProcessed );
		return 0;
	}

	CLzmaDec state;
	LzmaDec_Construct(&state);

	if ( LzmaDec_Allocate(&state, pHeader->properties, LZMA_PROPS_SIZE, &g_Alloc) != SZ_OK )
	{
		Assert( false );
		return 0;
	}

	SizeT inProcessed = pHeader->lzmaSize;
	ELzmaStatus status;
	SRes result = LzmaDecode( static_cast<unsigned char*>(pOutput), &outProcessed, static_cast<unsigned char*>(pInput) + sizeof( lzma_header_t ),
	                          &inProcessed, pHeader->properties, LZMA_PROPS_SIZE, LZMA_FINISH_END, &status, &g_Alloc );


	LzmaDec_Free(&state, &g_Alloc);

	if ( result != SZ_OK || pHeader->actualSize != outProcessed )
	{
		Warning( "LZMA Decompression failed (%i).\n", result );
		return 0;
	}

	return outProcessed;
}