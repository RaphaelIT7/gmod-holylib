#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "util.h"
#include <networkstringtabledefs.h>
#include <vprof.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CPrecacheFixModule : public IModule
{
public:
	void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) override;
	void InitDetour(bool bPreServer) override;
	void LevelShutdown() override;
	const char* Name() override { return "precachefix"; };
	int Compatibility() override { return LINUX32 | LINUX64; };
};

ConVar model_fallback("holylib_precache_modelfallback", "-1", FCVAR_ARCHIVE, "The model index to fallback to if the precache failed");
ConVar generic_fallback("holylib_precache_genericfallback", "-1", FCVAR_ARCHIVE, "The generic index to fallback to if the precache failed");

static CPrecacheFixModule g_pPrecacheFixModule;
IModule* pPrecacheFixModule = &g_pPrecacheFixModule;

static void PR_CheckEmptyString(const char *s)
{
	if (s[0] <= ' ')
		Warning ("Bad string: %s", s); // This should be a Host_Error.
}

// NOTE: CVEngineServer::PrecacheDecal doesn't have this engine error. Why?

static INetworkStringTableContainer* networkStringTableContainerServer = nullptr;
static INetworkStringTable* modelPrecache = nullptr;
static INetworkStringTable* genericPrecache = nullptr;

static Symbols::SV_FindOrAddModel func_SV_FindOrAddModel;
static Detouring::Hook detour_CVEngineServer_PrecacheModel;
static int hook_CVEngineServer_PrecacheModel(IVEngineServer* eengine, const char* mdl, bool preload)
{
	VPROF_BUDGET("HolyLib - CVEngineServer::PrecacheModel", VPROF_BUDGETGROUP_GAME);

	if (!func_SV_FindOrAddModel)
		return detour_CVEngineServer_PrecacheModel.GetTrampoline<Symbols::CVEngineServer_PrecacheModel>()(eengine, mdl, preload);

	PR_CheckEmptyString(mdl);

	bool bNewModel = false;
	if (networkStringTableContainerServer)
	{
		if (!modelPrecache)
		{
			modelPrecache = networkStringTableContainerServer->FindTable("modelprecache");
		}

		if (modelPrecache)
		{
			bNewModel = modelPrecache->FindStringIndex(mdl) == INVALID_STRING_INDEX;
		}
	}

	int idx = func_SV_FindOrAddModel(mdl, preload);
	if (idx >= 0)
	{
		if (bNewModel)
		{
			if (Lua::PushHook("HolyLib:OnModelPrecache"))
			{
				g_Lua->PushString(mdl);
				g_Lua->PushNumber(idx);
				g_Lua->CallFunctionProtected(3, 0, true);
			}
		}

		return idx;
	}

	int iModelFallback = model_fallback.GetInt();
	if (Lua::PushHook("HolyLib:OnModelPrecacheFail"))
	{
		g_Lua->PushString(mdl);
		if (g_Lua->CallFunctionProtected(2, 1, true))
		{
			iModelFallback = (int)g_Lua->CheckNumberOpt(-1, iModelFallback);
			g_Lua->Pop(1);
		}
	}
		
	Warning("CVEngineServer::PrecacheModel: '%s' overflow, too many models\n", mdl);
	return iModelFallback; // The engine already handles -1 cases, so this shouldn't cause other issues, that models that failed to precache being an error model for the client or the entity may not spawn.
}

static Symbols::SV_FindOrAddGeneric func_SV_FindOrAddGeneric;
static Detouring::Hook detour_CVEngineServer_PrecacheGeneric;
static int hook_CVEngineServer_PrecacheGeneric(IVEngineServer* eengine, const char* mdl, bool preload)
{		
	VPROF_BUDGET("HolyLib - CVEngineServer::PrecacheGeneric", VPROF_BUDGETGROUP_GAME);

	if (!func_SV_FindOrAddGeneric)
		return detour_CVEngineServer_PrecacheGeneric.GetTrampoline<Symbols::CVEngineServer_PrecacheGeneric>()(eengine, mdl, preload);

	PR_CheckEmptyString(mdl);
	bool bNewFile = false;
	if (networkStringTableContainerServer)
	{
		if (!genericPrecache)
		{
			genericPrecache = networkStringTableContainerServer->FindTable("genericprecache");
		}
		
		if (genericPrecache)
		{
			bNewFile = genericPrecache->FindStringIndex(mdl) == INVALID_STRING_INDEX;
		}
	}

	int idx = func_SV_FindOrAddGeneric(mdl, preload);
	if (idx >= 0)
	{
		if (bNewFile)
		{
			if (Lua::PushHook("HolyLib:OnGenericPrecache"))
			{
				g_Lua->PushString(mdl);
				g_Lua->PushNumber(idx);
				g_Lua->CallFunctionProtected(3, 0, true);
			}
		}

		return idx;
	}

	int iGenericFallback = generic_fallback.GetInt();
	if (Lua::PushHook("HolyLib:OnGenericPrecacheFail"))
	{
		g_Lua->PushString(mdl);
		if (g_Lua->CallFunctionProtected(2, 1, true))
		{
			iGenericFallback = (int)g_Lua->CheckNumberOpt(-1, iGenericFallback);
			g_Lua->Pop(1);
		}
	}
		
	Warning("CVEngineServer::PrecacheGeneric: '%s' overflow\n", mdl);
	return iGenericFallback; // ToDo: Find out, what happens when we hit the limit and verify that everything that calls this, handles a case like 0 properly.
}

void CPrecacheFixModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	networkStringTableContainerServer = (INetworkStringTableContainer*)appfn[0](INTERFACENAME_NETWORKSTRINGTABLESERVER, nullptr);
	Detour::CheckValue("get interface", "INetworkStringTableContainer", networkStringTableContainerServer != nullptr);
}

#if SYSTEM_WINDOWS
DETOUR_THISCALL_START()
	DETOUR_THISCALL_ADDRETFUNC2( hook_CVEngineServer_PrecacheModel, int, PrecacheModel, IVEngineServer*, const char*, bool );
	DETOUR_THISCALL_ADDRETFUNC2( hook_CVEngineServer_PrecacheGeneric, int, PrecacheGeneric, IVEngineServer*, const char*, bool );
DETOUR_THISCALL_FINISH();
#endif

void CPrecacheFixModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	DETOUR_PREPARE_THISCALL();
	SourceSDK::ModuleLoader engine_loader("engine");
	Detour::Create(
		&detour_CVEngineServer_PrecacheModel, "CVEngineServer::PrecacheModel",
		engine_loader.GetModule(), Symbols::CVEngineServer_PrecacheModelSym,
		(void*)DETOUR_THISCALL(hook_CVEngineServer_PrecacheModel, PrecacheModel), m_pID
	);

	Detour::Create(
		&detour_CVEngineServer_PrecacheGeneric, "CVEngineServer::PrecacheGeneric",
		engine_loader.GetModule(), Symbols::CVEngineServer_PrecacheGenericSym,
		(void*)DETOUR_THISCALL(hook_CVEngineServer_PrecacheGeneric, PrecacheGeneric), m_pID
	);

	func_SV_FindOrAddModel = (Symbols::SV_FindOrAddModel)Detour::GetFunction(engine_loader.GetModule(), Symbols::SV_FindOrAddModelSym);
	func_SV_FindOrAddGeneric = (Symbols::SV_FindOrAddGeneric)Detour::GetFunction(engine_loader.GetModule(), Symbols::SV_FindOrAddGenericSym);
}

void CPrecacheFixModule::LevelShutdown()
{
	modelPrecache = nullptr;
	genericPrecache = nullptr;
}