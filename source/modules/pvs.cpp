#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "unordered_map"
#include "player.h"
#include "iserver.h"
#include "sourcesdk/baseclient.h"
#include "vprof.h"

class CPVSModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "pvs"; };
	virtual int Compatibility() { return LINUX32; };
};

static CPVSModule g_pPVSModule;
IModule* pPVSModule = &g_pPVSModule;

static int currentPVSSize = -1;
static unsigned char* currentPVS = NULL;
static int mapPVSSize = -1;
#ifndef HOLYLIB_MANUALNETWORKING
static Detouring::Hook detour_CGMOD_Player_SetupVisibility;
static void hook_CGMOD_Player_SetupVisibility(void* ent, unsigned char* pvs, int pvssize)
{
	currentPVS = pvs;
	currentPVSSize = pvssize;

	detour_CGMOD_Player_SetupVisibility.GetTrampoline<Symbols::CGMOD_Player_SetupVisibility>()(ent, pvs, pvssize);

	currentPVS = NULL;
	currentPVSSize = -1;
}
#endif

#ifdef HOLYLIB_MANUALNETWORKING
static std::unordered_map<edict_t*, int> pOriginalFlags;
#endif
static std::vector<edict_t*> g_pAddEntityToPVS;
static std::unordered_map<edict_t*, int> g_pOverrideStateFlag;

static CCheckTransmitInfo* g_pCurrentTransmitInfo = NULL;
static const unsigned short *g_pCurrentEdictIndices = NULL;
static int g_nCurrentEdicts = -1;

static Detouring::Hook detour_CServerGameEnts_CheckTransmit;
#ifndef HOLYLIB_MANUALNETWORKING
static void hook_CServerGameEnts_CheckTransmit(void* gameents, CCheckTransmitInfo *pInfo, const unsigned short *pEdictIndices, int nEdicts)
{
	VPROF_BUDGET("HolyLib - CServerGameEnts::CheckTransmit", VPROF_BUDGETGROUP_OTHER_NETWORKING);
	g_pCurrentTransmitInfo = pInfo;
	g_pCurrentEdictIndices = pEdictIndices;
	g_nCurrentEdicts = nEdicts;

	if(Lua::PushHook("HolyLib:PreCheckTransmit"))
	{
		Util::Push_Entity(Util::servergameents->EdictToBaseEntity(pInfo->m_pClientEnt));
		if (g_Lua->CallFunctionProtected(2, 1, true))
		{
			bool bCancel = g_Lua->GetBool(-1);
			g_Lua->Pop(1);

			if (bCancel)
			{
				g_pAddEntityToPVS.clear();
				g_pOverrideStateFlag.clear();

				g_pCurrentTransmitInfo = NULL;
				g_pCurrentEdictIndices = NULL;
				g_nCurrentEdicts = -1;
				return;
			}
		}
	}

	for (edict_t* ent : g_pAddEntityToPVS)
		Util::servergameents->EdictToBaseEntity(ent)->SetTransmit(pInfo, true);
	
	static std::unordered_map<edict_t*, int> pOriginalFlags;
	for (auto&[ent, flag] : g_pOverrideStateFlag)
	{
		pOriginalFlags[ent] = ent->m_fStateFlags;
		if (g_pPVSModule.InDebug())
			Msg("Overriding ent(%i) flags for snapshot (%i -> %i)\n", ent->m_EdictIndex, ent->m_fStateFlags, flag);
		
		ent->m_fStateFlags = flag;
	}

	detour_CServerGameEnts_CheckTransmit.GetTrampoline<Symbols::CServerGameEnts_CheckTransmit>()(gameents, pInfo, pEdictIndices, nEdicts);

	if(Lua::PushHook("HolyLib:PostCheckTransmit"))
	{
		Util::Push_Entity(Util::servergameents->EdictToBaseEntity(pInfo->m_pClientEnt));
		g_Lua->CallFunctionProtected(2, 0, true);
	}

	for (auto&[ent, flag] : pOriginalFlags)
		ent->m_fStateFlags = flag;

	pOriginalFlags.clear();
	g_pAddEntityToPVS.clear();
	g_pOverrideStateFlag.clear();

	g_pCurrentTransmitInfo = NULL;
	g_pCurrentEdictIndices = NULL;
	g_nCurrentEdicts = -1;
}
#else
void PreSetupVisibility(unsigned char* pvs, int pvssize)
{
	currentPVS = pvs;
	currentPVSSize = pvssize;
}

