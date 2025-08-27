#define _ALLOWVERSIONFILE
#include "_versioninfo.h"
#undef _ALLOWVERSIONFILE

// This file will always have a cache miss, so we make this as simple as possible to not become a slow down in the compile.

const char* HolyLib_GetPluginDescription()
{
#if !HOLYLIB_BUILD_RELEASE // DATA should always fallback to 0. We will set it to 1 in releases.
	return "HolyLib Serverplugin V0.8-pre DEV (Workflow: " HOLYLIB_BUILD_RUN_NUMBER " - Branch: " HOLYLIB_BUILD_BRANCH ")";
#else
	return "HolyLib Serverplugin V0.8-pre";
#endif
}

const char* HolyLib_GetVersion()
{
	return "0.8-pre";
}

const char* HolyLib_GetRunNumber()
{
	return HOLYLIB_BUILD_RUN_NUMBER;
}

const char* HolyLib_GetBranch()
{
	return HOLYLIB_BUILD_BRANCH;
}