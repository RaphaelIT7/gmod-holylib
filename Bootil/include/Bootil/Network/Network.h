#pragma once

namespace Bootil
{
	namespace Network
	{
		BOOTIL_EXPORT void Start();
		BOOTIL_EXPORT void End();

		namespace Util 
		{
			BOOTIL_EXPORT BString IpToString( unsigned long IP, bool bBigEnd = false );
			BOOTIL_EXPORT unsigned long StringToIp( const BString& strIP );
		}

	}
}
