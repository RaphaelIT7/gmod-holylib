#pragma once

namespace Bootil
{
	namespace Math
	{
		namespace Random
		{
			BOOTIL_EXPORT void Seed( unsigned int Seed );

			BOOTIL_EXPORT int Int( int iMin, int iMax );
			BOOTIL_EXPORT bool Bool();
		}
	}
}