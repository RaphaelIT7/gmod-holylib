#include "player.h"
#include "positionwatcher.h"

//-----------------------------------------------------------------------------
// Purpose: Calculates the absolute position of an edict in the world
//			assumes the parent's absolute origin has already been calculated
//-----------------------------------------------------------------------------
void CBaseEntity::CalcAbsolutePosition(void)
{
	// ToDo: Load Gmod's version and call it here. For not it'll be empty
}

CBaseEntity* CGlobalEntityList::NextEnt(CBaseEntity* pCurrentEnt)
{
	if (!pCurrentEnt)
	{
		const CEntInfo* pInfo = FirstEntInfo();
		if (!pInfo)
			return NULL;

		return (CBaseEntity*)pInfo->m_pEntity;
	}

	// Run through the list until we get a CBaseEntity.
	const CEntInfo* pList = GetEntInfoPtr(pCurrentEnt->GetRefEHandle());
	if (pList)
		pList = NextEntInfo(pList);

	while (pList)
	{
#if 0
		if (pList->m_pEntity)
		{
			IServerUnknown* pUnk = static_cast<IServerUnknown*>(const_cast<IHandleEntity*>(pList->m_pEntity));
			CBaseEntity* pRet = pUnk->GetBaseEntity();
			if (pRet)
				return pRet;
		}
#else
		return (CBaseEntity*)pList->m_pEntity;
#endif
		pList = pList->m_pNext;
	}

	return NULL;

}