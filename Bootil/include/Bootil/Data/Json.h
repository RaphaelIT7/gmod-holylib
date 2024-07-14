#pragma once

namespace Bootil
{
	namespace Data
	{
		namespace Json
		{
			BOOTIL_EXPORT bool Export( const Bootil::Data::Tree & tree, Bootil::BString & output, bool bPretty = false );
			BOOTIL_EXPORT bool Import( Bootil::Data::Tree & tree, const char* strInput );
		}
	}
}
