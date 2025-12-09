#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "server_class.h"
#include "eiface.h"
#include "dt.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CNW2Module : public IModule
{
public:
	void InitDetour(bool bPreServer) override;
	const char* Name() override { return "nw2"; };
	int Compatibility() override { return LINUX32; };
};

static CNW2Module g_pNW2Module;
IModule* pNW2Module = &g_pNW2Module;

/*
	Very simple module.
	NW2 cannot handle being in the baseline causing lots of issues.
	So when NW2 is Encoded we skip it when it's meant for the Baseline preventing all the issues.

	The NW2 bug is also very specific
	it happens/differs PER Entity class and NW2 MUST be used in the creation tick / when the entity is packed for the first time a NW2 Var Must exist.

	Because then, the NW2 var is written into the Baseline and causes all these issues with copying.
*/

// MUST be thread_local as there are other threads that we DONT want to affect!
static thread_local bool g_bSkipGMODTableEncode = false;
static Detouring::Hook detour_SV_EnsureInstanceBaseline;
static Symbols::SendTable_Encode func_SendTable_Encode = nullptr;
void hook_SV_EnsureInstanceBaseline(ServerClass *pServerClass, int iEdict, const void *pData, intp nBytes)
{
	// Fk off.
	if (pServerClass->m_InstanceBaselineIndex != INVALID_STRING_INDEX)
		return;

	ALIGN4 char writeBuffer[MAX_PACKEDENTITY_DATA] ALIGN4_POST;
	bf_write writeBuf( "SV_PackEntity->writeBuf", writeBuffer, sizeof( writeBuffer ) );

	edict_t* pEdict = Util::engineserver->PEntityOfEntIndex( iEdict );
	if ( !pEdict || !pEdict->GetUnknown() )
	{
		Warning( PROJECT_NAME " - SV_EnsureInstanceBaseline(nw2): Failed to get edict? (Skipping our NW2 fix!)\n");
		detour_SV_EnsureInstanceBaseline.GetTrampoline<Symbols::SV_EnsureInstanceBaseline>()(pServerClass, iEdict, pData, nBytes);
		return;
	}

	// Weh... I hate this but anyways, we need this class.
	unsigned char tempData[ sizeof( CSendProxyRecipients ) * MAX_DATATABLE_PROXIES ];
	CUtlMemory< CSendProxyRecipients > recip( (CSendProxyRecipients*)tempData, pServerClass->m_pTable->m_pPrecalc->GetNumDataTableProxies() );
	
	g_bSkipGMODTableEncode = true;
	if ( !func_SendTable_Encode( pServerClass->m_pTable, pEdict->GetUnknown(), &writeBuf, iEdict, &recip, false ) )
	{
		Warning( PROJECT_NAME " - SV_EnsureInstanceBaseline: SendTable_Encode returned false (ent %d).\n", iEdict );
		detour_SV_EnsureInstanceBaseline.GetTrampoline<Symbols::SV_EnsureInstanceBaseline>()(pServerClass, iEdict, pData, nBytes);
		return;
	}
	g_bSkipGMODTableEncode = false;

	detour_SV_EnsureInstanceBaseline.GetTrampoline<Symbols::SV_EnsureInstanceBaseline>()(pServerClass, iEdict, pData, nBytes);
}

static constexpr int nGMODTableIndexBits = 12;
static Detouring::Hook detour_GMODTable_Encode;
void hook_GMODTable_Encode(const unsigned char *pStruct, DVariant *pVar, const SendProp *pProp, bf_write *pOut, int objectID)
{
	if (g_bSkipGMODTableEncode)
	{
		pOut->WriteUBitLong( 0, nGMODTableIndexBits ); // No changes since were the baseline!
		pOut->WriteUBitLong( 0, 1 ); // No full update
		return;
	}

	detour_GMODTable_Encode.GetTrampoline<Symbols::GMODTable_Encode>()(pStruct, pVar, pProp, pOut, objectID);
}

void CNW2Module::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::FactoryLoader engine_loader("engine");
	Detour::Create(
		&detour_SV_EnsureInstanceBaseline, "SV_EnsureInstanceBaseline",
		engine_loader.GetModule(), Symbols::SV_EnsureInstanceBaselineSym,
		(void*)hook_SV_EnsureInstanceBaseline, m_pID
	);

	Detour::Create(
		&detour_GMODTable_Encode, "GMODTable_Encode",
		engine_loader.GetModule(), Symbols::GMODTable_EncodeSym,
		(void*)hook_GMODTable_Encode, m_pID
	);
}