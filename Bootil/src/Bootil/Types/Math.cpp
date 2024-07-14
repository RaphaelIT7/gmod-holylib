#include "Bootil/Bootil.h"
#include <stdlib.h>
#include <time.h>

class InitialRandomSeeder
{
	public:

		InitialRandomSeeder()
		{
			Bootil::Math::Random::Seed( time( NULL ) );
		}
};

InitialRandomSeeder seeder;

namespace Bootil
{
	namespace Math
	{
		namespace Random
		{
			BOOTIL_EXPORT void Seed( unsigned int Seed )
			{
				srand( Seed );
			}

			BOOTIL_EXPORT int Int( int iMin, int iMax )
			{
				return iMin + ( rand() % ( iMax - iMin ) );
			}

			BOOTIL_EXPORT bool Bool()
			{
				return rand() % 2 == 1;
			}
		}

	}
}
