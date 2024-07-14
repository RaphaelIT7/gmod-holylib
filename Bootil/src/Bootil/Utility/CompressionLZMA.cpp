
#include "Bootil/Bootil.h"
#include "lzma/LzmaLib.h"
#include "lzma/LzmaDec.h"


// Blurgh!!
static void* AllocForLzma( void* p, size_t size ) { return malloc( size ); }
static void FreeForLzma( void* p, void* address ) { free( address ); }
static ISzAlloc SzAllocForLzma = { &AllocForLzma, &FreeForLzma };

namespace Bootil
{
	namespace Compression
	{
		namespace LZMA
		{
			bool Compress( const void* pInputData, unsigned int iInputLength, Bootil::Buffer & output, int iLevel, int iDictSize )
			{
				//
				// The data is written:
				//	5 bytes:	props
				//	8 bytes:	uncompressed size
				//	the rest:	the actual data
				//
				unsigned int iStartPos = output.GetPos();
				size_t props_size = LZMA_PROPS_SIZE;
				size_t iDestSize = iInputLength + iInputLength / 3 + 128;

				if ( !output.EnsureCapacity( iStartPos + iDestSize + LZMA_PROPS_SIZE + 8 ) )
				{ return false; }

				unsigned char* pPropStart = ( unsigned char* ) output.GetCurrent();
				unsigned char* pSizeStart = pPropStart + LZMA_PROPS_SIZE;
				unsigned char* pBodyStart = pSizeStart + 8;
				int res = LzmaCompress(	pBodyStart,						&iDestSize,		// Dest out
										( unsigned char* ) pInputData,	iInputLength,	// Input in
										pPropStart,						&props_size,	// Props out
										iLevel,			// level [0-9]
										iDictSize,		// dict size ( ie 1 << 4 )
										3,
										0,
										2,
										32,
										2 );

				if ( props_size != LZMA_PROPS_SIZE )
				{ return false; }

				if ( res != SZ_OK )
				{ return false; }

				//
				// Write our 8 byte LE size
				//
				pSizeStart[0] = iInputLength & 255;
				pSizeStart[1] = ( iInputLength >> 8 ) & 255;
				pSizeStart[2] = ( iInputLength >> 16 ) & 255;
				pSizeStart[3] = ( iInputLength >> 24 ) & 255;
				pSizeStart[4] = 0;
				pSizeStart[5] = 0;
				pSizeStart[6] = 0;
				pSizeStart[7] = 0;
				//
				// Set the buffer pointers
				//
				unsigned int iEndPos = iStartPos + iDestSize + LZMA_PROPS_SIZE + 8;
				output.SetWritten( iEndPos );
				output.SetPos( iEndPos );
				return true;
			}

			bool PropsAreValid( const unsigned char* pProps )
			{
				if ( pProps[0] >= ( 9 * 5 * 5 ) ) { return false; }

				//LZMA_DIC_MIN
				unsigned int dicSize = pProps[1] | ( ( unsigned int )pProps[2] << 8 ) | ( ( unsigned int )pProps[3] << 16 ) | ( ( unsigned int )pProps[4] << 24 );

				if ( dicSize < ( 1 << 12 ) ) { return false; }

				return true;
			}

			bool Extract( const void* pData, unsigned int iLength, Bootil::Buffer & output, ProgressCallback* pProgress )
			{
				const unsigned char* pPropsBuf = ( unsigned char* ) pData;
				const unsigned char* pSizeBuf = pPropsBuf + LZMA_PROPS_SIZE;
				const unsigned char* pDataBuf = pSizeBuf + 8;

				//
				// Verify that the buffer contains what we're looking for..
				//
				if ( iLength <= LZMA_PROPS_SIZE + 8 ) { return false; }

				if ( !PropsAreValid( pPropsBuf ) ) { return false; }

				//
				// Work out the dest length from the header
				//
				size_t iDestLen = pSizeBuf[0] | ( pSizeBuf[1] << 8 ) | ( pSizeBuf[2] << 16 ) | ( pSizeBuf[3] << 24 );
				size_t iRealDestLen = iDestLen;

				//
				// TODO: Santity check?
				//

				//
				// Make sure we can accommodate this extracted size
				//
				if ( !output.EnsureCapacity( output.GetPos() + iDestLen ) )
				{ return false; }

				//
				// Do extraction
				//
				size_t srcLen = iLength - LZMA_PROPS_SIZE - 8;
				int res = LzmaUncompress( ( unsigned char* )output.GetCurrent(), &iDestLen, pDataBuf, &srcLen, pPropsBuf, LZMA_PROPS_SIZE );

				// TODO! pProgress

				if ( res != SZ_OK )
				{ return false; }

				//
				// Update buffers with new positions
				//
				output.SetWritten( output.GetPos() + iRealDestLen );
				output.SetPos( output.GetPos() + iRealDestLen );
				return true;
			}

