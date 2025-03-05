#include "filesystem_base.h" // Has to be before symbols.h
#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "player.h"
#include "sourcesdk/hltvserver.h"
#include <unordered_map>
#include <unordered_set>
#include "usermessages.h"
#include "sourcesdk/hltvdirector.h"

class CSourceTVLibModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "sourcetv"; };
	virtual int Compatibility() { return LINUX32; };
};

static ConVar sourcetv_allownetworking("holylib_sourcetv_allownetworking", "0", 0, "Allows HLTV Clients to send net messages to the server.");
static ConVar sourcetv_allowcommands("holylib_sourcetv_allowcommands", "0", 0, "Allows HLTV Clients to send commands to the server.");

static CSourceTVLibModule g_pSourceTVLibModule;
IModule* pSourceTVLibModule = &g_pSourceTVLibModule;

// NOTE: If in the future, Rubat changes the CHLTVServer class, just get the symbols instead of recreating the functions. 
// Using the original function is in most cases better and easier.
CDemoFile *CHLTVDemoRecorder::GetDemoFile()
{
	return &m_DemoFile;
}

bool CHLTVDemoRecorder::IsRecording()
{
	return m_bIsRecording;
}

bool CHLTVServer::IsTVRelay()
{
	return !IsMasterProxy();
}

int CHLTVServer::GetHLTVSlot()
{
	return m_nPlayerSlot;
}

CHLTVServer* hltv = NULL;
static Detouring::Hook detour_CHLTVServer_CHLTVServer;
static void hook_CHLTVServer_CHLTVServer(CHLTVServer* srv)
{
	hltv = srv;
	detour_CHLTVServer_CHLTVServer.GetTrampoline<Symbols::CHLTVServer_CHLTVServer>()(srv);
}

static Detouring::Hook detour_CHLTVServer_DestroyCHLTVServer;
static void hook_CHLTVServer_DestroyCHLTVServer(CHLTVServer* srv)
{
	hltv = NULL;
	detour_CHLTVServer_DestroyCHLTVServer.GetTrampoline<Symbols::CHLTVServer_DestroyCHLTVServer>()(srv);
}

int CHLTVClient_TypeID = -1;
PushReferenced_LuaClass(CHLTVClient, CHLTVClient_TypeID)
Get_LuaClass(CHLTVClient, CHLTVClient_TypeID, "CHLTVClient")

static Detouring::Hook detour_CHLTVClient_Deconstructor;
static void hook_CHLTVClient_Deconstructor(CHLTVClient* pClient)
{
	Delete_CHLTVClient(pClient); // ToDo: Remind myself to remove this since CHLTVClient's won't be deleted while the server is running. It's reusing them.
	detour_CHLTVClient_Deconstructor.GetTrampoline<Symbols::CHLTVClient_Deconstructor>()(pClient);
}

void SourceTV_OnClientDisconnect(CBaseClient* pClient)
{
	if (((CHLTVServer*)pClient->GetServer()) != hltv)
		return;

	if (g_Lua && Lua::PushHook("HolyLib:OnSourceTVClientDisconnect"))
	{
		Push_CHLTVClient((CHLTVClient*)pClient);
		g_Lua->CallFunctionProtected(2, 0, true);
	}

	Delete_CHLTVClient((CHLTVClient*)pClient);
}

LUA_FUNCTION_STATIC(CHLTVClient__tostring)
{
	CHLTVClient* pClient = Get_CHLTVClient(1, false);
	if (!pClient || !pClient->IsConnected())
	{
		LUA->PushString("CHLTVClient [NULL]");
	} else {
		char szBuf[128] = {};
		V_snprintf(szBuf, sizeof(szBuf), "CHLTVClient [%i][%s]", pClient->GetPlayerSlot(), pClient->GetClientName());
		LUA->PushString(szBuf);
	}

	return 1;
}

/*
 * Yes, this will be a memory leak.
 * But it's the easiest way and the leak is only for each new client connection
 * and it'll only leak like 20 bytes or so(idk the amount).
 * 
 * We can clear it later by looping thru all clients and then nuking and recreating it with only the current clients values.
 */
