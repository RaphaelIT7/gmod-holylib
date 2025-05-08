// Copyright (C) Ipion Software GmbH 1999-2000. All rights reserved.

//IVP_EXPORT_PUBLIC

#ifndef _IVP_U_TYPES_INCLUDED
#define _IVP_U_TYPES_INCLUDED

#ifdef _LINUX
	#include <signal.h>
#endif

#include <cstddef>  // std::nullptr_t

#ifdef WIN32
#define IVP_PIII			/* set for P3 specific code */
//#define IVP_WILLAMETTE	/* set for Willamette specific code */
//#define IVP_WMT_ALIGN		/* set to compile with MS but Willamette compatible */
//#define IVP_PSXII			/* playstation II */
#endif
//#define IVP_NO_DOUBLE	/* set if processor has no double floating point unit, or lib should use float only */
#define IVP_VECTOR_UNIT_FLOAT  /* set if extra data is inserted to support vector units */
#define IVP_VECTOR_UNIT_DOUBLE /* set if extra data should be insersted to utilize double vector units */

#if defined(PSXII)
#define IVP_USE_PS2_VU0
#endif

#ifdef IVP_WILLAMETTE
#define IVP_IF_WILLAMETTE_OPT(cond) if(cond)
#else
#define IVP_IF_WILLAMETTE_OPT(cond) if(0)
#endif

#if  defined(IVP_WILLAMETTE) || defined(IVP_PIII) || defined(IVP_WMT_ALIGN)
#define IVP_WINDOWS_ALIGN16
#endif

// recheck the settings for various computers
#if !defined(IVP_NO_DOUBLE) && (defined(PSXII) || defined(GEKKO) || defined(_XBOX))
#define IVP_NO_DOUBLE
#endif

#if !defined(IVP_VECTOR_UNIT_DOUBLE) && defined(PSXII)
#	define IVP_VECTOR_UNIT_FLOAT
#	define IVP_VECTOR_UNIT_DOUBLE
#endif
						 
// typedefs for our special types

#if defined(NDEBUG) + defined(DEBUG) != 1
	   Exactly DEBUG or NDEBUG has to be defined, check Makefile
#endif

#ifndef WIN32
#define CORE raise(SIGINT)  /* send fatal signal */
#else
#define CORE __debugbreak()
#endif

#if defined (PSXII)
#	if defined (__MWERKS__)
inline void BREAKPOINT()
{
	__asm__ __volatile__(" \
		breakc 0x0 \
		nop \
	");
}
#	else // __MWERKS__
inline void BREAKPOINT()
{
	__asm__ __volatile__(" \
		break 0x0 \
		nop \
	");
}
#	endif // __MWERKS__
#endif // PSXII

#ifdef	NDEBUG
#	define IVP_ASSERT(cond)	
#	define IVP_USE(a) 
#	define IVP_IF(flag)	if (0==1)
#else
#	if defined (PSXII)
#		define IVP_ASSERT(cond) \
		{ \
			if(!(cond)) \
			{ \
				::fprintf(stderr, "\nASSERTION FAILURE: %s\nFILE: %s\nLINE: %d\n\n", cond, __FILE__, __LINE__); \
				BREAKPOINT(); \
			} \
		}
#	elif defined(GEKKO)
#		define IVP_ASSERT(cond) \
		{ \
			if(!(cond)) \
			{ \
				::fprintf(stderr, "\nASSERTION FAILURE: %s\nFILE: %s\nLINE: %d\n\n", cond, __FILE__, __LINE__); \
				CORE; \
			} \
		}
#	elif defined(__MWERKS__) && defined(__POWERPC__)
#		include <MacTypes.h>
#		define IVP_ASSERT(cond) \
		{ \
			if(!(cond)) \
			{ \
						char error[128]; \
						snprintf(error, std::size(error), (char*)"\pASSERT FAILURE: \nFILE: %s\nLINE: %d\n\n", __FILE__, __LINE__);  \
		 			DebugStr((unsigned char *)error); \
			} \
		}
