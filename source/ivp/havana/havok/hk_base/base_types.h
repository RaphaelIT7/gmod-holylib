#ifndef HK_BASE_BASE_TYPES_H
#define HK_BASE_BASE_TYPES_H

#ifdef _LINUX
#include <signal.h>
#endif

#ifndef _LINUX
#include <DirectXMath.h>
#endif

// dimhotepus: Use SSE when DirectXMath uses SSE.
#ifdef _XM_SSE_INTRINSICS_
#define HK_PIII_SSE
#endif

// dimhotepus: Use ARM NEON when DirectXMath uses ARM NEON.
#ifdef _XM_ARM_NEON_INTRINSICS_
#define HK_ARM_NEON
#endif

// some of these are wrong
#if defined(_DEBUG) && !defined(HK_DEBUG)
#	define HK_DEBUG
#endif

#ifndef HK_ASSERT
# ifdef HK_DEBUG
#  define HK_ASSERT(a) do { if(!(a)) hk_assert(a,#a,__LINE__,__FILE__); } while (false)
#  define HK_IF_DEBUG(a) if(a)
# else
#  define HK_ASSERT(a) 
#  define HK_IF_DEBUG(a) if(0)
# endif
#endif

#if defined(HK_DEBUG)
#	define HK_IF_CHECK(a) if (a)
#	define HK_CHECK(a) do { if(!(a)) hk_check(a,#a,__LINE__,__FILE__); } while (false)
#else
#	define HK_IF_CHECK(a) if (0)
#	define HK_CHECK(a) 
#endif

#define HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(a,b)

#ifndef WIN32
#define HK_BREAKPOINT() raise(SIGINT)
#else
#define HK_BREAKPOINT() __debugbreak()
#endif


class hkBaseObject
{
public:
	int m_memsize;
	virtual ~hkBaseObject(){}
};




// simple commonly used types
using hk_real = float;
using hk_double = double;

using hk_client = void*;

using hk_char = signed char;
using hk_int16 = signed short;
using hk_int32 = signed int;
using hk_int64 = signed long long int;

using hk_uchar = unsigned char;
using hk_uint16 = unsigned short;
using hk_uint32 = unsigned int;
using hk_uint64 = unsigned long long int;

#include <cstddef>
using hk_size_t = size_t;  // CK: unsigned long int ..

#ifdef _LINUX
#define HK_BREAK raise(SIGINT)
#else
#define HK_BREAK __debugbreak()
#endif

#define HK_PUBLIC public

#if defined(__GNUC__)
#	define HK_HAVE_PRAGMA_INTERFACE
#	define HK_HAVE_GNU_INLINE_ASSEMBLY
#elif defined(WIN32)
#	define HK_HAVE_FORCE_INLINE
#	define HK_HAVE_MSVC_INLINE_ASSEMBLY
#endif


constexpr inline std::nullptr_t HK_NULL{nullptr};

//: Note that M must be a power of two for this to work.
template<typename M, typename P>
constexpr auto HK_NEXT_MULTIPLE_OF(M m, P p) {
	M mm{m - 1};
	return (p + mm) & ~mm;
}

void hk_assert(bool test, const char* cond, int line, const char* file);
void hk_check (bool test, const char* cond, int line, const char* file);

// return values for just simple functions
enum hk_result
{
	HK_OK,
	HK_FAULT
};

using hk_bool = bool;
constexpr inline bool HK_FALSE{false};
constexpr inline bool HK_TRUE{true};

using hk_time = hk_real;
using hk_sorted_set_store_index = hk_uint16;

using hk_sorted_set_index = hk_int32;
using hk_array_index = hk_int32;
using hk_array_store_index = hk_uint16;
using hk_id = hk_uint32;

// TODO(crack): disallow to redefine keywords
#ifdef HK_HAVE_FORCE_INLINE
//#	define inline __forceinline
#endif
#define HK_TEMPLATE_INLINE inline

#if (defined(__i386__) || defined(WIN32)) && !defined(IVP_NO_PERFORMANCE_TIMER)
#	define HK_HAVE_QUERY_PERFORMANCE_TIMER
#endif

#if !defined(HK_ALIGNED_VARIABLE)
#	if defined(HK_PS2)
#		define HK_ALIGNED_VARIABLE(NAME,ALIGNMENT) NAME __attribute__((aligned(ALIGNMENT)))
#	elif defined(HK_PIII_SSE)
#		define HK_ALIGNED_VARIABLE(NAME,ALIGNMENT) alignas(ALIGNMENT) NAME
#	else //no special alignment
#		define HK_ALIGNED_VARIABLE(NAME,ALIGNMENT) NAME
#	endif 
#endif //HK_ALIGNED_VARIABLE

#endif //HK_BASE_BASE_TYPES_H

