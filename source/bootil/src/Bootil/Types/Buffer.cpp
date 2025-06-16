#include "Bootil/Bootil.h"

bool globber( const char* wild, const char* string );

namespace Bootil
{
	Buffer::Buffer( void* pData, int iSize )
	{
		SetExternalBuffer( pData, iSize );
	}

	Buffer::Buffer()
	{
		m_pData		= NULL;
		m_iSize		= 0;
		m_iWritten	= 0;
		m_iPos		= 0;
	}

	Buffer::~Buffer()
	{
	}

	void Buffer::SetExternalBuffer( void* pData, int iSize )
	{
		Clear();
		m_pData		= pData;
		m_iSize		= iSize;
		m_iPos		= 0;
		m_iWritten	= iSize;
	}

	void Buffer::Write( const void* pData, unsigned int iSize )
	{
		if ( iSize == 0 ) { return; }

		if ( !EnsureCapacity( m_iPos + iSize ) ) { return; }

		memcpy( GetCurrent(), pData, iSize );
		m_iPos += iSize;
		m_iWritten = Bootil::Max( m_iWritten, m_iPos );
	}

	void Buffer::WriteBuffer( const Buffer & buffer )
	{
		Write( buffer.GetBase(), buffer.GetWritten() );
	}

	unsigned int Buffer::Read( void* pData, int iSize )
	{
		if ( m_iPos + iSize > m_iSize ) { return 0; }

		if ( iSize == 0 ) { return 0; }

		memcpy( pData, GetCurrent(), iSize );
		m_iPos += iSize;
		return iSize;
	}

	bool Buffer::EnsureCapacity( unsigned int iSize )
	{
		return iSize < m_iSize;
	}

	void Buffer::Clear()
	{
		m_iWritten	= 0;
		m_iPos		= 0;
	}

	unsigned int	Buffer::GetSize() const { return m_iSize; }
	unsigned int	Buffer::GetPos() const { return m_iPos; }
	void*			Buffer::GetBase( unsigned int iOffset ) const { return ( char* )m_pData + iOffset; }
	void*			Buffer::GetCurrent() { return ( ( char* )m_pData ) + m_iPos; }
	bool			Buffer::SetPos( unsigned int iPos ) { m_iPos = iPos; return true; }
	void			Buffer::SetWritten( unsigned int iWritten ) { m_iWritten = iWritten; }
	unsigned int	Buffer::GetWritten() const { return m_iWritten; }


	AutoBuffer::AutoBuffer( int iInitialSize )
	{
		m_pData		= NULL;
		m_iSize		= 0;
		m_iWritten	= 0;
		m_iPos		= 0;

		EnsureCapacity( iInitialSize );
	}

	AutoBuffer::~AutoBuffer()
	{
		if ( m_pData )
		{
			free( m_pData );
			m_pData = NULL;
		}
	}

	void AutoBuffer::Clear()
	{
		m_iWritten	= 0;
		m_iPos		= 0;
		m_iSize		= 0;

		if ( m_pData )
		{
			free( m_pData );
			m_pData = NULL;
		}
	}

	bool AutoBuffer::EnsureCapacity( unsigned int iSize )
	{
		if ( iSize <= m_iSize ) { return true; }

		//
		// More than 500mb - we're probably doing something wrong - right??
		//
		//if ( iSize > 536870912 ) { return false; }

		if ( !m_pData )
		{
			m_pData = malloc( iSize );
			if ( !m_pData )
			{
				return false;
			}
		}
		else
		{
			void* pData = realloc( m_pData, iSize );

			if ( pData )
			{
				m_pData = pData;
			}
			else
			{
				pData = malloc( iSize );

				if ( !pData ) { return false; }

				memcpy( pData, m_pData, m_iSize );
				free( m_pData );
				m_pData = pData;
			}

			if ( !m_pData )
			{
				return false;
			}
		}

		m_iSize = iSize;
		return true;
	}

	int Buffer::WriteString( const Bootil::BString & str, bool addNull )
	{
		int iWritten = 0;

		for ( int i = 0; i < str.length(); i++ )
		{
			WriteType<char>( str[i] );
			iWritten++;
		}

		if ( addNull )
		{
			WriteType<char>( 0 );
			iWritten++;
		}

		return iWritten;
	}

	Bootil::BString Buffer::ReadString()
	{
		Bootil::BString str;

		while ( true )
		{
			if ( m_iPos + sizeof(char) > m_iSize )
				break;

			char c = ReadType<char>();

			if ( c == 0 ) { break; }

			str += c;
		}

		return str;
	}


	void Buffer::MoveMem( unsigned int iSrcPos, unsigned int iSrcSize, unsigned int iToPos )
	{
		if ( iSrcSize == 0 ) return;

		EnsureCapacity( iToPos + iSrcSize );
		memmove( GetBase( iToPos ), GetBase( iSrcPos ), iSrcSize );
	}

	void Buffer::TrimLeft( unsigned int iTrimAmount )
	{
		if ( iTrimAmount == 0 ) return;
		if ( iTrimAmount > GetWritten() ) iTrimAmount = GetWritten();

		MoveMem( iTrimAmount, GetWritten() - iTrimAmount, 0 );
		if ( m_iPos > iTrimAmount ) m_iPos -= iTrimAmount; else m_iPos = 0;
		m_iWritten	-= iTrimAmount;
	}

}
