#pragma once

#ifdef BOOTIL_COMPILE_DLL
#ifdef _WIN32
#if defined(__GNUC__)
#define BOOTIL_EXPORT __attribute__((dllexport))
#else
#define BOOTIL_EXPORT __declspec(dllexport)
#endif
#else
#define BOOTIL_EXPORT
#endif
#else

#define BOOTIL_EXPORT
#endif


// Standards
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>

// STL
#include <string>
#include <vector>
#include <list>
#include <map>
#include <stack>
#include <sstream>
#include <queue>
#include <set>
#include <algorithm>
#include <fstream>
#include <iostream>


//
// Forward declarations
//
#include "Bootil/Base.h"

#include "Bootil/Types/Buffer.h"
#include "Bootil/Types/Math.h"
#include "Bootil/Types/String.h"
#include "Bootil/Debug/Debug.h"
#include "Bootil/Platform/Platform.h"
#include "Bootil/Utility/STLUtility.h"
#include "Bootil/Utility/Hasher.h"
#include "Bootil/Utility/CommandLine.h"
#include "Bootil/Utility/Compression.h"
#include "Bootil/Utility/Time.h"
#include "Bootil/Utility/Processes.h"

#include "Bootil/Threads/Mutex.h"
#include "Bootil/Threads/Utility.h"
#include "Bootil/Threads/Thread.h"

#include "Bootil/File/File.h"
#include "Bootil/File/System.h"
#include "Bootil/File/Changes.h"

#include "Bootil/Data/Tree.h"
#include "Bootil/Data/Json.h"

#include "Bootil/Image/Image.h"
#include "Bootil/Image/JPEG.h"
#include "Bootil/Image/PNG.h"

#include "Bootil/Console/Console.h"
#include "Bootil/Console/ConsoleInput.h"

#include "Bootil/Network/Network.h"
#include "Bootil/Network/Socket.h"
#include "Bootil/Network/Router.h"
#include "Bootil/Network/HTTP.h"

namespace Bootil
{
	BOOTIL_EXPORT void Startup();
	BOOTIL_EXPORT void Shutdown();
	BOOTIL_EXPORT bool IsShuttingDown();
}

namespace Bootil
{
	template<typename T> void SafeDelete( T* & Ptr )
	{
		delete Ptr;
		Ptr = NULL;
	}

	template<typename T> void SafeRelease( T* & Ptr )
	{
		if ( !Ptr ) { return; }

		Ptr->Release();
		Ptr = NULL;
	}

	template <typename T> T Min( T a, T b )
	{
		return a < b ? a : b;
	}

	template <typename T> T Max( T a, T b )
	{
		return a > b ? a : b;
	}

	template <typename T> T Clamp( T val, T min, T max )
	{
		if ( val < min ) return min;
		if ( val > max ) return max;
		return val;
	}
	

}
