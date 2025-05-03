#pragma once

extern bool COM_BufferToBufferCompress_LZ4(void* dest, unsigned int* destLen, const void* source, unsigned int sourceLen, int accelerationLevel = 1);
extern unsigned int COM_GetIdealDestinationCompressionBufferSize_LZ4(unsigned int uncompressedSize);
extern bool COM_BufferToBufferDecompress_LZ4(void* dest, unsigned int* destLen, const void* source, unsigned int sourceLen);

/*
 * Compresses the source and returns the dest and dest length.
 * It fully handles the allocation of the dest, you provide a NULL pointer and it'll do the rest.
 * It's expected that YOU free the data after your done with it.
 * Use "free" and NOT "delete" when your done with it.
 */
extern bool COM_Compress_LZ4(const void* source, unsigned int sourceLen, void** dest, unsigned int* destLen, int accelerationLevel = 1);

/*
 * Decompresses the source and returns the dest and dest length.
 * It fully handles the allocation of the dest, you provide a NULL pointer and it'll do the rest.
 * It's expected that YOU free the data after your done with it.
 * Use "free" and NOT "delete" when your done with it.
 */
extern bool COM_Decompress_LZ4(const void* source, unsigned int sourceLen, void** dest, unsigned int* destLen);