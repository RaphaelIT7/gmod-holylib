#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "tier0/threadtools.h"
#include "sourcesdk/baseserver.h"
#include "sourcesdk/cnetchan.h"
#include <atomic>
#include <memory>
#include <mutex>
#include "sourcesdk/proto_oob.h"
#include "sourcesdk/net_ws_headers.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#include <netadr_new.h>

/*
	Current purpose:
	We currently handle receiving and filtering packages on a different thread
	allowing the main thread to have less work as it only now has to work off already confirmed packets
	instead of having first to receive all of them and filter them.
*/
class CNetworkThreadingModule : public IModule
{
public:
	void InitDetour(bool bPreServer) override;
	void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax) override;
	void LevelShutdown() override;
	void Think(bool bSimulating) override;
	const char* Name() override { return "networkthreading"; };
	int Compatibility() override { return LINUX32; };
	bool IsEnabledByDefault() override { return true; };
};

static CNetworkThreadingModule g_pNetworkThreadingModule;
IModule* pNetworkThreadingModule = &g_pNetworkThreadingModule;

static ConVar networkthreading_parallelprocessing("holylib_networkthreading_parallelprocessing", "0", 0, "If enabled, some packets will be processed by the networking thread instead of the main thread");
static ConVar networkthreading_forcechallenge("holylib_networkthreading_forcechallenge", "0", 0, "If enabled, clients are ALWAYS requested to have a challenge for A2S requests.");
static ConVar networkthreading_strictpackets("holylib_networkthreading_strictpackets", "1", 0, "If enabled, split packets and compressed packets from non-connected addresses will not be processed and harder limits are enforced.");

// NOTE: There is inside gcsteamdefines.h the AUTO_LOCK_WRITE which we could probably use
//static CThreadRWLock g_pIPFilterMutex; // Idk if using a std::shared_mutex might be faster
static std::shared_mutex g_pIPFilterMutex; // Using it now since the CThreadRWLock caused linker issues and I don't have the nerves rn to deal with that crap
static Symbols::Filter_ShouldDiscard func_Filter_ShouldDiscard;
static bool Filter_ShouldDiscard(const netadr_t& adr)
{
	std::shared_lock<std::shared_mutex> readLock(g_pIPFilterMutex);

	if (func_Filter_ShouldDiscard)
		return func_Filter_ShouldDiscard(adr);

	return false;
}

static Detouring::Hook detour_Filter_Add_f;
static void hook_Filter_Add_f(const CCommand* pCommand)
{
	std::unique_lock<std::shared_mutex> writeLock(g_pIPFilterMutex);
	
	detour_Filter_Add_f.GetTrampoline<Symbols::Filter_Add_f>()(pCommand);
}

static Detouring::Hook detour_removeip;
static void hook_removeip(const CCommand* pCommand)
{
	std::unique_lock<std::shared_mutex> writeLock(g_pIPFilterMutex);

	detour_removeip.GetTrampoline<Symbols::removeip>()(pCommand);
}

static Detouring::Hook detour_writeip;
static void hook_writeip(const CCommand* pCommand)
{
	std::unique_lock<std::shared_mutex> writeLock(g_pIPFilterMutex);

	detour_writeip.GetTrampoline<Symbols::writeip>()(pCommand);
}

static Detouring::Hook detour_listip;
static void hook_listip(const CCommand* pCommand)
{
	// read lock since it doesn't modify it.
	std::shared_lock<std::shared_mutex> readLock(g_pIPFilterMutex);

	detour_listip.GetTrampoline<Symbols::listip>()(pCommand);
}

struct QueuedPacket {
	~QueuedPacket()
	{
		if (pBytes)
		{
			delete[] pBytes;
			pBytes = nullptr;
		}

		if (g_pNetworkThreadingModule.InDebug() == 1)
			Msg(PROJECT_NAME " - networkthreading: Freeing %i bytes after packet was now processed (%p)\n", pPacket.size, this);
	}

	netpacket_s pPacket;
	bool bIsConnectionless = false;
	unsigned char* pBytes = nullptr;
};

