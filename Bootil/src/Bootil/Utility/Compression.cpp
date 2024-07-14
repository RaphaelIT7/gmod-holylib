
#include "Bootil/Bootil.h"
#include "zlib/zlib.h"

#include "xzip/zip.h"
#include "xzip/unzip.h"


extern "C"
{
	int fastlz_compress_level( int level, const void* input, int length, void* output );
	int fastlz_decompress( const void* input, int length, void* output, int maxout );
}

namespace Bootil
{
	namespace Compression
	{
		namespace FastLZ
		{
			//
			// Best guess whether this is FastLZ compressed.
			//
			bool Is( const void* pData, unsigned int iLength )
			{
				if ( !pData ) return false;

				if ( iLength < 2 ) return false;

				int level = ( ( *( const unsigned char* )pData ) >> 5 ) + 1;

				if ( level != 1 && level != 2 ) { return false; }

				// Further proof?
				return true;
			}

			bool Compress( const void* pData, unsigned int iLength, Bootil::Buffer & output )
			{
				int iStartPos = output.GetPos();

				if ( !output.EnsureCapacity( iStartPos + iLength * 1.5 ) )
				{ return false; }

				int iSize = fastlz_compress_level( 2, pData, iLength, output.GetCurrent() );

				if ( iSize <= 0 ) { return false; }

				output.SetWritten( iStartPos + iSize );
				output.SetPos( iStartPos + iSize );
				return true;
			}

			bool Extract( const void* pData, unsigned int iLength, Bootil::Buffer & output )
			{
				if ( !output.EnsureCapacity( iLength ) )
				{ return false; }

				//
				// We loop here because we want to guess the extracted size.
				// We won't loop at all if output has the right capacity when it's passed in
				//
				for ( int i = 0; i < 128; i++ )
				{
					int ret = fastlz_decompress( pData, iLength, output.GetCurrent(), output.GetSize() - output.GetPos() );

					//
					// We didn't have room to extract it, up the size and try again
					//
					if ( ret == -1 )
					{
						if ( !output.EnsureCapacity( output.GetSize() * 1.20f ) )
						{ return false; }

						continue;
					}

					output.SetWritten( output.GetPos() + ret );
					output.SetPos( output.GetPos() + ret );
					return ret > 0;
				}

				return false;
			}

			class ExtractionThread : public Threads::Thread, public Job
			{
				public:

					ExtractionThread( const void* pData, unsigned int iLength, unsigned int iTargetSize, const Bootil::BString & strOutputFile )
					{
						m_Buffer.Write( pData, iLength );
						m_strOutputFile = strOutputFile;
						m_bFinished = false;
						m_bSuccess = false;
						m_TargetSize = iTargetSize;
						StartInThread();
					}

					virtual void Run()
					{
						AutoBuffer buffer;

						if ( m_TargetSize > 0 ) { buffer.EnsureCapacity( m_TargetSize ); }

						bool bSuccess = Extract( m_Buffer.GetBase(), m_Buffer.GetWritten(), buffer );

						if ( bSuccess )
						{
							std::ofstream OutputFile;
							OutputFile.open( m_strOutputFile.c_str(), std::ios::binary | std::ios::out );

							if ( OutputFile.is_open() )
							{
								OutputFile.write( ( const char* ) buffer.GetBase(), buffer.GetWritten() );
								OutputFile.close();
							}
							else
							{
								bSuccess = false;
							}
						}

						m_Buffer.Clear();
						Lock();
						m_bFinished = true;
						m_bSuccess = bSuccess;
						Unlock();
					}

					virtual bool Finished()
					{
						Threads::Guard g( &m_Mutex );
						return m_bFinished;
					}

					virtual void Destroy()
					{
						delete this;
					}

					virtual bool Success()
					{
						Threads::Guard g( &m_Mutex );
						return m_bSuccess;
					}

					virtual float GetProgress(){ return 0.5f; }

					Bootil::BString		m_strOutputFile;
					AutoBuffer			m_Buffer;
					unsigned int		m_TargetSize;

					bool m_bFinished;
					bool m_bSuccess;

			};

			Job* ExtractInThread( const void* pData, unsigned int iLength, unsigned int iTargetSize, const Bootil::BString & strOutputFile )
			{
				return new ExtractionThread( pData, iLength, iTargetSize, strOutputFile );
			}
		}

