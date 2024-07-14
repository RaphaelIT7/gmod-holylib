#include "Bootil/Bootil.h"



namespace Bootil
{
	namespace String
	{
		BOOTIL_EXPORT void Lower( BString & str )
		{
			std::transform( str.begin(), str.end(), str.begin(), ::tolower );
		}

		BOOTIL_EXPORT void Upper( BString & str )
		{
			std::transform( str.begin(), str.end(), str.begin(), ::toupper );
		}

		BOOTIL_EXPORT BString Random( int iSize, bool bLetters, bool bCapitals, bool bNumbers, bool Specials )
		{
			BString str;

			for ( int i = 0; i < iSize; i++ )
			{
				if ( bLetters && Math::Random::Bool() ) { str += ( char ) Bootil::Math::Random::Int( 'a', 'z' ); }
				else if ( bCapitals && Math::Random::Bool() ) { str += ( char ) Bootil::Math::Random::Int( 'A', 'Z' ); }
				else if ( bNumbers && Math::Random::Bool() ) { str += ( char ) Bootil::Math::Random::Int( '0', '9' ); }
				else { str += ( char ) Bootil::Math::Random::Int( 'a', 'z' ); }
			}

			return str;
		}

		BOOTIL_EXPORT void SortList( String::List & list, bool bReverse )
		{
			if ( bReverse )
			{ std::sort( list.rbegin(), list.rend() ); }
			else
			{ std::sort( list.begin(), list.end() ); }
		}
	}
}
