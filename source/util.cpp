#include "util.h"
#include <string>
#include "GarrysMod/InterfacePointers.hpp"
#include "sourcesdk/baseclient.h"
#include "iserver.h"
#include "module.h"
#include "icommandline.h"
#include "player.h"

GarrysMod::Lua::IUpdatedLuaInterface* g_Lua;
IVEngineServer* engine;
CGlobalEntityList* Util::entitylist = NULL;

void Util::StartTable() {
	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	g_Lua->CreateTable();
}

void Util::AddFunc(GarrysMod::Lua::CFunc Func, const char* Name) {
	g_Lua->PushCFunction(Func);
	g_Lua->SetField(-2, Name);
}

void Util::FinishTable(const char* Name) {
	g_Lua->SetField(-2, Name);
	g_Lua->Pop();
}

void Util::NukeTable(const char* pName)
{
	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		g_Lua->PushNil();
		g_Lua->SetField(-2, pName);
	g_Lua->Pop(1);
}

bool Util::PushTable(const char* pName)
{
	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		g_Lua->GetField(-1, pName);
		g_Lua->Remove(-2);
		return g_Lua->IsType(-1, GarrysMod::Lua::Type::Table);
}

void Util::PopTable()
{
	g_Lua->Pop(1);
}

void Util::RemoveFunc(const char* pName)
{
	g_Lua->PushNil();
	g_Lua->SetField(-2, pName);
}


static Symbols::Get_Player func_GetPlayer;
static Symbols::Push_Entity func_PushEntity;
static Symbols::Get_Entity func_GetEntity;
CBasePlayer* Util::Get_Player(int iStackPos, bool bError) // bError = error if not a valid player
{
	if (func_GetPlayer)
		return func_GetPlayer(iStackPos, bError);

	return NULL;
}

void Util::Push_Entity(CBaseEntity* pEnt)
{
	if (func_PushEntity)
		func_PushEntity(pEnt);
}

CBaseEntity* Util::Get_Entity(int iStackPos, bool bError)
{
	if (func_GetEntity)
		return func_GetEntity(iStackPos, bError);

	return NULL;
}

IServer* Util::server;
CBaseClient* Util::GetClientByUserID(int userid)
{
	for (int i = 0; i < Util::server->GetClientCount(); i++)
	{
		IClient* pClient = Util::server->GetClient(i);
		if ( pClient && pClient->GetUserID() == userid)
		{
			return (CBaseClient*)pClient;
		}
	}

	return NULL;
}

IVEngineServer* Util::engineserver = NULL;
IServerGameEnts* Util::servergameents = NULL;
CBaseClient* Util::GetClientByPlayer(CBasePlayer* ply)
{
	return Util::GetClientByUserID(Util::engineserver->GetPlayerUserId(Util::GetEdictOfEnt((CBaseEntity*)ply)));
}

CBaseClient* Util::GetClientByIndex(int index)
{
	if (server->GetClientCount() <= index)
		return NULL;

	return (CBaseClient*)server->GetClient(index);
}

std::vector<CBaseClient*> Util::GetClients()
{
	std::vector<CBaseClient*> pClients;

	for (int i = 0; i < server->GetClientCount(); i++)
	{
		IClient* pClient = server->GetClient(i);
		pClients.push_back((CBaseClient*)pClient);
	}

	return pClients;
}

CBasePlayer* Util::GetPlayerByClient(CBaseClient* client)
{
	return (CBasePlayer*)servergameents->EdictToBaseEntity(engineserver->PEntityOfEntIndex(client->GetPlayerSlot() + 1));
}

CBaseEntityList* g_pEntityList = NULL;
void Util::AddDetour()
{
	if (g_pModuleManager.GetAppFactory())
		engineserver = (IVEngineServer*)g_pModuleManager.GetAppFactory()(INTERFACEVERSION_VENGINESERVER, NULL);
	else
		engineserver = InterfacePointers::VEngineServer();
	Detour::CheckValue("get interface", "IVEngineServer", engineserver != NULL);

	SourceSDK::FactoryLoader server_loader("server");
	if (g_pModuleManager.GetAppFactory())
		servergameents = (IServerGameEnts*)g_pModuleManager.GetGameFactory()(INTERFACEVERSION_SERVERGAMEENTS, NULL);
	else
		servergameents = server_loader.GetInterface<IServerGameEnts>(INTERFACEVERSION_SERVERGAMEENTS);
	Detour::CheckValue("get interface", "IServerGameEnts", servergameents != NULL);

	server = InterfacePointers::Server();
	Detour::CheckValue("get class", "IServer", server != NULL);

#ifdef ARCHITECTURE_X86 // We don't use it on 64x, do we. Look into pas_FindInPAS to see how we do it ^^
	g_pEntityList = Detour::ResolveSymbol<CBaseEntityList>(server_loader, Symbols::g_pEntityListSym);
	Detour::CheckValue("get class", "g_pEntityList", g_pEntityList != NULL);
	entitylist = (CGlobalEntityList*)g_pEntityList;
#endif

	/*
	 * IMPORTANT TODO
	 * 
	 * We now will run in the menu state so if we try to push an entity or so, we may push it in the wrong realm!
	 * How will we handle multiple realms?
	 */

#ifndef SYSTEM_WINDOWS
	func_GetPlayer = (Symbols::Get_Player)Detour::GetFunction(server_loader.GetModule(), Symbols::Get_PlayerSym);
	func_PushEntity = (Symbols::Push_Entity)Detour::GetFunction(server_loader.GetModule(), Symbols::Push_EntitySym);
	func_GetEntity = (Symbols::Get_Entity)Detour::GetFunction(server_loader.GetModule(), Symbols::Get_EntitySym);
	Detour::CheckFunction((void*)func_GetPlayer, "Get_Player");
	Detour::CheckFunction((void*)func_PushEntity, "Push_Entity");
	Detour::CheckFunction((void*)func_GetEntity, "Get_Entity");
#endif
}

static bool g_pShouldLoad = false;
bool Util::ShouldLoad()
{
	if (CommandLine()->FindParm("-holylibexists") && !g_pShouldLoad) // Don't set this manually!
		return false;

	if (g_pShouldLoad)
		return true;

	g_pShouldLoad = true;
	CommandLine()->AppendParm("-holylibexists", "true");

	return true;
}