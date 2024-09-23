#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "sourcesdk/ivp_synapse.h"
#include "sourcesdk/ivp_core.h"
#include "sourcesdk/ivp_environment.h"
#include "lua.h"
#include <chrono>
#include <ivp_friction.h>

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

IVP_Mindist** g_pCurrentMindist;
bool* g_fDeferDeleteMindist;

Symbols::IVP_Mindist_get_environment func_IVP_Mindist_get_environment;
Symbols::IVP_Mindist_get_synapse func_IVP_Mindist_get_synapse;
Symbols::IVP_Real_Object_revive_object_for_simulation func_IVP_Real_Object_revive_object_for_simulation;
Symbols::IVP_Real_Object_recheck_collision_filter func_IVP_Real_Object_recheck_collision_filter;
Symbols::IVP_Impact_Solver_Long_Term_do_impact_of_two_objects func_IVP_Impact_Solver_Long_Term_do_impact_of_two_objects;
Symbols::IVP_Real_Object_get_core func_IVP_Real_Object_get_core;
Symbols::IVP_Core_synchronize_with_rot_z func_IVP_Core_synchronize_with_rot_z;
Symbols::IVP_U_Memory_start_memory_transaction func_IVP_U_Memory_start_memory_transaction;
Symbols::IVP_U_Memory_end_memory_transaction func_IVP_U_Memory_end_memory_transaction;

enum IVP_SkipType {
	IVP_NoCall = -2, // Were not in our expected simulation, so don't handle anything.
	IVP_None, // We didn't ask lua yet. So make the lua call.

	// Lua Enums
	IVP_NoSkip,
	IVP_SkipImpact,
	IVP_SkipSimulation,
};

auto pCurrentTime = std::chrono::high_resolution_clock::now();
IVP_SkipType pCurrentSkipType = IVP_None;
double pCurrentLagThreadshold = 100; // 100 ms
Detouring::Hook detour_IVP_Event_Manager_Standard_simulate_time_events;
void hook_IVP_Event_Manager_Standard_simulate_time_events(void* timemanager, void* environment, IVP_Time time)
{
	pCurrentTime = std::chrono::high_resolution_clock::now();
	pCurrentSkipType = IVP_None; // Idk if it can happen that something else sets it in the mean time but let's just be sure...

	if (g_pPhysEnvModule.InDebug())
		Msg("physenv: IVP_Event_Manager_Standart::simulate_time_events called!\n");

	detour_IVP_Event_Manager_Standard_simulate_time_events.GetTrampoline<Symbols::IVP_Event_Manager_Standard_simulate_time_events>()(timemanager, environment, time);

	pCurrentSkipType = IVP_NoCall; // Reset it.
}

Detouring::Hook detour_IVP_Mindist_do_impact;
void hook_IVP_Mindist_do_impact(IVP_Mindist* mindist)
{
	if (g_pPhysEnvModule.InDebug())
		Msg("physenv: IVP_Mindist::do_impact called! (%i)\n", (int)pCurrentSkipType);

	if (pCurrentSkipType == IVP_SkipImpact)
		return;

	*g_pCurrentMindist = mindist;
	IVP_Environment *env;
	env = func_IVP_Mindist_get_environment(mindist);
	
	//IVP_ASSERT( mindist_status == IVP_MD_EXACT);

	// move variables too impact system
	IVP_Real_Object *objects[2];
	
	for (int i = 0;i<2;i++){
		IVP_Synapse *syn = func_IVP_Mindist_get_synapse(mindist, i);
		IVP_Real_Object *obj= syn->l_obj;
		Msg("%p %i %i\n", syn, (int)syn->mindist_offset, (int)syn->status);
		objects[i] = obj;
		func_IVP_Real_Object_revive_object_for_simulation(obj);
	}

	*g_pCurrentMindist = NULL;

	//revive_object_core is calling start_memory_transaction and end_memory_transaction, so we cannot start memory transaction earlier
	func_IVP_U_Memory_start_memory_transaction(env->sim_unit_mem);

	for(int j=0;j<2;j++) {
		IVP_Core *core = func_IVP_Real_Object_get_core(objects[j]);
		if ( IVP_MTIS_SIMULATED(core->movement_state) && !core->pinned){ //@@CB
			func_IVP_Core_synchronize_with_rot_z(core);
		}	
	}	
	
	env->mindist_event_timestamp_reference++;

	Msg("Dump: %p %p %p\n", mindist, objects[0], objects[1]);
	func_IVP_Impact_Solver_Long_Term_do_impact_of_two_objects(mindist, objects[0], objects[1]); // It's a static function, but it could be that we still need to make a IVP_Impact_Solver_Long_Term.
	func_IVP_U_Memory_end_memory_transaction(env->sim_unit_mem);

	if (*g_fDeferDeleteMindist)
	{
		// BUGBUG: someone changed a collision filter and didn't tell us!
		// 
		// Raphael:
		// Calling recheck_collision_filter should fix it and stop it from breaking the entire engine. (Never call "delete this;" here). 
		// In most cases, this IVP_Mindist will also be deleted from recheck_collision_filter.
		Warning("physenv: Someone didn't call Entity:CollisionRulesChanged()!\n"); // Let the world know.

		for (int i = 0;i<2;i++)
			func_IVP_Real_Object_recheck_collision_filter(objects[i]);
	}
}

