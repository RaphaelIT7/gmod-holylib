#pragma once

#include <Platform.hpp>
#include "tier0/annotations.h"
#include "tier1/strtools.h"
#include "tier1/lzmaDecoder.h"
#include <cctype>
#include <limits>
// RaphaelIT7: Needed for std::size?
#include <vector>

// RaphaelIT7:
// These are some backported functions from
// https://github.com/Source-Authors/Obsoletium/blob/cf1465256a12f74569ea3e6f4e34daefd9c26c51/public/tier1/strtools.h#L225
// https://github.com/Source-Authors/Obsoletium/blob/cf1465256a12f74569ea3e6f4e34daefd9c26c51/public/tier0/commonmacros.h#L571
// https://github.com/Source-Authors/Obsoletium/blob/cf1465256a12f74569ea3e6f4e34daefd9c26c51/public/tier0/platform.h#L144
// https://github.com/Source-Authors/Obsoletium/blob/cf1465256a12f74569ea3e6f4e34daefd9c26c51/public/tier0/platform.h#L1565

#ifdef _MSC_VER
#define IN_OPT _In_opt_
#define IN_OPT_Z _In_opt_z_
#define IN_Z_ARRAY _Pre_z_
#else
#define IN_OPT
#define IN_OPT_Z
#define IN_Z_ARRAY
#define _In_
#define _In_opt_
#define _In_z_
#define _Out_
#define _Out_opt_
#define _Out_writes_(x)
#define _Out_writes_z_(x)
#define _Printf_format_string_
#define _Scanf_s_format_string_
#endif

[[nodiscard]] inline char V_tolower(char ch)
{
	constexpr auto zMinusA = static_cast<unsigned char>('Z' - 'A');
	constexpr auto aMinusA = static_cast<unsigned char>('a' - 'A');

	const auto uch = static_cast<unsigned char>(ch);

	if (static_cast<unsigned char>(uch - static_cast<unsigned char>('A')) <=
		zMinusA)
		return static_cast<char>(uch + aMinusA);

	if (uch >= 0x80)  // non-ASCII, fall back to CRT
		return static_cast<char>(std::tolower(uch));

	return ch;
}

[[nodiscard]] inline bool V_streq(IN_Z const char *l, IN_Z const char *r) {
  return V_strcmp(l, r) == 0;
}

[[nodiscard]] inline bool V_strieq(IN_Z const char *l, IN_Z const char *r) {
  return V_stricmp(l, r) == 0;
}

// dimhotepus: Define single API for path separators.
#ifdef _WIN32
[[nodiscard]] constexpr inline bool PATHSEPARATOR( char c )
{
	return c == '\\' || c == '/';
}
#elif defined(POSIX)
[[nodiscard]] constexpr inline bool PATHSEPARATOR( char c )
{
	return c == '/';
}
#endif	//_WIN32

// dimhotepus: Add type-safe interface.
#define stackallocT( type_, _size )		static_cast<type_*>( stackalloc( sizeof(type_) * (_size) ) )

// Given a path and a filename, composes "path\filename", inserting the (OS correct) separator if necessary
template<intp destSize>
void V_ComposeFileName( IN_Z const char *path, IN_Z const char *filename, OUT_Z_ARRAY char (&dest)[destSize] )
{
	V_ComposeFileName( path, filename, dest, destSize );
}

/**
 * @brief Type-safe memory set.
 * @tparam T Type.
 * @param src Source to clear.
 * @param byte Byte to fill src.
 * @return void.
 */
template <typename T>
std::enable_if_t<std::is_trivially_copyable_v<T> &&
                 std::is_trivially_constructible_v<T> && !std::is_pointer_v<T>>
BitwiseSet(T& src, unsigned char setbyte) noexcept {
  memset(&src, setbyte, sizeof(T));
}

/**
 * @brief Type-safe memory clear.
 * @tparam T Type.
 * @param src Source to clear.
 * @return void.
 */
template <typename T>
std::enable_if_t<std::is_trivially_copyable_v<T> &&
                 std::is_trivially_constructible_v<T> && !std::is_pointer_v<T>>
BitwiseClear(T& src) noexcept {
  BitwiseSet(src, 0);
}

/**
 * @brief Type-safe memory set for array.
 * @tparam T Type of array element.
 * @tparam size Array size.
 * @param src Source to set.
 * @param byte Byte to fill src.
 * @return void.
 */
template <typename T, size_t size>
std::enable_if_t<std::is_trivially_copyable_v<T> &&
                 std::is_trivially_constructible_v<T>>
BitwiseSet(T (&src)[size], unsigned char setbyte) noexcept {
  memset(src, setbyte, sizeof(src));
}

/**
 * @brief Type-safe memory clear for array.
 * @tparam T Type of array element.
 * @tparam size Array size.
 * @param src Source to clear.
 * @return void.
 */