			bool ExtractToFile( const void* pData, unsigned int iLength, const Bootil::BString & output, ProgressCallback* pProgress )
			{
				std::ofstream OutputFile;
				const unsigned char* pPropsBuf = ( unsigned char* ) pData;
				const unsigned char* pSizeBuf = pPropsBuf + LZMA_PROPS_SIZE;
				const unsigned char* pDataBuf = pSizeBuf + 8;
				const unsigned int iDataLength = iLength - 8 - LZMA_PROPS_SIZE;

				//
				// Verify that the buffer contains what we're looking for..
				//
				if ( iLength <= LZMA_PROPS_SIZE + 8 ) { return false; }

				if ( !PropsAreValid( pPropsBuf ) ) { return false; }

				//
				// Work out the dest length from the header
				//
				size_t iDestLen = pSizeBuf[0] | ( pSizeBuf[1] << 8 ) | ( pSizeBuf[2] << 16 ) | ( pSizeBuf[3] << 24 );
				//
				// TODO: Santity check?
				//
				CLzmaDec state;
				LzmaDec_Construct( &state );

				if ( SZ_OK != LzmaDec_Allocate( &state, pPropsBuf, LZMA_PROPS_SIZE, &SzAllocForLzma ) ) { return false; }

				LzmaDec_Init( &state );
				// Create the file
				{
					OutputFile.open( output.c_str(), std::ios::binary | std::ios::out );

					if ( !OutputFile.is_open() ) { return false; }
				}
				unsigned int		iChunkSize		= 1024 * 256;
				bool				bReturnStatus	= false;
				unsigned int		iOutPos			= 0;
				unsigned			inPos			= 0;
				unsigned char*		pDestBuffer		= new unsigned char[ iChunkSize ];

				while ( true )
				{
					SizeT iInputSize	= iDataLength - inPos;
					SizeT iOutputSize	= iChunkSize;
					ELzmaStatus	status;
					SRes res = LzmaDec_DecodeToBuf( &state, pDestBuffer, &iOutputSize, pDataBuf + inPos, &iInputSize, LZMA_FINISH_ANY, &status );

					//
					// Got data!
					//
					if ( iOutputSize > 0 )
					{
						OutputFile.write( ( const char* ) pDestBuffer, iOutputSize );
						iOutPos += iOutputSize;

						if ( iOutPos >= iDestLen )
						{
							bReturnStatus = true;
							break;
						}
					}

					//
					// Finished (!!)
					//
					if ( iOutputSize == 0 && iInputSize == 0 )
					{
						bReturnStatus = true;
						break;
					}

					if ( res != SZ_OK )
					{
						break;
					}

					if ( pProgress )
					{
						pProgress->OnProgress( (float)iOutPos / (float)iDestLen,  iDestLen, iOutPos );
					}

					inPos += iInputSize;
				}

				if ( pProgress )
				{
					pProgress->OnProgress( 1.0f, iDestLen, iDestLen );
				}

				// Clean up
				delete[] pDestBuffer;
				LzmaDec_Free( &state, &SzAllocForLzma );
				// Close the file. We're all done!
				{
					OutputFile.close();
				}

				// If we errored, remove the file
				if ( !bReturnStatus )
				{
					File::RemoveFile( output.c_str() );
				}

				return bReturnStatus;
			}

			class ExtractionThread : public Threads::Thread, public Job, public ProgressCallback
			{
				public:

					ExtractionThread( const void* pData, unsigned int iLength, const Bootil::BString & strOutputFile )
					{
						m_Buffer.Write( pData, iLength );
						m_strOutputFile = strOutputFile;
						m_bFinished = false;
						m_bSuccess = false;
						m_fProgress = 0.0f;
						StartInThread();
					}

					virtual void Run()
					{
						AutoBuffer buffer;
						bool bSuccess = ExtractToFile( m_Buffer.GetBase(), m_Buffer.GetWritten(), m_strOutputFile, this );
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

					virtual float GetProgress()
					{
						Threads::Guard g( &m_Mutex );
						return m_fProgress;
					}

					virtual bool OnProgress( float fPercent, unsigned int iDataSize, unsigned int iDataProcessed )
					{
						Lock();
							m_fProgress = fPercent;
						Unlock();

						return true;
					}

					Bootil::BString		m_strOutputFile;
					AutoBuffer			m_Buffer;

					bool m_bFinished;
					bool m_bSuccess;

					float	m_fProgress;
			};

			Job* ExtractInThread( const void* pData, unsigned int iLength, const Bootil::BString & strOutputFile )
			{
				return new ExtractionThread( pData, iLength, strOutputFile );
			}
		}
	}
}