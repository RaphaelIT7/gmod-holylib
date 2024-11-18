#include "LuaInterface.h"
#include "symbols.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include <chrono>
#include "sourcesdk/ivp_time.h"
#include "vcollide_parse.h"
#include "solidsetdefaults.h"
#include <vphysics_interface.h>
#include <vphysics/collision_set.h>
#include <vphysics/performance.h>
#include "sourcesdk/cphysicsobject.h"
#include "sourcesdk/cphysicsenvironment.h"
#include "player.h"

class CPhysEnvModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn);
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour(bool bPreServer);
	virtual const char* Name() { return "physenv"; };
	virtual int Compatibility() { return LINUX32; };
};

CPhysEnvModule g_pPhysEnvModule;
IModule* pPhysEnvModule = &g_pPhysEnvModule;

class IVP_Mindist;
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
	if (g_pPhysEnvModule.InDebug() > 2)
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
	pCurrentTime = std::chrono::high_resolution_clock::now();
	pCurrentSkipType = IVP_None; // Idk if it can happen that something else sets it in the mean time but let's just be sure...

	if (g_pPhysEnvModule.InDebug() > 2)
		Msg("physenv: IVP_Event_Manager_Standart::simulate_time_events called!\n");

	detour_IVP_Event_Manager_Standard_simulate_time_events.GetTrampoline<Symbols::IVP_Event_Manager_Standard_simulate_time_events>()(eventmanager, timemanager, environment, time);

	pCurrentSkipType = IVP_NoCall; // Reset it.
}

void CheckPhysicsLag()
{
	if (pCurrentSkipType != IVP_None) // We already have a skip type.
		return;

	auto pTime = std::chrono::high_resolution_clock::now();
	auto pSimulationTime = std::chrono::duration_cast<std::chrono::milliseconds>(pTime - pCurrentTime).count();
	if (pSimulationTime > pCurrentLagThreadshold)
	{
		if (Lua::PushHook("HolyLib:PhysicsLag"))
		{
			g_Lua->PushNumber((double)pSimulationTime);
			if (g_Lua->CallFunctionProtected(2, 1, true))
			{
				int pType = (int)g_Lua->GetNumber();
				if (pType > 2 || pType < 0)
					pType = 0; // Invalid value. So we won't do shit.

				pCurrentSkipType = (IVP_SkipType)pType;
				g_Lua->Pop(1);

				if (g_pPhysEnvModule.InDebug() > 2)
					Msg("physenv: Lua hook called (%i)\n", (int)pCurrentSkipType);
			}
		}
	}
}

static Detouring::Hook detour_IVP_Mindist_simulate_time_event;
static void hook_IVP_Mindist_simulate_time_event(void* mindist, void* environment)
{
	if (g_pPhysEnvModule.InDebug() > 2)
		Msg("physenv: IVP_Mindist::simulate_time_event called! (%i)\n", (int)pCurrentSkipType);

	CheckPhysicsLag();
	if (pCurrentSkipType == IVP_SkipSimulation)
		return;

	detour_IVP_Mindist_simulate_time_event.GetTrampoline<Symbols::IVP_Mindist_simulate_time_event>()(mindist, environment);
}

static Detouring::Hook detour_IVP_Mindist_update_exact_mindist_events;
static void hook_IVP_Mindist_update_exact_mindist_events(void* mindist, IVP_BOOL allow_hull_conversion, IVP_MINDIST_EVENT_HINT event_hint)
{
	if (g_pPhysEnvModule.InDebug() > 2)
		Msg("physenv: IVP_Mindist::update_exact_mindist_events called! (%i)\n", (int)pCurrentSkipType);

	CheckPhysicsLag();
	if (pCurrentSkipType == IVP_SkipSimulation)
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
	int pType = (int)LUA->CheckNumber(1);
	pCurrentSkipType = (IVP_SkipType)pType;

	return 0;
}

IVModelInfo* modelinfo;
IStaticPropMgrServer* staticpropmgr;
IPhysics* physics = NULL;
static IPhysicsCollision* physcollide = NULL;
IPhysicsSurfaceProps* physprops;
void CPhysEnvModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	physics = (IPhysics*)appfn[0](VPHYSICS_INTERFACE_VERSION, NULL);
	Detour::CheckValue("get interface", "physics", physics != NULL);

	physprops = (IPhysicsSurfaceProps*)appfn[0](VPHYSICS_SURFACEPROPS_INTERFACE_VERSION, NULL);
	Detour::CheckValue("get interface", "physprops", physprops != NULL);

	physcollide = (IPhysicsCollision*)appfn[0](VPHYSICS_COLLISION_INTERFACE_VERSION, NULL);
	Detour::CheckValue("get interface", "physcollide", physcollide != NULL);

	staticpropmgr = (IStaticPropMgrServer*)appfn[0](INTERFACEVERSION_STATICPROPMGR_SERVER, NULL);
	Detour::CheckValue("get interface", "staticpropmgr", staticpropmgr != NULL);

	modelinfo = (IVModelInfo*)appfn[0](VMODELINFO_SERVER_INTERFACE_VERSION, NULL);
	Detour::CheckValue("get interface", "modelinfo", modelinfo != NULL);
}

PushReferenced_LuaClass(IPhysicsObject, GarrysMod::Lua::Type::PhysObj) // This will later cause so much pain when they become Invalid XD
Get_LuaClass(IPhysicsObject, GarrysMod::Lua::Type::PhysObj, "IPhysicsObject")

class CLuaPhysicsObjectEvent : public IPhysicsObjectEvent
{
public:
	virtual void ObjectWake(IPhysicsObject* obj)
	{
		if (!g_Lua || m_iObjectWakeFunction == -1)
			return;

		g_Lua->ReferencePush(m_iObjectWakeFunction);
		Push_IPhysicsObject(obj);
		g_Lua->CallFunctionProtected(1, 0, true);
	}

	virtual void ObjectSleep(IPhysicsObject* obj)
	{
		if (!g_Lua || m_iObjectSleepFunction == -1)
			return;

		g_Lua->ReferencePush(m_iObjectSleepFunction);
		Push_IPhysicsObject(obj);
		g_Lua->CallFunctionProtected(1, 0, true);
	}

public:
	virtual ~CLuaPhysicsObjectEvent()
	{
		if (!g_Lua)
			return;

		if (m_iObjectWakeFunction != -1)
		{
			g_Lua->ReferenceFree(m_iObjectWakeFunction);
			m_iObjectWakeFunction = -1;
		}

		if (m_iObjectSleepFunction != -1)
		{
			g_Lua->ReferenceFree(m_iObjectSleepFunction);
			m_iObjectSleepFunction = -1;
		}
	}

	CLuaPhysicsObjectEvent(int iObjectWakeFunction, int iObjectSleepFunction)
	{
		m_iObjectWakeFunction = iObjectWakeFunction;
		m_iObjectSleepFunction = iObjectSleepFunction;
	}

private:
	int m_iObjectWakeFunction;
	int m_iObjectSleepFunction;
};

struct ILuaPhysicsEnvironment
{
	ILuaPhysicsEnvironment(IPhysicsEnvironment* env)
	{
		pEnvironment = env;
	}

	~ILuaPhysicsEnvironment()
	{
		if (pObjectEvent)
			delete pObjectEvent;

		//if (pCollisionSolver)
		//	delete pCollisionSolver;

		//if (pCollisionEvent)
		//	delete pCollisionEvent;
	}

	IPhysicsEnvironment* pEnvironment = NULL;
	CLuaPhysicsObjectEvent* pObjectEvent = NULL;
	//CLuaPhysicsCollisionSolver* pCollisionSolver = NULL;
	//CLuaPhysicsCollisionEvent* pCollisionEvent = NULL;
};

static int IPhysicsEnvironment_TypeID = -1;
PushReferenced_LuaClass(ILuaPhysicsEnvironment, IPhysicsEnvironment_TypeID)
Get_LuaClass(ILuaPhysicsEnvironment, IPhysicsEnvironment_TypeID, "IPhysicsEnvironment")

