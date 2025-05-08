#include <hk_base/base.h>
#include <hk_base/memory/memory_util.h>


void hk_Memory_Util::print_statistics( hk_Memory *mem, hk_Console *out )
{
	const char *enum_to_string[static_cast<unsigned>(hk_MEMORY_CLASS::HK_MEMORY_CLASS_MAX)] = {nullptr};
	{
#define H(a) enum_to_string[static_cast<unsigned>(a)] = #a
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_UNKNOWN);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_DUMMY);

		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_ARRAY);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_HASH);
		
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_SORTED_SET);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_DENSE_VECTOR);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_ENTITY);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_RIGID_BODY);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_EFFECTOR);

		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_CORE);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_CONSTRAINT);

		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_MANAGER);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_SIMUNINT);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_SIM_SLOT);

		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_PSI_TIME);

		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_BROAD_PHASE);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_CA_DISPATCHER);

		//havok compat
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_GENERIC);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_STL);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_MATH);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_EVENT);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_ACTION);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_GEOMETRY);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_CDINFO);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_DISPLAY);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_EXPORT);

		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_USR1);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_USR2);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_USR3);
		H(hk_MEMORY_CLASS::HK_MEMORY_CLASS_USR4);
#undef H
	}

	hk_size_t max_mem = 0, current_mem = 0, i = 0;
	{
		// summary
		for (auto &&s : mem->m_statistics)
		{
			if ( i == static_cast<unsigned>(hk_MEMORY_CLASS::HK_MEMORY_CLASS_DUMMY) ) continue;

			max_mem += s.m_max_size_in_use;
			current_mem += s.m_size_in_use;

			++i;
		}
	}

	out->printf( "\nMemory Summary\n" );
	out->printf(   "**************\n" );

	out->printf( "\n%32s: Bytes allocated %zu, max bytes allocated %zu\n","SUMMARY", current_mem, max_mem );


	{
		// details per type
		out->printf( "\nDetails per type\n" );
		out->printf(   "****************\n" );
		for (unsigned j = 0; j < static_cast<unsigned>(hk_MEMORY_CLASS::HK_MEMORY_CLASS_MAX); j++)
		{
			if ( j == static_cast<unsigned>(hk_MEMORY_CLASS::HK_MEMORY_CLASS_DUMMY) ) continue;

			hk_Memory::hk_Memory_Statistics &s = mem->m_statistics[j];
			if ( !s.m_max_size_in_use ) {
				continue;
			}

			const char *type_name = enum_to_string[ j ];
			if (!type_name){
				type_name = "hk_Memory::print_statistics does not know type";
			}
			out->printf( "%32s: blocks: %4zu  size: %5zu  max_size %5zu  avg_size %5zu  allocs %6zu\n",
				type_name,
				s.m_blocks_in_use, s.m_size_in_use,
				s.m_max_size_in_use, s.m_blocks_in_use ? s.m_size_in_use / s.m_blocks_in_use + 1 : 0,
				s.m_n_allocates);
		}
	}

	{
		// details per size
		out->printf( "\nDetails per size\n" );
		out->printf(   "****************\n" );

		for (int j = 0; j < HK_MEMORY_MAX_ROW; j++)
		{
			hk_size_t free_blocks = 0;
			for ( hk_Memory::hk_Memory_Elem *el = mem->m_free_list[j]; el; el = el->m_next ){
				++free_blocks;
			}
			out->printf( "%32s  blocks %4zu  size %5hu  free_elems %4zu  total %6zu\n",
				"", 
				mem->m_blocks_in_use[j],
				mem->m_row_to_size[j],
				free_blocks,
				mem->m_blocks_in_use[j] * mem->m_row_to_size[j]);
		}
	}
}

