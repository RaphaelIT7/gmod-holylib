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

static char s_text[1024];

bool SVC_UserMessage::WriteToBuffer( bf_write &buffer )
{
	m_nLength = m_DataOut.GetNumBitsWritten();

	Assert( m_nLength < (1 << NETMSG_LENGTH_BITS) );
	if ( m_nLength >= (1 << NETMSG_LENGTH_BITS) )
		return false;

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteByte( m_nMsgType );
	buffer.WriteUBitLong( m_nLength, NETMSG_LENGTH_BITS );  // max 256 * 8 bits, see MAX_USER_MSG_DATA

	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool SVC_UserMessage::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_UserMessage::ReadFromBuffer" );
	m_nMsgType = buffer.ReadByte();
	m_nLength = buffer.ReadUBitLong( NETMSG_LENGTH_BITS ); // max 256 * 8 bits, see MAX_USER_MSG_DATA
	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_UserMessage::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: type %i, bytes %i", GetName(), m_nMsgType, Bits2Bytes(m_nLength) );
	return s_text;
}

bool SVC_VoiceData::WriteToBuffer(bf_write& buffer)
{
	buffer.WriteUBitLong(GetType(), NETMSG_TYPE_BITS);
	buffer.WriteByte(m_nFromClient);
	buffer.WriteByte(m_bProximity);
	buffer.WriteWord(m_nLength);

	if (IsX360())
	{
		buffer.WriteLongLong(m_xuid);
	}

	return buffer.WriteBits(m_DataOut, m_nLength);
}

bool SVC_VoiceData::ReadFromBuffer(bf_read& buffer)
{
	VPROF("SVC_VoiceData::ReadFromBuffer");

	m_nFromClient = buffer.ReadByte();
	m_bProximity = !!buffer.ReadByte();
	m_nLength = buffer.ReadWord();

	if (IsX360())
	{
		m_xuid = buffer.ReadLongLong();
	}


	m_DataIn = buffer;
	return buffer.SeekRelative(m_nLength);
}

const char* SVC_VoiceData::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: client %i, bytes %i", GetName(), m_nFromClient, Bits2Bytes(m_nLength));
	return s_text;
}

bool SVC_GameEvent::WriteToBuffer(bf_write& buffer)
{
	m_nLength = m_DataOut.GetNumBitsWritten();
	Assert(m_nLength < (1 << NETMSG_LENGTH_BITS));
	if (m_nLength >= (1 << NETMSG_LENGTH_BITS))
		return false;

	buffer.WriteUBitLong(GetType(), NETMSG_TYPE_BITS);
	buffer.WriteUBitLong(m_nLength, NETMSG_LENGTH_BITS);  // max 8 * 256 bits
	return buffer.WriteBits(m_DataOut.GetData(), m_nLength);

}

bool SVC_GameEvent::ReadFromBuffer(bf_read& buffer)
{
	VPROF("SVC_GameEvent::ReadFromBuffer");

	m_nLength = buffer.ReadUBitLong(NETMSG_LENGTH_BITS); // max 8 * 256 bits
	m_DataIn = buffer;
	return buffer.SeekRelative(m_nLength);
}

const char* SVC_GameEvent::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: bytes %i", GetName(), Bits2Bytes(m_nLength));
	return s_text;
}

bool SVC_SetView::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteUBitLong( m_nEntityIndex, MAX_EDICT_BITS );
	return !buffer.IsOverflowed();
}

bool SVC_SetView::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_SetView::ReadFromBuffer" );

	m_nEntityIndex = buffer.ReadUBitLong( MAX_EDICT_BITS );
	return !buffer.IsOverflowed();
}

const char *SVC_SetView::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: view entity %i", GetName(), m_nEntityIndex );
	return s_text;
}

bool SVC_ServerInfo::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteShort ( m_nProtocol );
	buffer.WriteLong  ( m_nServerCount );
	buffer.WriteOneBit( m_bIsHLTV?1:0);
	buffer.WriteOneBit( m_bIsDedicated?1:0);
	buffer.WriteLong  ( 0xffffffff );  // Used to be client.dll CRC.  This was far before signed binaries, VAC, and cross-platform play
	buffer.WriteWord  ( m_nMaxClasses );
	buffer.WriteBytes( m_nMapMD5.bits, MD5_DIGEST_LENGTH );		// To prevent cheating with hacked maps
	buffer.WriteByte  ( m_nPlayerSlot );
	buffer.WriteByte  ( m_nMaxClients );
	buffer.WriteFloat ( m_fTickInterval );
	buffer.WriteChar  ( m_cOS );
	buffer.WriteString( m_szGameDir );
	buffer.WriteString( m_szMapName );
	buffer.WriteString( m_szSkyName );
	buffer.WriteString( m_szHostName );

#if defined( REPLAY_ENABLED )
	buffer.WriteOneBit( m_bIsReplay?1:0);
#endif

	return !buffer.IsOverflowed();
}

bool SVC_ServerInfo::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_ServerInfo::ReadFromBuffer" );

	m_szGameDir = m_szGameDirBuffer;
	m_szMapName = m_szMapNameBuffer;
	m_szSkyName = m_szSkyNameBuffer;
	m_szHostName = m_szHostNameBuffer;

	m_nProtocol		= buffer.ReadShort();
	m_nServerCount	= buffer.ReadLong();
	m_bIsHLTV		= buffer.ReadOneBit()!=0;
	m_bIsDedicated	= buffer.ReadOneBit()!=0;
	buffer.ReadLong();  // Legacy client CRC.
	m_nMaxClasses	= buffer.ReadWord();

	// Prevent cheating with hacked maps
	if ( m_nProtocol > PROTOCOL_VERSION_17 )
	{
		buffer.ReadBytes( m_nMapMD5.bits, MD5_DIGEST_LENGTH );
	}
	else
	{
		m_nMapCRC	= buffer.ReadLong();
	}

	m_nPlayerSlot	= buffer.ReadByte();
	m_nMaxClients	= buffer.ReadByte();
	m_fTickInterval	= buffer.ReadFloat();
	m_cOS			= (char)buffer.ReadChar();
	buffer.ReadString( m_szGameDirBuffer, sizeof(m_szGameDirBuffer) );
	buffer.ReadString( m_szMapNameBuffer, sizeof(m_szMapNameBuffer) );
	buffer.ReadString( m_szSkyNameBuffer, sizeof(m_szSkyNameBuffer) );
	buffer.ReadString( m_szHostNameBuffer, sizeof(m_szHostNameBuffer) );

#if defined( REPLAY_ENABLED )
	// Only attempt to read the 'replay' bit if the net channel's protocol
	// version is greater or equal than the protocol version for replay's release.
	// INetChannel::GetProtocolVersion() will return PROTOCOL_VERSION for
	// a regular net channel, or the network protocol version from the demo
	// file, if we're playing back a demo.
	if ( m_NetChannel->GetProtocolVersion() >= PROTOCOL_VERSION_REPLAY )
	{
		m_bIsReplay = buffer.ReadOneBit() != 0;
	}
#endif

	return !buffer.IsOverflowed();
}

const char *SVC_ServerInfo::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: game \"%s\", map \"%s\", max %i", GetName(), m_szGameDir, m_szMapName, m_nMaxClients );
	return s_text;
}