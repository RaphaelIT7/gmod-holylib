class IClientMessageHandler2 : public INetMessageHandler
{
public:
	virtual ~IClientMessageHandler2( void ) {};

	PROCESS_CLC_MESSAGE( ClientInfo ) = 0;
	PROCESS_CLC_MESSAGE( Move ) = 0;
	PROCESS_CLC_MESSAGE( VoiceData ) = 0;
	PROCESS_CLC_MESSAGE( BaselineAck ) = 0;
	PROCESS_CLC_MESSAGE( ListenEvents ) = 0;
	PROCESS_CLC_MESSAGE( RespondCvarValue ) = 0;
	PROCESS_CLC_MESSAGE( FileCRCCheck ) = 0;
	PROCESS_CLC_MESSAGE( FileMD5Check ) = 0;
#if defined( REPLAY_ENABLED )
	PROCESS_CLC_MESSAGE( SaveReplay ) = 0;
#endif
	PROCESS_CLC_MESSAGE( CmdKeyValues ) = 0;

	PROCESS_CLC_MESSAGE( GMod_ClientToServer ) = 0;
};

#define clc_GMod_ClientToServer 18
class CLC_GMod_ClientToServer : public CNetMessage
{
public:
	bool			ReadFromBuffer( bf_read &buffer );
	bool			WriteToBuffer( bf_write &buffer );
	const char		*ToString() const; // boobies xd
	int				GetType() const { return clc_GMod_ClientToServer; }
		const char* GetName() const { return "GMod_ClientToServer"; }

	IClientMessageHandler2 *m_pMessageHandler;
	bool Process() { return m_pMessageHandler->ProcessGMod_ClientToServer( this ); }

	int magic; // offset
	bf_read		m_DataIn;
};