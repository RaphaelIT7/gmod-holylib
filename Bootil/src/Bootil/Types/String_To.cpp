#include "Bootil/Bootil.h"

namespace Bootil
{
	namespace String
	{
		namespace To
		{
			BOOTIL_EXPORT int Int( const BString & str )
			{
				if ( str == "" ) { return 0; }

				return atoi( str.c_str() );
			}

			BOOTIL_EXPORT unsigned long long UInt64( const BString & str )
			{
				unsigned long long val = 0;
				sscanf( str.c_str(), "%llu", &val );
				return val;
			}

			BOOTIL_EXPORT float Float( const BString & str )
			{
				if ( str == "" ) { return 0.0f; }

				return ( float )atof( str.c_str() );
			}

			BOOTIL_EXPORT double Double( const BString & str )
			{
				if ( str == "" ) { return 0.0f; }

				return atof( str.c_str() );
			}

			BOOTIL_EXPORT bool Bool( const BString & str )
			{
				if ( str.size() == 0 ) { return false; }

				if ( str[0] == 'T' || str[0] == 't' || str[0] == 'y' || str[0] == 'Y' ) { return true; }

				if ( str[0] == 'F' || str[0] == 'f' || str[0] == 'n' || str[0] == 'N' ) { return false; }

				if ( str[0] == '0' ) { return false; }

				return true;
			}

			BOOTIL_EXPORT bool Floats( const BString & str, float* f, int iCount )
			{
				String::List lst;
				String::Util::Split( str, " ", lst );

				if ( lst.size() != iCount ) { return false; }

				for ( int i = 0; i < iCount; i++ )
				{
					f[i] = String::To::Float( lst[i] );
				}

				return true;
			}
		}
	}
}
