#include "Bootil/Bootil.h"

namespace Bootil
{
	namespace String
	{
		namespace File
		{
			BOOTIL_EXPORT void CleanPath( BString & strIn )
			{
				FixSlashes( strIn );
				Util::Trim( strIn, "/" );
				Util::FindAndReplace( strIn, "//", "/" );
			}

			BOOTIL_EXPORT void FixSlashes( BString & strIn, const BString & strFrom, const BString & strTo )
			{
				return Util::FindAndReplace( strIn, strFrom, strTo );
			}

			BOOTIL_EXPORT void ToWindowsSlashes( BString & strIn )
			{
				return FixSlashes( strIn, "/", "\\" );
			}

			BOOTIL_EXPORT bool IsAbsolutePath( const BString & strPath )
			{
				if ( strPath.length() < 2 ) { return false; }

				if ( strPath[1] != ':' ) { return false; }

				if ( strPath[0] == '.' ) { return false; }

				if ( strPath[0] == '/' ) { return false; }

				if ( strPath[0] == '\\' ) { return false; }

				return true;
			}

			BOOTIL_EXPORT void ExtractFilename( BString & str )
			{
				int i = str.find_last_of( '/' );

				if ( i == BString::npos ) { i = str.find_last_of( '\\' ); }

				if ( i == BString::npos ) { return; }

				str = str.substr( i + 1, str.length() );
			}

			BOOTIL_EXPORT void StripFilename( BString & str )
			{
				int i = str.find_last_of( '/' );

				if ( i == BString::npos ) { i = str.find_last_of( '\\' ); }

				if ( i == BString::npos ) { str = ""; return; }

				str = str.substr( 0, i + 1 );
			}

			BOOTIL_EXPORT BString GetFileExtension( const BString & str )
			{
				int i = str.find_last_of( '.' );

				if ( i == BString::npos ) { return ""; }

				return str.substr( i + 1, str.length() );
			}

			BOOTIL_EXPORT void StripExtension( BString & str )
			{
				int i = str.find_last_of( '.' );

				if ( i == BString::npos ) { return; }

				str = str.substr( 0, i );
			}

			BOOTIL_EXPORT bool IsFileExtension( const BString & str, const BString & strExt )
			{
				BString strA = str;
				BString strB = strExt;
				Lower( strA );
				Lower( strB );
				return Test::EndsWith( strA, strB );
			}

			BOOTIL_EXPORT void UpOneDirectory( BString & strIn )
			{
				String::Util::TrimRight( strIn, "\\/" );
				int iStrLastSlash = strIn.find_last_of( '/' );

				if ( iStrLastSlash == BString::npos ) { strIn = ""; return; }

				strIn = strIn.substr( 0, iStrLastSlash );
			}

			BOOTIL_EXPORT void LastDirectory( BString & str )
			{
				StripFilename( str );
				String::Util::TrimRight( str, "\\/" );
				int i = str.find_last_of( '/' );

				if ( i == BString::npos ) { i = str.find_last_of( '\\' ); }

				if ( i == BString::npos ) { str = ""; return; }

				str = str.substr( i + 1, str.length() );
			}

		}
	}
}