void PostSetupVisibility()
{
	currentPVS = NULL;
	currentPVSSize = -1;
}

void PreCheckTransmit(void* gameents, CCheckTransmitInfo *pInfo, const unsigned short *pEdictIndices, int nEdicts)
{
	VPROF_BUDGET("HolyLib - CServerGameEnts::(Pre)CheckTransmit", VPROF_BUDGETGROUP_OTHER_NETWORKING);

	g_pCurrentTransmitInfo = pInfo;
	g_pCurrentEdictIndices = pEdictIndices;
	g_nCurrentEdicts = nEdicts;

	if(Lua::PushHook("HolyLib:PreCheckTransmit"))
	{
		Util::Push_Entity(Util::servergameents->EdictToBaseEntity(pInfo->m_pClientEnt));
		if (g_Lua->CallFunctionProtected(2, 1, true))
		{
			bool bCancel = g_Lua->GetBool(-1);
			g_Lua->Pop(1);

			if (bCancel)
			{
				g_pAddEntityToPVS.clear();
				g_pOverrideStateFlag.clear();

				g_pCurrentTransmitInfo = NULL;
				g_pCurrentEdictIndices = NULL;
				g_nCurrentEdicts = -1;
				return;
			}
		}
	}

	for (edict_t* ent : g_pAddEntityToPVS)
	{
		Util::servergameents->EdictToBaseEntity(ent)->SetTransmit(pInfo, true);
	}
	
	static std::unordered_map<edict_t*, int> pOriginalFlags;
	for (auto&[ent, flag] : g_pOverrideStateFlag)
	{
		pOriginalFlags[ent] = ent->m_fStateFlags;
		Msg("Overriding ent(%i) flags for snapshot (%i -> %i)\n", ent->m_EdictIndex, ent->m_fStateFlags, flag);
		ent->m_fStateFlags = flag;
	}

	g_pCurrentTransmitInfo = NULL;
	g_pCurrentEdictIndices = NULL;
	g_nCurrentEdicts = -1;
}

void PostCheckTransmit(void* gameents, CCheckTransmitInfo *pInfo, const unsigned short *pEdictIndices, int nEdicts)
{
	VPROF_BUDGET("HolyLib - CServerGameEnts::(Post)CheckTransmit", VPROF_BUDGETGROUP_OTHER_NETWORKING);

	g_pCurrentTransmitInfo = pInfo;
	g_pCurrentEdictIndices = pEdictIndices;
	g_nCurrentEdicts = nEdicts;

	if(Lua::PushHook("HolyLib:PostCheckTransmit"))
	{
		Util::Push_Entity(Util::servergameents->EdictToBaseEntity(pInfo->m_pClientEnt));
		g_Lua->CallFunctionProtected(2, 0, true);
	}

	for (auto&[ent, flag] : pOriginalFlags)
	{
		ent->m_fStateFlags = flag;
	}
	pOriginalFlags.clear();
	g_pAddEntityToPVS.clear();
	g_pOverrideStateFlag.clear();

	g_pCurrentTransmitInfo = NULL;
	g_pCurrentEdictIndices = NULL;
	g_nCurrentEdicts - 1;
}
#endif

LUA_FUNCTION_STATIC(pvs_ResetPVS)
{
	if (!currentPVS)
		LUA->ThrowError("pvs: tried to call pvs.ResetPVS with no active PVS!");

	Util::engineserver->ResetPVS(currentPVS, currentPVSSize);

	return 0;
}

LUA_FUNCTION_STATIC(pvs_CheckOriginInPVS)
{
	Vector* vec = Get_Vector(1);

	if (!currentPVS)
		LUA->ThrowError("pvs: tried to call pvs.CheckOriginInPVS with no active PVS!");

	LUA->PushBool(Util::engineserver->CheckOriginInPVS(*vec, currentPVS, currentPVSSize));
	return 1;
}

