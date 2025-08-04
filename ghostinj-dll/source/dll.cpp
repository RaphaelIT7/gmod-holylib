#include <Platform.hpp>
#include <stdio.h>
#include <filesystem>

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

void* ghostinj2 = NULL;
void* holylib = NULL;
typedef void ( *plugin_main )();
void Load()
{
	printf( "--- HolyLib-GhostInj Loading ---\n" );

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
		printf( "Failed to find HolyLib entry point (%s)\n", dlerror() );
	} else {
		plugin();
	}

	ghostinj2 = dlopen( "ghostinj2.dll", RTLD_NOW );
	if ( ghostinj2 )
		printf( "Found and loaded ghostinj2.dll\n" );

	printf( "--- HolyLib-GhostInj loaded ---\n" );
}

void Unload()
{
	printf( "--- HolyLib-GhostInj unloading ---\n" );

	if ( holylib )
		dlclose( holylib );

	if ( ghostinj2 )
		dlclose( ghostinj2 );

	printf( "--- HolyLib-GhostInj unloaded ---\n" );
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