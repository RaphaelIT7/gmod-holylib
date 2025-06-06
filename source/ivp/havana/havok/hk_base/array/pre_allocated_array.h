#ifndef HK_BASE_PRE_ALLOCATED_ARRAY_H
#define HK_BASE_PRE_ALLOCATED_ARRAY_H


#ifndef HK_BASE_BASE_H
#	error Include <hk_base/base.h> instead.
#endif


// array class where the elems are allocated just behind
// the array itself
template<class T>
class hk_Pre_Allocated_Array : public hk_Array<T>
{	
	protected:

		inline hk_Pre_Allocated_Array(T* elems, int initial_size)
			:  hk_Array<T>( elems, initial_size)
		{
			;
		} 

	public:

		inline ~hk_Pre_Allocated_Array()
		{
			if ( this->get_elems() == reinterpret_cast<T*>(this+1))
			{
				this->m_elems = 0;
			}
			
		}
	HK_NEW_DELETE_FUNCTION_CLASS( hk_Pre_Allocated_Array<T>, hk_MEMORY_CLASS::HK_MEMORY_CLASS_ARRAY )		// e.g. defines new as   new (hk_Memory *)hk_Class_Name and delete var, hk_Memory *
};


#endif /* HK_PRE_ALLOCATED_ARRAY_H */

