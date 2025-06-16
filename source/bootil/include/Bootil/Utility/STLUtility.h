#pragma once

namespace Bootil
{
	namespace STL
	{
		namespace Vector
		{
			template <typename T, typename U>
			void Remove( std::vector<T> & vec, const U & var )
			{
				vec.erase( std::remove( vec.begin(), vec.end(), var ), vec.end() );
			}
			/*
						template <typename A>
						A Random( std::vector<A>& list )
						{
							unsigned int iSize = list.size();
							return list[ Random::Int( 0, iSize-1 ) ];
						}
			*/
		}
		/*
				template < class A, class B >
				void PushBack_Unique( A& a, B& b )
				{
					BOOST_FOREACH( A& str, b ){ if ( str == a ) return;}
					b.push_back( a );
				}
		*/
	}

}