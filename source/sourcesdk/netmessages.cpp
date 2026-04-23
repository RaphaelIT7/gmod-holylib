#include "netmessages.h"
#include "bitbuf.h"
#include "const.h"
#include "net_chan.h"
#include "mathlib/mathlib.h"
#include "tier0/vprof.h"
#include "net.h"
#include <common.h>
#include <networkstringtabledefs.h>
#include <event_system.h>
#include <convar.h>
#include <memory>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

constexpr size_t s_textSize = 1024;
static thread_local std::unique_ptr<char[]> s_text(new char[1024]);

//
// CmdKeyValues message
//

Base_CmdKeyValues::Base_CmdKeyValues( KeyValues *pKeyValues /* = NULL */ ) :
	m_pKeyValues( pKeyValues )
{
}

Base_CmdKeyValues::~Base_CmdKeyValues()
{
	if ( m_pKeyValues )
		m_pKeyValues->deleteThis();
	m_pKeyValues = nullptr;
}

bool Base_CmdKeyValues::WriteToBuffer( bf_write &buffer )
{
	Assert( m_pKeyValues );
	if ( !m_pKeyValues )
		return false;

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	CUtlBuffer bufData;
	if ( !m_pKeyValues->WriteAsBinary( bufData ) )
	{
		Assert( false );
		return false;
	}

	// Note how many we're sending
	int numBytes = bufData.TellMaxPut();
	buffer.WriteLong( numBytes );
	buffer.WriteBits( bufData.Base(), numBytes * 8 );

	return !buffer.IsOverflowed();
}

bool Base_CmdKeyValues::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "Base_CmdKeyValues::ReadFromBuffer" );

	if ( !m_pKeyValues )
		m_pKeyValues = new KeyValues( "" );

	m_pKeyValues->Clear();

	int numBytes = buffer.ReadLong();
	if ( numBytes <= 0 || numBytes > buffer.GetNumBytesLeft() )
	{
		return false; // don't read past the end of the buffer
	}

	void *pvBuffer = malloc( numBytes );
	if ( !pvBuffer )
	{
		return false;
	}

	buffer.ReadBits( pvBuffer, numBytes * 8 );

	CUtlBuffer bufRead( pvBuffer, numBytes, CUtlBuffer::READ_ONLY );
	if ( !m_pKeyValues->ReadAsBinary( bufRead ) )
	{
		free( pvBuffer );
		Assert( false );
		return false;
	}

	free( pvBuffer );
	return !buffer.IsOverflowed();
}

const char * Base_CmdKeyValues::ToString() const
{
	Q_snprintf( s_text.get(), s_textSize, "%s: %s", 
		GetName(), m_pKeyValues ? m_pKeyValues->GetName() : "<<null>>" );
	return s_text.get();
}

CLC_CmdKeyValues::CLC_CmdKeyValues( KeyValues *pKeyValues /* = NULL */ )
	: Base_CmdKeyValues( pKeyValues ), m_pMessageHandler{nullptr}
{
}

bool CLC_CmdKeyValues::WriteToBuffer( bf_write &buffer )
{
	return Base_CmdKeyValues::WriteToBuffer( buffer );
}

bool CLC_CmdKeyValues::ReadFromBuffer( bf_read &buffer )
{
	return Base_CmdKeyValues::ReadFromBuffer( buffer );
}

const char *CLC_CmdKeyValues::ToString() const
{
	return Base_CmdKeyValues::ToString();
}

SVC_CmdKeyValues::SVC_CmdKeyValues( KeyValues *pKeyValues /* = NULL */ )
	: Base_CmdKeyValues( pKeyValues ), m_pMessageHandler{nullptr}
{
}

bool SVC_CmdKeyValues::WriteToBuffer( bf_write &buffer )
{
	return Base_CmdKeyValues::WriteToBuffer( buffer );
}

bool SVC_CmdKeyValues::ReadFromBuffer( bf_read &buffer )
{
	return Base_CmdKeyValues::ReadFromBuffer( buffer );
}

const char *SVC_CmdKeyValues::ToString() const
{
	return Base_CmdKeyValues::ToString();
}


//
// SVC_Print message
//

bool SVC_Print::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	return buffer.WriteString( m_szText ? m_szText : " svc_print NULL" );
}

bool SVC_Print::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_Print::ReadFromBuffer" );

	m_szText = m_szTextBuffer;
	
	return buffer.ReadString(m_szTextBuffer, sizeof(m_szTextBuffer));
}