#	elif defined (_LINUX)
#		define IVP_ASSERT(cond) \
		do { \
			if(!(cond)) \
			{ \
				::fprintf(stderr, "\nASSERTION FAILURE: %s\nFILE: %s\nLINE: %d\n\n", ""#cond, __FILE__, __LINE__); \
				raise(SIGINT); \
			} \
		} while (false)
#	else
#		define IVP_ASSERT(cond) \
		do { \
			if(!(cond)) \
			{ \
				::fprintf(stderr, "\nASSERTION FAILURE: %s\nFILE: %s\nLINE: %d\n\n", ""#cond, __FILE__, __LINE__); \
				CORE; \
			} \
		} while (false)
//#		define IVP_ASSERT(cond)	if (!(cond)) CORE
#	endif // PSXII, Mac, etc.  debug assert
#	define IVP_USE(a) a=a
#	define IVP_IF(flag)	if (flag)
#endif


using IVP_FLOAT = float;
using IVP_INT32 = int;
using IVP_UINT32 = unsigned int;

// ************************
#ifndef IVP_NO_DOUBLE
using IVP_DOUBLE = double;
class IVP_Time {
	double seconds;
public:
	void operator+=(double val){
	seconds += val;
	}
	double get_seconds() const { return seconds; }
	double get_time() const { return seconds; } // for debugging
	double operator-(const IVP_Time &b) const { return this->seconds - b.seconds; }
	void operator-=(const IVP_Time b) { this->seconds -= b.seconds; }
	IVP_Time operator+(double val) const { IVP_Time result; result.seconds = this->seconds + val; return result;}

	IVP_Time() = default;
	IVP_Time(double time){ seconds = time; }
};

#else
// ************************
using IVP_DOUBLE = float;
class IVP_Time {
	float seconds;
	float sub_seconds;
public:
	void operator+=(float val){
	sub_seconds += val;
	while (sub_seconds > 1.0f){	seconds ++;	sub_seconds -= 1.0f;	}
	}
	float get_seconds() const { return seconds; };
	float get_time() const{ return seconds + sub_seconds; }; // for debugging
	float operator-(const IVP_Time &b) const { return (this->seconds - b.seconds) + this->sub_seconds - b.sub_seconds; }
	void operator-=(const IVP_Time b) { this->seconds -= b.seconds; this->sub_seconds -= b.sub_seconds;
		while (sub_seconds > 1.0f){	seconds ++;	sub_seconds -= 1.0f;	}
		while (sub_seconds < 0.0f){	seconds --;	sub_seconds += 1.0f;	}
	}
	IVP_Time operator+(float val) const {
	IVP_Time result; result.seconds = this->seconds; result.sub_seconds = this->sub_seconds + val;
	while (result.sub_seconds > 1.0f){	result.seconds ++;	result.sub_seconds -= 1.0f;	}
	return result;
	}
	IVP_Time(){}
	IVP_Time(float time){ seconds = float(int(time)); sub_seconds = time - int(time); };
};
#endif
 
using IVP_HTIME = IVP_FLOAT;

using uchar = unsigned char; // feel free to remove these three typedefs
using ushort = unsigned short;
using uint = unsigned int;

using intp = ptrdiff_t;
using uintp = size_t;

using IVP_ERROR_STRING = const char *;
constexpr inline std::nullptr_t IVP_NO_ERROR{nullptr};

#if defined(PSXII) || defined(LINUX) || defined(GEKKO)
//#   define IVP_ALIGN_16  __attribute__ ((aligned(16)))
#endif

#if !defined(IVP_ALIGN_16)
#   define IVP_ALIGN_16 
#endif


/********************************************************************************
 *						Simple Base Types
 ********************************************************************************/
  
using IVP_Time_CODE = int;	// Internal enumeration for time handles.
using ivp_u_bool = int;		// must be int!!

// boolean enumeration, compatible to C++ bool, ivp_u_bool
enum IVP_BOOL {
	IVP_FALSE = 0,
	IVP_TRUE = 1
};


enum IVP_RETURN_TYPE {
	IVP_FAULT = 0,
	IVP_OK = 1
};

#define IVP_CDECL	   /* set this to whatever you need to satisfy your linker */

#if !defined(__MWERKS__) || !defined(__POWERPC__)
#   ifdef OSX
#	   include <malloc/malloc.h>
#   else
#	   include <malloc.h>
#   endif
#endif

#include <cstring>


	char * IVP_CDECL p_calloc(size_t nelem, size_t size);
	void * IVP_CDECL p_realloc(void* memblock, size_t size);
	void * IVP_CDECL p_malloc(size_t size);
	void   IVP_CDECL p_free(void *data);
	
	void * IVP_CDECL ivp_malloc_aligned(size_t size, unsigned short alignment);
	void * IVP_CDECL ivp_calloc_aligned(size_t size, unsigned short alignment);
	
	void  IVP_CDECL ivp_free_aligned(void *data);

	//void  IVP_CDECL ivp_byte_double(double& eightbytes);
	void  IVP_CDECL ivp_byte_swap4(uint& fourbytes);
	void  IVP_CDECL ivp_byte_swap2(ushort& twobytes);

#ifdef GEKKO

	int	strcasecmp(const char *,const char *);
#endif


	char *p_strdup(const char *s);

extern void ivp_memory_check(void *a);

#define P_DELETE(a) do { /*ivp_memory_check((void*)a);*/ delete(a); a=NULL; } while (false)
#define P_DELETE_THIS(a) do { /*ivp_memory_check((void*)a);*/ delete(a); } while (false)
#define P_DELETE_ARRAY(a) do { /*ivp_memory_check((void*)a);*/ delete[](a); (a)= 0; } while (false)

#define P_FREE_ALIGNED(a) do { if (a) { /*ivp_memory_check((void*)a);*/ ivp_free_aligned( (void *)a ); a = NULL; } } while (false)
#define P_FREE(a) do { if (a) { /*ivp_memory_check((void*)a);*/ p_free( (char *) a); a = NULL; } } while (false)

#define P_MEM_CLEAR(a) memset((char*)(a), 0, sizeof(*a))
#define P_MEM_CLEAR_M4(a) memset((char*)(a)+sizeof(void*), 0, sizeof(*a)-sizeof(void *))
#define P_MEM_CLEAR_ARRAY(clss,elems) memset((char*)(clss), 0, sizeof(*clss)*(elems))

constexpr inline float P_FLOAT_EPS{1e-10f};  // used for division checking
constexpr inline float P_FLOAT_RES{1e-6f};	// float resolution for numbers < 1.0
constexpr inline float P_FLOAT_MAX{1e16f};

#ifdef IVP_NO_DOUBLE
#	define IVP_PI		3.14159265358979323846f	/* pi */
#	define IVP_PI_2	  1.57079632679489661923f	/* pi/2 */
#	define P_DOUBLE_MAX P_FLOAT_MAX
#	define P_DOUBLE_RES P_FLOAT_RES	// double resolution for numbers < 1.0
#	define IVP_3D_SOLVER_NULLSTELLE_EPS 3e-3f
#	define P_DOUBLE_EPS P_FLOAT_EPS	// used for division checking
#	define P_MAX_WORLD_DOUBLE 3000.0f // max world koords
#else
constexpr inline double IVP_PI{3.14159265358979323846};	/* pi */
constexpr inline double IVP_PI_2{1.57079632679489661923}; /* pi/2 */
constexpr inline double P_DOUBLE_MAX{1e20};
constexpr inline double P_DOUBLE_RES{1E-12};	// double resolution for numbers < 1.0
constexpr inline double IVP_3D_SOLVER_NULLSTELLE_EPS{1e-8};
constexpr inline double P_DOUBLE_EPS { 1e-10};	// used for division checking
constexpr inline double P_MAX_WORLD_DOUBLE{10000};  // max world koords
#endif

constexpr inline float P_MAX_OBJECT_SIZE{1000.0f}; 
constexpr inline float P_MIN_EDGE_LEN{0.01f};	// 10 mm min edge len of polygon objects
constexpr inline float P_RES_EPS{/*P_MAX_WORLD_DOUBLE*/ 10000.0f * P_FLOAT_RES}; // effective IVP_DOUBLE resolution for world coords

void ivp_srand(int seed);
IVP_FLOAT ivp_rand();		// returns [0 .. 1]

#if defined(PSXII)
#	define IVP_NO_ALLOCA
#endif

#if defined(WIN32)
#   if defined(IVP_PIII) || defined(IVP_WILLAMETTE)
#   	if defined(IVP_PIII)
#		define IVP_PREFETCH_CLINE_SIZE 0x20
#	else
#		define IVP_PREFETCH_CLINE_SIZE 0x40
#	endif
#	define IVP_IF_PREFETCH_ENABLED(x) if(x)
#	include <xmmintrin.h>
#	define IVP_PREFETCH( pntr, offset) _mm_prefetch( intp(offset) + (char *)pntr, _MM_HINT_T1)
#   endif


#elif defined(PSXII) && 0
#	define IVP_PREFETCH_CLINE_SIZE 0x40
#	define IVP_IF_PREFETCH_ENABLED(x) if(x)
#	define	IVP_PREFETCH(__addr,__offs)  ({asm volatile("pref 0,%1(%0)" : : "r"(__addr),"i"(__offs));})
#endif


#if !defined(IVP_PREFETCH)
#	define IVP_PREFETCH_CLINE_SIZE 0x100
#	define IVP_PREFETCH(pntr, offset)
#	ifdef DEBUG
#		define IVP_IF_PREFETCH_ENABLED(x) if(1)
#	else
#		define IVP_IF_PREFETCH_ENABLED(x) if(0)
#	endif
#	define IVP_PREFETCH_BLOCK(a,b)  /* IVP_ASSERT(b < 128);*/
#else

#   define IVP_PREFETCH_BLOCK(pntr, size) do {	\
	IVP_PREFETCH(pntr,0);	\
	if ( size > IVP_PREFETCH_CLINE_SIZE)   IVP_PREFETCH( pntr, IVP_PREFETCH_CLINE_SIZE);   \
	if ( size > 2*IVP_PREFETCH_CLINE_SIZE) IVP_PREFETCH( pntr, 2*IVP_PREFETCH_CLINE_SIZE); \
	if ( size > 3*IVP_PREFETCH_CLINE_SIZE) IVP_PREFETCH( pntr, 3*IVP_PREFETCH_CLINE_SIZE); \
	/*if ( size > 4*IVP_PREFETCH_CLINE_SIZE) IVP_PREFETCH( pntr, 4*IVP_PREFETCH_CLINE_SIZE);*/ \
	/*IVP_PREFETCH( pntr, (size - sizeof(void *))) */\
} while (false)

#endif

#ifndef IVP_FAST_WHEELS_ENABLED
#define IVP_FAST_WHEELS_ENABLED
#endif // IVP_FAST_WHEELS_ENABLED

// define this if you have the havana constraints module
#ifndef HAVANA_CONSTRAINTS
//#define HAVANA_CONSTRAINTS
#endif // HAVANA_CONSTRAINTS

// define this if you have the havok MOPP module
#ifndef HAVOK_MOPP
//#define HAVOK_MOPP
#endif // HAVOK_MOPP
			   
#endif


