#pragma once

namespace Bootil
{
	namespace Process
	{
		BOOTIL_EXPORT void* FindNamed( const Bootil::BString& strName );

		BOOTIL_EXPORT void* Start( const Bootil::BString& strName, const Bootil::BString& strParam = "", bool bShow = true );
		BOOTIL_EXPORT void Terminate( void* pHandle );

		BOOTIL_EXPORT void CloseHandle( void* pHandle );
	}
}