#include "filesystem_base.h" // Has to be before symbols.h
#include "LuaInterface.h"
#include "symbols.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "vprof.h"
#include "framesnapshot.h"
#include "packed_entity.h"
#include "server_class.h"
#include "dt.h"
#include "edict.h"

class CNetworkingModule : public IModule
{
public:
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual const char* Name() { return "networking"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; };
};

/*
 * This module can't be disabled at runtime!
 * 
 * This is because of it replacing the entire CChangeFrameList class which is used & stored in the engine.
 * We would have to add a check to the Copy function to then slowly switch to the original until it's not used anymore.
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
		VPROF_BUDGET("CChangeFrameList::Copy", VPROF_BUDGETGROUP_OTHER_NETWORKING);
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

CFrameSnapshotManager* framesnapshotmanager = NULL;
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

	framesnapshotmanager = Detour::ResolveSymbol<CFrameSnapshotManager>(engine_loader, Symbols::g_FrameSnapshotManagerSym);
	Detour::CheckValue("get class", "framesnapshotmanager", framesnapshotmanager != NULL);
}

extern CGlobalVars *gpGlobals;
void CNetworkingModule::Shutdown()
{
	if (!framesnapshotmanager) // If we failed, we failed
	{
		Msg("holylib: Failed to find framesnapshotmanager. Unable to fully unload!\n");
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