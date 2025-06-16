#pragma once

namespace Bootil
{
	namespace Image
	{
		namespace JPEG
		{
			BOOTIL_EXPORT bool Load( Bootil::Buffer & input, Bootil::Image::Format & output );
			BOOTIL_EXPORT bool Save( Bootil::Image::Format & input, Bootil::Buffer & output, int iQuality = 85 );
		}
	}

}