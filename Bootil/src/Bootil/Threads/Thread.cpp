
#include "Bootil/Bootil.h"
#include "tinythreadpp/tinythread.h"
#include "tinythreadpp/fast_mutex.h"

#if defined(_TTHREAD_POSIX_)
#include <unistd.h>
#include <map>
#elif defined(_TTHREAD_WIN32_)
#include <process.h>
#endif

namespace Bootil
{
	namespace Threads
	{
		void Thread::RunInThread( void* aArg )
		{
			Bootil::Debug::Crash::SetupCallback();

			Thread* pThread = ( Thread* ) aArg;

			pThread->Lock();
			pThread->m_bClosing = false;
			pThread->m_bRunning = true;
			pThread->Unlock();

			try 
			{
				pThread->Run();
			}
			catch ( ... )
			{

			}

			pThread->Lock();
			pThread->m_bRunning = false;
			pThread->Unlock();

			//
			// you can use this function to delete this; probably.
			//
			pThread->OnThreadFinished();
		}

		Thread::Thread()
		{
			m_pThread = NULL;
			m_bRunning = false;
		}

		Thread::~Thread()
		{
			// Thread should have been destroyed at this point
			BAssert( m_pThread == NULL );
			BAssert( m_bRunning == false );
		}

		bool Thread::StartInThread()
		{
			if ( m_pThread ) { Join(); }

			m_bRunning = true;
			m_bClosing = false;

			m_pThread = new tthread::thread( RunInThread, this );
			return true;
		}

		bool Thread::Running() { Threads::Guard m( &m_Mutex ); return m_bRunning; }
		void Thread::Lock() { m_Mutex.Lock(); }
		void Thread::Unlock() { m_Mutex.Unlock(); }
		void Thread::TryLock() { m_Mutex.TryLock(); }

		bool Thread::WantsToClose() { Threads::Guard m( &m_Mutex ); return m_bClosing; }
		void Thread::SetClosing( bool b ) { Threads::Guard m( &m_Mutex ); m_bClosing = b; }

		void Thread::Join()
		{
			if ( !m_pThread )
				return;

			SetClosing( true );

			if ( tthread::this_thread::get_id() == m_pThread->get_id() )
			{
				m_pThread->detach();
			}
			else 
			{
				m_pThread->join();
			}
			
			delete m_pThread;
			m_pThread = NULL;

			m_bRunning = false;
		}

#ifdef _WIN32 
		unsigned int WINAPI RunFunctionThenDestroyThread( void* aArg )
		{
			Thread* pThread = (Thread*) aArg;
			pThread->Run();
			delete pThread;
			return 0;
		}

		BOOTIL_EXPORT void Thread::StartInThreadAndDestroy()
		{
			HANDLE mHandle = (HANDLE) _beginthreadex( 0, 0, RunFunctionThenDestroyThread, (void *) this, 0, 0 );
			CloseHandle( mHandle );
		}
#else 
		void* RunFunctionThenDestroyThread( void* aArg )
		{
			Thread* pThread = (Thread*) aArg;
			pThread->Run();
			delete pThread;
			return 0;
		}

		BOOTIL_EXPORT void Thread::StartInThreadAndDestroy()
		{
			pthread_t mHandle = 0;

			if ( pthread_create( &mHandle, NULL, RunFunctionThenDestroyThread, (void *) this) )
			{
				pthread_detach( mHandle );
			}
		}
#endif 



	}
}
