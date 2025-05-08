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
#include "player.h"
#include "baseclient.h"
#include <bitset>
#include <unordered_set>
#include <datacache/imdlcache.h>
#include <cmodel_private.h>

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

static std::unordered_map<CBasePlayer*, std::unordered_set<CBaseEntity*>> g_pShouldPreventTransmitPlayer;
static bool g_pShouldPrevent[MAX_EDICTS][MAX_PLAYERS];
static Detouring::Hook detour_CBaseEntity_GMOD_ShouldPreventTransmitToPlayer;
static bool hook_CBaseEntity_GMOD_ShouldPreventTransmitToPlayer(CBaseEntity* ent, CBasePlayer* ply)
{
	return g_pShouldPrevent[ent->entindex()][ply->entindex()-1];
}

static void CleanupEntity(CBaseEntity* pEnt, bool bPlyCleanup = false)
{
	memset(g_pShouldPrevent[pEnt->entindex()], 0, MAX_PLAYERS);
}

static void CleaupSetPreventTransmit(CBaseEntity* ent)
{
	if (!ent->IsPlayer())
	{
		CleanupEntity(ent, false);
		return;
	}

	CBasePlayer* pPly = (CBasePlayer*)ent;
	auto it = g_pShouldPreventTransmitPlayer.find(pPly);
	if (it == g_pShouldPreventTransmitPlayer.end())
		return;

	for (CBaseEntity* pEnt : it->second)
		g_pShouldPrevent[pEnt->entindex()][pPly->entindex()-1] = false;

	g_pShouldPreventTransmitPlayer.erase(it);
}

