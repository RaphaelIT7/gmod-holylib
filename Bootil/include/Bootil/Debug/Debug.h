#pragma once

namespace Bootil
{
	namespace Debug
	{
		class IListener
		{
			public:

				typedef std::list< IListener* > List;
				static BOOTIL_EXPORT void Add( IListener* listener );
				static BOOTIL_EXPORT void Remove( IListener* listener );

			public:

				virtual void Msg( const char* ) {};
				virtual void Warning( const char* ) {};
				virtual void Error( const char* ) {};
		};

		BOOTIL_EXPORT BString LastError();
		BOOTIL_EXPORT void PopupMessage( const char* str, ... );

		//
		// By default calls to Error will pop up an error message
		// This behavior isn't ideal for some apps, so disable using this.
		//
		BOOTIL_EXPORT void SuppressPopups( bool bSuppress );

		namespace Internal
		{
			BOOTIL_EXPORT void DoAssert( const char* strFile, unsigned int iLine, const char* strFunction, const char* strModule, const char* format, ... );
		}

		namespace Crash
		{
			typedef void (*MiniDumpFunction)( unsigned int nExceptionCode, void *pException );

			BOOTIL_EXPORT void SetMinidumpFunction( MiniDumpFunction func );
			BOOTIL_EXPORT void SetupCallback();

			BOOTIL_EXPORT void DoCrash();

		}

	}

	namespace Output
	{
		BOOTIL_EXPORT void Error( const char* str, ... );
		BOOTIL_EXPORT void Msg( const char* str, ... );
		BOOTIL_EXPORT void Warning( const char* str, ... );
	}



}

#if !defined(_M_X64) && defined(_WIN32) && defined(_DEBUG)
#define BreakInDebugger() _asm { int 3 }
#else
#define BreakInDebugger() // Nothing
#endif

#define BAssert( _something_ )				\
if ( !(bool)(_something_) )					\
{											\
	Bootil::Debug::Internal::DoAssert( __FILE__, __LINE__, __FUNCTION__, "","%s", #_something_ );		\
	BreakInDebugger();\
}
