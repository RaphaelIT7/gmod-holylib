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
	const char* Name() override { return "networkthreading"; };
	int Compatibility() override { return LINUX32; };
	bool IsEnabledByDefault() override { return true; };
};

static CNetworkThreadingModule g_pNetworkThreadingModule;
IModule* pNetworkThreadingModule = &g_pNetworkThreadingModule;

static ConVar networkthreading_parallelprocessing("holylib_networkthreading_parallelprocessing", "0", 0, "If enabled, some packets will be processed by the networking thread instead of the main thread");

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

static CThreadMutex g_pQueuePacketsMutex;
static std::vector<QueuedPacket*> g_pQueuedPackets;
static void AddPacketToQueueForMainThread(netpacket_s* pPacket, bool bIsConnectionless)
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

	AUTO_LOCK(g_pQueuePacketsMutex);
	g_pQueuedPackets.push_back(pQueue);
}

enum HandleStatus
{
	QUEUE_TO_MAIN, // Queues the packet to be processed by the main thread
	HANDLE_NOW, // Handles the packet in our network thread
	DISCARD, // Discards the packet as it probably has junk
};

// Returning false will result in the packet being put into the queue and let for the main thread to handle.
static HandleStatus ShouldHandlePacket(netpacket_s* pPacket, bool isConnectionless)
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

enum NetworkThreadState
{
	STATE_NOTRUNNING,
	STATE_RUNNING,
	STATE_SHOULD_SHUTDOWN,
};

static std::atomic<int> g_nThreadState = NetworkThreadState::STATE_NOTRUNNING;
static Symbols::NET_GetPacket func_NET_GetPacket;
static Symbols::Filter_SendBan func_Filter_SendBan;
static Symbols::NET_FindNetChannel func_NET_FindNetChannel;
static SIMPLETHREAD_RETURNVALUE NetworkThread(void* pThreadData)
{
	if (!Util::server || !func_NET_GetPacket || !func_Filter_SendBan || !func_NET_FindNetChannel)
	{
		Msg(PROJECT_NAME " - networkthreading: Shutting down thread since were missing some functions!\n");
		g_nThreadState.store(NetworkThreadState::STATE_NOTRUNNING);
		return 0;
	}

	CBaseServer* pServer = (CBaseServer*)Util::server;
	int nSocket = pServer->m_Socket;

	std::unique_ptr<unsigned char[]> pScratchBuffer(new unsigned char[NET_MAX_MESSAGE]);
	unsigned char* pBuffer = pScratchBuffer.get();

	ConVarRef net_showudp("net_showudp");

	netpacket_s* packet;
	while (g_nThreadState.load() == NetworkThreadState::STATE_RUNNING)
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
				packet->message.ReadLong();	// read the -1
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

	g_nThreadState.store(NetworkThreadState::STATE_NOTRUNNING);
	return 0;
}

static Detouring::Hook detour_NET_ProcessSocket;
static std::unordered_set<INetChannel*> g_pNetChannels; // No mutex since only the main thread parties on it... hopefully
static void hook_NET_ProcessSocket(int nSocket, IConnectionlessPacketHandler* pHandler)
{
	if (!Util::server || nSocket != ((CBaseServer*)Util::server)->m_Socket || g_nThreadState.load() == NetworkThreadState::STATE_NOTRUNNING)
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

	AUTO_LOCK(g_pQueuePacketsMutex);
	for (QueuedPacket* pQueuePacket : g_pQueuedPackets)
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
	g_pQueuedPackets.clear();
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

static ThreadHandle_t g_pNetworkThread = nullptr;
void CNetworkThreadingModule::ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
	g_nThreadState.store(NetworkThreadState::STATE_RUNNING);
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
	if (g_nThreadState.load() != NetworkThreadState::STATE_NOTRUNNING)
	{
		g_nThreadState.store(NetworkThreadState::STATE_SHOULD_SHUTDOWN);
		while (g_nThreadState.load() != NetworkThreadState::STATE_NOTRUNNING) // Wait for shutdown
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

	func_Filter_ShouldDiscard = (Symbols::Filter_ShouldDiscard)Detour::GetFunction(engine_loader.GetModule(), Symbols::Filter_ShouldDiscardSym);
	Detour::CheckFunction((void*)func_Filter_ShouldDiscard, "Filter_ShouldDiscard");

	func_Filter_SendBan = (Symbols::Filter_SendBan)Detour::GetFunction(engine_loader.GetModule(), Symbols::Filter_SendBanSym);
	Detour::CheckFunction((void*)func_Filter_SendBan, "Filter_SendBan");

	func_NET_GetPacket = (Symbols::NET_GetPacket)Detour::GetFunction(engine_loader.GetModule(), Symbols::NET_GetPacketSym);
	Detour::CheckFunction((void*)func_NET_GetPacket, "NET_GetPacket");

	func_NET_FindNetChannel = (Symbols::NET_FindNetChannel)Detour::GetFunction(engine_loader.GetModule(), Symbols::NET_FindNetChannelSym);
	Detour::CheckFunction((void*)func_NET_FindNetChannel, "NET_FindNetChannel");

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
}