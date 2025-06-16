#include "Bootil/Bootil.h"

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#endif 

namespace Bootil
{
	namespace Console
	{
		namespace Input 
		{
			Bootil::BString					m_strLine;
			std::queue<Bootil::BString>		m_Lines;
			int								m_CaretPos;

			void Flush()
			{
				m_strLine = "";
				//m_Lines.clear();
				m_CaretPos = 0;
			}

			void ClearLine()
			{
				Console::PosPushRelative( 0, 0 );

				printf( "\n" );

				for ( int i=0; i<80-1; i++ )
					printf( " " );

				for ( int i=0; i<80; i++ )
					printf( "\b" );

				Console::PosPop();
			}

			void DrawLine()
			{
				Console::FGColorPush( Console::White );
				Console::BGColorPush( Console::Black );

				ClearLine();

				Console::PosPushRelative( 0, 0 );

				printf( "\n> " );
				

				if ( m_strLine.length() > 76 )
				{
					printf( "%s", m_strLine.substr( m_strLine.length() - 76, 76 ).c_str() );
				}
				else
				{
					printf( "%s", m_strLine.c_str() );
				}
				

				//
				// Draw our own cursor. This might be a platform dependant thing.
				//
				if ( m_CaretPos != m_strLine.length() )
				{
					for ( int i=m_strLine.length(); i>m_CaretPos; i-- )
						printf( "\b" );

					char caret = m_strLine[m_CaretPos];

					Console::FGColorPush( Console::Black );
					Console::BGColorPush( Console::Green );
						printf( "%c", caret );
					Console::BGColorPop();
					Console::FGColorPop();
				}
				

				Console::PosPop();
				Console::BGColorPop();
				Console::FGColorPop();
			}

			void OnReturn()
			{
				if ( !m_strLine.empty() )
				{
					ClearLine();
					m_Lines.push( m_strLine );
					m_strLine.clear();
				}

				m_CaretPos = 0;
			}

			void OnLeft()
			{
				m_CaretPos = Bootil::Clamp<int>( m_CaretPos - 1, 0, m_strLine.length() ); 
				DrawLine();
			}

			void OnRight()
			{
				m_CaretPos = Bootil::Clamp<int>( m_CaretPos + 1, 0, m_strLine.length() ); 
				DrawLine();
			}

			void OnBackspace()
			{
				if ( m_strLine.empty() ) return;
				if ( m_CaretPos == 0 ) return;

				m_strLine.erase( m_CaretPos-1, 1 );
				m_CaretPos--;
				DrawLine();
			}

			char Plat_GetConsoleCharNonBlocking()
			{
#ifdef _WIN32
				HANDLE stdIn = GetStdHandle( STD_INPUT_HANDLE );

				DWORD eventnum = 0;
				GetNumberOfConsoleInputEvents( stdIn, &eventnum );
				if ( eventnum <= 0 ) return 0;

				INPUT_RECORD input[ 1 ];
				ReadConsoleInput( stdIn, input, 1, &eventnum );
				if ( eventnum <= 0 ) return 0;

				if ( input[0].EventType != KEY_EVENT ) return 0;
				if ( !input[0].Event.KeyEvent.bKeyDown ) return 0;

				if ( input[0].Event.KeyEvent.wVirtualKeyCode == VK_BACK ){ OnBackspace(); return 0; }
				if ( input[0].Event.KeyEvent.wVirtualKeyCode == VK_LEFT ){ OnLeft(); return 0; }
				if ( input[0].Event.KeyEvent.wVirtualKeyCode == VK_RIGHT ){ OnRight(); return 0; }
				if ( input[0].Event.KeyEvent.wVirtualKeyCode == VK_RETURN ){ OnReturn(); return 0; }


				return input[0].Event.KeyEvent.uChar.AsciiChar;
#endif 
				return 0;
			}

			void Cycle()
			{				
				char c = Plat_GetConsoleCharNonBlocking();
				if ( c == 0 ) return;

				if ( m_CaretPos == m_strLine.length() )
				{
					m_strLine.push_back( c );
				}
				else
				{
					m_strLine.insert( m_strLine.begin() + m_CaretPos, c );
				}
				
				m_CaretPos++;

				DrawLine();
				Cycle();
			}

			BString GetLine()
			{
				Cycle();

				if ( m_Lines.size() == 0 ) 
					return "";

				BString str = m_Lines.front();
				m_Lines.pop();
				return str;
			}

			BString GetLineInProgress()
			{
				return m_strLine;
			}

			void PreOutput()
			{
				if ( m_strLine.empty() ) return;
				 ClearLine();
			}

			void PostOutput()
			{
				if ( m_strLine.empty() ) return;
				DrawLine();
			}
		}

	}
}
