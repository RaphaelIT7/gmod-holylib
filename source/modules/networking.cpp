#include "filesystem_base.h" // Has to be before symbols.h
#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "vprof.h"
#include "framesnapshot.h"
#include "packed_entity.h"
#include "server_class.h"
#include "dt.h"
#include "edict.h"
#include "eiface.h"
#define protected public // Need access to CBasePlayer::m_hViewModel
#define private public // Need access to CBaseViewModel::m_hScreens
#include "player.h"
#include "vguiscreen.h"
#undef protected
#undef private
#include "baseclient.h"
#include <bitset>
#include <unordered_set>
#include <datacache/imdlcache.h>
#include <cmodel_private.h>
#include "server.h"
#include "hltvserver.h"
#include "SkyCamera.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CNetworkingModule : public IModule
{
public:
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual void OnEntityCreated(CBaseEntity* pEntity) OVERRIDE;
	virtual void OnEntityDeleted(CBaseEntity* pEntity) OVERRIDE;
	virtual void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax) OVERRIDE;
	virtual const char* Name() { return "networking"; };
	virtual int Compatibility() { return LINUX32; };
};

/*
 * This module can't be disabled at runtime!
 * 
 * This is because of it replacing the entire CChangeFrameList class which is used & stored in the engine.
 * I need to restore and fix the code in Shutdown to allow proper unloading.
 */

CNetworkingModule g_pNetworkingModule;
IModule* pNetworkingModule = &g_pNetworkingModule;

abstract_class IChangeFrameList
{
public:
	virtual void	Release() = 0;
	virtual int		GetNumProps() = 0;
	virtual void	SetChangeTick( const int *pPropIndices, int nPropIndices, const int iTick ) = 0;
	virtual int		GetPropsChangedAfterTick( int iTick, int *iOutProps, int nMaxOutProps ) = 0;
	virtual IChangeFrameList* Copy() = 0;
protected:
	virtual			~IChangeFrameList() {}
};	

/*
=============================================================================
Simplified BSD License, see http://www.opensource.org/licenses/
-----------------------------------------------------------------------------
Copyright (c) 2017, sigsegv <sigsegv@sigpipe.info>
see https://github.com/rafradek/sigsegv-mvm/tree/master?tab=License-1-ov-file for full BSD License
*/

// This is originally from here: https://github.com/rafradek/sigsegv-mvm/blob/910b92456c7578a3eb5dff2a7e7bf4bc906677f7/src/mod/perf/sendprop_optimize.cpp#L35-L144
class CChangeFrameList : public IChangeFrameList
{
public:
	void Init(int nProperties, int iCurTick)
	{
		VPROF_BUDGET("CChangeFrameList::Init", VPROF_BUDGETGROUP_OTHER_NETWORKING);
		m_ChangeTicks.SetSize(nProperties);
		for (int i=0; i < nProperties; ++i)
			m_ChangeTicks[i] = iCurTick;
	}
public:
	virtual void Release()
	{
		--m_CopyCounter;
		if (m_CopyCounter < 0)
			delete this;
	}

	virtual IChangeFrameList* Copy()
	{
		//VPROF_BUDGET("CChangeFrameList::Copy", VPROF_BUDGETGROUP_OTHER_NETWORKING);
		++m_CopyCounter;
		return this;
	}

	virtual int GetNumProps()
	{
		return m_ChangeTicks.Count();
	}

	virtual void SetChangeTick(const int *pPropIndices, int nPropIndices, const int iTick)
	{
		VPROF_BUDGET("CChangeFrameList::SetChangeTick", VPROF_BUDGETGROUP_OTHER_NETWORKING);
		bool same = (int)m_LastChangeTicks.size() == nPropIndices;
		m_LastChangeTicks.resize(nPropIndices);
		for (int i=0; i < nPropIndices; ++i)
		{
			int prop = pPropIndices[i];
			m_ChangeTicks[prop] = iTick;
			
			same = same && m_LastChangeTicks[i] == prop;
			m_LastChangeTicks[i] = prop;
		}

		if (!same)
			m_LastSameTickNum = iTick;

		m_LastChangeTickNum = iTick;
		if (m_LastChangeTicks.capacity() > m_LastChangeTicks.size() * 8)
			m_LastChangeTicks.shrink_to_fit();
	}

	virtual int GetPropsChangedAfterTick(int iTick, int *iOutProps, int nMaxOutProps)
	{
		// Should we remove vprof here? It could slow this entire thing down since it's called so often
		// VPROF_BUDGET("CChangeFrameList::GetPropsChangedAfterTick", VPROF_BUDGETGROUP_OTHER_NETWORKING);
		int nOutProps = 0;
		if (iTick + 1 >= m_LastSameTickNum)
		{
			if (iTick >= m_LastChangeTickNum)
				return 0;

			nOutProps = m_LastChangeTicks.size();
			for (int i=0; i < nOutProps; ++i)
				iOutProps[i] = m_LastChangeTicks[i];

			return nOutProps;
		} else {
			int c = m_ChangeTicks.Count();
			for (int i=0; i < c; ++i)
			{
				if (m_ChangeTicks[i] > iTick)
				{
					iOutProps[nOutProps] = i;
					++nOutProps;
				}
			}

			return nOutProps;
		}
	}

protected:
	virtual ~CChangeFrameList()
	{
	}

private:
	CUtlVector<int>		m_ChangeTicks;

	int m_CopyCounter = 0;
	int m_LastChangeTickNum = 0;
	int m_LastSameTickNum = 0;
	std::vector<int> m_LastChangeTicks;
};

// -------------------------------------------------------------------------------------------------

static Detouring::Hook detour_AllocChangeFrameList;
static IChangeFrameList* hook_AllocChangeFrameList(int nProperties, int iCurTick)
{
	VPROF_BUDGET("AllocChangeFrameList", VPROF_BUDGETGROUP_OTHER_NETWORKING);
	CChangeFrameList* pRet = new CChangeFrameList;
	pRet->Init(nProperties, iCurTick);

	return pRet;
}

// -------------------------------------------------------------------------------------------------

static CBitVec<MAX_EDICTS> g_pShouldPrevent[MAX_PLAYERS];
static Detouring::Hook detour_CBaseEntity_GMOD_ShouldPreventTransmitToPlayer;
static bool hook_CBaseEntity_GMOD_ShouldPreventTransmitToPlayer(CBaseEntity* ent, CBasePlayer* ply)
{
	edict_t* pEdict = ent->edict();
	if (!pEdict)
		return false;

	edict_t* pPlayerEdict = ply->edict();
	if (!pPlayerEdict)
		return false;

	return g_pShouldPrevent[pPlayerEdict->m_EdictIndex-1].IsBitSet(pEdict->m_EdictIndex);
}

static void CleaupSetPreventTransmit(CBaseEntity* ent)
{
	edict_t* pEdict = ent->edict();
	if (!pEdict)
		return;

	int entIndex = pEdict->m_EdictIndex;
	if (!ent->IsPlayer())
	{
		for (int i=0; i<MAX_PLAYERS; ++i)
		{
			g_pShouldPrevent[i].Clear(entIndex);
		}
		return;
	}

	g_pShouldPrevent[entIndex-1].ClearAll();
}

static Detouring::Hook detour_CBaseEntity_GMOD_SetShouldPreventTransmitToPlayer;
static void hook_CBaseEntity_GMOD_SetShouldPreventTransmitToPlayer(CBaseEntity* pEnt, CBasePlayer* pPly, bool bPreventTransmit)
{
	edict_t* pEdict = pEnt->edict();
	if (!pEdict)
		return;

	edict_t* pPlayerEdict = pPly->edict();
	if (!pPlayerEdict)
		return;

	int plyIndex = pPlayerEdict->m_EdictIndex - 1;
	int entIndex = pEdict->m_EdictIndex;
	if (bPreventTransmit) {
		g_pShouldPrevent[plyIndex].Set(entIndex);
	} else {
		g_pShouldPrevent[plyIndex].Clear(entIndex);
	}
}

// -------------------------------------------------------------------------------------------------

static Detouring::Hook detour_CGMOD_Player_CreateViewModel;
static ConVar networking_maxviewmodels("holylib_networking_maxviewmodels", "3", 0, "Determins how many view models each player gets.", true, 0, true, 3);
static void hook_CGMOD_Player_CreateViewModel(CBasePlayer* pPlayer, int viewmodelindex)
{
	if (viewmodelindex >= networking_maxviewmodels.GetInt())
		return;

	detour_CGMOD_Player_CreateViewModel.GetTrampoline<Symbols::CGMOD_Player_CreateViewModel>()(pPlayer, viewmodelindex);
}

// -------------------------------------------------------------------------------------------------

// Everything below is also from many places inside https://github.com/rafradek/sigsegv-mvm/tree/cd2ee719719cda9c24da6e395557fdb66487adfe
#define PROP_INDEX_INVALID 0xffff
#define INVALID_PROP_INDEX 65535
struct PropIndexData
{
	unsigned short offset = 0;
	unsigned short element = 0;
	unsigned short index1 = PROP_INDEX_INVALID;
	unsigned short index2 = PROP_INDEX_INVALID;
};

struct SpecialSendPropCalc
{
	const int index;
};

struct SpecialDataTableCalc
{
	std::vector<int> propIndexes;
	int baseOffset;
};

class ServerClassCache
{
public:
	std::vector<PropIndexData> prop_offset_sendtable;

	std::vector<SpecialSendPropCalc> prop_special;

	std::unordered_map<const SendProp *, SpecialDataTableCalc> datatable_special;

	unsigned short *prop_offsets;

	//CSendNode **send_nodes;

	// prop indexes that are stopped from being send to players
	unsigned char *prop_cull;

	// prop indexes that are stopped from being send to players
	unsigned short *prop_propproxy_first;
};

bool* player_local_exclusive_send_proxy;
inline ServerClassCache* GetServerClassCache(const SendTable *pTable)
{
	return (ServerClassCache*)pTable->m_pPrecalc->m_pDTITable;
}