		namespace Zip
		{
			File::File()
			{
				Init();
			}

			File::File( const BString & strFileName, const BString & strPassword )
			{
				Init();
				m_pRead = ( void* )XZip::OpenZip( strFileName.c_str(), strPassword.c_str() );
				RefreshFiles();
			}

			File::File( void* pBuffer, unsigned int iSize, const BString & strPassword )
			{
				Init();
				m_pRead = ( void* )XZip::OpenZip( pBuffer, iSize, strPassword.c_str() );
				RefreshFiles();
			}

			void File::Init()
			{
				m_pRead = NULL;
				m_Write = NULL;
			}

			int File::GetNumItems()
			{
				return m_Files.size();
			}

			bool File::IsOpen()
			{
				if ( m_pRead ) { return true; }

				if ( m_Write ) { return true; }

				return false;
			}

			void File::EnsureWriter()
			{
				if ( m_Write ) { return; }

				XZip::HZIP handle = XZip::CreateZip( 0, 1024 * 1024 * 1024, NULL );
				m_Write = ( void* )handle;
			}

			bool File::AddFile( const BString & ZipName, const BString & LocalFileName )
			{
				EnsureWriter();
				AutoBuffer file;

				if ( !Bootil::File::Read( LocalFileName, file ) )
				{ return false; }

				if ( ZR_OK != XZip::ZipAdd( ( XZip::HZIP )m_Write, ZipName.c_str(), file.GetBase(), file.GetWritten() ) )
				{ return false; }

				return true;
			}

			bool File::WriteAndClose( const BString & ZipName )
			{
				AutoBuffer buffer;

				if ( !WriteAndClose( buffer ) )
				{ return false; }

				Bootil::File::Write( ZipName, buffer );
				return true;
			}

			bool File::WriteAndClose( Buffer & buffer )
			{
				EnsureWriter();
				void* pBuffer = NULL;
				unsigned long iLength = 0;

				if ( ZR_OK != XZip::ZipGetMemory( ( XZip::HZIP )m_Write, &pBuffer, &iLength ) )
				{ return false; }

				buffer.Write( pBuffer, iLength );
				Close();
				return true;
			}

			void File::Close()
			{
				if ( m_Write )
				{
					CloseZip( ( XZip::HZIP ) m_Write );
					m_Write = NULL;
				}
			}

			void File::ExtractToFolder( const BString & Folder )
			{
				int iItems = GetNumItems();

				for ( int i = 0; i < iItems; i++ )
				{
					BString FileName = GetFileName( i );
					BString LocalFolder = String::File::GetStripFilename( FileName );
					Bootil::File::CreateFolder( Folder + "/" + LocalFolder );
					ExtractFile( i, Folder + "/" + FileName );
				}
			}

			void File::ExtractFile( int i, const BString & FileName )
			{
				XZip::UnzipItem( ( XZip::HZIP )m_pRead, i, FileName.c_str() );
			}

			void File::RefreshFiles()
			{
				m_Files.clear();
				XZip::ZIPENTRY ze;
				XZip::GetZipItem( ( XZip::HZIP )m_pRead, -1, &ze );
				int iCount = ze.index;

				for ( int i = 0; i < iCount; i++ )
				{
					XZip::GetZipItem( ( XZip::HZIP )m_pRead, i, &ze );
					m_Files[ i ].Name = ze.name;
					m_Files[ i ].Size = ze.unc_size;
				}
			}

			BString File::GetFileName( int i )
			{
				return m_Files[ i ].Name;
			}
		}

		namespace GZip
		{
			BOOTIL_EXPORT bool Compress( const void* pData, unsigned int iLength, Bootil::Buffer & output )
			{
				if ( !output.EnsureCapacity( iLength ) )
					return false;

				uLongf outputSize = output.GetSize();

				if ( compress( (Bytef *) output.GetBase(), &outputSize, (const Bytef *)pData, iLength ) != Z_OK )
				{
					return false;
				}

				output.SetWritten( outputSize );
				return true;
			}

/*
			BOOTIL_EXPORT bool Extract( const void* pData, unsigned int iLength, Bootil::Buffer & output, ProgressCallback* pProgress )
			{
				// TODO.

				return false;
			}
*/
		}
	}

}