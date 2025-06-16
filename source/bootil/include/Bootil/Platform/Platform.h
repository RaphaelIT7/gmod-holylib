#pragma once
#include "Bootil/Bootil.h"

namespace Bootil
{
	namespace Platform
	{
		BOOTIL_EXPORT BString LastError();
        BOOTIL_EXPORT BString FormatSystemError( unsigned long errorid );

		BOOTIL_EXPORT BString FullProgramName( void );
		BOOTIL_EXPORT BString ProgramName( void );
		BOOTIL_EXPORT BString ProgramFolder( void );
		BOOTIL_EXPORT BString CurrentUserName( void );
		BOOTIL_EXPORT BString CurrentDir( void );
		BOOTIL_EXPORT void ChangeDir( const BString & strName );

		BOOTIL_EXPORT BString TemporaryFilename( void );
		BOOTIL_EXPORT BString TemporaryDir( void );

		BOOTIL_EXPORT void SetupAssociation( BString ext );

		BOOTIL_EXPORT unsigned int DesktopWidth( void );
		BOOTIL_EXPORT unsigned int DesktopHeight( void );

		BOOTIL_EXPORT bool Popup( const BString & strName, const BString & strText );
		BOOTIL_EXPORT void DebuggerOutput( const BString & strText );

		BOOTIL_EXPORT int FindFiles( String::List* files, String::List* folders, const BString & strFind, bool bUpUpFolders = false );

		BOOTIL_EXPORT void OpenWebpage( const BString & strURL );
		BOOTIL_EXPORT void StartProcess( const BString & strProcess, bool AndWait = true );

		BOOTIL_EXPORT void Sleep( unsigned int ms );

		BOOTIL_EXPORT BString PlatformName(); // windows/linux/osx
        BOOTIL_EXPORT BString PlatformNameShort(); // WIN/LIN/OSX
        BOOTIL_EXPORT BString Architecture(); // 32/64

        BOOTIL_EXPORT long long GetMilliseconds();

        BOOTIL_EXPORT void* LibraryLoad( const BString& name );
        BOOTIL_EXPORT void* GetFunctionAddress( void* library, const BString& name );
        BOOTIL_EXPORT void LibraryClose( void* library );

        template<typename T> T GetFunctionAddress( void* library, const BString& name )
        {
            return ( T )GetFunctionAddress( library, name );
        }

        BOOTIL_EXPORT BString GetAbsolutePath( const BString& path );

        BOOTIL_EXPORT bool IsKeyPressed();
        BOOTIL_EXPORT char GetKeyChar();

	}
}