static std::mutex g_pQueuePacketsMutex;
static std::vector<QueuedPacket*> g_pQueuedPackets;
static inline void AddPacketToQueueForMainThread(netpacket_s* pPacket, bool bIsConnectionless)
{
	if (pPacket->size > NET_MAX_MESSAGE)
	{
		char pNetAddr[64]; // Needed since else .ToString is not threadsafe!
		(*(netadrnew_s*)&pPacket->from).ToString(pNetAddr, sizeof(pNetAddr), false);
		ConDMsg(PROJECT_NAME " - networkthreading: Unholy Packet size from %s (%i/%i) dropping!\n", pNetAddr, pPacket->size, NET_MAX_MESSAGE);
		return;
	}

	QueuedPacket* pQueue = new QueuedPacket();
	pQueue->pPacket = *pPacket;
	pQueue->bIsConnectionless = bIsConnectionless;
	pQueue->pBytes = new unsigned char[pPacket->size];
	memcpy(pQueue->pBytes, pPacket->data, pPacket->size);

	pQueue->pPacket.data = pQueue->pBytes; // Update the pointer for later access
	pQueue->pPacket.message.StartReading( pQueue->pPacket.data, pQueue->pPacket.size, pPacket->message.GetNumBitsRead() ); // also needs updating
	pQueue->pPacket.pNext = nullptr;

	if (g_pNetworkThreadingModule.InDebug() == 1)
		Msg(PROJECT_NAME " - networkthreading: Added %i bytes packet to queue (%p)\n", pPacket->size, pQueue);

	std::lock_guard<std::mutex> lock(g_pQueuePacketsMutex);
	g_pQueuedPackets.push_back(pQueue);
}

enum HandleStatus
{
	QUEUE_TO_MAIN, // Queues the packet to be processed by the main thread
	HANDLE_NOW, // Handles the packet in our network thread
	DISCARD, // Discards the packet as it probably has junk
};

// Returning false will result in the packet being put into the queue and let for the main thread to handle.
static inline HandleStatus ShouldHandlePacket(netpacket_s* pPacket, bool isConnectionless)
{
	if (isConnectionless)
	{
		bf_read msg = pPacket->message;
		char c = (char)msg.ReadChar();
		if (c == 0) // Junk
			return HandleStatus::DISCARD;

		if (networkthreading_parallelprocessing.GetBool())
		{
			if (c == A2S_GETCHALLENGE || c == A2S_SERVERQUERY_GETCHALLENGE || c == A2S_INFO || c == A2S_PLAYER || c == A2S_RULES)
			{
				return HandleStatus::HANDLE_NOW;
			}
		}
	}

	// For now we just do the receiving and filtering.
	// We don't allow any specific packets yet to be processed threaded.
	return HandleStatus::QUEUE_TO_MAIN;
}

#if MODULE_EXISTS_GAMESERVER
extern ConVar sv_filter_nobanresponse;
#endif

// BUG: GMod's CBaseServer::GetChallengeNr isn't thread safe
static std::atomic<uint32> g_nChallengeNr = 0;
static std::atomic<uint32> g_nLastChallengeNr = 0;
static Symbols::NET_SendPacket func_NET_SendPacket = nullptr;
static inline void SendChallenge(netpacket_s* pPacket)
{
	uint64 challenge = ((uint64)pPacket->from.GetIPNetworkByteOrder() << 32) + g_nChallengeNr.load();
	CRC32_t hash;
	CRC32_Init(&hash);
	CRC32_ProcessBuffer(&hash, &challenge, sizeof(challenge));
	CRC32_Final(&hash);
	int challengeNr = (int)hash;

	CBaseServer* pServer = (CBaseServer*)Util::server;

	ALIGN4 char buffer[16] ALIGN4_POST;
	bf_write msg(buffer,sizeof(buffer));
	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(S2C_CHALLENGE);
	msg.WriteLong(challengeNr);
	func_NET_SendPacket(NULL, pServer->m_Socket, pPacket->from, msg.GetData(), msg.GetNumBytesWritten(), nullptr, false);
}

