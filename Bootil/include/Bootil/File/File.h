#pragma once

namespace Bootil
{
	namespace File
	{
		BOOTIL_EXPORT BString RelativeToAbsolute( const BString & strIn );


		BOOTIL_EXPORT int Find( String::List* files, String::List* folders, const BString & strFind, bool bUpUpFolders = false );
		BOOTIL_EXPORT int GetFilesInFolder( const BString & strFolder, String::List & files, bool Recursive );

		BOOTIL_EXPORT int					Exists( const BString & strFileName );
		BOOTIL_EXPORT unsigned long			CRC( const BString & strFileName );
		BOOTIL_EXPORT int					Size( const BString & strFileName );
		BOOTIL_EXPORT bool					Read( const BString & strFileName, Bootil::Buffer & bufferOut );
		BOOTIL_EXPORT bool					Read( const BString & strFileName, BString & strOut );
		BOOTIL_EXPORT bool					Write( const BString & strFileName, const Bootil::Buffer & bufferOut );
		BOOTIL_EXPORT bool					Write( const BString & strFileName, const BString & strOut );
		BOOTIL_EXPORT bool					Append( const BString & strFileName, const BString & strOut );

		BOOTIL_EXPORT bool					IsFolder( const BString & strFileName );
		BOOTIL_EXPORT bool					CreateFolder( const BString & strFileName, bool bRecursive = false );
		BOOTIL_EXPORT bool					RemoveFolder( const BString & strFileName, bool bAndFilesRecursive = false );

		BOOTIL_EXPORT bool					RemoveFile( const BString & strFileName );

		BOOTIL_EXPORT bool					Copy( const BString & strFrom, const BString & strTo );

		BOOTIL_EXPORT BString				GetTempDir();
		BOOTIL_EXPORT BString				GetTempFilename();

	}
}