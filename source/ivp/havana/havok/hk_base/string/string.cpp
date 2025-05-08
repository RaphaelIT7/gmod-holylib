#include <hk_base/base.h>
#include <cstring>

int hk_String::strcmp( const char* a, const char* b )
{
	return ::strcmp(a, b);
}

void hk_String::strcpy( char* a, const char* b )
{
	::strcpy(a, b);
}

void hk_String::memcpy( void* a, const void* b, size_t size )
{
	::memcpy(a, b, size);
}

