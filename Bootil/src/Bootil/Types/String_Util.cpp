#include "Bootil/Bootil.h"

namespace Bootil
{
	namespace String
	{
		namespace Util
		{
			BOOTIL_EXPORT void FindAndReplace( BString & strIn, const BString & strFind, const BString & strReplace )
			{
				size_t pos = 0;

				while ( ( pos = strIn.find( strFind, pos ) ) != std::string::npos )
				{
					strIn.replace( pos, strFind.length(), strReplace );
					pos += strReplace.length();
				}
			}

			BOOTIL_EXPORT void TrimRight( BString & str, const BString & strChars )
			{
				str.erase( str.find_last_not_of( strChars ) + 1 );
			}

			BOOTIL_EXPORT void TrimLeft( BString & str, const BString & strChars )
			{
				str.erase( 0, str.find_first_not_of( strChars ) );
			}

			BOOTIL_EXPORT void Trim( BString & str, const BString & strChars )
			{
				TrimLeft( str, strChars );
				TrimRight( str, strChars );
			}

			BOOTIL_EXPORT void Split( const BString & str, const BString & seperator, String::List & outbits )
			{
				BString strBit;
				int iOffset = 0;
				int iLength = str.length();
				int iSepLen = seperator.length();
				int i = str.find( seperator, 0 );

				while ( i != std::string::npos )
				{
					outbits.push_back( str.substr( iOffset, i - iOffset ) );
					iOffset = i + iSepLen;
					i = str.find( seperator, iOffset );
				}

				outbits.push_back( str.substr( iOffset, iLength - iOffset ) );
			}

			BOOTIL_EXPORT int Count( const BString & str, char chrFind )
			{
				int iCount = 0;
				std::string::const_iterator tmp( str.begin() );
				std::string::const_iterator const tmp_end( str.end() );

				for ( ; tmp != tmp_end; ++tmp )
				{
					if ( *tmp == chrFind )
					{ iCount++; }
				}

				return iCount;
			}

			BOOTIL_EXPORT void TrimAfter( Bootil::BString & str, const Bootil::BString & strFind, bool bIncluding )
			{
				std::size_t i = str.find( strFind.c_str() );

				if ( i == std::string::npos ) { return; }

				if ( bIncluding )
				{ str = str.substr( 0, i ); }
				else
				{ str = str.substr( 0, i + strFind.length() ); }
			}

			BOOTIL_EXPORT void TrimBefore( Bootil::BString & str, const Bootil::BString & strFind, bool bIncluding )
			{
				std::size_t i = str.find( strFind.c_str() );

				if ( i == std::string::npos ) { return; }

				if ( bIncluding )
				{ 
					str = str.substr( i + strFind.length() ); 
				}
				else
				{ 
					str = str.substr( i + 1 ); 
				}
			}

			BOOTIL_EXPORT void SplitLength( const BString & str, int iLength, String::List & outbits )
			{
				for (int i=0; i < str.length(); i += iLength )
				{
					BString strPart = str.substr( i, Bootil::Min<int>( i + iLength, str.length() ) - i );
					outbits.push_back( strPart );
				}
			}
		}

        BOOTIL_EXPORT void ToCharBuffer( const BString & str, char* pBuffer, int len )
        {
            BAssert( len > str.length()+1 );
            memcpy( pBuffer, str.c_str(), str.length()+1 );
        }
    }
}
