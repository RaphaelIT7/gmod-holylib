#define LUAJIT_ENABLE_GC64
#define LUAJIT_DISABLE_FFI
extern "C"
{
#if PLATFORM_64BITS
	#include "luajit21/lj_obj.h"
#else
	#include "luajit20/lj_obj.h"
#endif
}