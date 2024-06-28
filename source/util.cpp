#include "util.h"
#include <string>
#include "edict.h"

GarrysMod::Lua::ILuaInterface* g_Lua;
IVEngineServer* engine;

CreateInterfaceFn g_interfaceFactory;
CreateInterfaceFn g_gameServerFactory;

void Util::StartTable() {
	g_Lua->PushSpecial(SPECIAL_GLOB);
	g_Lua->CreateTable();
}

void Util::AddFunc(CFunc Func, const char* Name) {
	g_Lua->PushCFunction(Func);
	g_Lua->SetField(-2, Name);
}

void Util::FinishTable(const char* Name) {
	g_Lua->SetField(-2, Name);
	g_Lua->Pop();
}


Symbols::Get_Player func_GetPlayer;
Symbols::Push_Entity func_PushEntity;
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

void Util::AddDetour()
{
	SourceSDK::ModuleLoader server_loader("server_srv");
	func_GetPlayer = (Symbols::Get_Player)Detour::GetFunction(server_loader.GetModule(), Symbols::Get_PlayerSym);
	func_PushEntity = (Symbols::Push_Entity)Detour::GetFunction(server_loader.GetModule(), Symbols::Push_EntitySym);
	Detour::CheckFunction(func_GetPlayer, "Get_Player");
	Detour::CheckFunction(func_PushEntity, "Push_Entity");
}