LUA_FUNCTION_STATIC(pvs_AddOriginToPVS)
{
	Vector* vec = Get_Vector(1);

	if (!currentPVS)
		LUA->ThrowError("pvs: tried to call pvs.AddOriginToPVS with no active PVS!");

	Util::engineserver->AddOriginToPVS(*vec);

	return 0;
}

LUA_FUNCTION_STATIC(pvs_GetClusterCount)
{
	LUA->PushNumber(Util::engineserver->GetClusterCount());
	return 1;
}

LUA_FUNCTION_STATIC(pvs_GetClusterForOrigin)
{
	Vector* vec = Get_Vector(1);

	LUA->PushNumber(Util::engineserver->GetClusterForOrigin(*vec));
	return 1;
}

LUA_FUNCTION_STATIC(pvs_CheckAreasConnected)
{
	int area1 = LUA->CheckNumber(1);
	int area2 = LUA->CheckNumber(2);

	LUA->PushBool(Util::engineserver->CheckAreasConnected(area1, area2));
	return 1;
}

LUA_FUNCTION_STATIC(pvs_GetArea)
{
	Vector* vec = Get_Vector(1);

	LUA->PushNumber(Util::engineserver->GetArea(*vec));
	return 1;
}

LUA_FUNCTION_STATIC(pvs_GetPVSForCluster)
{
	int cluster = LUA->CheckNumber(1);

	if (!currentPVS)
		LUA->ThrowError("pvs: tried to call pvs.GetPVSForCluster with no active PVS!");

	Util::engineserver->ResetPVS(currentPVS, currentPVSSize);
	Util::engineserver->GetPVSForCluster(cluster, currentPVSSize, currentPVS);

	return 0;
}

LUA_FUNCTION_STATIC(pvs_CheckBoxInPVS)
{
	Vector* vec1 = Get_Vector(1);
	Vector* vec2 = Get_Vector(2);

	LUA->PushBool(engine->CheckBoxInPVS(*vec1, *vec2, currentPVS, currentPVSSize));
	return 1;
}

static void AddEntityToPVS(CBaseEntity* ent)
{
	edict_t* edict = ent->edict();
	if (edict)
		g_pAddEntityToPVS.push_back(edict);
	else
		g_Lua->ThrowError("Failed to get edict?");
}

LUA_FUNCTION_STATIC(pvs_AddEntityToPVS)
{
	if (LUA->IsType(1, GarrysMod::Lua::Type::Table))
	{
		LUA->Push(1);
		LUA->PushNil();
		while (LUA->Next(-2))
		{
			CBaseEntity* ent = Util::Get_Entity(-1, true);
			AddEntityToPVS(ent);

			LUA->Pop(1);
		}
		LUA->Pop(1);
	} else if (Is_EntityList(1)) {
		EntityList* entList = Get_EntityList(1, true);
		for (CBaseEntity* ent : entList->GetEntities())
			AddEntityToPVS(ent);
	} else {
		CBaseEntity* ent = Util::Get_Entity(1, true);
		AddEntityToPVS(ent);
	}

	return 0;
}

#define LUA_FL_EDICT_DONTSEND 1 << 0
#define LUA_FL_EDICT_ALWAYS 1 << 1
#define LUA_FL_EDICT_PVSCHECK 1 << 2
#define LUA_FL_EDICT_FULLCHECK 1 << 3
static void SetOverrideStateFlags(CBaseEntity* ent, int flags, bool force)
{
	edict_t* edict = ent->edict();
	if (!edict)
		g_Lua->ThrowError("Failed to get edict?");

	int newFlags = flags;
	if (!force)
	{
		newFlags = edict->m_fStateFlags;
		newFlags = newFlags & ~FL_EDICT_DONTSEND;
		newFlags = newFlags & ~FL_EDICT_ALWAYS;
		newFlags = newFlags & ~FL_EDICT_PVSCHECK;
		newFlags = newFlags & ~FL_EDICT_FULLCHECK;

		if (flags & LUA_FL_EDICT_DONTSEND)
			newFlags |= FL_EDICT_DONTSEND;

		if (flags & LUA_FL_EDICT_ALWAYS)
			newFlags |= FL_EDICT_ALWAYS;

		if (flags & LUA_FL_EDICT_PVSCHECK)
			newFlags |= FL_EDICT_PVSCHECK;

		if (flags & LUA_FL_EDICT_FULLCHECK)
			newFlags |= FL_EDICT_FULLCHECK;
	}

	g_pOverrideStateFlag[edict] = newFlags;
}