static Detouring::Hook detour_SendTable_CullPropsFromProxies;
static int hook_SendTable_CullPropsFromProxies( 
	const SendTable *pTable,
	
	const int *pStartProps,
	int nStartProps,

	const int iClient,
	
	const CSendProxyRecipients *pOldStateProxies,
	const int nOldStateProxies, 
	
	const CSendProxyRecipients *pNewStateProxies,
	const int nNewStateProxies,
	
	int *pOutProps,
	int nMaxOutProps
	)
{
	//memcpy(pOutProps, pStartProps, nStartProps * sizeof(int));
	int count = 0;
	//auto pPrecalc = pTable->m_pPrecalc;
	auto &prop_cull = GetServerClassCache(pTable)->prop_cull;
	for (int i = 0; i <nStartProps; i++) {
		int prop = pStartProps[i];
		//DevMsg("prop %d %d", prop, player_prop_cull[prop]);
		int proxyindex = prop_cull[prop];
		//DevMsg("%s", pPrecalc->m_Props[prop]->GetName());
		if (proxyindex < 254 ) {
			//DevMsg("node %s\n", player_send_nodes[proxyindex]->m_pTable->GetName());
			if (pNewStateProxies[proxyindex].m_Bits.IsBitSet(iClient)) {
				pOutProps[count++] = prop;
			}
		} else {
			//DevMsg("node none\n");
			pOutProps[count++] = prop;
		}
	}
	return count;
}

void AddOffsetToList(ServerClassCache &cache, int offset, int index, int element) {
	int size = cache.prop_offset_sendtable.size();
	for (int i = 0; i < size; i++) {
		if (cache.prop_offset_sendtable[i].offset == (unsigned short) offset) {
			cache.prop_offset_sendtable[i].index2 = (unsigned short) index;
			return;
		}
	}

	cache.prop_offset_sendtable.emplace_back();
	PropIndexData &data = cache.prop_offset_sendtable.back();
	data.offset = (unsigned short) offset;
	data.index1 = (unsigned short) index;
	data.element = (unsigned short) element;
};

void PropScan(int off, SendTable *s_table, int &index)
{
	for (int i = 0; i < s_table->GetNumProps(); ++i) {
		SendProp *s_prop = s_table->GetProp(i);

		if (s_prop->GetDataTable() != nullptr) {
			PropScan(off + s_prop->GetOffset(), s_prop->GetDataTable(), index);
		} else {
			//Msg("Scan Data table for %d %s %d is %d %d %d\n", index, s_prop->GetName(),  off + s_prop->GetOffset(), off, s_prop->GetProxyFn(), s_prop->GetDataTableProxyFn());
			index++;
			//onfound(s_prop, off + s_prop->GetOffset());
		}
	}
}

SendTableProxyFn local_sendtable_proxy;
void RecurseStack(ServerClassCache &cache, unsigned char* stack, CSendNode *node, CSendTablePrecalc *precalc)
{
	//stack[node->m_RecursiveProxyIndex] = strcmp(node->m_pTable->GetName(), "DT_TFNonLocalPlayerExclusive") == 0;
	stack[node->m_RecursiveProxyIndex] = node->m_DataTableProxyIndex;
	if (node->m_DataTableProxyIndex < 254) {
		//cache.send_nodes[node->m_DataTableProxyIndex] = node;
		player_local_exclusive_send_proxy[node->m_DataTableProxyIndex] = precalc->m_DatatableProps[node->m_iDatatableProp]->GetDataTableProxyFn() == local_sendtable_proxy;
	}
			
	//("data %d %d %s %d\n", node->m_RecursiveProxyIndex, stack[node->m_RecursiveProxyIndex], node->m_pTable->GetName(), node->m_nRecursiveProps);
	for (int i = 0; i < node->m_Children.Count(); i++) {
		CSendNode *child = node->m_Children[i];
		RecurseStack(cache, stack, child, precalc);
	}
}

abstract_class CDatatableStack
{
public:
	CDatatableStack( CSendTablePrecalc *pPrecalc, unsigned char *pStructBase, int objectID );

	// This must be called before accessing properties.
	void Init( bool bExplicitRoutes=false );

	// The stack is meant to be used by calling SeekToProp with increasing property
	// numbers.
	void			SeekToProp( int iProp );

	bool			IsCurProxyValid() const;
	bool			IsPropProxyValid(int iProp ) const;
	int				GetCurPropIndex() const;
	
	unsigned char*	GetCurStructBase() const;
	
	int				GetObjectID() const;

	// Derived classes must implement this. The server gets one and the client gets one.
	// It calls the proxy to move to the next datatable's data.
	virtual void RecurseAndCallProxies( CSendNode *pNode, unsigned char *pStructBase ) = 0;

public:
	CSendTablePrecalc *m_pPrecalc;
	
	enum
	{
		MAX_PROXY_RESULTS = 256
	};

	// These point at the various values that the proxies returned. They are setup once, then 
	// the properties index them.
	unsigned char *m_pProxies[MAX_PROXY_RESULTS];
	unsigned char *m_pStructBase;
	int m_iCurProp;

protected:
	const SendProp *m_pCurProp;
	int m_ObjectID;
	bool m_bInitted;
};

inline bool CDatatableStack::IsPropProxyValid(int iProp ) const
{
	return m_pProxies[m_pPrecalc->m_PropProxyIndices[iProp]] != 0;
}

inline bool CDatatableStack::IsCurProxyValid() const
{
	return m_pProxies[m_pPrecalc->m_PropProxyIndices[m_iCurProp]] != 0;
}

inline int CDatatableStack::GetCurPropIndex() const
{
	return m_iCurProp;
}

inline unsigned char* CDatatableStack::GetCurStructBase() const
{
	return m_pProxies[m_pPrecalc->m_PropProxyIndices[m_iCurProp]]; 
}

inline void CDatatableStack::SeekToProp( int iProp )
{
	Assert( m_bInitted );
	
	m_iCurProp = iProp;
	m_pCurProp = m_pPrecalc->m_Props[iProp];
}

inline int CDatatableStack::GetObjectID() const
{
	return m_ObjectID;
}

CDatatableStack::CDatatableStack( CSendTablePrecalc *pPrecalc, unsigned char *pStructBase, int objectID )
{
	m_pPrecalc = pPrecalc;

	m_pStructBase = pStructBase;
	m_ObjectID = objectID;
	
	m_iCurProp = 0;
	m_pCurProp = NULL;

	m_bInitted = false;

#ifdef _DEBUG
	memset( m_pProxies, 0xFF, sizeof( m_pProxies ) );
#endif
}

void CDatatableStack::Init( bool bExplicitRoutes )
{
	if ( bExplicitRoutes )
	{
		memset( m_pProxies, 0xFF, sizeof( m_pProxies[0] ) * m_pPrecalc->m_ProxyPaths.Count() );
	}
	else
	{
		// Walk down the tree and call all the datatable proxies as we hit them.
		RecurseAndCallProxies( &m_pPrecalc->m_Root, m_pStructBase );
	}

	m_bInitted = true;
}

class CPropMapStack : public CDatatableStack
{
public:
	CPropMapStack( CSendTablePrecalc *pPrecalc, const CStandardSendProxies *pSendProxies ) :
		CDatatableStack( pPrecalc, (unsigned char*)1, -1 )
	{
		m_pPropMapStackPrecalc = pPrecalc;
		m_pSendProxies = pSendProxies;
	}

	static inline bool IsNonPointerModifyingProxy( SendTableProxyFn fn, const CStandardSendProxies *pSendProxies )
	{
		if ( fn == pSendProxies->m_DataTableToDataTable || fn == pSendProxies->m_SendLocalDataTable )
			return true;
		
		if( pSendProxies->m_ppNonModifiedPointerProxies )
		{
			CNonModifiedPointerProxy *pCur = *pSendProxies->m_ppNonModifiedPointerProxies;
			while ( pCur )
			{
				if ( pCur->m_Fn == fn )
					return true;

				pCur = pCur->m_pNext;
			}
		}

		return false;
	}

	inline unsigned char*	CallPropProxy( CSendNode *pNode, int iProp, unsigned char *pStructBase )
	{
		if ( !pStructBase )
			return 0;
		
		const SendProp *pProp = m_pPropMapStackPrecalc->m_DatatableProps[iProp];
		if ( IsNonPointerModifyingProxy( pProp->GetDataTableProxyFn(), m_pSendProxies ) )
		{
			// Note: these are offset by 1 (see the constructor), otherwise it won't recurse
			// during the Init call because pCurStructBase is 0.
			return pStructBase + pProp->GetOffset();
		} else {
			return 0;
		}
	}

	virtual void RecurseAndCallProxies( CSendNode *pNode, unsigned char *pStructBase )
	{
		// Remember where the game code pointed us for this datatable's data so 
		m_pProxies[ pNode->m_RecursiveProxyIndex ] = pStructBase;
		
		//const SendProp *pProp = m_pPropMapStackPrecalc->m_DatatableProps[pNode->m_iDatatableProp];

		for ( int iChild=0; iChild < pNode->m_Children.Count(); iChild++ )
		{
			CSendNode *pCurChild = pNode->m_Children[iChild];
			const SendProp *pChildProp = m_pPropMapStackPrecalc->m_DatatableProps[pCurChild->m_iDatatableProp];
			
			unsigned char *pNewStructBase = NULL;
			if ( pStructBase )
				pNewStructBase = CallPropProxy( pCurChild, pCurChild->m_iDatatableProp, pStructBase );

			m_pIsPointerModifyingProxy[pCurChild->m_RecursiveProxyIndex] = 
				IsNonPointerModifyingProxy(pChildProp->GetDataTableProxyFn(), m_pSendProxies) ? 
					m_pIsPointerModifyingProxy[pNode->m_RecursiveProxyIndex] : pCurChild;
			m_iBaseOffset[pCurChild->m_RecursiveProxyIndex] = reinterpret_cast<intptr_t>(pStructBase);

			RecurseAndCallProxies( pCurChild, pNewStructBase );
		}
	}

public:
	CSendTablePrecalc *m_pPropMapStackPrecalc;
	const CStandardSendProxies *m_pSendProxies;
	const CSendNode *m_pIsPointerModifyingProxy[MAX_PROXY_RESULTS] {nullptr};
	intptr_t m_iBaseOffset[MAX_PROXY_RESULTS] {0};
};

class CEntityWriteInfo
{
public:
	virtual	~CEntityWriteInfo() {};
	
	bool			m_bAsDelta;
	CClientFrame	*m_pFrom;
	CClientFrame	*m_pTo;

	int		m_UpdateType;

	int				m_nOldEntity;	// current entity index in m_pFrom
	int				m_nNewEntity;	// current entity index in m_pTo

	int				m_nHeaderBase;
	int				m_nHeaderCount;
	bf_write		*m_pBuf;
	int				m_nClientEntity;

	PackedEntity	*m_pOldPack;
	PackedEntity	*m_pNewPack;
};

#if 0
struct PropCacheEntry
{
	int offset = 0;
	fieldtype_t fieldType = FIELD_VOID;
	int arraySize = 1;
	int elementStride = 0;
	SendProp *prop = nullptr;
	DatatableProxyVector usedTables;
};

