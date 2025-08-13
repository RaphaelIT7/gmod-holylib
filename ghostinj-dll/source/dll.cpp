#include <Platform.hpp>
#include <stdio.h>
#include <filesystem>

extern "C" {
#include "../lua/lua.h"
#include "../lua/lauxlib.h"
#include "../lua/lualib.h"
#include "../lua/luajit.h"
}


#if SYSTEM_WINDOWS
#ifndef DLFCN_H
	#define DLFCN_H

	#ifdef __cplusplus
	extern "C" {
	#endif

	#if defined(DLFCN_WIN32_SHARED)
	#if defined(DLFCN_WIN32_EXPORTS)
	#   define DLFCN_EXPORT __declspec(dllexport)
	#else
	#   define DLFCN_EXPORT __declspec(dllimport)
	#endif
	#else
	#   define DLFCN_EXPORT
	#endif

	#define RTLD_LAZY	0x00001
	#define RTLD_NOW	0x00002
	#define	RTLD_BINDING_MASK   0x3
	#define RTLD_NOLOAD	0x00004
	#define RTLD_DEEPBIND	0x00008	

	typedef struct dl_info
	{
		const char* dli_fname;
		void* dli_fbase;
		const char* dli_sname;
		void* dli_saddr;
	} Dl_info;

	DLFCN_EXPORT void* dlopen( const char* file, int mode );
	DLFCN_EXPORT int dlclose( void* handle );
	DLFCN_EXPORT void* dlsym( void* handle, const char* name );
	DLFCN_EXPORT char* dlerror( void );
	DLFCN_EXPORT int dladdr( const void* addr, Dl_info* info );

	#ifdef __cplusplus
	}
	#endif

	int usleep( int a ) { 
		return a;
	}
#endif /* DLFCN_H */
#else
#include <dlfcn.h>
#endif

static void TestJIT()
{
	lua_State* state = luaL_newstate();
	luaL_openlibs(state);
	//luaJIT_setmode(state, 0, 0);

	std::string code = "jit.on() local num = 1.7976931348623e308 for i = 1, 9999 do if 1 / num > 0 then --[[print(true, 1 / num)]] else error(\"wtf\") end end error(\"worked\")";

	printf("holylib: Loading jit thing\n");
	if (luaL_loadbuffer(state, code.c_str(), code.length(), "Test") == 0)
	{
		printf("holylib: calling jit thing\n");
		if (lua_pcall(state, 0, 0, 0) != 0)
		{
			printf("holylib: lua error: %s\n", lua_tostring(state, -1));
		} else {
			printf("holylib: Called jit thing\n");
		}
	} else {
		printf("holylib: lua buf error: %s\n", lua_tostring(state, -1));
	}

	printf("holylib: Closing lua state\n");
	lua_close(state);
}

void* ghostinj2 = NULL;
void* holylib = NULL;
typedef void ( *plugin_main )();
void Load()
{
	printf( "--- Holylib-GhostInj Loading ---\n" );

#if SYSTEM_LINUX && ARCHITECTURE_X86
	{
		uint16_t cw;
		__asm__ __volatile__ ("fnstcw %0" : "=m" (cw));
		printf("Current FPU control word: 0x%04x\n", cw);
	}
#endif

	TestJIT();

#ifdef ARCHITECTURE_X86
	if ( std::filesystem::exists( "garrysmod/lua/bin/gmsv_holylib_linux_updated.so" ) )
	{
		printf( "Found a updated holylib version.\n" );
		if ( std::filesystem::remove( "garrysmod/lua/bin/gmsv_holylib_linux.so" ) )
		{
			std::filesystem::rename( "garrysmod/lua/bin/gmsv_holylib_linux_updated.so", "garrysmod/lua/bin/gmsv_holylib_linux.so" );
			printf( "Updated HolyLib\n" );
		} else {
			printf( "Failed to delete old HolyLib version!\n" );
		}
	}

	holylib = dlopen( "garrysmod/lua/bin/gmsv_holylib_linux.so", RTLD_NOW );
	if ( !holylib )
		printf( "Failed to open gmsv_holylib_linux.so (%s)\n", dlerror() );
#else
	if ( std::filesystem::exists( "garrysmod/lua/bin/gmsv_holylib_linux64_updated.so" ) )
	{
		printf( "Found a updated holylib version.\n" );
		if ( std::filesystem::remove( "garrysmod/lua/bin/gmsv_holylib_linux64.so" ) )
		{
			std::filesystem::rename( "garrysmod/lua/bin/gmsv_holylib_linux64_updated.so", "garrysmod/lua/bin/gmsv_holylib_linux64.so" );
			printf( "Updated HolyLib\n" );
		} else {
			printf( "Failed to delete old HolyLib version!\n" );
		}
	}

	holylib = dlopen( "garrysmod/lua/bin/gmsv_holylib_linux64.so", RTLD_NOW );
	if ( !holylib )
		printf( "Failed to open gmsv_holylib_linux64.so (%s)\n", dlerror() );
#endif

	plugin_main plugin = reinterpret_cast< plugin_main >( dlsym( holylib, "HolyLib_PreLoad" ) );
	if ( !plugin ) {
		printf( "Failed to find holylib entry point (%s)\n", dlerror() );
	} else {
		plugin();
	}

	ghostinj2 = dlopen( "ghostinj2.dll", RTLD_NOW );
	if ( ghostinj2 )
		printf( "Found and loaded ghostinj2.dll\n" );

	TestJIT();

#if SYSTEM_LINUX && ARCHITECTURE_X86
	{
		uint16_t cw;
		__asm__ __volatile__ ("fnstcw %0" : "=m" (cw));
		printf("Final FPU control word: 0x%04x\n", cw);
	}
#endif

	printf( "--- Holylib-GhostInj loaded ---\n" );
}

void Unload()
{
	printf( "--- Holylib-GhostInj unloading ---\n" );

	if ( holylib )
		dlclose( holylib );

	if ( ghostinj2 )
		dlclose( ghostinj2 );

	printf( "--- Holylib-GhostInj unloaded ---\n" );
}

#if SYSTEM_WINDOWS
#include <windows.h>
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
	switch( fdwReason ) 
	{ 
		case DLL_PROCESS_ATTACH:
			Load();
			break;
		case DLL_PROCESS_DETACH:
			if ( lpvReserved )
				break;

			Unload();
			break;
	}

	return TRUE;
}
#else
void __attribute__((constructor)) SO_load()
{
	Load();
}

void __attribute__((destructor)) SO_unload()
{
	Unload();
}
#endif