LUA_FUNCTION_STATIC(pvs_OverrideStateFlags)
{
	int flags = LUA->CheckNumber(2);
	bool force = LUA->GetBool(3);

	if (LUA->IsType(1, GarrysMod::Lua::Type::Table))
	{
		LUA->Push(1);
		LUA->PushNil();
		while (LUA->Next(-2))
		{
			CBaseEntity* ent = Util::Get_Entity(-1, true);
			SetOverrideStateFlags(ent, flags, force);

			LUA->Pop(1);
		}
		LUA->Pop(1);
	} else if (Is_EntityList(1)) {
		EntityList* entList = Get_EntityList(1, true);
		for (CBaseEntity* ent : entList->GetEntities())
			SetOverrideStateFlags(ent, flags, force);
	} else {
		CBaseEntity* ent = Util::Get_Entity(1, true);
		SetOverrideStateFlags(ent, flags, force);
	}

	return 0;
}

static void SetStateFlags(CBaseEntity* ent, int flags, bool force)
{
	edict_t* edict = ent->edict();
	if (!edict)
		g_Lua->ThrowError("Failed to get edict?");

	int newFlags = flags;
	if (!force)
	{
		newFlags = edict->m_fStateFlags;
		newFlags = newFlags & ~FL_EDICT_DONTSEND;
		newFlags = newFlags & ~FL_EDICT_ALWAYS;
		newFlags = newFlags & ~FL_EDICT_PVSCHECK;
		newFlags = newFlags & ~FL_EDICT_FULLCHECK;

		if (flags & LUA_FL_EDICT_DONTSEND)
			newFlags |= FL_EDICT_DONTSEND;

		if (flags & LUA_FL_EDICT_ALWAYS)
			newFlags |= FL_EDICT_ALWAYS;

		if (flags & LUA_FL_EDICT_PVSCHECK)
			newFlags |= FL_EDICT_PVSCHECK;

		if (flags & LUA_FL_EDICT_FULLCHECK)
			newFlags |= FL_EDICT_FULLCHECK;
	}

	edict->m_fStateFlags = newFlags;
}

LUA_FUNCTION_STATIC(pvs_SetStateFlags)
{
	int flags = LUA->CheckNumber(2);
	bool force = LUA->GetBool(3);

	if (LUA->IsType(1, GarrysMod::Lua::Type::Table))
	{
		LUA->Push(1);
		LUA->PushNil();
		while (LUA->Next(-2))
		{
			CBaseEntity* ent = Util::Get_Entity(-1, true);
			SetStateFlags(ent, flags, force);

			LUA->Pop(1);
		}
		LUA->Pop(1);
	} else if (Is_EntityList(1)) {
		EntityList* entList = Get_EntityList(1, true);
		for (CBaseEntity* ent : entList->GetEntities())
			SetStateFlags(ent, flags, force);
	} else {
		CBaseEntity* ent = Util::Get_Entity(1, true);
		SetStateFlags(ent, flags, force);
	}

	return 0;
}

static int GetStateFlags(CBaseEntity* ent, bool force)
{
	edict_t* edict = ent->edict();
	if (!edict)
		g_Lua->ThrowError("Failed to get edict?");

	int flags = edict->m_fStateFlags;
	int newFlags = flags;
	if (!force)
	{
		newFlags = 0;
		if (flags & FL_EDICT_DONTSEND)
			newFlags |= LUA_FL_EDICT_DONTSEND;

		if (flags & FL_EDICT_ALWAYS)
			newFlags |= LUA_FL_EDICT_ALWAYS;

		if (flags & FL_EDICT_PVSCHECK)
			newFlags |= LUA_FL_EDICT_PVSCHECK;

		if (flags & FL_EDICT_FULLCHECK)
			newFlags |= LUA_FL_EDICT_FULLCHECK;
	}

	return newFlags;
}