struct PluginCallbackInfo
{
	CBaseEntity *entity;
	//IPluginFunction *callback; 
	int arrayIndex;
	std::string name;
	int callbackIndex;
	bool isPerClient;
	bool valueSet = false;
	bool isSendProxyExt = false;
	bool isGameRules = false;
	variant_t valuePre;
	std::bitset<ABSOLUTE_PLAYER_LIMIT+1> valueSetClients;
	std::vector<variant_t> valueApplyClients;
	PropCacheEntry propCacheEntry;
};

static std::vector<PluginCallbackInfo> callbacks;
void CallbackPluginCall(CBaseEntity *entity, int clientIndex, DVariant &value, int callbackIndex, SendProp *prop, uintptr_t data) {
	auto &info = callbacks[data];
	if (!info.valueSet || !info.valueSetClients.test(clientIndex))
		return;

	variant_t &varValue = info.valueApplyClients[clientIndex];
	switch (value.m_Type) 
	{
		case DPT_Int: {
			if (varValue.FieldType() == FIELD_EHANDLE) {
				auto handle = varValue.Entity();
				int serialNum = handle.GetSerialNumber() & ((1 << NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS) - 1);
				value.m_Int = handle.Get() ? (handle.GetEntryIndex() | (serialNum << MAX_EDICT_BITS)) : INVALID_NETWORKED_EHANDLE_VALUE;
			} else {
				varValue.Convert(FIELD_INTEGER);
				value.m_Int = varValue.Int();
			}
			break;
		} case DPT_Float: {
			varValue.Convert(FIELD_FLOAT);
			value.m_Float = varValue.Float();
			break;
		} case DPT_String: {
			value.m_pString = varValue.String();
			break;
		} case DPT_Vector: {
			varValue.Convert(FIELD_VECTOR);
			varValue.Vector3D(*(reinterpret_cast<Vector *>(value.m_Vector)));
			break;
		} case DPT_VectorXY: {
			varValue.Convert(FIELD_VECTOR);
			varValue.Vector3D(*(reinterpret_cast<Vector *>(value.m_Vector)));
			break;
		} default:
			break;
	}		
}
#endif

static Detouring::Hook detour_CBaseServer_WriteDeltaEntities;
static Symbols::CBaseServer_WriteDeltaEntities CBaseServer_WriteDeltaEntities_func;
void hook_CBaseServer_WriteDeltaEntities(CBaseServer* pServer, CBaseClient *client, CClientFrame *to, CClientFrame *from, bf_write &pBuf)
{
	CBaseServer_WriteDeltaEntities_func(pServer, client, to, from, pBuf);
}

/*#define PROP_WRITE_OFFSET_ABSENT 65535
struct PropWriteOffset
{
	unsigned short offset;
	unsigned short size;
};

static std::vector<PropWriteOffset> prop_write_offset[MAX_EDICTS];
static std::atomic<unsigned int> rc_CHLTVClient_SendSnapshot;
static Detouring::Hook detour_SendTable_WritePropList;
static Symbols::SendTable_WritePropList SendTable_WritePropList_func;
void hook_SendTable_WritePropList(
	const SendTable *pTable,
	const void *pState,
	const int nBits,
	bf_write *pOut,
	const int objectID,
	const int *pCheckProps,
	const int nCheckProps
	)
{
	if (rc_CHLTVClient_SendSnapshot.load() == 0 && objectID >= 0) {
		if ( nCheckProps == 0 ) {
			// Write single final zero bit, signifying that there no changed properties
			pOut->WriteOneBit( 0 );
			return;
		}
		if (prop_write_offset[objectID].empty()) {
			SendTable_WritePropList_func(pTable, pState, nBits, pOut, objectID, pCheckProps, nCheckProps);
			return;
		}
		CDeltaBitsWriter deltaBitsWriter( pOut );
		bf_read inputBuffer( "SendTable_WritePropList->inputBuffer", pState, BitByte( nBits ), nBits );

		auto pPrecalc = pTable->m_pPrecalc;
		auto offset_data = prop_write_offset[objectID].data();
		for (int i = 0; i < nCheckProps; i++) {
			int propid = pCheckProps[i];
			int offset = offset_data[propid].offset;
			if (offset == 0 || offset == PROP_WRITE_OFFSET_ABSENT)
				continue;
                

			deltaBitsWriter.WritePropIndex(propid);

			int len = offset_data[propid].size;
			inputBuffer.Seek(offset);
			pOut->WriteBitsFromBuffer(&inputBuffer, len);
		}

		return;
	}

	SendTable_WritePropList_func(pTable, pState, nBits, pOut, objectID, pCheckProps, nCheckProps);
}*/

static Detouring::Hook detour_SendTable_CalcDelta;
int hook_SendTable_CalcDelta(
	const SendTable *pTable,
	
	const void *pFromState,
	const int nFromBits,
	
	const void *pToState,
	const int nToBits,
	
	int *pDeltaProps,
	int nMaxDeltaProps,

	const int objectID
	)
{
	int count = detour_SendTable_CalcDelta.GetTrampoline<Symbols::SendTable_CalcDelta>()(pTable, pFromState, nFromBits, pToState, nToBits, pDeltaProps, nMaxDeltaProps, objectID);

	return count;
}

#if 0
thread_local PackedEntity *preOldPack = nullptr;
thread_local CEntityWriteInfo *writeInfo = nullptr;
static Detouring::Hook detour_SV_DetermineUpdateType;
static Symbols::SV_DetermineUpdateType SV_DetermineUpdateType_func;
static void hook_SV_DetermineUpdateType(CEntityWriteInfo& u)
{
#if ARCHITECTURE_IS_X86 // Load 'u' from eax manually since the compiler is screwing up
#if SYSTEM_WINDOWS
	__asm {
		mov esi, eax
	}
#else
	asm("mov %eax, %esi");
#endif
#endif

	bool hasOverride = entityHasOverride[u.m_nNewEntity];
	if (hasOverride) {
		preOldPack = u.m_pOldPack;
		writeInfo = &u;
		u.m_pOldPack = nullptr;
	}

#if ARCHITECTURE_IS_X86
#if SYSTEM_WINDOWS
	__asm {
		lea edx, SV_DetermineUpdateType_func
		mov eax, u
		call edx
	}
#else
	asm("movl %0, %%edx" : : "r" (SV_DetermineUpdateType_func) : "edx");
	asm("movl %0, %%eax" : : "r" (&u) : "%eax");
	asm("call *%edx");
#endif
#endif

	if (hasOverride) {
		u.m_pOldPack = preOldPack;
		writeInfo = nullptr;
	}
}

static Detouring::Hook detour_PackedEntity_GetPropsChangedAfterTick;
static Symbols::PackedEntity_GetPropsChangedAfterTick PackedEntity_GetPropsChangedAfterTick_func;
int hook_PackedEntity_GetPropsChangedAfterTick(PackedEntity* pPacked, int tick, int *iOutProps, int nMaxOutProps)
{
	auto result = PackedEntity_GetPropsChangedAfterTick_func(pPacked, tick, iOutProps, nMaxOutProps);
	if (entityHasOverride[pPacked->m_nEntityIndex] && writeInfo != nullptr) {
		writeInfo->m_pOldPack = preOldPack;
		return CheckOverridePropIndex(iOutProps, result, pPacked->m_nEntityIndex);
	}

	return result;
}
#endif

static Detouring::Hook detour_CGameServer_SendClientMessages;
static Symbols::CGameServer_SendClientMessages CGameServer_SendClientMessages_func;
void hook_CGameServer_SendClientMessages(CBaseServer* pServer, bool sendSnapshots)
{
	CGameServer_SendClientMessages_func(pServer, sendSnapshots);
}

class CCServerNetworkProperty : public IServerNetworkable, public IEventRegisterCallback
{
public:
	DECLARE_CLASS_NOBASE( CServerNetworkProperty );
	DECLARE_DATADESC();

public:
	CCServerNetworkProperty();
	virtual	~CCServerNetworkProperty();

public:
	inline IHandleEntity  *GetEntityHandle( );
	inline edict_t			*GetEdict() const;
	inline CBaseNetworkable* GetBaseNetworkable();
	inline CBaseEntity*	GetBaseEntity();
	inline ServerClass*	GetServerClass();
	inline const char*		GetClassName() const;
	inline void			Release();
	inline int		AreaNum();
	inline PVSInfo_t*		GetPVSInfo();

public:
	inline void Init( CBaseEntity *pEntity );
	inline void AttachEdict( edict_t *pRequiredEdict = NULL );
	inline int	entindex() const { return m_pPev->m_EdictIndex; };
	inline edict_t *edict() { return m_pPev; };
	inline const edict_t *edict() const { return m_pPev; };
	inline void SetEdict( edict_t *pEdict );
	inline void NetworkStateForceUpdate();
	inline void NetworkStateChanged();
	inline void NetworkStateChanged( unsigned short offset );
	inline void MarkPVSInformationDirty();
	inline void MarkForDeletion();
	inline bool IsMarkedForDeletion() const;
	inline void SetNetworkParent( EHANDLE hParent );
	inline CCServerNetworkProperty* GetNetworkParent();
	inline void			SetUpdateInterval( float N );
	inline bool IsInPVS( const CCheckTransmitInfo *pInfo );
	inline bool IsInPVS( const edict_t *pRecipient, const void *pvs, int pvssize );
	inline virtual void FireEvent();
	inline void RecomputePVSInformation();

private:
	void DetachEdict();
	CBaseEntity *GetOuter();
	void SetTransmit( CCheckTransmitInfo *pInfo );

public:
	CBaseEntity *m_pOuter;
	edict_t	*m_pPev;
	PVSInfo_t m_PVSInfo;
	ServerClass *m_pServerClass;
	EHANDLE m_hParent;
	CEventRegister	m_TimerEvent;
	bool m_bPendingStateChange : 1;
};

inline void CCServerNetworkProperty::RecomputePVSInformation()
{
	if ( m_pPev && ( ( m_pPev->m_fStateFlags & FL_EDICT_DIRTY_PVS_INFORMATION ) != 0 ) )
	{
		m_pPev->m_fStateFlags &= ~FL_EDICT_DIRTY_PVS_INFORMATION;
		engine->BuildEntityClusterList( m_pPev, &m_PVSInfo );
	}
}

inline int CCServerNetworkProperty::AreaNum()
{
	RecomputePVSInformation();
	return m_PVSInfo.m_nAreaNum;
}

inline CCServerNetworkProperty* CCServerNetworkProperty::GetNetworkParent()
{
	CBaseEntity *pParent = m_hParent.Get();
	return pParent ? (CCServerNetworkProperty*)pParent->NetworkProp() : NULL;
}