static std::unordered_map<int, int> g_iTarget;
LUA_FUNCTION_STATIC(CHLTVClient_SetCameraMan)
{
	CHLTVClient* pClient = Get_CHLTVClient(1, true);

	int iTarget = 0;
	if (LUA->IsType(2, GarrysMod::Lua::Type::Number))
		iTarget = LUA->GetNumber(2);
	else {
		CBaseEntity* pEnt = Util::Get_Entity(2, false);
		iTarget = pEnt ? pEnt->edict()->m_EdictIndex : 0; // If given NULL, set it to 0.
	}
	g_iTarget[pClient->GetUserID()] = iTarget;

	IGameEvent* pEvent = Util::gameeventmanager->CreateEvent("hltv_cameraman");
	if (pEvent)
	{
		pEvent->SetInt("index", iTarget);
		pClient->FireGameEvent(pEvent);
		Util::gameeventmanager->FreeEvent(pEvent);
	}

	return 0;
}

#define LUA_RECORD_OK 0
#define LUA_RECORD_NOSOURCETV -1
#define LUA_RECORD_NOTMASTER -2
#define LUA_RECORD_ACTIVE -3
#define LUA_RECORD_NOTACTIVE -4
#define LUA_RECORD_INVALIDPATH -5
#define LUA_RECORD_FILEEXISTS -6
LUA_FUNCTION_STATIC(sourcetv_IsActive)
{
	if (hltv)
		LUA->PushBool(hltv->IsActive());
	else
		LUA->PushBool(false);
	
	return 1;
}

LUA_FUNCTION_STATIC(sourcetv_IsRecording)
{
	if (hltv)
		LUA->PushBool(hltv->m_DemoRecorder.IsRecording());
	else
		LUA->PushBool(false);
	
	return 1;
}

LUA_FUNCTION_STATIC(sourcetv_IsMasterProxy)
{
	if (hltv)
		LUA->PushBool(hltv->IsMasterProxy());
	else
		LUA->PushBool(false);
	
	return 1;
}

LUA_FUNCTION_STATIC(sourcetv_IsRelay)
{
	if (hltv)
		LUA->PushBool(hltv->IsTVRelay());
	else
		LUA->PushBool(false);
	
	return 1;
}

LUA_FUNCTION_STATIC(sourcetv_GetClientCount)
{
	if (hltv)
		LUA->PushNumber(hltv->GetClientCount());
	else
		LUA->PushNumber(0);
	
	return 1;
}

LUA_FUNCTION_STATIC(sourcetv_GetHLTVSlot)
{
	if (hltv)
		LUA->PushNumber(hltv->GetHLTVSlot());
	else
		LUA->PushNumber(0);
	
	return 1;
}

static Symbols::COM_IsValidPath func_COM_IsValidPath;
static Symbols::CHLTVDemoRecorder_StartRecording func_CHLTVDemoRecorder_StartRecording;
LUA_FUNCTION_STATIC(sourcetv_StartRecord)
{
	const char* strFileName = LUA->CheckString(1);

	if (!hltv || !hltv->IsActive())
	{
		LUA->PushNumber(LUA_RECORD_NOSOURCETV);
		return 1;
	}

	if (!hltv->IsMasterProxy())
	{
		LUA->PushNumber(LUA_RECORD_NOTMASTER);
		return 1;
	}

	if (hltv->m_DemoRecorder.IsRecording())
	{
		LUA->PushNumber(LUA_RECORD_ACTIVE);
		return 1;
	}

	if (!func_COM_IsValidPath(strFileName))
	{
		LUA->PushNumber(LUA_RECORD_INVALIDPATH);
		return 1;
	}
 
	char strName[MAX_OSPATH];
	Q_strncpy(strName, strFileName, sizeof(strName));
	Q_DefaultExtension(strName, ".dem", sizeof(strName));

	if (g_pFullFileSystem->FileExists(strName))
	{
		LUA->PushNumber(LUA_RECORD_FILEEXISTS);
		return 1;
	}

	if (!func_CHLTVDemoRecorder_StartRecording)
		LUA->ThrowError("Failed to get CHLTVDemoRecorder::StartRecording!");

	func_CHLTVDemoRecorder_StartRecording(&hltv->m_DemoRecorder, strName, false);
	LUA->PushNumber(LUA_RECORD_OK);

	return 1;
}

