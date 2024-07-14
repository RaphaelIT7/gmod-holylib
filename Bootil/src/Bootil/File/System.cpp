
#include "Bootil/Bootil.h"


namespace Bootil
{
	BOOTIL_EXPORT File::System FileSystem( "" );

	namespace File
	{
		System::System()
		{
		}

		System::System( BString strInitalPath )
		{
			AddPath( strInitalPath );
		}

		void System::AddPath( BString strPath )
		{
			// Clean Path
			strPath = File::RelativeToAbsolute( strPath );
			String::Util::TrimRight( strPath, "/" );
			String::Util::TrimRight( strPath, "\\" );
			m_Paths.push_back( strPath + "/" );
		}


	}
}