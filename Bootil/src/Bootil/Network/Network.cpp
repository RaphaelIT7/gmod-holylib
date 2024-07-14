
#include "Bootil/Bootil.h"

#ifdef _WIN32 
	#include <winsock2.h>
#else 
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
#endif 


namespace Bootil
{
	namespace Network
	{
		BOOTIL_EXPORT void Start()
		{
			#ifdef _WIN32
			WSAData wsaData;
			WSAStartup( MAKEWORD(1, 1), &wsaData );
			#endif 
		}

		BOOTIL_EXPORT void End()
		{
			#ifdef _WIN32

			WSACleanup();
			
			#endif 
		}

		namespace Util 
		{
			BOOTIL_EXPORT BString IpToString( unsigned long IP, bool bBigEnd )
			{
				BString strOut;
				unsigned char *ipByte = (unsigned char *)&IP;
				
				if ( bBigEnd )
					strOut = Bootil::String::Format::Print( "%u.%u.%u.%u", (int)(ipByte[0]), (int)(ipByte[1]), (int)(ipByte[2]), (int)(ipByte[3]) );
				else 
					strOut = Bootil::String::Format::Print( "%u.%u.%u.%u", (int)(ipByte[3]), (int)(ipByte[2]), (int)(ipByte[1]), (int)(ipByte[0]) );

				return strOut;
			}

			BOOTIL_EXPORT unsigned long StringToIp( const BString& strIP )
			{
				return htonl( inet_addr( strIP.c_str() ) );
			}
		}
	}
}