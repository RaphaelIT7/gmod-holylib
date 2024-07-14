#pragma once

namespace Bootil
{
	namespace Debug
	{
		class CInstanceCounter
		{
			public:

				unsigned int iInstances;
				BString name;

				CInstanceCounter( BString strName )
				{
					name = strName;
					iInstances = 0;
				}

				~CInstanceCounter()
				{
					static bool bMoaned = false;
					Assert( iInstances >= 0 );

					if ( iInstances == 0 ) { return; }

					if ( !bMoaned )
					{
						PopupMessage( "%i unreleased instances of %s", iInstances, name.c_str() );
						Warning( "\n\nINSTANCECNT: %i unreleased instances of %s\n\n", iInstances, name.c_str() );
						bMoaned = true;
					}
					else
					{
						Msg( "INSTANCECNT: %i unreleased instances of %s\n", iInstances, name.c_str() );
					}
				}

		};
	}
}


#define InstanceCounter( strname ) static Bootil::Debug::CInstanceCounter c_##strname( #strname )
#define CountInstance( strname ) c_##strname.iInstances++
#define DiscountInstance( strname ) c_##strname.iInstances--