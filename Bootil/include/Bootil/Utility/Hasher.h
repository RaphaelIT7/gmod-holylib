#pragma once

namespace Bootil
{
	namespace Hasher
	{
		//
		// Not to tell you your business but CRC is used to verify data rather than to
		//  create low collision hashes. You should use something else for that!
		//
		namespace CRC32
		{
			unsigned long Easy( const void* pData, unsigned int iLength, unsigned long iSeed = 0 );
			unsigned long String( const Bootil::BString & strName, unsigned long iSeed = 0 );

			//
			// Note that when adding, you need to pass a length
			// divisible by 8 until the last add! ( 8, 16, 32, 64 etc )
			//
			unsigned long Start( unsigned long iSeed = 0 );
			void Add( unsigned long & iCRC, const void* pData, unsigned int iLength );
			void End( unsigned long & iCRC );
		}


		namespace MD5
		{
			Bootil::BString	Easy( const void* pData, unsigned int iLength );
			Bootil::BString	String( const Bootil::BString & strName );
		}
	}

}