
#include <hk_base/base.h>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <array> // std::size implementation

#ifdef WIN32
#	ifdef _XBOX
#		include <xtl.h>
#	else
#		include <sal.h>
#	endif
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(
	_In_opt_ const char *lpOutputString);

#endif

hk_Console *hk_Console::m_console = HK_NULL;
hk_Console hk_Console::m_default_console_buffer;

hk_Console *hk_Console::get_instance()
{
	if ( !m_console )
	{
		m_console = &m_default_console_buffer;
	}
	return m_console;
}
#define MAX_ERROR_BUFFER_LEN 2048

void hk_Console::printf( const char *fmt, ...)
{
    va_list args;
    char buffer_tmp[MAX_ERROR_BUFFER_LEN];
    va_start(args, fmt); //-V2018 //-V2019
    vsnprintf(buffer_tmp, std::size(buffer_tmp), fmt, args);
    va_end(args);

    char buffer_out[MAX_ERROR_BUFFER_LEN];
    snprintf(buffer_out, std::size(buffer_out), "[havok] %s", buffer_tmp);

#ifdef WIN32
    OutputDebugStringA(buffer_out);
#else
    fprintf(stderr, "%s", buffer_out);
#endif
}

void hk_Console::flush()
{
#ifndef WIN32
#ifndef HK_PS2
	fflush(stderr);
#endif
#endif
}


void hk_Console::exit( int code )
{
	::exit(code); //-V2014
}
