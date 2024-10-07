#include "cbase.h"
#include "usermessages.h"
#include <bitbuf.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

int CUserMessages::LookupUserMessage( const char *name )
{
	int idx = m_UserMessages.Find( name );
	if ( idx == m_UserMessages.InvalidIndex() )
	{
		DevMsg(1, "holylib: Failed to find anything. FK.\n");
		FOR_EACH_DICT( m_UserMessages, i )
		{
			DevMsg(1, "holylib: Entry: %s, %i\n", m_UserMessages[i]->name, i);
		}

		return -1;
	}

	return idx;
}