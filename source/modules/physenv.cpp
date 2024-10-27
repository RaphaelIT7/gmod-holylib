#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"
#include <chrono>
#include "sourcesdk/ivp_time.h"

class CPhysEnvModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour(bool bPreServer);
	virtual const char* Name() { return "physenv"; };
	virtual int Compatibility() { return LINUX32; };
	virtual bool IsEnabledByDefault() OVERRIDE { return false; };
};

CPhysEnvModule g_pPhysEnvModule;
IModule* pPhysEnvModule = &g_pPhysEnvModule;

static bool g_bInImpactCall = false;
IVP_Mindist** g_pCurrentMindist;
bool* g_fDeferDeleteMindist;

enum IVP_SkipType {
	IVP_NoCall = -2, // Were not in our expected simulation, so don't handle anything.
	IVP_None, // We didn't ask lua yet. So make the lua call.

	// Lua Enums
	IVP_NoSkip,
	IVP_SkipImpact,
	IVP_SkipSimulation,
};
IVP_SkipType pCurrentSkipType = IVP_None;

#define TOSTRING( var ) var ? "true" : "false"

static Detouring::Hook detour_IVP_Mindist_D2;
static void hook_IVP_Mindist_D2(IVP_Mindist* mindist)
{
	if (g_bInImpactCall && *g_fDeferDeleteMindist && *g_pCurrentMindist == NULL)
	{
		*g_fDeferDeleteMindist = false; // The single thing missing in the physics engine that causes it to break.....
		Warning("holylib: Someone forgot to call Entity:CollisionRulesChanged!\n");
	}

	detour_IVP_Mindist_D2.GetTrampoline<Symbols::IVP_Mindist_D2>()(mindist);
}

static Detouring::Hook detour_IVP_Mindist_do_impact;
static void hook_IVP_Mindist_do_impact(IVP_Mindist* mindist)
{
	if (g_pPhysEnvModule.InDebug())
		Msg("physenv: IVP_Mindist::do_impact called! (%i)\n", (int)pCurrentSkipType);

	if (pCurrentSkipType == IVP_SkipImpact)
		return;

	g_bInImpactCall = true; // Verify: Do we actually need this?
	detour_IVP_Mindist_do_impact.GetTrampoline<Symbols::IVP_Mindist_do_impact>()(mindist);
	g_bInImpactCall = false;
}

auto pCurrentTime = std::chrono::high_resolution_clock::now();
double pCurrentLagThreadshold = 100; // 100 ms
static Detouring::Hook detour_IVP_Event_Manager_Standard_simulate_time_events;
static void hook_IVP_Event_Manager_Standard_simulate_time_events(void* eventmanager, void* timemanager, void* environment, IVP_Time time)
{
	if (pCurrentSkipType == IVP_SkipSimulation)
		return;

	pCurrentTime = std::chrono::high_resolution_clock::now();
	pCurrentSkipType = IVP_None; // Idk if it can happen that something else sets it in the mean time but let's just be sure...

	if (g_pPhysEnvModule.InDebug())
		Msg("physenv: IVP_Event_Manager_Standart::simulate_time_events called!\n");

	detour_IVP_Event_Manager_Standard_simulate_time_events.GetTrampoline<Symbols::IVP_Event_Manager_Standard_simulate_time_events>()(eventmanager, timemanager, environment, time);

	//pCurrentSkipType = IVP_NoCall; // Reset it.
}

static Detouring::Hook detour_IVP_Mindist_simulate_time_event;
static void hook_IVP_Mindist_simulate_time_event(void* mindist, void* environment)
{
	if (g_pPhysEnvModule.InDebug())
		Msg("physenv: IVP_Mindist::simulate_time_event called! (%i)\n", (int)pCurrentSkipType);

	if (pCurrentSkipType == IVP_SkipSimulation)
		return;

	auto pTime = std::chrono::high_resolution_clock::now();
	auto pSimulationTime = std::chrono::duration_cast<std::chrono::milliseconds>(pTime - pCurrentTime).count();
	if (pSimulationTime > pCurrentLagThreadshold)
	{
		if (Lua::PushHook("HolyLib:PhysicsLag"))
		{
			g_Lua->PushNumber(pSimulationTime);
			if (g_Lua->CallFunctionProtected(2, 1, true))
			{
				int pType = g_Lua->GetNumber();
				if (pType > 2 || pType < 0)
					pType = 0; // Invalid value. So we won't do shit.

				pCurrentSkipType = (IVP_SkipType)pType;

				if (g_pPhysEnvModule.InDebug())
					Msg("physenv: Lua hook called (%i)\n", (int)pCurrentSkipType);

				if (pCurrentSkipType == IVP_SkipSimulation)
					return;
			}
		}
	}


	detour_IVP_Mindist_simulate_time_event.GetTrampoline<Symbols::IVP_Mindist_simulate_time_event>()(mindist, environment);
}

