//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
// 
// $NoKeywords: $
//=============================================================================//
#ifndef SE_FILESYSTEM_LINUX_SUPPORT_H_
#define SE_FILESYSTEM_LINUX_SUPPORT_H_

#include <climits> // PATH_MAX define 
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "tier0/annotations.h"
#include "tier0/platform.h"

#include <sdk_backports.h>

#define FILE_ATTRIBUTE_DIRECTORY S_IFDIR

struct FIND_DATA
{
	// public data
	int dwFileAttributes;
	char cFileName[PATH_MAX]; // the file name returned from the call
	char cBaseDir[PATH_MAX]; // the root dir for this find

	int numMatches;
	int curMatch;
	dirent **namelist;  
};

#define WIN32_FIND_DATA FIND_DATA

HANDLE FindFirstFile( const char *findName, FIND_DATA *dat );
bool FindNextFile( HANDLE handle, FIND_DATA *dat );
bool FindClose( HANDLE handle );

// findFileInDirCaseInsensitive looks for the specified file. It returns
// false if no directory separator is found. Otherwise it looks for the
// requested file by scanning the specified directory and doing a case
// insensitive match. If the file exists (in any case) then the correctly cased
// filename will be returned in the user's buffer and 'true' will be returned.
// If the file does not exist then the filename will be lowercased and 'false'
// will be returned.
// dimhotepus: This function uses a thread local static buffer for internal purposes and is therefore
// thread safe.
bool findFileInDirCaseInsensitive( IN_Z const char *file, OUT_Z_BYTECAP(bufSize) char* output, size_t bufSize );
// The _safe version of this function should be preferred since it always infers
// the directory size correctly.
template<size_t bufSize>
bool findFileInDirCaseInsensitive_safe( const char *file, OUT_Z_ARRAY char (&output)[bufSize] )
{
	return findFileInDirCaseInsensitive( file, output, bufSize );
}

#endif  // !SE_FILESYSTEM_LINUX_SUPPORT_H_