const char *SVC_Print::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: \"%s\"", GetName(), m_szText );
	return s_text.get();
}

bool NET_StringCmd::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	return buffer.WriteString( m_szCommand ? m_szCommand : " NET_StringCmd NULL" );
}

bool NET_StringCmd::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "NET_StringCmd::ReadFromBuffer" );

	m_szCommand = m_szCommandBuffer;
	
	return buffer.ReadString(m_szCommandBuffer, sizeof(m_szCommandBuffer));
}

const char *NET_StringCmd::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: \"%s\"", GetName(), m_szCommand );
	return s_text.get();
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

const char *SVC_ServerInfo::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, R"(%s: game "%s", map "%s", max %i)", GetName(), m_szGameDir, m_szMapName, m_nMaxClients );
	return s_text.get();
}

bool NET_SignonState::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteByte( m_nSignonState );
	buffer.WriteLong( m_nSpawnCount );

	return !buffer.IsOverflowed();
}

bool NET_SignonState::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "NET_SignonState::ReadFromBuffer" );

	m_nSignonState = buffer.ReadByte();
	m_nSpawnCount = buffer.ReadLong();

	return !buffer.IsOverflowed();
}

const char *NET_SignonState::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: state %i, count %i", GetName(), m_nSignonState, m_nSpawnCount );
	return s_text.get();
}

bool SVC_BSPDecal::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteBitVec3Coord( m_Pos );
	buffer.WriteUBitLong( m_nDecalTextureIndex, MAX_DECAL_INDEX_BITS );

	if ( m_nEntityIndex != 0)
	{
		buffer.WriteOneBit( 1 );
		buffer.WriteUBitLong( m_nEntityIndex, MAX_EDICT_BITS );
		buffer.WriteUBitLong( m_nModelIndex, SP_MODEL_INDEX_BITS );
	}
	else
	{
		buffer.WriteOneBit( 0 );
	}
	buffer.WriteOneBit( m_bLowPriority ? 1 : 0 );

	return !buffer.IsOverflowed();
}

bool SVC_BSPDecal::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_BSPDecal::ReadFromBuffer" );

	buffer.ReadBitVec3Coord( m_Pos );
	m_nDecalTextureIndex = buffer.ReadUBitLong( MAX_DECAL_INDEX_BITS );

	if ( buffer.ReadOneBit() != 0 )
	{
		m_nEntityIndex = buffer.ReadUBitLong( MAX_EDICT_BITS );
		m_nModelIndex = buffer.ReadUBitLong( SP_MODEL_INDEX_BITS );
	}
	else
	{
		m_nEntityIndex = 0;
		m_nModelIndex = 0;
	}
	m_bLowPriority = buffer.ReadOneBit() ? true : false;
	
	return !buffer.IsOverflowed();
}

const char *SVC_BSPDecal::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: tex %i, ent %i, mod %i lowpriority %i", 
		GetName(), m_nDecalTextureIndex, m_nEntityIndex, m_nModelIndex, m_bLowPriority ? 1 : 0 );
	return s_text.get();
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

const char *SVC_SetView::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: view entity %i", GetName(), m_nEntityIndex );
	return s_text.get();
}

bool SVC_FixAngle::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteOneBit( m_bRelative ? 1 : 0 );
	buffer.WriteBitAngle( m_Angle.x, 16 );
	buffer.WriteBitAngle( m_Angle.y, 16 );
	buffer.WriteBitAngle( m_Angle.z, 16 );
	return !buffer.IsOverflowed();
}

bool SVC_FixAngle::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_FixAngle::ReadFromBuffer" );

	m_bRelative = buffer.ReadOneBit() != 0;
	m_Angle.x = buffer.ReadBitAngle( 16 );
	m_Angle.y = buffer.ReadBitAngle( 16 );
	m_Angle.z = buffer.ReadBitAngle( 16 );
	return !buffer.IsOverflowed();
}

const char *SVC_FixAngle::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: %s %.1f %.1f %.1f ", GetName(), m_bRelative?"relative":"absolute",
		m_Angle[0], m_Angle[1], m_Angle[2] );
	return s_text.get();
}

bool SVC_CrosshairAngle::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteBitAngle( m_Angle.x, 16 );
	buffer.WriteBitAngle( m_Angle.y, 16 );
	buffer.WriteBitAngle( m_Angle.z, 16 );
	return !buffer.IsOverflowed();
}

