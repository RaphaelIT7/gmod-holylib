#include "netmessages.h"
#include "bitbuf.h"
#include "const.h"
#include "net_chan.h"
#include "mathlib/mathlib.h"
#include "tier0/vprof.h"
#include "net.h"
#include <common.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

INetMessage *CNetChan::FindMessage(int type)
{
	int numtypes = m_NetMessages.Count();

	for (int i=0; i<numtypes; i++ )
	{
		if ( m_NetMessages[i]->GetType() == type )
			return m_NetMessages[i];
	}

	return NULL;
}

bool CNetChan::IsFileInWaitingList( const char *filename )
{
	if ( !filename || !filename[0] )
		return true;

	for ( int stream=0; stream<MAX_STREAMS; stream++)
	{
		for ( int i = 0; i < m_WaitingList[stream].Count(); i++ )
		{
			dataFragments_t * data = m_WaitingList[stream][i]; 

			if ( !Q_strcmp( data->filename, filename ) )
				return true; // alread in list
		}
	}

	return false; // file not found
}

CNetChan::subChannel_s *CNetChan::GetFreeSubChannel()
{
	for ( int i=0; i<MAX_SUBCHANNELS; i++ )
	{
		if ( m_SubChannels[i].state == SUBCHANNEL_FREE )
			return &m_SubChannels[i];
	}
	return NULL;
}