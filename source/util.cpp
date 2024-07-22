#include "util.h"
#include <string>
#include "edict.h"

GarrysMod::Lua::IUpdatedLuaInterface* g_Lua;
IVEngineServer* engine;

CreateInterfaceFn g_interfaceFactory;
CreateInterfaceFn g_gameServerFactory;

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


static Symbols::Get_Player func_GetPlayer;
static Symbols::Push_Entity func_PushEntity;
static Symbols::Get_Entity func_GetEntity;
CBasePlayer* Util::Get_Player(int iStackPos, bool unknown)
{
	if (func_GetPlayer)
		return func_GetPlayer(iStackPos, unknown);

	return NULL;
}

void Util::Push_Entity(CBaseEntity* pEnt)
{
	if (func_PushEntity)
		func_PushEntity(pEnt);
}

CBaseEntity* Util::Get_Entity(int iStackPos, bool unknown)
{
	if (func_GetEntity)
		return func_GetEntity(iStackPos, unknown);

	return NULL;
}

void Util::AddDetour()
{
	SourceSDK::ModuleLoader server_loader("server_srv");
	func_GetPlayer = (Symbols::Get_Player)Detour::GetFunction(server_loader.GetModule(), Symbols::Get_PlayerSym);
	func_PushEntity = (Symbols::Push_Entity)Detour::GetFunction(server_loader.GetModule(), Symbols::Push_EntitySym);
	func_GetEntity = (Symbols::Get_Entity)Detour::GetFunction(server_loader.GetModule(), Symbols::Get_EntitySym);
	Detour::CheckFunction(func_GetPlayer, "Get_Player");
	Detour::CheckFunction(func_PushEntity, "Push_Entity");
	Detour::CheckFunction(func_GetEntity, "Get_Entity");
}