static Detouring::Hook detour_CBaseEntity_GMOD_SetShouldPreventTransmitToPlayer;
static void hook_CBaseEntity_GMOD_SetShouldPreventTransmitToPlayer(CBaseEntity* pEnt, CBasePlayer* pPly, bool bPreventTransmit)
{
	auto plySet = g_pShouldPreventTransmitPlayer[pPly];
	auto entIT = plySet.find(pEnt);
	if (bPreventTransmit)
	{
		if (entIT == plySet.end())
			plySet.insert(pEnt);

		g_pShouldPrevent[pEnt->entindex()][pPly->entindex()-1] = true;
	} else {
		if (entIT != plySet.end())
			plySet.erase(entIT);

		g_pShouldPrevent[pEnt->entindex()][pPly->entindex()-1] = false;
	}
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

class EntityModule
{
public:
	EntityModule() {}
	EntityModule(CBaseEntity *entity) {}

	virtual ~EntityModule() {}
};

template<typename T>
class AutoList
{
public:
	AutoList()
	{
		AllocList();
		s_List->push_back(static_cast<T *>(this));
	}
	virtual ~AutoList()
	{
		if (s_List != nullptr) {
			s_List->erase(std::find(s_List->begin(), s_List->end(), static_cast<T *>(this)));
			if (s_List->empty()) {
				delete s_List;
				s_List = nullptr;
			}
		}
	}
	
	static const std::vector<T *>& List()
	{
		AllocList();
		return *s_List;
	}
	
private:
	static void AllocList()
	{
		if (s_List == nullptr) {
			s_List = new std::vector<T *>();
		}
	}
	
	static inline std::vector<T *> *s_List = nullptr;
};

typedef void ( *SendPropOverrideCallback )(CBaseEntity *, int, DVariant &, int, SendProp *, uintptr_t data);
struct PropOverride
{
	int id;
	SendPropOverrideCallback callback;
	SendProp *prop;
	int propIndex;
	uintptr_t data;
	bool stopped = false;
};

static int propOverrideId;
bool entityHasOverride[MAX_EDICTS] {};
class SendpropOverrideModule : public EntityModule, public AutoList<SendpropOverrideModule>
{
public:
	SendpropOverrideModule(CBaseEntity *pEntity) : EntityModule(pEntity), m_pEntity(pEntity) {}

	~SendpropOverrideModule();

	int AddOverride(SendPropOverrideCallback callback, const std::string &name, int index = -1, uintptr_t data = 0);
	int AddOverride(SendPropOverrideCallback callback, int indexProp, SendProp *prop, uintptr_t data = 0);
		
	void RemoveOverride(int id);
		
	CBaseEntity *m_pEntity;
	std::vector<PropOverride> propOverrides;
};

SendpropOverrideModule::~SendpropOverrideModule()
{
	entityHasOverride[m_pEntity->entindex()] = false;
}

struct DatatableProxyOffset
{
	SendProp *prop;
	int base;
	int offset;
};

using DatatableProxyVector = std::vector<DatatableProxyOffset>;
bool FindSendProp(int& off, SendTable *s_table, const char *name, SendProp *&prop, DatatableProxyVector &usedTables, int index)
{
	static CStandardSendProxies* sendproxies = Util::servergamedll->GetStandardSendProxies();
	for (int i = 0; i < s_table->GetNumProps(); ++i) {
		SendProp *s_prop = s_table->GetProp(i);
		
		if (s_prop->GetName() != nullptr && strcmp(s_prop->GetName(), name) == 0) {
			off += s_prop->GetOffset();
			if (index >= 0) {
				if (s_prop->GetDataTable() != nullptr && index < s_prop->GetDataTable()->GetNumProps()) {
					prop = s_prop->GetDataTable()->GetProp(index);
					off += prop->GetOffset();
					return true;
				}
				if (s_prop->IsInsideArray()) {
					auto prop_array = s_table->GetProp(i + 1);
					if (prop_array != nullptr && prop_array->GetType() == DPT_Array && index < prop_array->GetNumElements()) {
						off += prop_array->GetElementStride() * index;
					}
				}
			}
			else {
				if (s_prop->IsInsideArray()) {
					off -= s_prop->GetOffset();
					continue;
				}
			}
			if (s_prop->GetDataTable() != nullptr) {
				bool modifying = !CPropMapStack::IsNonPointerModifyingProxy(s_prop->GetDataTableProxyFn(), sendproxies);
				//int oldOffset = usedTables.empty() ? 0 : usedTables.back().second;
				if (modifying) {
					usedTables.push_back({s_prop, off - s_prop->GetOffset(), s_prop->GetOffset()});
					off = 0;
				}
			}
			prop = s_prop;
			return true;
		}
		
		if (s_prop->GetDataTable() != nullptr) {
			bool modifying = !CPropMapStack::IsNonPointerModifyingProxy(s_prop->GetDataTableProxyFn(), sendproxies);
			//int oldOffset = usedTables.empty() ? 0 : usedTables.back().second;
			int oldOffReal = off;
			off += s_prop->GetOffset();
			if (modifying) {
				usedTables.push_back({s_prop, oldOffReal,  off - oldOffReal});
				off = 0;
			}

			if (FindSendProp(off, s_prop->GetDataTable(), name, prop, usedTables, index)) {
				return true;
			}
			off -= s_prop->GetOffset();
			if (modifying) {
				off = oldOffReal;
				usedTables.pop_back();
			}
		}
	}
	
	return false;
}

int FindSendPropPrecalcIndex(SendTable *table, const std::string &name, int index, SendProp *&prop) {
	prop = nullptr;
	int offset = 0;
	DatatableProxyVector usedTables;

	if (FindSendProp(offset, table, name.c_str(), prop, usedTables, index)) {
		auto precalc = table->m_pPrecalc;
		//int indexProp = -1;
		for (int i = 0; i < precalc->m_Props.Count(); i++) {
			if (precalc->m_Props[i] == prop) {
				return i;
			}
		}
	}
	return -1;
}

int SendpropOverrideModule::AddOverride(SendPropOverrideCallback callback, const std::string &name, int index, uintptr_t data)
{
	SendProp *prop = nullptr;
	return this->AddOverride(callback, FindSendPropPrecalcIndex(m_pEntity->GetServerClass()->m_pTable, name, index, prop), prop, data);
}

int SendpropOverrideModule::AddOverride(SendPropOverrideCallback callback, int indexProp, SendProp *prop, uintptr_t data)
{
	if (indexProp != -1) {
		if (prop == nullptr) {
			prop = (SendProp *) m_pEntity->GetServerClass()->m_pTable->m_pPrecalc->m_Props[indexProp];
		}
		auto insertPos = propOverrides.end();
		for (auto it = propOverrides.begin(); it != propOverrides.end(); it++) {
			if (it->propIndex > indexProp) insertPos = it;
		}
		PropOverride overr {++propOverrideId, callback, prop, indexProp, data};
		propOverrides.insert(insertPos, overr);

		entityHasOverride[m_pEntity->entindex()] = true;
		return propOverrideId;
	}
	return -1;
}

void SendpropOverrideModule::RemoveOverride(int id)
{
	for (auto it = propOverrides.begin(); it != propOverrides.end(); it++) {
		auto &overr = *it;
		if (overr.id == id) {
			overr.stopped = true;
			break;
		}
	}
}

static std::unordered_map<CBaseEntity*, EntityModule*> g_pEntityModules;

template<class ModuleType>
inline ModuleType *GetEntityModule(CBaseEntity* pEnt, const char* name)
{
	auto it = g_pEntityModules.find(pEnt);
	if (it != g_pEntityModules.end())
		return static_cast<ModuleType *>(it->second);

	return nullptr;
}

template<class ModuleType>
inline ModuleType* GetOrCreateEntityModule(CBaseEntity* pEnt, const char* name)
{
	auto module = GetEntityModule<ModuleType>(pEnt, name);
	if (module == nullptr) {
		module = new ModuleType(pEnt);
		g_pEntityModules[pEnt] = module;
	}

	return static_cast<ModuleType*>(module);
}

inline void RemoveEntityModule(CBaseEntity* pEnt)
{
	auto it = g_pEntityModules.find(pEnt);
	if (it != g_pEntityModules.end())
	{
		g_pEntityModules.erase(it);
		delete it->second;
	}
}

static thread_local int client_num = 0;
static edict_t* world_edict;
void WriteOverride(const SendTable *pTable,
	const void *pState,
	const int nBits,
	bf_write *pOut,
	const int objectID,
	int *pCheckProps,
	int nCheckProps)
{
	CSendTablePrecalc *pPrecalc = pTable->m_pPrecalc;
	CDeltaBitsWriter deltaBitsWriter( pOut );

	bf_read inputBuffer( "SendTable_WritePropList->inputBuffer", pState, BitByte( nBits ), nBits );
	CDeltaBitsReader inputBitsReader( &inputBuffer );
	// Ok, they want to specify a small list of properties to check.
	unsigned int iToProp = inputBitsReader.ReadNextPropIndex();
	int i = 0;
	auto entity = Util::GetCBaseEntityFromEdict(world_edict+objectID);
	auto mod = GetEntityModule<SendpropOverrideModule>(entity, "sendpropoverride");
	int nextOverridePropIndex = MAX_DATATABLE_PROPS;
	std::vector<PropOverride>::iterator propIterator = mod->propOverrides.begin();
	while (propIterator != mod->propOverrides.end()) {
		if (propIterator->stopped)
			propIterator++;
		else {
			nextOverridePropIndex = propIterator->propIndex;
			propIterator++;
			break;
		}
	}

	while ( i < nCheckProps )
	{
		// Seek the 'to' state to the current property we want to check.
		while ( iToProp < (unsigned int) pCheckProps[i] )
		{
			inputBitsReader.SkipPropData( pPrecalc->m_Props[iToProp] );
			iToProp = inputBitsReader.ReadNextPropIndex();
		}

		if ( iToProp >= MAX_DATATABLE_PROPS )
			break;
			
		if ( iToProp == (unsigned int) pCheckProps[i] )
		{
			const SendProp *pProp = pPrecalc->m_Props[iToProp];

			deltaBitsWriter.WritePropIndex( iToProp );
				
			if (iToProp == (unsigned int) nextOverridePropIndex)
			{
					DecodeInfo decodeInfo;
					decodeInfo.m_pRecvProp = nullptr; // Just skip the data if the proxies are screwed.
					decodeInfo.m_pProp = pProp;
					decodeInfo.m_pIn = &inputBuffer;
					decodeInfo.m_ObjectID = objectID;
					decodeInfo.m_Value.m_Type = (SendPropType)pProp->m_Type;
					g_PropTypeFns[pProp->GetType()].Decode(&decodeInfo);
						
					while (iToProp == (unsigned int) nextOverridePropIndex) {
						PropOverride &rride = *(propIterator-1);
						auto callback = rride.callback;

						(* callback)(entity, client_num, decodeInfo.m_Value, rride.id, rride.prop, rride.data);
							
						while (propIterator != mod->propOverrides.end()) {
							if (propIterator->stopped)
								propIterator++;
							else {
								nextOverridePropIndex = propIterator->propIndex;
								propIterator++;
								break;
							}
						}

						if (propIterator == mod->propOverrides.end())
							break;
					}

					g_PropTypeFns[ pProp->GetType() ].Encode((const unsigned char *)entity, &decodeInfo.m_Value, pProp, pOut, objectID);
			} else {
				inputBitsReader.CopyPropData( deltaBitsWriter.GetBitBuf(), pProp ); 
			}

			// Seek to the next prop.
			iToProp = inputBitsReader.ReadNextPropIndex();
		}

		++i;
	}

	inputBitsReader.ForceFinished(); // avoid a benign assert
}

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

int CheckOverridePropIndex(int *pDeltaProps, int nDeltaProps, const int objectID)
{
	auto mod = GetEntityModule<SendpropOverrideModule>(Util::GetCBaseEntityFromEdict(world_edict+objectID), "sendpropoverride");
	for (auto it = mod->propOverrides.begin(); it != mod->propOverrides.end(); it++) {
		auto &overr = *it;
		bool added = false;
		if (it+1 != mod->propOverrides.end() && (it+1)->propIndex == overr.propIndex) continue;

		for (int i = 0; i < nDeltaProps; i++) {
			auto num = pDeltaProps[i];
			if (num == overr.propIndex) { 
				added = true;
				break;
			}

			if (num > overr.propIndex) { 
				added = true;
				nDeltaProps++;
				for (int j = nDeltaProps - 2; j >= i; j--)
					pDeltaProps[j+1] = pDeltaProps[j];

				pDeltaProps[i] = overr.propIndex;
				break;
			}
		}

		if (!added)
			pDeltaProps[nDeltaProps++] = overr.propIndex;
	}

	return nDeltaProps;
}

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
	client_num = client->m_nEntityIndex;
	CBaseServer_WriteDeltaEntities_func(pServer, client, to, from, pBuf);
	client_num = 0;
}

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
	if (objectID >= 0 && objectID < MAX_EDICTS && entityHasOverride[objectID]) {
		WriteOverride(pTable, pState, nBits, pOut, objectID, (int*)pCheckProps, nCheckProps);
		return;
	}

	SendTable_WritePropList_func(pTable, pState, nBits, pOut, objectID, pCheckProps, nCheckProps);
}

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
	if (client_num != 0 && objectID >= 0 && objectID < 2048 && entityHasOverride[objectID])
		return CheckOverridePropIndex(pDeltaProps, count, objectID);

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
	for (auto mod : AutoList<SendpropOverrideModule>::List()) {
		for (auto it = mod->propOverrides.begin(); it != mod->propOverrides.end();) {
			auto &overr = *it;
			if (overr.stopped)
				it = mod->propOverrides.erase(it);
			else
				it++;
		}

		if (mod->propOverrides.empty())
			entityHasOverride[mod->m_pEntity->entindex()] = false;
	}
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

