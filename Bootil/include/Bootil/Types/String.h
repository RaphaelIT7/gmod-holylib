#pragma once

//
// Original function alters passed string - we define a Get# version of that
// function which returns the result instead - leaving the original string untouched.
//
#define BSTRGET( _name_ ) \
BOOTIL_EXPORT void _name_( Bootil::BString& str );\
inline BString Get##_name_( BString str ) { _name_( str ); return str; }

namespace Bootil
{
	typedef std::string BString;
	typedef std::wstring WString;

	namespace String
	{
		typedef std::vector<BString> List;
		typedef std::map<BString, BString> Map;
		typedef std::list<BString> Lines;

		namespace Convert
		{
			BOOTIL_EXPORT BString FromWide( const WString & strIn );
			BOOTIL_EXPORT WString ToWide( const BString & strIn );
		}

		template <typename T>
		BString ToString( T var )
		{
			std::ostringstream os;
			os << var;
			return os.str();
		}

        BOOTIL_EXPORT void ToCharBuffer( const BString & str, char* pBuffer, int len );

		BOOTIL_EXPORT BString Random( int iSize, bool bLetters = true, bool bCapitals = false, bool bNumbers = false, bool Specials = false );

		BOOTIL_EXPORT void SortList( String::List & list, bool bReverse );

		BSTRGET( Lower );
		BSTRGET( Upper );

		namespace Util
		{
			BOOTIL_EXPORT void Trim( BString & str, const BString & strChars = " \n\t\r" );
			inline BString GetTrim( BString str, const BString & strChars ) { Trim( str, strChars ); return str; }

			BOOTIL_EXPORT void TrimLeft( BString & str, const BString & strChars );
			inline BString GetTrimLeft( BString str, const BString & strChars ) { TrimLeft( str, strChars ); return str; }

			BOOTIL_EXPORT void TrimRight( BString & str, const BString & strChars );
			inline BString GetTrimRight( BString str, const BString & strChars ) { TrimRight( str, strChars ); return str; }

			BOOTIL_EXPORT void TrimBefore( Bootil::BString & str, const Bootil::BString & strFind, bool bIncluding = false );
			inline BString GetTrimBefore( BString str, const Bootil::BString & strFind, bool bIncluding = false ) { TrimBefore( str, strFind, bIncluding ); return str; }

			BOOTIL_EXPORT void TrimAfter( Bootil::BString & str, const Bootil::BString & strFind, bool bIncluding = false );
			inline BString GetTrimAfter( BString str, const Bootil::BString & strFind, bool bIncluding = false ) { TrimAfter( str, strFind, bIncluding ); return str; }

			BOOTIL_EXPORT void Split( const BString & str, const BString & seperator, String::List & outbits );
			BOOTIL_EXPORT void SplitLength( const BString & str, int iLength, String::List & outbits );
			BOOTIL_EXPORT int Count( const BString & str, char chrFind );

            BOOTIL_EXPORT void FindAndReplace( BString & strIn, const BString & strFind, const BString & strReplace );
            inline BString GetFindAndReplace( const BString & strIn, const BString & strFind, const BString & strReplace ) { BString strOut = strIn; FindAndReplace( strOut, strFind, strReplace ); return strOut; }
		}

		namespace Test
		{
			BOOTIL_EXPORT bool StartsWith( const BString & str, const BString & strFind );
			BOOTIL_EXPORT bool EndsWith( const BString & str, const BString & strFind );
			BOOTIL_EXPORT bool ContainsOnly( const BString & str, const BString & strContents );
			BOOTIL_EXPORT bool ContainsAny( const BString & str, const BString & strContents );
			BOOTIL_EXPORT bool Contains( const BString & strHaystack, const BString & strNeedle, bool IgnoreCaps = false );
			BOOTIL_EXPORT bool Wildcard( BString strWildcard, BString strHaystack );
			BOOTIL_EXPORT bool ContainsChar( const BString & strHaystack, char c );
			BOOTIL_EXPORT bool ICompare( const BString & strA, const BString & strB );
			BOOTIL_EXPORT bool IsNumber( const BString & strA );
		}

		namespace Sanitize
		{
			BSTRGET( Javascript );
			BSTRGET( Filename );
		}

		namespace File
		{
			BSTRGET( StripFilename );
			BSTRGET( ExtractFilename );
			BSTRGET( StripExtension );
			BSTRGET( ToWindowsSlashes );
			BSTRGET( UpOneDirectory );
			BSTRGET( CleanPath );
			BSTRGET( LastDirectory );

			BOOTIL_EXPORT bool IsFileExtension( const BString & str, const BString & strExt );
			BOOTIL_EXPORT BString GetFileExtension( const BString & str );
			BOOTIL_EXPORT bool IsAbsolutePath( const BString & strPath );
			BOOTIL_EXPORT void FixSlashes( BString & strIn, const BString & strFrom = "\\", const BString & strTo = "/" );
			inline BString GetFixSlashes( BString strIn, const BString & strFrom = "\\", const BString & strTo = "/" ) { FixSlashes( strIn, strFrom, strTo ); return strIn; }
		}

		namespace Format
		{
			BOOTIL_EXPORT WString VarArgs( const wchar_t* s, va_list l );
			BOOTIL_EXPORT BString VarArgs( const char* s, va_list l );
			BOOTIL_EXPORT BString Print( const char* fmt, ... );
			BOOTIL_EXPORT WString Print( const wchar_t* fmt, ... );
			BOOTIL_EXPORT BString Memory( int iBytes );
			BOOTIL_EXPORT BString MemoryPerSecond( int iBytes );
			BOOTIL_EXPORT BString YesNo( bool b );
			BOOTIL_EXPORT BString CommaSeperatedInt( int i );
			BOOTIL_EXPORT BString Time( const BString & strFormat = "%y-%m-%d %H-%M-%S", unsigned int iTime = 0 );
			BOOTIL_EXPORT BString NiceFloat( float f );
			BOOTIL_EXPORT BString NiceDouble( double f );
			BOOTIL_EXPORT BString BinaryToHex( const void* pData, unsigned int iLength );
			BOOTIL_EXPORT BString UInt64( unsigned long long iBytes );
			BOOTIL_EXPORT BString Int( int iBytes );
		}

		namespace To
		{
			BOOTIL_EXPORT int Int( const BString & str );
			BOOTIL_EXPORT float Float( const BString & str );
			BOOTIL_EXPORT double Double( const BString & str );
			BOOTIL_EXPORT bool Floats( const BString & str, float* f, int iCount );
			BOOTIL_EXPORT bool Bool( const BString & str ); // Converts true, false, yes, no, 0, 1
			BOOTIL_EXPORT unsigned long long UInt64( const BString & str );
		}

		namespace URL
		{
			BSTRGET( URI );
			BSTRGET( Query );

			BOOTIL_EXPORT int ParseArguments( BString strQuery, String::Map & map );
		}

		namespace Encode
		{
			BSTRGET( Base64 );
			BSTRGET( URL );
		}

		namespace Decode
		{
			BSTRGET( Base64 );
		}

	}

	template <typename T>
	BString ToStr( const T & t );

}

// Clean this up
#undef BSTRGET

#define Bootil_FormatString( strOutput, format )\
{\
	va_list arglist;	\
	va_start( arglist, format ); \
	strOutput = Bootil::String::Format::VarArgs( format, arglist ); \
	va_end(arglist); \
}