static int IPhysicsCollisionSet_TypeID = -1;
PushReferenced_LuaClass(IPhysicsCollisionSet, IPhysicsCollisionSet_TypeID)
Get_LuaClass(IPhysicsCollisionSet, IPhysicsCollisionSet_TypeID, "IPhysicsCollisionSet")

static int CPhysCollide_TypeID = -1;
PushReferenced_LuaClass(CPhysCollide, CPhysCollide_TypeID)
Get_LuaClass(CPhysCollide, CPhysCollide_TypeID, "CPhysCollide")

static int CPhysPolysoup_TypeID = -1;
PushReferenced_LuaClass(CPhysPolysoup, CPhysPolysoup_TypeID)
Get_LuaClass(CPhysPolysoup, CPhysPolysoup_TypeID, "CPhysPolysoup")

static int CPhysConvex_TypeID = -1;
PushReferenced_LuaClass(CPhysConvex, CPhysConvex_TypeID)
Get_LuaClass(CPhysConvex, CPhysConvex_TypeID, "CPhysConvex")

static int ICollisionQuery_TypeID = -1;
PushReferenced_LuaClass(ICollisionQuery, ICollisionQuery_TypeID)
Get_LuaClass(ICollisionQuery, ICollisionQuery_TypeID, "ICollisionQuery")

static Push_LuaClass(Vector, GarrysMod::Lua::Type::Vector)

inline IPhysicsEnvironment* GetPhysicsEnvironment(int iStackPos, bool bError)
{
	ILuaPhysicsEnvironment* pEnvironment = Get_ILuaPhysicsEnvironment(iStackPos, bError);
	if (!pEnvironment->pEnvironment && bError)
		g_Lua->ThrowError(triedNull_ILuaPhysicsEnvironment.c_str());

	return pEnvironment->pEnvironment;
}

LUA_FUNCTION_STATIC(physenv_CreateEnvironment)
{
	if (!physics)
		LUA->ThrowError("Failed to get IPhysics!");

	ILuaPhysicsEnvironment* pLua = new ILuaPhysicsEnvironment(physics->CreateEnvironment());
	IPhysicsEnvironment* pEnvironment = pLua->pEnvironment;
	CPhysicsEnvironment* pMainEnvironment = (CPhysicsEnvironment*)physics->GetActiveEnvironmentByIndex(0);

	if (pMainEnvironment)
	{
		physics_performanceparams_t params;
		pMainEnvironment->GetPerformanceSettings(&params);
		pEnvironment->SetPerformanceSettings(&params);

		Vector vec;
		pMainEnvironment->GetGravity(&vec);
		pEnvironment->SetGravity(vec);

		pEnvironment->EnableDeleteQueue(true);
		pEnvironment->SetSimulationTimestep(pMainEnvironment->GetSimulationTimestep());

		CCollisionEvent* g_Collisions = (CCollisionEvent*)pMainEnvironment->m_pCollisionSolver->m_pSolver; // Raw access is always fun :D

		pEnvironment->SetCollisionSolver(g_Collisions);
		pEnvironment->SetCollisionEventHandler(g_Collisions);

		pEnvironment->SetConstraintEventHandler(pMainEnvironment->m_pConstraintListener->m_pCallback);
		pEnvironment->EnableConstraintNotify(true);

		pEnvironment->SetObjectEventHandler(g_Collisions);
	} else {
		physics_performanceparams_t params;
		params.Defaults();
		params.maxCollisionsPerObjectPerTimestep = 10;
		pEnvironment->SetPerformanceSettings(&params);
		pEnvironment->EnableDeleteQueue(true);

		//physenv->SetCollisionSolver(&g_Collisions);
		//physenv->SetCollisionEventHandler(&g_Collisions);
		//physenv->SetConstraintEventHandler(g_pConstraintEvents);
		//physenv->EnableConstraintNotify(true);

		//physenv->SetObjectEventHandler(&g_Collisions);
	
		//pEnvironment->SetSimulationTimestep(gpGlobals->interval_per_tick);

		ConVarRef gravity("sv_gravity");
		if (gravity.IsValid())
			pEnvironment->SetGravity( Vector( 0, 0, -gravity.GetFloat() ) );
	}

	Push_ILuaPhysicsEnvironment(pLua);
	return 1;
}

LUA_FUNCTION_STATIC(physenv_GetActiveEnvironmentByIndex)
{
	if (!physics)
		LUA->ThrowError("Failed to get IPhysics!");

	int index = (int)LUA->CheckNumber(1);
	Push_ILuaPhysicsEnvironment(new ILuaPhysicsEnvironment(physics->GetActiveEnvironmentByIndex(index)));
	return 1;
}

LUA_FUNCTION_STATIC(physenv_DestroyEnvironment)
{
	if (!physics)
		LUA->ThrowError("Failed to get IPhysics!");

	ILuaPhysicsEnvironment* pEnvironment = Get_ILuaPhysicsEnvironment(1, true);
	if (pEnvironment->pEnvironment)
		physics->DestroyEnvironment(pEnvironment->pEnvironment);

	Delete_ILuaPhysicsEnvironment(pEnvironment);

	return 0;
}

LUA_FUNCTION_STATIC(physenv_FindCollisionSet)
{
	if (!physics)
		LUA->ThrowError("Failed to get IPhysics!");

	unsigned int index = (int)LUA->CheckNumber(1);
	Push_IPhysicsCollisionSet(physics->FindCollisionSet(index));
	return 1;
}

LUA_FUNCTION_STATIC(physenv_FindOrCreateCollisionSet)
{
	if (!physics)
		LUA->ThrowError("Failed to get IPhysics!");

	unsigned int index = (int)LUA->CheckNumber(1);
	int maxElements = (int)LUA->CheckNumber(2);
	Push_IPhysicsCollisionSet(physics->FindOrCreateCollisionSet(index, maxElements));
	return 1;
}

LUA_FUNCTION_STATIC(physenv_DestroyAllCollisionSets)
{
	if (!physics)
		LUA->ThrowError("Failed to get IPhysics!");

	physics->DestroyAllCollisionSets();
	for (auto& [key, val] : g_pPushedIPhysicsCollisionSet)
	{
		delete val;
	}
	g_pPushedIPhysicsCollisionSet.clear();
	return 0;
}

