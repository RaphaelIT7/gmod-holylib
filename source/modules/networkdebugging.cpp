#include "module.h"
#include "util.h"
#include "detours.h"
#include "symbols.h"
#include <memory_resource>
#include <mutex>
#include <deque>
#include "netmessages.h"
#include <convar.h>
#include "cnetchan.h"
#include <atomic>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CNetworkDebuggingModule : public IModule
{
public:
	void InitDetour(bool bPreServer) override;
	void Shutdown() override;
	const char* Name() override { return "networkdebugging"; };
	int Compatibility() override { return LINUX64; };
};

static CNetworkDebuggingModule g_pNetworkDebuggingModule;
IModule* pNetworkDebuggingModule = &g_pNetworkDebuggingModule;

struct NetEntry
{
	NetEntry(std::pmr::memory_resource* mr) : pData(mr) {};

	std::pmr::vector<std::byte> pData;
};

struct NetHistory
{
	// Let's avoid memory fragmentation since we'd be doing a heck of inserts & erasing
	std::pmr::unsynchronized_pool_resource pool;
	std::pmr::deque<NetEntry> pHistory{&pool};
	std::mutex pSendMutex;
	std::atomic<bool> pSendBool = false;

	void AddEntry(const void* pData, size_t nLength)
	{
		auto& entry = pHistory.emplace_front(&pool);

		entry.pData.resize(nLength);
		memcpy(entry.pData.data(), pData, nLength);

		// We save the last 256 packets only!
		while (pHistory.size() > 256)
			pHistory.pop_back();
	}

	void DumpToDisk(const char* steamid)
	{
		CSteamID steamID;
		steamID.SetFromString(steamid, EUniverse::k_EUniversePublic);
		uint64_t steamID64 = steamID.ConvertToUint64();

		char pFileName[MAX_PATH];
		snprintf(pFileName, sizeof(pFileName), "holylib/netdump/%llu", steamID64);
		g_pFullFileSystem->CreateDirHierarchy(pFileName, "MOD");

		for (size_t i = 0; i<pHistory.size(); ++i)
		{
			snprintf(pFileName, sizeof(pFileName), "holylib/netdump/%llu/%i.dat", steamID64, i);
			FileHandle_t fh = g_pFullFileSystem->Open(pFileName, "wb", "MOD");
			if (!fh)
			{
				Msg(PROJECT_NAME " - networkdebugging: Failed to write dump %i for %llu\n", i, steamID64);
				continue;
			}

			g_pFullFileSystem->Write(pHistory[i].pData.data(), pHistory[i].pData.size(), fh);
			g_pFullFileSystem->Close(fh);
		}
	}
};

static std::unordered_map<void*, NetHistory*> g_pNetHistory;
static std::shared_mutex g_pNetHistoryMutex;

// MUST be thread_local as iirc with SendSnapshot a disconnect can happen on other threads!
static thread_local bool g_bDumpNextDisconnect = false;
static thread_local std::string g_strNextSteamID = "";
static Detouring::Hook detour_NET_RemoveNetChannel;
static void hook_NET_RemoveNetChannel(INetChannel* pChannel, bool bShouldRemove)
{
	std::unique_lock<std::shared_mutex> lock(g_pNetHistoryMutex);
	auto it = g_pNetHistory.find(pChannel);
	if (it != g_pNetHistory.end())
	{
		if (g_bDumpNextDisconnect)
		{
			it->second->DumpToDisk(g_strNextSteamID.c_str());
			g_bDumpNextDisconnect = false;
		}

		delete it->second;
		g_pNetHistory.erase(it);
	}

	detour_NET_RemoveNetChannel.GetTrampoline<Symbols::NET_RemoveNetChannel>()(pChannel, bShouldRemove);
}

static Detouring::Hook detour_NET_SendPacket;
static int hook_NET_SendPacket(INetChannel *chan, int sock, const netadr_t &to, const unsigned char *data, int length, bf_write *pVoicePayload /* = nullptr */, bool bUseCompression /*=false*/)
{
	int ret = detour_NET_SendPacket.GetTrampoline<Symbols::NET_SendPacket>()(chan, sock, to, data, length, pVoicePayload, bUseCompression);
	if (!chan)
		return ret;

	std::shared_lock<std::shared_mutex> lock(g_pNetHistoryMutex);
	auto it = g_pNetHistory.find(chan);
	if (it != g_pNetHistory.end())
		it->second->AddEntry((const void*)data, length);

	return ret;
}