static inline bool IsValidChallenge(netadr_t &adr, int nChallengeValue)
{
	if (adr.IsLoopback())
		return true;

	if (nChallengeValue == 0xFFFFFFFF)
		return false;

	uint64 challenge = ((uint64)adr.GetIPNetworkByteOrder() << 32) + g_nChallengeNr.load();
	CRC32_t hash;
	CRC32_Init(&hash);
	CRC32_ProcessBuffer(&hash, &challenge, sizeof(challenge));
	CRC32_Final(&hash);
	//Msg("g_nChallengeNr: %i == %i (%s)\n", (int)hash, nChallengeValue, adr.ToString());
	if ((int)hash == nChallengeValue)
		return true;

	challenge &= 0xffffffff00000000ull;
	challenge += g_nLastChallengeNr.load();
	hash = 0;
	CRC32_Init(&hash);
	CRC32_ProcessBuffer(&hash, &challenge, sizeof(challenge));
	CRC32_Final(&hash);

	//Msg("g_nLastChallengeNr: %i == %i (%s)\n", (int)hash, nChallengeValue, adr.ToString());
	return (int)hash == nChallengeValue;
}

static inline int IsValidConnectionlessPacket(netpacket_s* pPacket, bool bOnlyA2S)
{
	char c = (char)pPacket->message.ReadChar();
	if (c == A2S_INFO)
	{
		constexpr int payload = sizeof("Source Engine Query") * 8;
		if (!pPacket->message.SeekRelative(payload))
			return -1;
	} else {
		if (c != A2S_GETCHALLENGE && c != A2S_SERVERQUERY_GETCHALLENGE && c != A2S_PLAYER && c != A2S_RULES && (bOnlyA2S || c != C2S_CONNECT))
			return -1;
	}

	return c;
}

static inline bool EnforceConnectionlessChallenge(netpacket_s* pPacket, int type)
{
	if (!networkthreading_forcechallenge.GetBool() || !func_NET_SendPacket)
		return false;

	if (type != A2S_PLAYER && type != A2S_RULES && type != A2S_INFO)
		return false;

	//Msg("verifying challenge! (%s - %i)\n", pPacket->from.ToString(), type);
	// Now it can only be A2S_INFO, A2S_PLAYER, A2S_RULES
	int challenge = (int)pPacket->message.ReadLong();
	if (IsValidChallenge(pPacket->from, challenge))
		return false;

	//Msg("Requesting challenge! (%s - %i)\n", pPacket->from.ToString(), type);
	SendChallenge(pPacket);
	return true;
}

