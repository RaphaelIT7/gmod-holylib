#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"

#define DEDICATED
#include "vstdlib/jobthread.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CThreadPoolFixModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "threadpoolfix"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; }; 
};

static CThreadPoolFixModule g_pThreadPoolFixModule;
IModule* pThreadPoolFixModule = &g_pThreadPoolFixModule;

void CThreadPoolFixModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{

}

void CThreadPoolFixModule::InitDetour(bool bPreServer)
{
	// Currently we do nothing as all fixes were implemented into gmod.
}