static Detouring::Hook detour_CNetChan_SendNetMsg;
static bool hook_CNetChan_SendNetMsg(void* _this, INetMessage &msg, bool bForceReliable, bool bVoice)
{
	NetHistory* pHistory = nullptr;
	// We use a unique_lock since normally when SendNetMsg is called only the main thread is usually doing work
	// While in NET_SendPacket for example we can have multiple threads doing work and blocking there is far more expensive
	std::unique_lock<std::shared_mutex> lock(g_pNetHistoryMutex);
	auto it = g_pNetHistory.find(_this);
	if (it == g_pNetHistory.end()) {
		pHistory = new NetHistory;
		g_pNetHistory[_this] = pHistory;
	} else
		pHistory = it->second;

	// I really doubt this would ever be hit.
	if (pHistory->pSendBool.exchange(true, std::memory_order_acq_rel))
		Warning(PROJECT_NAME " - networkdebugging: Race condition! Two threads tried to use CNetChan::SendNetMsg on the same channel!\n");

	std::unique_lock<std::mutex> sendLock(pHistory->pSendMutex);
	bool bRet = detour_CNetChan_SendNetMsg.GetTrampoline<Symbols::CNetChan_SendNetMsg>()(_this, msg, bForceReliable, bVoice);
	pHistory->pSendBool.store(false, std::memory_order_release);

	return bRet;
}

class GameEventListener : public IGameEventListener2
{
public:
	GameEventListener() = default;

	void FireGameEvent(IGameEvent* pEvent)
	{
		if (V_stricmp(pEvent->GetName(), "player_disconnect") != 0)
			return;
		
		const char* pReason = pEvent->GetString("reason");
		const char* pSteamID = pEvent->GetString("networkid");
		if (!pReason || !pSteamID)
			return;

		std::string_view strReason = pReason;
		if (strReason.find("timed out") != std::string_view::npos)
			return;

		if (strReason.find("Disconnect by user") != std::string_view::npos)
			return;

		if (strReason.find("Server shutting down") != std::string_view::npos)
			return;

		g_strNextSteamID = pSteamID;
		g_bDumpNextDisconnect = true;
		// This works since right now were inside CGameClient::Disconnect
		// Which then goes -> CBaseClient::Disconnect -> CNetChan::Shutdown -> NET_RemoveNetChannel (which we hooked into)
	}
};
static GameEventListener g_pGameEventListener;

void CNetworkDebuggingModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	Util::gameeventmanager->AddListener(&g_pGameEventListener, "player_disconnect", true);

	g_pFullFileSystem->CreateDirHierarchy("holylib/netdump/");

	SourceSDK::FactoryLoader engine_loader("engine");
	Detour::Create(
		&detour_NET_RemoveNetChannel, "NET_RemoveNetChannel",
		engine_loader.GetModule(), Symbols::NET_RemoveNetChannelSym,
		(void*)hook_NET_RemoveNetChannel, m_pID
	);

	Detour::Create(
		&detour_NET_SendPacket, "NET_SendPacket",
		engine_loader.GetModule(), Symbols::NET_SendPacketSym,
		(void*)hook_NET_SendPacket, m_pID
	);

	Detour::Create(
		&detour_CNetChan_SendNetMsg, "CNetChan::SendNetMsg",
		engine_loader.GetModule(), Symbols::CNetChan_SendNetMsgSym,
		(void*)hook_CNetChan_SendNetMsg, m_pID
	);
}

void CNetworkDebuggingModule::Shutdown()
{
	std::unique_lock<std::shared_mutex> lock(g_pNetHistoryMutex);
	for (auto it = g_pNetHistory.begin(); it != g_pNetHistory.end(); ++it)
		delete it->second;

	g_pNetHistory.clear();

	if (Util::gameeventmanager)
		Util::gameeventmanager->RemoveListener(&g_pGameEventListener);
}

