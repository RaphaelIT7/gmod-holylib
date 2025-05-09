#ifndef HK_PS2
#	include <memory.h>
#else //HK_PS2
#	include <string.h>
#endif //HK_PS2

inline void* hk_Memory::memcpy(void* dest, const void* src, hk_size_t size)
{
	return ::memcpy(dest,src,size);
}

inline void* hk_Memory::memset(void* dest, hk_uchar val, hk_size_t size)
{
	return ::memset(dest, val, size);
}

