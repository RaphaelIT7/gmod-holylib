#pragma once
#include "Bootil/Bootil.h"

namespace Bootil
{
	namespace Network
	{
		namespace HTTP
		{
			//
			// If you're using these functions - you should call these
			// at the start and at the end of your program.
			//
			BOOTIL_EXPORT void Open();
			BOOTIL_EXPORT void Close();

			//
			// A simple HTTP query
			//
			class BOOTIL_EXPORT Query : public Bootil::Threads::Thread
			{
				public:

					Query()
					{
						SetMethod( "GET" );
					}

					void SetURL( const Bootil::BString& strHostname, const Bootil::BString& strRequest, unsigned int iPort = 80 ){ m_strHost = strHostname; m_strRequest = strRequest; m_iPort = iPort; }
					void SetMethod( const Bootil::BString& strMethod ){ m_strMethod = strMethod; }
					void SetPostVar( const Bootil::BString& k, const Bootil::BString& v );
					void SetPostFile( const Bootil::BString& title, const Bootil::BString& filename, const Bootil::Buffer& filedata );

					void Run();

					Bootil::Buffer& GetResponse(){ return m_Response; }
					Bootil::BString GetResponseString();

					bool IsErrored(){return m_bError; }
					Bootil::BString GetErrorString(){ return m_strError; }

				protected:

					void DoRun();
					void SetupBoundary();

					bool				m_bError;
					Bootil::BString		m_strError;

					Bootil::BString		m_strHost;
					Bootil::BString		m_strMethod;
					unsigned int		m_iPort;
					Bootil::BString		m_strRequest;
					Bootil::AutoBuffer	m_Response;

					Bootil::AutoBuffer	m_PostBody;
					Bootil::BString		m_strBoundary;

			};

		}
	}

}