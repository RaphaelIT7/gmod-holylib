
#include "Bootil/Bootil.h"

#define STBI_NO_STDIO
#include "stb_image/stb_image.h"

#include "jpegcompressor/jpge.h"

namespace Bootil
{
	namespace Image
	{
		namespace JPEG
		{
			BOOTIL_EXPORT bool Load( Bootil::Buffer & buf, Bootil::Image::Format & imageout )
			{
				stbi s;
				start_mem( &s, ( const uint8* ) buf.GetBase(), buf.GetWritten() );

				if ( !stbi_jpeg_test( &s ) )
				{ return false; }

				int x, y, comp;
				unsigned char* pData = stbi_jpeg_load( &s, &x, &y, &comp, 3 );
				if ( !pData ) return false;

				imageout.width	= x;
				imageout.height = y;
				imageout.alpha	= false;
				imageout.data.Write( pData, x * y * 3 );
				stbi_image_free( pData );
				return true;
			}

			BOOTIL_EXPORT bool Save( Bootil::Image::Format & input, Bootil::Buffer & output, int iQuality )
			{
				output.EnsureCapacity( input.height * input.width * 3 );

				int buffer_size = output.GetSize();

				jpge::params config;
				config.m_quality = iQuality;

				if ( jpge::compress_image_to_jpeg_file_in_memory( output.GetBase(), buffer_size, input.width, input.height, 3, (const jpge::uint8*) input.data.GetBase(), config ) )
				{
					output.SetWritten( buffer_size );
					return true;
				}

				return false;
			}
		}

		namespace PNG
		{
			BOOTIL_EXPORT bool Load( Bootil::Buffer & buf, Bootil::Image::Format & imageout )
			{
				stbi s;
				start_mem( &s, ( const uint8* ) buf.GetBase(), buf.GetWritten() );

				if ( !stbi_png_test( &s ) )
				{ return false; }

				int x, y, comp;
				unsigned char* pData = stbi_png_load( &s, &x, &y, &comp, 3 );
				if ( !pData) return false;

				imageout.width	= x;
				imageout.height = y;
				imageout.alpha	= false;
				imageout.data.Write( pData, x * y * 3 );
				stbi_image_free( pData );

				return true;
			}
		}
	}

}