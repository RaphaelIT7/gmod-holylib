#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/InterfacePointers.hpp>
#include "detours.h"
#include "util.h"
#include "lua.h"
#include <vstdlib/jobthread.h>

class CPrecacheFixModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn);
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour(bool bPreServer);
	virtual void Think(bool simulating);
	virtual void Shutdown();
	virtual const char* Name() { return "precachefix"; };
};

CPrecacheFixModule g_pPrecacheFixModule;
IModule* pPrecacheFixModule = &g_pPrecacheFixModule;

static void PR_CheckEmptyString( const char *s )
{
	if ( s[0] <= ' ' )
		Warning ( "Bad string: %s", s ); // This should be a Host_Error.
}

// NOTE: CVEngineServer::PrecacheDecal doesn't have this engine error. Why?

Symbols::SV_FindOrAddModel func_SV_FindOrAddModel;
Detouring::Hook detour_CVEngineServer_PrecacheModel;
int hook_CVEngineServer_PrecacheModel(IVEngineServer* eengine, const char* mdl, bool preload)
{
	PR_CheckEmptyString( mdl );
	int i = func_SV_FindOrAddModel( mdl, preload );
	if ( i >= 0 )
	{
		return i;
	}

	if (Lua::PushHook("HolyLib:OnModelPrecacheFail"))
	{
		g_Lua->PushString(mdl);
		g_Lua->CallFunctionProtected(2, 0, true);
	}
		
	Warning( "CVEngineServer::PrecacheModel: '%s' overflow, too many models\n", mdl );
	return 0; // The engine already handles 0 cases, so this shouldn't cause other issues, that models that failed to precache being an error model for the client or the entity may not spawn.
}

Symbols::SV_FindOrAddGeneric func_SV_FindOrAddGeneric;
Detouring::Hook detour_CVEngineServer_PrecacheGeneric;
int hook_CVEngineServer_PrecacheGeneric(IVEngineServer* eengine, const char* mdl, bool preload)
{		
	PR_CheckEmptyString( mdl );
	int i = func_SV_FindOrAddGeneric( mdl, preload );
	if ( i >= 0 )
	{
		return i;
	}

	if (Lua::PushHook("HolyLib:OnGenericPrecacheFail"))
	{
		g_Lua->PushString(mdl);
		g_Lua->CallFunctionProtected(2, 0, true);
	}
		
	Warning( "CVEngineServer::PrecacheGeneric: '%s' overflow\n", mdl );
	return 0; // ToDo: Find out, what happens when we hit the limit and verify that everything that calls this, handles a case like 0 properly.
}

void CPrecacheFixModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
}

void CPrecacheFixModule::LuaInit(bool bServerInit)
{
}

void CPrecacheFixModule::LuaShutdown()
{
}

void CPrecacheFixModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }

	SourceSDK::ModuleLoader engine_loader("engine_srv");
	Detour::Create(
		&detour_CVEngineServer_PrecacheModel, "CVEngineServer::PrecacheModel",
		engine_loader.GetModule(), Symbols::CVEngineServer_PrecacheModelSym,
		(void*)hook_CVEngineServer_PrecacheModel, m_pID
	);

	Detour::Create(
		&detour_CVEngineServer_PrecacheGeneric, "CVEngineServer::PrecacheGeneric",
		engine_loader.GetModule(), Symbols::CVEngineServer_PrecacheGenericSym,
		(void*)hook_CVEngineServer_PrecacheGeneric, m_pID
	);

	func_SV_FindOrAddModel = (Symbols::SV_FindOrAddModel)Detour::GetFunction(engine_loader.GetModule(), Symbols::SV_FindOrAddModelSym);
	func_SV_FindOrAddGeneric = (Symbols::SV_FindOrAddGeneric)Detour::GetFunction(engine_loader.GetModule(), Symbols::SV_FindOrAddGenericSym);
}

void CPrecacheFixModule::Think(bool bSimulating)
{
}

void CPrecacheFixModule::Shutdown()
{
	Detour::Remove(m_pID);
}