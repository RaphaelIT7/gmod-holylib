
#include "Bootil/Bootil.h"
#ifdef _WIN32 
#include <windows.h>
#include <tlhelp32.h>
#endif 

namespace Bootil
{
	namespace Process
	{
		BOOTIL_EXPORT void* FindNamed( const Bootil::BString& strName )
		{
			#ifdef _WIN32
			HANDLE snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );

			PROCESSENTRY32 process;
				memset( &process, 0, sizeof(process) );
				process.dwSize = sizeof(process);

			if ( Process32First(snapshot, &process) )
			{
				do
				{
					if ( strName == process.szExeFile )
					{
						void* pReturn = OpenProcess( PROCESS_ALL_ACCESS, FALSE, process.th32ProcessID );
						::CloseHandle( snapshot );
						return pReturn;
					}

				} while ( Process32Next(snapshot, &process) );
			}

			::CloseHandle( snapshot );

			#endif 

			return NULL;
		}

		BOOTIL_EXPORT void CloseHandle( void* pHandle )
		{
			#ifdef _WIN32
			::CloseHandle( pHandle );
			#endif
		}

		BOOTIL_EXPORT void* Start( const Bootil::BString& strName, const Bootil::BString& strParam, bool bShow )
		{
			#ifdef _WIN32
			SHELLEXECUTEINFO shExecInfo;
				shExecInfo.cbSize			= sizeof(SHELLEXECUTEINFO);
				shExecInfo.fMask			= SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
				shExecInfo.hwnd				= NULL;
				shExecInfo.lpVerb			= NULL;
				shExecInfo.lpFile			= strName.c_str();
				shExecInfo.lpParameters		= strParam.c_str();
				shExecInfo.lpDirectory		= NULL;
				shExecInfo.nShow			= bShow ? SW_SHOWNORMAL : SW_HIDE;
				shExecInfo.hInstApp			= NULL;

			if ( ShellExecuteEx( &shExecInfo ) )
			{
				WaitForInputIdle( shExecInfo.hProcess, INFINITE );
				return shExecInfo.hProcess;
			}
			#endif

			return NULL;
		}

		BOOTIL_EXPORT void Terminate( void* pHandle )
		{
			#ifdef _WIN32
			TerminateProcess( pHandle, 0 );
			#endif
		}
	}

}