Detouring::Hook detour_IVP_Mindist_simulate_time_event;
void hook_IVP_Mindist_simulate_time_event(void* mindist, void* environment)
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

void CPhysEnvModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	if (Util::PushTable("physenv"))
	{
		Util::AddFunc(physenv_SetLagThreshold, "SetLagThreshold");
		Util::AddFunc(physenv_GetLagThreshold, "GetLagThreshold");
		Util::PopTable();
	}
}

void CPhysEnvModule::LuaShutdown()
{
	if (Util::PushTable("physenv"))
	{
		Util::RemoveFunc("SetLagThreshold");
		Util::RemoveFunc("GetLagThreshold");
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

	if (g_pPhysEnvModule.InDebug() != 2)
	{
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
	}

	func_IVP_Mindist_get_environment = (Symbols::IVP_Mindist_get_environment)Detour::GetFunction(vphysics_loader.GetModule(), Symbols::IVP_Mindist_get_environmentSym);
	Detour::CheckFunction((void*)func_IVP_Mindist_get_environment, "IVP_Mindist::get_environment");

	func_IVP_Mindist_get_synapse = (Symbols::IVP_Mindist_get_synapse)Detour::GetFunction(vphysics_loader.GetModule(), Symbols::IVP_Mindist_get_synapseSym);
	Detour::CheckFunction((void*)func_IVP_Mindist_get_synapse, "IVP_Mindist::get_synapse");

	func_IVP_Real_Object_revive_object_for_simulation = (Symbols::IVP_Real_Object_revive_object_for_simulation)Detour::GetFunction(vphysics_loader.GetModule(), Symbols::IVP_Real_Object_revive_object_for_simulationSym);
	Detour::CheckFunction((void*)func_IVP_Real_Object_revive_object_for_simulation, "IVP_Real_Object::revive_object_for_simulation");

	func_IVP_Real_Object_recheck_collision_filter = (Symbols::IVP_Real_Object_recheck_collision_filter)Detour::GetFunction(vphysics_loader.GetModule(), Symbols::IVP_Real_Object_recheck_collision_filterSym);
	Detour::CheckFunction((void*)func_IVP_Real_Object_recheck_collision_filter, "IVP_Real_Object::recheck_collision_filter");

	func_IVP_Impact_Solver_Long_Term_do_impact_of_two_objects = (Symbols::IVP_Impact_Solver_Long_Term_do_impact_of_two_objects)Detour::GetFunction(vphysics_loader.GetModule(), Symbols::IVP_Impact_Solver_Long_Term_do_impact_of_two_objectsSym);
	Detour::CheckFunction((void*)func_IVP_Impact_Solver_Long_Term_do_impact_of_two_objects, "IVP_Impact_Solver_Long_Term::do_impact_of_two_objects");

	func_IVP_Real_Object_get_core = (Symbols::IVP_Real_Object_get_core)Detour::GetFunction(vphysics_loader.GetModule(), Symbols::IVP_Real_Object_get_coreSym);
	Detour::CheckFunction((void*)func_IVP_Real_Object_get_core, "IVP_Real_Object::get_core");

	func_IVP_Core_synchronize_with_rot_z = (Symbols::IVP_Core_synchronize_with_rot_z)Detour::GetFunction(vphysics_loader.GetModule(), Symbols::IVP_Core_synchronize_with_rot_zSym);
	Detour::CheckFunction((void*)func_IVP_Core_synchronize_with_rot_z, "IVP_Core::synchronize_with_rot_z");

	func_IVP_U_Memory_start_memory_transaction = (Symbols::IVP_U_Memory_start_memory_transaction)Detour::GetFunction(vphysics_loader.GetModule(), Symbols::IVP_U_Memory_start_memory_transactionSym);
	Detour::CheckFunction((void*)func_IVP_U_Memory_start_memory_transaction, "IVP_U_Memory::start_memory_transaction");

	func_IVP_U_Memory_end_memory_transaction = (Symbols::IVP_U_Memory_end_memory_transaction)Detour::GetFunction(vphysics_loader.GetModule(), Symbols::IVP_U_Memory_end_memory_transactionSym);
	Detour::CheckFunction((void*)func_IVP_U_Memory_end_memory_transaction, "IVP_U_Memory::end_memory_transaction");

	g_pCurrentMindist = Detour::ResolveSymbol<IVP_Mindist*>(vphysics_loader, Symbols::g_pCurrentMindistSym);
	Detour::CheckValue("get class", "g_pCurrentMindist", g_pCurrentMindist != NULL);

	g_fDeferDeleteMindist = Detour::ResolveSymbol<bool>(vphysics_loader, Symbols::g_fDeferDeleteMindistSym);
	Detour::CheckValue("get class", "g_fDeferDeleteMindist", g_fDeferDeleteMindist != NULL);
}