static std::atomic<int> g_nThreadState = ThreadState::STATE_NOTRUNNING;
static Symbols::NET_GetPacket func_NET_GetPacket;
static Symbols::Filter_SendBan func_Filter_SendBan;
static Symbols::NET_FindNetChannel func_NET_FindNetChannel;
static SIMPLETHREAD_RETURNVALUE NetworkThread(void* pThreadData)
{
	// ThreadSetDebugName(ThreadGetCurrentId(), PROJECT_NAME " - NetworkThread");
	if (!Util::server || !func_NET_GetPacket || !func_Filter_SendBan || !func_NET_FindNetChannel)
	{
		Msg(PROJECT_NAME " - networkthreading: Shutting down thread since were missing some functions!\n");
		g_nThreadState.store(ThreadState::STATE_NOTRUNNING);
		return 0;
	}

	CBaseServer* pServer = (CBaseServer*)Util::server;
	int nSocket = pServer->m_Socket;

	std::unique_ptr<unsigned char[]> pScratchBuffer(new unsigned char[NET_MAX_MESSAGE]);
	unsigned char* pBuffer = pScratchBuffer.get();

	ConVarRef net_showudp("net_showudp");

	netpacket_s* packet;
	while (g_nThreadState.load() == ThreadState::STATE_RUNNING)
	{
		while ((packet = func_NET_GetPacket(nSocket, pBuffer)) != nullptr)
		{
			if (Filter_ShouldDiscard(packet->from)) // filtering is done by network layer
			{
#if MODULE_EXISTS_GAMESERVER
				if (!sv_filter_nobanresponse.GetBool())
#endif
				{
					func_Filter_SendBan(packet->from); // tell them we aren't listening...
				}
				continue;
			} 

			// check for connectionless packet (0xffffffff) first
			if (LittleLong(*(unsigned int *)packet->data) == CONNECTIONLESS_HEADER)
			{
				packet->message.SeekRelative(sizeof(long)*8);	// read the -1
				int nPacketType = IsValidConnectionlessPacket(packet, false);
				if (networkthreading_strictpackets.GetBool() && nPacketType == -1)
					continue;

				if (EnforceConnectionlessChallenge(packet, nPacketType))
					continue;

				packet->message.StartReading( packet->data, packet->size ); // Reset buffer
				if (net_showudp.GetInt())
					Msg("UDP <- %s: sz=%i OOB '%c' wire=%i\n", packet->from.ToString(), packet->size, packet->data[4], packet->wiresize);

				HandleStatus pStatus = ShouldHandlePacket(packet, true);
				if (pStatus == HandleStatus::DISCARD)
					continue;

				if (pStatus == HandleStatus::HANDLE_NOW) {
					pServer->ProcessConnectionlessPacket(packet);
				} else {
					AddPacketToQueueForMainThread(packet, true);
				}
				continue;
			}

			// check for packets from connected clients
			CNetChan* netchan = func_NET_FindNetChannel(nSocket, packet->from);
			if (netchan)
			{
				HandleStatus pStatus = ShouldHandlePacket(packet, false);
				if (pStatus == HandleStatus::HANDLE_NOW) {
					netchan->ProcessPacket(packet, true);
				} else {
					AddPacketToQueueForMainThread(packet, false);
				}
			} else {
				char pNetAddr[64]; // Needed since else .ToString is not threadsafe!
				(*(netadrnew_s*)&packet->from).ToString(pNetAddr, sizeof(pNetAddr), false);
				if (g_pNetworkThreadingModule.InDebug() == 1)
					Msg(PROJECT_NAME " - networkthreading: Discarding of %i bytes since there is no channel for %s!\n", packet->size, pNetAddr);
			}
		}

		ThreadSleep(1); // hoping to not make it use like 100% CPU without slowing down networking
	}

	g_nThreadState.store(ThreadState::STATE_NOTRUNNING);
	return 0;
}

static Detouring::Hook detour_NET_ProcessSocket;
static std::unordered_set<INetChannel*> g_pNetChannels; // No mutex since only the main thread parties on it... hopefully
static void hook_NET_ProcessSocket(int nSocket, IConnectionlessPacketHandler* pHandler)
{
	if (!Util::server || nSocket != ((CBaseServer*)Util::server)->m_Socket || g_nThreadState.load() == ThreadState::STATE_NOTRUNNING)
	{
		detour_NET_ProcessSocket.GetTrampoline<Symbols::NET_ProcessSocket>()(nSocket, pHandler);
		return;
	}

	VPROF_BUDGET("HolyLib - NET_ProcessSocket", VPROF_BUDGETGROUP_HOLYLIB);

	// get streaming data from channel sockets
	// NOTE: This code is probably completely useless since Gmod doesn't use TCP
	for(INetChannel* netchan : g_pNetChannels)
	{
		// sockets must match
		if (nSocket != netchan->GetSocket())
			continue;

		if (!netchan->ProcessStream())
			netchan->GetMsgHandler()->ConnectionCrashed("TCP connection failed.");
	}

	std::vector<QueuedPacket*> pPackets;
	{
		std::lock_guard<std::mutex> lock(g_pQueuePacketsMutex);
		pPackets = std::move(g_pQueuedPackets);
	}

	for (QueuedPacket* pQueuePacket : pPackets)
	{
		if (pQueuePacket->bIsConnectionless) {
			if (g_pNetworkThreadingModule.InDebug() == 1)
				Msg(PROJECT_NAME " - networkthreading: Processed %i bytes as connectionless! (%p)\n", pQueuePacket->pPacket.size, pQueuePacket);

			Util::server->ProcessConnectionlessPacket(&pQueuePacket->pPacket);
		} else {
			if (g_pNetworkThreadingModule.InDebug() == 1)
				Msg(PROJECT_NAME " - networkthreading: Processed %i bytes as net channel! (%p)\n", pQueuePacket->pPacket.size, pQueuePacket);

			CNetChan* netchan = func_NET_FindNetChannel(nSocket, pQueuePacket->pPacket.from);
			if (netchan)
				netchan->ProcessPacket(&pQueuePacket->pPacket, true);
		}

		delete pQueuePacket;
	}
}

