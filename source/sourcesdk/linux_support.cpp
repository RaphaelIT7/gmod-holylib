//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $  
//=============================================================================//

#include "linux_support.h"

#include <sys/stat.h> // stat()
#include <unistd.h> 
#include <dirent.h> // scandir()

#include <cctype>
#include <cstdlib>
#include <cstdio>

#include "tier1/strtools.h"

namespace 
{

// dimhotepus: Make thread_local to support multiple threads.
thread_local char selectBuf[PATH_MAX];

int FileSelect( const dirent *ent )
{
	const char *mask{selectBuf}, *name{ent->d_name};

	if (V_streq(name, ".") || V_streq(name, "..") ) return 0;
	if (V_streq(selectBuf, "*.*")) return 1;

	while (*mask && *name)
	{
		if (*mask == '*')
		{
			mask++; // move to the next char in the mask

			// if this is the end of the mask its a match
			if (!*mask) return 1;

			// dimhotepus: toupper -> V_toupper.
			// while the two don't meet up again
			while (*name && V_toupper(*name) != V_toupper(*mask)) 
			{
				name++;
			}

			// end of the name
			if (!*name) break; 
		}
		else if (*mask != '?')
		{
			// dimhotepus: toupper -> V_toupper.
			// mismatched!
			if (V_toupper(*mask) != V_toupper(*name)) return 0;

			mask++;
			name++;

			// if its at the end of the buffer
			if (!*mask && !*name) return 1;
		}
		else /* mask is "?", we don't care*/
		{
			mask++;
			name++;
		}
	}

	// both of the strings are at the end
	return !*mask && !*name;
}

int FillDataStruct( FIND_DATA *dat )
{
	if (dat->curMatch >= dat->numMatches)
		return -1;

	auto *name = dat->namelist[dat->curMatch];

	char szFullPath[MAX_PATH];
	V_sprintf_safe( szFullPath, "%s/%s", dat->cBaseDir, name->d_name );  

	struct stat fileStat;
	dat->dwFileAttributes = !stat( szFullPath, &fileStat ) ? fileStat.st_mode : 0;           

	// now just put the filename in the output data
	V_strcpy_safe( dat->cFileName, name->d_name );

	free( name );

	++dat->curMatch;
	return 1;
}

}  // namespace

HANDLE FindFirstFile( const char *fileName, FIND_DATA *dat )
{
	char nameStore[PATH_MAX];
	char *dir = nullptr;
	int iret=-1;
	
	V_strcpy_safe(nameStore, fileName );

	if (strrchr(nameStore, '/'))
	{
		dir = nameStore;
		while (strrchr(dir, '/'))
		{
			// zero this with the dir name
			dir = strrchr(nameStore, '/');
			if (dir == nameStore) // special case for root dir, '/'
			{
				dir[1] = '\0';
			}
			else
			{
				*dir='\0';
				dir = nameStore;
			}
			
			struct stat dirChk;
			if (stat(dir, &dirChk) < 0)
			{
				continue;
			}

			if (S_ISDIR(dirChk.st_mode))
			{
				break;	
			}
		}
	}
	else
	{
		// couldn't find a dir seperator...
		return (HANDLE)-1;
	}

	if (!Q_isempty(dir))
	{
		// dimhotepus: Speedup root dir computation.
		const bool isRoot = !Q_isempty(dir) && dir[0] == '/' && dir[1] == '\0';
		V_strcpy_safe(selectBuf, fileName + (isRoot ? 0 : 1) + 1);

		dat->namelist = nullptr;
		V_strcpy_safe(dat->cBaseDir, dir);

		const int n = scandir(dir, &dat->namelist, FileSelect, alphasort);
		if ( n < 0 )
		{
			if ( dat->namelist )
			{
				free(dat->namelist);
				// dimhotepus: Set to nullptr to prefent UAF.
				dat->namelist = nullptr;
			}
			// silently return, nothing interesting
		}
		else 
		{
			dat->numMatches = n;
			dat->curMatch = 0;

			iret = FillDataStruct(dat);
			if ( iret < 0 )
			{
				if ( dat->namelist )
				{
					free(dat->namelist);
					dat->namelist = nullptr;
				}
			}
		}
	}

	return (HANDLE)iret;
}

bool FindNextFile( HANDLE handle, FIND_DATA *dat )
{
	if ( dat->curMatch >= dat->numMatches )
	{	
		if ( dat->namelist != nullptr )
		{
			free( dat->namelist );
			dat->namelist = nullptr;
		}

		return false; // no matches left
	}

	// dimhotepus: Report false if fill failed.
	return FillDataStruct( dat ) > 0;
}

bool FindClose( HANDLE handle )
{
	return true;
}



// Pass this function a full path and it will look for files in the specified
// directory that match the file name but potentially with different case.
// The directory name itself is not treated specially.
// If multiple names that match are found then lowercase letters take precedence.
bool findFileInDirCaseInsensitive( IN_Z const char *file, OUT_Z_BYTECAP(bufSize) char* output, size_t bufSize )
{
	// Make sure the output buffer is always null-terminated.
	if (bufSize > 0)
		output[0] = '\0';

	// Find where the file part starts.
	const char *dirSep = strrchr(file,'/');
	if ( !dirSep )
	{
		dirSep = strrchr(file, '\\');
		if ( !dirSep ) 
		{
			return false;
		}
	}

	// Allocate space for the directory portion.
	size_t dirSize = (dirSep - file) + 1;
	char *dirName = stackallocT( char, dirSize ); 

	V_strncpy( dirName, file, dirSize );

	DIR* pDir = opendir( dirName );
	if ( !pDir )
		return false;

	RunCodeAtScopeExit(closedir( pDir ));

	const char* filePart = dirSep + 1;
	// The best matching file name will be placed in this array.
	char outputFileName[MAX_PATH];
	bool foundMatch = false;

	// Scan through the directory.
	for ( dirent* pEntry = nullptr; pEntry = readdir( pDir ); /**/ )
	{
		if ( V_strieq( pEntry->d_name, filePart ) )
		{
			// If we don't have an existing candidate or if this name is
			// a better candidate then copy it in. A 'better' candidate
			// means that test beats tesT which beats tEst -- more lowercase
			// letters earlier equals victory.
			if ( !foundMatch || V_strcmp( outputFileName, pEntry->d_name ) < 0 )
			{
				foundMatch = true;
				V_strcpy_safe( outputFileName, pEntry->d_name );
			}
		}
	}

	// If we didn't find any matching names then lowercase the passed in
	// file name and use that.
	if ( !foundMatch )
	{
		V_strcpy_safe( outputFileName, filePart );
		V_strlower( outputFileName );
	}

	Q_snprintf( output, bufSize, "%s/%s", dirName, outputFileName );
	return foundMatch;
}
