#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/InterfacePointers.hpp>
#include "detours.h"
#include "util.h"
#include "lua.h"
#include <vstdlib/jobthread.h>

class CThreadPoolFixModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn);
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour(bool bPreServer);
	virtual void Think(bool simulating);
	virtual void Shutdown();
	virtual const char* Name() { return "threadpoolfix"; };
};

static CThreadPoolFixModule g_pThreadPoolFixModule;
IModule* pThreadPoolFixModule = &g_pThreadPoolFixModule;

static Detouring::Hook detour_CThreadPool_ExecuteToPriority;
int hook_CThreadPool_ExecuteToPriority(IThreadPool* pool, void* idx, void* idx2)
{
	if (pool->GetJobCount() <= 0)
		return 0;

	return detour_CThreadPool_ExecuteToPriority.GetTrampoline<Symbols::CThreadPool_ExecuteToPriority>()(pool, idx, idx2);
}

void CThreadPoolFixModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
}

void CThreadPoolFixModule::LuaInit(bool bServerInit)
{
}

void CThreadPoolFixModule::LuaShutdown()
{
}

void CThreadPoolFixModule::InitDetour(bool bPreServer)
{
	if ( !bPreServer ) { return; }

	SourceSDK::ModuleLoader libvstdlib_loader("libvstdlib_srv");
	Detour::Create(
		&detour_CThreadPool_ExecuteToPriority, "CThreadPool::ExecuteToPriority",
		libvstdlib_loader.GetModule(), Symbols::CThreadPool_ExecuteToPrioritySym,
		(void*)hook_CThreadPool_ExecuteToPriority, m_pID
	);
}

void CThreadPoolFixModule::Think(bool bSimulating)
{
}

void CThreadPoolFixModule::Shutdown()
{
	Detour::Remove(m_pID);
}