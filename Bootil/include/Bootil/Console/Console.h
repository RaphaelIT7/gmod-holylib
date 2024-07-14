#pragma once

namespace Bootil
{
	namespace Console
	{
		enum ConsoleColor
		{
			White,
			Black,
			Grey,
			Red,
			Blue,
			Green,
			Yellow
		};

		BOOTIL_EXPORT void FGColorPush( ConsoleColor col );
		BOOTIL_EXPORT void FGColorPop();

		BOOTIL_EXPORT void BGColorPush( ConsoleColor col );
		BOOTIL_EXPORT void BGColorPop();

		BOOTIL_EXPORT char WaitForKey();

		BOOTIL_EXPORT void Cls();

		BOOTIL_EXPORT void PosPush( int x, int y );
		BOOTIL_EXPORT void PosPushRelative( int x, int y );
		BOOTIL_EXPORT void PosPop();

		BOOTIL_EXPORT void SetCursorVisible( bool Visible );

		BOOTIL_EXPORT void Msg( ConsoleColor colA, ConsoleColor colB, const char* str, ... );
	}
}