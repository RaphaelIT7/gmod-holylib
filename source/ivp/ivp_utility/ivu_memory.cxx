// Copyright (C) Ipion Software GmbH 1999-2000. All rights reserved.

#include <ivp_physics.hxx>
#include <cstdlib>

#if !defined(__MWERKS__) || !defined(__POWERPC__)
#ifdef OSX
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif
#endif

#ifndef WIN32
#	pragma implementation "ivu_memory.hxx"
#endif

#include "ivu_memory.hxx"

//IVP_Environment *ivp_global_env=NULL;

void ivp_memory_check(void *a) {
  if (a) return;
#if 0
  //if( !ivp_global_env ) {
  //        return;
  //    }
	    //int fp;
	    //fp=sceOpen("host0:/ipion_out/ipion.txt",SCE_CREAT);
		hk_uintp address = (hk_uintp)a;
		IVP_Time now_time = ivp_global_env->get_current_time();
		IVP_DOUBLE tt = now_time.get_time();
		if (tt > 11.98) {
		    fprintf(stderr, "trying to free %p time %f\n", a, tt);
		    if (a == (void *)(hk_uintp)0x30ca50){
		    	fprintf(stderr, "Crashing soon\n");
		    }

#endif
}

void ivp_byte_swap4(uint& fourbytes)
{
#ifdef _WIN32
	fourbytes = _byteswap_ulong(fourbytes);
#else
	struct FOURBYTES {
		union { 
			unsigned char b[4];
			uint v;
		};
	} in, out;

	in.v = fourbytes;

	out.b[0] = in.b[3];
	out.b[1] = in.b[2];
	out.b[2] = in.b[1];
	out.b[3] = in.b[0];

	fourbytes = out.v;
#endif
}

void ivp_byte_swap2(ushort& twobytes)
{
#ifdef _WIN32
	twobytes = _byteswap_ushort(twobytes);
#else
	struct TWOBYTES {
		union { 
			unsigned char b[2];
			ushort v;
		};
	} in, out;

	in.v = twobytes;

	out.b[0] = in.b[1];
	out.b[1] = in.b[0];
	
	twobytes = out.v;
#endif
}

void *p_malloc(size_t size)
{
#ifndef GEKKO
    return malloc(size);
#else
    return new char[size];
 #endif
}

char *p_calloc(size_t nelem, size_t size)
{
	size_t s = nelem * size;

#ifndef GEKKO
	char *sp = static_cast<char *>(malloc(s));
#else
	char *sp = new char[s];
#endif

	if (sp) memset(sp, 0, s);

	return sp;
}

void* p_realloc(void* memblock, size_t size)
{
#ifndef GEKKO
	return realloc(memblock, size);
#else
	IVP_ASSERT(0);
	return 0;
#endif
}

void p_free(void* data)
{
#ifndef GEKKO
	free(data);
#else
	delete [] static_cast<char*>(data);
#endif
}

constexpr inline uint IVP_MEMORY_MAGIC{0x65981234U};

struct IVP_Aligned_Memory {
    uint magic_number;
    void *back_link;
};

void *ivp_malloc_aligned(size_t size, unsigned short alignment) {
#if defined(SUN__)
    return memalign( alignment, size);
#else
    size += alignment + sizeof(IVP_Aligned_Memory);

    auto *data = static_cast<IVP_Aligned_Memory*>(p_malloc(size));
    if (data) {
        data->magic_number = IVP_MEMORY_MAGIC;

        void *ret = (void *)((((hk_uintp)data) + alignment + sizeof(IVP_Aligned_Memory) - 1) & (-static_cast<hk_intp>(alignment)));
        ((void **)ret)[-1] = (void *)data;
        return ret;
    }

    return nullptr;
#endif
}

void* IVP_CDECL ivp_calloc_aligned(size_t size, unsigned short alignment) {
    size += alignment + sizeof(IVP_Aligned_Memory);

    auto *data = static_cast<IVP_Aligned_Memory*>(p_malloc(size));
    if (data) {
        memset(data, 0, size);
        data->magic_number = IVP_MEMORY_MAGIC;

        void *ret = (void *)((((hk_uintp)data) + alignment + sizeof(IVP_Aligned_Memory) - 1) & (-static_cast<hk_intp>(alignment)));
        ((void **)ret)[-1] = (void *)data;
        return ret;
    }

    return nullptr;
}

void ivp_free_aligned(void *data)
{
#if defined(SUN__)
    p_free(data);
#else
    auto *am = static_cast<IVP_Aligned_Memory *>(((void **)data)[-1]);

    IVP_ASSERT ( am->magic_number == IVP_MEMORY_MAGIC);
    IVP_IF(1){
        am->magic_number = 0;
    }
    p_free( am );
#endif
}

IVP_U_Memory::~IVP_U_Memory()
{
    free_mem();
}

void IVP_U_Memory::init_mem_transaction_usage(char *external_mem, size_t size) {
    //IVP_IF(1) {
	transaction_in_use=0;
    //}
#if defined(MEMTEST)
#else
	if (external_mem) {
		size_of_external_mem = size - IVU_MEM_ALIGN;  // for header
		auto *memelem = reinterpret_cast<p_Memory_Elem *>(external_mem);
		memelem->next = last_elem;
		last_elem = memelem;
		auto *tmp = static_cast<char*>(align_to_next_adress(&memelem->data[0]));
		speicherbeginn = tmp;
		speicherende = tmp + size_of_external_mem;
	} else {
		size_of_external_mem = 0;
		neuer_sp_block(0);
	}
	first_elem=last_elem;
#endif
}


void IVP_U_Memory::free_mem_transaction()
{
#if defined(MEMTEST)
    for (int i = mem_vector.len()-1; i>=0; i--){
	ivp_free_aligned(mem_vector.element_at(i));
    }
    mem_vector.clear();
#else
	//IVP_ASSERT(first_elem!=NULL); playstation doesn't like this ...
	struct p_Memory_Elem *f, *n = nullptr;
	for (f = last_elem; f; f = n){
		n = f->next;
		if(f==first_elem) {
		    break;
		}
		p_free( f);
	}
	speicherbeginn = static_cast<char*>(align_to_next_adress(&first_elem->data[0]));

	size_t ng = size_of_external_mem ? size_of_external_mem : IVU_MEMORY_BLOCK_SIZE;

	speicherende = speicherbeginn + ng;
	last_elem = first_elem;
#endif
}

#if !defined(MEMTEST)
char *IVP_U_Memory::neuer_sp_block(size_t groesse)
{
	size_t ng = IVU_MEMORY_BLOCK_SIZE - sizeof(p_Memory_Elem);
	groesse += IVU_MEM_ALIGN-1;
	groesse &= IVU_MEM_MASK;
	if (groesse > ng) ng = groesse;

	auto *memelem = static_cast<p_Memory_Elem *>(p_malloc(sizeof(p_Memory_Elem) + ng + IVU_MEM_ALIGN));
	memelem->next = last_elem;
	last_elem = memelem;
	char *tmp = static_cast<char*>(align_to_next_adress(&memelem->data[0]));
	speicherbeginn =  tmp + groesse;
	speicherende   =  tmp + ng;
	return tmp;
}
#endif

void *IVP_U_Memory::get_memc(size_t groesse)
{
//	if (groesse & 0x7) *(int *)0 = 0;
    void *neubeginn = get_mem(groesse);
    memset(neubeginn, 0, groesse);
    return neubeginn;
}

IVP_U_Memory::IVP_U_Memory(){
    init_mem();
}

void IVP_U_Memory::init_mem()
{
	transaction_in_use = 3; //for transactions: first start init_mem_transaction_usage
	size_of_external_mem = 0;
#if defined(MEMTEST)
#else	
	speicherbeginn = 0;
	speicherende = 0;
	last_elem = 0;
	first_elem=NULL;
#endif
}

void IVP_U_Memory::free_mem()
{
#if defined(MEMTEST)
    for (int i = mem_vector.len()-1; i>=0; i--){
	ivp_free_aligned(mem_vector.element_at(i));
    }
    mem_vector.clear();
#else
	struct p_Memory_Elem	*f,*n;
	for (f = last_elem; f; f = n){
		n = f->next;
		if (this->size_of_external_mem && f==first_elem) {
		    break;
		}
		P_FREE( f);
	}
	speicherbeginn = 0;
	speicherende = 0;
	last_elem = 0;
	first_elem=0;
#endif
}
