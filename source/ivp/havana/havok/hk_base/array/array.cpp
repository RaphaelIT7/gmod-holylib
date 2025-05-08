#include <hk_base/base.h>
#include <hk_base/memory/memory.h>
#include <climits>
#include <cstring>

void hk_Array_Base::alloc_mem( int size, int num)
{
	HK_ASSERT( size > 0 );
	HK_ASSERT( num > 0 && num <= USHRT_MAX );

	m_elems = hk_allocate<char>( static_cast<size_t>(size) * static_cast<size_t>(num), hk_MEMORY_CLASS::HK_MEMORY_CLASS_ARRAY );
	m_memsize = static_cast<hk_array_store_index>(num);
}

void hk_Array_Base::grow_mem( int size )
{
	HK_ASSERT( size > 0 );
	HK_ASSERT( m_memsize != USHRT_MAX );

	// dimhotepus: Sometimes m_memsize * 2 doesn't fit unsigned short.
	hk_array_store_index new_memsize = static_cast<hk_array_store_index>(std::min( USHRT_MAX, m_memsize + m_memsize ));
	if (!new_memsize) {
		new_memsize = 2;
	}

	char *new_array = hk_allocate<char>( new_memsize * static_cast<size_t>(size), hk_MEMORY_CLASS::HK_MEMORY_CLASS_ARRAY );

	if( m_elems )
		memcpy( new_array, m_elems, m_memsize * static_cast<size_t>(size) );

	if ( m_elems && (m_elems != reinterpret_cast<char *>(this + 1)))
	{
		hk_deallocate( m_elems, m_memsize * static_cast<size_t>(size), hk_MEMORY_CLASS::HK_MEMORY_CLASS_ARRAY );
	}
	m_memsize = new_memsize;
	m_elems = new_array;
}

void hk_Array_Base::grow_mem( int size, int n_elems )
{
	HK_ASSERT( size > 0 );
	HK_ASSERT( n_elems > 0 );

	int new_memsize = m_memsize + n_elems;
	HK_ASSERT( new_memsize <= USHRT_MAX );

	char *new_array = hk_allocate<char>( static_cast<size_t>(new_memsize) * static_cast<size_t>(size), hk_MEMORY_CLASS::HK_MEMORY_CLASS_ARRAY );
	if (!new_array) HK_BREAK;

	if ( m_elems )
	{
		memcpy( new_array, m_elems, m_memsize * static_cast<size_t>(size) );

		if ( m_elems != reinterpret_cast<char *>(this + 1) )
		{
			hk_deallocate( m_elems, m_memsize * static_cast<size_t>(size), hk_MEMORY_CLASS::HK_MEMORY_CLASS_ARRAY );
		}
	}
	m_memsize = static_cast<hk_array_store_index>(new_memsize);
	m_elems = new_array;
}

