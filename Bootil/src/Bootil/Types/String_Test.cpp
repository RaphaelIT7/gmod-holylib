#include "Bootil/Bootil.h"

bool globber( const char* wild, const char* string );

namespace Bootil
{
	namespace String
	{
		namespace Test
		{
			BOOTIL_EXPORT bool StartsWith( const BString & str, const BString & strFind )
			{
				return str.find( strFind ) == 0;
			}

			BOOTIL_EXPORT bool EndsWith( const BString & str, const BString & strFind )
			{
				size_t i = str.rfind( strFind );
				return ( i != std::string::npos ) && ( i == ( str.length() - strFind.length() ) );
			}

			BOOTIL_EXPORT bool Contains( const BString & haystack, const BString & needle, bool IgnoreCaps )
			{
				if ( needle.empty() ) { return true; }

				//
				// If we're ignoring caps then lowercase the haystack and needle
				// then test without ignoring caps.
				//
				if ( IgnoreCaps )
				{
					return Contains( String::GetLower( haystack ), String::GetLower( needle ), false );
				}

				size_t i;
				i = haystack.find( needle );
				return ( i != std::string::npos );
			}

			BOOTIL_EXPORT bool ContainsChar( const BString & strContents, char c )
			{
				std::string::const_iterator tmp( strContents.begin() );
				std::string::const_iterator const tmp_end( strContents.end() );

				for ( ; tmp != tmp_end; ++tmp )
				{
					if ( *tmp == c ) { return true; }
				}

				return false;
			}

			BOOTIL_EXPORT bool ContainsOnly( const BString & str, const BString & strContents )
			{
				std::string::const_iterator tmp( str.begin() );
				std::string::const_iterator const tmp_end( str.end() );

				for ( ; tmp != tmp_end; ++tmp )
				{
					if ( !ContainsChar( strContents, *tmp ) )
					{ return false; }
				}

				return true;
			}

			BOOTIL_EXPORT bool ContainsAny( const BString & str, const BString & strContents )
			{
				std::string::const_iterator tmp( str.begin() );
				std::string::const_iterator const tmp_end( str.end() );

				for ( ; tmp != tmp_end; ++tmp )
				{
					if ( ContainsChar( strContents, *tmp ) )
					{ return true; }
				}

				return false;
			}

			BOOTIL_EXPORT bool Wildcard( BString strWildcard, BString strHaystack )
			{
				return globber( strWildcard.c_str(), strHaystack.c_str() );
			}

			BOOTIL_EXPORT bool ICompare( const BString & strA, const BString & strB )
			{
				if ( strA.size() != strB.size() ) { return false; }

				for ( unsigned int i = 0; i < strA.length(); i++ )
				{
					if ( tolower( strA[i] ) != tolower( strB[i] ) ) { return false; }
				}

				return true;
			}

			BOOTIL_EXPORT bool IsNumber( const BString & str )
			{
				if ( str.length() == 0 ) return false;

				char* cEnd = NULL;
				strtod( str.c_str(), &cEnd );

				// Not sure if this is possible - better safe than sorry.
				if ( cEnd == NULL ) return false;

				// Return true if we're at the end of the string!
				return cEnd[0] == '\0';
			}

		}
	}
}
