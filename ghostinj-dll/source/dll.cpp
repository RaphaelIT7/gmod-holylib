#include <Platform.hpp>
#include <stdio.h>
#include <filesystem>

// Realized after adding windows support that usegh was linux only... well just in case for the future I guess... :sob:
#ifdef WIN32
#include <Windows.h>
#undef GetObject
#undef GetClassName
#define DLL_Handle HMODULE
#define DLL_LoadModule(name, _) LoadLibrary(name)
#define DLL_UnloadModule(handle) FreeLibrary((DLL_Handle)handle)
#define DLL_GetAddress(handle, name) GetProcAddress((DLL_Handle)handle, name)
#define DLL_LASTERROR "LINUXONLY"
#define DLL_EXTENSION ".dll"
#else
#include <dlfcn.h>
#define DLL_Handle void*
#define DLL_LoadModule(name, type) dlopen(name, type)
#define DLL_UnloadModule(handle) dlclose(handle)
#define DLL_GetAddress(handle, name) dlsym(handle, name)
#define DLL_LASTERROR dlerror()
#define DLL_EXTENSION ".so"
#endif

#ifdef ARCHITECTURE_X86
#define HOLYLIB_FILENAME "gmsv_holylib_linux"
#else
#define HOLYLIB_FILENAME "gmsv_holylib_linux64"
#endif

void UpdateHolyLib()
{
	if ( std::filesystem::exists( "garrysmod/lua/bin/" HOLYLIB_FILENAME "_updated" DLL_EXTENSION ) )
	{
		printf( "Found a updated holylib version.\n" );
		std::filesystem::rename( "garrysmod/lua/bin/" HOLYLIB_FILENAME DLL_EXTENSION, "garrysmod/lua/bin/" HOLYLIB_FILENAME "_previous" DLL_EXTENSION );
		std::filesystem::rename( "garrysmod/lua/bin/" HOLYLIB_FILENAME "_updated" DLL_EXTENSION, "garrysmod/lua/bin/" HOLYLIB_FILENAME DLL_EXTENSION );
		printf( "Updated HolyLib\n" );
	}
}

DLL_Handle ghostinj2 = NULL;
DLL_Handle holylib = NULL;
typedef void ( *plugin_main )();
void Load()
{
	printf( "--- HolyLib-GhostInj Loading ---\n" );

	UpdateHolyLib();

	holylib = DLL_LoadModule( "garrysmod/lua/bin/" HOLYLIB_FILENAME DLL_EXTENSION, RTLD_NOW );
	if ( !holylib )
		printf( "Failed to open " HOLYLIB_FILENAME DLL_EXTENSION " (%s)\n", DLL_LASTERROR );

	plugin_main plugin = reinterpret_cast< plugin_main >( DLL_GetAddress( holylib, "HolyLib_PreLoad" ) );
	if ( !plugin ) {
		printf( "Failed to find HolyLib entry point (%s)\n", DLL_LASTERROR );
	} else {
		plugin();
	}

	ghostinj2 = DLL_LoadModule( "ghostinj2.dll", RTLD_NOW );
	if ( ghostinj2 )
		printf( "Found and loaded ghostinj2.dll\n" );

	printf( "--- HolyLib-GhostInj loaded ---\n" );
}

void Unload()
{
	printf( "--- HolyLib-GhostInj unloading ---\n" );

	if ( holylib )
		DLL_UnloadModule( holylib );

	if ( ghostinj2 )
		DLL_UnloadModule( ghostinj2 );

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