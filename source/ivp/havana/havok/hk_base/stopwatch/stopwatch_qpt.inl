
#if defined(HK_HAVE_MSVC_INLINE_ASSEMBLY)

#include "winlite.h"

#include <intrin.h>

#elif defined(HK_HAVE_GNU_INLINE_ASSEMBLY)

#include <x86intrin.h>

#else
#	error HK_HAVE_QUERY_PERFORMANCE_TIMER is defined, but no implementation.
#endif

inline hk_uint64 hk_query_performance_timer_start()
{
	return __rdtsc();
}

inline hk_uint64 hk_query_performance_timer_stop()
{
	unsigned aux;
	return __rdtscp(&aux);
}

inline void hk_query_performance_timer_frequency(hk_uint64* freq)
{
#ifdef _WIN32
	LARGE_INTEGER waitTime, startCount, curCount;

	// Take 1/128 of a second for the measurement.
	QueryPerformanceFrequency( &waitTime );
	unsigned scale = 7;
	waitTime.QuadPart >>= scale;

	QueryPerformanceCounter( &startCount );
	hk_uint64 startTicks = hk_query_performance_timer_start();
	do
	{
		QueryPerformanceCounter( &curCount );
	}
	while ( curCount.QuadPart - startCount.QuadPart < waitTime.QuadPart );
	hk_uint64 endTicks = hk_query_performance_timer_stop();

	*freq = (endTicks - startTicks) << scale;
	if ( *freq == 0 )
	{
		// Steam was seeing Divide-by-zero crashes on some Windows machines due to
		// WIN64_AMD_DUALCORE_TIMER_WORKAROUND that can cause rdtsc to effectively
		// stop. Staging doesn't have the workaround but I'm checking in the fix
		// anyway. Return a plausible speed and get on with our day.
		*freq = 2000000000U;
	}
#else
	// assume 2000 Mhz for now
	*freq = 2000000000U;
#endif
}

////////////////////////

class hk_Stopwatch_qpt
{
	public:

		hk_Stopwatch_qpt();

		void start();
		void stop();
		void reset();

		hk_real get_elapsed_time() const;
		hk_real get_split_time();

	private:

		hk_uint64	m_ticks_at_start;
		hk_uint64	m_ticks_at_split;
		hk_uint64	m_ticks_total;
		bool		m_running_flag;
		int			m_num_timings;

		static hk_uint64	s_ticks_per_second;
};

inline hk_Stopwatch_qpt::hk_Stopwatch_qpt()
{
	if(s_ticks_per_second==0)
		hk_query_performance_timer_frequency(&s_ticks_per_second);
	reset();
}

inline void hk_Stopwatch_qpt::start()
{
	HK_ASSERT(! m_running_flag);
	m_running_flag = true;
	m_ticks_at_start = hk_query_performance_timer_start();
	m_ticks_at_split = m_ticks_at_start;
}

inline void hk_Stopwatch_qpt::stop()
{
	HK_ASSERT(m_running_flag);

	m_running_flag = false;
	hk_uint64 ticks_now = hk_query_performance_timer_stop();
	m_ticks_total += ticks_now - m_ticks_at_start;
	++m_num_timings;
}

inline void hk_Stopwatch_qpt::reset()
{
	m_ticks_at_start = 0;
	m_ticks_at_split = 0;
	m_ticks_total = 0;
	m_running_flag = false;
	m_num_timings = 0;
}

inline hk_real hk_Stopwatch_qpt::get_elapsed_time() const
{
	return static_cast<hk_real>(m_ticks_total) / static_cast<hk_real>(s_ticks_per_second);
}

inline hk_real hk_Stopwatch_qpt::get_split_time()
{
	hk_uint64 ticks_now = hk_query_performance_timer_stop();
	hk_uint64 sticks = ticks_now - m_ticks_at_split;
	m_ticks_at_split = ticks_now;
	return static_cast<hk_real>(sticks) / static_cast<hk_real>(s_ticks_per_second);
}

