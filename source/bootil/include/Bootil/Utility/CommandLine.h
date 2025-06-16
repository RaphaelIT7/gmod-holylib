#pragma once

namespace Bootil
{
	namespace CommandLine
	{
		BOOTIL_EXPORT void Set( int argc, char* argv[] );

		BOOTIL_EXPORT BString GetFull();
		BOOTIL_EXPORT int GetArgCount();

		BOOTIL_EXPORT BString GetArg( int iNum, const BString & strDefault = "" );
		BOOTIL_EXPORT BString GetSwitch( const BString & strArgName, const BString & strDefault = "" );

		BOOTIL_EXPORT bool HasSwitch( const BString & strArgName );

	}

}