/*
 * For now this is called from the pvs module meaning we RELY on it.
 * What did we change? basicly nothing yet. I'm just testing around.
 * 
 * NOTE: It's shit & somehow were loosing performance to something. Probably us detouring it is causing our performance loss.
 */
extern IMDLCache* mdlcache;
static ConVar* sv_force_transmit_ents;
static CBaseEntity* g_pEntityCache[MAX_EDICTS];
bool g_pReplaceCServerGameEnts_CheckTransmit = false;
void New_CServerGameEnts_CheckTransmit(IServerGameEnts* gameents, CCheckTransmitInfo *pInfo, const unsigned short *pEdictIndices, int nEdicts)
{
	// get recipient player's skybox:
	CBaseEntity *pRecipientEntity = CBaseEntity::Instance( pInfo->m_pClientEnt );

	//Assert( pRecipientEntity && pRecipientEntity->IsPlayer() );
	if ( !pRecipientEntity )
		return;
	
	MDLCACHE_CRITICAL_SECTION();
	CBasePlayer *pRecipientPlayer = static_cast<CBasePlayer*>( pRecipientEntity );
	const int skyBoxArea = pRecipientPlayer->m_Local.m_skybox3d.area;
	
	// ToDo: Bring over's CS:GO code for InitialSpawnTime
	//const bool bIsFreshlySpawned = pRecipientPlayer->GetInitialSpawnTime()+3.0f > gpGlobals->curtime;

#ifndef _X360
	const bool bIsHLTV = pRecipientPlayer->IsHLTV();
	//const bool bIsReplay = pRecipientPlayer->IsReplay();

	// m_pTransmitAlways must be set if HLTV client
	Assert( bIsHLTV == ( pInfo->m_pTransmitAlways != NULL) );
#endif

	bool bForceTransmit = sv_force_transmit_ents->GetBool();
	int iPlayerIndex = pInfo->m_pClientEnt->m_EdictIndex - 1;
	for ( int i=0; i < nEdicts; i++ )
	{
		int iEdict = pEdictIndices[i];

		edict_t *pEdict = &world_edict[iEdict]; // world_edict is already cached.
		// Assert( pEdict == engine->PEntityOfEntIndex( iEdict ) );
		int nFlags = pEdict->m_fStateFlags & (FL_EDICT_DONTSEND|FL_EDICT_ALWAYS|FL_EDICT_PVSCHECK|FL_EDICT_FULLCHECK);

		// entity needs no transmit
		if ( nFlags & FL_EDICT_DONTSEND )
			continue;
		
		// entity is already marked for sending
		if ( pInfo->m_pTransmitEdict->Get( iEdict ) )
			continue;

		if ( g_pShouldPrevent[i][iPlayerIndex] ) // Implements gmod's SetPreventTransmit but hopefully faster.
			continue;
		
		if ( nFlags & FL_EDICT_ALWAYS )
		{
			// FIXME: Hey! Shouldn't this be using SetTransmit so as 
			// to also force network down dependent entities?
			while ( true )
			{
				// mark entity for sending
				pInfo->m_pTransmitEdict->Set( iEdict );
	
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

		if ( nFlags == FL_EDICT_FULLCHECK )
		{
			// do a full ShouldTransmit() check, may return FL_EDICT_CHECKPVS
			nFlags = pEnt->ShouldTransmit( pInfo );

			// Assert( !(nFlags & FL_EDICT_FULLCHECK) );

			if ( nFlags & FL_EDICT_ALWAYS )
			{
				pEnt->SetTransmit( pInfo, true );
				continue;
			}	
		}

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
			int checkIndex = check->entindex();

			// Parent already being sent
			if ( pInfo->m_pTransmitEdict->Get( checkIndex ) )
			{
				pEnt->SetTransmit( pInfo, true );
				break;
			}

			edict_t *checkEdict = check->edict();
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
				CBaseEntity *pCheckEntity = check->GetBaseEntity();
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

//	Msg("A:%i, N:%i, F: %i, P: %i\n", always, dontSend, fullCheck, PVS );
}

void CNetworkingModule::OnEntityDeleted(CBaseEntity* pEntity)
{
	CleaupSetPreventTransmit(pEntity);
	RemoveEntityModule(pEntity);
	g_pEntityCache[pEntity->entindex()] = NULL;
}

void CNetworkingModule::OnEntityCreated(CBaseEntity* pEntity)
{
	//auto mod = GetOrCreateEntityModule<SendpropOverrideModule>(pEntity, "sendpropoverride");
	//mod->AddOverride(CallbackPluginCall, precalcIndex, prop, callbacks.size() - 1);
	g_pEntityCache[pEntity->entindex()] = pEntity;
}

static SendTable* playerSendTable;
static ServerClass* playerServerClass;
static CSharedEdictChangeInfo* g_SharedEdictChangeInfo = nullptr;
static CFrameSnapshotManager* framesnapshotmanager = NULL;
static ServerClassCache *player_class_cache = nullptr;
static CStandardSendProxies* sendproxies;
static SendTableProxyFn datatable_sendtable_proxy;
PropTypeFns g_PropTypeFns[DPT_NUMSendPropTypes];
void CNetworkingModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

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
		&detour_CBaseEntity_GMOD_ShouldPreventTransmitToPlayer, "CBaseEntity_GMOD_ShouldPreventTransmitToPlayer",
		server_loader.GetModule(), Symbols::CBaseEntity_GMOD_ShouldPreventTransmitToPlayerSym,
		(void*)hook_CBaseEntity_GMOD_ShouldPreventTransmitToPlayer, m_pID
	);

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
	
	for (size_t i = 0; i < DPT_NUMSendPropTypes; ++i)
		g_PropTypeFns[i] = pPropTypeFns[i]; // Crash any% speed run. I don't believe this will work

	//g_pReplaceCServerGameEnts_CheckTransmit = true;
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
	//g_pReplaceCServerGameEnts_CheckTransmit = false;

	if (!framesnapshotmanager) // If we failed, we failed
	{
		Msg(PROJECT_NAME ": Failed to find framesnapshotmanager. Unable to fully unload!\n");
		return;
	}

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