LUA_FUNCTION_STATIC(sourcetv_GetRecordingFile)
{
	if (hltv && hltv->m_DemoRecorder.IsRecording())
		LUA->PushString(hltv->m_DemoRecorder.GetDemoFile()->m_szFileName);
	else
		LUA->PushNil();
	
	return 1;
}

static Symbols::CHLTVDemoRecorder_StopRecording func_CHLTVDemoRecorder_StopRecording;
LUA_FUNCTION_STATIC(sourcetv_StopRecord)
{
	if (!hltv || !hltv->IsActive())
	{
		LUA->PushNumber(LUA_RECORD_NOSOURCETV);
		return 1;
	}

	if (!hltv->IsMasterProxy())
	{
		LUA->PushNumber(LUA_RECORD_NOTMASTER);
		return 1;
	}

	if (!hltv->m_DemoRecorder.IsRecording())
	{
		LUA->PushNumber(LUA_RECORD_NOTACTIVE);
		return 1;
	}

	if (!func_CHLTVDemoRecorder_StopRecording)
		LUA->ThrowError("Failed to get CHLTVDemoRecorder::StopRecording!");

	func_CHLTVDemoRecorder_StopRecording(&hltv->m_DemoRecorder);
	LUA->PushNumber(LUA_RECORD_OK);

	return 1;
}

LUA_FUNCTION_STATIC(sourcetv_GetAll)
{
	LUA->CreateTable();
		if (!hltv || !hltv->IsActive())
			return 1;

		int iTableIndex = 0;
		for (int iClientIndex=0; iClientIndex<hltv->GetClientCount(); ++iClientIndex)
		{
			CHLTVClient* pClient = hltv->Client(iClientIndex);
			if (!pClient->IsConnected())
				continue;

			Push_CHLTVClient(pClient);
			Util::RawSetI(LUA, -2, ++iTableIndex);
		}

	return 1;
}

LUA_FUNCTION_STATIC(sourcetv_GetClient)
{
	if (!hltv || !hltv->IsActive())
	{
		LUA->PushNil();
		return 1;
	}

	int iClientIndex = (int)LUA->CheckNumber(1);
	if (iClientIndex >= hltv->GetClientCount())
	{
		LUA->PushNil();
		return 1;
	}

	CHLTVClient* pClient = hltv->Client(iClientIndex);
	if (pClient && !pClient->IsConnected())
		pClient = NULL;

	Push_CHLTVClient(pClient);

	return 1;
}

static bool g_bLuaGameEvent = false; // Set to true if you call FireGameEvent on the hltv server if you don't want your event to be potentially blocked / not sent to specific clients.
LUA_FUNCTION_STATIC(sourcetv_FireEvent)
{
	if (!hltv || !hltv->IsActive())
		return 0;

	IGameEvent* pEvent = Get_IGameEvent(1, true);
	g_bLuaGameEvent = LUA->GetBool(2);
	hltv->FireGameEvent(pEvent);
	g_bLuaGameEvent = false;

	return 0;
}

LUA_FUNCTION_STATIC(sourcetv_SetCameraMan)
{
	int iTarget = 0;
	if (LUA->IsType(1, GarrysMod::Lua::Type::Number))
		iTarget = LUA->GetNumber(1);
	else {
		CBaseEntity* pEnt = Util::Get_Entity(1, false);
		iTarget = pEnt ? pEnt->edict()->m_EdictIndex : 0; // If given NULL, set it to 0.
	}

	for (int i = 0; i < hltv->GetClientCount(); ++i)
	{
		CHLTVClient* pClient = hltv->Client(i);
		if (!pClient->IsConnected())
			continue;

		g_iTarget[pClient->GetUserID()] = iTarget;
	}

	IGameEvent* pEvent = Util::gameeventmanager->CreateEvent("hltv_cameraman");
	if (pEvent)
	{
		pEvent->SetInt("index", iTarget);
		hltv->BroadcastEvent(pEvent);
		Util::gameeventmanager->FreeEvent(pEvent);
	}

	return 0;
}

