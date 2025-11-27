//HOLYLIB_REQUIRES_MODULE=gameevent
#include "sourcesdk/GameEventManager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CGameEventDescriptor *CGameEventManager::GetEventDescriptor(const char * name)
{
	if ( !name || !name[0] )
		return nullptr;

	for (int i=0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor *descriptor = &m_GameEvents[i];

		if ( Q_strcmp( descriptor->name, name ) == 0 )
			return descriptor;
	}

	return nullptr;
}

CGameEventDescriptor *CGameEventManager::GetEventDescriptor(int eventid) // returns event name or nullptr
{
	if ( eventid < 0 )
		return nullptr;

	for ( int i = 0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor *descriptor = &m_GameEvents[i];

		if ( descriptor->eventid == eventid )
			return descriptor;
	}

	return nullptr;
}

CGameEventCallback* CGameEventManager::FindEventListener( void* pCallback )
{
	for (int i=0; i < m_Listeners.Count(); i++ )
	{
		CGameEventCallback *listener = m_Listeners.Element(i);

		if ( listener->m_pCallback == pCallback )
		{
			return listener;
		}
	}

	return nullptr;
}

bool CGameEventManager::AddListener( void *listener, CGameEventDescriptor *descriptor,  int nListenerType )
{
	if ( !listener || !descriptor )
		return false;	// bahh

	// check if we already know this listener
	CGameEventCallback *pCallback = FindEventListener( listener );

	if ( pCallback == nullptr )
	{
		// add new callback 
		pCallback = new CGameEventCallback;
		m_Listeners.AddToTail( pCallback );

		pCallback->m_nListenerType = nListenerType;
		pCallback->m_pCallback = listener;
	}
	else
	{
		// make sure that it hasn't changed:
		Assert( pCallback->m_nListenerType == nListenerType	 );
		Assert( pCallback->m_pCallback == listener );
	}

	// add to event listeners list if not already in there
	if ( descriptor->listeners.Find( pCallback ) == descriptor->listeners.InvalidIndex() )
	{
		descriptor->listeners.AddToTail( pCallback );

		if ( nListenerType == CLIENTSIDE || nListenerType == CLIENTSIDE_OLD )
			m_bClientListenersChanged = true;
	}

	return true;
}