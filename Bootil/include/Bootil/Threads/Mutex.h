#pragma once
#include "Bootil/Bootil.h"

namespace tthread
{
	class fast_mutex;
};

namespace Bootil
{
	namespace Threads
	{
		class Mutex
		{
			public:

				Mutex();
				~Mutex();

				void Lock();
				void Unlock();
				bool TryLock();

			protected:

				tthread::fast_mutex*	m;
		};

		class Guard
		{
			public:

				Guard( Mutex* m ) : m_mutex( m )
				{
					m_mutex->Lock();
				}

				~Guard()
				{
					m_mutex->Unlock();
				}

				Mutex* m_mutex;
		};
	}
}