LUA_FUNCTION_STATIC(pvs_GetStateFlags)
{
	bool force = LUA->GetBool(2);
	if (LUA->IsType(1, GarrysMod::Lua::Type::Table))
	{
		LUA->CreateTable();
		LUA->Push(1);
		LUA->PushNil();
		while (LUA->Next(-2))
		{
			CBaseEntity* ent = Util::Get_Entity(-1, true);
			LUA->PushNumber(GetStateFlags(ent, force));
			LUA->RawSet(-4);
		}
		LUA->Pop(1);
	} else if (Is_EntityList(1)) {
		LUA->CreateTable();
		EntityList* entList = Get_EntityList(1, true);
		for (auto& [pEnt, iReference] : entList->GetReferences())
		{
			if (!entList->IsValidReference(iReference))
				entList->CreateReference(pEnt);

			Util::ReferencePush(iReference);
			LUA->PushNumber(GetStateFlags(pEnt, force));
			LUA->RawSet(-3);
		}
	} else {
		CBaseEntity* ent = Util::Get_Entity(1, true);
		LUA->PushNumber(GetStateFlags(ent, force));
	}

	return 1;
}

static bool RemoveEntityFromTransmit(CBaseEntity* ent)
{
	edict_t* edict = ent->edict();
	if (!edict)
		g_Lua->ThrowError("Failed to get edict?");

	if (!g_pCurrentTransmitInfo)
		g_Lua->ThrowError("Tried to use pvs.RemoveEntityFromTransmit while not in a CheckTransmit call!");

	if (!g_pCurrentTransmitInfo->m_pTransmitEdict->Get(edict->m_EdictIndex))
		return false;

	g_pCurrentTransmitInfo->m_pTransmitEdict->Clear(edict->m_EdictIndex);
	if (g_pCurrentTransmitInfo->m_pTransmitAlways && g_pCurrentTransmitInfo->m_pTransmitAlways->Get(edict->m_EdictIndex))
		g_pCurrentTransmitInfo->m_pTransmitAlways->Clear(edict->m_EdictIndex);

	return true;
}

LUA_FUNCTION_STATIC(pvs_RemoveEntityFromTransmit)
{
	if (LUA->IsType(1, GarrysMod::Lua::Type::Table))
	{
		LUA->Push(1);
		LUA->PushNil();
		while (LUA->Next(-2))
		{
			CBaseEntity* ent = Util::Get_Entity(-1, true);
			RemoveEntityFromTransmit(ent);

			LUA->Pop(1);
		}
		LUA->Pop(1);

		LUA->PushBool(true);
	} else if (Is_EntityList(1)) {
		EntityList* entList = Get_EntityList(1, true);
		for (CBaseEntity* ent : entList->GetEntities())
			RemoveEntityFromTransmit(ent);

		LUA->PushBool(true);
	} else {
		CBaseEntity* ent = Util::Get_Entity(1, true);
		LUA->PushBool(RemoveEntityFromTransmit(ent));
	}

	return 1;
}

LUA_FUNCTION_STATIC(pvs_RemoveAllEntityFromTransmit)
{
	if (!g_pCurrentTransmitInfo)
		LUA->ThrowError("Tried to use pvs.RemoveEntityFromTransmit while not in a CheckTransmit call!");

	g_pCurrentTransmitInfo->m_pTransmitEdict->ClearAll();
	if (g_pCurrentTransmitInfo->m_pTransmitAlways)
		g_pCurrentTransmitInfo->m_pTransmitAlways->ClearAll();

	return 0;
}

static void AddEntityToTransmit(CBaseEntity* ent, bool force)
{
	if (!g_pCurrentTransmitInfo)
		g_Lua->ThrowError("Tried to use pvs.RemoveEntityFromTransmit while not in a CheckTransmit call!");

	ent->SetTransmit(g_pCurrentTransmitInfo, force);
}

