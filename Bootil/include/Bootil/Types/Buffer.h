#pragma once

namespace Bootil
{
	class BOOTIL_EXPORT Buffer
	{
		public:

			Buffer( void* pData, int iSize );
			virtual ~Buffer();

			void SetExternalBuffer( void* pData, int iSize );

			void Write( const void* pData, unsigned int iSize );
			void WriteBuffer( const Buffer & buffer );

			unsigned int Read( void* pData, int iSize );

			unsigned int	GetSize() const;

			unsigned int	GetPos() const;
			bool			SetPos( unsigned int iPos );
			void			Start() { SetPos( 0 ); }
			void			End() { SetPos( GetWritten() ); }

			void			SetWritten( unsigned int iSize );
			unsigned int	GetWritten() const;
			unsigned int	GetRemaining() const { return GetWritten() - GetPos(); }

			void*			GetBase( unsigned int iOffset = 0 ) const;

			void*			GetCurrent();

			//
			// Resets position and written to 0
			//
			virtual void	Clear();

			bool			Forward( unsigned int iAmt ) { return SetPos( GetPos() + iAmt ); }
			bool			Backward( unsigned int iAmt ) { return SetPos( GetPos() - iAmt ); }

			virtual bool	EnsureCapacity( unsigned int iSize );

			// Move a block of the buffer memory around inside.
			void			MoveMem( unsigned int iSrcPos, unsigned int iSrcSize, unsigned int iToPos );

			// Crop a block of memory from the front of the buffer. This translates pos and written pos too.
			void			TrimLeft( unsigned int iTrimAmount );

			//
			// Read/Write a null terminated string
			//
			int WriteString( const Bootil::BString & str, bool addNull = true );
			Bootil::BString ReadString();

			template <typename T>
			int WriteType( const T & var )
			{
				Write( ( void* )&var, sizeof( T ) );
				return sizeof( T );
			}

			template <typename T>
			T ReadType()
			{
				T var;
				Read( ( void* )&var, sizeof( T ) );
				return var;
			}



		protected:

			Buffer();

			void*				m_pData;
			unsigned int		m_iSize;
			unsigned int		m_iPos;
			unsigned int		m_iWritten;
	};

	class BOOTIL_EXPORT AutoBuffer : public Buffer
	{
		public:

			AutoBuffer( int iInitialSize = 32 );
			virtual ~AutoBuffer();

			//
			// Clears everything, frees the data, sets everything to 0.
			//
			virtual void	Clear();

			virtual bool	EnsureCapacity( unsigned int iSize );

	};
}
