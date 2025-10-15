#include <hk_base/base.h>
#include <cstdlib>

namespace {

struct hk_Memory_With_Size {
	hk_size_t m_size;

	enum {
		MAGIC_MEMORY_WITH_SIZE = 0x2345656
	} m_magic;

	hk_int32 m_dummy[2];
};

constexpr inline hk_uchar size_to_row( hk_uint16 size )
{
	static_assert (HK_MEMORY_MAX_ROW == 12 );

	if (size <= 8 ) return 1;
	else if (size <= 16 ) return 2;
	else if (size <= 32 ) return 3; //-V112
	else if (size <= 48 ) return 4; //-V112
	else if (size <= 64 ) return 5;
	else if (size <= 96 ) return 6;
	else if (size <= 128 ) return 7;
	else if (size <= 160 ) return 8;
	else if (size <= 192 ) return 9;
	else if (size <= 256 ) return 10;
	else if (size <= 512 ) return 11;
	else{
		HK_BREAK;
		return 0;
	}
}

}  // namespace

void hk_Memory::init_memory( char *buffer, hk_size_t buffer_size )
{
	m_memory_start = buffer;
	m_used_end = buffer;
	m_memory_end = m_used_end + buffer_size;
	m_allocated_memory_blocks = HK_NULL;

	for (int i = HK_MEMORY_MAX_ROW-1; i>=0 ; i-- )
	{
		m_free_list[i] = HK_NULL;
		m_blocks_in_use[i] = 0;
	}

	for (hk_uint16 j = 0; j <= HK_MEMORY_MAX_SIZE_SMALL_BLOCK; j++ )
	{
		hk_uchar row = size_to_row(j);
		m_size_to_row[ j ] = row;
		m_row_to_size[row] = j;
	}
	{ // statistics
		for (auto &&s : m_statistics)
		{
			s.m_max_size_in_use = 0U;
			s.m_size_in_use = 0U;
			s.m_n_allocates = 0U;
			s.m_blocks_in_use = 0U;
		}
	}
}

hk_Memory::hk_Memory()
{
	// dimhotepus: Ensure zeroed.
	memset(m_statistics, 0, sizeof(m_statistics));

#ifdef HK_MEMORY_POOL_INITIAL_SIZE
	init_memory( new char[HK_MEMORY_POOL_INITIAL_SIZE], 0 );
#else
	init_memory( HK_NULL, 0 );
#endif
}

hk_Memory::hk_Memory(char *buffer, hk_size_t buffer_size)
{
	// dimhotepus: Ensure zeroed.
	memset(m_statistics, 0, sizeof(m_statistics));

	init_memory( buffer, buffer_size );
}


hk_Memory::~hk_Memory() = default;


void *hk_Memory::allocate_real( hk_size_t size )
{
	if ( size > HK_MEMORY_MAX_SIZE_SMALL_BLOCK )
	{
#ifdef HK_CHECK
		hkprintf("big block allocated size %zu\n", size );
#endif
		return hk_Memory::aligned_malloc( size, HK_MEMORY_CACHE_ALIGNMENT );
	}

	hk_uchar row = m_size_to_row[size];
	size = m_row_to_size[ row ];

	hk_size_t allocated_size = size;
	void *result;

	// allocate first block
	if ( size + m_used_end > m_memory_end)
	{
#ifdef HK_CHECK
		hkprintf("running out of space: block size %zu\n", size );
#endif
		auto *b = static_cast<hk_Memory_Block *>(
			hk_Memory::aligned_malloc( sizeof(hk_Memory_Block) + HK_MEMORY_EXTRA_BLOCK_SIZE, HK_MEMORY_CACHE_ALIGNMENT ) );

		b->m_next = m_allocated_memory_blocks;
		m_allocated_memory_blocks = b;
		m_memory_start = reinterpret_cast<char *>(b+1);
		m_used_end = m_memory_start;
		m_memory_end = m_used_end + HK_MEMORY_EXTRA_BLOCK_SIZE;
	}

	result = m_used_end;

	auto *el = reinterpret_cast<hk_Memory_Elem *>( m_used_end );
	el->m_magic = 0;

	m_used_end += size;

	// allocate rest to get make sure the alignment is ok
	hk_size_t biu = m_blocks_in_use[row];
	while ( allocated_size < 256U )
	{
		if ( size + m_used_end < m_memory_end){
			auto *e = reinterpret_cast<hk_Memory_Elem *>( m_used_end );
			e->m_magic = 0;

			this->deallocate( m_used_end, size, hk_MEMORY_CLASS::HK_MEMORY_CLASS_DUMMY );
			m_used_end += size;
		}else{
			break;
		}
		allocated_size += size;
	}
	m_blocks_in_use[row] = biu;
	return result;
}