LUA_FUNCTION_STATIC(pvs_AddEntityToTransmit)
{
	bool force = LUA->GetBool(2);
	if (LUA->IsType(1, GarrysMod::Lua::Type::Table))
	{
		LUA->Push(1);
		LUA->PushNil();
		while (LUA->Next(-2))
		{
			CBaseEntity* ent = Util::Get_Entity(-1, true);
			AddEntityToTransmit(ent, force);

			LUA->Pop(1);
		}
		LUA->Pop(1);
	} else if (Is_EntityList(1)) {
		EntityList* entList = Get_EntityList(1, true);
		for (CBaseEntity* ent : entList->GetEntities())
			AddEntityToTransmit(ent, true);
	} else {
		CBaseEntity* ent = Util::Get_Entity(1, true);
		AddEntityToTransmit(ent, force);
	}
	
	return 0;
}

CBasePlayer *UTIL_PlayerByIndex(int playerIndex)
{
	CBasePlayer *pPlayer = NULL;

	if (playerIndex > 0 && playerIndex <= gpGlobals->maxClients)
	{
		edict_t *pPlayerEdict = INDEXENT(playerIndex);
		if (pPlayerEdict && !pPlayerEdict->IsFree())
			pPlayer = (CBasePlayer*)Util::GetCBaseEntityFromEdict(pPlayerEdict);
	}
	
	return pPlayer;
}

LUA_FUNCTION_STATIC(pvs_SetPreventTransmitBulk)
{
	CBasePlayer* ply = NULL;
	std::vector<CBasePlayer*> filterplys;
	if (LUA->IsType(2, GarrysMod::Lua::Type::RecipientFilter))
	{
		CRecipientFilter* filter = (CRecipientFilter*)Get_IRecipientFilter(2, true);
		for (int i=0; i<gpGlobals->maxClients; ++i)
			if (filter->GetRecipientIndex(i) != -1)
				filterplys.push_back(UTIL_PlayerByIndex(i));
	}
	else if (LUA->IsType(2, GarrysMod::Lua::Type::Table))
	{
		LUA->Push(2);
		LUA->PushNil();
		while (LUA->Next(-2))
		{
			CBasePlayer* pply = Util::Get_Player(-1, true);
			filterplys.push_back(pply);

			LUA->Pop(1);
		}
		LUA->Pop(1);
	}
	else
		ply = Util::Get_Player(2, true);

	bool notransmit = LUA->GetBool(3);
	if (LUA->IsType(1, GarrysMod::Lua::Type::Table))
	{
		LUA->Push(1);
		LUA->PushNil();
		while (LUA->Next(-2))
		{
			CBaseEntity* ent = Util::Get_Entity(-1, true);
			if (filterplys.size() > 0)
			{
				for (CBasePlayer* pply : filterplys)
				{
					ent->GMOD_SetShouldPreventTransmitToPlayer(pply, notransmit);
				}
			} else {
				ent->GMOD_SetShouldPreventTransmitToPlayer(ply, notransmit);
			}

			LUA->Pop(1);
		}
		LUA->Pop(1);
	} else if (Is_EntityList(1)) {
		EntityList* entList = Get_EntityList(1, true);
		for (CBaseEntity* ent : entList->GetEntities())
		{
			if (filterplys.size() > 0)
				for (CBasePlayer* pply : filterplys)
					ent->GMOD_SetShouldPreventTransmitToPlayer(pply, notransmit);
			else
				ent->GMOD_SetShouldPreventTransmitToPlayer(ply, notransmit);
		}
	} else {
		CBaseEntity* ent = Util::Get_Entity(1, true);
		ent->GMOD_SetShouldPreventTransmitToPlayer(ply, notransmit);
	}
	
	return 0;
}