bool SVC_CrosshairAngle::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_CrosshairAngle::ReadFromBuffer" );

	m_Angle.x = buffer.ReadBitAngle( 16 );
	m_Angle.y = buffer.ReadBitAngle( 16 );
	m_Angle.z = buffer.ReadBitAngle( 16 );
	return !buffer.IsOverflowed();
}

const char *SVC_CrosshairAngle::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: (%.1f %.1f %.1f)", GetName(), m_Angle[0], m_Angle[1], m_Angle[2] );
	return s_text.get();
}

bool SVC_VoiceInit::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteString( m_szVoiceCodec );
	buffer.WriteByte( /* Legacy Quality Field */ 255 );
	buffer.WriteShort( m_nSampleRate );
	return !buffer.IsOverflowed();
}

bool SVC_VoiceInit::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_VoiceInit::ReadFromBuffer" );

	buffer.ReadString( m_szVoiceCodec, sizeof(m_szVoiceCodec) );
	unsigned char nLegacyQuality = buffer.ReadByte();
	if ( nLegacyQuality == 255 )
	{
		// v2 packet
		m_nSampleRate = buffer.ReadShort();
	}
	else
	{
		// v1 packet
		//
		// Hacky workaround for v1 packets not actually indicating if we were using steam voice -- we've kept the steam
		// voice separate convar that was in use at the time as replicated&hidden, and if whatever network stream we're
		// interpreting sets it, lie about the subsequent voice init's codec & sample rate.
		ConVarRef sv_use_steam_voice("sv_use_steam_voice");
		if ( sv_use_steam_voice.GetBool() )
		{
			Msg( "Legacy SVC_VoiceInit - got a set for sv_use_steam_voice convar, assuming Steam voice\n" );
			V_strncpy( m_szVoiceCodec, "steam", sizeof( m_szVoiceCodec ) );
			// Legacy steam voice can always be parsed as auto sample rate.
			m_nSampleRate = 0;
		}
		else if ( V_strncasecmp( m_szVoiceCodec, "vaudio_celt", sizeof( m_szVoiceCodec ) ) == 0 )
		{
			// Legacy rate vaudio_celt always selected during v1 packet era
			m_nSampleRate = 22050;
		}
		else
		{
			// Legacy rate everything but CELT always selected during v1 packet era
			m_nSampleRate = 11025;
		}
	}

	return !buffer.IsOverflowed();
}

const char *SVC_VoiceInit::ToString() const
{
	Q_snprintf( s_text.get(), s_textSize, "%s: codec \"%s\", sample rate %i", GetName(), m_szVoiceCodec, m_nSampleRate );
	return s_text.get();
}

bool SVC_VoiceData::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteByte( m_nFromClient );
	buffer.WriteByte( m_bProximity );
	buffer.WriteWord( m_nLength );

	if ( IsX360() )
	{
		buffer.WriteLongLong( m_xuid );
	}

	return buffer.WriteBits( m_DataOut, m_nLength );
}

bool SVC_VoiceData::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_VoiceData::ReadFromBuffer" );

	m_nFromClient = buffer.ReadByte();
	m_bProximity = !!buffer.ReadByte();
	m_nLength = buffer.ReadWord();

	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_VoiceData::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: client %i, bytes %i", GetName(), m_nFromClient, Bits2Bytes(m_nLength) );
	return s_text.get();
}

#define NET_TICK_SCALEUP	100000.0f

bool NET_Tick::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteLong( m_nTick );
	buffer.WriteUBitLong( clamp( (int)( NET_TICK_SCALEUP * m_flHostFrameTime ), 0, 65535 ), 16 );
	buffer.WriteUBitLong( clamp( (int)( NET_TICK_SCALEUP * m_flHostFrameTimeStdDeviation ), 0, 65535 ), 16 );

	return !buffer.IsOverflowed();
}

bool NET_Tick::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "NET_Tick::ReadFromBuffer" );

	m_nTick = buffer.ReadLong();
	m_flHostFrameTime				= (float)buffer.ReadUBitLong( 16 ) / NET_TICK_SCALEUP;
	m_flHostFrameTimeStdDeviation	= (float)buffer.ReadUBitLong( 16 ) / NET_TICK_SCALEUP;

	return !buffer.IsOverflowed();
}

const char *NET_Tick::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: tick %i", GetName(), m_nTick );
	return s_text.get();
}

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

