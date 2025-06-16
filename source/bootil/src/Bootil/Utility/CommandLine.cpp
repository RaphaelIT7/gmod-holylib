
#include "Bootil/Bootil.h"

namespace Bootil
{
	namespace CommandLine
	{
		String::List	g_ArgList;
		BString			g_Full;

		BString GetFull() { return g_Full; }

		void AddArgument( const BString & str )
		{
			g_ArgList.push_back( str );
		}

		BOOTIL_EXPORT void Set( int argc, char* argv[] )
		{
			for ( int i = 1; i < argc; i++ )
			{
				AddArgument( argv[i] );
				g_Full = g_Full + argv[i] + " ";
			}
		}

		BOOTIL_EXPORT int GetArgCount()
		{
			return g_ArgList.size();
		}

		BOOTIL_EXPORT BString GetArg( int iNum, const BString & strDefault )
		{
			if ( iNum < 0 || iNum >= GetArgCount() ) { return strDefault; }

			return g_ArgList[ iNum ];
		}

		BOOTIL_EXPORT BString GetSwitch( const BString & strName, const BString & strDefault )
		{
			bool bFound = false;
			BOOTIL_FOREACH_CONST( arg, g_ArgList, String::List )
			{
				if ( bFound )
				{ return *arg; }

				if ( *arg == strName )
				{ bFound = true; }
			}
			return strDefault;
		}

		BOOTIL_EXPORT bool HasSwitch( const BString & strName )
		{
			BOOTIL_FOREACH_CONST( arg, g_ArgList, String::List )
			{
				if ( *arg == strName )
					return true;
			}

			return false;
		}
	}

}