static CModule* g_pBitBufModule;
static Detouring::Hook detour_CHLTVClient_ProcessGMod_ClientToServer;
static bool hook_CHLTVClient_ProcessGMod_ClientToServer(CHLTVClient* pClient, CLC_GMod_ClientToServer* pBf)
{
	VPROF_BUDGET("HolyLib - CHLTVClient::ProcessGMod_ClientToServer", VPROF_BUDGETGROUP_HOLYLIB);

	if (!sourcetv_allownetworking.GetBool())
		return true;

	if (!g_pBitBufModule)
	{
		g_pBitBufModule = (CModule*)g_pModuleManager.FindModuleByName("bitbuf");
		if (!g_pBitBufModule)
		{
			Warning("HolyLib (sourcetv): Failed to find bitbuf module?\n");
			return true;
		}
	}

	if (!g_pBitBufModule->IsEnabled()) // This relies on the bitbuf module.
		return true;

	pBf->m_DataIn.Seek(0);
	int iType = pBf->m_DataIn.ReadUBitLong(4);
	if (iType != 2) // Only handle type 2 -> Lua net message.
		return true;

	pBf->m_DataIn.ReadUBitLong(8);
	pBf->m_DataIn.ReadUBitLong(22); // Skiping to the header
	//pBf->m_DataIn.ReadBitLong(16, false); // The header -> the string. Why not an 12 bits? (This will be read by net.ReadHeader())

	if (Lua::PushHook("HolyLib:OnSourceTVNetMessage")) // Maybe change the name? I don't have a better one rn :/
	{
		Push_CHLTVClient(pClient);
		Push_bf_read(&pBf->m_DataIn);
		g_Lua->Push(-1);
		int iReference = Util::ReferenceCreate("ProcessGMod_ClientToServer - net message buffer");
		g_Lua->CallFunctionProtected(3, 0, true);
		Util::ReferencePush(g_Lua, iReference);
		g_Lua->SetUserType(-1, NULL); // Make sure that the we don't keep the buffer.
		g_Lua->Pop(1);
		Util::ReferenceFree(iReference, "ProcessGMod_ClientToServer - Free net message buffer");
	}

	return true;
}

static Detouring::Hook detour_CHLTVClient_ExecuteStringCommand;
static bool hook_CHLTVClient_ExecuteStringCommand(CHLTVClient* pClient, const char* pCommandString)
{
	VPROF_BUDGET("HolyLib - CHLTVClient::ExecuteStringCommand", VPROF_BUDGETGROUP_HOLYLIB);

	if (!sourcetv_allowcommands.GetBool())
		return detour_CHLTVClient_ExecuteStringCommand.GetTrampoline<Symbols::CHLTVClient_ExecuteStringCommand>()(pClient, pCommandString);

	CCommand pCommandArgs;
	if (!pCommandArgs.Tokenize(pCommandString))
		return true;

	if (Lua::PushHook("HolyLib:OnSourceTVCommand")) // Maybe change the name? I don't have a better one rn :/
	{
		Push_CHLTVClient(pClient);
		g_Lua->PushString(pCommandArgs[0]); // cmd
		g_Lua->PreCreateTable(pCommandArgs.ArgC(), 0);
			for (int i=1; i< pCommandArgs.ArgC(); ++i) // skip cmd -> 0
			{
				g_Lua->PushString(pCommandArgs.Arg(i));
				Util::RawSetI(g_Lua, -2, i);
			}
		g_Lua->PushString(pCommandArgs.ArgS());
		if (g_Lua->CallFunctionProtected(5, 1, true))
		{
			bool handled = g_Lua->GetBool(-1); // If true was returned, the command was handled.
			g_Lua->Pop(1);
			if (handled)
				return true;
		}
	}

	// Fallback.
	return detour_CHLTVClient_ExecuteStringCommand.GetTrampoline<Symbols::CHLTVClient_ExecuteStringCommand>()(pClient, pCommandString);
}

extern CGlobalVars* gpGlobals;
static Detouring::Hook detour_CHLTVDirector_StartNewShot;
static void hook_CHLTVDirector_StartNewShot(CHLTVDirector* director)
{
	VPROF_BUDGET("HolyLib - CHLTVDirector::StartNewShot", VPROF_BUDGETGROUP_HOLYLIB);

	if (Lua::PushHook("HolyLib:OnSourceTVStartNewShot"))
	{
		if (g_Lua->CallFunctionProtected(1, 1, true))
		{
			bool bCancel = g_Lua->GetBool(-1);
			g_Lua->Pop(1);

			if (bCancel)
			{
				int iSmallestTick = MAX(0, gpGlobals->tickcount - TIME_TO_TICKS(HLTV_MAX_DELAY));
				director->RemoveEventsFromHistory(iSmallestTick);
				director->m_nNextShotTick = director->m_nBroadcastTick + TIME_TO_TICKS(MAX_SHOT_LENGTH);
				return;
			}
		}
	}

	detour_CHLTVDirector_StartNewShot.GetTrampoline<Symbols::CHLTVDirector_StartNewShot>()(director);
}