const char *SVC_UserMessage::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: type %i, bytes %i", GetName(), m_nMsgType, Bits2Bytes(m_nLength) );
	return s_text.get();
}

bool SVC_SetPause::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteOneBit( m_bPaused?1:0 );
	return !buffer.IsOverflowed();
}

bool SVC_SetPause::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_SetPause::ReadFromBuffer" );

	m_bPaused = buffer.ReadOneBit() != 0;
	return !buffer.IsOverflowed();
}

const char *SVC_SetPause::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: %s", GetName(), m_bPaused?"paused":"unpaused" );
	return s_text.get();
}

bool NET_SetConVar::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	int numvars = m_ConVars.Count();
	Assert( numvars <= UCHAR_MAX );

	// Note how many we're sending
	buffer.WriteByte( (uint8_t)numvars );

	for (auto &ccvar : m_ConVars)
	{
		buffer.WriteString( ccvar.name  );
		buffer.WriteString( ccvar.value );
	}

	return !buffer.IsOverflowed();
}

bool NET_SetConVar::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "NET_SetConVar::ReadFromBuffer" );

	int numvars = buffer.ReadByte();

	m_ConVars.RemoveAll();

	for (int i=0; i< numvars; i++ )
	{
		cvar_t var;
		buffer.ReadString( var.name, sizeof(var.name) );
		buffer.ReadString( var.value, sizeof(var.value) );
		m_ConVars.AddToTail( var );

	}
	return !buffer.IsOverflowed();
}

const char *NET_SetConVar::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, R"(%s: %zd cvars, "%s"="%s")", 
		GetName(), m_ConVars.Count(), 
		m_ConVars[0].name, m_ConVars[0].value );
	return s_text.get();
}

bool SVC_UpdateStringTable::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	m_nLength = m_DataOut.GetNumBitsWritten();

	buffer.WriteUBitLong( m_nTableID, Q_log2( MAX_TABLES ) );	// TODO check bounds

	if ( m_nChangedEntries == 1 )
	{
		buffer.WriteOneBit( 0 ); // only one entry changed
	}
	else
	{
		buffer.WriteOneBit( 1 );
		buffer.WriteWord( m_nChangedEntries );	// more entries changed
	}

	buffer.WriteUBitLong( m_nLength, 20 );
	
	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool SVC_UpdateStringTable::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_UpdateStringTable::ReadFromBuffer" );

	int bits = Q_log2( MAX_TABLES );
	m_nTableID = buffer.ReadUBitLong( bits );

	if ( buffer.ReadOneBit() != 0 )
	{
		m_nChangedEntries = buffer.ReadWord();
	}
	else
	{
		m_nChangedEntries = 1;
	}
	
	m_nLength = buffer.ReadUBitLong( 20 );
	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_UpdateStringTable::ToString() const
{
	Q_snprintf(s_text.get(),s_textSize, "%s: table %i, changed %i, bytes %i", GetName(), m_nTableID, m_nChangedEntries, Bits2Bytes(m_nLength) );
	return s_text.get();
}

SVC_CreateStringTable::SVC_CreateStringTable()
    : m_pMessageHandler{nullptr},
	m_szTableName{nullptr},
	m_nMaxEntries{-1},
	m_nNumEntries{-1},
	m_bUserDataFixedSize{false},
	m_nUserDataSize{-1},
	m_nUserDataSizeBits{-1},
	m_bIsFilenames{false},
	m_nLength{-1},
	m_bDataCompressed{false}
{
	m_szTableNameBuffer[0] = '\0';
}