LUA_FUNCTION_STATIC(Default__Index)
{
	if (!LUA->FindOnObjectsMetaTable(1, 2))
		LUA->PushNil();

	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsCollisionSet__tostring)
{
	IPhysicsCollisionSet* pCollideSet = Get_IPhysicsCollisionSet(1, false);
	if (!pCollideSet)
		LUA->PushString("IPhysicsCollisionSet [NULL]");
	else
		LUA->PushString("IPhysicsCollisionSet");

	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsCollisionSet_IsValid)
{
	IPhysicsCollisionSet* pCollideSet = Get_IPhysicsCollisionSet(1, false);

	LUA->PushBool(pCollideSet != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsCollisionSet_EnableCollisions)
{
	IPhysicsCollisionSet* pCollideSet = Get_IPhysicsCollisionSet(1, true);

	int index1 = (int)LUA->CheckNumber(2);
	int index2 = (int)LUA->CheckNumber(3);
	pCollideSet->EnableCollisions(index1, index2);
	return 0;
}

LUA_FUNCTION_STATIC(IPhysicsCollisionSet_DisableCollisions)
{
	IPhysicsCollisionSet* pCollideSet = Get_IPhysicsCollisionSet(1, true);

	int index1 = (int)LUA->CheckNumber(2);
	int index2 = (int)LUA->CheckNumber(3);
	pCollideSet->DisableCollisions(index1, index2);
	return 0;
}

LUA_FUNCTION_STATIC(IPhysicsCollisionSet_ShouldCollide)
{
	IPhysicsCollisionSet* pCollideSet = Get_IPhysicsCollisionSet(1, true);

	int index1 = (int)LUA->CheckNumber(2);
	int index2 = (int)LUA->CheckNumber(3);
	LUA->PushBool(pCollideSet->ShouldCollide(index1, index2));
	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment__tostring)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, false);
	if (!pEnvironment)
		LUA->PushString("IPhysicsEnvironment [NULL]");
	else
		LUA->PushString("IPhysicsEnvironment");

	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_IsValid)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, false);

	LUA->PushBool(pEnvironment != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_TransferObject)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);
	IPhysicsObject* pObject = Get_IPhysicsObject(2, true);
	IPhysicsEnvironment* pTargetEnvironment = GetPhysicsEnvironment(3, true);

	LUA->PushBool(pEnvironment->TransferObject(pObject, pTargetEnvironment));
	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_SetGravity)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);
	Vector* pVec = Get_Vector(2, true);

	pEnvironment->SetGravity(*pVec);
	return 0;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_GetGravity)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);

	Vector vec;
	pEnvironment->GetGravity(&vec);
	Push_Vector(&vec);
	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_SetAirDensity)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);
	float airDensity = (float)LUA->CheckNumber(2);

	pEnvironment->SetAirDensity(airDensity);
	return 0;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_GetAirDensity)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);

	LUA->PushNumber(pEnvironment->GetAirDensity());
	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_SetPerformanceSettings)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);
	LUA->CheckType(2, GarrysMod::Lua::Type::Table);

	physics_performanceparams_t params;
	LUA->Push(2);
		if (Util::HasField("LookAheadTimeObjectsVsObject", GarrysMod::Lua::Type::Number))
			params.lookAheadTimeObjectsVsObject = (float)LUA->GetNumber(-1);
		LUA->Pop(1);

		if (Util::HasField("LookAheadTimeObjectsVsWorld", GarrysMod::Lua::Type::Number))
			params.lookAheadTimeObjectsVsWorld = (float)LUA->GetNumber(-1);
		LUA->Pop(1);

		if (Util::HasField("MaxCollisionChecksPerTimestep", GarrysMod::Lua::Type::Number))
			params.maxCollisionChecksPerTimestep = (int)LUA->GetNumber(-1);
		LUA->Pop(1);

		if (Util::HasField("MaxCollisionsPerObjectPerTimestep", GarrysMod::Lua::Type::Number))
			params.maxCollisionsPerObjectPerTimestep = (int)LUA->GetNumber(-1);
		LUA->Pop(1);

		if (Util::HasField("MaxVelocity", GarrysMod::Lua::Type::Number))
			params.maxVelocity = (float)LUA->GetNumber(-1);
		LUA->Pop(1);

		if (Util::HasField("MaxAngularVelocity", GarrysMod::Lua::Type::Number))
			params.maxAngularVelocity = (float)LUA->GetNumber(-1);
		LUA->Pop(1);

		if (Util::HasField("MinFrictionMass", GarrysMod::Lua::Type::Number))
			params.minFrictionMass = (float)LUA->GetNumber(-1);
		LUA->Pop(1);

		if (Util::HasField("MaxFrictionMass", GarrysMod::Lua::Type::Number))
			params.maxFrictionMass = (float)LUA->GetNumber(-1);
		LUA->Pop(1);
	LUA->Pop(1);

	pEnvironment->SetPerformanceSettings(&params);
	return 0;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_GetPerformanceSettings)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);

	physics_performanceparams_t params;
	pEnvironment->GetPerformanceSettings(&params);
	LUA->CreateTable();
		Util::AddValue(params.lookAheadTimeObjectsVsObject, "LookAheadTimeObjectsVsObject");
		Util::AddValue(params.lookAheadTimeObjectsVsWorld, "LookAheadTimeObjectsVsWorld");
		Util::AddValue(params.maxCollisionChecksPerTimestep, "MaxCollisionChecksPerTimestep");
		Util::AddValue(params.maxCollisionsPerObjectPerTimestep, "MaxCollisionsPerObjectPerTimestep");
		Util::AddValue(params.maxVelocity, "MaxVelocity");
		Util::AddValue(params.maxAngularVelocity, "MaxAngularVelocity");
		Util::AddValue(params.minFrictionMass, "MinFrictionMass");
		Util::AddValue(params.maxFrictionMass, "MaxFrictionMass");

	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_GetNextFrameTime)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);

	LUA->PushNumber(pEnvironment->GetNextFrameTime());
	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_GetSimulationTime)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);

	LUA->PushNumber(pEnvironment->GetSimulationTime());
	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_SetSimulationTimestep)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);
	float timeStep = (float)LUA->CheckNumber(2);

	pEnvironment->SetSimulationTimestep(timeStep);
	return 0;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_GetSimulationTimestep)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);

	LUA->PushNumber(pEnvironment->GetSimulationTimestep());
	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_GetActiveObjectCount)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);

	LUA->PushNumber(pEnvironment->GetActiveObjectCount());
	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_GetActiveObjects)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);

	int activeCount = pEnvironment->GetActiveObjectCount();
	IPhysicsObject** pActiveList = NULL;
	if (!activeCount)
		return 0;

	pActiveList = (IPhysicsObject**)stackalloc(sizeof(IPhysicsObject*) * activeCount);
	pEnvironment->GetActiveObjects(pActiveList);
	LUA->PreCreateTable(activeCount, 0);
	int idx = 0;
	for (int i=0; i<activeCount; ++i)
	{
		++idx;
		LUA->PushNumber(idx);
		Push_IPhysicsObject(pActiveList[i]);
		LUA->RawSet(-3);
	}

	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_GetObjectList)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);

	int iCount = 0;
	IPhysicsObject** pList = (IPhysicsObject**)pEnvironment->GetObjectList(&iCount);
	LUA->PreCreateTable(iCount, 0);
	int idx = 0;
	for (int i = 0; i < iCount; ++i)
	{
		++idx;
		LUA->PushNumber(idx);
		Push_IPhysicsObject(pList[i]);
		LUA->RawSet(-3);
	}

	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_IsInSimulation)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);

	LUA->PushBool(pEnvironment->IsInSimulation());
	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_ResetSimulationClock)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);

	pEnvironment->ResetSimulationClock();
	return 0;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_CleanupDeleteList)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);

	pEnvironment->CleanupDeleteList();
	return 0;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_SetQuickDelete)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);
	bool quickDelete = LUA->GetBool(2);

	pEnvironment->SetQuickDelete(quickDelete);
	return 0;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_EnableDeleteQueue)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);
	bool deleteQueue = LUA->GetBool(2);

	pEnvironment->EnableDeleteQueue(deleteQueue);
	return 0;
}

Symbols::CBaseEntity_VPhysicsUpdate func_CBaseEntity_VPhysicsUpdate;
ILuaPhysicsEnvironment* g_pCurrentEnvironment = NULL;
LUA_FUNCTION_STATIC(IPhysicsEnvironment_Simulate)
{
	ILuaPhysicsEnvironment* pLuaEnv = Get_ILuaPhysicsEnvironment(1, true);
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);

	float deltaTime = (float)LUA->CheckNumber(2);

	VPROF_BUDGET("HolyLib(Lua) - IPhysicsEnvironment::Simulate", VPROF_BUDGETGROUP_HOLYLIB);
	ILuaPhysicsEnvironment* pOldEnvironment = g_pCurrentEnvironment; // Simulating a environment in a already simulating environment? sounds fun :^
	g_pCurrentEnvironment = pLuaEnv;
	pEnvironment->Simulate(deltaTime);

	int activeCount = pEnvironment->GetActiveObjectCount();
	IPhysicsObject **pActiveList = NULL;
	if ( activeCount )
	{
		pActiveList = (IPhysicsObject **)stackalloc( sizeof(IPhysicsObject *)*activeCount );
		pEnvironment->GetActiveObjects( pActiveList );

		for ( int i = 0; i < activeCount; i++ )
		{
			CBaseEntity *pEntity = reinterpret_cast<CBaseEntity *>(pActiveList[i]->GetGameData());
			if ( pEntity )
			{
				if ( pEntity->CollisionProp()->DoesVPhysicsInvalidateSurroundingBox() )
				{
					pEntity->CollisionProp()->MarkSurroundingBoundsDirty();
				}

				//pEntity->VPhysicsUpdate( pActiveList[i] ); // BUG: The VTABLE IS BROKEN AGAIN. NOOOOOO
				func_CBaseEntity_VPhysicsUpdate(pEntity, pActiveList[i]);
			}
		}
		stackfree( pActiveList );
	}

	/*for ( entitem_t* pItem = g_pShadowEntities->m_pItemList; pItem; pItem = pItem->pNext )
	{
		CBaseEntity *pEntity = pItem->hEnt.Get();
		if ( !pEntity )
		{
			Msg( "Dangling pointer to physics entity!!!\n" );
			continue;
		}

		IPhysicsObject *pPhysics = pEntity->VPhysicsGetObject();
		if ( pPhysics && !pPhysics->IsAsleep() )
		{
			pEntity->VPhysicsShadowUpdate( pPhysics );
		}
	}*/

	g_pCurrentEnvironment = pOldEnvironment;
	return 0;
}

