#include "Bootil/Bootil.h"

#ifdef _WIN32 
#include <excpt.h>
#endif 

namespace Bootil
{
	namespace Debug
	{
		IListener::List & Listeners()
		{
			static IListener::List Listeners;
			return Listeners;
		}

		void IListener::Add( IListener* listener )
		{
			Listeners().push_back( listener );
		}

		void IListener::Remove( IListener* listener )
		{
			Listeners().remove( listener );
		}

		static BString	s_strError				= "";
		static bool		s_bSuppressPopups		= false;

		BOOTIL_EXPORT void SuppressPopups( bool bSuppress )
		{
			s_bSuppressPopups = bSuppress;
		}

		BOOTIL_EXPORT BString LastError()
		{
			return s_strError;
		}

		namespace Internal
		{
			BOOTIL_EXPORT void DoAssert( const char* strFile, unsigned int iLine, const char* strFunction, const char* strModule, const char* format, ... )
			{
				BString strBuilt;
				Bootil_FormatString( strBuilt, format );
				BString strError = String::Format::Print( "ASSERT ASSERT ASSERT\nMessage: %s\nModule:\t%s\nFile:\t%s\nLine:\t%i\nFunction:\t%s\n", strBuilt.c_str(), strModule, strFile, iLine, strFunction );
				Output::Warning( "%s", strError.c_str() );

#if _DEBUG 
				PopupMessage( "%s", strError.c_str() );
#endif 
			}
		}


		BOOTIL_EXPORT void PopupMessage( const char* str, ... )
		{
			BString strBuilt;
			Bootil_FormatString( strBuilt, str );
			Output::Warning( strBuilt.c_str() );
			printf( "%s", strBuilt.c_str() );

			//
			// Pop up a message unless SuppressPopups was set
			//
			if ( !s_bSuppressPopups )
			{
				Bootil::Platform::Popup( "Bootil", strBuilt );
			}
		}

		namespace Crash
		{
			static MiniDumpFunction g_MinidumpFunction = NULL;

			BOOTIL_EXPORT void SetMinidumpFunction( MiniDumpFunction func )
			{
				g_MinidumpFunction = func;
			}

			void FunctionCalledOnCrash( unsigned int nExceptionCode, void *pException )
			{
				if ( g_MinidumpFunction ) g_MinidumpFunction( nExceptionCode, pException );
			}

			BOOTIL_EXPORT void SetupCallback()
			{
#ifdef _WIN32 
				_set_se_translator( (_se_translator_function) FunctionCalledOnCrash );
#endif 
			}

#if _WIN32 
			int ForcedCrash( unsigned int nExceptionCode, void *pException )
			{
				FunctionCalledOnCrash( nExceptionCode, pException );
				return EXCEPTION_EXECUTE_HANDLER;
			}
#endif 

			BOOTIL_EXPORT void DoCrash()
			{
				//
				// Not sure of the portability of this
				//
				#ifdef _WIN32
				__try 
				{
					* (int *) 0 = 0;
				} 
				__except( ForcedCrash( GetExceptionCode(), GetExceptionInformation() ) )
				{
					// Nothing Here.				
				}
				#endif

				exit( -1 );
			}
		}

	}

	struct StoredMessage
	{
		BString strMsg;
		bool bWarning;
	};

	namespace Output
	{
		//-----------------------------------------------------------------------------
		//
		//-----------------------------------------------------------------------------
		BOOTIL_EXPORT void Error( const char* str, ... )
		{
			BString strBuilt;
			Bootil_FormatString( strBuilt, str );
			Debug::s_strError = strBuilt;

			for ( Debug::IListener::List::iterator i = Debug::Listeners().begin(); i != Debug::Listeners().end(); i++ )
			{
				( *i )->Error( strBuilt.c_str() );
			}

			Console::FGColorPush( Console::Black );
			Console::BGColorPush( Console::Red );
			Msg( "Error:\n\n" );
			Msg( "%s", strBuilt.c_str() );
			Msg( "\n\n" );
			Console::FGColorPop();
			Console::BGColorPop();

			if ( !Debug::s_bSuppressPopups )
			{
				Bootil::Platform::Popup( "Error", strBuilt );
			}

#ifdef _WIN32
			//	_asm { int 3 }
#endif
			exit( 0 );
		}


		//-----------------------------------------------------------------------------
		//
		//-----------------------------------------------------------------------------
		BOOTIL_EXPORT void Warning( const char* str, ... )
		{
			BString strBuilt;
			Bootil_FormatString( strBuilt, str );
			Console::FGColorPush( Console::Red );
			Output::Msg( "%s", strBuilt.c_str() );
			Console::FGColorPop();

			for ( Debug::IListener::List::iterator i = Debug::Listeners().begin(); i != Debug::Listeners().end(); i++ )
			{
				( *i )->Warning( strBuilt.c_str() );
			}
		}


		//-----------------------------------------------------------------------------
		//
		//-----------------------------------------------------------------------------
		BOOTIL_EXPORT void Msg( const char* str, ... )
		{
			Bootil::Console::Input::PreOutput();

			BString strBuilt;
			Bootil_FormatString( strBuilt, str );
			printf( "%s", strBuilt.c_str() );
			Platform::DebuggerOutput( strBuilt );

			Bootil::Console::Input::PostOutput();

			if ( Bootil::IsShuttingDown() )
			{ return; }

			for ( Debug::IListener::List::iterator i = Debug::Listeners().begin(); i != Debug::Listeners().end(); i++ )
			{
				( *i )->Msg( strBuilt.c_str() );
			}
		}

	}

}