bool SVC_CreateStringTable::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	m_nLength = m_DataOut.GetNumBitsWritten();

	/*
	JASON: this code is no longer needed; the ':' is prepended to the table name at table creation time.
	if ( m_bIsFilenames )
	{
		// identifies a table that hosts filenames
		buffer.WriteByte( ':' );
	}
	*/

	buffer.WriteString( m_szTableName );
	buffer.WriteWord( m_nMaxEntries );
	int encodeBits = Q_log2( m_nMaxEntries );
	buffer.WriteUBitLong( m_nNumEntries, encodeBits+1 );
	buffer.WriteVarInt32( m_nLength ); // length in bits

	buffer.WriteOneBit( m_bUserDataFixedSize ? 1 : 0 );
	if ( m_bUserDataFixedSize )
	{
		buffer.WriteUBitLong( m_nUserDataSize, 12 );
		buffer.WriteUBitLong( m_nUserDataSizeBits, 4 );
	}
	
	buffer.WriteOneBit( m_bDataCompressed ? 1 : 0 );
	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool SVC_CreateStringTable::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_CreateStringTable::ReadFromBuffer" );

	char prefix = buffer.PeekUBitLong( 8 );
	if ( prefix == ':' )
	{
		// table hosts filenames
		m_bIsFilenames = true;
		// dimhotepus: Skip :
		(void)buffer.ReadByte();
	}
	else
	{
		m_bIsFilenames = false;
	}

	m_szTableName = m_szTableNameBuffer;
	buffer.ReadString( m_szTableNameBuffer, sizeof(m_szTableNameBuffer) );
	m_nMaxEntries = buffer.ReadWord();
	int encodeBits = Q_log2( m_nMaxEntries );
	m_nNumEntries = buffer.ReadUBitLong( encodeBits+1 );
	if ( m_NetChannel->GetProtocolVersion() > PROTOCOL_VERSION_23 )
		m_nLength = buffer.ReadVarInt32();
	else
		m_nLength = buffer.ReadUBitLong( NET_MAX_PAYLOAD_BITS_V23 + 3 );

	m_bUserDataFixedSize = buffer.ReadOneBit() ? true : false;
	if ( m_bUserDataFixedSize )
	{
		m_nUserDataSize = buffer.ReadUBitLong( 12 );
		m_nUserDataSizeBits = buffer.ReadUBitLong( 4 );
	}
	else
	{
		m_nUserDataSize = 0;
		m_nUserDataSizeBits = 0;
	}

	//if ( m_pMessageHandler->GetDemoProtocolVersion() > PROTOCOL_VERSION_14 )
	{
		m_bDataCompressed = buffer.ReadOneBit() != 0;
	}
	//else
	//{
	//	m_bDataCompressed = false;
	//}

	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_CreateStringTable::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: table %s, entries %i, bytes %i userdatasize %i userdatabits %i", 
		GetName(), m_szTableName, m_nNumEntries, Bits2Bytes(m_nLength), m_nUserDataSize, m_nUserDataSizeBits );
	return s_text.get();
}

bool SVC_Sounds::WriteToBuffer( bf_write &buffer )
{
	m_nLength = m_DataOut.GetNumBitsWritten();

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	Assert( m_nNumSounds > 0 );
	
	if ( m_bReliableSound )
	{
		// as single sound message is 32 bytes long maximum
		buffer.WriteOneBit( 1 );
		buffer.WriteUBitLong( m_nLength, 8 );
	}
	else
	{
		// a bunch of unreliable messages
		buffer.WriteOneBit( 0 );
		buffer.WriteUBitLong( m_nNumSounds, 8 );
		buffer.WriteUBitLong( m_nLength, 16  );
	}
	
	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool SVC_Sounds::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_Sounds::ReadFromBuffer" );

	m_bReliableSound = buffer.ReadOneBit() != 0;

	if ( m_bReliableSound )
	{
		m_nNumSounds = 1;
		m_nLength = buffer.ReadUBitLong( 8 );

	}
	else
	{
		m_nNumSounds = buffer.ReadUBitLong( 8 );
		m_nLength = buffer.ReadUBitLong( 16 );
	}
		
	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_Sounds::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: number %i,%s bytes %i", 
		GetName(), m_nNumSounds, m_bReliableSound?" reliable,":"", Bits2Bytes(m_nLength) );
	return s_text.get();
} 

bool SVC_Prefetch::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	// Don't write type until we have more thanone
	// buffer.WriteUBitLong( m_fType, 1 );
	buffer.WriteUBitLong( m_nSoundIndex, MAX_SOUND_INDEX_BITS );
	return !buffer.IsOverflowed();
}

bool SVC_Prefetch::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_Prefetch::ReadFromBuffer" );

	m_fType = SOUND; // buffer.ReadUBitLong( 1 );
	//if( m_pMessageHandler->GetDemoProtocolVersion() > 22 )
	{
		m_nSoundIndex = buffer.ReadUBitLong( MAX_SOUND_INDEX_BITS );
	}
	//else
	//{
	//	m_nSoundIndex = buffer.ReadUBitLong( 13 );
	//}

	return !buffer.IsOverflowed();
}

