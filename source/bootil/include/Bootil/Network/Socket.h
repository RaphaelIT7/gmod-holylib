#pragma once

namespace Bootil
{
	namespace Network
	{
		class Socket
		{
			public:

				Socket();
				~Socket();

				//
				// Listener Mode (Start as a server)
				//
				bool InitAsListener( unsigned int iPort );
				Socket* Accept();

				//
				// Regular connection mode
				//
				bool Connect( const Bootil::BString& strIP, unsigned int iPort );
				bool WaitForConnection();

				
				bool IsConnected();			// Returns true if connection is all fine and dandy				
				bool IsConnecting();		// Returns true if we're in the process of connecting
				BString ToString();			// Returns a stringified IP

				// Closes the socket, ends all connections.
				void Close( const char* strReason );

				//
				// Cycle - sends and received - call every frame!
				//
				void Cycle();

				//
				// Writing
				//
				bool WriteData( void* pData, unsigned long iDataLen );
				bool WriteData( Bootil::Buffer& buffer );

				template <typename T> bool Write( const T& t ){ return WriteData( (void*) &t, sizeof(t) ); }

				//
				// Returns the read buffer
				//
				Bootil::Buffer& GetBuffer();

				//
				// Connection Info
				//
				Bootil::BString GetIP();

				//
				// This is called automatically - but you can manually flush this out if the mood strikes you.
				//
				void SendQueued();
				
			private:

				void InitializeSocket();
				bool PreventedBlock();
				void ReceiveToQueue();
				void FinishConnecting();
				void StoreLastError();

			protected:

				int					m_pSocket;
				Bootil::AutoBuffer	m_SendQueue;
				Bootil::AutoBuffer	m_RecvQueue;
				

				bool				m_bListener;
				bool				m_bAttemptingConnect;
				Time::Timer			m_ConnectionTimer;
				
				int					m_LastError;
		};

	}
}