void* hk_Memory::allocate(hk_size_t size, hk_MEMORY_CLASS cl)
{
#ifdef HK_MEMORY_ENABLE_STATISTICS 
	hk_Memory_Statistics &s = m_statistics[static_cast<unsigned>(cl)];
	s.m_size_in_use += size;
	s.m_blocks_in_use += 1U;
	s.m_n_allocates += 1U;
	if ( s.m_size_in_use > 	s.m_max_size_in_use)
	{
		s.m_max_size_in_use = s.m_size_in_use;
	}
#endif

#ifdef HK_MEMORY_ENABLE_DEBUG_CHECK
	auto *x = static_cast<hk_Memory_With_Size *>(
		this->hk_Memory::aligned_malloc( size + sizeof( hk_Memory_With_Size ), HK_MEMORY_CACHE_ALIGNMENT ) );
	x->m_size = size;
	x->m_magic = hk_Memory_With_Size::MAGIC_MEMORY_WITH_SIZE;
	return x + 1;
#else

	if ( size <= HK_MEMORY_MAX_SIZE_SMALL_BLOCK){
		hk_uchar row = m_size_to_row[size];
		m_blocks_in_use[row]++;
		hk_Memory_Elem *n = m_free_list[row];
		if ( n ){
			m_free_list[row] = n->m_next;
			n->m_magic = 0;
			return static_cast<void *>(n);
		}
	}
	return allocate_real( size );
#endif
}

void hk_Memory::deallocate(void* p, hk_size_t size, hk_MEMORY_CLASS cl)
{
#ifdef HK_MEMORY_ENABLE_STATISTICS 
	hk_Memory_Statistics &s = m_statistics[static_cast<unsigned>(cl)];
	s.m_size_in_use -= size;
	s.m_blocks_in_use -= 1U;
#endif

#ifdef HK_MEMORY_ENABLE_DEBUG_CHECK
	{
		auto *x = static_cast<hk_Memory_With_Size *>(p);
		x--;
		HK_ASSERT( x->m_magic == hk_Memory_With_Size::MAGIC_MEMORY_WITH_SIZE );
		HK_ASSERT( x->m_size == size );
		this->aligned_free( x );
	}
#else
	if ( size <= HK_MEMORY_MAX_SIZE_SMALL_BLOCK ){
		auto *me = static_cast<hk_Memory_Elem *>(p);
		hk_uchar row = m_size_to_row[size];
		m_blocks_in_use[row]--;
		me->m_next = m_free_list[row];
		HK_ASSERT( me->m_magic != HK_MEMORY_MAGIC_NUMBER);
		me->m_magic = HK_MEMORY_MAGIC_NUMBER;
		m_free_list[row] = me;
	}else{
		hk_Memory::aligned_free(p);
	}
#endif
}



void* hk_Memory::allocate_debug(hk_size_t n,
		const char* /*file*/,
		int /*line*/)
{
	return new char[n];
}

void hk_Memory::deallocate_debug(
		void* p,
		hk_size_t /*n*/,
		const char* /*file*/,
		int /*line*/)
{
	delete[] static_cast<char*>(p);
}



void* hk_Memory::allocate_and_store_size(hk_size_t byte_size, hk_MEMORY_CLASS cl)
{
	auto *x = static_cast<hk_Memory_With_Size *>(
		this->allocate( byte_size + sizeof( hk_Memory_With_Size ), cl) );
	x->m_size = byte_size;
	x->m_magic = hk_Memory_With_Size::MAGIC_MEMORY_WITH_SIZE;
	return x + 1;
}

void  hk_Memory::deallocate_stored_size(void* p, hk_MEMORY_CLASS cl)
{
	if (p)
	{
		auto *x = static_cast<hk_Memory_With_Size *>(p);
		x--;

		HK_ASSERT( x->m_magic == hk_Memory_With_Size::MAGIC_MEMORY_WITH_SIZE );
		this->deallocate( x, x->m_size + sizeof( hk_Memory_With_Size ), cl );
	}
}



void *hk_Memory::aligned_malloc( hk_size_t size, hk_size_t alignment )
{
#if defined(_WIN32)
	return _aligned_malloc ( size, alignment );
#else
	return std::aligned_alloc( alignment, size );
#endif
}

void hk_Memory::aligned_free( void *data )
{
#if defined(_WIN32)
	_aligned_free ( data );
#else
	std::free(data);
#endif

}

hk_Memory *hk_Memory::get_instance()
{
	static hk_Memory s_memory_instance;
	return &s_memory_instance;
}

















