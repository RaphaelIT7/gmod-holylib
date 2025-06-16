
#include "Bootil/Bootil.h"

#ifdef _WIN32 
	#include <winsock2.h>
	#include <ws2tcpip.h>
#else 
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <errno.h>
	#include <sys/ioctl.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <unistd.h>
	#define ioctlsocket ioctl
	#define closesocket close
	#define WSAGetLastError() errno
#endif 

namespace Bootil
{
	namespace Network
	{
		Socket::Socket()
		{
			m_pSocket				= 0;
			m_bAttemptingConnect	= false;
			m_bListener				= false;
			m_LastError				= 0;
		}

		Socket::~Socket()
		{
			Close( "socket deleted" );
		}

		bool Socket::InitAsListener( unsigned int iPort )
		{
			BAssert( m_pSocket == 0 );

			// create the socket
			m_pSocket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
			if ( m_pSocket == 0 )
				return false;

			// Set up some common config settings
			InitializeSocket();
			
			// bind the socket to this port
			struct sockaddr_in addr;
			addr.sin_family			= AF_INET;
			addr.sin_port			= htons( iPort );
			addr.sin_addr.s_addr	= htonl( INADDR_ANY );
			
			if ( bind( m_pSocket, (struct sockaddr *) &addr, sizeof(addr) ) == -1 )
			{
				Close( "couldn't bind address" );
				return false;
			}

			// start listening
			if ( listen( m_pSocket, 64 ) == -1 )
			{
				Close( "couldn't start listening" );
				return false;
			}

			m_bListener = true;

			return true;
		}

		//
		// Regular connection mode
		//
		bool Socket::Connect( const Bootil::BString& strIP, unsigned int iPort )
		{
			BAssert( m_pSocket == 0 );

			// create the socket
			m_pSocket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
			if ( m_pSocket == 0 )
				return false;

			// Set up some common config settings
			InitializeSocket();

			//
			// Convert string to an actual IP address
			//
			struct hostent* hp = NULL;
			if ( inet_addr( strIP.c_str() ) == INADDR_NONE )
			{
				hp = gethostbyname( strIP.c_str() );
			}
			else
			{
				unsigned long addr = inet_addr( strIP.c_str() );
				hp = gethostbyaddr( (char*)&addr, sizeof(addr), AF_INET );
			}

			if ( !hp )
			{
				Close( "couldn't connect" );
				return false;
			}

			struct sockaddr_in saddr;
				saddr.sin_addr.s_addr	= *((unsigned long*)hp->h_addr);
				saddr.sin_family		= AF_INET;
				saddr.sin_port			= htons( iPort );

			int status = connect( m_pSocket, (struct sockaddr *)&saddr, sizeof(saddr) );
			StoreLastError();

			if ( status == -1 )
			{
				if ( !PreventedBlock() )
				{
					Close( "connect error" );
					return false;
				}

				m_ConnectionTimer.Reset();
				m_bAttemptingConnect = true;
				return true;
			}

			return true;
		}

		bool Socket::WaitForConnection()
		{
			while ( IsConnecting() )
			{
				Cycle();
				Bootil::Platform::Sleep( 10 );				
			}

			return IsConnected();
		}

		Socket* Socket::Accept()
		{
			sockaddr saddr;
			socklen_t saddr_len = sizeof( saddr );

			unsigned int newsock = accept( m_pSocket, &saddr, &saddr_len );
			if ( newsock == -1 )
			{
				return NULL;
			}

			Socket* socket = new Socket();
			socket->m_pSocket = newsock;
			socket->InitializeSocket();

			return socket;
		}

		void Socket::Close( const char* strReason )
		{
			if ( m_pSocket != 0 )
				closesocket( m_pSocket );

			m_pSocket				= 0;
			m_bAttemptingConnect	= false;
			m_bListener				= false;

			m_SendQueue.Clear();
			m_RecvQueue.Clear();

			// Output::Msg( "Close: %i %s\n", m_LastError, strReason );
		}

		bool Socket::IsConnected()
		{
			if ( m_pSocket == 0 ) return false;

			return true;
		}

		void Socket::InitializeSocket()
		{
			// Set to non blocking
			u_long iNonBlocking = 1;
			ioctlsocket( m_pSocket, FIONBIO, &iNonBlocking );

			// disable nagle 
			char iNagle = 1;
			setsockopt( m_pSocket, IPPROTO_TCP, TCP_NODELAY, &iNagle, sizeof(iNagle) );

			// Reusable address
			int reusable = 1;
			setsockopt( m_pSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reusable, sizeof(reusable) );
		}


		void Socket::ReceiveToQueue()
		{
			unsigned long iDataToRead = 0;
			ioctlsocket( m_pSocket, FIONREAD, &iDataToRead );

			if ( iDataToRead == 0 ) 
				iDataToRead = 1;

			m_RecvQueue.EnsureCapacity( m_RecvQueue.GetWritten() + iDataToRead );				

			int ireceived = recv( m_pSocket, (char*) m_RecvQueue.GetBase( m_RecvQueue.GetWritten() ), iDataToRead, 0 );
			StoreLastError();

			// Closed
			if ( ireceived == 0 )
			{
				Close( "recv 0" );
				return;
			}

			// An error
			if ( ireceived < 0 )
			{
				if ( PreventedBlock() )
				{
					return; // It's normal, just chill
				}

				Close( "recv error" );
				return;
			}

			m_RecvQueue.SetWritten( m_RecvQueue.GetWritten() + ireceived );
		}