LUA_FUNCTION_STATIC(physenv_GetCurrentEnvironment)
{
	Push_ILuaPhysicsEnvironment(g_pCurrentEnvironment);
	return 1;
}

const objectparams_t g_PhysDefaultObjectParams =
{
	NULL,
	1.0, //mass
	1.0, // inertia
	0.1f, // damping
	0.1f, // rotdamping
	0.05f, // rotIntertiaLimit
	"DEFAULT",
	NULL,// game data
	0.f, // volume (leave 0 if you don't have one or call physcollision->CollideVolume() to compute it)
	1.0f, // drag coefficient
	true,// enable collisions?
};

static void FillObjectParams(objectparams_t& params, int iStackPos, GarrysMod::Lua::ILuaInterface* LUA)
{
	params = g_PhysDefaultObjectParams;
	if (!LUA->IsType(iStackPos, GarrysMod::Lua::Type::Table))
		return;

	LUA->Push(iStackPos);
		if (Util::HasField("damping", GarrysMod::Lua::Type::Number))
			params.damping = (float)LUA->GetNumber(-1);
		LUA->Pop(1);

		if (Util::HasField("dragCoefficient", GarrysMod::Lua::Type::Number))
			params.dragCoefficient = (float)LUA->GetNumber(-1);
		LUA->Pop(1);

		if (Util::HasField("enableCollisions", GarrysMod::Lua::Type::Bool))
			params.enableCollisions = LUA->GetBool(-1);
		LUA->Pop(1);

		if (Util::HasField("inertia", GarrysMod::Lua::Type::Number))
			params.inertia = (float)LUA->GetNumber(-1);
		LUA->Pop(1);

		if (Util::HasField("mass", GarrysMod::Lua::Type::Number))
			params.mass = (float)LUA->GetNumber(-1);
		LUA->Pop(1);

		if (Util::HasField("massCenterOverride", GarrysMod::Lua::Type::Vector))
			params.massCenterOverride = Get_Vector(-1, true);
		LUA->Pop(1);

		if (Util::HasField("name", GarrysMod::Lua::Type::Number))
			params.pName = LUA->GetString(-1);
		LUA->Pop(1);

		if (Util::HasField("rotdamping", GarrysMod::Lua::Type::Number))
			params.rotdamping = (float)LUA->GetNumber(-1);
		LUA->Pop(1);

		if (Util::HasField("rotInertiaLimit", GarrysMod::Lua::Type::Number))
			params.rotInertiaLimit = (float)LUA->GetNumber(-1);
		LUA->Pop(1);

		if (Util::HasField("volume", GarrysMod::Lua::Type::Number))
			params.volume = (float)LUA->GetNumber(-1);
		LUA->Pop(1);
	LUA->Pop(1);
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_CreatePolyObject)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);
	CPhysCollide* pCollide = Get_CPhysCollide(2, true);
	int materialIndex = (int)LUA->CheckNumber(3);
	Vector* pOrigin = Get_Vector(4, true);
	QAngle* pAngles = Get_QAngle(5, true);

	objectparams_t params;
	FillObjectParams(params, 6, LUA);
	Push_IPhysicsObject(pEnvironment->CreatePolyObject(pCollide, materialIndex, *pOrigin, *pAngles, &params));
	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_CreatePolyObjectStatic)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);
	CPhysCollide* pCollide = Get_CPhysCollide(2, true);
	int materialIndex = (int)LUA->CheckNumber(3);
	Vector* pOrigin = Get_Vector(4, true);
	QAngle* pAngles = Get_QAngle(5, true);

	objectparams_t params;
	FillObjectParams(params, 6, LUA);
	Push_IPhysicsObject(pEnvironment->CreatePolyObjectStatic(pCollide, materialIndex, *pOrigin, *pAngles, &params));
	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_CreateSphereObject)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);
	float radius = (float)LUA->CheckNumber(2);
	int materialIndex = (int)LUA->CheckNumber(3);
	Vector* pOrigin = Get_Vector(4, true);
	QAngle* pAngles = Get_QAngle(5, true);
	bool bStatic = LUA->GetBool(6);

	objectparams_t params;
	FillObjectParams(params, 6, LUA);
	Push_IPhysicsObject(pEnvironment->CreateSphereObject(radius, materialIndex, *pOrigin, *pAngles, &params, bStatic));
	return 1;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_DestroyObject)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);
	IPhysicsObject* pObject = Get_IPhysicsObject(2, true);

	pEnvironment->DestroyObject(pObject);
	Delete_IPhysicsObject(pObject);
	return 0;
}

LUA_FUNCTION_STATIC(IPhysicsEnvironment_IsCollisionModelUsed)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);
	CPhysCollide* pCollide = Get_CPhysCollide(2, true);

	LUA->PushBool(pEnvironment->IsCollisionModelUsed(pCollide));
	return 1;
}

/*LUA_FUNCTION_STATIC(IPhysicsEnvironment_SweepCollideable)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);

	pEnvironment->SweepCollideable // This function seems to be fully empty???
	return 1;
}*/

LUA_FUNCTION_STATIC(IPhysicsEnvironment_SetObjectEventHandler)
{
	ILuaPhysicsEnvironment* pLuaEnv = Get_ILuaPhysicsEnvironment(1, true);
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);
	LUA->CheckType(2, GarrysMod::Lua::Type::Function);
	LUA->CheckType(3, GarrysMod::Lua::Type::Function);

	LUA->Push(3);
	LUA->Push(2);
	if (pLuaEnv->pObjectEvent)
		delete pLuaEnv->pObjectEvent; // Free the old one

	pLuaEnv->pObjectEvent = new CLuaPhysicsObjectEvent(LUA->ReferenceCreate(), LUA->ReferenceCreate());
	pEnvironment->SetObjectEventHandler(pLuaEnv->pObjectEvent);
	return 0;
}

/*LUA_FUNCTION_STATIC(IPhysicsEnvironment_CreateCopy)
{
	ILuaPhysicsEnvironment* pLuaEnv = Get_ILuaPhysicsEnvironment(1, true);
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);
	CPhysicsObject* pObject = (CPhysicsObject*)Get_IPhysicsObject(2, true);

	Vector pos;
	QAngle ang;
	pObject->GetPosition(&pos, &ang);

	objectparams_t params;
	pObject->GetDamping(&params.damping, &params.rotdamping);
	params.mass = pObject->GetMass();
	params.enableCollisions = pObject->IsCollisionEnabled();
	params.pGameData = pObject->GetGameData();
	Vector vec = pObject->GetMassCenterLocalSpace();
	params.massCenterOverride = &vec;
	params.volume = pObject->m_volume;
	params.pName = pObject->GetName();
	params.dragCoefficient = pObject->m_dragCoefficient;

	Push_IPhysicsObject(pEnvironment->CreatePolyObject(pObject->m_pCollide, pObject->m_materialIndex, pos, ang, &params));
	return 1;
}*/

/*
 * ToDo: Move the code below somewhere else. 
 */

#if ARCHITECTURE_IS_X86
void CSolidSetDefaults::ParseKeyValue( void *pData, const char *pKey, const char *pValue )
{
	if ( !Q_stricmp( pKey, "contents" ) )
	{
		m_contentsMask = atoi( pValue );
	}
}

