#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"
#include "sourcesdk/hltvserver.h"
#include "sourcesdk/gmod_netmessages.h"

class CSourceTVLibModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn);
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour(bool bPreServer);
	virtual void Think(bool bSimulating);
	virtual void Shutdown();
	virtual const char* Name() { return "sourcetv"; };
};

ConVar sourcetv_allownetworking("holylib_sourcetv_allownetworking", "0", 0, "Allows HLTV Clients to send net messages to the server.");

CSourceTVLibModule g_pSourceTVLibModule;
IModule* pSourceTVLibModule = &g_pSourceTVLibModule;

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
Detouring::Hook detour_CHLTVServer_CHLTVServer;
void hook_CHLTVServer_CHLTVServer(CHLTVServer* srv)
{
	hltv = srv;
	detour_CHLTVServer_CHLTVServer.GetTrampoline<Symbols::CHLTVServer_CHLTVServer>()(srv);
}

Detouring::Hook detour_CHLTVServer_DestroyCHLTVServer;
void hook_CHLTVServer_DestroyCHLTVServer(CHLTVServer* srv)
{
	hltv = NULL;
	detour_CHLTVServer_DestroyCHLTVServer.GetTrampoline<Symbols::CHLTVServer_DestroyCHLTVServer>()(srv);
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

Symbols::COM_IsValidPath func_COM_IsValidPath;
Symbols::CHLTVDemoRecorder_StartRecording func_CHLTVDemoRecorder_StartRecording;
LUA_FUNCTION_STATIC(sourcetv_StartRecord)
{
	const char* pFileName = LUA->CheckString(1);

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

	if (!func_COM_IsValidPath(pFileName))
	{
		LUA->PushNumber(LUA_RECORD_INVALIDPATH);
		return 1;
	}
 
	char name[MAX_OSPATH];
	Q_strncpy(name, pFileName, sizeof(name));
	Q_DefaultExtension(name, ".dem", sizeof(name));

	if (g_pFullFileSystem->FileExists(name))
	{
		LUA->PushNumber(LUA_RECORD_FILEEXISTS);
		return 1;
	}

	func_CHLTVDemoRecorder_StartRecording(&hltv->m_DemoRecorder, name, false);
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

Symbols::CHLTVDemoRecorder_StopRecording func_CHLTVDemoRecorder_StopRecording;
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

	func_CHLTVDemoRecorder_StopRecording(&hltv->m_DemoRecorder);
	LUA->PushNumber(LUA_RECORD_OK);

	return 1;
}

Detouring::Hook detour_CHLTVClient_ProcessGMod_ClientToServer;
bool hook_CHLTVClient_ProcessGMod_ClientToServer(CHLTVClient* hltvclient, CLC_GMod_ClientToServer* bf)
{
	if (!sourcetv_allownetworking.GetBool())
		return true;

	bf->m_DataIn.Seek(0);
	int type = bf->m_DataIn.ReadBitLong(4, false);
	if ( type != 2 ) // Only handle type 2 -> Lua net message.
		return true;

	bf->m_DataIn.ReadBitLong(8, false);
	bf->m_DataIn.ReadBitLong(22, false); // Skiping to the header
	//bf->m_DataIn.ReadBitLong(16, false); // The header -> the string. Why not an 12 bits? (This will be read by net.ReadHeader())

	// ToDo: Add the Lua call.
}

void CSourceTVLibModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
}

void CSourceTVLibModule::LuaInit(bool bServerInit)
{
	if (!bServerInit)
	{
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

			g_Lua->PushNumber(LUA_RECORD_OK);
			g_Lua->SetField(-2, "RECORD_OK");

			g_Lua->PushNumber(LUA_RECORD_NOSOURCETV);
			g_Lua->SetField(-2, "RECORD_NOSOURCETV");

			g_Lua->PushNumber(LUA_RECORD_NOTMASTER);
			g_Lua->SetField(-2, "RECORD_NOTMASTER");

			g_Lua->PushNumber(LUA_RECORD_ACTIVE);
			g_Lua->SetField(-2, "RECORD_ACTIVE");

			g_Lua->PushNumber(LUA_RECORD_NOTACTIVE);
			g_Lua->SetField(-2, "RECORD_NOTACTIVE");

			g_Lua->PushNumber(LUA_RECORD_INVALIDPATH);
			g_Lua->SetField(-2, "RECORD_INVALIDPATH");

			g_Lua->PushNumber(LUA_RECORD_FILEEXISTS);
			g_Lua->SetField(-2, "RECORD_FILEEXISTS");
		Util::FinishTable("sourcetv");
	}
}

void CSourceTVLibModule::LuaShutdown()
{
	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		g_Lua->PushNil();
		g_Lua->SetField(-2, "sourcetv");
	g_Lua->Pop(1);
}

void CSourceTVLibModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }

	SourceSDK::ModuleLoader engine_loader("engine_srv");
	Detour::Create(
		&detour_CHLTVClient_ProcessGMod_ClientToServer, "CHLTVClient::ProcessGMod_ClientToServer",
		engine_loader.GetModule(), Symbols::CHLTVClient_ProcessGMod_ClientToServerSym,
		(void*)hook_CHLTVClient_ProcessGMod_ClientToServer, m_pID
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

	func_CHLTVDemoRecorder_StartRecording = (Symbols::CHLTVDemoRecorder_StartRecording)Detour::GetFunction(engine_loader.GetModule(), Symbols::CHLTVDemoRecorder_StartRecordingSym);
	func_CHLTVDemoRecorder_StopRecording = (Symbols::CHLTVDemoRecorder_StopRecording)Detour::GetFunction(engine_loader.GetModule(), Symbols::CHLTVDemoRecorder_StopRecordingSym);
}

void CSourceTVLibModule::Think(bool simulating)
{
}

void CSourceTVLibModule::Shutdown()
{
}