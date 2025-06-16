#include "Bootil/Bootil.h"

namespace Bootil
{
	namespace String
	{
		namespace Sanitize
		{
			BOOTIL_EXPORT void Javascript( BString & str )
			{
				Util::FindAndReplace( str, "\\", "\\\\" );
				Util::FindAndReplace( str, "\"", "\\\"" );
				Util::FindAndReplace( str, "\n", "\\n" );
				Util::FindAndReplace( str, "\r", "\\r" );
			}

			BOOTIL_EXPORT void Filename( BString & str )
			{
				Util::FindAndReplace( str, "\\", "" );
				Util::FindAndReplace( str, "/", "" );
				Util::FindAndReplace( str, "\n", "" );
				Util::FindAndReplace( str, "\r", "" );
				Util::FindAndReplace( str, "\"", "" );
				Util::FindAndReplace( str, "'", "" );
				Util::FindAndReplace( str, ":", "" );
				Util::FindAndReplace( str, "<", "" );
				Util::FindAndReplace( str, ">", "" );
				Util::FindAndReplace( str, "*", "" );
				Util::FindAndReplace( str, "?", "" );
				Util::FindAndReplace( str, "|", "" );
				Util::FindAndReplace( str, "@", "" );
				Util::FindAndReplace( str, " ", "_" );
				Util::FindAndReplace( str, ",", "_" );
				Util::FindAndReplace( str, "!", "_" );
			}
		}
	}
}