static ConVar* ref_tv_debug;
static Detouring::Hook detour_CHLTVServer_BroadcastEvent;
/*static std::unordered_set<std::string> g_pShotEvents = {
	"hltv_fixed",
	"hltv_chase",
	"hltv_cameraman",
};*/
static void hook_CHLTVServer_BroadcastEvent(CHLTVServer* pServer, IGameEvent* pEvent) // NOTE: We fully detour this one. We never call the original function.
{
	VPROF_BUDGET("HolyLib - CHLTVServer::BroadcastEvent", VPROF_BUDGETGROUP_HOLYLIB);

	char buffer_data[MAX_EVENT_BYTES];
	SVC_GameEvent eventMsg;
	eventMsg.m_DataOut.StartWriting(buffer_data, sizeof(buffer_data));

	if (!Util::gameeventmanager->SerializeEvent(pEvent, &eventMsg.m_DataOut))
	{
		DevMsg("CHLTVServer: failed to serialize event '%s'.\n", pEvent->GetName());
		return;
	}

	const char* pEventName = pEvent->GetName();
	bool bIsShotEvent =	V_stricmp(pEventName, "hltv_fixed") == 0 ||
				V_stricmp(pEventName, "hltv_chase") == 0 ||
				V_stricmp(pEventName, "hltv_cameraman") == 0; // Is it faster to use the unordered_set?

	// Now we can control which gameevents are sent and to which clients.
	// We can use this to block the CHLTVDirector from manipulating specific clients where we want manual control.

	// Below is the implementation of BroadcastMessage that we will adjust as needed.
	bool bReliable = false;
	bool bOnlyActive = true;
	for (int iClientIndex=0; iClientIndex<pServer->GetClientCount(); ++iClientIndex)
	{
		CHLTVClient* pClient = pServer->Client(iClientIndex);

		if ((bOnlyActive && !pClient->IsActive()) || !pClient->IsSpawned())
			continue;

		if (!g_bLuaGameEvent && bIsShotEvent) // If you fire lua gameevents, they can take control.
		{
			auto it = g_iTarget.find(pClient->GetUserID());
			if (it != g_iTarget.end() && it->second != 0) // target id is not 0 so this client has an active target.
				continue;
		}

		if (!pClient->SendNetMsg(eventMsg, bReliable) && (eventMsg.IsReliable() || bReliable))
			DevMsg("BroadcastMessage: Reliable broadcast message overflow for client %s", pClient->GetClientName());
	}

	if (ref_tv_debug && ref_tv_debug->GetBool())
		Msg("SourceTV broadcast event: %s\n", pEvent->GetName());
}