static CCollisionBSPData* g_BSPData;
inline bool CheckAreasConnected(int area1, int area2)
{
	return g_BSPData->map_areas[area1].floodnum == g_BSPData->map_areas[area2].floodnum;
}

inline bool CCServerNetworkProperty::IsInPVS( const CCheckTransmitInfo *pInfo )
{
	// PVS data must be up to date
	// Assert( !m_pPev || ( ( m_pPev->m_fStateFlags & FL_EDICT_DIRTY_PVS_INFORMATION ) == 0 ) );
	
	int i;

	// Early out if the areas are connected
	if ( !m_PVSInfo.m_nAreaNum2 )
	{
		for ( i=0; i< pInfo->m_AreasNetworked; i++ )
		{
			int clientArea = pInfo->m_Areas[i];
			if ( clientArea == m_PVSInfo.m_nAreaNum || CheckAreasConnected( clientArea, m_PVSInfo.m_nAreaNum ) )
				break;
		}
	}
	else
	{
		// doors can legally straddle two areas, so
		// we may need to check another one
		for ( i=0; i< pInfo->m_AreasNetworked; i++ )
		{
			int clientArea = pInfo->m_Areas[i];
			if ( clientArea == m_PVSInfo.m_nAreaNum || clientArea == m_PVSInfo.m_nAreaNum2 )
				break;

			if ( CheckAreasConnected( clientArea, m_PVSInfo.m_nAreaNum ) )
				break;

			if ( CheckAreasConnected( clientArea, m_PVSInfo.m_nAreaNum2 ) )
				break;
		}
	}

	if ( i == pInfo->m_AreasNetworked )
	{
		// areas not connected
		return false;
	}

	// ignore if not touching a PV leaf
	// negative leaf count is a node number
	// If no pvs, add any entity

	//Assert( edict() != pInfo->m_pClientEnt );

	unsigned char *pPVS = ( unsigned char * )pInfo->m_PVS;
	
	if ( m_PVSInfo.m_nClusterCount < 0 )   // too many clusters, use headnode
	{
		return (engine->CheckHeadnodeVisible( m_PVSInfo.m_nHeadNode, pPVS, pInfo->m_nPVSSize ) != 0);
	}
	
	for ( i = m_PVSInfo.m_nClusterCount; --i >= 0; )
	{
		int nCluster = m_PVSInfo.m_pClusters[i];
		if ( ((int)(pPVS[nCluster >> 3])) & BitVec_BitInByte( nCluster ) )
			return true;
	}

	return false;		// not visible
}

inline CBaseEntity* CCServerNetworkProperty::GetBaseEntity()
{
	return m_pOuter;
}

static inline void CBitVec_AndNot(CBitVec<MAX_EDICTS>* a, const CBitVec<MAX_EDICTS>* b)
{
	uint32* aBase = a->Base();
	const uint32* bBase = b->Base();
	int nWords = a->GetNumDWords();

	for (int i = 0; i < nWords; ++i)
	{
		aBase[i] = aBase[i] & ~bBase[i];
	}
}

/*
 * For now this is called from the pvs module meaning we RELY on it.
 * What did we change? basicly nothing yet. I'm just testing around.
 * 
 * NOTE: It's shit & somehow were loosing performance to something. Probably us detouring it is causing our performance loss.
 */
static ConVar* sv_force_transmit_ents = nullptr;
static CBaseEntity* g_pEntityCache[MAX_EDICTS] = {nullptr};
bool g_pReplaceCServerGameEnts_CheckTransmit = false;
static edict_t* world_edict = nullptr;
static Symbols::CBasePlayer_GetViewModel func_CBasePlayer_GetViewModel;
static CBaseEntity* g_pPlayerHandsEntity[MAX_PLAYERS] = {0}; // Should never contain invalid entities since any Entity that is removed will have their entry set to NULL

static Detouring::Hook detour_Player__SetHands;
static int hook_Player__SetHands(GarrysMod::Lua::ILuaInterface* LUA)
{
	CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, false);
	CBaseEntity* pEntity = Util::Get_Entity(LUA, 2, false);
	if (pEntity && pPlayer && pEntity->edict())
	{
		g_pPlayerHandsEntity[pPlayer->edict()->m_EdictIndex-1] = pEntity;
	}

	return detour_Player__SetHands.GetTrampoline<Symbols::Player__SetHands>()(LUA);
}

static CBitVec<MAX_EDICTS> g_pDontTransmitCache; // Reset on every CServerGameEnts::CheckTransmit call
static CBitVec<MAX_EDICTS> g_pDontTransmitWeaponCache; // Per frame cache
static bool g_bFilledDontTransmitWeaponCache[MAX_PLAYERS] = {0};
static Detouring::Hook detour_CBaseCombatCharacter_SetTransmit;
static Symbols::CBaseCombatCharacter_SetTransmit func_CBaseAnimating_SetTransmit;
static ConVar networking_transmit_all_weapons("holylib_networking_transmit_all_weapons", "1", 0, "Experimental - By default all weapons are networked based on their PVS, though normally if they have an owner you might only want the active weapon to be networked");
static ConVar networking_transmit_all_weapons_to_owner("holylib_networking_transmit_all_weapons_to_owner", "1", 0, "Experimental - By default all weapons are networked to the owner");
static void hook_CBaseCombatCharacter_SetTransmit(CBaseCombatCharacter* pCharacter, CCheckTransmitInfo *pInfo, bool bAlways)
{
	edict_t* pEdict = pCharacter->edict();
	if ( pInfo->m_pTransmitEdict->Get( pEdict->m_EdictIndex ) )
		return;

	func_CBaseAnimating_SetTransmit( pCharacter, pInfo, bAlways ); // Base transmit

	bool bLocalPlayer = ( pInfo->m_pClientEnt == pEdict );
	if (networking_transmit_all_weapons.GetBool() || (bLocalPlayer && networking_transmit_all_weapons_to_owner.GetBool()))
	{
		for ( int i=0; i < MAX_WEAPONS; i++ )
		{
			int entIndex = pCharacter->m_hMyWeapons[i].GetEntryIndex();
			if (entIndex < 0 || entIndex > MAX_EDICTS)
				continue;

			CBaseEntity *pWeapon = g_pEntityCache[entIndex];
			if ( !pWeapon )
				continue;

			// The local player is sent all of his weapons.
			pWeapon->SetTransmit( pInfo, bAlways );
		}
	} else {
		int pActiveWeaponIndex = pCharacter->m_hActiveWeapon.m_Value.GetEntryIndex();
		if (pActiveWeaponIndex > 0 && pActiveWeaponIndex < MAX_EDICTS)
		{
			CBaseEntity* pActiveWeapon = g_pEntityCache[pActiveWeaponIndex];
			if (pActiveWeapon)
			{
				pActiveWeapon->SetTransmit(pInfo, bAlways);
			}
		}

		if (!g_bFilledDontTransmitWeaponCache[pEdict->m_EdictIndex-1])
		{
			for ( int i=0; i < MAX_WEAPONS; i++ )
			{
				int entIndex = pCharacter->m_hMyWeapons[i].GetEntryIndex();
				if (entIndex < 0 || entIndex > MAX_EDICTS)
					continue;

				CBaseEntity *pWeapon = g_pEntityCache[entIndex];
				if ( !pWeapon )
					continue;

				g_pDontTransmitCache.Set(entIndex);
				g_pDontTransmitWeaponCache.Set(entIndex);
			}
			g_bFilledDontTransmitWeaponCache[pEdict->m_EdictIndex-1] = true;
		}
	}
}

