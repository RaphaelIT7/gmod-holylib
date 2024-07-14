
#include "Bootil/Bootil.h"
#include <sys/stat.h>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace Bootil
{
	namespace File
	{
		BOOTIL_EXPORT BString RelativeToAbsolute( const BString & strIn )
		{
			return strIn;
		}

		BOOTIL_EXPORT int Find( String::List* files, String::List* folders, const BString & strFind, bool bUpUpFolders )
		{
			return Platform::FindFiles( files, folders, strFind, bUpUpFolders );
		}

		BOOTIL_EXPORT int Size( const BString & strFileName )
		{
			struct stat fileStat;
			int err = stat( strFileName.c_str(), &fileStat );

			if ( err != 0 ) { return 0; }

			return fileStat.st_size;
		}

		BOOTIL_EXPORT int Exists( const BString & strFileName )
		{
			struct stat fileStat;
			int err = stat( strFileName.c_str(), &fileStat );

			if ( err != 0 ) { return false; }

			return true;
		}

		BOOTIL_EXPORT bool Read( const BString & strFileName, Bootil::Buffer & bufferOut )
		{
			std::ifstream f( strFileName.c_str(), std::ios_base::in | std::ios::ate | std::ios::binary );

			if ( !f.is_open() ) { return false; }

			std::streamsize size = f.tellg();

			if ( !bufferOut.EnsureCapacity( bufferOut.GetPos() + size ) )
			{ return false; }

			f.seekg( 0, std::ios::beg );
			f.read( reinterpret_cast<char*>( bufferOut.GetCurrent() ), size );
			f.close();
			bufferOut.SetWritten( bufferOut.GetPos() + size );
			bufferOut.SetPos( bufferOut.GetPos() + size );
			return true;
		}

		BOOTIL_EXPORT bool Read( const BString & strFileName, BString & strOut )
		{
			std::ifstream f( strFileName.c_str(), std::ios_base::in );

			if ( !f.is_open() ) { return false; }

			strOut = BString( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
			return true;
		}

		BOOTIL_EXPORT bool Write( const BString & strFileName, const Bootil::Buffer & bufferOut )
		{
			std::ofstream f( strFileName.c_str(), std::ios_base::out | std::ios::binary );

			if ( !f.is_open() ) { return false; }

			f.write( reinterpret_cast<char*>( bufferOut.GetBase() ), bufferOut.GetWritten() );
			f.close();
			return true;
		}

		BOOTIL_EXPORT bool Write( const BString & strFileName, const BString & strOut )
		{
			std::ofstream f( strFileName.c_str(), std::ios_base::out );

			if ( !f.is_open() ) { return false; }

			f.write( strOut.c_str(), strOut.length() );
			f.close();
			return true;
		}

		BOOTIL_EXPORT bool Append( const BString & strFileName, const BString & strOut )
		{
			std::ofstream f( strFileName.c_str(), std::ios_base::out | std::ios_base::app );
			if ( !f.is_open() ) { return false; }

			f.write( strOut.c_str(), strOut.length() );
			f.close();
			return true;
		}


		BOOTIL_EXPORT unsigned long CRC( const BString & strFileName )
		{
			Bootil::AutoBuffer buffer;

			if ( !File::Read( strFileName, buffer ) )
			{ return 0; }

			return Hasher::CRC32::Easy( buffer.GetBase(), buffer.GetWritten() );
		}

		BOOTIL_EXPORT bool IsFolder( const BString & strFileName )
		{
			struct stat fileStat;
			int err = stat( strFileName.c_str(), &fileStat );

			if ( err != 0 ) { return false; }

			return ( fileStat.st_mode & S_IFMT ) == S_IFDIR;
		}

		BOOTIL_EXPORT int GetFilesInFolder( const BString & strFolder, String::List & OutputFiles, bool Recursive )
		{
			String::List files;
			String::List folders;
			File::Find( &files, &folders, strFolder + "/*", false );
			BOOTIL_FOREACH_CONST( f, files, String::List )
			{
				OutputFiles.push_back( *f );
			}

			if ( !Recursive ) { return  OutputFiles.size(); }

			BOOTIL_FOREACH_CONST( f, folders, String::List )
			{
				files.clear();
				GetFilesInFolder( strFolder + "/" + *f, files, true );
				BOOTIL_FOREACH_CONST( file, files, String::List )
				{
					OutputFiles.push_back( *f + "/" + *file );
				}
			}
			return OutputFiles.size();
		}

		BOOTIL_EXPORT bool CreateFolder( const BString & strFileName, bool bRecursive )
		{
			// Fix slashes, trim last slash
			BString strFixed = Bootil::String::File::GetFixSlashes( strFileName );
			Bootil::String::Util::TrimAfter( strFixed, "/\\" );

			// We have reached the peak, bail!
			if ( strFileName.empty() )
			{ return false; }

			// Folder exists, don't do anything
			if ( IsFolder( strFixed ) )
			{ return false; }

			//
			// If recursive then we'll create the parent folder first
			//
			if ( bRecursive )
			{
				CreateFolder( Bootil::String::File::GetUpOneDirectory( strFixed ), bRecursive );
			}

			//
			// Actually create the folder
			//
#ifdef _WIN32
			return 0 == mkdir( strFixed.c_str() );
#else
			return 0 == mkdir( strFixed.c_str(), S_IRWXU | S_IRWXG | S_IRWXO );
#endif
		}

		BOOTIL_EXPORT bool RemoveFolder( const BString & strFolder, bool bAndFilesRecursive )
		{
			if ( !IsFolder( strFolder ) ) { return false; }

			if ( bAndFilesRecursive )
			{
				String::List files;
				String::List folders;
				File::Find( &files, &folders, strFolder + "/*", false );
				BOOTIL_FOREACH_CONST( f, files, String::List )
				{
					if ( !RemoveFile( strFolder + "/" + *f ) )
					{ return false; }
				}
				BOOTIL_FOREACH_CONST( f, folders, String::List )
				{
					if ( !RemoveFolder( strFolder + "/" + *f, true ) )
					{ return false; }
				}
			}

			rmdir( strFolder.c_str() );
			return true;
		}

		BOOTIL_EXPORT bool RemoveFile( const BString & strFileName )
		{
			return 0 == unlink( strFileName.c_str() );
		}

		BOOTIL_EXPORT bool Copy( const BString & strFrom, const BString & strTo )
		{
			std::ifstream f1( strFrom.c_str(), std::fstream::binary );

			if ( !f1.is_open() ) { return false; }

			std::ofstream f2( strTo.c_str(), std::fstream::trunc | std::fstream::binary );

			if ( !f2.is_open() ) { return false; }

			f2 << f1.rdbuf();
			return true;
		}

		BOOTIL_EXPORT BString GetTempDir()
		{
			BString strName = std::getenv( "TEMP" );
			BAssert( strName.length() > 2 );
			String::File::FixSlashes( strName );
			String::Util::Trim( strName, "/" );
			return strName;
		}

		BOOTIL_EXPORT BString GetTempFilename()
		{
			BString fn = GetTempDir();
			char cTempName[L_tmpnam];
			tmpnam( cTempName );
			BString name = cTempName;
			String::Util::Trim( name, "/\\." );
			name = fn + "/" + name;
			return name;
		}
	}

}