const char *SVC_Prefetch::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: type %i index %i", 
		GetName(), 
		(int)m_fType, 
		(int)m_nSoundIndex );
	return s_text.get();
} 

bool SVC_TempEntities::WriteToBuffer( bf_write &buffer )
{
	Assert( m_nNumEntries > 0 );

	m_nLength = m_DataOut.GetNumBitsWritten();

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteUBitLong( m_nNumEntries, CEventInfo::EVENT_INDEX_BITS );
	buffer.WriteVarInt32( m_nLength );
	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool SVC_TempEntities::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_TempEntities::ReadFromBuffer" );

	m_nNumEntries = buffer.ReadUBitLong( CEventInfo::EVENT_INDEX_BITS );
	//if ( m_pMessageHandler->GetDemoProtocolVersion() > PROTOCOL_VERSION_23 )
		m_nLength = buffer.ReadVarInt32();
	//else
	//	m_nLength = buffer.ReadUBitLong( NET_MAX_PAYLOAD_BITS_V23 );

	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_TempEntities::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: number %i, bytes %i", GetName(), m_nNumEntries, Bits2Bytes(m_nLength) );
	return s_text.get();
} 

bool SVC_ClassInfo::WriteToBuffer( bf_write &buffer )
{
	if ( !m_bCreateOnClient )
	{
		m_nNumServerClasses = m_Classes.Count();	// use number from list
	}

	Assert( m_nNumServerClasses <= SHRT_MAX );
	int numServerClasses = m_nNumServerClasses;
	
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteShort( numServerClasses );
	buffer.WriteOneBit( m_bCreateOnClient ? 1 : 0 );

	if ( m_bCreateOnClient )
		return !buffer.IsOverflowed();

	int serverClassBits = Q_log2( numServerClasses ) + 1;
	for ( int i = 0; i < m_nNumServerClasses; i++ )
	{
		class_t * serverclass = &m_Classes[i];

		buffer.WriteUBitLong( serverclass->classID, serverClassBits );
		buffer.WriteString( serverclass->classname );
		buffer.WriteString( serverclass->datatablename );
	}

	return !buffer.IsOverflowed();
}

bool SVC_ClassInfo::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_ClassInfo::ReadFromBuffer" );

	m_Classes.RemoveAll();

	int numServerClasses = buffer.ReadShort();
	int nServerClassBits = Q_log2( numServerClasses ) + 1;

	m_nNumServerClasses = numServerClasses;
	m_bCreateOnClient = buffer.ReadOneBit() != 0;

	if ( m_bCreateOnClient )
	{
		return !buffer.IsOverflowed(); // stop here
	}

	for ( int i=0; i < m_nNumServerClasses; i++ )
	{
		class_t serverclass;

		serverclass.classID = buffer.ReadUBitLong( nServerClassBits );
		buffer.ReadString( serverclass.classname, sizeof(serverclass.classname) );
		buffer.ReadString( serverclass.datatablename, sizeof(serverclass.datatablename) );

		m_Classes.AddToTail( serverclass );
	}
	
	return !buffer.IsOverflowed();
}

const char *SVC_ClassInfo::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: num %zd, %s", GetName(), 
		m_nNumServerClasses, m_bCreateOnClient ? "use client classes" : "full update" );
	return s_text.get();
} 

/*
bool SVC_SpawnBaseline::WriteToBuffer( bf_write &buffer )
{
	m_nLength = m_DataOut.GetNumBitsWritten();

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	buffer.WriteUBitLong( m_nEntityIndex, MAX_EDICT_BITS );

	buffer.WriteUBitLong( m_nClassID, MAX_SERVER_CLASS_BITS );

	buffer.WriteUBitLong( m_nLength, Q_log2(MAX_PACKEDENTITY_DATA<<3) ); // TODO see below

	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool SVC_SpawnBaseline::ReadFromBuffer( bf_read &buffer )
{
	m_nEntityIndex = buffer.ReadUBitLong( MAX_EDICT_BITS );

	m_nClassID = buffer.ReadUBitLong( MAX_SERVER_CLASS_BITS );

	m_nLength = buffer.ReadUBitLong( Q_log2(MAX_PACKEDENTITY_DATA<<3) ); // TODO wrong, check bounds

	m_DataIn = buffer;

	return buffer.SeekRelative( m_nLength );
}

const char *SVC_SpawnBaseline::ToString(void) const
{
	static char text[256];
	Q_snprintf(text, sizeof(text), "%s: ent %i, class %i, bytes %i", 
		GetName(), m_nEntityIndex, m_nClassID, Bits2Bytes(m_nLength) );
	return text;
} */