extern int CBaseClient_TypeID; // Now we need to make sure gameserevr module is loaded before sourcetv.
extern void Push_CBaseClientMeta();
void CSourceTVLibModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	ref_tv_debug = cvar->FindVar("tv_debug"); // We only search for it once. Verify: ConVarRef would always search for it in it's constructor/Init if I remember correctly.

	CHLTVClient_TypeID = g_Lua->CreateMetaTable("CHLTVClient");
		Push_CBaseClientMeta();

		Util::AddFunc(CHLTVClient__tostring, "__tostring");
		Util::AddFunc(CHLTVClient_SetCameraMan, "SetCameraMan");
	g_Lua->Pop(1);

	Util::StartTable();
		Util::AddFunc(sourcetv_IsActive, "IsActive");
		Util::AddFunc(sourcetv_IsRecording, "IsRecording");
		Util::AddFunc(sourcetv_IsMasterProxy, "IsMasterProxy");
		Util::AddFunc(sourcetv_IsRelay, "IsRelay");
		Util::AddFunc(sourcetv_GetClientCount, "GetClientCount");
		Util::AddFunc(sourcetv_GetHLTVSlot, "GetHLTVSlot");
		Util::AddFunc(sourcetv_StartRecord, "StartRecord");
		Util::AddFunc(sourcetv_GetRecordingFile, "GetRecordingFile");
		Util::AddFunc(sourcetv_StopRecord, "StopRecord");
		Util::AddFunc(sourcetv_FireEvent, "FireEvent");
		Util::AddFunc(sourcetv_SetCameraMan, "SetCameraMan");

		// Client Functions
		Util::AddFunc(sourcetv_GetAll, "GetAll");
		Util::AddFunc(sourcetv_GetClient, "GetClient");

		Util::AddValue(LUA_RECORD_OK, "RECORD_OK");

		Util::AddValue(LUA_RECORD_NOSOURCETV, "RECORD_NOSOURCETV");
		Util::AddValue(LUA_RECORD_NOTMASTER, "RECORD_NOTMASTER");
		Util::AddValue(LUA_RECORD_ACTIVE, "RECORD_ACTIVE");
		Util::AddValue(LUA_RECORD_NOTACTIVE, "RECORD_NOTACTIVE");
		Util::AddValue(LUA_RECORD_INVALIDPATH, "RECORD_INVALIDPATH");
		Util::AddValue(LUA_RECORD_FILEEXISTS, "RECORD_FILEEXISTS");
	Util::FinishTable("sourcetv");
}

void CSourceTVLibModule::LuaShutdown()
{
	Util::NukeTable("sourcetv");
	g_pPushedCHLTVClient.clear();
}

void CSourceTVLibModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::ModuleLoader engine_loader("engine");
	Detour::Create(
		&detour_CHLTVClient_ProcessGMod_ClientToServer, "CHLTVClient::ProcessGMod_ClientToServer",
		engine_loader.GetModule(), Symbols::CHLTVClient_ProcessGMod_ClientToServerSym,
		(void*)hook_CHLTVClient_ProcessGMod_ClientToServer, m_pID
	);

	Detour::Create(
		&detour_CHLTVClient_ExecuteStringCommand, "CHLTVClient::ExecuteStringCommand",
		engine_loader.GetModule(), Symbols::CHLTVClient_ExecuteStringCommandSym,
		(void*)hook_CHLTVClient_ExecuteStringCommand, m_pID
	);

	Detour::Create(
		&detour_CHLTVServer_CHLTVServer, "CHLTVServer::CHLTVServer",
		engine_loader.GetModule(), Symbols::CHLTVServer_CHLTVServerSym,
		(void*)hook_CHLTVServer_CHLTVServer, m_pID
	);

	Detour::Create(
		&detour_CHLTVServer_DestroyCHLTVServer, "CHLTVServer::~CHLTVServer",
		engine_loader.GetModule(), Symbols::CHLTVServer_DestroyCHLTVServerSym,
		(void*)hook_CHLTVServer_DestroyCHLTVServer, m_pID
	);

	Detour::Create(
		&detour_CHLTVClient_Deconstructor, "CHLTVClient::~CHLTVClient",
		engine_loader.GetModule(), Symbols::CHLTVClient_DeconstructorSym,
		(void*)hook_CHLTVClient_Deconstructor, m_pID
	);

	Detour::Create(
		&detour_CHLTVServer_BroadcastEvent, "CHLTVServer::BroadcastEvent",
		engine_loader.GetModule(), Symbols::CHLTVServer_BroadcastEventSym,
		(void*)hook_CHLTVServer_BroadcastEvent, m_pID
	);

	SourceSDK::ModuleLoader server_loader("server");
	Detour::Create(
		&detour_CHLTVDirector_StartNewShot, "CHLTVDirector::StartNewShot",
		server_loader.GetModule(), Symbols::CHLTVDirector_StartNewShotSym,
		(void*)hook_CHLTVDirector_StartNewShot, m_pID
	);

	func_CHLTVDemoRecorder_StartRecording = (Symbols::CHLTVDemoRecorder_StartRecording)Detour::GetFunction(engine_loader.GetModule(), Symbols::CHLTVDemoRecorder_StartRecordingSym);
	func_CHLTVDemoRecorder_StopRecording = (Symbols::CHLTVDemoRecorder_StopRecording)Detour::GetFunction(engine_loader.GetModule(), Symbols::CHLTVDemoRecorder_StopRecordingSym);
}