LUA_FUNCTION_STATIC(pvs_FindInPVS) // Copy from pas.FindInPAS
{
	VPROF_BUDGET("pvs.FindInPVS", VPROF_BUDGETGROUP_HOLYLIB);

	Vector* orig;
	if (LUA->IsType(1, GarrysMod::Lua::Type::Vector))
	{
		orig = Get_Vector(1);
	} else {
		CBaseEntity* ent = Util::Get_Entity(1, true);
		orig = (Vector*)&ent->GetAbsOrigin(); // ToDo: This currently breaks the compile.
	}

	Util::CM_Vis(*orig, DVIS_PVS);

	LUA->PreCreateTable(MAX_EDICTS / 16, 0); // Should we reduce this later? (Currently: 512)
	int idx = 0;
	if (Util::pEntityList->IsEnabled())
	{
		for (auto& [pEnt, iReference] : g_pGlobalEntityList.GetReferences())
		{
			if (Util::engineserver->CheckOriginInPVS(pEnt->GetAbsOrigin(), Util::g_pCurrentCluster, sizeof(Util::g_pCurrentCluster)))
			{
				if (!g_pGlobalEntityList.IsValidReference(iReference))
					g_pGlobalEntityList.CreateReference(pEnt);

				Util::ReferencePush(iReference);
				Util::RawSetI(-2, ++idx);
			}
		}
		return 1;
	}

#if ARCHITECTURE_IS_X86
	CBaseEntity* pEnt = Util::entitylist->FirstEnt();
	while (pEnt != NULL)
	{
		if (Util::engineserver->CheckOriginInPVS(pEnt->GetAbsOrigin(), Util::g_pCurrentCluster, sizeof(Util::g_pCurrentCluster)))
		{
			Util::Push_Entity(pEnt);
			Util::RawSetI(-2, ++idx);
		}

		pEnt = Util::entitylist->NextEnt(pEnt);
	}
#else
	edict_t* pWorldEdict = Util::engineserver->PEntityOfEntIndex(0);
	for(int i=0; i<MAX_EDICTS; ++i)
	{
		edict_t* pEdict = &pWorldEdict[i]; // Let's hope this works.... It's used in CServerGameEnts::CheckTransmit as an optimization.
		CBaseEntity* pEnt = Util::GetCBaseEntityFromEdict(pEdict);
		if (!pEnt)
			continue;

		if (Util::engineserver->CheckOriginInPVS(pEnt->GetAbsOrigin(), Util::g_pCurrentCluster, sizeof(Util::g_pCurrentCluster)))
		{
			Util::Push_Entity(pEnt);
			Util::RawSetI(-2, ++idx);
		}
	}
#endif

	return 1;
}

inline bool TestPVS(const Vector& hearPos)
{
	return Util::engineserver->CheckOriginInPVS(hearPos, Util::g_pCurrentCluster, sizeof(Util::g_pCurrentCluster));
}

LUA_FUNCTION_STATIC(pvs_TestPVS)
{
	Vector* orig;
	if (LUA->IsType(1, GarrysMod::Lua::Type::Vector))
	{
		orig = Get_Vector(1);
	} else {
		CBaseEntity* ent = Util::Get_Entity(1, true);
		orig = (Vector*)&ent->GetAbsOrigin(); // ToDo: This currently breaks the compile.
	}

	Util::CM_Vis(*orig, DVIS_PVS);

	LUA->CheckType(2, GarrysMod::Lua::Type::Vector);
	if (LUA->IsType(2, GarrysMod::Lua::Type::Vector))
	{
		LUA->PushBool(TestPVS(*Get_Vector(2)));
	} else if (Is_EntityList(2)) {
		EntityList* entList = Get_EntityList(2, true);
		LUA->PreCreateTable(0, entList->GetEntities().size());
		for (auto& [pEnt, iReference] : entList->GetReferences())
		{
			if (!entList->IsValidReference(iReference))
				entList->CreateReference(pEnt);

			Util::ReferencePush(iReference);
			LUA->PushBool(TestPVS(pEnt->GetAbsOrigin()));
			LUA->RawSet(-3);
		}
	} else {
		LUA->CheckType(2, GarrysMod::Lua::Type::Entity);
		CBaseEntity* ent = Util::Get_Entity(2, false);

		LUA->PushBool(TestPVS(ent->GetAbsOrigin()));
	}

	return 1;
}

LUA_FUNCTION_STATIC(pvs_ForceFullUpdate)
{
	CBasePlayer* ply = Util::Get_Player(1, true);
	CBaseClient* pClient = Util::GetClientByPlayer(ply);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!");

	pClient->ForceFullUpdate();
	return 0;
}

