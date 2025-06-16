#include "Bootil/Bootil.h"

namespace Bootil
{
	namespace String
	{
		namespace Format
		{
			BOOTIL_EXPORT WString VarArgs( const wchar_t* s, va_list l )
			{
				static wchar_t string[4096];
				vswprintf( string, 4096, s, l );
				return string;
			}

			BOOTIL_EXPORT BString VarArgs( const char* s, va_list l )
			{
				static char string[4096];
				vsnprintf( string, 4096, s, l );
				return string;
			}

			BOOTIL_EXPORT BString Print( const char* fmt, ... )
			{
				va_list s;
				va_start( s, fmt );
				static char strOut[4096];
				vsnprintf( strOut, 4096, fmt, s );
				va_end( s );
				return strOut;
			}

			BOOTIL_EXPORT WString Print( const wchar_t* fmt, ... )
			{
				va_list s;
				va_start( s, fmt );
				static wchar_t strOut[4096];
				vswprintf( strOut, 4096, fmt, s );
				va_end( s );
				return strOut;
			}

			BOOTIL_EXPORT BString Memory( int iBytes )
			{
				float gb = iBytes / 1024.0f / 1024.0f / 1024.0f;

				if ( gb >= 1.0 )
				{
					return Print( "%.1f GB", gb );
				}

				float mb = iBytes / 1024.0f / 1024.0f;

				if ( mb >= 1.0 )
				{
					return Print( "%.1f MB", mb );
				}

				float kb = iBytes / 1024.0f;

				if ( kb >= 1.0 )
				{
					return Print( "%.1f KB", kb );
				}

				// return as bytes
				return Print( "%i B", iBytes );
			}

			BOOTIL_EXPORT BString MemoryPerSecond( int iBytes )
			{
				return Memory( iBytes ) + "/s";
			}

			BOOTIL_EXPORT BString YesNo( bool b )
			{
				if ( b ) { return "Yes"; }

				return "No";
			}

			BOOTIL_EXPORT BString CommaSeperatedInt( int i )
			{
				BString str = Print( "%i", i );
				unsigned int iCommas = ( str.length() / 3 );

				for ( unsigned int i = 0; i < iCommas; i++ )
				{
					if ( str.length() <= ( ( i + 1 ) * 3 ) + i )
					{ continue; }

					str.insert( str.length() - ( ( ( i + 1 ) * 3 ) + i ), "," );
				}

				return str;
			}

			BOOTIL_EXPORT BString Time( const BString & strFormat, unsigned int iTime )
			{
				time_t rawtime = iTime;
				time( &rawtime );
				struct tm* timeinfo = localtime( &rawtime );
				char buffer[ 512 ];
				strftime( buffer, 512, strFormat.c_str(), timeinfo );
				return BString( buffer );
			}

			BOOTIL_EXPORT BString NiceFloat( float f )
			{
				BString strFloat = Print( "%.04f", f );
				Util::TrimRight( strFloat, "0" );
				Util::TrimRight( strFloat, "." );
				return strFloat;
			}

			BOOTIL_EXPORT BString NiceDouble( double f )
			{
				BString strFloat = Print( "%.04f", f );
				Util::TrimRight( strFloat, "0" );
				Util::TrimRight( strFloat, "." );
				return strFloat;
			}

			BOOTIL_EXPORT BString BinaryToHex( const void* pData, unsigned int iLength )
			{
				BString str = "";
				const unsigned char* pBytes = ( const unsigned char* ) pData;

				for ( unsigned int i = 0; i < iLength; i++ )
				{
					str += Print( "%02x", pBytes[i] );
				}

				return str;
			}

			BOOTIL_EXPORT BString UInt64( unsigned long long iBytes )
			{
				return Print( "%llu", iBytes );
			}

			BOOTIL_EXPORT BString Int( int iBytes )
			{
				return Print( "%i", iBytes );
			}
		}
	}

	template <> BOOTIL_EXPORT BString ToStr( const int & t )					{ return Bootil::String::Format::Print( "%i", t ); }
	template <> BOOTIL_EXPORT BString ToStr( const unsigned int & t )		{ return Bootil::String::Format::Print( "%u", t ); }
	template <> BOOTIL_EXPORT BString ToStr( const float & t )				{ return Bootil::String::Format::Print( "%f", t ); }
	template <> BOOTIL_EXPORT BString ToStr( const unsigned long long & t )	{ return Bootil::String::Format::Print( "%llu", t ); }
}
