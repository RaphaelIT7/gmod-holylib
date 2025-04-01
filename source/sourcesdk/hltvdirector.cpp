#include "player.h"
#include "sourcesdk/hltvdirector.h"
#include "util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void CHLTVDirector::RemoveEventsFromHistory(int tick)
{
	int index = m_EventHistory.FirstInorder();

	while ( index != m_EventHistory.InvalidIndex() )
	{
		CHLTVGameEvent &dc = m_EventHistory[index];

		if ( (dc.m_Tick < tick) || (tick == -1) )
		{
			Util::gameeventmanager->FreeEvent( dc.m_Event );
			dc.m_Event = NULL;
			m_EventHistory.RemoveAt( index );
			index = m_EventHistory.FirstInorder();	// start again
		}
		else
		{
			index = m_EventHistory.NextInorder( index );
		}
	}

#ifdef _DEBUG
	CheckHistory();
#endif
}