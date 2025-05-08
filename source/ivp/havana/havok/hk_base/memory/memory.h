#ifndef HK_BASE_MEMORY_H
#define HK_BASE_MEMORY_H

#define HK_MEMORY_MAX_ROW				12
#define HK_MEMORY_MAX_SIZE_SMALL_BLOCK	512U
#define HK_MEMORY_EXTRA_BLOCK_SIZE		8192U

#define HK_MEMORY_MAGIC_NUMBER			0x3425234U
#define HK_MEMORY_CACHE_ALIGNMENT 64U

#define HK_MEMORY_ENABLE_STATISTICS 

//#define HK_MEMORY_ENABLE_DEBUG_CHECK		/* enable if extra asserts and checks should be performed */

// If you add a memory class, remember to change
// the table in print_statistics
enum class hk_MEMORY_CLASS : unsigned
{
	HK_MEMORY_CLASS_UNKNOWN = 0,
	HK_MEMORY_CLASS_DUMMY,
	HK_MEMORY_CLASS_ARRAY,
	HK_MEMORY_CLASS_HASH,
	HK_MEMORY_CLASS_SORTED_SET,
	HK_MEMORY_CLASS_DENSE_VECTOR,
	HK_MEMORY_CLASS_ENTITY,
	HK_MEMORY_CLASS_RIGID_BODY,
	HK_MEMORY_CLASS_EFFECTOR,
	HK_MEMORY_CLASS_CORE,
	HK_MEMORY_CLASS_CONSTRAINT,
	HK_MEMORY_CLASS_MANAGER,

	HK_MEMORY_CLASS_SIMUNINT,
	HK_MEMORY_CLASS_SIM_SLOT,

	HK_MEMORY_CLASS_PSI_TIME,

	HK_MEMORY_CLASS_BROAD_PHASE,
	HK_MEMORY_CLASS_CA_DISPATCHER,

	//havok compat
	HK_MEMORY_CLASS_GENERIC,
	HK_MEMORY_CLASS_STL,
	HK_MEMORY_CLASS_MATH,
	HK_MEMORY_CLASS_EVENT,
	HK_MEMORY_CLASS_ACTION,
	HK_MEMORY_CLASS_GEOMETRY,
	HK_MEMORY_CLASS_CDINFO,
	HK_MEMORY_CLASS_DISPLAY,
	HK_MEMORY_CLASS_EXPORT,

	HK_MEMORY_CLASS_USR1,
	HK_MEMORY_CLASS_USR2,
	HK_MEMORY_CLASS_USR3,
	HK_MEMORY_CLASS_USR4,

	HK_MEMORY_CLASS_MAX = 256
};


#define HK_NEW_DELETE_FUNCTION_CLASS( THIS_CLASS, MEMORY_CLASS )							\
	inline void *operator new(hk_size_t size){										\
		HK_ASSERT ( sizeof( THIS_CLASS ) == size );											\
		void *object = hk_Memory::get_instance()->allocate( size, MEMORY_CLASS );			\
		return object;																		\
	}																						\
																							\
	inline void operator delete(void *o){											\
		hk_Memory::get_instance()->deallocate( o, sizeof( THIS_CLASS ), MEMORY_CLASS );				\
	}																						\
																							\

#define HK_NEW_DELETE_FUNCTION_VIRTUAL_CLASS( THIS_CLASS, MEMORY_CLASS )					\
	inline void *operator new(hk_size_t size ){										\
		THIS_CLASS *object = (THIS_CLASS *)hk_Memory::get_instance()->allocate( size, MEMORY_CLASS );	\
		object->m_memsize = size;															\
		return object;																		\
	}																						\
	inline void operator delete(void *o){											\
		THIS_CLASS *object = (THIS_CLASS *)o;												\
		hk_Memory::get_instance()->deallocate( o, object->m_memsize, MEMORY_CLASS );			\
	}																						\


class hk_Memory
{
	public:

		hk_Memory();
		//: a really empty constructor

		hk_Memory(char *buffer, hk_size_t buffer_size);

		void init_memory( char *buffer, hk_size_t buffer_size );
		//: initialized the memory pool


		~hk_Memory();

		void *allocate(hk_size_t byte_size, hk_MEMORY_CLASS);
		//: allocate a piece of memory
		//: Note: the size of that piece is not stored, so
		//: the user has to do remember the size !!!
		//: the memory class is currently used only for statistics

		void deallocate(void *, hk_size_t byte_size, hk_MEMORY_CLASS);
		//: deallocate a piece of memory 

		void* allocate_and_store_size(hk_size_t byte_size, hk_MEMORY_CLASS );
		//: allocate a piece of memory
		//: Note: the size of that piece is stored, so
		//: 16 bytes of memory are wasted
		void  deallocate_stored_size(void*, hk_MEMORY_CLASS );
		//: deallocate a piece of memory which has been allocated of allocate_and_store_size

		void* allocate_debug(hk_size_t n, const char* file, int line);
		void  deallocate_debug(void*, hk_size_t n, const char* file, int line);

		static hk_Memory *get_instance();

		//void print_statistics(class hk_Console *); // see hk_Memory_Util

	public: // THE interfaces to the system allocate, change this if you want to add in your own big block memory allocation
		static void *aligned_malloc( hk_size_t size, hk_size_t alignment);
		static void aligned_free(	 void *data );
		static inline void* memcpy(void* dest, const void* src, hk_size_t size);
		static inline void* memset(void* dest, hk_uchar val, hk_size_t size);

	private:
		void* allocate_real(hk_size_t size);

	protected:
		friend class hk_Memory_Util;
		struct hk_Memory_Elem {
			hk_Memory_Elem *m_next;
			hk_uint32 m_magic;
		};

		struct hk_Memory_Statistics {
			hk_size_t m_size_in_use;
			hk_size_t m_n_allocates;
			hk_size_t m_blocks_in_use;
			hk_size_t m_max_size_in_use;
		};

		struct hk_Memory_Block {
			hk_Memory_Block *m_next;
			hk_int32		m_pad[(64 - sizeof(hk_Memory_Block *))/ sizeof(hk_int32)];
		};

	protected:
		hk_Memory_Elem *m_free_list[HK_MEMORY_MAX_ROW];
		hk_Memory_Block *m_allocated_memory_blocks;

		hk_size_t  m_blocks_in_use[HK_MEMORY_MAX_ROW];
		char *m_memory_start;
		char *m_memory_end;
		char *m_used_end;
		hk_uint16 m_row_to_size[HK_MEMORY_MAX_ROW];
		hk_Memory_Statistics m_statistics[static_cast<unsigned>(hk_MEMORY_CLASS::HK_MEMORY_CLASS_MAX)];
		hk_uchar m_size_to_row[ HK_MEMORY_MAX_SIZE_SMALL_BLOCK+1 ];

};

#include <hk_base/memory/memory.inl>

template<typename T>
T* hk_allocate(size_t size, hk_MEMORY_CLASS memory_class) {
	return static_cast<T *>(
		hk_Memory::get_instance()->allocate(sizeof(T) * size, memory_class));
	//(T*)hk_Memory::get_instance()->allocate_debug(sizeof(T)*(N),__FILE__,__LINE__)
}

template<typename T>
void hk_deallocate(T *buffer, size_t size, hk_MEMORY_CLASS memory_class) {
	hk_Memory::get_instance()->deallocate(buffer, sizeof(T) * size, memory_class);
	//hk_Memory::get_instance()->deallocate_debug(P,sizeof(T)*(N),__FILE__,__LINE__)
}

#endif // HK_BASE_MEMORY_H
