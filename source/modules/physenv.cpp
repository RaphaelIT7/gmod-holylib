#include "LuaInterface.h"
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
#include <detouring/classproxy.hpp>
#include "unordered_set"
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
		if (Lua::PushHook("HolyLib:OnPhysicsLag"))
		{
			g_Lua->PushNumber((double)pSimulationTime);
			if (g_Lua->CallFunctionProtected(2, 1, true))
			{
				int pType = (int)g_Lua->GetNumber();
				if (pType > 2 || pType < 0)
					pType = IVP_NoSkip; // Invalid value. So we won't do shit.

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
	SourceSDK::FactoryLoader vphysics_loader("vphysics");
	if (appfn[0])
	{
		physics = (IPhysics*)appfn[0](VPHYSICS_INTERFACE_VERSION, NULL);
		physprops = (IPhysicsSurfaceProps*)appfn[0](VPHYSICS_SURFACEPROPS_INTERFACE_VERSION, NULL);
		physcollide = (IPhysicsCollision*)appfn[0](VPHYSICS_COLLISION_INTERFACE_VERSION, NULL);
	} else {
		physics = vphysics_loader.GetInterface<IPhysics>(VPHYSICS_INTERFACE_VERSION);
		physprops = vphysics_loader.GetInterface<IPhysicsSurfaceProps>(VPHYSICS_SURFACEPROPS_INTERFACE_VERSION);
		physcollide = vphysics_loader.GetInterface<IPhysicsCollision>(VPHYSICS_COLLISION_INTERFACE_VERSION);
	}

	Detour::CheckValue("get interface", "physics", physics != NULL);
	Detour::CheckValue("get interface", "physprops", physprops != NULL);
	Detour::CheckValue("get interface", "physcollide", physcollide != NULL);

	SourceSDK::FactoryLoader engine_loader("engine");
	if (appfn[0])
	{
		staticpropmgr = (IStaticPropMgrServer*)appfn[0](INTERFACEVERSION_STATICPROPMGR_SERVER, NULL);
		modelinfo = (IVModelInfo*)appfn[0](VMODELINFO_SERVER_INTERFACE_VERSION, NULL);
	} else {
		staticpropmgr = engine_loader.GetInterface<IStaticPropMgrServer>(INTERFACEVERSION_STATICPROPMGR_SERVER);
		modelinfo = engine_loader.GetInterface<IVModelInfo>(VMODELINFO_SERVER_INTERFACE_VERSION);
	}

	Detour::CheckValue("get interface", "staticpropmgr", staticpropmgr != NULL);
	Detour::CheckValue("get interface", "modelinfo", modelinfo != NULL);
}

PushReferenced_LuaClass(IPhysicsObject, GarrysMod::Lua::Type::PhysObj) // This will later cause so much pain when they become Invalid XD
Get_LuaClass(IPhysicsObject, GarrysMod::Lua::Type::PhysObj, "IPhysicsObject")

CCollisionEvent* g_Collisions = NULL;
class CLuaPhysicsObjectEvent : public IPhysicsObjectEvent
{
public:
	virtual void ObjectWake(IPhysicsObject* obj)
	{
		g_Collisions->ObjectWake(obj);

		if (!g_Lua || m_iObjectWakeFunction == -1)
			return;

		Util::ReferencePush(g_Lua, m_iObjectWakeFunction);
		Push_IPhysicsObject(obj);
		g_Lua->CallFunctionProtected(1, 0, true);
	}

	virtual void ObjectSleep(IPhysicsObject* obj)
	{
		g_Collisions->ObjectSleep(obj);

		if (!g_Lua || m_iObjectSleepFunction == -1)
			return;

		Util::ReferencePush(g_Lua, m_iObjectSleepFunction);
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

struct ILuaPhysicsEnvironment;
static int IPhysicsEnvironment_TypeID = -1;
PushReferenced_LuaClass(ILuaPhysicsEnvironment, IPhysicsEnvironment_TypeID)
Get_LuaClass(ILuaPhysicsEnvironment, IPhysicsEnvironment_TypeID, "IPhysicsEnvironment")

#ifdef SYSTEM_WINDOWS
extern void hook_CPhysicsEnvironment_DestroyObject(CPhysicsEnvironment* pEnvironment, IPhysicsObject* pObject);
class CPhysicsEnvironmentProxy : public Detouring::ClassProxy<IPhysicsEnvironment, CPhysicsEnvironmentProxy> {
public:
	CPhysicsEnvironmentProxy(IPhysicsEnvironment* env) {
		if (Detour::CheckValue("initialize", "CLuaInterfaceProxy", Initialize(env)))
			Detour::CheckValue("CLuaInterface::PushPooledString", Hook(&IPhysicsEnvironment::DestroyObject, &CPhysicsEnvironmentProxy::DestroyObject));
	}

	void DeInit()
	{
		UnHook(&IPhysicsEnvironment::DestroyObject);
	}

	virtual void DestroyObject(IPhysicsObject* pObject)
	{
		hook_CPhysicsEnvironment_DestroyObject((CPhysicsEnvironment*)This(), pObject);
	}
};
#endif

static std::unordered_map<IPhysicsEnvironment*, ILuaPhysicsEnvironment*> g_pEnvironmentToLua;
static std::unordered_map<IPhysicsObject*, ILuaPhysicsEnvironment*> g_pObjects; // contains all IPhysicsObject that exist
struct ILuaPhysicsEnvironment
{
	ILuaPhysicsEnvironment(IPhysicsEnvironment* env)
	{
		pEnvironment = env;
		//g_pEnvironmentToLua[env] = this;
#ifdef SYSTEM_WINDOWS
		pEnvironmentProxy = std::make_unique<CPhysicsEnvironmentProxy>(env);
#endif
	}

	~ILuaPhysicsEnvironment()
	{
		Delete_ILuaPhysicsEnvironment(this);

		//g_pEnvironmentToLua.erase(g_pEnvironmentToLua.find(pEnvironment));
#ifdef SYSTEM_WINDOWS
		pEnvironmentProxy->DeInit();
#endif
		//physics->DestroyEnvironment(pEnvironment);
		pEnvironment = NULL;

		for (IPhysicsObject* pObject : pObjects)
		{
			auto it = g_pObjects.find(pObject);
			if (it != g_pObjects.end())
				g_pObjects.erase(it);
		}

		pObjects.clear();

		if (pObjectEvent)
			delete pObjectEvent;

		//if (pCollisionSolver)
		//	delete pCollisionSolver;

		//if (pCollisionEvent)
		//	delete pCollisionEvent;
	}

	inline void RegisterObject(IPhysicsObject* pObject)
	{
		auto it = pObjects.find(pObject);
		if (it == pObjects.end())
			pObjects.insert(pObject);
	}

	inline void UnregisterObject(IPhysicsObject* pObject)
	{
		auto it = pObjects.find(pObject);
		if (it != pObjects.end())
			pObjects.erase(it);
	}

	inline bool HasObject(IPhysicsObject* pObject)
	{
		auto it = pObjects.find(pObject);
		return it != pObjects.end();
	}

	inline bool Created()
	{
		return bCreatedEnvironment;
	}

	bool bCreatedEnvironment = false; // If we were the one that created the environment.
	std::unordered_set<IPhysicsObject*> pObjects;
	
	IPhysicsEnvironment* pEnvironment = NULL;
#ifdef SYSTEM_WINDOWS
	std::unique_ptr<CPhysicsEnvironmentProxy> pEnvironmentProxy = NULL;
#endif
	CLuaPhysicsObjectEvent* pObjectEvent = NULL;
	//CLuaPhysicsCollisionSolver* pCollisionSolver = NULL;
	//CLuaPhysicsCollisionEvent* pCollisionEvent = NULL;
};

static inline ILuaPhysicsEnvironment* RegisterPhysicsEnvironment(IPhysicsEnvironment* pEnv)
{
	auto it = g_pEnvironmentToLua.find(pEnv);
	if (it == g_pEnvironmentToLua.end())
	{
		ILuaPhysicsEnvironment* pLuaEnv = new ILuaPhysicsEnvironment(pEnv);
		g_pEnvironmentToLua[pEnv] = pLuaEnv;
		return pLuaEnv;
	}

	return it->second;
}

static inline void UnregisterPhysicsEnvironment(IPhysicsEnvironment* pEnv)
{
	auto it = g_pEnvironmentToLua.find(pEnv);
	if (it != g_pEnvironmentToLua.end())
	{
		g_pEnvironmentToLua.erase(it);
		delete it->second;
	}
}

static inline ILuaPhysicsEnvironment* GetPhysicsEnvironment(IPhysicsEnvironment* pEnv)
{
	auto it = g_pEnvironmentToLua.find(pEnv);
	if (it != g_pEnvironmentToLua.end())
		return it->second;

	return NULL;
}

static inline void RegisterPhysicsObject(ILuaPhysicsEnvironment* pEnv, IPhysicsObject* pObject)
{
	auto it = g_pObjects.find(pObject);
	if (it == g_pObjects.end())
		g_pObjects[pObject] = pEnv;

	pEnv->RegisterObject(pObject);
}

static inline void UnregisterPhysicsObject(ILuaPhysicsEnvironment* pEnv, IPhysicsObject* pObject)
{
	auto it = g_pObjects.find(pObject);
	if (it != g_pObjects.end())
		g_pObjects.erase(it);

	pEnv->UnregisterObject(pObject);
}

static inline bool IsRegisteredPhysicsObject(IPhysicsObject* pObject)
{
	auto it = g_pObjects.find(pObject);
	return it != g_pObjects.end();
}

static inline ILuaPhysicsEnvironment* GetPhysicsObjectLuaEnvironment(IPhysicsObject* pObject)
{
	auto it = g_pObjects.find(pObject);
	if (it != g_pObjects.end())
		return it->second;

	return NULL;
}

/*
 * BUG: The engine likes to use PhysDestroyObject which will call DestroyObject on the wrong environment now.
 * Solution: If it was called on the main Environment, we loop thru all environments until we find our object and delete it.
 * 
 * Real Solution: The real soulution would be to let the IPhysicsObject store it's environment and have a method ::GetEnvironment and use that inside PhysDestroyObject to delete it properly.
 *                It would most likely require one to edit the vphysics dll for a clean and proper solution.
 *                Inside the engine there most likely would also be a CPhysicsObject::SetEnvironment method that would need to be added and called at all points, were it uses m_objects.AddToTail()
 */
static Detouring::Hook detour_CPhysicsEnvironment_DestroyObject;
void hook_CPhysicsEnvironment_DestroyObject(CPhysicsEnvironment* pEnvironment, IPhysicsObject* pObject)
{
	int foundIndex = -1;
	ILuaPhysicsEnvironment* pLuaEnvironment = GetPhysicsObjectLuaEnvironment(pObject);
	if (!pLuaEnvironment || !pLuaEnvironment->pEnvironment)
	{
		CBaseEntity* pEntity = (CBaseEntity*)pObject->GetGameData();
		Warning("holylib - physenv: Failed to find environment of physics object.... How?!? (%p, %i, %s)\n", pEntity, pEntity ? pEntity->entindex() : -1, pEntity ? pEntity->edict()->GetClassName() : "NULL");
		return;
	}

	CPhysicsEnvironment* pEnv = (CPhysicsEnvironment*)pLuaEnvironment->pEnvironment;
	for (int i = pEnv->m_objects.Count(); --i >= 0; )
		if (pEnv->m_objects[i] == pObject)
			foundIndex = i;

	if (foundIndex == -1)
	{
		if (g_pPhysEnvModule.InDebug())
			Warning("holylib - physenv: Failed to find object on environment (%p)!\n", pEnvironment);
		return;
	}

	pEnv->m_objects.FastRemove(foundIndex);

	UnregisterPhysicsObject(pLuaEnvironment, pObject);
	Delete_IPhysicsObject(pObject); // Delete any reference we might hold.
	//pEnv->DestroyObject(pObject);

	CPhysicsObject* pPhysics = static_cast<CPhysicsObject*>(pObject);
	// add this flag so we can optimize some cases
	pPhysics->AddCallbackFlags(CALLBACK_MARKED_FOR_DELETE);

	// was created/destroyed without simulating.  No need to wake the neighbors!
	if (foundIndex > pEnv->m_lastObjectThisTick)
		pPhysics->ForceSilentDelete();

	if (pEnv->m_inSimulation || pEnv->m_queueDeleteObject)
	{
		// don't delete while simulating
		pEnv->m_deadObjects.AddToTail(pObject);
	}
	else
	{
		pEnv->m_pSleepEvents->DeleteObject(pPhysics);
		delete pObject;
	}
}

static Detouring::Hook detour_CPhysicsEnvironment_Restore;
bool hook_CPhysicsEnvironment_Restore(IPhysicsEnvironment* pEnv, physrestoreparams_t const& params)
{
	bool bSuccess = detour_CPhysicsEnvironment_Restore.GetTrampoline<Symbols::CPhysicsEnvironment_Restore>()(pEnv, params);
	if (bSuccess && params.type == PIID_IPHYSICSOBJECT)
	{
		ILuaPhysicsEnvironment* pLuaEnv = GetPhysicsEnvironment(pEnv);
		if (pLuaEnv)
			RegisterPhysicsObject(GetPhysicsEnvironment(pEnv), (IPhysicsObject*)(*params.ppObject));
	}

	return bSuccess;
}

static Detouring::Hook detour_CPhysicsEnvironment_TransferObject;
bool hook_CPhysicsEnvironment_TransferObject(IPhysicsEnvironment* pEnv, IPhysicsObject* pObject, IPhysicsEnvironment* pDestinationEnvironment)
{
	bool bSuccess = detour_CPhysicsEnvironment_TransferObject.GetTrampoline<Symbols::CPhysicsEnvironment_TransferObject>()(pEnv, pObject, pDestinationEnvironment);
	if (bSuccess)
	{
		ILuaPhysicsEnvironment* pLuaEnv = GetPhysicsEnvironment(pEnv);
		ILuaPhysicsEnvironment* pDestLuaEnv = GetPhysicsEnvironment(pDestinationEnvironment);
		if (pLuaEnv && pDestLuaEnv)
		{
			UnregisterPhysicsObject(pLuaEnv, pObject);
			RegisterPhysicsObject(pDestLuaEnv, pObject);
		}
	}

	return bSuccess;
}

static Detouring::Hook detour_CPhysicsEnvironment_CreateSphereObject;
IPhysicsObject* hook_CPhysicsEnvironment_CreateSphereObject(IPhysicsEnvironment* pEnv, float radius, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t* pParams, bool isStatic)
{
	IPhysicsObject* pObject = detour_CPhysicsEnvironment_CreateSphereObject.GetTrampoline<Symbols::CPhysicsEnvironment_CreateSphereObject>()(pEnv, radius, materialIndex, position, angles, pParams, isStatic);
	if (pObject)
	{
		ILuaPhysicsEnvironment* pLuaEnv = GetPhysicsEnvironment(pEnv);
		if (pLuaEnv)
			RegisterPhysicsObject(GetPhysicsEnvironment(pEnv), pObject);
	}

	return pObject;
}

static Detouring::Hook detour_CPhysicsEnvironment_UnserializeObjectFromBuffer;
IPhysicsObject* hook_CPhysicsEnvironment_UnserializeObjectFromBuffer(IPhysicsEnvironment* pEnv, void* pGameData, unsigned char* pBuffer, unsigned int bufferSize, bool enableCollisions)
{
	IPhysicsObject* pObject = detour_CPhysicsEnvironment_UnserializeObjectFromBuffer.GetTrampoline<Symbols::CPhysicsEnvironment_UnserializeObjectFromBuffer>()(pEnv, pGameData, pBuffer, bufferSize, enableCollisions);
	if (pObject)
	{
		ILuaPhysicsEnvironment* pLuaEnv = GetPhysicsEnvironment(pEnv);
		if (pLuaEnv)
			RegisterPhysicsObject(GetPhysicsEnvironment(pEnv), pObject);
	}

	return pObject;
}

static Detouring::Hook detour_CPhysicsEnvironment_CreatePolyObjectStatic;
IPhysicsObject* hook_CPhysicsEnvironment_CreatePolyObjectStatic(IPhysicsEnvironment* pEnv, const CPhysCollide* pCollisionModel, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t* pParams)
{
	IPhysicsObject* pObject = detour_CPhysicsEnvironment_CreatePolyObjectStatic.GetTrampoline<Symbols::CPhysicsEnvironment_CreatePolyObjectStatic>()(pEnv, pCollisionModel, materialIndex, position, angles, pParams);
	if (pObject)
	{
		ILuaPhysicsEnvironment* pLuaEnv = GetPhysicsEnvironment(pEnv);
		if (pLuaEnv)
			RegisterPhysicsObject(GetPhysicsEnvironment(pEnv), pObject);
	}

	return pObject;
}

static Detouring::Hook detour_CPhysicsEnvironment_CreatePolyObject;
IPhysicsObject* hook_CPhysicsEnvironment_CreatePolyObject(IPhysicsEnvironment* pEnv, const CPhysCollide* pCollisionModel, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t* pParams)
{
	IPhysicsObject* pObject = detour_CPhysicsEnvironment_CreatePolyObject.GetTrampoline<Symbols::CPhysicsEnvironment_CreatePolyObject>()(pEnv, pCollisionModel, materialIndex, position, angles, pParams);
	if (pObject)
	{
		ILuaPhysicsEnvironment* pLuaEnv = GetPhysicsEnvironment(pEnv);
		if (pLuaEnv)
			RegisterPhysicsObject(GetPhysicsEnvironment(pEnv), pObject);
	}

	return pObject;
}

static Detouring::Hook detour_CPhysicsEnvironment_D2;
void hook_CPhysicsEnvironment_D2(IPhysicsEnvironment* pEnv)
{
	UnregisterPhysicsEnvironment(pEnv);
	detour_CPhysicsEnvironment_D2.GetTrampoline<Symbols::CPhysicsEnvironment_D2>()(pEnv);
}

static Detouring::Hook detour_CPhysicsEnvironment_C2;
void hook_CPhysicsEnvironment_C2(IPhysicsEnvironment* pEnv)
{
	detour_CPhysicsEnvironment_C2.GetTrampoline<Symbols::CPhysicsEnvironment_C2>()(pEnv);
	RegisterPhysicsEnvironment(pEnv);
}

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

static IPhysicsEnvironment* GetPhysicsEnvironment(int iStackPos, bool bError)
{
	ILuaPhysicsEnvironment* pLuaEnv = Get_ILuaPhysicsEnvironment(iStackPos, bError);
	if (bError && (!pLuaEnv || !pLuaEnv->pEnvironment))
		g_Lua->ThrowError(triedNull_ILuaPhysicsEnvironment.c_str());

	return pLuaEnv ? pLuaEnv->pEnvironment : NULL;
}

LUA_FUNCTION_STATIC(physenv_CreateEnvironment)
{
	if (!physics)
		LUA->ThrowError("Failed to get IPhysics!");

	ILuaPhysicsEnvironment* pLua = GetPhysicsEnvironment(physics->CreateEnvironment());
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

		g_Collisions = (CCollisionEvent*)pMainEnvironment->m_pCollisionSolver->m_pSolver; // Raw access is always fun :D

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

	pLua->bCreatedEnvironment = true; // WE created the environment.
	Push_ILuaPhysicsEnvironment(pLua);
	return 1;
}

LUA_FUNCTION_STATIC(physenv_GetActiveEnvironmentByIndex)
{
	if (!physics)
		LUA->ThrowError("Failed to get IPhysics!");

	int index = (int)LUA->CheckNumber(1);
	IPhysicsEnvironment* pEnvironment = physics->GetActiveEnvironmentByIndex(index);
	if (!pEnvironment)
	{
		Push_ILuaPhysicsEnvironment(NULL);
		return 1;
	}

	auto it = g_pEnvironmentToLua.find(pEnvironment);
	if (it != g_pEnvironmentToLua.end())
	{
		Push_ILuaPhysicsEnvironment(it->second);
		return 1;
	}

	Push_ILuaPhysicsEnvironment(new ILuaPhysicsEnvironment(pEnvironment));
	return 1;
}

static std::vector<ILuaPhysicsEnvironment*> g_pCurrentEnvironment;
LUA_FUNCTION_STATIC(physenv_DestroyEnvironment)
{
	if (!physics)
		LUA->ThrowError("Failed to get IPhysics!");

	ILuaPhysicsEnvironment* pLuaEnv = Get_ILuaPhysicsEnvironment(1, true);
	for (ILuaPhysicsEnvironment* ppLuaEnv : g_pCurrentEnvironment)
	{
		if (pLuaEnv == ppLuaEnv)
			LUA->ThrowError("Tried to delete a IPhysicsEnvironment that is simulating!"); // Don't even dare.....
	}

	CPhysicsEnvironment* pEnvironment = (CPhysicsEnvironment*)pLuaEnv->pEnvironment;
	if (pLuaEnv->pEnvironment)
	{
		for (int i = pEnvironment->m_objects.Count(); --i >= 0; )
		{
			IPhysicsObject* pObject = pEnvironment->m_objects[i];
			CBaseEntity* pEntity = (CBaseEntity*)pObject->GetGameData();
			if (pEntity)
				pEntity->VPhysicsDestroyObject();
		}
	}

	physics->DestroyEnvironment(pEnvironment);
	delete pLuaEnv;

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

LUA_FUNCTION_STATIC(IPhysicsCollisionSet__tostring)
{
	IPhysicsCollisionSet* pCollideSet = Get_IPhysicsCollisionSet(1, false);
	if (!pCollideSet)
		LUA->PushString("IPhysicsCollisionSet [NULL]");
	else
		LUA->PushString("IPhysicsCollisionSet");

	return 1;
}

Default__index(IPhysicsCollisionSet);
Default__newindex(IPhysicsCollisionSet);
Default__GetTable(IPhysicsCollisionSet);

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

Default__index(ILuaPhysicsEnvironment);
Default__newindex(ILuaPhysicsEnvironment);
Default__GetTable(ILuaPhysicsEnvironment);

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
		Push_IPhysicsObject(pActiveList[i]);
		Util::RawSetI(LUA, -2, ++idx);
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
		Push_IPhysicsObject(pList[i]);
		Util::RawSetI(LUA, -2, ++idx);
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

LUA_FUNCTION_STATIC(IPhysicsEnvironment_Simulate)
{
	ILuaPhysicsEnvironment* pLuaEnv = Get_ILuaPhysicsEnvironment(1, true);
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);

	float deltaTime = (float)LUA->CheckNumber(2);
	bool onlyEntities = LUA->GetBool(3);

	VPROF_BUDGET("HolyLib(Lua) - IPhysicsEnvironment::Simulate", VPROF_BUDGETGROUP_HOLYLIB);
	g_pCurrentEnvironment.push_back(pLuaEnv);
	if (!onlyEntities)
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
					pEntity->CollisionProp()->MarkSurroundingBoundsDirty();

				pEntity->VPhysicsUpdate( pActiveList[i] );
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

	g_pCurrentEnvironment.pop_back();
	return 0;
}

LUA_FUNCTION_STATIC(physenv_GetCurrentEnvironment)
{
	Push_ILuaPhysicsEnvironment(g_pCurrentEnvironment.back());
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
	ILuaPhysicsEnvironment* pLuaEnv = Get_ILuaPhysicsEnvironment(1, true);
	IPhysicsEnvironment* pEnvironment = GetPhysicsEnvironment(1, true);
	
#if ARCHITECTURE_IS_X86
	Push_IPhysicsObject(PhysCreateWorld(pEnvironment, Util::GetCBaseEntityFromEdict(Util::engineserver->PEntityOfEntIndex(0))));

	int iCount = 0;
	IPhysicsObject** pList = (IPhysicsObject**)pEnvironment->GetObjectList(&iCount);
	for (int i = 0; i < iCount; ++i)
		pLuaEnv->RegisterObject(pList[i]); // Make sure everything is registered properly

	return 1;
#else
	return 0;
#endif
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

Default__index(CPhysCollide);
Default__newindex(CPhysCollide);
Default__GetTable(CPhysCollide);

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

Default__index(CPhysPolysoup);
Default__newindex(CPhysPolysoup);
Default__GetTable(CPhysPolysoup);

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

Default__index(CPhysConvex);
Default__newindex(CPhysConvex);
Default__GetTable(CPhysConvex);

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

Default__index(ICollisionQuery);
Default__newindex(ICollisionQuery);
Default__GetTable(ICollisionQuery);

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
 * 
 * Gmod does this because it can't invalidate the userdata properly which means that calling a function like PhysObj:Wake could be called on a invalid pointer.
 * So they seem to have added this function as a workaround to check if the pointer Lua has is still valid.
 */
Detouring::Hook detour_GMod_Util_IsPhysicsObjectValid;
bool hook_GMod_Util_IsPhysicsObjectValid(IPhysicsObject* pObject)
{
	if (!pObject)
		return false;

	// This is what Gmod does. Gmod loops thru all physics objects of the main environment to check if the specific IPhysicsObject is part of it.
	/*int iCount = 0;
	IPhysicsObject** pList = (IPhysicsObject**)physenv->GetObjectList(&iCount);
	for (int i = 0; i < iCount; ++i)
	{
		if (pList[i] == pObject)
			return true;
	}*/

	// Should be O(1) now since were using a hash / unordered_map.
	return IsRegisteredPhysicsObject(pObject);
}

/*
 * BUG: This causes weird crashes. WHY. 
 * NOTE: This only ocurrs when you delete a Environment that still has objects?
 */
/*Detouring::Hook detour_CBaseEntity_GMOD_VPhysicsTest;
void hook_CBaseEntity_GMOD_VPhysicsTest(CBaseEntity* pEnt, IPhysicsObject* obj)
{
	// NUKE THE FUNCTION for now.
	// detour_CBaseEntity_GMOD_VPhysicsTest.GetTrampoline<Symbols::CBaseEntity_GMOD_VPhysicsTest>()(pEnt, obj);
}*/

static bool g_bCallPhysHook = false;
static Detouring::Hook detour_PhysFrame;
static void hook_PhysFrame(float deltaTime)
{
	if (g_bCallPhysHook && Lua::PushHook("HolyLib:OnPhysFrame"))
	{
		g_Lua->PushNumber(deltaTime);
		if (g_Lua->CallFunctionProtected(2, 1, true))
		{
			bool skipEngine = g_Lua->GetBool(-1);
			g_Lua->Pop(1);
			if (skipEngine)
				return;
		}
	}

	detour_PhysFrame.GetTrampoline<Symbols::PhysFrame>()(deltaTime);
}

LUA_FUNCTION_STATIC(physenv_EnablePhysHook)
{
	g_bCallPhysHook = LUA->GetBool(1);

	return 0;
}

void CPhysEnvModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	CPhysCollide_TypeID = g_Lua->CreateMetaTable("CPhysCollide");
		Util::AddFunc(CPhysCollide__tostring, "__tostring");
		Util::AddFunc(CPhysCollide__index, "__index");
		Util::AddFunc(CPhysCollide__newindex, "__newindex");
		Util::AddFunc(CPhysCollide_IsValid, "IsValid");
		Util::AddFunc(CPhysCollide_GetTable, "GetTable");
	g_Lua->Pop(1);

	CPhysPolysoup_TypeID = g_Lua->CreateMetaTable("CPhysPolysoup");
		Util::AddFunc(CPhysPolysoup__tostring, "__tostring");
		Util::AddFunc(CPhysPolysoup__index, "__index");
		Util::AddFunc(CPhysPolysoup__newindex, "__newindex");
		Util::AddFunc(CPhysPolysoup_IsValid, "IsValid");
		Util::AddFunc(CPhysPolysoup_GetTable, "GetTable");
	g_Lua->Pop(1);

	CPhysConvex_TypeID = g_Lua->CreateMetaTable("CPhysCollide");
		Util::AddFunc(CPhysConvex__tostring, "__tostring");
		Util::AddFunc(CPhysConvex__index, "__index");
		Util::AddFunc(CPhysConvex__newindex, "__newindex");
		Util::AddFunc(CPhysConvex_IsValid, "IsValid");
		Util::AddFunc(CPhysConvex_GetTable, "GetTable");
	g_Lua->Pop(1);

	ICollisionQuery_TypeID = g_Lua->CreateMetaTable("ICollisionQuery");
		Util::AddFunc(ICollisionQuery__tostring, "__tostring");
		Util::AddFunc(ICollisionQuery__index, "__index");
		Util::AddFunc(ICollisionQuery__newindex, "__newindex");
		Util::AddFunc(ICollisionQuery_IsValid, "IsValid");
		Util::AddFunc(ICollisionQuery_GetTable, "GetTable");
		Util::AddFunc(ICollisionQuery_ConvexCount, "ConvexCount");
		Util::AddFunc(ICollisionQuery_TriangleCount, "TriangleCount");
		Util::AddFunc(ICollisionQuery_GetTriangleMaterialIndex, "GetTriangleMaterialIndex");
		Util::AddFunc(ICollisionQuery_SetTriangleMaterialIndex, "SetTriangleMaterialIndex");
		Util::AddFunc(ICollisionQuery_GetTriangleVerts, "GetTriangleVerts");
		Util::AddFunc(ICollisionQuery_SetTriangleVerts, "SetTriangleVerts");
	g_Lua->Pop(1);

	IPhysicsCollisionSet_TypeID = g_Lua->CreateMetaTable("IPhysicsCollisionSet");
		Util::AddFunc(IPhysicsCollisionSet__tostring, "__tostring");
		Util::AddFunc(IPhysicsCollisionSet__index, "__index");
		Util::AddFunc(IPhysicsCollisionSet__newindex, "__newindex");
		Util::AddFunc(IPhysicsCollisionSet_IsValid, "IsValid");
		Util::AddFunc(IPhysicsCollisionSet_GetTable, "GetTable");
		Util::AddFunc(IPhysicsCollisionSet_EnableCollisions, "EnableCollisions");
		Util::AddFunc(IPhysicsCollisionSet_DisableCollisions, "DisableCollisions");
		Util::AddFunc(IPhysicsCollisionSet_ShouldCollide, "ShouldCollide");
	g_Lua->Pop(1);

	IPhysicsEnvironment_TypeID = g_Lua->CreateMetaTable("IPhysicsEnvironment");
		Util::AddFunc(IPhysicsEnvironment__tostring, "__tostring");
		Util::AddFunc(ILuaPhysicsEnvironment__index, "__index");
		Util::AddFunc(ILuaPhysicsEnvironment__newindex, "__newindex");
		Util::AddFunc(ILuaPhysicsEnvironment_GetTable, "GetTable");
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
		Util::AddFunc(physenv_EnablePhysHook, "EnablePhysHook");

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

#if !defined(SYSTEM_WINDOWS) && defined(ARCHITECTURE_X86)
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

	Detour::Create(
		&detour_CPhysicsEnvironment_DestroyObject, "CPhysicsEnvironment::DestroyObject",
		vphysics_loader.GetModule(), Symbols::CPhysicsEnvironment_DestroyObjectSym,
		(void*)hook_CPhysicsEnvironment_DestroyObject, m_pID
	);

	Detour::Create(
		&detour_CPhysicsEnvironment_Restore, "CPhysicsEnvironment::Restore",
		vphysics_loader.GetModule(), Symbols::CPhysicsEnvironment_RestoreSym,
		(void*)hook_CPhysicsEnvironment_Restore, m_pID
	);

	Detour::Create(
		&detour_CPhysicsEnvironment_TransferObject, "CPhysicsEnvironment::TransferObject",
		vphysics_loader.GetModule(), Symbols::CPhysicsEnvironment_TransferObjectSym,
		(void*)hook_CPhysicsEnvironment_TransferObject, m_pID
	);

	Detour::Create(
		&detour_CPhysicsEnvironment_CreateSphereObject, "CPhysicsEnvironment::CreateSphereObject",
		vphysics_loader.GetModule(), Symbols::CPhysicsEnvironment_CreateSphereObjectSym,
		(void*)hook_CPhysicsEnvironment_CreateSphereObject, m_pID
	);

	Detour::Create(
		&detour_CPhysicsEnvironment_UnserializeObjectFromBuffer, "CPhysicsEnvironment::UnserializeObjectFromBuffer",
		vphysics_loader.GetModule(), Symbols::CPhysicsEnvironment_UnserializeObjectFromBufferSym,
		(void*)hook_CPhysicsEnvironment_UnserializeObjectFromBuffer, m_pID
	);

	Detour::Create(
		&detour_CPhysicsEnvironment_CreatePolyObjectStatic, "CPhysicsEnvironment::CreatePolyObjectStatic",
		vphysics_loader.GetModule(), Symbols::CPhysicsEnvironment_CreatePolyObjectStaticSym,
		(void*)hook_CPhysicsEnvironment_CreatePolyObjectStatic, m_pID
	);

	Detour::Create(
		&detour_CPhysicsEnvironment_CreatePolyObject, "CPhysicsEnvironment::CreatePolyObject",
		vphysics_loader.GetModule(), Symbols::CPhysicsEnvironment_CreatePolyObjectSym,
		(void*)hook_CPhysicsEnvironment_CreatePolyObject, m_pID
	);

	Detour::Create(
		&detour_CPhysicsEnvironment_D2, "CPhysicsEnvironment::~CPhysicsEnvironment",
		vphysics_loader.GetModule(), Symbols::CPhysicsEnvironment_D2Sym,
		(void*)hook_CPhysicsEnvironment_D2, m_pID
	);

	Detour::Create(
		&detour_CPhysicsEnvironment_C2, "CPhysicsEnvironment::CPhysicsEnvironment",
		vphysics_loader.GetModule(), Symbols::CPhysicsEnvironment_C2Sym,
		(void*)hook_CPhysicsEnvironment_C2, m_pID
	);
#endif

	SourceSDK::FactoryLoader server_loader("server");
	Detour::Create(
		&detour_GMod_Util_IsPhysicsObjectValid, "GMod::Util::IsPhysicsObjectValid",
		server_loader.GetModule(), Symbols::GMod_Util_IsPhysicsObjectValidSym,
		(void*)hook_GMod_Util_IsPhysicsObjectValid, m_pID
	);

	Detour::Create(
		&detour_PhysFrame, "PhysFrame",
		server_loader.GetModule(), Symbols::PhysFrameSym,
		(void*)hook_PhysFrame, m_pID
	);

	/*Detour::Create(
		&detour_CBaseEntity_GMOD_VPhysicsTest, "CBaseEntity::GMOD_VPhysicsTest",
		server_loader.GetModule(), Symbols::CBaseEntity_GMOD_VPhysicsTestSym,
		(void*)hook_CBaseEntity_GMOD_VPhysicsTest, m_pID
	);*/

#if !defined(SYSTEM_WINDOWS) && defined(ARCHITECTURE_X86)
	g_pCurrentMindist = Detour::ResolveSymbol<IVP_Mindist*>(vphysics_loader, Symbols::g_pCurrentMindistSym);
	Detour::CheckValue("get class", "g_pCurrentMindist", g_pCurrentMindist != NULL);

	g_fDeferDeleteMindist = Detour::ResolveSymbol<bool>(vphysics_loader, Symbols::g_fDeferDeleteMindistSym);
	Detour::CheckValue("get class", "g_fDeferDeleteMindist", g_fDeferDeleteMindist != NULL);
#endif
}