template <typename T, size_t size>
std::enable_if_t<std::is_trivially_copyable_v<T> &&
                 std::is_trivially_constructible_v<T>>
BitwiseClear(T (&src)[size]) noexcept {
  BitwiseSet(src, 0);
}

/**
 * @brief Type-safe memory clear.  Note src size should be >= size.
 * @tparam T Type.
 * @param src Source.
 * @param size Source size to clear.
 * @return void.
 */
template <typename T>
std::enable_if_t<std::is_trivially_copyable_v<T> &&
                 std::is_trivially_constructible_v<T>>
BitwiseClear(T* src, size_t size) noexcept {
  assert(sizeof(*src) >= size);
  memset(src, 0, size);
}

[[nodiscard]] inline bool V_isempty( IN_OPT_Z const char* pszString ) { return !pszString || !pszString[ 0 ]; }
// dimhotepus: Add wchar_t version.
[[nodiscard]] inline bool V_isempty( IN_OPT_Z const wchar_t* pszString ) { return !pszString || !pszString[ 0 ]; }

[[nodiscard]] inline bool Q_isempty(IN_OPT_Z const char *v) { return V_isempty(v); }

[[nodiscard]] inline bool Q_isempty(IN_OPT_Z const wchar_t *v) { return V_isempty(v); }

template<size_t size>
[[nodiscard]] constexpr inline bool Q_isempty(IN_Z_ARRAY char (&v)[size]) { return v[0] == '\0'; }

template<size_t size>
[[nodiscard]] constexpr inline bool Q_isempty(IN_Z_ARRAY wchar_t (&v)[size]) { return v[0] == L'\0'; }

// dimhotepus: Correctly work with signed/unsigned char.
[[nodiscard]] inline char V_toupper(char ch)
{
	constexpr auto zMinusA = static_cast<unsigned char>('z' - 'a');
	constexpr auto aMinusA = static_cast<unsigned char>('a' - 'A');

	const auto uch = static_cast<unsigned char>(ch);

	if (static_cast<unsigned char>(uch - static_cast<unsigned char>('a')) <=
		zMinusA)
		return static_cast<char>(uch - aMinusA);

	if (uch >= 0x80)  // non-ASCII, fall back to CRT
		return static_cast<char>(std::toupper(uch));

	return ch;
}

// If pPath is a relative path, this function makes it into an absolute path
// using the current working directory as the base, or pStartingDir if it's non-nullptr.
// Returns false if it runs out of room in the string, or if pPath tries to ".." past the root directory.
template<int outSize>
void V_MakeAbsolutePath( OUT_Z_ARRAY char (&pOut)[outSize], IN_Z const char *pPath, IN_OPT_Z const char *pStartingDir = nullptr )
{
	V_MakeAbsolutePath( pOut, outSize, pPath, pStartingDir );
}

inline void V_MakeAbsolutePath( OUT_Z_CAP(outLen) char *pOut, int outLen, IN_Z const char *pPath, IN_OPT_Z const char *pStartingDir, bool bLowercaseName )
{
	V_MakeAbsolutePath( pOut, outLen, pPath, pStartingDir );
	if ( bLowercaseName )
	{
		V_strlower( pOut );
	}
}

template<int outSize>
inline void V_MakeAbsolutePath( OUT_Z_ARRAY char (&pOut)[outSize], IN_Z const char *pPath, IN_OPT_Z const char *pStartingDir, bool bLowercaseName )
{
	V_MakeAbsolutePath( pOut, outSize, pPath, pStartingDir, bLowercaseName );
}

// Extracts the base name of a file (no path, no extension, assumes '/' or '\' as path separator)
template<intp outSize>
void V_FileBase( IN_Z const char *in, OUT_Z_ARRAY char (&out)[outSize] )
{
	V_FileBase( in, out, outSize );
}

template<intp destSize>
void V_ExtractFileExtension( IN_Z const char *path, OUT_Z_ARRAY char (&dest)[destSize] )
{
	V_ExtractFileExtension( path, dest, destSize );
}

// Copy out the path except for the stuff after the final pathseparator
template<intp destSize>
bool V_ExtractFilePath( IN_Z const char *path, OUT_Z_ARRAY char (&dest)[destSize] )
{
	return V_ExtractFilePath( path, dest, destSize );
}

