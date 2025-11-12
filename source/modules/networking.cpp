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
#include "sourcesdk/GameEventManager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CNetworkingModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn);
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual void OnEntityCreated(CBaseEntity* pEntity) OVERRIDE;
	virtual void OnEntityDeleted(CBaseEntity* pEntity) OVERRIDE;
	virtual void ClientDisconnect(edict_t* pClient) OVERRIDE;
	virtual void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax) OVERRIDE;
	virtual const char* Name() { return "networking"; };
	virtual int Compatibility() { return LINUX32; }; // ToDo: Fix CBaseClient offset being broken on 64x causing the access to CGameClient::m_pCurrentFrame to return a invalid pointer
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
// sigsegv part

static Detouring::Hook detour_AllocChangeFrameList;
static IChangeFrameList* hook_AllocChangeFrameList(int nProperties, int iCurTick)
{
	VPROF_BUDGET("AllocChangeFrameList", VPROF_BUDGETGROUP_OTHER_NETWORKING);
	CChangeFrameList* pRet = new CChangeFrameList;
	pRet->Init(nProperties, iCurTick);

	return pRet;
}

// -------------------------------------------------------------------------------------------------
// HolyLib part

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
// HolyLib part

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
	for (int i = 0; i <nStartProps; ++i) {
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
	for (int i = 0; i < size; ++i) {
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
	for (int i = 0; i < node->m_Children.Count(); ++i) {
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
		for (int i = 0; i < nCheckProps; ++i) {
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
	if (!g_BSPData) // WEH!
		return Util::engineserver->CheckAreasConnected(area1, area2);

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
		for ( i=0; i< pInfo->m_AreasNetworked; ++i )
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
		for ( i=0; i< pInfo->m_AreasNetworked; ++i )
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

// -------------------------------------------------------------------------------------------------
// HolyLib part

// ToDo: Move this into util.h
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

// Offset & helper functins

static DTVarByOffset m_Local_Offset("DT_LocalPlayerExclusive", "m_Local");
static DTVarByOffset m_SkyBox3DArea_Offset("DT_Local", "m_skybox3d.area");
static inline int GetSkybox3DArea(const void* pPlayer) // Fully safe access :3
{
	void* pLocal = m_Local_Offset.GetPointer(pPlayer);
	if (!pLocal)
		return 255; // 255 is the default max value used so we just fallback to that.

	void* pSkybox3DArea = m_SkyBox3DArea_Offset.GetPointer(pLocal);
	if (!pSkybox3DArea)
		return 255;

	return *(int*)pSkybox3DArea;
}

static inline CBaseEntity* IndexToEntity(int nEntIndex)
{
	if (nEntIndex < 0 || nEntIndex > MAX_EDICTS)
		return nullptr;

	return g_pEntityCache[nEntIndex];
}

static DTVarByOffset m_Hands_Offset("DT_GMOD_Player", "m_Hands");
static inline CBaseEntity* GetGMODPlayerHands(const void* pPlayer)
{
	return IndexToEntity(((CBaseHandle*)m_Hands_Offset.GetPointer(pPlayer))->GetEntryIndex());
}

static DTVarByOffset m_hActiveWeapon_Offset("DT_BaseCombatCharacter", "m_hActiveWeapon");
static inline CBaseEntity* GetActiveWeapon(const void* pPlayer)
{
	return IndexToEntity(((CBaseHandle*)m_hActiveWeapon_Offset.GetPointer(pPlayer))->GetEntryIndex());
}

static DTVarByOffset m_hMyWeapons_Offset("DT_BaseCombatCharacter", "m_hMyWeapons", sizeof(CBaseCombatWeaponHandle));
static inline CBaseEntity* GetMyWeapon(const void* pPlayer, int nWeaponSlot)
{
	return IndexToEntity(((CBaseCombatWeaponHandle*)m_hMyWeapons_Offset.GetPointerArray(pPlayer, nWeaponSlot))->GetEntryIndex());
}

static DTVarByOffset m_hViewModel_Offset("DT_BasePlayer", "m_hViewModel", sizeof(CBasePlayer::CBaseViewModelHandle));
static inline CBaseViewModel* GetViewModel(const void* pPlayer, int nViewModelSlot)
{
	return (CBaseViewModel*)IndexToEntity(((CBasePlayer::CBaseViewModelHandle*)m_hViewModel_Offset.GetPointerArray(pPlayer, nViewModelSlot))->GetEntryIndex());
}

/*
	Planned setup:
	- Have a system to split everything into 4 groups.
	-> Always Transmit, PVS Transmit, Check Transmit, No Transmit

	- Keep a cache across ticks that contains the visleaf the entity is inside
	-> used only for entities using PVS Transmit
	-> If changed then we could simply check again if the Entity is in the player's current PVS
	-> If enters/leaves pvs we simply update all stuff

	- Check Transmit is the only list using which entities will have their PVS checked every tick.
	-> Always Transmit and No Transmit are applied before iteration

	Current state:
	I still have not really any good plan/idea for this and just write random stuff and see how it'll go
*/
#define NETWORKING_USE_ENTITYCACHE 1
static ConVar networking_bind_gmodhands_to_player("holylib_networking_bind_gmodhands_to_player", "1", 0, "Experimental - If enabled, the GMOD Hands / Player:SetHands entity will be bound to the player and only networked if the player is networked");
static ConVar networking_bind_viewmodels_to_player("holylib_networking_bind_viewmodels_to_player", "1", 0, "Experimental - If enabled, the viewmodels will be bound to the player and only networked if the player is networked");
static ConVar networking_cachedump("holylib_networking_cachedump", "0", 0, "Experimental - Debug. You wouldn't need this...");
static ConVar networking_areasplit("holylib_networking_areasplit", "1", 0, "Experimental - PVS entities are split into areas");
struct EntityTransmitCache // Well.... Still kinda acts as a tick-based cache, though its a good foundation now.
{
	// Updates the cache for the current tick
	void UpdateEntities(const unsigned short *pEdictIndices, int nEdicts)
	{
		m_bIsActivelyNetworking = true;

		Plat_FastMemset(&pAlwaysTransmitBits, 0, sizeof(pAlwaysTransmitBits) * 4); // Again, very "safe"
		//pAlwaysTransmitBits.ClearAll();
		//pNeverTransmitBits.ClearAll();
		//pPVSTransmitBits.ClearAll();
		//pFullTransmitBits.ClearAll();

		nPVSEdictCount = -1;
		nFullEdictCount = -1; // -1 so that we can use preincrement :hehe:
		Plat_FastMemset(pPVSEntityList, 0, sizeof(pPVSEntityList) * 2); // * 2 to also clear nFullEdictList which lies directly after it in memory. I know. very "safe" but I want this in 1 call
		Plat_FastMemset(nAreaEntities, 0, sizeof(nAreaEntities));

		// First we build data based off all players
		bool bBindGmodHandsToPlayer = networking_bind_gmodhands_to_player.GetBool();
		bool bBindViewModelsToPlayer = networking_bind_viewmodels_to_player.GetBool();
		for (int iPlayerIndex = 1; iPlayerIndex <= gpGlobals->maxClients; ++iPlayerIndex)
		{
			CBaseEntity* pPlayer = g_pEntityCache[iPlayerIndex];
			if (!pPlayer)
				continue;

			if (bBindGmodHandsToPlayer)
			{
				CBaseEntity* pHands = GetGMODPlayerHands(pPlayer);
				if (pHands)
					pNeverTransmitBits.Set(pHands->edict()->m_EdictIndex); // We make hands never transmit by default simply to save performance by reducing PVS checks.
			}

			if (bBindViewModelsToPlayer)
			{
				for (int i=0; i<MAX_VIEWMODELS; ++i)
				{
					CBaseEntity* pViewModel = GetViewModel(pPlayer, i);
					if (pViewModel)
						pNeverTransmitBits.Set(pViewModel->edict()->m_EdictIndex);
				}
			}

			// Now let's also mark all weapons a player has so that they won't later enter into useless PVS checks.
			for (int i=0; i<MAX_WEAPONS; ++i)
			{
				CBaseEntity *pWeapon = GetMyWeapon(pPlayer, i);
				if (pWeapon)
					pNeverTransmitBits.Set(pWeapon->edict()->m_EdictIndex);
			}
		}

		for (int i=0; i < nEdicts; ++i)
		{
			int iEdict = pEdictIndices[i];
			edict_t *pEdict = &world_edict[iEdict];

			if (pNeverTransmitBits.IsBitSet(iEdict))
				continue;

			int nFlags = pEdict->m_fStateFlags & (FL_EDICT_DONTSEND|FL_EDICT_ALWAYS|FL_EDICT_PVSCHECK|FL_EDICT_FULLCHECK);
			if (nFlags & FL_EDICT_DONTSEND)
			{
				pNeverTransmitBits.Set(iEdict);
				continue;
			}

			if (nFlags & FL_EDICT_ALWAYS)
			{
				while (true)
				{
					pAlwaysTransmitBits.Set(iEdict);

					CCServerNetworkProperty *pEnt = static_cast<CCServerNetworkProperty*>(pEdict->GetNetworkable());
					if (!pEnt)
						break;

					CCServerNetworkProperty *pParent = pEnt->GetNetworkParent();
					if (!pParent)
						break;

					pEdict = pParent->edict();
					iEdict = pEdict->m_EdictIndex;
				}
				continue;
			}

			CBaseEntity* pEnt = g_pEntityCache[iEdict];
			if (pEnt)
			{
				if (pEnt->edict() != pEdict)
					Warning(PROJECT_NAME " - networking: Entity chache is unreliable! We are cooked!\n");

				if (nFlags == FL_EDICT_FULLCHECK)
				{
					pFullEntityList[++nFullEdictCount] = pEnt;
					pFullTransmitBits.Set(iEdict);
					continue;
				}

				if (nFlags == FL_EDICT_PVSCHECK)
				{
					if (networking_areasplit.GetBool())
					{
						AddPVSEntity(pEnt);
					} else {
						pPVSEntityList[++nPVSEdictCount] = pEnt;
					}
					pPVSTransmitBits.Set(iEdict);
					continue;
				}
			}

			// It remained? So never send it!
			pNeverTransmitBits.Set(iEdict);
		}

		if (networking_cachedump.GetBool())
		{
			Msg("Fullcheck:\n");
			for (int i=0; i<nFullEdictCount; ++i)
				Msg("    %i: %s[%i]\n", i, pFullEntityList[i]->GetClassname(), pFullEntityList[i]->edict()->m_EdictIndex);

			Msg("PVS:\n");
			for (int i=0; i<nPVSEdictCount; ++i)
				Msg("    %i: %s[%i]\n", i, pPVSEntityList[i]->GetClassname(), pPVSEntityList[i]->edict()->m_EdictIndex);

			if (networking_areasplit.GetBool())
			{
				for (int nArea = 0; nArea<MAX_MAP_AREAS-1; ++nArea)
				{
					AreaCache& pArea = nAreaEntities[nArea];
					if (pArea.nCount == 0)
						continue;

					Msg("Area %i [%i]:\n", nArea+1, pArea.nCount);
					for (int i=0; i<pArea.nCount; ++i)
						Msg("    %i: %s[%i]\n", i, pArea.pEntities[i]->GetClassname(), pArea.pEntities[i]->edict()->m_EdictIndex);
				}
			}

			networking_cachedump.SetValue(0);
		}
	}

	void EntityRemoved(CBaseEntity* pEntity, edict_t* pEdict)
	{
		// Not networking? Then we can easily skip all this since everything is cleared on the next transmit anyways
		if (!m_bIsActivelyNetworking)
			return;

		// Deleted during networking? Now you fked up.

		int nIndex = pEdict->m_EdictIndex;
		pAlwaysTransmitBits.Clear(nIndex);
		pNeverTransmitBits.Clear(nIndex);
		pPVSTransmitBits.Clear(nIndex);
		pFullTransmitBits.Clear(nIndex);

		nEntityCluster[nIndex] = 0;
		bDirtyEntities.Clear(nIndex);

		for (int i = 0; i<nFullEdictCount; ++i)
		{
			CBaseEntity* pFullEnt = pFullEntityList[i];
			if (pFullEnt != pEntity)
				continue;

			if (i < nFullEdictCount - 1)
				memmove(&pFullEntityList[i], &pFullEntityList[i + 1], (nFullEdictCount - i - 1) * sizeof(CBaseEntity*));

			--nFullEdictCount;
			pFullEntityList[nFullEdictCount] = NULL;
			break;
		}

		for (int i = 0; i<nPVSEdictCount; ++i)
		{
			CBaseEntity* pFullEnt = pFullEntityList[i];
			if (pFullEnt != pEntity)
				continue;

			if (i < nPVSEdictCount - 1)
				memmove(&pPVSEntityList[i], &pPVSEntityList[i + 1], (nPVSEdictCount - i - 1) * sizeof(CBaseEntity*));

			--nPVSEdictCount;
			pPVSEntityList[nPVSEdictCount] = NULL;
			break;
		}

		for (int nArea = 0; nArea<MAX_MAP_AREAS-1; ++nArea)
		{
			AreaCache& pArea = nAreaEntities[nArea];
			if (pArea.nCount == 0)
				continue;

			for (int i=0; i<pArea.nCount; ++i)
			{
				if (pArea.pEntities[i] != pEntity)
					continue;

				if (i < pArea.nCount - 1)
					memmove(&pArea.pEntities[i], &pArea.pEntities[i + 1], (pArea.nCount - i - 1) * sizeof(CBaseEntity*));

				--pArea.nCount;
				pArea.pEntities[pArea.nCount] = NULL;
				break;
			}
		}

		DevMsg(PROJECT_NAME " - networking: An entity (class: %s) was deleted during networking! This is utterly expensive, stop this >:(\n", pEntity->GetClassname());
	}

	void AddPVSEntity(CBaseEntity* pEntity)
	{
#if 0
		int nArea = Util::engineserver->GetArea(pEntity->GetAbsOrigin());
		if (nArea == 0)
		{
			// Pray that this fallback works
			nArea = Util::engineserver->GetArea(pEntity->WorldSpaceCenter());
		}
#endif
		const CCollisionProperty* pCollision = pEntity->CollisionProp();
		if (!pCollision)
		{
			int nArea = Util::engineserver->GetArea(pEntity->GetAbsOrigin());
			
			if (nArea >= MAX_MAP_AREAS || nArea <= 0)
			{
				pPVSEntityList[++nPVSEdictCount] = pEntity;
				return;
			}

			// We do -1 since nArea 0 is not actually an valid area
			AreaCache& pArea = nAreaEntities[nArea-1];
			if (pArea.nCount >= 512)
			{
				pPVSEntityList[++nPVSEdictCount] = pEntity;
				return;
			}

			pArea.pEntities[pArea.nCount++] = pEntity;
			return;
		}

		Vector &vecResult = AllocTempVector();
		pCollision->CollisionToWorldSpace( pCollision->OBBMins(), &vecResult );
		int minsArea = Util::engineserver->GetArea(vecResult);
		bool bMinsValid = minsArea < MAX_MAP_AREAS && minsArea > 0; // Area 0 is also invalid, thats why > 0

		pCollision->CollisionToWorldSpace( pCollision->OBBMaxs(), &vecResult );
		int maxsArea = Util::engineserver->GetArea(vecResult);
		bool bMaxsValid = maxsArea < MAX_MAP_AREAS && maxsArea > 0;

		if (!bMinsValid && !bMaxsValid)
		{
			// Test center, in case you have things like a vent door where the mins/maxs may be out of bounds but not the center
			pCollision->CollisionToWorldSpace( pCollision->OBBCenter(), &vecResult );
			minsArea = Util::engineserver->GetArea(vecResult);
			bMinsValid = minsArea < MAX_MAP_AREAS && minsArea > 0;
		}

		if (!bMinsValid && !bMaxsValid)
		{
			pPVSEntityList[++nPVSEdictCount] = pEntity;
			return;
		}

		if (bMinsValid)
		{
			AddEntityToArea(minsArea, pEntity);
		}

		// IMPORTANT: This entity stretches across to another area! Probably a door with an areaportal!
		//            If it crosses more than two areas, GG. Not our problem
		if (bMaxsValid && minsArea != maxsArea)
		{
			// We add them here too, an entity can be in two areas at once for our case
			AddEntityToArea(maxsArea, pEntity);
		}
	}

	inline void AddEntityToArea(int areaNum, CBaseEntity* pEntity)
	{
		// We do -1 since nArea 0 is not actually an valid area and we shifted all by 1
		AreaCache& pArea = nAreaEntities[areaNum-1];
		if (pArea.nCount >= 512)
		{
			pPVSEntityList[++nPVSEdictCount] = pEntity;
			return;
		}

		pArea.pEntities[pArea.nCount++] = pEntity;
	}

	inline void FinishNetworking()
	{
		m_bIsActivelyNetworking = false;
	}

	bool m_bIsActivelyNetworking = false;

	CBitVec<MAX_EDICTS> pAlwaysTransmitBits;
	CBitVec<MAX_EDICTS> pNeverTransmitBits;
	CBitVec<MAX_EDICTS> pPVSTransmitBits;
	CBitVec<MAX_EDICTS> pFullTransmitBits;

	int nEntityCluster[MAX_EDICTS] = {0};
	CBitVec<MAX_EDICTS> bDirtyEntities = {false}; // Their Cluster changed compared to last tick.

	int nPVSEdictCount = -1; // NOTE: This will only contain entities that were unable to be fitted into an AreaCache! (only happens on overflow)
	int nFullEdictCount = -1;
	CBaseEntity* pPVSEntityList[MAX_EDICTS] = {NULL};
	CBaseEntity* pFullEntityList[MAX_EDICTS] = {NULL};

	/*
		If holylib_networking_areasplit is enabled
		we split all PVS entities into Areas
		This allows us to first check if an area even connects to our and only if so, we check the entities inside
		this save a LOT of performance as we heavily reduce PVS checks by checking areas first

		ToDo: Think about if we should move Areas with 1-2 entities into the nPVSEntityList simply because its probably quicker to do a PVS check than area? (verify)
	*/
	struct AreaCache
	{
		int nCount = 0;
		CBaseEntity* pEntities[512];
	};

	AreaCache nAreaEntities[MAX_MAP_AREAS-1]; // -1 since Area 0 is not a valid one so we save some bytes
};
static EntityTransmitCache g_nEntityTransmitCache;

// Full cache persisting across ticks, reset only when the player disconnects.
static ConVar networking_transmit_newweapons("holylib_networking_transmit_newweapons", "1", 0, "Experimental - If enabled, weapons that a player equipped/was given are networked for the first x ticks");
static ConVar networking_transmit_ticks("holylib_networking_transmit_ticks", "100", 0, "Experimental - How many ticks to use for transmit_newweapons & transmit_onfullupdate");
static ConVar networking_transmit_onfullupdate("holylib_networking_transmit_onfullupdate", "1", 0, "Experimental - If enabled, players and their own weapons are transmitted for the first x ticks when they had a full update");
static ConVar networking_transmit_onfullupdate_networktoothers("holylib_networking_transmit_onfullupdate_networktoothers", "1", 0, "Experimental - If enabled, any player that has a full update will be networked to everyone");
struct PlayerTransmitCache
{
	inline void NextTick(const CBaseEntity* pPlayer, const int nTick)
	{
		int nTransmitTicks = networking_transmit_ticks.GetInt();
		for (int i=0; i<MAX_WEAPONS; ++i)
		{
			WeaponSlot& pSlot = pWeapons[i];
			CBaseEntity *pWeapon = GetMyWeapon(pPlayer, i);
			if (pWeapon)
			{
				if (!pSlot.bIsValid || pWeapon != pSlot.pWeapon)
				{
					pSlot.bIsValid = true;
					pSlot.bIsNew = true;
					pSlot.nFreshTick = nTick + nTransmitTicks;
					pSlot.pWeapon = pWeapon;
				} else if (pSlot.nFreshTick < nTick) {
					pSlot.bIsNew = false;
				}

				nHighestWeaponSlot = i;
			} else {
				pSlot.bIsValid = false;
				pSlot.bIsNew = false;
			}
		}

		// If you have less weapons, they will be transmitted more frequently
		if (++nNextWeaponSlot >= nHighestWeaponSlot)
			nNextWeaponSlot = 0;
	}

	void Reset()
	{
		Plat_FastMemset(this, 0, sizeof(PlayerTransmitCache));
	}

	void MarkFullUpdate()
	{
		nFreshFullUpdate = gpGlobals->tickcount + networking_transmit_ticks.GetInt();
	}

	bool InFullUpdate(const int nTick) const
	{
		return nFreshFullUpdate > nTick;
	}

	int nFreshFullUpdate = 0;
	int nLastAreaNum = 0;
	CBitVec<MAX_EDICTS> pLastTransmitBits; // No use yet
	CBitVec<MAX_EDICTS> pWeaponTransmitBits; // These are ALWAYS transmitted

	struct WeaponSlot
	{
		bool bIsNew = false; // Exists for quick checking to not have to compare numbers
		bool bIsValid = false;
		int nFreshTick = 0; // For how many ticks a weapon is considered new
		CBaseEntity* pWeapon = NULL; // in case a weapon is removed/given onto the same slot in a tick
	};

	// Used for rotating weapon slots when networking
	int nNextWeaponSlot = 0;
	int nHighestWeaponSlot = 0;
	WeaponSlot pWeapons[MAX_WEAPONS];
};
static PlayerTransmitCache g_pPlayerTransmitCache[MAX_PLAYERS];

class NetworkingGameEventListener : public IGameEventListener2
{
public:
	NetworkingGameEventListener() = default;

	void FireGameEvent(IGameEvent* pEvent)
	{
		if (V_stricmp(pEvent->GetName(), "OnRequestFullUpdate") != 0)
			return;
		
		int nPlayerSlot = pEvent->GetInt("index");
		if (nPlayerSlot >= MAX_PLAYERS || nPlayerSlot < 0)
		{
			Warning(PROJECT_NAME " - networking: Invalid OnRequestFullUpdate event! (Index: %i)\n", nPlayerSlot);
			return;
		}

		g_pPlayerTransmitCache[nPlayerSlot].MarkFullUpdate();
	}
};
static NetworkingGameEventListener g_pNetworkGameEventListener;

// Per tick cache
// Reset every tick using memset to 0!
struct PlayerTransmitTickCache
{
	int nAreaNum = 0;

	// All Entities sent to this player from the transmit check
	// 
	// NOTE:
	// We remove all Entities networked inside the client's transmit check (see pClientCache usage)
	// so the client and anything networked inside CGMOD_Player::SetTransmit is removed from this cache
	// this is to prevent issues like this:
	// All weapons of all players being networked instead of just the active weapon
	// as it would include per-client specific transmits
	CBitVec<MAX_EDICTS> pClientBitVec;
};

struct GlobalTransmitTickCache
{
	inline bool IsNewTick(int nTick)
	{
		return g_iLastCheckTransmit != nTick;
	}

	inline void NewTick(int nTick)
	{
		g_iLastCheckTransmit = nTick;

		g_bWasSeenByPlayer.ClearAll();
		g_pAlwaysTransmitCacheBitVec.ClearAll();
		Plat_FastMemset(g_bFilledDontTransmitWeaponCache, 0, sizeof(g_bFilledDontTransmitWeaponCache));
	}

	int g_iLastCheckTransmit = -1;
	CBitVec<MAX_EDICTS> g_pAlwaysTransmitCacheBitVec;
	CBitVec<MAX_EDICTS> g_bWasSeenByPlayer;
	bool g_bFilledDontTransmitWeaponCache[MAX_PLAYERS] = {0};
};
static GlobalTransmitTickCache g_pGlobalTransmitTickCache;
static PlayerTransmitTickCache g_pPlayerTransmitTickCache[MAX_PLAYERS] = {0};

#if 0 // Would be needed for pvs.AddEntitiesToTransmit / this would need to be called after the HolyLib:PostCheckTransmit hook if we'd were to allow entity additions in there
void Networking_DoPostTransmitCheck(CCheckTransmitInfo* pInfo)
{
	// Needed as else in PackEntities_Normal the entity may not be packed
	// Causing a crash in CBaseServer::WriteDeltas as its missing the packed data then
	pInfo->m_pTransmitEdict->Or(g_pGlobalTransmitTickCache.g_bWasSeenByPlayer, &g_pGlobalTransmitTickCache.g_bWasSeenByPlayer);
}
#endif

static CBitVec<MAX_EDICTS> g_pDontTransmitCache; // Reset on every CServerGameEnts::CheckTransmit call
#if !NETWORKING_USE_ENTITYCACHE // Not needed since our cache already excludes all the things by default
static CBitVec<MAX_EDICTS> g_pDontTransmitWeaponCache; // Per frame cache
static bool g_bFilledDontTransmitWeaponCache[MAX_PLAYERS] = {0}; // Exists just to avoid lots of iterations over all the weapons of players
#endif
static Detouring::Hook detour_CBaseCombatCharacter_SetTransmit;
static Symbols::CBaseCombatCharacter_SetTransmit func_CBaseAnimating_SetTransmit;
static ConVar networking_transmit_all_weapons("holylib_networking_transmit_all_weapons", "1", 0, "Experimental - By default all weapons are networked based on their PVS, though normally if they have an owner you might only want the active weapon to be networked");
static ConVar networking_transmit_all_weapons_to_owner("holylib_networking_transmit_all_weapons_to_owner", "1", 0, "Experimental - By default all weapons are networked to the owner");
static ConVar networking_transmit_one_per_tick("holylib_networking_transmit_one_per_tick", "0", 0, "Experimental - If enabled, one additional weapon is networked per tick");
static void hook_CBaseCombatCharacter_SetTransmit(CBaseCombatCharacter* pCharacter, CCheckTransmitInfo *pInfo, bool bAlways)
{
	if (!func_CBaseAnimating_SetTransmit)
	{
		// Without it we won't do shit, simply because possibly missing a transmit can cause quite the issues.
		detour_CBaseCombatCharacter_SetTransmit.GetTrampoline<Symbols::CBaseCombatCharacter_SetTransmit>()(pCharacter, pInfo, bAlways);
		return;
	}

	edict_t* pEdict = pCharacter->edict();
	if (pInfo->m_pTransmitEdict->Get(pEdict->m_EdictIndex)) // Already being networked!
		return;

	func_CBaseAnimating_SetTransmit(pCharacter, pInfo, bAlways); // Base transmit

	bool bLocalPlayer = pInfo->m_pClientEnt == pEdict;
	if (networking_bind_gmodhands_to_player.GetBool())
	{
		CBaseEntity* pGMODHands = GetGMODPlayerHands(pCharacter);
		if (pGMODHands) {
			if (bLocalPlayer) {
				pGMODHands->SetTransmit(pInfo, bAlways);
			}
#if !NETWORKING_USE_ENTITYCACHE
			else { // Hands are only networked to the owner, so we can save some checks by skipping them when going over them later
				int entIndex = pGMODHands->edict()->m_EdictIndex;
				g_pDontTransmitCache.Set(entIndex);
				g_pDontTransmitWeaponCache.Set(entIndex);
			}
#endif
		}
	}

	if (networking_bind_viewmodels_to_player.GetBool())
	{
		for (int i=0; i < MAX_VIEWMODELS; ++i)
		{
			CBaseEntity* pViewModel = GetViewModel(pCharacter, i);
			if (pViewModel) {
				if (bLocalPlayer) {
					pViewModel->SetTransmit(pInfo, bAlways);
				}
#if !NETWORKING_USE_ENTITYCACHE
				else {
					int entIndex = pViewModel->edict()->m_EdictIndex;
					g_pDontTransmitCache.Set(entIndex);
					g_pDontTransmitWeaponCache.Set(entIndex);
				}
#endif
			}
		}
	}

	if (networking_transmit_all_weapons.GetBool() || (bLocalPlayer && networking_transmit_all_weapons_to_owner.GetBool()))
	{
		for (int i=0; i < MAX_WEAPONS; ++i)
		{
			CBaseEntity *pWeapon = GetMyWeapon(pCharacter, i);
			if (!pWeapon)
				continue;

			// The local player is sent all of his weapons.
			pWeapon->SetTransmit(pInfo, bAlways);
		}
	} else {
		CBaseEntity* pActiveWeapon = GetActiveWeapon(pCharacter);
		if (pActiveWeapon)
			pActiveWeapon->SetTransmit(pInfo, bAlways);

#if !NETWORKING_USE_ENTITYCACHE // Our cache already removes them from transmit by default and expects us here to decide whos are networked.
		int nEdictIndex = pEdict->m_EdictIndex-1;
		if (!g_bFilledDontTransmitWeaponCache[nEdictIndex])
		{
			for ( int i=0; i < MAX_WEAPONS; ++i )
			{
				CBaseEntity *pWeapon = GetMyWeapon(pCharacter, i);
				if ( !pWeapon )
					continue;

				int entIndex = pWeapon->edict()->m_EdictIndex;
				g_pDontTransmitCache.Set(entIndex);
				g_pDontTransmitWeaponCache.Set(entIndex);
			}
			g_bFilledDontTransmitWeaponCache[nEdictIndex] = true;
		}
#endif
		/*
			If your out for performance without wanting to loose default behavior the following settings can improve performance while having just a little downside

			holylib_networking_transmit_all_weapons 0
			holylib_networking_transmit_all_weapons_to_owner 0
			holylib_networking_transmit_one_per_tick 1

			This way, ONLY the active weapon is always networked and 1 weapon every tick is additionally networked
			so after 255 Ticks a client will always have received an update for every weapon slot.

			NOTE:
			If you pick up a weapon and it does not become your active weapon
			it may happen that the client won't receive an update till that slot is reached leaving them unable to select the weapon

			Newer NOTE:
			If you don't need constant weapon updates, you can always keep networking_transmit_one_per_tick disabled and instead rely on
			holylib_networking_transmit_newweapons which will ensure players always know which weapons they have but won't receive constant updates about them.
		*/
		if (networking_transmit_one_per_tick.GetInt() == 1 || (networking_transmit_one_per_tick.GetInt() == 2 && bLocalPlayer))
		{
			int nSlot = g_pPlayerTransmitCache[pEdict->m_EdictIndex-1].nNextWeaponSlot;
			CBaseEntity *pWeapon = GetMyWeapon(pCharacter, nSlot);
			if ( pWeapon )
			{
#if !NETWORKING_USE_ENTITYCACHE
				int entIndex = pWeapon->edict()->m_EdictIndex;
				g_pDontTransmitCache.Clear(entIndex);
				// We don't clear g_bFilledDontTransmitWeaponCache to avoid it setting the bits above again
#endif

				pWeapon->SetTransmit(pInfo, bAlways);
			}
		}

		if (bLocalPlayer)
		{
			const PlayerTransmitCache& pCache = g_pPlayerTransmitCache[pEdict->m_EdictIndex-1];
			if (networking_transmit_onfullupdate.GetBool() && pCache.InFullUpdate(gpGlobals->tickcount))
			{
				for (int i=0; i < MAX_WEAPONS; ++i)
				{
					CBaseEntity *pWeapon = GetMyWeapon(pCharacter, i);
					if (!pWeapon)
						continue;

					pWeapon->SetTransmit(pInfo, bAlways);
				}
				return; // We don't gotta do the thing below
			}

			if (networking_transmit_newweapons.GetBool())
			{
				for (int i=0; i < MAX_WEAPONS; ++i)
				{
					const PlayerTransmitCache::WeaponSlot& pWeaponSlot = pCache.pWeapons[i];
					if (pWeaponSlot.bIsNew)
						pWeaponSlot.pWeapon->SetTransmit(pInfo, bAlways);
				}
			}
		}
	}
}

// Fast path method of networking a player by using the cached results of another one
static void TransmitFastPathPlayer(CBasePlayer* pRecipientPlayer, int clientIndex, CCheckTransmitInfo *pInfo, int iOtherClient, const PlayerTransmitTickCache& nOtherCache)
{
	nOtherCache.pClientBitVec.CopyTo(pInfo->m_pTransmitEdict);
	if (pInfo->m_pTransmitAlways)
		nOtherCache.pClientBitVec.CopyTo(pInfo->m_pTransmitAlways);

	// g_pPlayerTransmitCacheBitVec won't contain any information about the client the cache was build upon, so we need to call SetTransmit ourselfs.
	// & yes, using the g_pEntityCache like this is safe, even if it doesn't look save - Time to see how long it'll take until I regret writing this
	g_pEntityCache[iOtherClient+1]->SetTransmit(pInfo, true);
	pRecipientPlayer->SetTransmit(pInfo, true);
	// ENGINE BUG: CBaseCombatCharacter::SetTransmit doesn't network the player's viewmodel! So we need to do it ourself.
	// This was probably done since CBaseViewModel::ShouldTransmit determines if it would be sent or not.
	// We can remove this once we have: https://github.com/Facepunch/garrysmod-requests/issues/2839
	for (int iViewModel=0; iViewModel<MAX_VIEWMODELS; ++iViewModel)
	{
		CBaseViewModel* pViewModel = GetViewModel(pRecipientPlayer, iViewModel);
		if (pViewModel)
			pViewModel->SetTransmit(pInfo, true);
	}

	CBaseEntity* pHandsEntity = GetGMODPlayerHands(pRecipientPlayer);
	if (pHandsEntity)
		pHandsEntity->SetTransmit(pInfo, true);

	// Extra stuff to hopefully not break the observer mode
	if (pRecipientPlayer->GetObserverMode() == OBS_MODE_IN_EYE)
	{
		CBaseEntity* pObserverEntity = pRecipientPlayer->GetObserverTarget();
		if (pObserverEntity)
		{
			pObserverEntity->SetTransmit(pInfo, true);
			if (pObserverEntity->IsPlayer())
			{
				// Time to network these shit again
				CBasePlayer* pObserverPlayer = (CBasePlayer*)pObserverEntity;
				for (int iViewModel=0; iViewModel<MAX_VIEWMODELS; ++iViewModel)
				{
					CBaseViewModel* pViewModel = GetViewModel(pRecipientPlayer, iViewModel);
					if (pViewModel)
						pViewModel->SetTransmit(pInfo, true);
				}

				pHandsEntity = GetGMODPlayerHands(pObserverPlayer);
				if (pHandsEntity)
					pHandsEntity->SetTransmit(pInfo, true);
			}
		}
	}

	// Fast way to set all prevent transmit things.
	CBitVec_AndNot(pInfo->m_pTransmitEdict, &g_pShouldPrevent[clientIndex]);
	if (pInfo->m_pTransmitAlways)
		CBitVec_AndNot(pInfo->m_pTransmitAlways, &g_pShouldPrevent[clientIndex]);

	// Since we optimized PackEntities_Normal using g_bWasSeenByPlayer, we need to now also perform this Or here.
	// If we don't do this, Entities like the CBaseViewModel won't be packed by PackEntities_Normal causing a crash later deep inside SV_WriteEnterPVS
	pInfo->m_pTransmitEdict->Or(g_pGlobalTransmitTickCache.g_bWasSeenByPlayer, &g_pGlobalTransmitTickCache.g_bWasSeenByPlayer);
}

// Very expensive!
static inline void DoTransmitPVSCheck(edict_t* pEdict, CBaseEntity* pEnt, const bool bIsHLTV, CCheckTransmitInfo *pInfo, const bool bForceTransmit, const int skyBoxArea)
{
	CCServerNetworkProperty *netProp = static_cast<CCServerNetworkProperty*>( pEdict->GetNetworkable() );
	if ( !netProp )
	{
		Warning(PROJECT_NAME " - networking: Somehow CCServerNetworkProperty was NULL!\n");
		return;
	}

	if ( bIsHLTV )
	{
		// for the HLTV/Replay we don't cull against PVS
		if ( netProp->AreaNum() == skyBoxArea ) {
			pEnt->SetTransmit( pInfo, true );
		} else {
			pEnt->SetTransmit( pInfo, false );
		}
		return;
	}

	// Always send entities in the player's 3d skybox.
	// Sidenote: call of AreaNum() ensures that PVS data is up to date for this entity
	bool bSameAreaAsSky = netProp->AreaNum() == skyBoxArea;
	if ( bSameAreaAsSky )
	{
		pEnt->SetTransmit( pInfo, true );
		return;
	}

	bool bInPVS = netProp->IsInPVS( pInfo );
	if ( bInPVS || bForceTransmit )
	{
		// only send if entity is in PVS
		pEnt->SetTransmit( pInfo, false );
		return;
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
			int nFlags = pCheckEntity->ShouldTransmit( pInfo );
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

static ConVar networking_fastpath("holylib_networking_fastpath", "0", 0, "Experimental - If two players are in the same area, then it will reuse the transmit state of the first calculated player saving a lot of time");
static ConVar networking_fasttransmit("holylib_networking_fasttransmit", "1", 0, "Experimental - Replaces CServerGameEnts::CheckTransmit with our own implementation");
static ConVar networking_fastpath_usecluster("holylib_networking_fastpath_usecluster", "1", 0, "Experimental - When using the fastpatth, it will compate against clients in the same cluster instead of area");
bool New_CServerGameEnts_CheckTransmit(IServerGameEnts* gameents, CCheckTransmitInfo *pInfo, const unsigned short *pEdictIndices, int nEdicts)
{
	if (!networking_fasttransmit.GetBool() || !gpGlobals || !engine || !mdlcache || !func_CBaseAnimating_SetTransmit)
		return false;

	// get recipient player's skybox: 3670181
	CBaseEntity *pRecipientEntity = Util::servergameents->EdictToBaseEntity(pInfo->m_pClientEnt);
	if (!pRecipientEntity)
		return true;
	
	MDLCACHE_CRITICAL_SECTION();
	CBasePlayer *pRecipientPlayer = static_cast<CBasePlayer*>(pRecipientEntity);
	const int skyBoxArea = GetSkybox3DArea(pRecipientPlayer);
	const int clientIndex = pInfo->m_pClientEnt->m_EdictIndex - 1;

	// BUG: Can this even happen? Probably, when people screw with the gameserver module & disable spawn safety
	if (clientIndex >= MAX_PLAYERS || clientIndex < 0)
		return true; // We don't return false since we never want to transmit anything to a player in a invalid slot!

	const Vector& clientPosition = (pRecipientPlayer->GetViewEntity() != NULL) ? pRecipientPlayer->GetViewEntity()->EyePosition() : pRecipientPlayer->EyePosition();
	const int clientArea = networking_fastpath_usecluster.GetBool() ? Util::engineserver->GetClusterForOrigin(clientPosition) : Util::engineserver->GetArea(clientPosition);

	// NOTE: We intentionally use GetArea and not GetCluster, since a Area is far bigger than a cluster & it should work good enouth.
	// Possible BUG: The PVS might hate us for doing such a cruel thing to it. Anyways >:3

	// ToDo: Bring over's CS:GO code for InitialSpawnTime
	//const bool bIsFreshlySpawned = pRecipientPlayer->GetInitialSpawnTime()+3.0f > gpGlobals->curtime;

	// pRecipientPlayer->IsHLTV(); Why do we not use IsHLTV()? Because its NOT a virtual function & the variables are fked
	const int nCurrentTick = gpGlobals->tickcount;
	const bool bIsHLTV = pInfo->m_pTransmitAlways != NULL;
	const bool bFastPath = networking_fastpath.GetBool();
	const bool bFirstTransmit = g_pGlobalTransmitTickCache.IsNewTick(nCurrentTick);
	if (bFirstTransmit)
	{
		if (bFastPath)
			Plat_FastMemset(g_pPlayerTransmitTickCache, 0, sizeof(g_pPlayerTransmitTickCache));

		for (int iPlayerIndex = 1; iPlayerIndex <= gpGlobals->maxClients; ++iPlayerIndex)
		{
			CBaseEntity* pPlayer = g_pEntityCache[iPlayerIndex];
			if (!pPlayer)
				continue;

			g_pPlayerTransmitCache[iPlayerIndex].NextTick(pPlayer, nCurrentTick);
		}

//#if NETWORKING_USE_ENTITYCACHE
		g_nEntityTransmitCache.UpdateEntities(pEdictIndices, nEdicts);
//#endif
		g_pGlobalTransmitTickCache.NewTick(nCurrentTick);

#if !NETWORKING_USE_ENTITYCACHE
		g_pDontTransmitWeaponCache.ClearAll();
#endif
	} else {
		g_pGlobalTransmitTickCache.g_pAlwaysTransmitCacheBitVec.CopyTo(pInfo->m_pTransmitEdict);
		if (bIsHLTV)
			g_pGlobalTransmitTickCache.g_pAlwaysTransmitCacheBitVec.CopyTo(pInfo->m_pTransmitAlways);

		if (bFastPath)
		{
			for (int iOtherClient = 0; iOtherClient<MAX_PLAYERS; ++iOtherClient)
			{
				const PlayerTransmitTickCache& nOtherCache = g_pPlayerTransmitTickCache[iOtherClient];
				if (nOtherCache.nAreaNum != clientArea)
					continue;

				TransmitFastPathPlayer(pRecipientPlayer, clientIndex, pInfo, iOtherClient, nOtherCache);
				return true; // fast route when players are in the same area, we can save a tone of calculation hopefully without breaking anything.
			}
		}
	}

	g_pShouldPrevent[clientIndex].CopyTo(&g_pDontTransmitCache); // We combine Gmod's prevent transmit with also our things to remove unessesary checks.
#if !NETWORKING_USE_ENTITYCACHE
	if (!bFirstTransmit)
		g_pDontTransmitWeaponCache.Or(g_pDontTransmitCache, &g_pDontTransmitCache); // Now combine our cached weapon cache.
#endif

	g_nEntityTransmitCache.pNeverTransmitBits.Or(g_pDontTransmitCache, &g_pDontTransmitCache);

	const int clientEntIndex = pInfo->m_pClientEnt->m_EdictIndex;
	static CBitVec<MAX_EDICTS> pClientCache; // Temporary cache used when we are calculating the transmit to the current pRecipientPlayer
	const bool bForceTransmit = sv_force_transmit_ents->GetBool();
	bool bWasTransmitToPlayer = false;
#if NETWORKING_USE_ENTITYCACHE
	pInfo->m_pTransmitEdict->Or(g_nEntityTransmitCache.pAlwaysTransmitBits, pInfo->m_pTransmitEdict);
	if (bIsHLTV)
		pInfo->m_pTransmitAlways->Or(g_nEntityTransmitCache.pAlwaysTransmitBits, pInfo->m_pTransmitAlways);

	for (int i=0; i<g_nEntityTransmitCache.nFullEdictCount; ++i)
	{
		CBaseEntity* pEnt = g_nEntityTransmitCache.pFullEntityList[i];

		int iEdict = pEnt->edict()->m_EdictIndex;
		if (iEdict == clientEntIndex) {
			pInfo->m_pTransmitEdict->CopyTo(&pClientCache);
			bWasTransmitToPlayer = true;
		} else if (bWasTransmitToPlayer) {
			// We Xor it so that the pClientCache contains all bits / entities
			// that were sent specifically to our client in it's transmit check.
			pInfo->m_pTransmitEdict->Xor(pClientCache, &pClientCache);
			bWasTransmitToPlayer = false;
		}

		if (pInfo->m_pTransmitEdict->Get(iEdict) || g_pDontTransmitCache.Get(iEdict))
			continue;

		// do a full ShouldTransmit() check, may return FL_EDICT_CHECKPVS
		int nFlags = pEnt->ShouldTransmit(pInfo);
		if (nFlags & FL_EDICT_ALWAYS)
		{
			pEnt->SetTransmit(pInfo, true);
			// g_pAlwaysTransmitCacheBitVec.Set( iEdict ); We do NOT do this since view models and such would also be included.
			continue;
		}

		if (nFlags & FL_EDICT_DONTSEND)
			continue;

		// Now only PVS remains
		DoTransmitPVSCheck(pEnt->edict(), pEnt, bIsHLTV, pInfo, bForceTransmit, skyBoxArea);
	}

	if (networking_areasplit.GetBool())
	{
		int nClientArea = Util::engineserver->GetArea(clientPosition);
		for (int nArea = 0; nArea<MAX_MAP_AREAS-1; ++nArea)
		{
			EntityTransmitCache::AreaCache& pArea = g_nEntityTransmitCache.nAreaEntities[nArea];
			if (pArea.nCount == 0)
				continue;

			// +1 since we shifted everythign by 1 to remove Area0 saving some KB
			if (!Util::engineserver->CheckAreasConnected(nClientArea, nArea+1))
				continue;

			for (int i=0; i<pArea.nCount; ++i)
			{
				CBaseEntity* pEnt = pArea.pEntities[i];

				edict_t* pEdict = pEnt->edict();
				int iEdict = pEdict->m_EdictIndex;
				if (pInfo->m_pTransmitEdict->Get(iEdict) || g_pDontTransmitCache.Get(iEdict))
					continue;

				// Now only PVS remains
				DoTransmitPVSCheck(pEdict, pEnt, bIsHLTV, pInfo, bForceTransmit, skyBoxArea);
			}
		}
	}

	for (int i=0; i<g_nEntityTransmitCache.nPVSEdictCount; ++i)
	{
		CBaseEntity* pEnt = g_nEntityTransmitCache.pPVSEntityList[i];

		edict_t* pEdict = pEnt->edict();
		int iEdict = pEdict->m_EdictIndex;
		if (pInfo->m_pTransmitEdict->Get(iEdict) || g_pDontTransmitCache.Get(iEdict))
			continue;

		// Now only PVS remains
		DoTransmitPVSCheck(pEdict, pEnt, bIsHLTV, pInfo, bForceTransmit, skyBoxArea);
	}

	if (bWasTransmitToPlayer)
	{
		// Same here just in case the player was the only pvs thing (which should like almost never be the case tho)
		pInfo->m_pTransmitEdict->Xor(pClientCache, &pClientCache);
		bWasTransmitToPlayer = false;
	}

	if (networking_transmit_onfullupdate.GetBool())
	{
		if (g_pPlayerTransmitCache[clientIndex].InFullUpdate(nCurrentTick))
		{
			for (int iPlayerIndex = 1; iPlayerIndex <= gpGlobals->maxClients; ++iPlayerIndex)
			{
				// Lets avoid trying to network invalid entity slots as else we trigger a crash/engine error in CBaseServer::WriteDeltaEntities
				if (!g_pEntityCache[iPlayerIndex])
					continue;

				// We mark all to transmit to they will receive the CBasePlayer's
				// but not all their weapon since that could cause a overflow due to the amount of data that could be sent at once
				pInfo->m_pTransmitEdict->Set(iPlayerIndex);
				if (bIsHLTV)
					pInfo->m_pTransmitAlways->Set(iPlayerIndex);
			}
		} else if (networking_transmit_onfullupdate_networktoothers.GetBool()) {
			// In this case, if any other player is having a full update, we network them to all others
			// simply because this ensures every player knows of every other players existance
			for (int iPlayerIndex = 1; iPlayerIndex <= gpGlobals->maxClients; ++iPlayerIndex)
			{
				if (g_pEntityCache[iPlayerIndex] && g_pPlayerTransmitCache[iPlayerIndex-1].InFullUpdate(nCurrentTick))
				{
					pInfo->m_pTransmitEdict->Set(iPlayerIndex);
					if (bIsHLTV)
						pInfo->m_pTransmitAlways->Set(iPlayerIndex);
				}
			}
		}
	}
#else
	for ( int i=0; i < nEdicts; ++i )
	{
		int iEdict = pEdictIndices[i];

		edict_t *pEdict = &world_edict[iEdict]; // world_edict is already cached.
		// Assert( pEdict == engine->PEntityOfEntIndex( iEdict ) );
		int nFlags = pEdict->m_fStateFlags & (FL_EDICT_DONTSEND|FL_EDICT_ALWAYS|FL_EDICT_PVSCHECK|FL_EDICT_FULLCHECK);

		if ( iEdict == clientEntIndex )
		{
			pInfo->m_pTransmitEdict->CopyTo(&pClientCache);
			bWasTransmitToPlayer = true;
		} else if ( bWasTransmitToPlayer ) {
			// We Xor it so that the pClientCache contains all bits / entities
			// that were sent specifically to our client in it's transmit check.
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
				g_pGlobalTransmitTickCache.g_pAlwaysTransmitCacheBitVec.Set( iEdict );
	
				if ( bIsHLTV )
					pInfo->m_pTransmitAlways->Set( iEdict );

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
		if ( nFlags == FL_EDICT_FULLCHECK )
		{
			// do a full ShouldTransmit() check, may return FL_EDICT_CHECKPVS
			nFlags = pEnt->ShouldTransmit( pInfo );

			if ( nFlags & FL_EDICT_ALWAYS )
			{
				pEnt->SetTransmit( pInfo, true );
				// g_pAlwaysTransmitCacheBitVec.Set( iEdict ); We do NOT do this since view models and such would also be included.
				continue;
			}	
		}

		// don't send this entity
		if ( !( nFlags & FL_EDICT_PVSCHECK ) )
			continue;

		DoTransmitPVSCheck(pEdict, pEnt, bIsHLTV, pInfo, bForceTransmit, skyBoxArea);
	}
#endif

#if 0 // I don't like this, useful for testing but not for actual prod
	for (int i=0; i<MAX_EDICTS; ++i)
	{
		if (pInfo->m_pTransmitEdict->IsBitSet(i) && !g_pEntityCache[i])
		{ // g_pEntityCache is basically just a quicker form of (CBaseEntity*)(&world_edict[i])->GetUnknown();
			Warning(PROJECT_NAME " - networking: Tried to network an Entity that does not exist! (%i)\n", i);
			pInfo->m_pTransmitEdict->Clear(i);
			if (bIsHLTV)
				pInfo->m_pTransmitAlways->Clear(i);
		}
	}
#endif

	// HLTV has different networking! Some things might transmit when for normal players they wouldn't!
	// ObserverMode also influences how things are networked!
	if (bFastPath && !bIsHLTV && !(pRecipientPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pRecipientPlayer->GetObserverTarget()))
	{
		PlayerTransmitTickCache& nTransmitCache = g_pPlayerTransmitTickCache[clientIndex];
		// Remove player's viewmodels from the cache since thoes are supposed to only be networked to the recipient player

		pInfo->m_pTransmitEdict->CopyTo(&nTransmitCache.pClientBitVec);
		CBitVec_AndNot(&nTransmitCache.pClientBitVec, &pClientCache);
		nTransmitCache.nAreaNum = clientArea;
	}
	pInfo->m_pTransmitEdict->Or(g_pGlobalTransmitTickCache.g_bWasSeenByPlayer, &g_pGlobalTransmitTickCache.g_bWasSeenByPlayer);
//	Msg("A:%i, N:%i, F: %i, P: %i\n", always, dontSend, fullCheck, PVS );

	return true;
}

void SV_FillHLTVData( CFrameSnapshot *pSnapshot, edict_t *edict, int iValidEdict )
{
	if ( pSnapshot->m_pHLTVEntityData && edict )
	{
		CHLTVEntityData *pHLTVData = &pSnapshot->m_pHLTVEntityData[iValidEdict];
		PVSInfo_t *pvsInfo = edict->GetNetworkable()->GetPVSInfo();
		if ( pvsInfo->m_nClusterCount == 1 ) {
			// store cluster, if entity spawns only over one cluster
			pHLTVData->m_nNodeCluster = pvsInfo->m_pClusters[0];
		} else {
			// otherwise save PVS head node for larger entities
			pHLTVData->m_nNodeCluster = pvsInfo->m_nHeadNode | (1<<31);
		}

		// remember origin
		pHLTVData->origin[0] = pvsInfo->m_vCenter[0];
		pHLTVData->origin[1] = pvsInfo->m_vCenter[1];
		pHLTVData->origin[2] = pvsInfo->m_vCenter[2];
	}
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

	if (!gpGlobals || (g_pGlobalTransmitTickCache.IsNewTick(gpGlobals->tickcount)))
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
			if (!g_pGlobalTransmitTickCache.g_bWasSeenByPlayer.IsBitSet(index))
				continue;

			PackWork_t& w = workItems[workItemCount++];
			w.nIdx = index;
			w.pEdict = edict;
			w.pSnapshot = snapshot;
		}
	}

	if (!sv_parallel_packentities)
		sv_parallel_packentities = g_pCVar->FindVar("sv_parallel_packentities");

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

	g_nEntityTransmitCache.EntityRemoved(pEntity, pEdict);
	CleaupSetPreventTransmit(pEntity);
	int entIndex = pEdict->m_EdictIndex;
	g_pEntityCache[entIndex] = NULL;
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

void CNetworkingModule::ClientDisconnect(edict_t* pPlayer)
{
	g_pPlayerTransmitCache[pPlayer->m_EdictIndex-1].Reset();
}

#if SYSTEM_WINDOWS
DETOUR_THISCALL_START()
	DETOUR_THISCALL_ADDFUNC2( hook_CBaseEntity_GMOD_SetShouldPreventTransmitToPlayer, GMOD_SetShouldPreventTransmitToPlayer, CBaseEntity*, CBasePlayer*, bool );
	DETOUR_THISCALL_ADDRETFUNC1( hook_CBaseEntity_GMOD_ShouldPreventTransmitToPlayer, bool, GMOD_ShouldPreventTransmitToPlayer, CBaseEntity*, CBasePlayer* );
	DETOUR_THISCALL_ADDFUNC1( hook_CGMOD_Player_CreateViewModel, CreateViewModel, CBasePlayer*, int );
	DETOUR_THISCALL_ADDFUNC2( hook_CBaseCombatCharacter_SetTransmit, SetTransmit, CBaseCombatCharacter*, CCheckTransmitInfo*, bool );
DETOUR_THISCALL_FINISH();
#endif

void CNetworkingModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	Util::gameeventmanager->AddListener(&g_pNetworkGameEventListener, "OnRequestFullUpdate", true);
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

	DETOUR_PREPARE_THISCALL();
	SourceSDK::FactoryLoader server_loader("server");
	Detour::Create(
		&detour_CBaseEntity_GMOD_SetShouldPreventTransmitToPlayer, "CBaseEntity::GMOD_SetShouldPreventTransmitToPlayer",
		server_loader.GetModule(), Symbols::CBaseEntity_GMOD_SetShouldPreventTransmitToPlayerSym,
		(void*)DETOUR_THISCALL(hook_CBaseEntity_GMOD_SetShouldPreventTransmitToPlayer, GMOD_SetShouldPreventTransmitToPlayer), m_pID
	);

	Detour::Create(
		&detour_CBaseEntity_GMOD_ShouldPreventTransmitToPlayer, "CBaseEntity::GMOD_ShouldPreventTransmitToPlayer",
		server_loader.GetModule(), Symbols::CBaseEntity_GMOD_ShouldPreventTransmitToPlayerSym,
		(void*)DETOUR_THISCALL(hook_CBaseEntity_GMOD_ShouldPreventTransmitToPlayer, GMOD_ShouldPreventTransmitToPlayer), m_pID
	);

	Detour::Create(
		&detour_CGMOD_Player_CreateViewModel, "CGMOD_Player::CreateViewModel",
		server_loader.GetModule(), Symbols::CGMOD_Player_CreateViewModelSym,
		(void*)DETOUR_THISCALL(hook_CGMOD_Player_CreateViewModel, CreateViewModel), m_pID
	);

	Detour::Create(
		&detour_CBaseCombatCharacter_SetTransmit, "CBaseCombatCharacter::SetTransmit",
		server_loader.GetModule(), Symbols::CBaseCombatCharacter_SetTransmitSym,
		(void*)DETOUR_THISCALL(hook_CBaseCombatCharacter_SetTransmit, SetTransmit), m_pID
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

#if ARCHITECTURE_IS_X86
	PropTypeFns* pPropTypeFns = Detour::ResolveSymbol<PropTypeFns>(engine_loader, Symbols::g_PropTypeFnsSym);
#else
	PropTypeFns* pPropTypeFns = Detour::ResolveSymbolFromLea<PropTypeFns>(engine_loader.GetModule(), Symbols::g_PropTypeFnsSym);
#endif
	Detour::CheckValue("get class", "pPropTypeFns", pPropTypeFns != NULL);

#if ARCHITECTURE_IS_X86
	g_BSPData = Detour::ResolveSymbol<CCollisionBSPData>(engine_loader, Symbols::g_BSPDataSym);
#else
	g_BSPData = Detour::ResolveSymbolFromLea<CCollisionBSPData>(engine_loader.GetModule(), Symbols::g_BSPDataSym);
#endif
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

	func_CBaseAnimating_SetTransmit = (Symbols::CBaseCombatCharacter_SetTransmit)Detour::GetFunction(server_loader.GetModule(), Symbols::CBaseAnimating_SetTransmitSym);
	Detour::CheckFunction((void*)func_CBaseAnimating_SetTransmit, "CBaseAnimating::SetTransmit");

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
		for (int i = 0; i < serverclass->m_pTable->GetNumProps(); ++i) {
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
		for (int i = 0; i < sendTable->m_pPrecalc->m_nDataTableProxies; ++i)
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

	Util::gameeventmanager->RemoveListener(&g_pNetworkGameEventListener);

	/*
	 * The code below to unload also belongs to sigsegv
	 * Source: https://github.com/rafradek/sigsegv-mvm/blob/e6a6cee305023f36e5b914872500ef8319317d71/src/mod/perf/sendprop_optimize.cpp#L1981-L2002
	 */
	/*for (CFrameSnapshot* pSnapshot : framesnapshotmanager->m_FrameSnapshots)
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
	}*/
}

static char strIndent = '\t';
static char strNewLine = '\n';
static void WriteString(std::string str, int nIndent, FileHandle_t pHandle)
{
	for (int i=0; i<nIndent; ++i)
	{
		g_pFullFileSystem->Write(&strIndent, 1, pHandle);
	}

	g_pFullFileSystem->Write(str.c_str(), str.length(), pHandle);
	g_pFullFileSystem->Write(&strNewLine, 1, pHandle);
}

#define APPEND_IF_PFLAGS_CONTAINS_SPROP(sprop) if(flags & SPROP_##sprop) pFlags.append(" " #sprop)
extern void WriteSendTable(SendTable* pTable, std::unordered_set<SendTable*>& pWrittenTables);
void WriteSendProp(SendProp* pProp, int nIndex, int nIndent, FileHandle_t pHandle, std::unordered_set<SendTable*>& pWrittenTables)
{
	std::string pIndex = "Index: ";
	pIndex.append(std::to_string(nIndex));
	WriteString(pIndex, nIndent, pHandle);

	std::string pName = "PropName: ";
	pName.append(pProp->GetName());
	WriteString(pName, nIndent, pHandle);

	std::string pExcludeDTName = "ExcludeName: ";
	pExcludeDTName.append(pProp->GetExcludeDTName() != NULL ? pProp->GetExcludeDTName() : "NULL");
	WriteString(pExcludeDTName, nIndent, pHandle);

	std::string pOffset = "Offset: ";
	pOffset.append(std::to_string(pProp->GetOffset()));
	WriteString(pOffset, nIndent, pHandle);

	std::string pFlags = "Flags:";
	int flags = pProp->GetFlags();
	if (flags == 0) {
		pFlags.append(" None");
	} else {
		APPEND_IF_PFLAGS_CONTAINS_SPROP(UNSIGNED);
		APPEND_IF_PFLAGS_CONTAINS_SPROP(COORD);
		APPEND_IF_PFLAGS_CONTAINS_SPROP(NOSCALE);
		APPEND_IF_PFLAGS_CONTAINS_SPROP(ROUNDDOWN);
		APPEND_IF_PFLAGS_CONTAINS_SPROP(ROUNDUP);
		APPEND_IF_PFLAGS_CONTAINS_SPROP(NORMAL);
		APPEND_IF_PFLAGS_CONTAINS_SPROP(EXCLUDE);
		APPEND_IF_PFLAGS_CONTAINS_SPROP(XYZE);
		APPEND_IF_PFLAGS_CONTAINS_SPROP(INSIDEARRAY);
		APPEND_IF_PFLAGS_CONTAINS_SPROP(PROXY_ALWAYS_YES);
		APPEND_IF_PFLAGS_CONTAINS_SPROP(CHANGES_OFTEN);
		APPEND_IF_PFLAGS_CONTAINS_SPROP(IS_A_VECTOR_ELEM);
		APPEND_IF_PFLAGS_CONTAINS_SPROP(COORD_MP);
		APPEND_IF_PFLAGS_CONTAINS_SPROP(COORD_MP_LOWPRECISION);
		APPEND_IF_PFLAGS_CONTAINS_SPROP(COORD_MP_INTEGRAL);
		APPEND_IF_PFLAGS_CONTAINS_SPROP(VARINT);
		APPEND_IF_PFLAGS_CONTAINS_SPROP(ENCODED_AGAINST_TICKCOUNT);
	}
	WriteString(pFlags, nIndent, pHandle);

	std::string pDataTableName = "Inherited: ";
	pDataTableName.append((pProp->GetType() == SendPropType::DPT_DataTable && pProp->GetDataTable()) ? pProp->GetDataTable()->GetName() : "NONE");
	

	if (pProp->GetDataTable())
		WriteSendTable(pProp->GetDataTable(), pWrittenTables);
	WriteString(pDataTableName, nIndent, pHandle);

	std::string pType = "Type: ";
	switch (pProp->GetType())
	{
	case SendPropType::DPT_Int:
		pType.append("DPT_Int");
		break;
	case SendPropType::DPT_Float:
		pType.append("DPT_Float");
		break;
	case SendPropType::DPT_Vector:
		pType.append("DPT_Vector");
		break;
	case SendPropType::DPT_VectorXY:
		pType.append("DPT_VectorXY");
		break;
	case SendPropType::DPT_String:
		pType.append("DPT_String");
		break;
	case SendPropType::DPT_Array:
		pType.append("DPT_Array");
		break;
	case SendPropType::DPT_DataTable:
		pType.append("DPT_DataTable");
		break;
	case SendPropType::DPT_GMODTable:
		pType.append("DPT_GMODTable");
		break;
	default:
		pType.append("UNKNOWN(");
		pType.append(std::to_string(pProp->GetType()));
		pType.append(")");
	}
	WriteString(pType, nIndent, pHandle);

	std::string pElemets = "NumElement: ";
	pElemets.append(std::to_string(pProp->GetNumElements()));
	WriteString(pElemets, nIndent, pHandle);

	std::string pBits = "Bits: ";
	pBits.append(std::to_string(pProp->m_nBits));
	WriteString(pBits, nIndent, pHandle);

	std::string pHighValue = "HighValue: ";
	pHighValue.append(std::to_string(pProp->m_fHighValue));
	WriteString(pHighValue, nIndent, pHandle);

	std::string pLowValue = "LowValue: ";
	pLowValue.append(std::to_string(pProp->m_fLowValue));
	WriteString(pLowValue, nIndent, pHandle);

	if (pProp->GetArrayProp())
	{
		std::string pArrayProp = "ArrayProp: ";

		WriteString(pArrayProp, nIndent, pHandle);
		WriteSendProp(pProp->GetArrayProp(), nIndex, nIndent + 1, pHandle, pWrittenTables);
	}
}

static void WriteSendTable(SendTable* pTable, FileHandle_t pHandle, std::unordered_set<SendTable*>& pWrittenTables)
{
	for (int i = 0; i < pTable->GetNumProps(); ++i) {
		SendProp* pProp = pTable->GetProp(i);
		
		WriteSendProp(pProp, i, 0, pHandle, pWrittenTables);

		g_pFullFileSystem->Write(&strNewLine, 1, pHandle);

		if (pWrittenTables.find(pTable) == pWrittenTables.end())
		{
			pWrittenTables.insert(pTable);
		}
	}
}

static std::string baseDTDumpFilePath = "holylib/dump/dt/";
void WriteSendTable(SendTable* pTable, std::unordered_set<SendTable*>& pWrittenTables)
{
	if (pWrittenTables.find(pTable) != pWrittenTables.end())
		return; // Already wrote it. Skipping...

	std::string fileName = baseDTDumpFilePath;
	fileName.append(pTable->GetName());
	fileName.append(".txt");

	FileHandle_t pHandle = g_pFullFileSystem->Open(fileName.c_str(), "wb", "MOD");
	if (!pHandle)
	{
		Warning(PROJECT_NAME " - DumpDT: Failed to open \"%s\" for dump!\n", fileName.c_str());
		return;
	}

	for (int i = 0; i < pTable->GetNumProps(); ++i) {
		SendProp* pProp = pTable->GetProp(i);
		
		WriteSendProp(pProp, i, 0, pHandle, pWrittenTables);

		g_pFullFileSystem->Write(&strNewLine, 1, pHandle);

		if (pWrittenTables.find(pTable) == pWrittenTables.end())
		{
			pWrittenTables.insert(pTable);
		}
	}

	g_pFullFileSystem->Close(pHandle);
}

static void DumpDT(const CCommand &args)
{
	g_pFullFileSystem->CreateDirHierarchy(baseDTDumpFilePath.c_str(), "MOD");

	std::unordered_set<SendTable*> pWrittenTables;
	int nClassIndex = 0;
	for(ServerClass *serverclass = Util::servergamedll->GetAllServerClasses(); serverclass->m_pNext != nullptr; serverclass = serverclass->m_pNext) {
		std::string fileName = baseDTDumpFilePath;
		fileName.append(std::to_string(nClassIndex++));
		fileName.append("_");
		fileName.append(serverclass->GetName());
		fileName.append("-");
		fileName.append(serverclass->m_pTable->GetName());
		fileName.append(".txt");

		FileHandle_t pHandle = g_pFullFileSystem->Open(fileName.c_str(), "wb", "MOD");
		if (!pHandle)
		{
			Warning(PROJECT_NAME " - DumpDT: Failed to open \"%s\" for dump!\n", fileName.c_str());
			continue;
		}

		WriteSendTable(serverclass->m_pTable, pHandle, pWrittenTables);

		g_pFullFileSystem->Close(pHandle);
	}

	FileHandle_t pFullList = g_pFullFileSystem->Open("holylib/dump/dt/fulllist.dt", "wb", "MOD");
	if (pFullList)
	{
		nClassIndex = 0;
		for(ServerClass *serverclass = Util::servergamedll->GetAllServerClasses(); serverclass->m_pNext != nullptr; serverclass = serverclass->m_pNext) {
			std::string pName = serverclass->GetName();
			pName.append(" = ");
			pName.append(std::to_string(nClassIndex++));
			g_pFullFileSystem->Write(pName.c_str(), pName.length(), pFullList);
			g_pFullFileSystem->Write(&strNewLine, 1, pFullList);
		}

		g_pFullFileSystem->Close(pFullList);
	}
}
static ConCommand dumpdt("holylib_networking_dumpdt", DumpDT, "Dumps a lot of DT into into the holylib/dumps/dt.txt file", 0);