LUA_FUNCTION_STATIC(pvs_GetEntitiesFromTransmit)
{
	if (!g_pCurrentTransmitInfo)
		LUA->ThrowError("Tried to use pvs.GetEntitiesFromTransmit while not in a CheckTransmit call!");

	g_Lua->PreCreateTable(g_nCurrentEdicts, 0);
	int idx = 0;
	edict_t *pBaseEdict = Util::engineserver->PEntityOfEntIndex(0);
	for (int i=0; i<g_nCurrentEdicts; ++i)
	{
		int iEdict = g_pCurrentEdictIndices[i];
		edict_t *pEdict = &pBaseEdict[iEdict];

		if (!g_pCurrentTransmitInfo->m_pTransmitEdict->Get(i))
			continue;

		Util::Push_Entity(Util::servergameents->EdictToBaseEntity(pEdict));
		Util::RawSetI(-2, ++idx);
	}

	return 1;
}

void CPVSModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	mapPVSSize = ceil(Util::engineserver->GetClusterCount() / 8.0f);

	Util::StartTable();
		Util::AddFunc(pvs_ResetPVS, "ResetPVS");
		Util::AddFunc(pvs_CheckOriginInPVS, "CheckOriginInPVS");
		Util::AddFunc(pvs_AddOriginToPVS, "AddOriginToPVS");
		Util::AddFunc(pvs_GetClusterCount, "GetClusterCount");
		Util::AddFunc(pvs_GetClusterForOrigin, "GetClusterForOrigin");
		Util::AddFunc(pvs_CheckAreasConnected, "CheckAreasConnected");
		Util::AddFunc(pvs_GetArea, "GetArea");
		Util::AddFunc(pvs_GetPVSForCluster, "GetPVSForCluster");
		Util::AddFunc(pvs_CheckBoxInPVS, "CheckBoxInPVS");
		Util::AddFunc(pvs_AddEntityToPVS, "AddEntityToPVS");
		Util::AddFunc(pvs_OverrideStateFlags, "OverrideStateFlags");
		Util::AddFunc(pvs_SetStateFlags, "SetStateFlags");
		Util::AddFunc(pvs_GetStateFlags, "GetStateFlags");
		Util::AddFunc(pvs_SetPreventTransmitBulk, "SetPreventTransmitBulk");
		Util::AddFunc(pvs_FindInPVS, "FindInPVS");
		Util::AddFunc(pvs_TestPVS, "TestPVS");
		Util::AddFunc(pvs_ForceFullUpdate, "ForceFullUpdate");
		Util::AddFunc(pvs_GetEntitiesFromTransmit, "GetEntitiesFromTransmit");

		// Use the functions below only inside the HolyLib:PostCheckTransmit hook.  
		Util::AddFunc(pvs_RemoveEntityFromTransmit, "RemoveEntityFromTransmit");
		Util::AddFunc(pvs_RemoveAllEntityFromTransmit, "RemoveAllEntityFromTransmit");
		Util::AddFunc(pvs_AddEntityToTransmit, "AddEntityToTransmit");

		Util::AddValue(LUA_FL_EDICT_DONTSEND, "FL_EDICT_DONTSEND");
		Util::AddValue(LUA_FL_EDICT_ALWAYS, "FL_EDICT_ALWAYS");
		Util::AddValue(LUA_FL_EDICT_PVSCHECK, "FL_EDICT_PVSCHECK");
		Util::AddValue(LUA_FL_EDICT_FULLCHECK, "FL_EDICT_FULLCHECK");
	Util::FinishTable("pvs");
}

void CPVSModule::LuaShutdown()
{
	Util::NukeTable("pvs");
}

void CPVSModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

#ifndef HOLYLIB_MANUALNETWORKING
	SourceSDK::ModuleLoader server_loader("server");
	Detour::Create(
		&detour_CGMOD_Player_SetupVisibility, "CGMOD_Player::SetupVisibility",
		server_loader.GetModule(), Symbols::CGMOD_Player_SetupVisibilitySym,
		(void*)hook_CGMOD_Player_SetupVisibility, m_pID
	);

	Detour::Create(
		&detour_CServerGameEnts_CheckTransmit, "CServerGameEnts::CheckTransmit",
		server_loader.GetModule(), Symbols::CServerGameEnts_CheckTransmitSym,
		(void*)hook_CServerGameEnts_CheckTransmit, m_pID
	);
#endif
}