// Per tick cache
static int g_iLastCheckTransmit = -1;
static CBitVec<MAX_EDICTS> g_pAlwaysTransmitCacheBitVec;
static CBitVec<MAX_EDICTS> g_pPlayerTransmitCacheBitVec[MAX_PLAYERS];
static CBitVec<MAX_EDICTS> g_bWasSeenByPlayer;
static int g_iPlayerTransmitCacheAreaNum[MAX_PLAYERS] = {0};
#define HOLYLIB_NETWORKING_PRECOMPUTEFULLCHECK 0
#if HOLYLIB_NETWORKING_PRECOMPUTEFULLCHECK
static unsigned char g_iEntityStateFlags[MAX_EDICTS] = {0};
#endif
static ConVar networking_fastpath("holylib_networking_fastpath", "0", 0, "Experimental - If two players are in the same area, then it will reuse the transmit state of the first calculated player saving a lot of time");
static ConVar networking_fasttransmit("holylib_networking_fasttransmit", "1", 0, "Experimental - Replaces CServerGameEnts::CheckTransmit with our own implementation");
static ConVar networking_fastpath_usecluster("holylib_networking_fastpath_usecluster", "1", 0, "Experimental - When using the fastpatth, it will compate against clients in the same cluster instead of area");
static Symbols::GetCurrentSkyCamera func_GetCurrentSkyCamera = NULL;
bool New_CServerGameEnts_CheckTransmit(IServerGameEnts* gameents, CCheckTransmitInfo *pInfo, const unsigned short *pEdictIndices, int nEdicts)
{
	if (!networking_fasttransmit.GetBool() || !gpGlobals || !engine)
		return false;

	// get recipient player's skybox:
	CBaseEntity *pRecipientEntity = Util::servergameents->EdictToBaseEntity( pInfo->m_pClientEnt );

	//Assert( pRecipientEntity && pRecipientEntity->IsPlayer() );
	if ( !pRecipientEntity )
		return true;
	
	MDLCACHE_CRITICAL_SECTION();
	CBasePlayer *pRecipientPlayer = static_cast<CBasePlayer*>( pRecipientEntity );
	// BUG: Our offsets are fked, so pRecipientPlayer->m_Local.m_skybox3d.area is pointing at the most random shit :sad:
	const int skyBoxArea = func_GetCurrentSkyCamera ? func_GetCurrentSkyCamera()->m_skyboxData.area : pRecipientPlayer->m_Local.m_skybox3d.area; // RIP, crash any% offsets are not reliable at all! Good thing the SDK is up to date
	const int clientIndex = pInfo->m_pClientEnt->m_EdictIndex - 1;

	const Vector& clientPosition = (pRecipientPlayer->GetViewEntity() != NULL) ? pRecipientPlayer->GetViewEntity()->EyePosition() : pRecipientPlayer->EyePosition();
	int clientArea = -1;
	if (networking_fastpath_usecluster.GetBool()) {
		clientArea = engine->GetClusterForOrigin(clientPosition);
	} else {
		clientArea = engine->GetArea(clientPosition);
	}

	// NOTE: We intentionally use GetArea and not GetCluster, since a Area is far bigger than a cluster & it should work good enouth.
	// Possible BUG: The PVS might hate us for doing such a cruel thing to it. Anyways >:3

	// ToDo: Bring over's CS:GO code for InitialSpawnTime
	//const bool bIsFreshlySpawned = pRecipientPlayer->GetInitialSpawnTime()+3.0f > gpGlobals->curtime;

#ifndef _X360
	const bool bIsHLTV = pInfo->m_pTransmitAlways != NULL; // pRecipientPlayer->IsHLTV(); Why do we not use IsHLTV()? Because its NOT a virtual function & the variables are fked
	//const bool bIsReplay = pRecipientPlayer->IsReplay();

	// m_pTransmitAlways must be set if HLTV client
	// Assert( bIsHLTV == ( pInfo->m_pTransmitAlways != NULL) );
#endif

	bool bFastPath = networking_fastpath.GetBool();
	bool bTransmitAllWeapons = networking_transmit_all_weapons.GetBool();
	bool bFirstTransmit = g_iLastCheckTransmit != gpGlobals->tickcount;
	if (bFirstTransmit)
	{
		if (bFastPath)
		{
			Plat_FastMemset(g_iPlayerTransmitCacheAreaNum, 0, sizeof(g_iPlayerTransmitCacheAreaNum));
			Plat_FastMemset(g_pPlayerTransmitCacheBitVec, 0, sizeof(g_pPlayerTransmitCacheBitVec));
		}

		g_bWasSeenByPlayer.ClearAll();
		g_pAlwaysTransmitCacheBitVec.ClearAll();

		g_pDontTransmitWeaponCache.ClearAll();
		Plat_FastMemset(g_bFilledDontTransmitWeaponCache, 0, sizeof(g_bFilledDontTransmitWeaponCache));

		g_iLastCheckTransmit = gpGlobals->tickcount;
#if HOLYLIB_NETWORKING_PRECOMPUTEFULLCHECK
		for (int i=0; i < nEdicts; ++i) // I feel like caching FL_EDICT_FULLCHECK will result in broken PVSs for things like dynamic lights.
		{
			int iEdict = pEdictIndices[i];

			edict_t *pEdict = &world_edict[iEdict];
			int nFlags = pEdict->m_fStateFlags & FL_EDICT_FULLCHECK;
			if (nFlags == FL_EDICT_FULLCHECK)
			{
				CBaseEntity *pEnt = g_pEntityCache[iEdict];
				nFlags = pEnt->ShouldTransmit( pInfo );

				if (nFlags == FL_EDICT_FULLCHECK)
					nFlags = FL_EDICT_PVSCHECK; // Fking case that should never happen.
			}

			g_iEntityStateFlags[iEdict] = nFlags;
		}
#endif
	} else {
		g_pAlwaysTransmitCacheBitVec.CopyTo(pInfo->m_pTransmitEdict);
		if (bIsHLTV)
		{
			g_pAlwaysTransmitCacheBitVec.CopyTo(pInfo->m_pTransmitAlways);
		}

		if (bFastPath)
		{
			for (int iOtherClient = 0; iOtherClient<MAX_PLAYERS; ++iOtherClient)
			{
				if (g_iPlayerTransmitCacheAreaNum[iOtherClient] != clientArea)
					continue;

				g_pPlayerTransmitCacheBitVec[iOtherClient].CopyTo(pInfo->m_pTransmitEdict);

				if (bIsHLTV)
				{
					g_pPlayerTransmitCacheBitVec[iOtherClient].CopyTo(pInfo->m_pTransmitAlways);
				}

				// g_pPlayerTransmitCacheBitVec won't contain any information about the client the cache was build upon, so we need to call SetTransmit ourselfs.
				// & yes, using the g_pEntityCache like this is safe, even if it doesn't look save - Time to see how long it'll take until I regret writing this
				g_pEntityCache[iOtherClient+1]->SetTransmit(pInfo, true);
				pRecipientPlayer->SetTransmit(pInfo, true);
				// ENGINE BUG: CBaseCombatCharacter::SetTransmit doesn't network the player's viewmodel! So we need to do it ourself.
				// This was probably done since CBaseViewModel::ShouldTransmit determines if it would be sent or not.
				if (func_CBasePlayer_GetViewModel)
				{
					for (int iViewModel=0; iViewModel<MAX_VIEWMODELS; ++iViewModel)
					{
						CBaseViewModel* pViewModel = func_CBasePlayer_GetViewModel(pRecipientPlayer, iViewModel, true); // Secret dependency on g_pEntityList
						if (pViewModel)
						{
							pViewModel->SetTransmit(pInfo, true);
						}
					}
				}

				CBaseEntity* pHandsEntity = g_pPlayerHandsEntity[clientIndex];
				if (pHandsEntity)
				{
					pHandsEntity->SetTransmit(pInfo, true);
				}

				// Extra stuff to hopefully not break the observer mode
				if (pRecipientPlayer->GetObserverMode() == OBS_MODE_IN_EYE)
				{
					CBaseEntity* pObserverEntity = pRecipientPlayer->GetObserverTarget();
					if (pObserverEntity)
					{
						pObserverEntity->SetTransmit(pInfo, true);
						if (pObserverEntity->IsPlayer())
						{
							CBasePlayer* pObserverPlayer = (CBasePlayer*)pObserverEntity;
							if (func_CBasePlayer_GetViewModel)
							{
								for (int iViewModel=0; iViewModel<MAX_VIEWMODELS; ++iViewModel)
								{
									CBaseViewModel* pViewModel = func_CBasePlayer_GetViewModel(pRecipientPlayer, iViewModel, true); // Secret dependency on g_pEntityList
									if (pViewModel)
									{
										pViewModel->SetTransmit(pInfo, true);
									}
								}
							}

							CBaseEntity* pHandsEntity = g_pPlayerHandsEntity[pObserverPlayer->edict()->m_EdictIndex-1];
							if (pHandsEntity)
							{
								pHandsEntity->SetTransmit(pInfo, true);
							}
						}
					}
				}

				// Fast way to set all prevent transmit things.
				CBitVec_AndNot(pInfo->m_pTransmitEdict, &g_pShouldPrevent[clientIndex]);
				if (bIsHLTV)
				{
					CBitVec_AndNot(pInfo->m_pTransmitAlways, &g_pShouldPrevent[clientIndex]);
				}

				// Since we optimized PackEntities_Normal using g_bWasSeenByPlayer, we need to now also perform this Or here.
				// If we don't do this, Entities like the CBaseViewModel won't be packed by PackEntities_Normal causing a crash later deep inside SV_WriteEnterPVS
				pInfo->m_pTransmitEdict->Or(g_bWasSeenByPlayer, &g_bWasSeenByPlayer);
				return true; // fast route when players are in the same area, we can save a tone of calculation hopefully without breaking anything.
			}
		}
	}

	g_pShouldPrevent[clientIndex].CopyTo(&g_pDontTransmitCache); // We combine Gmod's prevent transmit with also our things to remove unessesary checks.
	if (!bFirstTransmit) {
		g_pDontTransmitWeaponCache.Or(g_pDontTransmitCache, &g_pDontTransmitCache); // Now combine our cached weapon cache.
	}

	const int clientEntIndex = pInfo->m_pClientEnt->m_EdictIndex;
	static CBitVec<MAX_EDICTS> pClientCache; // Temporary cache used when we are calculating the transmit to the current pRecipientPlayer
	bool bForceTransmit = sv_force_transmit_ents->GetBool();
	bool bWasTransmitToPlayer = false;
	for ( int i=0; i < nEdicts; i++ )
	{
		int iEdict = pEdictIndices[i];

		edict_t *pEdict = &world_edict[iEdict]; // world_edict is already cached.
		// Assert( pEdict == engine->PEntityOfEntIndex( iEdict ) );
#if HOLYLIB_NETWORKING_PRECOMPUTEFULLCHECK
		int nFlags = g_iEntityStateFlags[iEdict]; // Get our cached values.
#else
		int nFlags = pEdict->m_fStateFlags & (FL_EDICT_DONTSEND|FL_EDICT_ALWAYS|FL_EDICT_PVSCHECK|FL_EDICT_FULLCHECK);
#endif

		if ( iEdict == clientEntIndex )
		{
			pInfo->m_pTransmitEdict->CopyTo(&pClientCache);
			bWasTransmitToPlayer = true;
		} else if ( bWasTransmitToPlayer ) {
			pInfo->m_pTransmitEdict->Xor(pClientCache, &pClientCache);
			bWasTransmitToPlayer = false;
		}

		// entity needs no transmit
		if ( nFlags & FL_EDICT_DONTSEND )
			continue;
		
		// entity is already marked for sending
		if ( pInfo->m_pTransmitEdict->Get( iEdict ) )
			continue;

		if ( g_pDontTransmitCache.Get(iEdict) ) // Implements gmod's SetPreventTransmit but far faster.
			continue;
		
		if ( nFlags & FL_EDICT_ALWAYS )
		{
			// FIXME: Hey! Shouldn't this be using SetTransmit so as 
			// to also force network down dependent entities?
			while ( true )
			{
				// mark entity for sending
				pInfo->m_pTransmitEdict->Set( iEdict );
				g_pAlwaysTransmitCacheBitVec.Set( iEdict );
	
#ifndef _X360
				if ( bIsHLTV/* || bIsReplay*/ )
				{
					pInfo->m_pTransmitAlways->Set( iEdict );
				}
#endif	
				CCServerNetworkProperty *pEnt = static_cast<CCServerNetworkProperty*>( pEdict->GetNetworkable() );
				if ( !pEnt )
					break;

				CCServerNetworkProperty *pParent = pEnt->GetNetworkParent();
				if ( !pParent )
					break;

				pEdict = pParent->edict();
				iEdict = pEdict->m_EdictIndex;
			}
			continue;
		}

		// FIXME: Would like to remove all dependencies
		CBaseEntity *pEnt = g_pEntityCache[iEdict];
		// Assert( dynamic_cast< CBaseEntity* >( pEdict->GetUnknown() ) == pEnt );

#if !HOLYLIB_NETWORKING_PRECOMPUTEFULLCHECK
		if ( nFlags == FL_EDICT_FULLCHECK )
		{
			// do a full ShouldTransmit() check, may return FL_EDICT_CHECKPVS
			nFlags = pEnt->ShouldTransmit( pInfo );

			// Assert( !(nFlags & FL_EDICT_FULLCHECK) );

			if ( nFlags & FL_EDICT_ALWAYS )
			{
				pEnt->SetTransmit( pInfo, true );
				// g_pAlwaysTransmitCacheBitVec.Set( iEdict ); We do NOT do this since view models and such would also be included.
				continue;
			}	
		}
#endif

		// don't send this entity
		if ( !( nFlags & FL_EDICT_PVSCHECK ) )
			continue;

		CCServerNetworkProperty *netProp = static_cast<CCServerNetworkProperty*>( pEdict->GetNetworkable() );

#ifndef _X360
		if ( bIsHLTV/* || bIsReplay*/ )
		{
			// for the HLTV/Replay we don't cull against PVS
			if ( netProp->AreaNum() == skyBoxArea )
			{
				pEnt->SetTransmit( pInfo, true );
			}
			else
			{
				pEnt->SetTransmit( pInfo, false );
			}
			continue;
		}
#endif

		// Always send entities in the player's 3d skybox.
		// Sidenote: call of AreaNum() ensures that PVS data is up to date for this entity
		bool bSameAreaAsSky = netProp->AreaNum() == skyBoxArea;
		if ( bSameAreaAsSky )
		{
			pEnt->SetTransmit( pInfo, true );
			continue;
		}

		bool bInPVS = netProp->IsInPVS( pInfo );
		if ( bInPVS || bForceTransmit )
		{
			// only send if entity is in PVS
			pEnt->SetTransmit( pInfo, false );
			continue;
		}

		// If the entity is marked "check PVS" but it's in hierarchy, walk up the hierarchy looking for the
		//  for any parent which is also in the PVS.  If none are found, then we don't need to worry about sending ourself
		CCServerNetworkProperty *check = netProp->GetNetworkParent();

		// BUG BUG:  I think it might be better to build up a list of edict indices which "depend" on other answers and then
		// resolve them in a second pass.  Not sure what happens if an entity has two parents who both request PVS check?
		while ( check )
		{
			edict_t *checkEdict = check->edict();
			int checkIndex = checkEdict->m_EdictIndex;

			// Parent already being sent
			if ( pInfo->m_pTransmitEdict->Get( checkIndex ) )
			{
				pEnt->SetTransmit( pInfo, true );
				break;
			}

			// Parent isn't transmitted, so we also shouldn't be transmitted.
			// if ( g_pDontTransmitCache.Get( checkIndex ) )
			//	break;

			int checkFlags = checkEdict->m_fStateFlags & (FL_EDICT_DONTSEND|FL_EDICT_ALWAYS|FL_EDICT_PVSCHECK|FL_EDICT_FULLCHECK);
			if ( checkFlags & FL_EDICT_DONTSEND )
				break;

			if ( checkFlags & FL_EDICT_ALWAYS )
			{
				pEnt->SetTransmit( pInfo, true );
				break;
			}

			if ( checkFlags == FL_EDICT_FULLCHECK )
			{
				// do a full ShouldTransmit() check, may return FL_EDICT_CHECKPVS
				CBaseEntity *pCheckEntity = g_pEntityCache[checkIndex];
				nFlags = pCheckEntity->ShouldTransmit( pInfo );
				// Assert( !(nFlags & FL_EDICT_FULLCHECK) );
				if ( nFlags & FL_EDICT_ALWAYS )
				{
					pCheckEntity->SetTransmit( pInfo, true );
					pEnt->SetTransmit( pInfo, true );
				}
				break;
			}

			if ( checkFlags & FL_EDICT_PVSCHECK )
			{
				// Check pvs
				check->RecomputePVSInformation();
				bool bMoveParentInPVS = check->IsInPVS( pInfo );
				if ( bMoveParentInPVS )
				{
					pEnt->SetTransmit( pInfo, true );
					break;
				}
			}

			// Continue up chain just in case the parent itself has a parent that's in the PVS...
			check = check->GetNetworkParent();
		}
	}

	// HLTV has different networking! Some things might transmit when for normal players they wouldn't!
	// ObserverMode also influences how things are networked!
	if (bFastPath && !bIsHLTV && !(pRecipientPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pRecipientPlayer->GetObserverTarget()))
	{
		// Remove player's viewmodels from the cache since thoes are supposed to only be networked to the recipient player

		pInfo->m_pTransmitEdict->CopyTo(&g_pPlayerTransmitCacheBitVec[clientIndex]);
		CBitVec_AndNot(&g_pPlayerTransmitCacheBitVec[clientIndex], &pClientCache);
		g_iPlayerTransmitCacheAreaNum[clientIndex] = clientArea;
	}
	pInfo->m_pTransmitEdict->Or(g_bWasSeenByPlayer, &g_bWasSeenByPlayer);
//	Msg("A:%i, N:%i, F: %i, P: %i\n", always, dontSend, fullCheck, PVS );

	return true;
}

