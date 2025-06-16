
#include "Bootil/Bootil.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif



namespace Bootil
{
	namespace Threads
	{
		BOOTIL_EXPORT unsigned long long CurrentThreadID()
		{
#ifdef _WIN32
			return GetCurrentThreadId();
#else
			return ( unsigned long long ) pthread_self();
#endif
		}
	}
}
