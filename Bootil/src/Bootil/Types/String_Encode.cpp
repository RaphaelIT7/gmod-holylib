#include "Bootil/Bootil.h"
#include "stlencoders/base64.hpp"
#include <iterator>

Bootil::BString CharToHex( char dec )
{
	char dig1 = ( dec & 0xF0 ) >> 4;
	char dig2 = ( dec & 0x0F );

	if ( 0 <= dig1 && dig1 <= 9 )	{ dig1 += 48; }

	if ( 10 <= dig1 && dig1 <= 15 ) { dig1 += ( 65 - 10 ); }

	if ( 0 <= dig2 && dig2 <= 9 )	{ dig2 += 48; }

	if ( 10 <= dig2 && dig2 <= 15 ) { dig2 += ( 65 - 10 ); }

	std::string r;
	r += dig1;
	r += dig2;
	return r;
}

namespace Bootil
{
	namespace String
	{
		namespace Encode
		{
			BOOTIL_EXPORT void Base64( BString & strOutput )
			{
				BString strInput = strOutput;
				strOutput = "";
				stlencoders::base64<char>::encode( strInput.begin(), strInput.end(), std::back_inserter( strOutput ) );
			}

			BOOTIL_EXPORT void URL( BString & strIn )
			{
				BString strOut;

				for ( int i = 0; i < strIn.length(); i++ )
				{
					if ( ( 48 <= strIn[i] && strIn[i] <= 57 ) || ( 65 <= strIn[i] && strIn[i] <= 90 ) || ( 97 <= strIn[i] && strIn[i] <= 122 ) || ( strIn[i] == '~' || strIn[i] == '-' || strIn[i] == '_' || strIn[i] == '.' ) )
					{
						strOut += strIn[i];
					}
					else
					{
						strOut += "%";
						strOut += CharToHex( strIn[i] );
					}
				}

				strIn = strOut;
			}
		}

		namespace Decode
		{
			BOOTIL_EXPORT void Base64( BString & strOutput )
			{
				BString strInput = strOutput;
				strOutput = "";
				stlencoders::base64<char>::decode( strInput.begin(), strInput.end(), std::back_inserter( strOutput ) );
			}
		}
	}

}