void SV_FillHLTVData( CFrameSnapshot *pSnapshot, edict_t *edict, int iValidEdict )
{
#if !defined( _XBOX )
	if ( pSnapshot->m_pHLTVEntityData && edict )
	{
		CHLTVEntityData *pHLTVData = &pSnapshot->m_pHLTVEntityData[iValidEdict];

		PVSInfo_t *pvsInfo = edict->GetNetworkable()->GetPVSInfo();

		if ( pvsInfo->m_nClusterCount == 1 )
		{
			// store cluster, if entity spawns only over one cluster
			pHLTVData->m_nNodeCluster = pvsInfo->m_pClusters[0];
		}
		else
		{
			// otherwise save PVS head node for larger entities
			pHLTVData->m_nNodeCluster = pvsInfo->m_nHeadNode | (1<<31);
		}

		// remember origin
		pHLTVData->origin[0] = pvsInfo->m_vCenter[0];
		pHLTVData->origin[1] = pvsInfo->m_vCenter[1];
		pHLTVData->origin[2] = pvsInfo->m_vCenter[2];
	}
#endif
}

static Symbols::PackWork_t_Process func_PackWork_t_Process;
static Symbols::SV_PackEntity func_SV_PackEntity;
struct PackWork_t
{
	int				nIdx;
	edict_t			*pEdict;
	CFrameSnapshot	*pSnapshot;

	static void Process( PackWork_t &item )
	{
#if SYSTEM_LINUX
		func_PackWork_t_Process(item);
#else
		func_SV_PackEntity( item.nIdx, item.pEdict, item.pSnapshot->m_pEntities[ item.nIdx ].m_pClass, item.pSnapshot );
#endif
	}
};

static Symbols::InvalidateSharedEdictChangeInfos func_InvalidateSharedEdictChangeInfos;
static ConVar* sv_parallel_packentities;
static Detouring::Hook detour_PackEntities_Normal;
void PackEntities_Normal(int clientCount, CGameClient **clients, CFrameSnapshot *snapshot)
{
	Assert( snapshot->m_nValidEntities >= 0 && snapshot->m_nValidEntities <= MAX_EDICTS );
	// tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s %d", __FUNCTION__, snapshot->m_nValidEntities );

	int workItemCount = 0;
	static PackWork_t workItems[MAX_EDICTS];
	/*
		Formerly used CUtlVectorFixed< PackWork_t, MAX_EDICTS > workItems(0, snapshot->m_nValidEntities);
		But there is no point in allocating the entire thing every time we call, instead we can keep it static and keep track of the count.

		Entries from previous frames will remain but that shouldn't be a issue,
		since we use workItemCount to keep track of how many entries we actually have for this update.
	*/

	if (!gpGlobals || (gpGlobals->tickcount != g_iLastCheckTransmit))
	{
		bool seen[MAX_EDICTS] = {false};
		for (int iClient = 0; iClient < clientCount; ++iClient)
		{
			CClientFrame *frame = clients[iClient]->m_pCurrentFrame;
			for (int iValidEdict = 0; iValidEdict < snapshot->m_nValidEntities; ++iValidEdict)
			{
				int index = snapshot->m_pValidEntities[iValidEdict];
				if (!seen[index] && frame->transmit_entity.Get(index))
				{
					seen[index] = true;
				}
			}
		}

		for (int iValidEdict = 0; iValidEdict < snapshot->m_nValidEntities; ++iValidEdict)
		{
			int index = snapshot->m_pValidEntities[iValidEdict];

			edict_t* edict = &world_edict[index];
			SV_FillHLTVData(snapshot, edict, iValidEdict);
			if (!seen[index])
				continue;

			PackWork_t& w = workItems[workItemCount++];
			w.nIdx = index;
			w.pEdict = edict;
			w.pSnapshot = snapshot;
		}
	} else {
		for (int iValidEdict = 0; iValidEdict < snapshot->m_nValidEntities; ++iValidEdict)
		{
			int index = snapshot->m_pValidEntities[iValidEdict];

			edict_t* edict = &world_edict[index];
			SV_FillHLTVData(snapshot, edict, iValidEdict);
			if (!g_bWasSeenByPlayer.IsBitSet(index))
				continue;

			PackWork_t& w = workItems[workItemCount++];
			w.nIdx = index;
			w.pEdict = edict;
			w.pSnapshot = snapshot;
		}
	}

	if (!sv_parallel_packentities)
	{
		sv_parallel_packentities = g_pCVar->FindVar("sv_parallel_packentities");
	}

	// Process work
	if ( sv_parallel_packentities && sv_parallel_packentities->GetBool() )
	{
#if ARCHITECTURE_X86
		ParallelProcess( "PackWork_t::Process", workItems, workItemCount, &PackWork_t::Process );
#else
		ParallelProcess( workItems, workItemCount, &PackWork_t::Process );
#endif
	}
	else
	{
		intp c = workItemCount;
		for ( intp i = 0; i < c; ++i )
		{
			PackWork_t &w = workItems[ i ];
#if SYSTEM_LINUX
			func_PackWork_t_Process(w);
#else
			func_SV_PackEntity( w.nIdx, w.pEdict, w.pSnapshot->m_pEntities[ w.nIdx ].m_pClass, w.pSnapshot );
#endif
		}
	}

	func_InvalidateSharedEdictChangeInfos();
}

/*void SV_ComputeClientPacks( 
	int clientCount, 
	CGameClient **clients,
	CFrameSnapshot *snapshot )
{
	MDLCACHE_CRITICAL_SECTION_(g_pMDLCache);

	{
		VPROF_BUDGET_FLAGS( "SV_ComputeClientPacks", "CheckTransmit", BUDGETFLAG_SERVER );

		for (int iClient = 0; iClient < clientCount; ++iClient)
		{
			CCheckTransmitInfo *pInfo = &clients[iClient]->m_PackInfo;
			clients[iClient]->SetupPackInfo( snapshot );
			Util::servergameents->CheckTransmit( pInfo, snapshot->m_pValidEntities, snapshot->m_nValidEntities );
			clients[iClient]->SetupPrevPackInfo();
		}
	}

	VPROF_BUDGET_FLAGS( "SV_ComputeClientPacks", "ComputeClientPacks", BUDGETFLAG_SERVER );

	// Fk local network backdoor, we expect holylib to rarely run on a local server so it's not worth to implement.

	PackEntities_Normal( clientCount, clients, snapshot );
}*/