bool SVC_GameEvent::WriteToBuffer( bf_write &buffer )
{
	m_nLength = m_DataOut.GetNumBitsWritten();
	Assert( m_nLength < (1 << NETMSG_LENGTH_BITS) );
	if ( m_nLength >= (1 << NETMSG_LENGTH_BITS) )
		return false;

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteUBitLong( m_nLength, NETMSG_LENGTH_BITS );  // max 8 * 256 bits
	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
	
}

bool SVC_GameEvent::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_GameEvent::ReadFromBuffer" );

	m_nLength = buffer.ReadUBitLong( NETMSG_LENGTH_BITS ); // max 8 * 256 bits
	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_GameEvent::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: bytes %i", GetName(), Bits2Bytes(m_nLength) );
	return s_text.get();
} 

bool SVC_SendTable::WriteToBuffer( bf_write &buffer )
{
	m_nLength = m_DataOut.GetNumBitsWritten();

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteOneBit( m_bNeedsDecoder?1:0 );
	buffer.WriteShort( m_nLength );
	buffer.WriteBits( m_DataOut.GetData(), m_nLength );

	return !buffer.IsOverflowed();
}

bool SVC_SendTable::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_SendTable::ReadFromBuffer" );

	m_bNeedsDecoder = buffer.ReadOneBit() != 0;
	m_nLength = buffer.ReadShort();		// TODO do we have a maximum length ? check that

	m_DataIn = buffer;

	return buffer.SeekRelative( m_nLength );
}

const char *SVC_SendTable::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: needs Decoder %s,bytes %i", 
		GetName(), m_bNeedsDecoder?"yes":"no", Bits2Bytes(m_nLength) );
	return s_text.get();
} 

bool SVC_EntityMessage::WriteToBuffer( bf_write &buffer )
{
	m_nLength = m_DataOut.GetNumBitsWritten();
	Assert( m_nLength < (1 << NETMSG_LENGTH_BITS) );
	if ( m_nLength >= (1 << NETMSG_LENGTH_BITS) )
		return false;

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteUBitLong( m_nEntityIndex, MAX_EDICT_BITS );
	buffer.WriteUBitLong( m_nClassID, MAX_SERVER_CLASS_BITS );
	buffer.WriteUBitLong( m_nLength, NETMSG_LENGTH_BITS );  // max 8 * 256 bits
	
	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool SVC_EntityMessage::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_EntityMessage::ReadFromBuffer" );

	m_nEntityIndex = buffer.ReadUBitLong( MAX_EDICT_BITS );
	m_nClassID = buffer.ReadUBitLong( MAX_SERVER_CLASS_BITS );
	m_nLength = buffer.ReadUBitLong( NETMSG_LENGTH_BITS );  // max 8 * 256 bits
	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_EntityMessage::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: entity %i, class %i, bytes %i",
		GetName(), m_nEntityIndex, m_nClassID, Bits2Bytes(m_nLength) );
	return s_text.get();
}

bool SVC_PacketEntities::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	buffer.WriteUBitLong( m_nMaxEntries, MAX_EDICT_BITS );
	
	buffer.WriteOneBit( m_bIsDelta?1:0 );

	if ( m_bIsDelta )
	{
		buffer.WriteLong( m_nDeltaFrom );
	}

	buffer.WriteUBitLong( m_nBaseline, 1 );

	buffer.WriteUBitLong( m_nUpdatedEntries, MAX_EDICT_BITS );

	buffer.WriteUBitLong( m_nLength, DELTASIZE_BITS );

	buffer.WriteOneBit( m_bUpdateBaseline?1:0 ); 

	buffer.WriteBits( m_DataOut.GetData(), m_nLength );

	return !buffer.IsOverflowed();
}

