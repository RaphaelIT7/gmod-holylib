#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/InterfacePointers.hpp>
#include "detours.h"
#include "util.h"
#include "lua.h"
#include "eiface.h"

class CPVSModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn);
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour(bool bPreServer);
	virtual void Think(bool simulating);
	virtual void Shutdown();
	virtual const char* Name() { return "pvs"; };
};

CPVSModule g_pPVSModule;
IModule* pPVSModule = &g_pPVSModule;

static int currentPVSSize = -1;
static unsigned char* currentPVS = NULL;
static IVEngineServer* engineserver = NULL;
static Detouring::Hook detour_CGMOD_Player_SetupVisibility;
static void hook_CGMOD_Player_SetupVisibility(void* ent, unsigned char* pvs, int pvssize)
{
	currentPVS = pvs;
	currentPVSSize = pvssize;

	detour_CGMOD_Player_SetupVisibility.GetTrampoline<Symbols::CGMOD_Player_SetupVisibility>()(ent, pvs, pvssize);

	currentPVS = NULL;
	currentPVSSize = -1;
}

LUA_FUNCTION_STATIC(pvs_ResetPVS)
{
	if (currentPVS)
		engineserver->ResetPVS(currentPVS, currentPVSSize);

	return 0;
}

void CPVSModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	engineserver = (IVEngineServer*)appfn[0](INTERFACEVERSION_VENGINESERVER, NULL);
	Detour::CheckValue("get interface", "IVEngineServer", engineserver != NULL);
}

void CPVSModule::LuaInit(bool bServerInit)
{
	if ( bServerInit ) { return; }

	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		Util::StartTable();
			Util::AddFunc(pvs_ResetPVS, "ResetPVS");
		Util::FinishTable("pvs");
	g_Lua->Pop(1);
}

void CPVSModule::LuaShutdown()
{
}

void CPVSModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }

	SourceSDK::ModuleLoader server_loader("server_srv");
	Detour::Create(
		&detour_CGMOD_Player_SetupVisibility, "CGMOD_Player::SetupVisibility",
		server_loader.GetModule(), Symbols::CGMOD_Player_SetupVisibilitySym,
		(void*)hook_CGMOD_Player_SetupVisibility, m_pID
	);
}

void CPVSModule::Think(bool bSimulating)
{
}

void CPVSModule::Shutdown()
{
	Detour::Remove(m_pID);
}