		//
		// This returns true if winsock threw an error
		// because the socket would have blocked.. so
		// it shouldn't br treated as a real error.
		//
		bool Socket::PreventedBlock()
		{
#ifdef _WIN32	 
			if ( m_LastError == WSAEWOULDBLOCK )
				return true;
#else 
			if ( m_LastError == EAGAIN ) return true;
			if ( m_LastError == EINPROGRESS ) return true;
			if ( m_LastError == EWOULDBLOCK ) return true;
#endif

			return false;
		}

		void Socket::StoreLastError()
		{
#ifdef _WIN32	 
			m_LastError = WSAGetLastError();
#else 
			m_LastError = errno;
#endif
		}

		void Socket::Cycle()
		{
			// Listeners don't do any of this.
			if ( m_bListener ) return;

			// Purge the read data
			m_RecvQueue.TrimLeft( m_RecvQueue.GetPos() );

			// Do send and receive
			if ( IsConnected() && !m_bAttemptingConnect )
			{
				SendQueued();
				ReceiveToQueue();
			}

			if ( m_bAttemptingConnect )
			{
				FinishConnecting();
			}
		}

		void Socket::FinishConnecting()
		{
			struct timeval tv;
				tv.tv_usec	= 1;
				tv.tv_sec	= 0;

			fd_set fdset;
				FD_ZERO( &fdset );
				FD_SET( static_cast<u_int>( m_pSocket ), &fdset );

			int ires = select( m_pSocket + 1, NULL, &fdset, NULL, &tv );
			StoreLastError();

			// Connected!
			if ( ires == 1 )
			{
				m_bAttemptingConnect = false;
				return;
			}

			// Timed out
			if ( m_ConnectionTimer.Seconds() >= 2.0f )
			{
				Close( "timed out" );
				return;
			}

			// Error
			if ( ires < 0 )
			{
				if ( PreventedBlock() ) return;

				Close( "finish connect error" );
			}

		}

		bool Socket::WriteData( void* pData, unsigned long iDataLen )
		{
			if ( !m_SendQueue.EnsureCapacity( iDataLen ) ) return false;
			m_SendQueue.Write( pData, iDataLen );

			return true;
		}

		bool Socket::WriteData( Bootil::Buffer& buffer )
		{
			return WriteData( buffer.GetBase(), buffer.GetWritten() );
		}

		void Socket::SendQueued()
		{
			int iWritten = 0;
			int iMaxWritten = m_SendQueue.GetWritten();

			// Nothing to send..
			if ( iMaxWritten == 0 ) return;

			//
			// Send as much to the network as it will take
			//
			while ( iWritten < iMaxWritten )
			{
				int iDataLeft = iMaxWritten - iWritten;
				//
				// Send a minimal packet size
				//
				int iDataToSend = Bootil::Max( iDataLeft, 1 );
				int ret = send( m_pSocket, (const char *)m_SendQueue.GetBase( iWritten ), iDataToSend, 0 );
				StoreLastError();

				//
				// No error.. All good.. next packet
				//
				if ( ret != -1 )
				{
					iWritten += ret;
					continue;
				}

				//
				// We didn't write anything because the buffer would have blocked
				//
				if ( PreventedBlock() )
				{
					break;
				}

				//
				// There was an error sending
				// Assume it was disconnected
				//
				Close( "send error" );
				return;				
			}

			//
			// Cleared like a fat chick's plate
			//
			if ( iWritten == iMaxWritten )
			{
				m_SendQueue.SetWritten( 0 );
				m_SendQueue.SetPos( 0 );
				return;
			}

			//
			// Still shit left on the queue, so lets crop it
			//
			m_SendQueue.TrimLeft( iWritten );
		}

		Bootil::Buffer& Socket::GetBuffer()
		{
			return m_RecvQueue;
		}

		bool Socket::IsConnecting()
		{
			if ( !m_bAttemptingConnect ) return false;

			Cycle();

			return m_bAttemptingConnect;
		}

		Bootil::BString Socket::ToString()
		{
			struct sockaddr_storage my_addr;
			socklen_t my_addr_len = sizeof(my_addr);

			if ( getsockname( m_pSocket, (struct sockaddr *) &my_addr, &my_addr_len) != -1 )
			{
				char host[256];
				char serv[32];

				if ( getnameinfo( (const struct sockaddr *) &my_addr, sizeof( my_addr ), host, sizeof(host), serv, sizeof(serv), 0) == 0, NI_NUMERICHOST | NI_NUMERICSERV )
				{
					return Bootil::String::Format::Print( "%s:%s", host, serv );
				}
			}

			return "0.0.0.0:0";		
		}

		Bootil::BString Socket::GetIP()
		{
			struct sockaddr_storage my_addr;
			socklen_t my_addr_len = sizeof(my_addr);

			if ( getsockname( m_pSocket, (struct sockaddr *) &my_addr, &my_addr_len) != -1 )
			{
				char host[256];
				char serv[32];

				if ( getnameinfo( (const struct sockaddr *) &my_addr, sizeof( my_addr ), host, sizeof(host), serv, sizeof(serv), 0) == 0, NI_NUMERICHOST | NI_NUMERICSERV )
				{
					return Bootil::String::Format::Print( "%s", host );
				}
			}

			return "0.0.0.0";
		}
	}
}