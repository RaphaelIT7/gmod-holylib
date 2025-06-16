
#ifdef __linux__

//#define X11_GRAPHICAL // Don't use X11 code on a server, that's just stupid

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "Bootil/Bootil.h"
#include <libgen.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/wait.h>

#ifdef X11_GRAPHICAL
#include <X11/Xlib.h>
#endif

extern char* program_invocation_name;
extern char* program_invocation_short_name;

int wildcmp( const char* wild, const char* string )
{
	// Written by Jack Handy - jakkhandy@hotmail.com
	const char* cp = NULL, *mp = NULL;

	while ( ( *string ) && ( *wild != '*' ) )
	{
		if ( ( *wild != *string ) && ( *wild != '?' ) )
		{
			return 0;
		}

		wild++;
		string++;
	}

	while ( *string )
	{
		if ( *wild == '*' )
		{
			if ( !*++wild )
			{
				return 1;
			}

			mp = wild;
			cp = string + 1;
		}
		else if ( ( *wild == *string ) || ( *wild == '?' ) )
		{
			wild++;
			string++;
		}
		else
		{
			wild = mp;
			string = cp++;
		}
	}

	while ( *wild == '*' )
	{
		wild++;
	}

	return !*wild;
}

namespace Bootil
{
	namespace Platform
	{
		BOOTIL_EXPORT BString LastError()
		{
			return strerror( errno );
		}

        BOOTIL_EXPORT BString FormatSystemError( unsigned long errorid )
        {
            return strerror( ( int )errorid );
        }

		BString FullProgramName( void )
		{
			return program_invocation_name;
		}

		BString ProgramName( void )
		{
			return program_invocation_short_name;
		}

		BString ProgramFolder( void )
		{
			BString strFolder = FullProgramName();
			String::File::StripFilename( strFolder );
			return strFolder;
		}

		BString CurrentUserName( void )
		{
			passwd* pw;
			uid_t uid;
			uid = geteuid();
			pw = getpwuid( uid );

			if ( pw )
			{
				return pw->pw_name;
			}
			else
			{ return "<UNKNOWN>"; }
		}

		BOOTIL_EXPORT void SetupAssociation( BString ext )
		{
		}

		BOOTIL_EXPORT unsigned int DesktopWidth( void )
		{
#ifdef X11_GRAPHICAL
			Display* display = XOpenDisplay( NULL );
			int screen = DefaultScreen( display );
			int width = XDisplayWidth( display, screen );
			XCloseDisplay( display );
			return width;
#else
			return 800;
#endif
		}

		BOOTIL_EXPORT unsigned int DesktopHeight( void )
		{
#ifdef X11_GRAPHICAL
			Display* display = XOpenDisplay( NULL );
			int screen = DefaultScreen( display );
			int height = XDisplayHeight( display, screen );
			XCloseDisplay( display );
			return height;
#else
			return 600;
#endif
		}

		BOOTIL_EXPORT bool Popup( const BString & strName, const BString & strText )
		{
			return true;
		}

		BOOTIL_EXPORT void DebuggerOutput( const BString & strText )
		{
			// there is not a separate debug stream on linux
			// printf( "%s", strText.c_str() );
		}

		BOOTIL_EXPORT int FindFiles( String::List* files, String::List* folders, const BString & strFind, bool bUpUpFolders )
		{
			BString dirName = strdup( strFind.c_str() );
			dirName = dirname( ( char* )dirName.c_str() );
			BString findName = strdup( strFind.c_str() );
			findName = basename( ( char* )findName.c_str() );
			DIR* dp;
			dirent* dirp;
			int iFiles = 0;

			if ( ( dp = opendir( dirName.c_str() ) ) == NULL )
			{ return 0; }

			while ( ( dirp = readdir( dp ) ) != NULL )
			{
				BString name( dirp->d_name );
				BString fullName = dirName + "/" + name;

				if ( wildcmp( findName.c_str(), name.c_str() ) )
				{
					if ( Bootil::File::IsFolder( fullName ) && folders )
					{
						if ( bUpUpFolders || ( name != "." && name != ".." ) )
						{
							folders->push_back( name );
							iFiles++;
						}
					}
					else if ( files )
					{
						files->push_back( name );
						iFiles++;
					}
				}
			}

			closedir( dp );
			return iFiles;
		}

		BOOTIL_EXPORT void OpenWebpage( const BString & strURL )
		{
			pid_t pid;
			const char* args[2];
			args[0] = strURL.c_str();
			args[1] = NULL;
			pid = fork();

			if ( pid == 0 )
			{ execvp( "/usr/bin/xdg-open", ( char * const* )args ); }
		}

		BOOTIL_EXPORT void StartProcess( const BString & strProcess, bool AndWait )
		{
			pid_t pid = fork();

			if ( pid != 0 )
			{
				bool isOk = ( pid > 0 );

				if ( isOk && AndWait )
				{
					int status;
					wait( &status );
				}
			}
			else
			{
				const char* args[2];
				args[0] = strProcess.c_str();
				args[1] = NULL;
				execvp( strProcess.c_str(), ( char * const* )args );
			}
		}

		BOOTIL_EXPORT BString TemporaryDir( void )
		{
			return "/tmp";
		}

		BOOTIL_EXPORT BString TemporaryFilename( void )
		{
			char buffer[ L_tmpnam ];
			return Bootil::String::File::GetFixSlashes( TemporaryDir() + tmpnam( buffer ) );
		}

		BOOTIL_EXPORT void Sleep( unsigned int ms )
		{
			::usleep( ms * 1000 );
		}

		BOOTIL_EXPORT BString CurrentDir( void )
		{
			char strCurrentWD[4096];
			getcwd( strCurrentWD, sizeof( strCurrentWD ) );

			return strCurrentWD;
		}

		BOOTIL_EXPORT BString PlatformName()
		{
			return "linux";
		}

        BOOTIL_EXPORT BString PlatformNameShort()
        {
            return "LIN";
        }

        BOOTIL_EXPORT BString Architecture()
        {
            #if __x86_64__ || __ppc64__
            return "64";
            #else 
            return "32";
            #endif
        }

		BOOTIL_EXPORT long long GetMilliseconds()
		{
			static time_t   startSeconds = 0;

			struct timeval  timecurrent;
			gettimeofday( &timecurrent, NULL );

			if ( !startSeconds )
			{
				startSeconds = timecurrent.tv_sec;
				return ( timecurrent.tv_usec / 1000.0 );
			}

			return (long long)((( timecurrent.tv_sec - startSeconds ) * 1000.0) + (timecurrent.tv_usec / 1000.0) );
		}

        BOOTIL_EXPORT void* LibraryLoad( const BString& name )
        {
            return NULL;
        }

        BOOTIL_EXPORT void* GetFunctionAddress( void* library, const BString& name )
        {
            return NULL;
        }

        BOOTIL_EXPORT void LibraryClose( void* library )
        {

        }

        BOOTIL_EXPORT BString GetAbsolutePath( const BString& path )
        {
            return path;
        }

        BOOTIL_EXPORT bool IsKeyPressed()
        {
            struct timeval tv;
            fd_set read_fd;

            tv.tv_sec = 0;
            tv.tv_usec = 0;
            FD_ZERO( &read_fd );
            FD_SET( 0, &read_fd );

            if ( select( 1, &read_fd, NULL, NULL, &tv ) == -1 )
                return false;

            if ( FD_ISSET( 0, &read_fd ) )
                return true;

            return false;
        }

        BOOTIL_EXPORT char GetKeyChar()
        {
            return getchar();
        }
	}
}

#endif