static Detouring::Hook detour_IVP_Mindist_update_exact_mindist_events;
static void hook_IVP_Mindist_update_exact_mindist_events(void* mindist, IVP_BOOL allow_hull_conversion, IVP_MINDIST_EVENT_HINT event_hint)
{
	if (g_pPhysEnvModule.InDebug())
		Msg("physenv: IVP_Mindist::update_exact_mindist_events called! (%i)\n", (int)pCurrentSkipType);

	if (pCurrentSkipType == IVP_SkipImpact)
		return;

	detour_IVP_Mindist_update_exact_mindist_events.GetTrampoline<Symbols::IVP_Mindist_update_exact_mindist_events>()(mindist, allow_hull_conversion, event_hint);
}

LUA_FUNCTION_STATIC(physenv_SetLagThreshold)
{
	pCurrentLagThreadshold = LUA->CheckNumber(1);

	return 0;
}

LUA_FUNCTION_STATIC(physenv_GetLagThreshold)
{
	LUA->PushNumber(pCurrentLagThreadshold);

	return 1;
}

LUA_FUNCTION_STATIC(physenv_SetPhysSkipType)
{
	int pType = g_Lua->CheckNumber(1);
	pCurrentSkipType = (IVP_SkipType)pType;

	return 0;
}

void CPhysEnvModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	if (Util::PushTable("physenv"))
	{
		Util::AddFunc(physenv_SetLagThreshold, "SetLagThreshold");
		Util::AddFunc(physenv_GetLagThreshold, "GetLagThreshold");
		Util::AddFunc(physenv_SetPhysSkipType, "SetPhysSkipType");
		Util::PopTable();
	}
}

void CPhysEnvModule::LuaShutdown()
{
	if (Util::PushTable("physenv"))
	{
		Util::RemoveFunc("SetLagThreshold");
		Util::RemoveFunc("GetLagThreshold");
		Util::RemoveFunc("SetPhysSkipType");
		Util::PopTable();
	}
}

void CPhysEnvModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::FactoryLoader vphysics_loader("vphysics");
	Detour::Create(
		&detour_IVP_Mindist_do_impact, "IVP_Mindist::do_impact",
		vphysics_loader.GetModule(), Symbols::IVP_Mindist_do_impactSym,
		(void*)hook_IVP_Mindist_do_impact, m_pID
	);

	Detour::Create(
		&detour_IVP_Mindist_D2, "IVP_Mindist::~IVP_Mindist",
		vphysics_loader.GetModule(), Symbols::IVP_Mindist_D2Sym,
		(void*)hook_IVP_Mindist_D2, m_pID
	);

	Detour::Create(
		&detour_IVP_Event_Manager_Standard_simulate_time_events, "IVP_Event_Manager_Standard::simulate_time_events",
		vphysics_loader.GetModule(), Symbols::IVP_Event_Manager_Standard_simulate_time_eventsSym,
		(void*)hook_IVP_Event_Manager_Standard_simulate_time_events, m_pID
	);

	Detour::Create(
		&detour_IVP_Mindist_simulate_time_event, "IVP_Mindist::simulate_time_event",
		vphysics_loader.GetModule(), Symbols::IVP_Mindist_simulate_time_eventSym,
		(void*)hook_IVP_Mindist_simulate_time_event, m_pID
	);

	Detour::Create(
		&detour_IVP_Mindist_update_exact_mindist_events, "IVP_Mindist::update_exact_mindist_events",
		vphysics_loader.GetModule(), Symbols::IVP_Mindist_update_exact_mindist_eventsSym,
		(void*)hook_IVP_Mindist_update_exact_mindist_events, m_pID
	);

	g_pCurrentMindist = Detour::ResolveSymbol<IVP_Mindist*>(vphysics_loader, Symbols::g_pCurrentMindistSym);
	Detour::CheckValue("get class", "g_pCurrentMindist", g_pCurrentMindist != NULL);

	g_fDeferDeleteMindist = Detour::ResolveSymbol<bool>(vphysics_loader, Symbols::g_fDeferDeleteMindistSym);
	Detour::CheckValue("get class", "g_fDeferDeleteMindist", g_fDeferDeleteMindist != NULL);
}