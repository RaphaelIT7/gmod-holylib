#include "Bootil/Bootil.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace Bootil
{
	namespace Console
	{
		static std::stack<ConsoleColor>	s_fg;
		static std::stack<ConsoleColor>	s_bg;

		void UpdateColor()
		{
			ConsoleColor fg = Grey;
			ConsoleColor bg = Black;

			if ( !s_fg.empty() ) { fg = s_fg.top(); }

			if ( !s_bg.empty() ) { bg = s_bg.top(); }

#ifdef _WIN32
			unsigned short win_bg = 0;
			unsigned short win_fg = 0;

			switch ( fg )
			{
				case White:
					win_fg = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
					break;

				case Black:
					win_fg = 0;
					break;

				case Grey:
					win_fg = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
					break;

				case Red:
					win_fg = FOREGROUND_RED | FOREGROUND_INTENSITY;
					break;

				case Blue:
					win_fg = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
					break;

				case Green:
					win_fg = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
					break;

				case Yellow:
					win_fg = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
					break;
			}

			switch ( bg )
			{
				case White:
					win_bg = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY;
					break;

				case Black:
					win_bg = 0;
					break;

				case Grey:
					win_bg = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
					break;

				case Red:
					win_bg = BACKGROUND_RED | BACKGROUND_INTENSITY;
					break;

				case Blue:
					win_bg = BACKGROUND_BLUE | BACKGROUND_INTENSITY;
					break;

				case Green:
					win_bg = BACKGROUND_GREEN | BACKGROUND_INTENSITY;
					break;

				case Yellow:
					win_bg = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_INTENSITY;
					break;
			}

			HANDLE hcon = GetStdHandle( STD_OUTPUT_HANDLE );
			SetConsoleTextAttribute( hcon, win_bg | win_fg );
#endif
		}

		BOOTIL_EXPORT void FGColorPush( ConsoleColor col )
		{
			s_fg.push( col );
			UpdateColor();
		}

		BOOTIL_EXPORT void FGColorPop()
		{
			s_fg.pop();
			UpdateColor();
		}

		BOOTIL_EXPORT void BGColorPush( ConsoleColor col )
		{
			s_bg.push( col );
			UpdateColor();
		}

		BOOTIL_EXPORT void BGColorPop()
		{
			s_bg.pop();
			UpdateColor();
		}

		BOOTIL_EXPORT char WaitForKey()
		{
			char c;
			c = std::getchar();
			return c;
		}

		BOOTIL_EXPORT void Cls()
		{
#ifdef _WIN32
			system( "cls" );
#else
			std::cout <<"\033[2J\033[H";
#endif
		}

		//
		// Cursor Position
		//

		struct PosInfo
		{
			typedef std::stack<PosInfo> Stack;
			int x;
			int y;
		};

		static PosInfo::Stack g_PosStack;

		BOOTIL_EXPORT void PosPush( int x, int y )
		{
#ifdef _WIN32

			CONSOLE_SCREEN_BUFFER_INFO ScreenBufferInfo = { 0 };
			GetConsoleScreenBufferInfo( GetStdHandle( STD_OUTPUT_HANDLE ), &ScreenBufferInfo );

			g_PosStack.push( PosInfo() );
			g_PosStack.top().x = ScreenBufferInfo.dwCursorPosition.X;
			g_PosStack.top().y = ScreenBufferInfo.dwCursorPosition.Y;

			COORD Cord;
			Cord.X = x;
			Cord.Y = y;


			SetConsoleCursorPosition( GetStdHandle( STD_OUTPUT_HANDLE ), Cord );
#endif 
		}

		BOOTIL_EXPORT void PosPushRelative( int x, int y )
		{
#ifdef _WIN32

			CONSOLE_SCREEN_BUFFER_INFO ScreenBufferInfo = { 0 };
			GetConsoleScreenBufferInfo( GetStdHandle( STD_OUTPUT_HANDLE ), &ScreenBufferInfo );

			g_PosStack.push( PosInfo() );
			g_PosStack.top().x = ScreenBufferInfo.dwCursorPosition.X;
			g_PosStack.top().y = ScreenBufferInfo.dwCursorPosition.Y;

			COORD Cord;
			Cord.X = x + ScreenBufferInfo.dwCursorPosition.X;
			Cord.Y = y + ScreenBufferInfo.dwCursorPosition.Y;


			SetConsoleCursorPosition( GetStdHandle( STD_OUTPUT_HANDLE ), Cord );
#endif 
		}

		BOOTIL_EXPORT void PosPop()
		{
#ifdef _WIN32
			COORD Cord;
				Cord.X = g_PosStack.top().x;
				Cord.Y = g_PosStack.top().y;
			SetConsoleCursorPosition( GetStdHandle( STD_OUTPUT_HANDLE ), Cord );

			g_PosStack.pop();			
#endif 
		}

		BOOTIL_EXPORT void SetCursorVisible( bool Visible )
		{
#ifdef _WIN32 
			HANDLE consoleHandle = GetStdHandle( STD_OUTPUT_HANDLE );
			CONSOLE_CURSOR_INFO info;
			info.dwSize = 1;
			info.bVisible = Visible;
			SetConsoleCursorInfo(consoleHandle, &info);
#endif
		}

		void Msg( ConsoleColor colA, ConsoleColor colB, const char* str, ... )
		{
			FGColorPush( colA );
			BGColorPush( colB );

			BString strBuilt;
			Bootil_FormatString( strBuilt, str );
			Bootil::Output::Msg( "%s", strBuilt.c_str() );

			FGColorPop();
			BGColorPop();
		}
	}
}
