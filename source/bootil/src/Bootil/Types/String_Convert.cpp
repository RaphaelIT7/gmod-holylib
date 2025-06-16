#include "Bootil/Bootil.h"

namespace Bootil
{
	namespace String
	{
		namespace Convert
		{
			BOOTIL_EXPORT BString FromWide( const WString & strIn )
			{
				BString temp( strIn.length(), ' ' );
				std::copy( strIn.begin(), strIn.end(), temp.begin() );
				return temp;
			}

			BOOTIL_EXPORT WString ToWide( const BString & strIn )
			{
				WString temp( strIn.length(), L' ' );
				std::copy( strIn.begin(), strIn.end(), temp.begin() );
				return temp;
			}
		}
	}

}