bool SVC_PacketEntities::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_PacketEntities::ReadFromBuffer" );

	m_nMaxEntries = buffer.ReadUBitLong( MAX_EDICT_BITS );
	
	m_bIsDelta = buffer.ReadOneBit()!=0;

	if ( m_bIsDelta )
	{
		m_nDeltaFrom = buffer.ReadLong();
	}
	else
	{
		m_nDeltaFrom = -1;
	}

	m_nBaseline = buffer.ReadUBitLong( 1 );

	m_nUpdatedEntries = buffer.ReadUBitLong( MAX_EDICT_BITS );

	m_nLength = buffer.ReadUBitLong( DELTASIZE_BITS );

	m_bUpdateBaseline = buffer.ReadOneBit() != 0;

	m_DataIn = buffer;
	
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_PacketEntities::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: delta %i, max %i, changed %i,%s bytes %i (%i)",
		GetName(), m_nDeltaFrom, m_nMaxEntries, m_nUpdatedEntries, m_bUpdateBaseline?" BL update,":"", Bits2Bytes(m_nLength), m_nLength );
	return s_text.get();
} 

SVC_Menu::SVC_Menu( DIALOG_TYPE type, KeyValues *data ) : m_pMessageHandler{nullptr}
{
	m_bReliable = true;

	m_Type = type;
	m_MenuKeyValues = data->MakeCopy();
	m_iLength = -1;
}

SVC_Menu::~SVC_Menu()
{
	if ( m_MenuKeyValues )
	{
		m_MenuKeyValues->deleteThis();
	}
}

bool SVC_Menu::WriteToBuffer( bf_write &buffer )
{
	if ( !m_MenuKeyValues )
	{
		return false;
	}

	CUtlBuffer buf;
	// dimhotepus: Ensure message is written successfully.
	if ( !m_MenuKeyValues->WriteAsBinary( buf ) )
	{
		// dimhotepus: Exit if write failed.
		Warning( "Unable to write menu data to buffer.\n" );
		return false;
	}

	if ( buf.TellPut() > 4096 )
	{
		Warning( "Too much menu data %zd (4096 bytes max)\n", buf.TellPut() );
		return false;
	}

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteShort( m_Type );
	buffer.WriteWord( buf.TellPut() );
	buffer.WriteBytes( buf.Base(), buf.TellPut() );

	return !buffer.IsOverflowed();
}

bool SVC_Menu::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_Menu::ReadFromBuffer" );

	m_Type = (DIALOG_TYPE)buffer.ReadShort();
	m_iLength = buffer.ReadWord();

	CUtlBuffer buf( (int)0, m_iLength, 0 );
	buffer.ReadBytes( buf.Base(), m_iLength );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, m_iLength );

	if ( m_MenuKeyValues ) 
	{
		m_MenuKeyValues->deleteThis();
	}
	m_MenuKeyValues = new KeyValues( "menu" );
	Assert( m_MenuKeyValues );
	
	// dimhotepus: Ensure message is read successfully.
	return m_MenuKeyValues->ReadAsBinary( buf ) && !buffer.IsOverflowed();
}

const char *SVC_Menu::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: %i \"%s\" (len:%i)", GetName(),
		m_Type, m_MenuKeyValues ? m_MenuKeyValues->GetName() : "No KeyValues", m_iLength );
	return s_text.get();
} 

bool SVC_GameEventList::WriteToBuffer( bf_write &buffer )
{
	Assert( m_nNumEvents > 0 );

	m_nLength = m_DataOut.GetNumBitsWritten();

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteUBitLong( m_nNumEvents, MAX_EVENT_BITS );
	buffer.WriteUBitLong( m_nLength, 20  );
	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool SVC_GameEventList::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_GameEventList::ReadFromBuffer" );

	m_nNumEvents = buffer.ReadUBitLong( MAX_EVENT_BITS );
	m_nLength = buffer.ReadUBitLong( 20 );
	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_GameEventList::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: number %i, bytes %i", GetName(), m_nNumEvents, Bits2Bytes(m_nLength) );
	return s_text.get();
} 

bool SVC_GetCvarValue::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	buffer.WriteSBitLong( m_iCookie, 32 );
	buffer.WriteString( m_szCvarName );

	return !buffer.IsOverflowed();
}

bool SVC_GetCvarValue::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_GetCvarValue::ReadFromBuffer" );

	m_iCookie = buffer.ReadSBitLong( 32 );
	buffer.ReadString( m_szCvarNameBuffer, sizeof(m_szCvarNameBuffer) );
	m_szCvarName = m_szCvarNameBuffer;
	
	return !buffer.IsOverflowed();
}

const char *SVC_GetCvarValue::ToString() const
{
	Q_snprintf( s_text.get(), s_textSize, "%s: cvar: %s, cookie: %d", GetName(), m_szCvarName, m_iCookie );
	return s_text.get();
}