void CSolidSetDefaults::SetDefaults( void *pData )
{
	solid_t *pSolid = (solid_t *)pData;
	pSolid->params = g_PhysDefaultObjectParams;
	m_contentsMask = CONTENTS_SOLID;
}

CSolidSetDefaults g_SolidSetup;

void PhysCreateVirtualTerrain( IPhysicsEnvironment* pEnvironment, CBaseEntity *pWorld, const objectparams_t &defaultParams )
{
	if ( !pEnvironment )
		return;

	char nameBuf[1024];
	for ( int i = 0; i < MAX_MAP_DISPINFO; i++ )
	{
		CPhysCollide *pCollide = modelinfo->GetCollideForVirtualTerrain( i );
		if ( pCollide )
		{
			solid_t solid;
			solid.params = defaultParams;
			solid.params.enableCollisions = true;
			solid.params.pGameData = static_cast<void *>(pWorld);
			Q_snprintf(nameBuf, sizeof(nameBuf), "vdisp_%04d", i );
			solid.params.pName = nameBuf;
			int surfaceData = physprops->GetSurfaceIndex( "default" );
			// create this as part of the world
			IPhysicsObject *pObject = pEnvironment->CreatePolyObjectStatic( pCollide, surfaceData, vec3_origin, vec3_angle, &solid.params );
			pObject->SetCallbackFlags( pObject->GetCallbackFlags() | CALLBACK_NEVER_DELETED );
		}
	}
}

IPhysicsObject *PhysCreateWorld_Shared( IPhysicsEnvironment* pEnvironment, CBaseEntity *pWorld, vcollide_t *pWorldCollide, const objectparams_t &defaultParams )
{
	solid_t solid;
	fluid_t fluid;

	if ( !pEnvironment )
		return NULL;

	int surfaceData = physprops->GetSurfaceIndex( "default" );

	objectparams_t params = defaultParams;
	params.pGameData = static_cast<void *>(pWorld);
	params.pName = "world";

	IPhysicsObject *pWorldPhysics = pEnvironment->CreatePolyObjectStatic( 
		pWorldCollide->solids[0], surfaceData, vec3_origin, vec3_angle, &params );

	// hint - saves vphysics some work
	pWorldPhysics->SetCallbackFlags( pWorldPhysics->GetCallbackFlags() | CALLBACK_NEVER_DELETED );

	//PhysCheckAdd( world, "World" );
	// walk the world keys in case there are some fluid volumes to create
	IVPhysicsKeyParser *pParse = physcollide->VPhysicsKeyParserCreate( pWorldCollide->pKeyValues );

	bool bCreateVirtualTerrain = false;
	while ( !pParse->Finished() )
	{
		const char *pBlock = pParse->GetCurrentBlockName();

		if ( !strcmpi( pBlock, "solid" ) || !strcmpi( pBlock, "staticsolid" ) )
		{
			solid.params = defaultParams;
			pParse->ParseSolid( &solid, &g_SolidSetup );
			solid.params.enableCollisions = true;
			solid.params.pGameData = static_cast<void *>(pWorld);
			solid.params.pName = "world";
			surfaceData = physprops->GetSurfaceIndex( "default" );

			// already created world above
			if ( solid.index == 0 )
				continue;

			if ( !pWorldCollide->solids[solid.index] )
			{
				// this implies that the collision model is a mopp and the physics DLL doesn't support that.
				bCreateVirtualTerrain = true;
				continue;
			}
			// create this as part of the world
			IPhysicsObject *pObject = pEnvironment->CreatePolyObjectStatic( pWorldCollide->solids[solid.index], 
				surfaceData, vec3_origin, vec3_angle, &solid.params );

			// invalid collision model or can't create, ignore
			if (!pObject)
				continue;

			pObject->SetCallbackFlags( pObject->GetCallbackFlags() | CALLBACK_NEVER_DELETED );
			Assert( g_SolidSetup.GetContentsMask() != 0 );
			pObject->SetContents( g_SolidSetup.GetContentsMask() );

			if ( !pWorldPhysics )
			{
				pWorldPhysics = pObject;
			}
		}
		else if ( !strcmpi( pBlock, "fluid" ) )
		{
			pParse->ParseFluid( &fluid, NULL );

			// create a fluid for floating
			if ( fluid.index > 0 )
			{
				solid.params = defaultParams;	// copy world's params
				solid.params.enableCollisions = true;
				solid.params.pName = "fluid";
				solid.params.pGameData = static_cast<void *>(pWorld);
				fluid.params.pGameData = static_cast<void *>(pWorld);
				surfaceData = physprops->GetSurfaceIndex( fluid.surfaceprop );
				// create this as part of the world
				IPhysicsObject *pWater = pEnvironment->CreatePolyObjectStatic( pWorldCollide->solids[fluid.index], 
					surfaceData, vec3_origin, vec3_angle, &solid.params );

				pWater->SetCallbackFlags( pWater->GetCallbackFlags() | CALLBACK_NEVER_DELETED );
				pEnvironment->CreateFluidController( pWater, &fluid.params );
			}
		}
		else if ( !strcmpi( pBlock, "materialtable" ) )
		{
			intp surfaceTable[128];
			memset( surfaceTable, 0, sizeof(surfaceTable) );

			pParse->ParseSurfaceTable( surfaceTable, NULL );
			physprops->SetWorldMaterialIndexTable( surfaceTable, 128 );
		}
		else if ( !strcmpi(pBlock, "virtualterrain" ) )
		{
			bCreateVirtualTerrain = true;
			pParse->SkipBlock();
		}
		else
		{
			// unknown chunk???
			pParse->SkipBlock();
		}
	}
	physcollide->VPhysicsKeyParserDestroy( pParse );

	if ( bCreateVirtualTerrain && physcollide->SupportsVirtualMesh() )
	{
		PhysCreateVirtualTerrain( pEnvironment, pWorld, defaultParams );
	}
	return pWorldPhysics;
}

IPhysicsObject *PhysCreateWorld(IPhysicsEnvironment* pEnvironment, CBaseEntity* pWorld)
{
	staticpropmgr->CreateVPhysicsRepresentations(pEnvironment, &g_SolidSetup, pWorld);
	return PhysCreateWorld_Shared(pEnvironment, pWorld, modelinfo->GetVCollide(1), g_PhysDefaultObjectParams);
}
#endif

LUA_FUNCTION_STATIC(IPhysicsEnvironment_CreateWorldPhysics)
{
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);
	
#if ARCHITECTURE_IS_X86
	Push_IPhysicsObject(PhysCreateWorld(pEnvironment, Util::GetCBaseEntityFromEdict(Util::engineserver->PEntityOfEntIndex(0))));
#endif
	return 1;
}

LUA_FUNCTION_STATIC(CPhysCollide__tostring)
{
	CPhysCollide* pCollide = Get_CPhysCollide(1, false);
	if (!pCollide)
		LUA->PushString("CPhysCollide [NULL]");
	else
		LUA->PushString("CPhysCollide");

	return 1;
}

