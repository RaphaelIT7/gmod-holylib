#pragma once

namespace Bootil
{
#if 0
	class PackFile
	{
		public:

			void Initialize();

			bool AddFile( Bootil::BString strName, const Bootil::Buffer & data );

			//void Save();
			//void Load( const Bootil::BString& strFileName );

			bool GetFile( Bootil::BString strName, Bootil::Buffer & buffer );

		protected:

			Bootil::AutoBuffer	m_Buffer;

			struct FileEntry
			{
				int iStartPos;
				int iLength;
			};

			typedef std::map< Bootil::BString, FileEntry > FileMap;

			FileMap	m_FileMap;
	};
#endif

}