// char buffer[ 9 ];
// V_binarytohex( &output, sizeof( output ), buffer );
// would put "ffffffff" into buffer (note null terminator!!!)
template<intp outSize>
void V_binarytohex( IN_BYTECAP(inputbytes) const byte *in, intp inputbytes, OUT_Z_ARRAY char (&out)[outSize] )
{
	V_binarytohex( in, inputbytes, out, outSize );
}
// char buffer[ 9 ];
// V_binarytohex( output, buffer );
// would put "ffffffff" into buffer (note null terminator!!!)
template<intp inSize, intp outSize>
void V_binarytohex( const byte (&in)[inSize], OUT_Z_ARRAY char (&out)[outSize] )
{
	V_binarytohex( in, inSize, out, outSize );
}
// char buffer[ 9 ];
// V_binarytohex( output, buffer );
// would put "ffffffff" into buffer (note null terminator!!!)
template<typename T, intp outSize>
std::enable_if_t<!std::is_pointer_v<T>>	V_binarytohex( const T &in, OUT_Z_ARRAY char (&out)[outSize] )
{
	V_binarytohex( reinterpret_cast<const byte*>(&in), static_cast<intp>(sizeof(in)), out, outSize );
}

// dimhotepus: TF2 backport.
#ifdef PLATFORM_WINDOWS_PC

# ifdef PLATFORM_64BITS
#  define PLATFORM_DIR "\\x64"
# else
#  define PLATFORM_DIR ""
# endif

//#elif PLATFORM_LINUX
#elif LINUX

# ifdef PLATFORM_64BITS
#  define PLATFORM_DIR "/linux64"
# else
#  define PLATFORM_DIR ""
# endif

//#elif PLATFORM_OSX
#elif OSX

#if PLATFORM_ARM
#  define PLATFORM_DIR "/osxarm64"
#else
# ifdef PLATFORM_64BITS
#  define PLATFORM_DIR "/osx64"
# else
#  define PLATFORM_DIR ""
# endif
#endif

#else
# error "Define a platform dir for me!"
#endif

#define PLATFORM_BIN_DIR "bin" PLATFORM_DIR

// dimhotepus: TF2 backport.
// misyl: Shamelessly nicked from Source 2 =)
//
//--------------------------------------------------------------------------------------------------
// RunCodeAtScopeExit
//
// Example:
//	int *x = new int;
//	RunCodeAtScopeExit( delete x )
//--------------------------------------------------------------------------------------------------
template <typename LambdaType>
class CScopeGuardLambdaImpl
{
public:
	explicit CScopeGuardLambdaImpl( LambdaType&& lambda ) : m_lambda( std::move( lambda ) ) { }
	~CScopeGuardLambdaImpl() { m_lambda(); }
private:
	LambdaType m_lambda;
};

//--------------------------------------------------------------------------------------------------
// RunCodeAtScopeExitOpt
//
// Example:
//	if ( flag ) Lock();
//	RunCodeAtScopeExitOpt( flag, Unlock() )
//--------------------------------------------------------------------------------------------------
template <typename LambdaType>
class CScopeGuardLambdaImplOpt
{
public:
	CScopeGuardLambdaImplOpt( bool do_invoke, LambdaType&& lambda )
		: m_lambda( std::move( lambda ) ), m_do_invoke( do_invoke ) { }
	~CScopeGuardLambdaImplOpt() { if ( m_do_invoke ) { m_lambda(); } }
private:
	LambdaType m_lambda;
	const bool m_do_invoke;
};

//--------------------------------------------------------------------------------------------------
template <typename LambdaType>
CScopeGuardLambdaImpl< LambdaType > MakeScopeGuardLambda( LambdaType&& lambda )
{
	return CScopeGuardLambdaImpl< LambdaType >( std::move( lambda ) );
}

template <typename LambdaType>
CScopeGuardLambdaImplOpt< LambdaType > MakeScopeGuardLambdaOpt( bool do_invoke, LambdaType&& lambda )
{
	return CScopeGuardLambdaImplOpt< LambdaType >( do_invoke, std::move( lambda ) );
}

//--------------------------------------------------------------------------------------------------
#define RunLambdaAtScopeExit2( VarName, ... )		[[maybe_unused]] const auto VarName( MakeScopeGuardLambda( __VA_ARGS__ ) )
#define RunLambdaAtScopeExit( ... )					RunLambdaAtScopeExit2( UNIQUE_ID, __VA_ARGS__ )
#define RunCodeAtScopeExit( ... )					RunLambdaAtScopeExit( [&]() { __VA_ARGS__ ; } )

#define RunLambdaAtScopeExit2Opt( VarName, do_invoke, ... )		[[maybe_unused]] const auto VarName( MakeScopeGuardLambdaOpt( do_invoke, __VA_ARGS__ ) )
#define RunLambdaAtScopeExitOpt( do_invoke, ... )		RunLambdaAtScopeExit2Opt( UNIQUE_ID, do_invoke, __VA_ARGS__ )
#define RunCodeAtScopeExitOpt( do_invoke, ... )			RunLambdaAtScopeExitOpt( do_invoke, [&]() { __VA_ARGS__ ; } )

class CLZMAExtra : public CLZMA
{
public:
	static size_t Uncompress( void *pInput, OUT_BYTECAP(outSize) void *pOutput, size_t outSize );
};