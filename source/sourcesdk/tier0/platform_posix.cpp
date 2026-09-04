#include <stdint.h>
#include <time.h>
#include "platform_extra.h"

uint64_t Plat_USTime()
{
	static timespec start{};
	static bool initialized = false;

	if (!initialized)
	{
		clock_gettime(CLOCK_MONOTONIC, &start);
		initialized = true;
	}

	timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	return uint64_t(now.tv_sec - start.tv_sec) * 1000000ULL + uint64_t(now.tv_nsec - start.tv_nsec) / 1000ULL;
}