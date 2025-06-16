#pragma once

namespace Bootil
{
	namespace File
	{
		class BOOTIL_EXPORT System
		{
			public:

				typedef std::list<BString> Paths;

				System();
				System( BString strInitalPath );

				void AddPath( BString strPath );

			private:

				File::System::Paths	m_Paths;

		};

	}

	extern BOOTIL_EXPORT File::System FileSystem;
}
