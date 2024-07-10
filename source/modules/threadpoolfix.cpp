#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/InterfacePointers.hpp>
#include "detours.h"
#include "util.h"
#include "lua.h"
#include <vstdlib/jobthread.h>

class CHolyLibModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* fn);
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour();
	virtual void Think(bool simulating);
	virtual void Shutdown();
	virtual const char* Name() { return "threadpoolfix"; };
	virtual void LoadConfig(KeyValues* config) {};
};

CHolyLibModule g_pThreadpoolFixModule;
IModule* pThreadpoolFixModule = &g_pThreadpoolFixModule;

Detouring::Hook detour_CThreadPool_ExecuteToPriority;
int hook_CThreadPool_ExecuteToPriority(IThreadPool* pool, void* idx, void* idx2)
{
	if (pool->GetJobCount() <= 0)
		return 0;

	return detour_CThreadPool_ExecuteToPriority.GetTrampoline<Symbols::CThreadPool_ExecuteToPriority>()(pool, idx, idx2);
}

void CHolyLibModule::Init(CreateInterfaceFn* fn)
{
}

void CHolyLibModule::LuaInit(bool bServerInit)
{
}

void CHolyLibModule::LuaShutdown()
{
}

void CHolyLibModule::InitDetour()
{
	SourceSDK::ModuleLoader libvstdlib_loader("libvstdlib_srv");
	Detour::Create(
		&detour_CThreadPool_ExecuteToPriority, "CThreadPool::ExecuteToPriority",
		libvstdlib_loader.GetModule(), Symbols::CThreadPool_ExecuteToPrioritySym,
		(void*)hook_CThreadPool_ExecuteToPriority, m_pID
	);
}

void CHolyLibModule::Think(bool bSimulating)
{
}

void CHolyLibModule::Shutdown()
{
	Detour::Remove(m_pID);
}