//HOLYLIB_REQUIRES_MODULE=networking

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
	inline void AttachEdict( edict_t *pRequiredEdict = nullptr );
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
	return pParent ? (CCServerNetworkProperty*)pParent->NetworkProp() : nullptr;
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