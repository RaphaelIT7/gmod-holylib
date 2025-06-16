
#include "Bootil/Bootil.h"
#include "tinythreadpp/fast_mutex.h"



namespace Bootil
{
	namespace Threads
	{
		Mutex::Mutex()
		{
			m = new tthread::fast_mutex;
		}

		Mutex::~Mutex()
		{
			delete m;
			m = NULL;
		}

		void Mutex::Lock()
		{
			m->lock();
		}

		void Mutex::Unlock()
		{
			m->unlock();
		}

		bool Mutex::TryLock()
		{
			return m->try_lock();
		}
	}
}