static Detouring::Hook detour_CNetChan_Constructor;
static void hook_CNetChan_Constructor(CNetChan* pChannel)
{
	detour_CNetChan_Constructor.GetTrampoline<Symbols::CNetChan_Constructor>()(pChannel);

	auto it = g_pNetChannels.find(pChannel);
	if (it != g_pNetChannels.end())
		return;

	g_pNetChannels.insert(pChannel);
}


static Detouring::Hook detour_NET_RemoveNetChannel;
static void hook_NET_RemoveNetChannel(INetChannel* pChannel, bool bShouldRemove)
{
	detour_NET_RemoveNetChannel.GetTrampoline<Symbols::NET_RemoveNetChannel>()(pChannel, bShouldRemove);

	auto it = g_pNetChannels.find(pChannel);
	if (it == g_pNetChannels.end())
		return;

	g_pNetChannels.erase(it);
	// We don't need to do any cleanup since any packets that can't be passed to a channel since they have been removed are simply dropped.
}

typedef struct
{
	int			nPort;		// UDP/TCP use same port number
	bool		bListening;	// true if TCP port is listening
	int			hUDP;		// handle to UDP socket from socket()
	int			hTCP;		// handle to TCP socket from socket()
} netsocket_t;

struct lzss_header_t
{
	unsigned int	id;
	unsigned int	actualSize;	// always little endian
};

constexpr inline unsigned MAKEUID(char d, char c, char b, char a) noexcept {
  return (static_cast<unsigned>(a) << 24) | (static_cast<unsigned>(b) << 16) |
         (static_cast<unsigned>(c) << 8) | static_cast<unsigned>(d);
}

constexpr unsigned int LZSS_ID{MAKEUID('L', 'Z', 'S', 'S')};
int COM_GetUncompressedSize( IN_BYTECAP(compressedLen) const void *compressed, unsigned int compressedLen )
{
	const lzss_header_t *pHeader = static_cast<const lzss_header_t *>(compressed);

	// Check for our own LZSS compressed data
	if ( ( compressedLen >= sizeof(lzss_header_t) ) && pHeader->id == LZSS_ID )
		return LittleLong( pHeader->actualSize );

	return -1;
}

/*
	Some exploit fix - There was some talk in the Discord, so I guess let's do something about it
	Split packets are a bit expensive, but in any case we should never be receiving split packets from non-connected adresses.
	So let's just block those, as I see absolutely no reason to why we should receive them.

	We also never expect to get a compressed packet from a non-connected address.
	Additionally non-connected addresses usually never send large packets, so we can safely limit size too.
*/

// We do 8+ as padding for slight free room.
// I've used A2S_INFO as the largest one since no other query is larger than that.
static constexpr size_t NET_MAX_CONNECTIONLESS = 1 + sizeof("Source Engine Query") + 4 + 8;

