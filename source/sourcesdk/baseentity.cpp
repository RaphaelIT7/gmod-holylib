#include "player.h"
#include "positionwatcher.h"

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
	}

	return NULL;

}

void CGlobalEntityList::AddListenerEntity(IEntityListener* pListener)
{
	if (m_entityListeners.Find(pListener) >= 0)
	{
		AssertMsg(0, "Can't add listeners multiple times\n");
		return;
	}
	m_entityListeners.AddToTail(pListener);
}

void CGlobalEntityList::RemoveListenerEntity(IEntityListener* pListener)
{
	m_entityListeners.FindAndRemove(pListener);
}

bool CCollisionProperty::DoesVPhysicsInvalidateSurroundingBox( ) const
{
	switch ( m_nSurroundType )
	{
	case USE_BEST_COLLISION_BOUNDS:
		return true;

	case USE_OBB_COLLISION_BOUNDS:
		return (GetSolid() == SOLID_VPHYSICS) && (GetOuter()->GetMoveType() == MOVETYPE_VPHYSICS) && GetOuter()->VPhysicsGetObject();

	// In the case of game code, we don't really know, so we have to assume it does
	case USE_GAME_CODE:
		return true;

	case USE_COLLISION_BOUNDS_NEVER_VPHYSICS:
	case USE_HITBOXES:
	case USE_ROTATION_EXPANDED_BOUNDS:
	case USE_SPECIFIED_BOUNDS:
		return false;

	default:
		Assert(0);
		return true;
	}
}
