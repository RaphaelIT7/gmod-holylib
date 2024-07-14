
#include "Bootil/Bootil.h"
#include <ctime>

#include "../../3rdParty/happyhttp/happyhttp.h"

#ifdef _WIN32
#include <winsock2.h>
#endif 

namespace Bootil
{
	namespace Network 
	{
		namespace HTTP
		{
			//
			// Add this data to the response
			//
			void OnData( const happyhttp::Response* r, void* userdata, const unsigned char* data, int n )
			{
				Query* q = static_cast<Query*>(userdata);
				q->GetResponse().Write( data, n );
			}

			void OnComplete( const happyhttp::Response* r, void* userdata ){}

			void Query::Run()
			{
				m_bError = false;
				m_strError = "";

				m_Response.Clear();

				unsigned int iLastData = 0;

				try
				{
					Lock();

						happyhttp::Connection conn( m_strHost.c_str(), m_iPort );
						conn.setcallbacks( NULL, OnData, NULL, (void*) this );

						conn.putrequest( m_strMethod.c_str(), m_strRequest.c_str() );
						conn.putheader( "Accept", "*/*" );
						conn.putheader( "User-Agent", "Agent:Mozilla/5.0 (Windows NT 6.2; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/27.0.1453.116 Safari/537.36" );

						if ( m_PostBody.GetWritten() > 0 )
						{
							// Finish off the post body
							m_PostBody.WriteString( "\r\n--" + m_strBoundary + "--\r\n\r\n", false );

							// Add headers
							conn.putheader( "Content-Type", ("multipart/form-data; boundary=" + m_strBoundary).c_str() );
							conn.putheader( "Content-Length", m_PostBody.GetWritten() );
							conn.putheader( "Connection", "Close" );
							conn.putheader( "Cache-Control", "no-cache" );
						}

						conn.endheaders();

						if ( m_PostBody.GetWritten() > 0 )
						{
							conn.send( (const unsigned char*) m_PostBody.GetBase(), m_PostBody.GetWritten() );
						}
						else
						{
							BString strEmpty;
							conn.send( (const unsigned char*) strEmpty.c_str(), strEmpty.length() );
						}
			
					Unlock();
					
					Time::Timer timer;

					while( conn.outstanding() )
					{
						conn.pump();

						//
						// Don't time out if we got data this frame
						//
						if ( GetResponse().GetWritten() != iLastData )
						{
							timer.Reset();
							iLastData = GetResponse().GetWritten();
						}

						//
						// Time out connection after x seconds of no activity
						//
						if ( timer.Seconds() > (60 * 3) ) break;

						//
						// Breathe
						//
						Bootil::Platform::Sleep( 10 );
					}
				}
				catch ( happyhttp::Wobbly& e )
				{
					m_bError = true;
					m_strError = e.what();
				}
			}


			void Query::SetPostVar( const Bootil::BString& k, const Bootil::BString& v )
			{
				if ( m_strBoundary.empty() )
					SetupBoundary();

				m_PostBody.WriteString( "\r\n--" + m_strBoundary + "\r\n", false );
				m_PostBody.WriteString( "Content-Disposition: form-data; name=\""+k+"\"\r\n\r\n", false );
				m_PostBody.WriteString( v, false );
			}

			Bootil::BString Query::GetResponseString()
			{
				if ( m_Response.GetWritten() == 0 ) return "";

				BString strOut;

				strOut.append( (char*)m_Response.GetBase(), m_Response.GetWritten() );

				return strOut;
			}

			void Query::SetPostFile( const Bootil::BString& strTitle, const Bootil::BString& strFileName, const Bootil::Buffer& data )
			{
				if ( m_strBoundary.empty() )
					SetupBoundary();

				m_PostBody.WriteString( "\r\n", false );
				m_PostBody.WriteString( "--" + m_strBoundary + "\r\n", false );
				m_PostBody.WriteString( "Content-Disposition: file; name=\""+strTitle+"\"; filename=\""+strFileName+"\"\r\n", false );
				m_PostBody.WriteString( "Content-Type: application/octet-stream\r\n", false );
				m_PostBody.WriteString( "Content-Transfer-Encoding: binary\r\n", false );
				m_PostBody.WriteString( "\r\n", false );

				m_PostBody.WriteBuffer( data );
			}

			void Query::SetupBoundary()
			{
				float fSeconds = Time::MilliSeconds();
				int iRandom = Bootil::Math::Random::Int( 0, INT_MAX );

				m_strBoundary = Hasher::MD5::String( String::Format::Print( "boundary_ %f %i _end", fSeconds, iRandom ) );
			}

		}
	}
}