void CNetworkingModule::OnEntityDeleted(CBaseEntity* pEntity)
{
	edict_t* pEdict = pEntity->edict();
	if (!pEdict)
		return;

	CleaupSetPreventTransmit(pEntity);
	int entIndex = pEdict->m_EdictIndex;
	g_pEntityCache[entIndex] = NULL;

	if (pEntity->IsPlayer())
	{
		g_pPlayerHandsEntity[entIndex-1] = NULL;
	} else {
		for (int iClient=0; iClient<MAX_PLAYERS; ++iClient)
		{
			if (g_pPlayerHandsEntity[iClient] == pEntity)
			{
				g_pPlayerHandsEntity[iClient] = NULL;
			}
		}
	}
}

void CNetworkingModule::OnEntityCreated(CBaseEntity* pEntity)
{
	//auto mod = GetOrCreateEntityModule<SendpropOverrideModule>(pEntity, "sendpropoverride");
	//mod->AddOverride(CallbackPluginCall, precalcIndex, prop, callbacks.size() - 1);
	edict_t* pEdict = pEntity->edict();
	if (pEdict)
	{
		g_pEntityCache[pEdict->m_EdictIndex] = pEntity;
	}
}

static SendTable* playerSendTable;
static ServerClass* playerServerClass;
static CFrameSnapshotManager* framesnapshotmanager = NULL;
static CSharedEdictChangeInfo* g_SharedEdictChangeInfo = nullptr;
static ServerClassCache *player_class_cache = nullptr;
static CStandardSendProxies* sendproxies;
static SendTableProxyFn datatable_sendtable_proxy;
PropTypeFns g_PropTypeFns[DPT_NUMSendPropTypes];
void CNetworkingModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	Plat_FastMemset(g_pEntityCache, 0, sizeof(g_pEntityCache));
	Plat_FastMemset(g_pPlayerHandsEntity, 0, sizeof(g_pPlayerHandsEntity));
	for (int i=0; i<MAX_PLAYERS; ++i)
		g_pShouldPrevent[i].ClearAll();

	SourceSDK::FactoryLoader engine_loader("engine");
	Detour::Create(
		&detour_AllocChangeFrameList, "AllocChangeFrameList",
		engine_loader.GetModule(), Symbols::AllocChangeFrameListSym,
		(void*)hook_AllocChangeFrameList, m_pID
	);

	Detour::Create(
		&detour_SendTable_CullPropsFromProxies, "SendTable_CullPropsFromProxies",
		engine_loader.GetModule(), Symbols::SendTable_CullPropsFromProxiesSym,
		(void*)hook_SendTable_CullPropsFromProxies, m_pID
	);

	SourceSDK::FactoryLoader server_loader("server");
	Detour::Create(
		&detour_CBaseEntity_GMOD_SetShouldPreventTransmitToPlayer, "CBaseEntity::GMOD_SetShouldPreventTransmitToPlayer",
		server_loader.GetModule(), Symbols::CBaseEntity_GMOD_SetShouldPreventTransmitToPlayerSym,
		(void*)hook_CBaseEntity_GMOD_SetShouldPreventTransmitToPlayer, m_pID
	);

	Detour::Create(
		&detour_CBaseEntity_GMOD_ShouldPreventTransmitToPlayer, "CBaseEntity::GMOD_ShouldPreventTransmitToPlayer",
		server_loader.GetModule(), Symbols::CBaseEntity_GMOD_ShouldPreventTransmitToPlayerSym,
		(void*)hook_CBaseEntity_GMOD_ShouldPreventTransmitToPlayer, m_pID
	);

	Detour::Create(
		&detour_CGMOD_Player_CreateViewModel, "CGMOD_Player::CreateViewModel",
		server_loader.GetModule(), Symbols::CGMOD_Player_CreateViewModelSym,
		(void*)hook_CGMOD_Player_CreateViewModel, m_pID
	);

	Detour::Create(
		&detour_Player__SetHands, "Player:SetHands",
		server_loader.GetModule(), Symbols::Player__SetHandsSym,
		(void*)hook_Player__SetHands, m_pID
	);

	Detour::Create(
		&detour_CBaseCombatCharacter_SetTransmit, "CBaseCombatCharacter::SetTransmit",
		server_loader.GetModule(), Symbols::CBaseCombatCharacter_SetTransmitSym,
		(void*)hook_CBaseCombatCharacter_SetTransmit, m_pID
	);

#if SYSTEM_LINUX
	Detour::Create(
		&detour_PackEntities_Normal, "PackEntities_Normal",
		engine_loader.GetModule(), Symbols::PackEntities_NormalSym,
		(void*)PackEntities_Normal, m_pID
	);
#endif

#if 0
	Detour::Create(
		&detour_SendTable_WritePropList, "SendTable_WritePropList",
		engine_loader.GetModule(), Symbols::SendTable_WritePropListSym,
		(void*)hook_SendTable_WritePropList, m_pID
	);
	SendTable_WritePropList_func = detour_SendTable_WritePropList.GetTrampoline<Symbols::SendTable_WritePropList>();

	Detour::Create(
		&detour_CBaseServer_WriteDeltaEntities, "CBaseServer::WriteDeltaEntities",
		engine_loader.GetModule(), Symbols::CBaseServer_WriteDeltaEntitiesSym,
		(void*)hook_CBaseServer_WriteDeltaEntities, m_pID
	);
	CBaseServer_WriteDeltaEntities_func = detour_CBaseServer_WriteDeltaEntities.GetTrampoline<Symbols::CBaseServer_WriteDeltaEntities>();

	Detour::Create(
		&detour_SV_DetermineUpdateType, "SV_DetermineUpdateType",
		engine_loader.GetModule(), Symbols::SV_DetermineUpdateTypeSym,
		(void*)hook_SV_DetermineUpdateType, m_pID
	);
	SV_DetermineUpdateType_func = detour_SV_DetermineUpdateType.GetTrampoline<Symbols::SV_DetermineUpdateType>();

	Detour::Create(
		&detour_PackedEntity_GetPropsChangedAfterTick, "PackedEntity::GetPropsChangedAfterTick",
		engine_loader.GetModule(), Symbols::PackedEntity_GetPropsChangedAfterTickSym,
		(void*)hook_PackedEntity_GetPropsChangedAfterTick, m_pID
	);
	PackedEntity_GetPropsChangedAfterTick_func = detour_PackedEntity_GetPropsChangedAfterTick.GetTrampoline<Symbols::PackedEntity_GetPropsChangedAfterTick>();

	Detour::Create(
		&detour_CGameServer_SendClientMessages, "CGameServer::SendClientMessages",
		engine_loader.GetModule(), Symbols::CGameServer_SendClientMessagesSym,
		(void*)hook_CGameServer_SendClientMessages, m_pID
	);
	CGameServer_SendClientMessages_func = detour_CGameServer_SendClientMessages.GetTrampoline<Symbols::CGameServer_SendClientMessages>();
#endif

	framesnapshotmanager = Detour::ResolveSymbol<CFrameSnapshotManager>(engine_loader, Symbols::g_FrameSnapshotManagerSym);
	Detour::CheckValue("get class", "framesnapshotmanager", framesnapshotmanager != NULL);

	PropTypeFns* pPropTypeFns = Detour::ResolveSymbol<PropTypeFns>(engine_loader, Symbols::g_PropTypeFnsSym);
	Detour::CheckValue("get class", "pPropTypeFns", pPropTypeFns != NULL);

	g_BSPData = Detour::ResolveSymbol<CCollisionBSPData>(engine_loader, Symbols::g_BSPDataSym);
	Detour::CheckValue("get class", "CCollisionBSPData", g_BSPData != NULL);
	
	if (pPropTypeFns)
	{
		for (size_t i = 0; i < DPT_NUMSendPropTypes; ++i)
			g_PropTypeFns[i] = pPropTypeFns[i]; // Crash any% speed run. I don't believe this will work
	}

	SourceSDK::FactoryLoader datacache_loader("datacache");
	mdlcache = datacache_loader.GetInterface<IMDLCache>(MDLCACHE_INTERFACE_VERSION);

#if SYSTEM_WINDOWS
	func_SV_PackEntity = (Symbols::SV_PackEntity)Detour::GetFunction(engine_loader.GetModule(), Symbols::SV_PackEntitySym);
	Detour::CheckFunction((void*)func_SV_PackEntity, "SV_PackEntity");
#endif

	func_InvalidateSharedEdictChangeInfos = (Symbols::InvalidateSharedEdictChangeInfos)Detour::GetFunction(engine_loader.GetModule(), Symbols::InvalidateSharedEdictChangeInfosSym);
	Detour::CheckFunction((void*)func_InvalidateSharedEdictChangeInfos, "InvalidateSharedEdictChangeInfos");

#if SYSTEM_LINUX
	func_PackWork_t_Process = (Symbols::PackWork_t_Process)Detour::GetFunction(engine_loader.GetModule(), Symbols::PackWork_t_ProcessSym);
	Detour::CheckFunction((void*)func_PackWork_t_Process, "PackWork_t::Process");
#endif

	func_CBasePlayer_GetViewModel = (Symbols::CBasePlayer_GetViewModel)Detour::GetFunction(server_loader.GetModule(), Symbols::CBasePlayer_GetViewModelSym);
	Detour::CheckFunction((void*)func_CBasePlayer_GetViewModel, "CBasePlayer::GetViewModel");

	func_CBaseAnimating_SetTransmit = (Symbols::CBaseCombatCharacter_SetTransmit)Detour::GetFunction(server_loader.GetModule(), Symbols::CBaseAnimating_SetTransmitSym);
	Detour::CheckFunction((void*)func_CBaseAnimating_SetTransmit, "CBaseAnimating::SetTransmit");

	func_GetCurrentSkyCamera = (Symbols::GetCurrentSkyCamera)Detour::GetFunction(server_loader.GetModule(), Symbols::GetCurrentSkyCameraSym);
	Detour::CheckFunction((void*)func_GetCurrentSkyCamera, "GetCurrentSkyCamera");

#if SYSTEM_WINDOWS // BUG: On Windows IModule::ServerActivate is not called if HolyLib gets loaded using: require("holylib")
	world_edict = Util::engineserver->PEntityOfEntIndex(0);
#endif

	g_pReplaceCServerGameEnts_CheckTransmit = true;
}

