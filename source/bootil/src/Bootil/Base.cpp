#include "Bootil/Bootil.h"

namespace Bootil
{
	static bool g_bShuttingDown = false;

	BOOTIL_EXPORT void Startup()
	{
		g_bShuttingDown = false;
	}

	BOOTIL_EXPORT void Shutdown()
	{
		g_bShuttingDown = true;
	}

	BOOTIL_EXPORT bool IsShuttingDown()
	{
		return g_bShuttingDown;
	}
}