#pragma once

namespace Bootil
{
	namespace Image
	{
		struct Format
		{
			unsigned int	width;
			unsigned int	height;
			AutoBuffer		data;
			bool			alpha;
		};
	}

}