// ToDo: Update our sourcesdk from the TF2 merge

// misyl: Shamelessly nicked from Source 2 =)
//
//--------------------------------------------------------------------------------------------------
// RunCodeAtScopeExit
//
// Example:
//	int *x = new int;
//	RunCodeAtScopeExit( delete x )
//--------------------------------------------------------------------------------------------------
template <typename LambdaType>
class CScopeGuardLambdaImpl
{
public:
	explicit CScopeGuardLambdaImpl( LambdaType&& lambda ) : m_lambda( std::move( lambda ) ) { }
	~CScopeGuardLambdaImpl() { m_lambda(); }
private:
	LambdaType m_lambda;
};

//--------------------------------------------------------------------------------------------------
template <typename LambdaType>
CScopeGuardLambdaImpl< LambdaType > MakeScopeGuardLambda( LambdaType&& lambda )
{
	return CScopeGuardLambdaImpl< LambdaType >( std::move( lambda ) );
}

//--------------------------------------------------------------------------------------------------
#define RunLambdaAtScopeExit2( VarName, ... )		const auto VarName( MakeScopeGuardLambda( __VA_ARGS__ ) ); (void)VarName
#define RunLambdaAtScopeExit( ... )					RunLambdaAtScopeExit2( UNIQUE_ID, __VA_ARGS__ )
#define RunCodeAtScopeExit( ... )					RunLambdaAtScopeExit( [&]() { __VA_ARGS__ ; } )

static inline INetMessage* FindMessage(std::vector<INetMessage*> messages, int type)
{
	for (auto& msg : messages)
	{
		if (msg->GetType() == type)
			return msg;
	}

	return nullptr;
}

class SVC_GMod_ServerToClient : public CNetMessage
{
public:
	DECLARE_SVC_MESSAGE( GMod_ServerToClient );

	int m_iGModType = -1;
	int m_iNetMsg = -1;
	int m_iLength = -1;
};

bool SVC_GMod_ServerToClient::WriteToBuffer( bf_write &buffer )
{
	Error(PROJECT_NAME " - NYI\n");
	return false;
}

constexpr size_t s_textSize = 1024;
static thread_local std::unique_ptr<char[]> s_text(new char[s_textSize]);
bool SVC_GMod_ServerToClient::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_CrosshairAngle::ReadFromBuffer" );

	m_iLength = buffer.ReadUBitLong(20);
	m_iGModType = buffer.ReadByte();
	if (m_iGModType == GarrysMod::NetworkMessage::LuaNetMessage)
		m_iNetMsg = buffer.ReadUBitLong(8);
	else
		m_iNetMsg = -1;

	buffer.SeekRelative(m_iLength - 8 - 16);

	return !buffer.IsOverflowed();
}

const char *SVC_GMod_ServerToClient::ToString() const
{
	Q_snprintf(s_text.get(), s_textSize, "%s: type %i bits %i netmsg: %i", GetName(), m_iGModType, m_iLength, m_iNetMsg );
	return s_text.get();
}

#define BYTES2FRAGMENTS(i) (((i)+FRAGMENT_SIZE-1)/FRAGMENT_SIZE)
static inline bool ReadSubChannelData(bf_read& buf)
{
	uint64_t startFragment = 0;
	uint32_t numFragments = 0;
	unsigned int offset = 0;
	unsigned int length = 0;

	bool bSingleBlock = (buf.ReadOneBit() == 0);
	if (!bSingleBlock)
	{
		startFragment = buf.ReadUBitLong(MAX_FILE_SIZE_BITS - FRAGMENT_BITS);
		numFragments = buf.ReadUBitLong(3);

		offset = static_cast<unsigned int>(startFragment * FRAGMENT_SIZE);
		length = numFragments * FRAGMENT_SIZE;
	}

	uint64_t totalBytes = 0;
	if (offset == 0)
	{
		if (bSingleBlock)
		{
			if (buf.ReadOneBit())
				buf.SeekRelative(MAX_FILE_SIZE_BITS);

			totalBytes = buf.ReadVarInt32();
		} else {
			if (buf.ReadOneBit())
			{
				buf.SeekRelative(32);

				char dummyFilename[1024];
				buf.ReadString(dummyFilename, sizeof(dummyFilename));
			}

			if (buf.ReadOneBit())
				buf.SeekRelative(MAX_FILE_SIZE_BITS); // nUncompressedSize

			totalBytes = buf.ReadUBitLong(MAX_FILE_SIZE_BITS);
		}

		if (bSingleBlock)
		{
			numFragments = BYTES2FRAGMENTS(totalBytes);
			length = numFragments * FRAGMENT_SIZE;
		}
	} else
		totalBytes = offset + length;

	if (numFragments > 0)
	{
		uint32_t totalFragments = BYTES2FRAGMENTS(totalBytes);
		if ((startFragment + numFragments) == totalFragments)
		{
			int rest = FRAGMENT_SIZE - (totalBytes % FRAGMENT_SIZE);
			if (rest < FRAGMENT_SIZE)
				length -= rest;
		}
	}

	if (length > 0)
		buf.SeekRelative(length * 8);

	return true;
}