LUA_FUNCTION_STATIC(CPhysCollide_IsValid)
{
	CPhysCollide* pCollide = Get_CPhysCollide(1, false);

	LUA->PushBool(pCollide != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(CPhysPolysoup__tostring)
{
	CPhysPolysoup* pPolySoup = Get_CPhysPolysoup(1, false);
	if (!pPolySoup)
		LUA->PushString("CPhysPolysoup [NULL]");
	else
		LUA->PushString("CPhysPolysoup");

	return 1;
}

LUA_FUNCTION_STATIC(CPhysPolysoup_IsValid)
{
	CPhysPolysoup* pPolySoup = Get_CPhysPolysoup(1, false);

	LUA->PushBool(pPolySoup != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(CPhysConvex__tostring)
{
	CPhysConvex* pConvex = Get_CPhysConvex(1, false);
	if (!pConvex)
		LUA->PushString("CPhysConvex [NULL]");
	else
		LUA->PushString("CPhysConvex");

	return 1;
}

LUA_FUNCTION_STATIC(CPhysConvex_IsValid)
{
	CPhysConvex* pConvex = Get_CPhysConvex(1, false);

	LUA->PushBool(pConvex != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(ICollisionQuery__tostring)
{
	ICollisionQuery* pQuery = Get_ICollisionQuery(1, false);
	if (!pQuery)
		LUA->PushString("ICollisionQuery [NULL]");
	else
		LUA->PushString("ICollisionQuery");

	return 1;
}

LUA_FUNCTION_STATIC(ICollisionQuery_IsValid)
{
	ICollisionQuery* pQuery = Get_ICollisionQuery(1, false);

	LUA->PushBool(pQuery != NULL);
	return 1;
}

LUA_FUNCTION_STATIC(ICollisionQuery_ConvexCount)
{
	ICollisionQuery* pQuery = Get_ICollisionQuery(1, true);
	
	LUA->PushNumber(pQuery->ConvexCount());
	return 1;
}

LUA_FUNCTION_STATIC(ICollisionQuery_TriangleCount)
{
	ICollisionQuery* pQuery = Get_ICollisionQuery(1, true);
	int convexIndex = (int)LUA->CheckNumber(2);

	LUA->PushNumber(pQuery->TriangleCount(convexIndex));
	return 1;
}

LUA_FUNCTION_STATIC(ICollisionQuery_GetTriangleMaterialIndex)
{
	ICollisionQuery* pQuery = Get_ICollisionQuery(1, true);
	int convexIndex = (int)LUA->CheckNumber(2);
	int triangleIndex = (int)LUA->CheckNumber(3);

	LUA->PushNumber(pQuery->GetTriangleMaterialIndex(convexIndex, triangleIndex));
	return 1;
}

LUA_FUNCTION_STATIC(ICollisionQuery_SetTriangleMaterialIndex)
{
	ICollisionQuery* pQuery = Get_ICollisionQuery(1, true);
	int convexIndex = (int)LUA->CheckNumber(2);
	int triangleIndex = (int)LUA->CheckNumber(3);
	int materialIndex = (int)LUA->CheckNumber(4);

	pQuery->SetTriangleMaterialIndex(convexIndex, triangleIndex, materialIndex);
	return 0;
}

LUA_FUNCTION_STATIC(ICollisionQuery_GetTriangleVerts)
{
	ICollisionQuery* pQuery = Get_ICollisionQuery(1, true);
	int convexIndex = (int)LUA->CheckNumber(2);
	int triangleIndex = (int)LUA->CheckNumber(3);

	Vector verts[3];
	pQuery->GetTriangleVerts(convexIndex, triangleIndex, verts);
	Push_Vector(&verts[0]);
	Push_Vector(&verts[1]);
	Push_Vector(&verts[2]);
	return 3;
}

LUA_FUNCTION_STATIC(ICollisionQuery_SetTriangleVerts)
{
	ICollisionQuery* pQuery = Get_ICollisionQuery(1, true);
	int convexIndex = (int)LUA->CheckNumber(2);
	int triangleIndex = (int)LUA->CheckNumber(3);
	Vector verts[3];
	verts[0] = *Get_Vector(4, true);
	verts[1] = *Get_Vector(5, true);
	verts[2] = *Get_Vector(6, true);

	pQuery->SetTriangleVerts(convexIndex, triangleIndex, verts);
	return 0;
}

LUA_FUNCTION_STATIC(physcollide_BBoxToCollide)
{
	Vector* mins = Get_Vector(1, true);
	Vector* maxs = Get_Vector(2, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	Push_CPhysCollide(physcollide->BBoxToCollide(*mins, *maxs));
	return 1;
}

LUA_FUNCTION_STATIC(physcollide_BBoxToConvex)
{
	Vector* mins = Get_Vector(1, true);
	Vector* maxs = Get_Vector(2, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	Push_CPhysConvex(physcollide->BBoxToConvex(*mins, *maxs));
	return 1;
}

LUA_FUNCTION_STATIC(physcollide_ConvertConvexToCollide)
{
	CPhysConvex* pConvex = Get_CPhysConvex(1, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	Push_CPhysCollide(physcollide->ConvertConvexToCollide(&pConvex, 1));
	Delete_CPhysConvex(pConvex);

	return 1;
}

LUA_FUNCTION_STATIC(physcollide_ConvertPolysoupToCollide)
{
	CPhysPolysoup* pPolySoup = Get_CPhysPolysoup(1, true);
	bool bUseMOPP = LUA->GetBool(2);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	Push_CPhysCollide(physcollide->ConvertPolysoupToCollide(pPolySoup, bUseMOPP));
	Delete_CPhysPolysoup(pPolySoup);

	return 1;
}

LUA_FUNCTION_STATIC(physcollide_ConvexFree)
{
	CPhysConvex* pConvex = Get_CPhysConvex(1, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	physcollide->ConvexFree(pConvex);
	Delete_CPhysConvex(pConvex);

	return 0;
}

LUA_FUNCTION_STATIC(physcollide_PolysoupCreate)
{
	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	Push_CPhysPolysoup(physcollide->PolysoupCreate());
	return 1;
}

LUA_FUNCTION_STATIC(physcollide_PolysoupAddTriangle)
{
	CPhysPolysoup* pPolySoup = Get_CPhysPolysoup(1, true);
	Vector* a = Get_Vector(2, true);
	Vector* b = Get_Vector(3, true);
	Vector* c = Get_Vector(4, true);
	int materialIndex = (int)LUA->CheckNumber(5);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	physcollide->PolysoupAddTriangle(pPolySoup, *a, *b, *c, materialIndex);
	return 0;
}

LUA_FUNCTION_STATIC(physcollide_PolysoupDestroy)
{
	CPhysPolysoup* pPolySoup = Get_CPhysPolysoup(1, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	physcollide->PolysoupDestroy(pPolySoup);
	Delete_CPhysPolysoup(pPolySoup);
	return 0;
}

LUA_FUNCTION_STATIC(physcollide_CollideGetAABB)
{
	CPhysCollide* pCollide = Get_CPhysCollide(1, true);
	Vector* pOrigin = Get_Vector(2, true);
	QAngle* pRotation = Get_QAngle(3, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	Vector mins, maxs;
	physcollide->CollideGetAABB(&mins, &maxs, pCollide, *pOrigin, *pRotation);
	Push_Vector(&mins);
	Push_Vector(&maxs);
	return 2;
}

LUA_FUNCTION_STATIC(physcollide_CollideGetExtent)
{
	CPhysCollide* pCollide = Get_CPhysCollide(1, true);
	Vector* pOrigin = Get_Vector(2, true);
	QAngle* pRotation = Get_QAngle(3, true);
	Vector* pDirection = Get_Vector(4, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	Vector vec = physcollide->CollideGetExtent(pCollide, *pOrigin, *pRotation, *pDirection);
	Push_Vector(&vec);
	return 1;
}

LUA_FUNCTION_STATIC(physcollide_CollideGetMassCenter)
{
	CPhysCollide* pCollide = Get_CPhysCollide(1, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	Vector pMassCenter;
	physcollide->CollideGetMassCenter(pCollide, &pMassCenter);
	Push_Vector(&pMassCenter);
	return 1;
}

LUA_FUNCTION_STATIC(physcollide_CollideGetOrthographicAreas)
{
	CPhysCollide* pCollide = Get_CPhysCollide(1, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	Vector vec = physcollide->CollideGetOrthographicAreas(pCollide);
	Push_Vector(&vec);
	return 1;
}

LUA_FUNCTION_STATIC(physcollide_CollideIndex)
{
	CPhysCollide* pCollide = Get_CPhysCollide(1, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	LUA->PushNumber(physcollide->CollideIndex(pCollide));
	return 1;
}

LUA_FUNCTION_STATIC(physcollide_CollideSetMassCenter)
{
	CPhysCollide* pCollide = Get_CPhysCollide(1, true);
	Vector* pMassCenter = Get_Vector(2, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	physcollide->CollideSetMassCenter(pCollide, *pMassCenter);
	return 0;
}

LUA_FUNCTION_STATIC(physcollide_CollideSetOrthographicAreas)
{
	CPhysCollide* pCollide = Get_CPhysCollide(1, true);
	Vector* pArea = Get_Vector(2, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	physcollide->CollideSetOrthographicAreas(pCollide, *pArea);
	return 0;
}

LUA_FUNCTION_STATIC(physcollide_CollideSize)
{
	CPhysCollide* pCollide = Get_CPhysCollide(1, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	LUA->PushNumber(physcollide->CollideSize(pCollide));
	return 1;
}

LUA_FUNCTION_STATIC(physcollide_CollideSurfaceArea)
{
	CPhysCollide* pCollide = Get_CPhysCollide(1, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	LUA->PushNumber(physcollide->CollideSurfaceArea(pCollide));
	return 1;
}

LUA_FUNCTION_STATIC(physcollide_CollideVolume)
{
	CPhysCollide* pCollide = Get_CPhysCollide(1, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	LUA->PushNumber(physcollide->CollideVolume(pCollide));
	return 1;
}

LUA_FUNCTION_STATIC(physcollide_CollideWrite)
{
	CPhysCollide* pCollide = Get_CPhysCollide(1, true);
	bool bSwap = LUA->GetBool(2);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	int iSize = physcollide->CollideSize(pCollide);
	char* pData = (char*)stackalloc(iSize);
	physcollide->CollideWrite(pData, pCollide, bSwap);
	LUA->PushString(pData, iSize);
	return 1;
}

LUA_FUNCTION_STATIC(physcollide_UnserializeCollide)
{
	Get_CPhysCollide(1, true);
	const char* pData = LUA->CheckString(2);
	int iSize = LUA->ObjLen(2);
	int index = LUA->CheckNumber(3);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	Push_CPhysCollide(physcollide->UnserializeCollide((char*)pData, iSize, index));
	return 1;
}

/*LUA_FUNCTION_STATIC(physcollide_ConvexFromVerts)
{
	CPhysCollide* pCollide = Get_CPhysCollide(1, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	physcollide->ConvexFromVerts(pData, pCollide, bSwap);
	return 1;
}*/

/*LUA_FUNCTION_STATIC(physcollide_ConvexFromVerts)
{
	CPhysCollide* pCollide = Get_CPhysCollide(1, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	physcollide->ConvexFromPlanes(pData, pCollide, bSwap);
	return 1;
}*/

LUA_FUNCTION_STATIC(physcollide_ConvexSurfaceArea)
{
	CPhysConvex* pConvex = Get_CPhysConvex(1, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	LUA->PushNumber(physcollide->ConvexSurfaceArea(pConvex));
	return 1;
}

LUA_FUNCTION_STATIC(physcollide_ConvexVolume)
{
	CPhysConvex* pConvex = Get_CPhysConvex(1, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	LUA->PushNumber(physcollide->ConvexVolume(pConvex));
	return 1;
}

/*LUA_FUNCTION_STATIC(physcollide_CreateDebugMesh)
{
	CPhysConvex* pConvex = Get_CPhysConvex(1, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	LUA->PushNumber(physcollide->CreateDebugMesh(pConvex));
	return 1;
}*/

LUA_FUNCTION_STATIC(physcollide_CreateQueryModel)
{
	CPhysCollide* pCollide = Get_CPhysCollide(1, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	Push_ICollisionQuery(physcollide->CreateQueryModel(pCollide));
	return 1;
}

LUA_FUNCTION_STATIC(physcollide_DestroyQueryModel)
{
	ICollisionQuery* pQuery = Get_ICollisionQuery(1, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	physcollide->DestroyQueryModel(pQuery);
	Delete_ICollisionQuery(pQuery);
	return 0;
}

LUA_FUNCTION_STATIC(physcollide_DestroyCollide)
{
	CPhysCollide* pCollide = Get_CPhysCollide(1, true);

	if (!physcollide)
		LUA->ThrowError("Failed to get IPhysicsCollision!");

	physcollide->DestroyCollide(pCollide);
	Delete_CPhysCollide(pCollide);
	return 0;
}

/*
 * BUG: In GMOD, this function checks the main IPhysicsEnvironment to see if the IPhysicsObject exists in it?
 * This causes all IPhysicsObject from other environments to be marked as invalid.
 */
Detouring::Hook detour_GMod_Util_IsPhysicsObjectValid;
bool hook_GMod_Util_IsPhysicsObjectValid(IPhysicsObject* obj)
{
	if (!obj)
		return false;

	// Are there cases where they somehow can be without environment?
	// Verify this later to make sure that we didn't break anything.

	return true;
}

void CPhysEnvModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	CPhysCollide_TypeID = g_Lua->CreateMetaTable("CPhysCollide");
		Util::AddFunc(CPhysCollide__tostring, "__tostring");
		Util::AddFunc(Default__Index, "__index");
		Util::AddFunc(CPhysCollide_IsValid, "IsValid");
	g_Lua->Pop(1);

	CPhysPolysoup_TypeID = g_Lua->CreateMetaTable("CPhysPolysoup");
		Util::AddFunc(CPhysPolysoup__tostring, "__tostring");
		Util::AddFunc(Default__Index, "__index");
		Util::AddFunc(CPhysPolysoup_IsValid, "IsValid");
	g_Lua->Pop(1);

	CPhysConvex_TypeID = g_Lua->CreateMetaTable("CPhysCollide");
		Util::AddFunc(CPhysConvex__tostring, "__tostring");
		Util::AddFunc(Default__Index, "__index");
		Util::AddFunc(CPhysConvex_IsValid, "IsValid");
	g_Lua->Pop(1);

	ICollisionQuery_TypeID = g_Lua->CreateMetaTable("ICollisionQuery");
		Util::AddFunc(ICollisionQuery__tostring, "__tostring");
		Util::AddFunc(Default__Index, "__index");
		Util::AddFunc(ICollisionQuery_IsValid, "IsValid");
		Util::AddFunc(ICollisionQuery_ConvexCount, "ConvexCount");
		Util::AddFunc(ICollisionQuery_TriangleCount, "TriangleCount");
		Util::AddFunc(ICollisionQuery_GetTriangleMaterialIndex, "GetTriangleMaterialIndex");
		Util::AddFunc(ICollisionQuery_SetTriangleMaterialIndex, "SetTriangleMaterialIndex");
		Util::AddFunc(ICollisionQuery_GetTriangleVerts, "GetTriangleVerts");
		Util::AddFunc(ICollisionQuery_SetTriangleVerts, "SetTriangleVerts");
	g_Lua->Pop(1);

	IPhysicsCollisionSet_TypeID = g_Lua->CreateMetaTable("IPhysicsCollisionSet");
		Util::AddFunc(IPhysicsCollisionSet__tostring, "__tostring");
		Util::AddFunc(Default__Index, "__index");
		Util::AddFunc(IPhysicsCollisionSet_IsValid, "IsValid");
		Util::AddFunc(IPhysicsCollisionSet_EnableCollisions, "EnableCollisions");
		Util::AddFunc(IPhysicsCollisionSet_DisableCollisions, "DisableCollisions");
		Util::AddFunc(IPhysicsCollisionSet_ShouldCollide, "ShouldCollide");
	g_Lua->Pop(1);

	IPhysicsEnvironment_TypeID = g_Lua->CreateMetaTable("IPhysicsEnvironment");
		Util::AddFunc(IPhysicsEnvironment__tostring, "__tostring");
		Util::AddFunc(Default__Index, "__index");
		Util::AddFunc(IPhysicsEnvironment_IsValid, "IsValid");
		Util::AddFunc(IPhysicsEnvironment_TransferObject, "TransferObject");
		Util::AddFunc(IPhysicsEnvironment_SetGravity, "SetGravity");
		Util::AddFunc(IPhysicsEnvironment_GetGravity, "GetGravity");
		Util::AddFunc(IPhysicsEnvironment_SetAirDensity, "SetAirDensity");
		Util::AddFunc(IPhysicsEnvironment_GetAirDensity, "GetAirDensity");
		Util::AddFunc(IPhysicsEnvironment_SetPerformanceSettings, "SetPerformanceSettings");
		Util::AddFunc(IPhysicsEnvironment_GetPerformanceSettings, "GetPerformanceSettings");
		Util::AddFunc(IPhysicsEnvironment_GetNextFrameTime, "GetNextFrameTime");
		Util::AddFunc(IPhysicsEnvironment_GetSimulationTime, "GetSimulationTime");
		Util::AddFunc(IPhysicsEnvironment_SetSimulationTimestep, "SetSimulationTimestep");
		Util::AddFunc(IPhysicsEnvironment_GetSimulationTimestep, "GetSimulationTimestep");
		Util::AddFunc(IPhysicsEnvironment_GetActiveObjectCount, "GetActiveObjectCount");
		Util::AddFunc(IPhysicsEnvironment_GetActiveObjects, "GetActiveObjects");
		Util::AddFunc(IPhysicsEnvironment_GetObjectList, "GetObjectList");
		Util::AddFunc(IPhysicsEnvironment_IsInSimulation, "IsInSimulation");
		Util::AddFunc(IPhysicsEnvironment_ResetSimulationClock, "ResetSimulationClock");
		Util::AddFunc(IPhysicsEnvironment_CleanupDeleteList, "CleanupDeleteList");
		Util::AddFunc(IPhysicsEnvironment_SetQuickDelete, "SetQuickDelete");
		Util::AddFunc(IPhysicsEnvironment_EnableDeleteQueue, "EnableDeleteQueue");
		Util::AddFunc(IPhysicsEnvironment_Simulate, "Simulate");
		Util::AddFunc(IPhysicsEnvironment_CreatePolyObject, "CreatePolyObject");
		Util::AddFunc(IPhysicsEnvironment_CreatePolyObjectStatic, "CreatePolyObjectStatic");
		Util::AddFunc(IPhysicsEnvironment_CreateSphereObject, "CreateSphereObject");
		Util::AddFunc(IPhysicsEnvironment_DestroyObject, "DestroyObject");
		Util::AddFunc(IPhysicsEnvironment_IsCollisionModelUsed, "IsCollisionModelUsed");
		Util::AddFunc(IPhysicsEnvironment_SetObjectEventHandler, "SetObjectEventHandler");
		//Util::AddFunc(IPhysicsEnvironment_CreateCopy, "CreateCopy");
		Util::AddFunc(IPhysicsEnvironment_CreateWorldPhysics, "CreateWorldPhysics");
	g_Lua->Pop(1);

	if (Util::PushTable("physenv"))
	{
		Util::AddFunc(physenv_CreateEnvironment, "CreateEnvironment");
		Util::AddFunc(physenv_GetActiveEnvironmentByIndex, "GetActiveEnvironmentByIndex");
		Util::AddFunc(physenv_DestroyEnvironment, "DestroyEnvironment");
		Util::AddFunc(physenv_GetCurrentEnvironment, "GetCurrentEnvironment");

		Util::AddFunc(physenv_FindCollisionSet, "FindCollisionSet");
		Util::AddFunc(physenv_FindOrCreateCollisionSet, "FindOrCreateCollisionSet");
		Util::AddFunc(physenv_DestroyAllCollisionSets, "DestroyAllCollisionSets");

		Util::AddFunc(physenv_SetLagThreshold, "SetLagThreshold");
		Util::AddFunc(physenv_GetLagThreshold, "GetLagThreshold");
		Util::AddFunc(physenv_SetPhysSkipType, "SetPhysSkipType");

		Util::AddValue(IVP_NoSkip, "IVP_NoSkip");
		Util::AddValue(IVP_SkipImpact, "IVP_SkipImpact");
		Util::AddValue(IVP_SkipSimulation, "IVP_SkipSimulation");
		Util::PopTable();
	}

	Util::StartTable();
		Util::AddFunc(physcollide_BBoxToCollide, "BBoxToCollide");
		Util::AddFunc(physcollide_BBoxToConvex, "BBoxToConvex");
		Util::AddFunc(physcollide_ConvertConvexToCollide, "ConvertConvexToCollide");
		Util::AddFunc(physcollide_ConvertPolysoupToCollide, "ConvertPolysoupToCollide");
		Util::AddFunc(physcollide_ConvexFree, "ConvexFree");
		Util::AddFunc(physcollide_PolysoupCreate, "PolysoupCreate");
		Util::AddFunc(physcollide_PolysoupAddTriangle, "PolysoupAddTriangle");
		Util::AddFunc(physcollide_PolysoupDestroy, "PolysoupDestroy");
		Util::AddFunc(physcollide_CollideGetAABB, "CollideGetAABB");
		Util::AddFunc(physcollide_CollideGetExtent, "CollideGetExtent");
		Util::AddFunc(physcollide_CollideGetMassCenter, "CollideGetMassCenter");
		Util::AddFunc(physcollide_CollideGetOrthographicAreas, "CollideGetOrthographicAreas");
		Util::AddFunc(physcollide_CollideIndex, "CollideIndex");
		Util::AddFunc(physcollide_CollideSetMassCenter, "CollideSetMassCenter");
		Util::AddFunc(physcollide_CollideSetOrthographicAreas, "CollideSetOrthographicAreas");
		Util::AddFunc(physcollide_CollideSize, "CollideSize");
		Util::AddFunc(physcollide_CollideSurfaceArea, "CollideSurfaceArea"); 
		Util::AddFunc(physcollide_CollideVolume, "CollideVolume");
		Util::AddFunc(physcollide_CollideWrite, "CollideWrite");
		Util::AddFunc(physcollide_UnserializeCollide, "UnserializeCollide");
		Util::AddFunc(physcollide_ConvexSurfaceArea, "ConvexSurfaceArea");
		Util::AddFunc(physcollide_ConvexVolume, "ConvexVolume");
		Util::AddFunc(physcollide_CreateQueryModel, "CreateQueryModel");
		Util::AddFunc(physcollide_DestroyQueryModel, "DestroyQueryModel");
		Util::AddFunc(physcollide_DestroyCollide, "DestroyCollide");
	Util::FinishTable("physcollide");
}

void CPhysEnvModule::LuaShutdown()
{
	if (Util::PushTable("physenv"))
	{
		Util::RemoveField("CreateEnvironment");
		Util::RemoveField("GetActiveEnvironmentByIndex");
		Util::RemoveField("DestroyEnvironment");

		Util::RemoveField("FindCollisionSet");
		Util::RemoveField("FindOrCreateCollisionSet");
		Util::RemoveField("DestroyAllCollisionSets");

		Util::RemoveField("SetLagThreshold");
		Util::RemoveField("GetLagThreshold");
		Util::RemoveField("SetPhysSkipType");

		Util::RemoveField("IVP_NoSkip");
		Util::RemoveField("IVP_SkipImpact");
		Util::RemoveField("IVP_SkipSimulation");
		Util::PopTable();
	}

	Util::NukeTable("physcollide");
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

	SourceSDK::FactoryLoader server_loader("server");
	Detour::Create(
		&detour_GMod_Util_IsPhysicsObjectValid, "GMod::Util::IsPhysicsObjectValid",
		server_loader.GetModule(), Symbols::GMod_Util_IsPhysicsObjectValidSym,
		(void*)hook_GMod_Util_IsPhysicsObjectValid, m_pID
	);

	g_pCurrentMindist = Detour::ResolveSymbol<IVP_Mindist*>(vphysics_loader, Symbols::g_pCurrentMindistSym);
	Detour::CheckValue("get class", "g_pCurrentMindist", g_pCurrentMindist != NULL);

	g_fDeferDeleteMindist = Detour::ResolveSymbol<bool>(vphysics_loader, Symbols::g_fDeferDeleteMindistSym);
	Detour::CheckValue("get class", "g_fDeferDeleteMindist", g_fDeferDeleteMindist != NULL);

	func_CBaseEntity_VPhysicsUpdate = (Symbols::CBaseEntity_VPhysicsUpdate)Detour::GetFunction(server_loader.GetModule(), Symbols::CBaseEntity_VPhysicsUpdateSym);
	Detour::CheckValue("get function", "CBaseEntity::VPhysicsUpdate", func_CBaseEntity_VPhysicsUpdate != NULL);
}