#pragma once
#include "Bootil/Bootil.h"

namespace Bootil
{
	namespace Threads
	{
		BOOTIL_EXPORT unsigned long long CurrentThreadID();

		template <class T>
		class MutexVar
		{
			public:

				T Get()
				{
					T var;
					m_mutex.Lock();
					var = m_var;
					m_mutex.Unlock();

					return var;
				}

				void Set( T var )
				{
					m_mutex.Lock();
					m_var = var;
					m_mutex.Unlock();
				}

			protected:

				T m_var;
				Threads::Mutex	m_mutex;
		};
	}
}