static void EmptyVector(std::vector<INetMessage*> pMessages)
{
	for (INetMessage* msg : pMessages)
		delete msg;
}

static bool ProcessControlMessage(int cmd, bf_read& buf)
{
	if (cmd == net_NOP)
	{
		Msg("holylib_netdebug_readdump: net_NOP\n");
		return true;
	}

	char string[1024];
	if (cmd == net_Disconnect)
	{
		buf.ReadString(string, sizeof(string));
		Msg("holylib_netdebug_readdump: net_Disconnect -> \"%s\"\n", string);
		return false;
	}

	if (cmd == net_File)
	{
		unsigned int transferID = buf.ReadUBitLong(32);
		buf.ReadString(string, sizeof(string));
		bool isRequest = buf.ReadOneBit();
		Msg("holylib_netdebug_readdump: net_File -> %u - \"%s\" - %s\n", transferID, string, isRequest ? "true" : "false");
		return true;
	}

	Msg("holylib_netdebug_readdump: Netchannel: received bad control cmd %i\n", cmd);
	return false;
}

static void ReadDump(const CCommand &args)
{
	if (args.ArgC() != 2)
	{
		Msg("Usage: holylib_netdebug_readdump <filepath>\n");
		return;
	}

	FileHandle_t fh = g_pFullFileSystem->Open(args.Arg(1), "rb", "MOD");
	if (!fh)
	{
		Msg("holylib_netdebug_readdump: Failed to open file!\n");
		return;
	}

	RunCodeAtScopeExit(g_pFullFileSystem->Close(fh));
	size_t size = g_pFullFileSystem->Size(fh);
	if (size < 0)
	{
		Msg("holylib_netdebug_readdump: Failed to get file size!\n");
		return;
	}

	char* pBuffer = new char[size];
	RunCodeAtScopeExit(delete[] pBuffer);

	g_pFullFileSystem->Read(pBuffer, size, fh);

	bf_read packet(pBuffer, size);
	Msg("holylib_netdebug_readdump: Got %i bits!\n", packet.GetNumBitsLeft());

	std::vector<INetMessage*> pMessages;
	pMessages.push_back((INetMessage*)new NET_Tick);
	pMessages.push_back((INetMessage*)new NET_StringCmd);
	pMessages.push_back((INetMessage*)new NET_SetConVar);
	pMessages.push_back((INetMessage*)new NET_SignonState);

	pMessages.push_back((INetMessage*)new SVC_Print);
	pMessages.push_back((INetMessage*)new SVC_ServerInfo);
	pMessages.push_back((INetMessage*)new SVC_SendTable);
	pMessages.push_back((INetMessage*)new SVC_ClassInfo);
	pMessages.push_back((INetMessage*)new SVC_SetPause);
	pMessages.push_back((INetMessage*)new SVC_CreateStringTable);
	pMessages.push_back((INetMessage*)new SVC_UpdateStringTable);
	pMessages.push_back((INetMessage*)new SVC_VoiceInit);
	pMessages.push_back((INetMessage*)new SVC_VoiceData);
	pMessages.push_back((INetMessage*)new SVC_Sounds);
	pMessages.push_back((INetMessage*)new SVC_SetView);
	pMessages.push_back((INetMessage*)new SVC_FixAngle);
	pMessages.push_back((INetMessage*)new SVC_CrosshairAngle);
	pMessages.push_back((INetMessage*)new SVC_BSPDecal);
	pMessages.push_back((INetMessage*)new SVC_GameEvent);
	pMessages.push_back((INetMessage*)new SVC_UserMessage);
	pMessages.push_back((INetMessage*)new SVC_EntityMessage);
	pMessages.push_back((INetMessage*)new SVC_PacketEntities);
	pMessages.push_back((INetMessage*)new SVC_TempEntities);
	pMessages.push_back((INetMessage*)new SVC_Prefetch);
	pMessages.push_back((INetMessage*)new SVC_Menu);
	pMessages.push_back((INetMessage*)new SVC_GameEventList);
	pMessages.push_back((INetMessage*)new SVC_GetCvarValue);
	pMessages.push_back((INetMessage*)new SVC_CmdKeyValues);
	pMessages.push_back((INetMessage*)new SVC_GMod_ServerToClient);

	RunCodeAtScopeExit(EmptyVector(pMessages));

	int sequence = packet.ReadLong();
	int sequence_ack = packet.ReadLong();
	int flags = packet.ReadByte();

	unsigned short usCheckSum = (unsigned short)packet.ReadUBitLong(16);

	int relState = packet.ReadByte();

	int nChoked = 0;
	if (flags & PACKET_FLAG_CHOKED)
		nChoked = packet.ReadByte();

	unsigned int nChallenge = 0;
	if (flags & PACKET_FLAG_CHALLENGE)
		nChallenge = packet.ReadLong();

	Msg("holylib_netdebug_readdump: header:\n\tsequence: %i\n\tsequence_ack: %i\n\tflags: %i\n\tusCheckSum: %i\n\trelState: %i\n",
		sequence, sequence_ack, flags, usCheckSum, relState);

	if (flags == -1)
	{
		Msg("holylib_netdebug_readdump: Invalid packet!\n");
		return;
	}

	if (flags & PACKET_FLAG_RELIABLE)
	{
		int subChannelIndex = packet.ReadUBitLong(3);
		for (int i=0; i<MAX_STREAMS; i++)
		{
			if (packet.ReadOneBit() != 0)
			{
				if (!ReadSubChannelData(packet))
				{
					Msg("holylib_netdebug_readdump: Invalid sub channel data for %i\n", i);
					return;
				}
			}
		}
	}

	if (packet.GetNumBitsLeft() <= 0)
	{
		Msg("holylib_netdebug_readdump: No bits left\n");
		return; // GGs
	}

	Msg("holylib_netdebug_readdump: message start bit %i\n", packet.GetNumBitsRead());
	while (true)
	{
		if (packet.IsOverflowed())
		{
			Msg("holylib_netdebug_readdump: Buffer overflow in net message");
			return;
		}

		if (packet.GetNumBitsLeft() < NETMSG_TYPE_BITS)
		{
			Msg("holylib_netdebug_readdump: No bits left inside loop!\n");
			break;
		}

		unsigned char cmd = packet.ReadUBitLong(NETMSG_TYPE_BITS);
		if (cmd <= net_File)
		{
			if (!ProcessControlMessage(cmd, packet))
			{
				Msg("holylib_netdebug_readdump: Control message failed!\n");
				return;
			}

			continue;
		}

		INetMessage* msg = FindMessage(pMessages, cmd);
		if (!msg)
		{
			Msg("holylib_netdebug_readdump: Netchannel: unknown net message (%i)\n", cmd);
			return;
		}

		int startBit = packet.GetNumBitsRead();
		const char* msgName = msg->GetName();
		if (!msg->ReadFromBuffer(packet))
		{
			Msg("holylib_netdebug_readdump: Netchannel: failed reading message \"%s\"\n", msgName);
			return;
		}

		Msg("holylib_netdebug_readdump: (read %i bits) %s\n", packet.GetNumBitsRead() - startBit, msg->ToString());
	}
}
static ConCommand dumpdt("holylib_netdebug_readdump", ReadDump, "DONT RUN THIS ON PROD! This command tries to read a net dump or it crashes. Good Luck", 0);