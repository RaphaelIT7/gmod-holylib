
#include "Bootil/Bootil.h"

void crc32( const void* key, int len, unsigned long seed, unsigned long* out );
void md5( unsigned char *input, int ilen, unsigned char output[16] );

unsigned long crc32_start( unsigned long seed );
void crc32_add( unsigned long & crc, const void* key, int len );
void crc32_end( unsigned long & crc );

namespace Bootil
{
	namespace Hasher
	{
		namespace CRC32
		{
			unsigned long Easy( const void* pData, unsigned int iLength, unsigned long iSeed )
			{
				unsigned long iReturn = 0;
				crc32( pData, iLength, iSeed, &iReturn );
				return iReturn;
			}

			unsigned long String( const Bootil::BString & strName, unsigned long iSeed )
			{
				return Easy( strName.c_str(), strName.length(), iSeed );
			}

			unsigned long Start( unsigned long iSeed )
			{
				return crc32_start( iSeed );
			}

			void Add( unsigned long & iCRC, const void* pData, unsigned int iLength )
			{
				crc32_add( iCRC, pData, iLength );
			}

			void End( unsigned long & iCRC )
			{
				crc32_end( iCRC );
			}
		}


		namespace MD5
		{
			Bootil::BString	Easy( const void* pData, unsigned int iLength )
			{
				unsigned char output[16];
				::md5( ( unsigned char* ) pData, iLength, output );
				return Bootil::String::Format::BinaryToHex( output, sizeof( output ) );
			}

			Bootil::BString	String( const Bootil::BString & strName )
			{
				return Easy( strName.c_str(), strName.length() );
			}
		}
	}

}