void CNetworkingModule::ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
	sv_force_transmit_ents = g_pCVar->FindVar("sv_force_transmit_ents");

	// Find player class (has DT_BasePlayer as a baseclass table)
	// We do this in ServerActivate since the engine only now hooked into the ServerClass allowing us to safely use them now.
	g_SharedEdictChangeInfo = Util::engineserver->GetSharedEdictChangeInfo();
	for(ServerClass *serverclass = Util::servergamedll->GetAllServerClasses(); serverclass->m_pNext != nullptr; serverclass = serverclass->m_pNext) {
		for (int i = 0; i < serverclass->m_pTable->GetNumProps(); i++) {
			if (serverclass->m_pTable->GetProp(i)->GetDataTable() != nullptr && strcmp(serverclass->m_pTable->GetProp(i)->GetDataTable()->GetName(), "DT_BasePlayer") == 0 ) {
				playerSendTable = serverclass->m_pTable;
				playerServerClass = serverclass;
			}
		}
	}

	world_edict = Util::engineserver->PEntityOfEntIndex(0);
	sendproxies = Util::servergamedll->GetStandardSendProxies();
	datatable_sendtable_proxy = sendproxies->m_DataTableToDataTable;
	local_sendtable_proxy = sendproxies->m_SendLocalDataTable;
	player_local_exclusive_send_proxy = new bool[playerSendTable->m_pPrecalc ? playerSendTable->m_pPrecalc->m_nDataTableProxies: 255];
	for(ServerClass *serverclass = Util::servergamedll->GetAllServerClasses(); serverclass->m_pNext != nullptr; serverclass = serverclass->m_pNext)
	{
		//DevMsg("Crash1\n");
		SendTable *sendTable = serverclass->m_pTable;
		auto serverClassCache = new ServerClassCache();
		if (sendTable == playerSendTable) 
			player_class_cache = serverClassCache;

		// Reuse unused variable
		sendTable->m_pPrecalc->m_pDTITable = (CDTISendTable*)serverClassCache;
		int propcount = sendTable->m_pPrecalc->m_Props.Count();
		//DevMsg("%s %d %d\n", pTable->GetName(), propcount, pTable->GetNumProps());
				
		CPropMapStack pmStack( sendTable->m_pPrecalc, sendproxies );
		serverClassCache->prop_offsets = new unsigned short[propcount];
		//serverClassCache.send_nodes = new CSendNode *[playerSendTable->m_pPrecalc->m_nDataTableProxies];
		pmStack.Init();

		//int reduce_coord_prop_offset = 0;

		//DevMsg("Crash2\n");
		int t = 0;
		PropScan(0,sendTable, t);
		unsigned char proxyStack[256];

		RecurseStack(*serverClassCache, proxyStack, &sendTable->m_pPrecalc->m_Root , sendTable->m_pPrecalc);
		serverClassCache->prop_cull = new unsigned char[sendTable->m_pPrecalc->m_Props.Count()];
		serverClassCache->prop_propproxy_first = new unsigned short[sendTable->m_pPrecalc->m_nDataTableProxies];
		for (int i = 0; i < sendTable->m_pPrecalc->m_nDataTableProxies; i++)
			serverClassCache->prop_propproxy_first[i] = INVALID_PROP_INDEX;

		for (int iToProp = 0; iToProp < sendTable->m_pPrecalc->m_Props.Count(); iToProp++)
		{ 
			const SendProp *pProp = sendTable->m_pPrecalc->m_Props[iToProp];

			pmStack.SeekToProp( iToProp );

			//player_local_exclusive_send_proxy[proxyStack[sendTable->m_pPrecalc->m_PropProxyIndices[iToProp]]] = player_prop_cull[iToProp] < 254 && pProp->GetDataTableProxyFn() == sendproxies->m_SendLocalDataTable;
					
			//bool local2 = pProp->GetDataTableProxyFn() == sendproxies->m_SendLocalDataTable;
			// bool local = player_local_exclusive_send_proxy[player_prop_cull[iToProp]];
			//Msg("Local %s %d %d %d %d\n",pProp->GetName(), local, local2, sendproxies->m_SendLocalDataTable, pProp->GetDataTableProxyFn());

			auto dataTableIndex = proxyStack[sendTable->m_pPrecalc->m_PropProxyIndices[iToProp]];
			serverClassCache->prop_cull[iToProp] = dataTableIndex;
			if (dataTableIndex < sendTable->m_pPrecalc->m_nDataTableProxies)
				serverClassCache->prop_propproxy_first[dataTableIndex] = iToProp;

			if ((intptr_t)pmStack.GetCurStructBase() != 0)
			{
				int offset = pProp->GetOffset() + (intptr_t)pmStack.GetCurStructBase() - 1;
						
				int elementCount = 1;
				int elementStride = 0;
				int propIdToUse = iToProp;
				int offsetToUse = offset;
				//auto arrayProp = pProp;
				if ( pProp->GetType() == DPT_Array )
				{
					offset = pProp->GetArrayProp()->GetOffset() + (intptr_t)pmStack.GetCurStructBase() - 1;
					elementCount = pProp->m_nElements;
					elementStride = pProp->m_ElementStride;
					pProp = pProp->GetArrayProp();
					offsetToUse = (intptr_t)pmStack.GetCurStructBase() - 1;
				}

				serverClassCache->prop_offsets[propIdToUse] = offsetToUse;

				//if (pProp->GetType() == DPT_Vector || pProp->GetType() == DPT_Vector )
				//	propIndex |= PROP_INDEX_VECTOR_ELEM_MARKER;
						
				if (offset != 0/*IsStandardPropProxy(pProp->GetProxyFn())*/)
				{
					if (offset != 0)
					{
						int offset_off = offset;
						for ( int j = 0; j < elementCount; j++ )
						{
							AddOffsetToList(*serverClassCache, offset_off, propIdToUse, j);
							if (pProp->GetType() == DPT_Vector) {
								AddOffsetToList(*serverClassCache, offset_off + 4, propIdToUse, j);
								AddOffsetToList(*serverClassCache, offset_off + 8, propIdToUse, j);
							} else if (pProp->GetType() == DPT_VectorXY) {
								AddOffsetToList(*serverClassCache, offset_off + 4, propIdToUse, j);
							}
							offset_off += elementStride;
						}
					}
				} else {
					// if (sendTable == playerSendTable) {
					//	 Msg("prop %d %s %d\n", iToProp, pProp->GetName(), offset);
					// }
					serverClassCache->prop_special.push_back({propIdToUse});
				}
			} else {
				auto &datatableSpecial = serverClassCache->datatable_special[sendTable->m_pPrecalc->m_DatatableProps[pmStack.m_pIsPointerModifyingProxy[sendTable->m_pPrecalc->m_PropProxyIndices[iToProp]]->m_iDatatableProp]];
						
				//serverClassCache->prop_special.push_back({iToProp, pmStack.m_iBaseOffset[sendTable->m_pPrecalc->m_PropProxyIndices[iToProp]], pProp, pmStack.m_pIsPointerModifyingProxy[sendTable->m_pPrecalc->m_PropProxyIndices[iToProp]]});
				datatableSpecial.propIndexes.push_back(iToProp);
				datatableSpecial.baseOffset = pmStack.m_iBaseOffset[sendTable->m_pPrecalc->m_PropProxyIndices[iToProp]];

				serverClassCache->prop_offsets[iToProp] = pProp->GetOffset();
			}
			//Msg("Data table for %d %s %d is %d %d %d %d\n", iToProp, pProp->GetName(),  pProp->GetOffset() + (int)pmStack.GetCurStructBase() - 1, (int)pmStack.GetCurStructBase(), pProp->GetProxyFn(), pProp->GetDataTableProxyFn(), pmStack.m_pIsPointerModifyingProxy[sendTable->m_pPrecalc->m_PropProxyIndices[iToProp]]) ;

			//int bitsread_pre = toBits.GetNumBitsRead();

			/*if (pProp->GetFlags() & SPROP_COORD_MP)) {
				if ((int)pmStack.GetCurStructBase() != 0) {
					reduce_coord_prop_offset += toBits.GetNumBitsRead() - bitsread_pre;
					player_prop_coord.push_back(iToProp);
					Msg("bits: %d\n", toBits.GetNumBitsRead() - bitsread_pre);
				}
			}*/
		}
	}
}

extern CGlobalVars *gpGlobals;
void CNetworkingModule::Shutdown()
{
	g_pReplaceCServerGameEnts_CheckTransmit = false;

	if (!framesnapshotmanager) // If we failed, we failed
	{
		Msg(PROJECT_NAME ": Failed to find framesnapshotmanager. Unable to fully unload!\n");
		return;
	}

	/*
	 * The code below to unload also belongs to sigsegv
	 * Source: https://github.com/rafradek/sigsegv-mvm/blob/e6a6cee305023f36e5b914872500ef8319317d71/src/mod/perf/sendprop_optimize.cpp#L1981-L2002
	 */
	for (CFrameSnapshot* pSnapshot : framesnapshotmanager->m_FrameSnapshots)
	{
		for (int i=0; i<pSnapshot->m_nNumEntities; ++i)
		{
			CFrameSnapshotEntry* pSnapshotEntry = pSnapshot->m_pEntities + i;
			if (!pSnapshotEntry)
				continue;

			PackedEntity* pPackedEntity = reinterpret_cast<PackedEntity*>(pSnapshotEntry->m_pPackedData);
			if (!pPackedEntity || !pPackedEntity->m_pChangeFrameList)
				continue;

			pPackedEntity->m_pChangeFrameList->Release();
			pPackedEntity->m_pChangeFrameList = detour_AllocChangeFrameList.GetTrampoline<Symbols::AllocChangeFrameList>()(pPackedEntity->m_pServerClass->m_pTable->m_pPrecalc->m_Props.Count(), gpGlobals->tickcount);
		}
	}

	// ToDo: Fix this crash. pPackedEntity will be invalid and it crashes when trying to access it's member.
	for (int i=0; i<MAX_EDICTS; ++i)
	{
		PackedEntity* pPackedEntity = reinterpret_cast<PackedEntity*>(framesnapshotmanager->m_pPackedData[i]);
		if (!pPackedEntity || !pPackedEntity->m_pChangeFrameList)
			continue;

		pPackedEntity->m_pChangeFrameList->Release();
		pPackedEntity->m_pChangeFrameList = detour_AllocChangeFrameList.GetTrampoline<Symbols::AllocChangeFrameList>()(pPackedEntity->m_pServerClass->m_pTable->m_pPrecalc->m_Props.Count(), gpGlobals->tickcount);
	}
}