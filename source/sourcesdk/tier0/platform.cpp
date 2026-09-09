#include <stdint.h>
#include <windows.h>
#include "platform_extra.h"

uint64_t Plat_USTime()
{
	static LARGE_INTEGER frequency{};
	static LARGE_INTEGER start{};
	static bool initialized = false;

	if (!initialized)
	{
		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&start);
		initialized = true;
	}

	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	return uint64_t((now.QuadPart - start.QuadPart) * 1000000ULL / frequency.QuadPart);
}