static CUtlVector<netsocket_t>* net_sockets; // This one is mostly thread safe unless NET_AddExtraSocket / NET_RemoveAllExtraSockets are called but those seem completely unused
static Symbols::NET_GetLong func_NET_GetLong = nullptr;
static Symbols::NET_LagPacket func_NET_LagPacket = nullptr;
static Symbols::NET_ErrorString func_NET_ErrorString = nullptr;
static Symbols::NET_GetLastError func_NET_GetLastError = nullptr;
static Symbols::COM_BufferToBufferDecompress func_COM_BufferToBufferDecompress = nullptr;
static constexpr size_t NET_COMPRESSION_STACKBUF_SIZE = 16384;
static Detouring::Hook detour_NET_ReceiveDatagram;
bool hook_NET_ReceiveDatagram(const intp sock, netpacket_t* packet)
{
	VPROF_BUDGET( "NET_ReceiveDatagram", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	CBaseServer* pServer = (CBaseServer*)Util::server;
	if (!pServer || !net_sockets || !func_NET_GetLong || !func_NET_LagPacket || !func_NET_GetLastError || !func_COM_BufferToBufferDecompress || !func_NET_ErrorString)
		return detour_NET_ReceiveDatagram.GetTrampoline<Symbols::NET_ReceiveDatagram>()(sock, packet);

	Assert ( packet );
	Assert ( net_multiplayer );

	struct sockaddr	from;
	socklen_t		fromlen = sizeof(from);
	int	net_socket = (*net_sockets)[packet->source].hUDP;

	int ret = 0;
	{
		VPROF_BUDGET( "recvfrom", VPROF_BUDGETGROUP_OTHER_NETWORKING );
		ret = recvfrom(net_socket, (char *)packet->data, NET_MAX_MESSAGE, 0, &from, (socklen_t*)&fromlen );
	}
	if ( ret >= NET_MIN_MESSAGE )
	{
		packet->wiresize = ret;
		if ( !packet->from.SetFromSockadr( &from ) )
			DevMsg( PROJECT_NAME " - networkthreading: Unable to set IPv4 address with family %hu.\n", from.sa_family );

		packet->size = ret;

		static ConVarRef net_showudp_wire("net_showudp_wire");
		if ( net_showudp_wire.IsValid() && net_showudp_wire.GetBool() )
			Msg( "WIRE:  UDP sz=%d tm=%f rt %f from %s\n", ret, net_time, Plat_FloatTime(), packet->from.ToString() );

		MEM_ALLOC_CREDIT();
		CUtlMemoryFixedGrowable< byte, NET_COMPRESSION_STACKBUF_SIZE > bufVoice( NET_COMPRESSION_STACKBUF_SIZE );

		if ( ret < NET_MAX_MESSAGE )
		{
			bool isConnected = !networkthreading_strictpackets.GetBool() || packet->from.IsLoopback() || func_NET_FindNetChannel(pServer->m_Socket, packet->from);
			char packetType = *((char*)packet->data + sizeof(unsigned int));
			if (!isConnected && ret > NET_MAX_CONNECTIONLESS && packetType != C2S_CONNECT)
			{
				DevMsg(PROJECT_NAME " - networkthreading: Blocked a large packet from an address that is not connected to the server! (%s - %i - %i)\n", packet->from.ToString(), ret, (int)packetType);
				return false;
			}

			// Check for split message
			if ( LittleLong( *(int *)packet->data ) == NET_HEADER_FLAG_SPLITPACKET )	
			{
				if (!isConnected)
				{
					DevMsg(PROJECT_NAME " - networkthreading: Blocked a split packet from an address that is not connected to the server! (%s)\n", packet->from.ToString());
					return false;
				}

				if ( !func_NET_GetLong( sock, packet ) )
					return false;
			}
			
			// Next check for compressed message
			if ( LittleLong( *(int *)packet->data) == NET_HEADER_FLAG_COMPRESSEDPACKET )
			{
				if (!isConnected)
				{
					DevMsg(PROJECT_NAME " - networkthreading: Blocked a compressed packet from an address that is not connected to the server! (%s)\n", packet->from.ToString());
					return false;
				}

				char *pCompressedData = (char*)packet->data + sizeof( unsigned int );
				unsigned nCompressedDataSize = packet->wiresize - sizeof( unsigned int );

				// Decompress
				int actualSize = COM_GetUncompressedSize( pCompressedData, nCompressedDataSize );
				if ( actualSize <= 0 || actualSize > NET_MAX_MESSAGE )
					return false;

				MEM_ALLOC_CREDIT();
				CUtlMemoryFixedGrowable< byte, NET_COMPRESSION_STACKBUF_SIZE > memDecompressed( NET_COMPRESSION_STACKBUF_SIZE );
				memDecompressed.EnsureCapacity( actualSize );

				unsigned uDecompressedSize = (unsigned)actualSize;
				func_COM_BufferToBufferDecompress( memDecompressed.Base(), &uDecompressedSize, pCompressedData, nCompressedDataSize );
				if ( uDecompressedSize == 0 || ((unsigned int)actualSize) != uDecompressedSize )
				{
					static ConVarRef net_showudp("net_showudp");
					if ( net_showudp.IsValid() && net_showudp.GetBool() )
					{
						DevMsg(PROJECT_NAME " - UDP :  discarding %d bytes from %s due to decompression error [%d decomp, actual %d] at tm=%f rt=%f\n", ret, packet->from.ToString(), uDecompressedSize, actualSize, 
							net_time, Plat_FloatTime() );
					}
					return false;
				}

				// packet->wiresize is already set
				Q_memcpy( packet->data, memDecompressed.Base(), uDecompressedSize );

				packet->size = uDecompressedSize;
			}

			return func_NET_LagPacket( true, packet );
		}
		else
		{
			ConDMsg(PROJECT_NAME " - networkthreading(NET_ReceiveDatagram):  Oversize packet from %s\n", packet->from.ToString() );
		}
	}
	else if ( ret == -1  ) // error?
	{
		const int net_error = func_NET_GetLastError();
		switch ( net_error )
		{
		case WSAEWOULDBLOCK:
		case WSAECONNRESET:
		case WSAECONNREFUSED:
			break;
		case WSAEMSGSIZE:
			ConDMsg ("NET_ReceivePacket: %s\n", func_NET_ErrorString(net_error));
			break;
		default:
			// Let's continue even after errors
			ConDMsg ("NET_ReceivePacket: %s\n", func_NET_ErrorString(net_error));
			break;
		}
	}

	return false;
}

void CNetworkThreadingModule::Think(bool bSimulating)
{
	CBaseServer* pServer = (CBaseServer*)Util::server;
	if (pServer)
	{
		g_nChallengeNr.store(pServer->m_CurrentRandomNonce);
		g_nLastChallengeNr.store(pServer->m_LastRandomNonce);
	}
}

static ThreadHandle_t g_pNetworkThread = nullptr;
void CNetworkThreadingModule::ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
	CBaseServer* pServer = (CBaseServer*)Util::server;
	if (pServer)
	{
		g_nChallengeNr.store(pServer->m_CurrentRandomNonce);
		g_nLastChallengeNr.store(pServer->m_LastRandomNonce);
	}

	g_nThreadState.store(ThreadState::STATE_RUNNING);
	if (g_pNetworkThread == nullptr)
	{
		ConDMsg(PROJECT_NAME " - networkthreading: Starting network thread...\n");
		g_pNetworkThread = CreateSimpleThread((ThreadFunc_t)NetworkThread, nullptr);
	}
}

