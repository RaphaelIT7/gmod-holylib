#include "lz4.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define LZ4_ID ( ('L' << 24) | ('Z' << 16) | ('4' << 8) | 0x00 )
struct lz4_header_t
{
	unsigned int id;
	unsigned int decompressedSize;
};
bool COM_BufferToBufferCompress_LZ4(void* dest, unsigned int* destLen, const void* source, unsigned int sourceLen, int accelerationLevel)
{
	lz4_header_t header;
	header.id = LZ4_ID;
	header.decompressedSize = sourceLen;

	memcpy(dest, &header, sizeof(lz4_header_t));

	int compressedSize = LZ4_compress_fast(
		(const char*)source,
		(char*)dest + sizeof(lz4_header_t),
		(int)sourceLen,
		(int)(*destLen - sizeof(lz4_header_t)),
		accelerationLevel
	);

	if (compressedSize <= 0)
	{
		Warning("COM_BufferToBufferCompress_LZ4: compression failed with error code: %d\n", compressedSize);
		return false;
	}

	*destLen = (unsigned int)(compressedSize + sizeof(lz4_header_t));
	return true;
}

bool COM_Compress_LZ4(const void* source, unsigned int sourceLen, void** dest, unsigned int* destLen, int accelerationLevel)
{
	if (source == nullptr || sourceLen == 0)
		return false;

	int maxCompressedSize = LZ4_compressBound(sourceLen);
	
	*dest = malloc(maxCompressedSize);
	if (!*dest)
		return false;

	*destLen = maxCompressedSize;
	bool result = COM_BufferToBufferCompress_LZ4(*dest, destLen, source, sourceLen, accelerationLevel);
	if (!result)
	{
		free(*dest);
		*dest = nullptr;
		return false;
	}

	return true;
}

unsigned int COM_GetIdealDestinationCompressionBufferSize_LZ4(unsigned int uncompressedSize)
{
	return sizeof(lz4_header_t) + LZ4_compressBound(uncompressedSize);
}

bool COM_BufferToBufferDecompress_LZ4(void* dest, unsigned int* destLen, const void* source, unsigned int sourceLen)
{
	if (sourceLen < sizeof(lz4_header_t))
	{
		Warning("COM_BufferToBufferDecompress_LZ4: source buffer too small for header\n");
		return false;
	}

	const lz4_header_t* pHeader = (const lz4_header_t*)source;
	if (pHeader->id != LZ4_ID)
	{
		Warning("COM_BufferToBufferDecompress_LZ4: invalid compression ID\n");
		return false;
	}

	unsigned int expectedSize = pHeader->decompressedSize;
	if (*destLen < expectedSize)
	{
		Warning("COM_BufferToBufferDecompress_LZ4: destination buffer too small (%u available, %u needed)\n", *destLen, expectedSize);
		return false;
	}

	int result = LZ4_decompress_safe(
		(const char*)(pHeader + 1),
		(char*)dest,
		(int)(sourceLen - sizeof(lz4_header_t)),
		(int)*destLen
	);

	if (result < 0)
	{
		Warning("COM_BufferToBufferDecompress_LZ4: LZ4 decompression failed\n");
		return false;
	}

	if ((unsigned int)result != expectedSize)
	{
		Warning("COM_BufferToBufferDecompress_LZ4: expected %u bytes, got %d bytes\n", expectedSize, result);
		return false;
	}

	*destLen = result;
	return true;
}

bool COM_Decompress_LZ4(const void* source, unsigned int sourceLen, void** dest, unsigned int* destLen)
{
	if (sourceLen < sizeof(lz4_header_t))
	{
		Warning("COM_Decompress_LZ4: source buffer too small for header\n");
		return false;
	}

	const lz4_header_t* pHeader = (const lz4_header_t*)source;
	if (pHeader->id != LZ4_ID)
	{
		Warning("COM_Decompress_LZ4: invalid compression ID\n");
		return false;
	}

	unsigned int expectedSize = pHeader->decompressedSize;
	if (*dest == nullptr)
	{
		*dest = malloc(expectedSize);
		if (*dest == nullptr)
		{
			Warning("COM_Decompress_LZ4: memory allocation failed for decompressed data\n");
			return false;
		}
		*destLen = expectedSize;
	}

	int result = LZ4_decompress_safe(
		(const char*)(pHeader + 1),
		(char*)dest,
		(int)(sourceLen - sizeof(lz4_header_t)),
		(int)*destLen
	);

	if (result < 0)
	{
		Warning("COM_Decompress_LZ4: LZ4 decompression failed\n");
		free(*dest);
		*dest = nullptr;
		return false;
	}

	if (*destLen != expectedSize)
	{
		Warning("COM_Decompress_LZ4: expected %u bytes, got %u bytes\n", expectedSize, *destLen);
		free(*dest);
		*dest = nullptr;
		return false;
	}

	return true;
}