
#include "Bootil/Bootil.h"

#if 0

#include "PackFile.h"



namespace Bootil
{

	void PackFile::Initialize( const Bootil::BString & strFileName )
	{
		m_FileName = strFileName;
		// Load.. Doesn't exist yet!
		Load( strFileName );
	}

	bool PackFile::AddFile( Bootil::BString strName, const Bootil::Buffer & data )
	{
		if ( !m_Buffer.EnsureCapacity( data.GetSize() ) ) { return false; }

		Bootil::String::Lower( strName );
		Bootil::String::File::CleanPath( strName );
		FileEntry fe;
		fe.iLength = data.GetSize();
		fe.iStartPos = m_Buffer.GetWritten();
		m_Buffer.End();
		m_Buffer.WriteBuffer( data );
		m_FileMap[ strName ] = fe;
		return true;
	}

	void PackFile::Load( const Bootil::BString & strName )
	{
		Msg( "LoadinG File!! %s\n", strName.c_str() );
		AutoBuffer buffer;

		if ( !Bootil::File::Read( strName, buffer ) )
		{
			Warning( "C0ouldn;t oipen!\n" );
			return;
		}

		buffer.SetPos( 0 );
		// Check file signature
		{
			Bootil::BString strSig = buffer.ReadString();
			Msg( "Signature [%s]\n", strSig.c_str() );

			if ( strSig != "FILEPACK001" ) { return; }
		}
		unsigned int iOffset = 0;

		while ( true )
		{
			if ( buffer.ReadType<unsigned char>() != 1 ) { break; }

			unsigned int iSize = buffer.ReadType<unsigned int>();
			Bootil::BString strName = buffer.ReadString();
			Msg( "%s %i\n", strName.c_str(), iSize );
			FileEntry fe;
			fe.iLength = iSize;
			fe.iStartPos = iOffset;
			m_FileMap[ strName ] = fe;
			iOffset += iSize;
		}

		Msg( "End Pos Files: %i\n", iOffset );
		Msg( "End Pos Actual: %i\n", buffer.GetWritten() - buffer.GetPos() );
		// The left over data goes into m_Buffer
		m_Buffer.Clear();
		m_Buffer.Write( buffer.GetCurrent(), buffer.GetWritten() - buffer.GetPos() );
	}

	void PackFile::Save()
	{
		std::ofstream Out;
		Out.open( m_FileName.c_str(), std::ios::binary | std::ios::out );

		if ( !Out.is_open() )
		{
			Warning( "Error saving file pack!\n" );
			return;
		}

		// Header
		Out.write( "FILEPACK001", 12 );
		unsigned int iOffset = 0;
		//
		// Output the filenames
		//
		BOOTIL_FOREACH( entry, m_FileMap, FileMap )
		{
			// Sig
			{
				unsigned char iSig = 1;
				Out.write( ( const char* ) &iSig, sizeof( unsigned char ) );
			}
			// Size
			{
				unsigned int iSize = entry->second.iLength;
				Out.write( ( const char* ) &iSize, sizeof( unsigned int ) );
			}
			// Name
			Out.write( entry->first.c_str(), entry->first.length() + 1 );
		}
		// Sig
		{
			unsigned char iSig = 0;
			Out.write( ( const char* ) &iSig, sizeof( unsigned char ) );
		}
		//
		// Output the data
		//
		BOOTIL_FOREACH( entry, m_FileMap, FileMap )
		{
			Out.write( ( const char* ) m_Buffer.GetBase( entry->second.iStartPos ), entry->second.iLength );
		}
		Out.close();
	}

	bool PackFile::GetFile( Bootil::BString strName, Bootil::Buffer & buffer )
	{
		Bootil::String::Lower( strName );
		Bootil::String::File::CleanPath( strName );
		FileMap::iterator it = m_FileMap.find( strName );

		if ( it == m_FileMap.end() ) { return false; }

		buffer.SetExternalBuffer( m_Buffer.GetBase( it->second.iStartPos ), it->second.iLength );
		return true;
	}

}

#endif