void CNetworkThreadingModule::LevelShutdown()
{
	if (g_pNetworkThread == nullptr)
		return;

	ConDMsg(PROJECT_NAME " - networkthreading: Stopping network thread...\n");
	if (g_nThreadState.load() != ThreadState::STATE_NOTRUNNING)
	{
		g_nThreadState.store(ThreadState::STATE_SHOULD_SHUTDOWN);
		while (g_nThreadState.load() != ThreadState::STATE_NOTRUNNING) // Wait for shutdown
			ThreadSleep(0);
	}
	ReleaseThreadHandle(g_pNetworkThread);
	g_pNetworkThread = nullptr;
}

void CNetworkThreadingModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::FactoryLoader engine_loader("engine");
	Detour::Create(
		&detour_NET_ProcessSocket, "NET_ProcessSocket",
		engine_loader.GetModule(), Symbols::NET_ProcessSocketSym,
		(void*)hook_NET_ProcessSocket, m_pID
	);

	Detour::Create(
		&detour_CNetChan_Constructor, "CNetChan_Constructor",
		engine_loader.GetModule(), Symbols::CNetChan_ConstructorSym,
		(void*)hook_CNetChan_Constructor, m_pID
	);

	Detour::Create(
		&detour_NET_RemoveNetChannel, "NET_RemoveNetChannel",
		engine_loader.GetModule(), Symbols::NET_RemoveNetChannelSym,
		(void*)hook_NET_RemoveNetChannel, m_pID
	);

	Detour::Create(
		&detour_NET_ReceiveDatagram, "NET_ReceiveDatagram",
		engine_loader.GetModule(), Symbols::NET_ReceiveDatagramSym,
		(void*)hook_NET_ReceiveDatagram, m_pID
	);

	func_NET_GetLastError = (Symbols::NET_GetLastError)Detour::GetFunction(engine_loader.GetModule(), Symbols::NET_GetLastErrorSym);
	Detour::CheckFunction((void*)func_NET_GetLastError, "NET_GetLastError");

	func_NET_ErrorString = (Symbols::NET_ErrorString)Detour::GetFunction(engine_loader.GetModule(), Symbols::NET_ErrorStringSym);
	Detour::CheckFunction((void*)func_NET_ErrorString, "NET_ErrorString");

	func_NET_GetLong = (Symbols::NET_GetLong)Detour::GetFunction(engine_loader.GetModule(), Symbols::NET_GetLongSym);
	Detour::CheckFunction((void*)func_NET_GetLong, "NET_GetLong");

	func_NET_LagPacket = (Symbols::NET_LagPacket)Detour::GetFunction(engine_loader.GetModule(), Symbols::NET_LagPacketSym);
	Detour::CheckFunction((void*)func_NET_LagPacket, "NET_LagPacket");

	func_COM_BufferToBufferDecompress = (Symbols::COM_BufferToBufferDecompress)Detour::GetFunction(engine_loader.GetModule(), Symbols::COM_BufferToBufferDecompressSym);
	Detour::CheckFunction((void*)func_COM_BufferToBufferDecompress, "COM_BufferToBufferDecompress");

	func_Filter_ShouldDiscard = (Symbols::Filter_ShouldDiscard)Detour::GetFunction(engine_loader.GetModule(), Symbols::Filter_ShouldDiscardSym);
	Detour::CheckFunction((void*)func_Filter_ShouldDiscard, "Filter_ShouldDiscard");

	func_Filter_SendBan = (Symbols::Filter_SendBan)Detour::GetFunction(engine_loader.GetModule(), Symbols::Filter_SendBanSym);
	Detour::CheckFunction((void*)func_Filter_SendBan, "Filter_SendBan");

	func_NET_GetPacket = (Symbols::NET_GetPacket)Detour::GetFunction(engine_loader.GetModule(), Symbols::NET_GetPacketSym);
	Detour::CheckFunction((void*)func_NET_GetPacket, "NET_GetPacket");

	func_NET_FindNetChannel = (Symbols::NET_FindNetChannel)Detour::GetFunction(engine_loader.GetModule(), Symbols::NET_FindNetChannelSym);
	Detour::CheckFunction((void*)func_NET_FindNetChannel, "NET_FindNetChannel");

	func_NET_SendPacket = (Symbols::NET_SendPacket)Detour::GetFunction(engine_loader.GetModule(), Symbols::NET_SendPacketSym);
	Detour::CheckFunction((void*)func_NET_SendPacket, "NET_SendPacket");

	// Command detours to make g_IPFilters threadsafe by applying a mutex
	Detour::Create(
		&detour_Filter_Add_f, "Filter_Add_f",
		engine_loader.GetModule(), Symbols::Filter_Add_fSym,
		(void*)hook_Filter_Add_f, m_pID
	);

	Detour::Create(
		&detour_removeip, "removeip",
		engine_loader.GetModule(), Symbols::removeipSym,
		(void*)hook_removeip, m_pID
	);

	Detour::Create(
		&detour_listip, "listip",
		engine_loader.GetModule(), Symbols::listipSym,
		(void*)hook_listip, m_pID
	);

	Detour::Create(
		&detour_writeip, "writeip",
		engine_loader.GetModule(), Symbols::writeipSym,
		(void*)hook_writeip, m_pID
	);

	net_sockets = Detour::ResolveSymbol<CUtlVector<netsocket_t>>(engine_loader, Symbols::net_socketsSym);
	Detour::CheckValue("get variable", "net_sockets", net_sockets != nullptr);
}