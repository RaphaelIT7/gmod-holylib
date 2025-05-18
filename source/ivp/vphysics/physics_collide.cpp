//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"

#include "ivp_surbuild_pointsoup.hxx"
#include "ivp_surbuild_ledge_soup.hxx"
#include "ivp_surman_polygon.hxx"
#include "ivp_compact_surface.hxx"
#include "ivp_compact_ledge.hxx"
#include "ivp_compact_ledge_solver.hxx"
#include "ivp_halfspacesoup.hxx"
#include "ivp_surbuild_halfspacesoup.hxx"
#include "ivp_template_surbuild.hxx"
#include "hk_mopp/ivp_surbuild_mopp.hxx"
#include "hk_mopp/ivp_surman_mopp.hxx"
#include "hk_mopp/ivp_compact_mopp.hxx"
#include "ivp_surbuild_polygon_convex.hxx"
#include "ivp_templates_intern.hxx"

#include "cmodel.h"
#include "physics_trace.h"
#include "vcollide_parse_private.h"
#include "physics_virtualmesh.h"

#include "mathlib/polyhedron.h"
#include "tier1/byteswap.h"

#include <GarrysMod/FactoryLoader.hpp>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CPhysCollideCompactSurface;
struct bboxcache_t
{
	Vector			mins;
	Vector			maxs;
	CPhysCollideCompactSurface	*pCollide;
};

class CPhysicsCollision : public IPhysicsCollision
{
public:
	CPhysicsCollision()
	{
		memset(m_bboxVertMap, 0, sizeof(m_bboxVertMap));
	}
	CPhysConvex	*ConvexFromVerts( Vector **pVerts, int vertCount ) override;
	CPhysConvex	*ConvexFromVertsFast( Vector **pVerts, int vertCount );
	CPhysConvex	*ConvexFromPlanes( float *pPlanes, int planeCount, float mergeDistance ) override;
	CPhysConvex *ConvexFromConvexPolyhedron( const CPolyhedron &ConvexPolyhedron ) override;
	void ConvexesFromConvexPolygon( const Vector &vPolyNormal, const Vector *pPoints, int iPointCount, CPhysConvex **pOutput ) override;
	CPhysConvex	*RebuildConvexFromPlanes( CPhysConvex *pConvex, float mergeDistance );
	float ConvexVolume( CPhysConvex *pConvex ) override;
	float ConvexSurfaceArea( CPhysConvex *pConvex ) override;
	CPhysCollide *ConvertConvexToCollide( CPhysConvex **pConvex, int convexCount ) override;
	CPhysCollide *ConvertConvexToCollideParams( CPhysConvex **pConvex, int convexCount, const convertconvexparams_t &convertParams ) override;

	
	CPolyhedron *PolyhedronFromConvex( CPhysConvex * const pConvex, bool bUseTempPolyhedron ) override;
	int GetConvexesUsedInCollideable( const CPhysCollide *pCollideable, CPhysConvex **pOutputArray, int iOutputArrayLimit ) override;

	// store game-specific data in a convex solid
	void SetConvexGameData( CPhysConvex *pConvex, unsigned int gameData ) override;
	void ConvexFree( CPhysConvex *pConvex ) override;
	
	CPhysPolysoup *PolysoupCreate( void ) override;
	void PolysoupDestroy( CPhysPolysoup *pSoup ) override;
	void PolysoupAddTriangle( CPhysPolysoup *pSoup, const Vector &a, const Vector &b, const Vector &c, int materialIndex7bits ) override;
	CPhysCollide *ConvertPolysoupToCollide( CPhysPolysoup *pSoup, bool useMOPP = true ) override;

	int CollideSize(CPhysCollide *pCollide) override;
	int CollideWrite( char *pDest, CPhysCollide *pCollide, bool bSwap = false ) override;
	// Get the AABB of an oriented collide
	void CollideGetAABB( Vector *pMins, Vector *pMaxs, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles ) override;
	Vector CollideGetExtent( const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, const Vector &direction ) override;
	// compute the volume of a collide
	float			CollideVolume( CPhysCollide *pCollide ) override;
	float			CollideSurfaceArea( CPhysCollide *pCollide ) override;

	// Free a collide that was created with ConvertConvexToCollide()
	// UNDONE: Move this up near the other Collide routines when the version is changed
	void			DestroyCollide( CPhysCollide *pCollide ) override;

	CPhysCollide *BBoxToCollide( const Vector &mins, const Vector &maxs ) override;
	CPhysConvex	*BBoxToConvex( const Vector &mins, const Vector &maxs ) override;

	// loads a set of solids into a vcollide_t
	void			VCollideLoad( vcollide_t *pOutput, int solidCount, const char *pBuffer, int size, bool swap ) override;
	// destroyts the set of solids created by VCollideLoad
	void			VCollideUnload( vcollide_t *pVCollide ) override;

	// Trace an AABB against a collide
	void TraceBox( const Vector &start, const Vector &end, const Vector &mins, const Vector &maxs, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr ) override;
	void TraceBox( const Ray_t &ray, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr ) override;
	void TraceBox( const Ray_t &ray, unsigned int contentsMask, IConvexInfo *pConvexInfo, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr ) override;
	// Trace one collide against another
	void TraceCollide( const Vector &start, const Vector &end, const CPhysCollide *pSweepCollide, const QAngle &sweepAngles, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr ) override;
	bool IsBoxIntersectingCone( const Vector &boxAbsMins, const Vector &boxAbsMaxs, const truncatedcone_t &cone ) override;

	// begins parsing a vcollide.  NOTE: This keeps pointers to the text
	// If you delete the text and call members of IVPhysicsKeyParser, it will crash
	IVPhysicsKeyParser	*VPhysicsKeyParserCreate( const char *pKeyData ) override;

#if PLATFORM_64BITS
	IVPhysicsKeyParser	*VPhysicsKeyParserCreate( vcollide_t *pVCollide ) override;
#endif

	// Free the parser created by VPhysicsKeyParserCreate
	void			VPhysicsKeyParserDestroy( IVPhysicsKeyParser *pParser ) override;

	// creates a list of verts from a collision mesh
	int	CreateDebugMesh( const CPhysCollide *pCollisionModel, Vector **outVerts ) override;
	// destroy the list of verts created by CreateDebugMesh
	void DestroyDebugMesh( int vertCount, Vector *outVerts ) override;
	// create a queryable version of the collision model
	ICollisionQuery *CreateQueryModel( CPhysCollide *pCollide ) override;
	// destroy the queryable version
	void DestroyQueryModel( ICollisionQuery *pQuery ) override;

	IPhysicsCollision *ThreadContextCreate( void ) override;
	void			ThreadContextDestroy( IPhysicsCollision *pThreadContex ) override;
	unsigned int	ReadStat( int ) override { return 0; }
	void			CollideGetMassCenter( CPhysCollide *pCollide, Vector *pOutMassCenter ) override;
	void			CollideSetMassCenter( CPhysCollide *pCollide, const Vector &massCenter ) override;

	int				CollideIndex( const CPhysCollide *pCollide ) override;
	Vector			CollideGetOrthographicAreas( const CPhysCollide *pCollide ) override;
	void			OutputDebugInfo( const CPhysCollide *pCollide ) override;
	CPhysCollide	*CreateVirtualMesh(const virtualmeshparams_t &params) override { return ::CreateVirtualMesh(params); }
	bool			GetBBoxCacheSize( int *pCachedSize, int *pCachedCount ) override;

	bool			SupportsVirtualMesh() override { return true; }

	CPhysCollide	*UnserializeCollide( char *pBuffer, int size, int index ) override;
	void			CollideSetOrthographicAreas( CPhysCollide *pCollide, const Vector &areas ) override;

#if PLATFORM_64BITS
	// Get an AABB for an oriented collision model
	float			CollideGetRadius( const CPhysCollide *pCollide ) override;
	void			*VCollideAllocUserData( vcollide_t *pVCollide, size_t userDataSize ) override;
	void			VCollideFreeUserData( vcollide_t *pVCollide ) override;
	void			VCollideCheck( vcollide_t *pVCollide, const char *pName ) override;
	bool			TraceBoxAA( const Ray_t &ray, const CPhysCollide *pCollide, trace_t *ptr ) override;
	void			DuplicateAndScale( vcollide_t *pOut, const vcollide_t *pIn, float flScale ) override;
#endif

private:
	void InitBBoxCache();
	bool IsBBoxCache( CPhysCollide *pCollide );
	void AddBBoxCache( CPhysCollideCompactSurface *pCollide, const Vector &mins, const Vector &maxs );
	CPhysCollideCompactSurface *GetBBoxCache( const Vector &mins, const Vector &maxs );
	CPhysCollideCompactSurface *FastBboxCollide( const CPhysCollideCompactSurface *pCollide, const Vector &mins, const Vector &maxs );

private:
	CPhysicsTrace		m_traceapi;
	CUtlVector<bboxcache_t>	m_bboxCache;
	byte				m_bboxVertMap[8];
};

CPhysicsCollision g_PhysicsCollision;
IPhysicsCollision *physcollision = &g_PhysicsCollision;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CPhysicsCollision, IPhysicsCollision, VPHYSICS_COLLISION_INTERFACE_VERSION, g_PhysicsCollision );


//-----------------------------------------------------------------------------
// Abstract compact_surface vs. compact_mopp
//-----------------------------------------------------------------------------
#define IVP_COMPACT_SURFACE_ID			MAKEID('I','V','P','S')
#define IVP_COMPACT_SURFACE_ID_SWAPPED	MAKEID('S','P','V','I')
#define IVP_COMPACT_MOPP_ID				MAKEID('M','O','P','P')
#define VPHYSICS_COLLISION_ID			MAKEID('V','P','H','Y')
#define VPHYSICS_COLLISION_VERSION		0x0100
// You can disable all of the havok Mopp collision model building by undefining this symbol
#define ENABLE_IVP_MOPP	0

struct physcollideheader_t
{
	DECLARE_BYTESWAP_DATADESC();
	int		vphysicsID;
	short	version;
	short	modelType;

	void Defaults( short inputModelType )
	{
		vphysicsID = VPHYSICS_COLLISION_ID;
		
		version = VPHYSICS_COLLISION_VERSION;
		modelType = inputModelType;
	}
};

struct compactsurfaceheader_t : public physcollideheader_t
{
	DECLARE_BYTESWAP_DATADESC();
	int		surfaceSize;
	Vector	dragAxisAreas;
	int		axisMapSize;

	void CompactSurface( const IVP_Compact_Surface *pSurface, const Vector &orthoAreas )
	{
		Defaults( COLLIDE_POLY );
		surfaceSize = pSurface->byte_size;
		dragAxisAreas = orthoAreas;
		axisMapSize = 0;	// NOTE: not yet supported
	}
};

BEGIN_BYTESWAP_DATADESC( physcollideheader_t )
	DEFINE_FIELD( vphysicsID, FIELD_INTEGER ),
	DEFINE_FIELD( version, FIELD_SHORT),
	DEFINE_FIELD( modelType, FIELD_SHORT ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC_( compactsurfaceheader_t, physcollideheader_t )
	DEFINE_FIELD( surfaceSize, FIELD_INTEGER ),
	DEFINE_FIELD( dragAxisAreas, FIELD_VECTOR ),
	DEFINE_FIELD( axisMapSize, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

#if ENABLE_IVP_MOPP
struct moppheader_t : public physcollideheader_t
{
	int moppSize;
	void Mopp( const IVP_Compact_Mopp *pMopp )
	{
		Defaults( COLLIDE_MOPP );
		moppSize = pMopp->byte_size;
	}
};
#endif

#if ENABLE_IVP_MOPP
class CPhysCollideMopp : public CPhysCollide
{
public:
	CPhysCollideMopp( const moppheader_t *pHeader );
	CPhysCollideMopp( IVP_Compact_Mopp *pMopp );
	CPhysCollideMopp( const char *pBuffer, unsigned int size );
	~CPhysCollideMopp();

	void Init( const char *pBuffer, unsigned int size );

	// IPhysCollide
	virtual int GetVCollideIndex() const { return 0; }
	virtual IVP_SurfaceManager *CreateSurfaceManager( short & ) const;
	virtual void GetAllLedges( IVP_U_BigVector<IVP_Compact_Ledge> &ledges ) const;
	virtual size_t GetSerializationSize() const;
	virtual Vector GetMassCenter() const;
	virtual void SetMassCenter( const Vector &massCenter );
	virtual size_t SerializeToBuffer( char *pDest, bool bSwap = false ) const;
	virtual void OutputDebugInfo() const;

private:
	IVP_Compact_Mopp		*m_pMopp;
};
#endif

class CPhysCollideCompactSurface : public CPhysCollide
{
public:
	~CPhysCollideCompactSurface();
	CPhysCollideCompactSurface( const char *pBuffer, unsigned int size, int index, bool swap = false );
	CPhysCollideCompactSurface( const compactsurfaceheader_t *pHeader, int index, bool swap = false );
	CPhysCollideCompactSurface( IVP_Compact_Surface *pSurface );
	
	void Init( const char *pBuffer, unsigned int size, int index, bool swap = false );

	// IPhysCollide
	int GetVCollideIndex() const override { return m_pCompactSurface->dummy[0]; }
	IVP_SurfaceManager *CreateSurfaceManager( short & ) const override;
	void GetAllLedges( IVP_U_BigVector<IVP_Compact_Ledge> &ledges ) const override;
	size_t GetSerializationSize() const override;
	Vector GetMassCenter() const override;
	void SetMassCenter( const Vector &massCenter ) override;
	size_t SerializeToBuffer( char *pDest, bool bSwap = false ) const override;
	Vector GetOrthographicAreas() const override;
	void SetOrthographicAreas( const Vector &areas ) override;
	void ComputeOrthographicAreas( float epsilon ) override;
	void OutputDebugInfo() const override;

	const IVP_Compact_Surface		*GetCompactSurface() const override { return m_pCompactSurface; }
	const collidemap_t *GetCollideMap() const override { return m_pCollideMap; }

private:

	struct hullinfo_t
	{
		hullinfo_t()
		{
			hasOuterHull = false;
			convexCount = 0;
		}
		bool	hasOuterHull;
		int		convexCount;
	};

	void ComputeHullInfo_r( hullinfo_t *pOut, const IVP_Compact_Ledgetree_Node *node ) const;
	void InitCollideMap();

	IVP_Compact_Surface		*m_pCompactSurface;
	Vector					m_orthoAreas;
	collidemap_t			*m_pCollideMap;
};


static const IVP_Compact_Surface *ConvertPhysCollideToCompactSurface( const CPhysCollide *pCollide )
{
	DebugPrint();
	return pCollide->GetCompactSurface();
}

IVP_SurfaceManager *CreateSurfaceManager( const CPhysCollide *pCollisionModel, short &collideType )
{
	DebugPrint();
	return pCollisionModel ? pCollisionModel->CreateSurfaceManager( collideType ) : NULL;
}

void OutputCollideDebugInfo( const CPhysCollide *pCollisionModel )
{
	DebugPrint();
	pCollisionModel->OutputDebugInfo();
}


CPhysCollide *CPhysCollide::UnserializeFromBuffer( const char *pBuffer, unsigned int size, int index, bool swap )
{
	DebugPrint();
	const physcollideheader_t *pHeader = reinterpret_cast<const physcollideheader_t *>(pBuffer);
	if ( pHeader->vphysicsID == VPHYSICS_COLLISION_ID )
	{
		Assert(pHeader->version == VPHYSICS_COLLISION_VERSION);
		switch( pHeader->modelType )
		{
		case COLLIDE_POLY:
			return new CPhysCollideCompactSurface( (const compactsurfaceheader_t *)pHeader, index, swap );
		case COLLIDE_MOPP:
#if ENABLE_IVP_MOPP
			return new CPhysCollideMopp( (moppheader_t *)pHeader );
#else
			DevMsg( 2, "Null physics model\n");
			return NULL;
#endif
		default:
			Assert(0);
			return NULL;
		}
	}
	const IVP_Compact_Surface *pSurface = reinterpret_cast<const IVP_Compact_Surface *>(pBuffer);
	if ( pSurface->dummy[2] == IVP_COMPACT_MOPP_ID )
	{
#if ENABLE_IVP_MOPP
		return new CPhysCollideMopp( pBuffer, size );
#else
		Assert(0);
		return NULL;
#endif
	}
	if ( pSurface->dummy[2] == IVP_COMPACT_SURFACE_ID || 
		 pSurface->dummy[2] == IVP_COMPACT_SURFACE_ID_SWAPPED || 
		 pSurface->dummy[2] == 0 )
	{
		if ( pSurface->dummy[2] == 0 )
		{
			// UNDONE: Print a name here?
			DevMsg( 1, "Old format .PHY file loaded!!!\n" );
		}
		return new CPhysCollideCompactSurface( pBuffer, size, index, swap );
	}

	Assert(0);
	return NULL;
}

#if ENABLE_IVP_MOPP

void CPhysCollideMopp::Init( const char *pBuffer, unsigned int size )
{
	DebugPrint();
	m_pMopp = (IVP_Compact_Mopp *)ivp_malloc_aligned( size, 32 );
	memcpy( m_pMopp, pBuffer, size );
}

CPhysCollideMopp::CPhysCollideMopp( const char *pBuffer, unsigned int size )
{
	DebugPrint();
	Init( pBuffer, size );
}

CPhysCollideMopp::CPhysCollideMopp( const moppheader_t *pHeader )
{
	DebugPrint();
	Init( (const char *)(pHeader+1), pHeader->moppSize );
}

CPhysCollideMopp::CPhysCollideMopp( IVP_Compact_Mopp *pMopp )
{
	DebugPrint();
	m_pMopp = pMopp;
	pMopp->dummy = IVP_COMPACT_MOPP_ID;
}

CPhysCollideMopp::~CPhysCollideMopp()
{
	DebugPrint();
	ivp_free_aligned(m_pMopp);
}

void CPhysCollideMopp::GetAllLedges( IVP_U_BigVector<IVP_Compact_Ledge> &ledges ) const
{
	DebugPrint();
	IVP_Compact_Ledge_Solver::get_all_ledges( m_pMopp, &ledges );
}

IVP_SurfaceManager *CPhysCollideMopp::CreateSurfaceManager( short &collideType ) const
{
	DebugPrint();
	collideType = COLLIDE_MOPP;
	return new IVP_SurfaceManager_Mopp( m_pMopp );
}

size_t CPhysCollideMopp::GetSerializationSize() const
{
	DebugPrint();
	return m_pMopp->byte_size + sizeof(moppheader_t);
}

size_t CPhysCollideMopp::SerializeToBuffer( char *pDest, bool bSwap ) const
{
	DebugPrint();
	moppheader_t header;
	header.Mopp( m_pMopp );
	memcpy( pDest, &header, sizeof(header) );
	pDest += sizeof(header);
	memcpy( pDest, m_pMopp, m_pMopp->byte_size );
	return GetSerializationSize();
}

Vector CPhysCollideMopp::GetMassCenter() const
{
	DebugPrint();
	Vector massCenterHL;
	ConvertPositionToHL( m_pMopp->mass_center, massCenterHL );
	return massCenterHL;
}

void CPhysCollideMopp::SetMassCenter( const Vector &massCenterHL )
{
	DebugPrint();
	ConvertPositionToIVP( massCenterHL, m_pMopp->mass_center );
}

void CPhysCollideMopp::OutputDebugInfo() const
{
	DebugPrint();
	Msg("CollisionModel: MOPP\n");
}
#endif

void CPhysCollideCompactSurface::InitCollideMap()
{
	DebugPrint();
	m_pCollideMap = NULL;
	if ( m_pCompactSurface )
	{
		IVP_U_BigVector<IVP_Compact_Ledge> ledges;
		GetAllLedges( ledges );
		// don't make these for really large models because there's a linear search involved in using this atm.
		if ( !ledges.len() || ledges.len() > 32 )
			return;
		int allocSize = sizeof(collidemap_t) + ((ledges.len()-1) * sizeof(leafmap_t));
		m_pCollideMap = (collidemap_t *)malloc(allocSize);
		m_pCollideMap->leafCount = ledges.len();
		for ( int i = 0; i < ledges.len(); i++ )
		{
			InitLeafmap( ledges.element_at(i), &m_pCollideMap->leafmap[i] );
		}
	}
}

void CPhysCollideCompactSurface::Init( const char *pBuffer, unsigned int size, int index, bool bSwap )
{
	DebugPrint();
	m_pCompactSurface = (IVP_Compact_Surface *)ivp_malloc_aligned( size, 32 );
	memcpy( m_pCompactSurface, pBuffer, size );
	if ( bSwap )
	{
		m_pCompactSurface->byte_swap_all();
	}
	m_pCompactSurface->dummy[0] = index;
	m_orthoAreas.Init(1,1,1);
	InitCollideMap();
}

CPhysCollideCompactSurface::CPhysCollideCompactSurface( const char *pBuffer, unsigned int size, int index, bool swap )
{
	DebugPrint();
	Init( pBuffer, size, index, swap );
}
CPhysCollideCompactSurface::CPhysCollideCompactSurface( const compactsurfaceheader_t *pHeader, int index, bool swap )
{
	DebugPrint();
	Init( (const char *)(pHeader+1), pHeader->surfaceSize, index, swap );
	m_orthoAreas = pHeader->dragAxisAreas;
}

CPhysCollideCompactSurface::CPhysCollideCompactSurface( IVP_Compact_Surface *pSurface )
{
	DebugPrint();
	m_pCompactSurface = pSurface;
	pSurface->dummy[2] = IVP_COMPACT_SURFACE_ID;
	m_pCompactSurface->dummy[0] = 0;
	m_orthoAreas.Init(1,1,1);
	InitCollideMap();
}

CPhysCollideCompactSurface::~CPhysCollideCompactSurface()
{
	DebugPrint();
	ivp_free_aligned(m_pCompactSurface);
	if ( m_pCollideMap )
	{
		free(m_pCollideMap);
	}
}

IVP_SurfaceManager *CPhysCollideCompactSurface::CreateSurfaceManager( short &collideType ) const
{
	DebugPrint();
	collideType = COLLIDE_POLY;
	return new IVP_SurfaceManager_Polygon( m_pCompactSurface );
}

void CPhysCollideCompactSurface::GetAllLedges( IVP_U_BigVector<IVP_Compact_Ledge> &ledges ) const
{
	DebugPrint();
	IVP_Compact_Ledge_Solver::get_all_ledges( m_pCompactSurface, &ledges );
}

size_t CPhysCollideCompactSurface::GetSerializationSize() const
{
	DebugPrint();
	return m_pCompactSurface->byte_size + sizeof(compactsurfaceheader_t);
}

size_t CPhysCollideCompactSurface::SerializeToBuffer( char *pDest, bool bSwap ) const
{
	DebugPrint();
	compactsurfaceheader_t header;
	header.CompactSurface( m_pCompactSurface, m_orthoAreas );
	if ( bSwap )
	{
		CByteswap swap;
		swap.ActivateByteSwapping( true );
		swap.SwapFieldsToTargetEndian( &header );
	}
	memcpy( pDest, &header, sizeof(header) );
	pDest += sizeof(header);
	int surfaceSize = m_pCompactSurface->byte_size;
	size_t serializationSize = GetSerializationSize();
	if ( bSwap )
	{
		m_pCompactSurface->byte_swap_all();
	}
	memcpy( pDest, m_pCompactSurface, surfaceSize );
	return serializationSize;
}

Vector CPhysCollideCompactSurface::GetMassCenter() const
{
	DebugPrint();
	Vector massCenterHL;
	ConvertPositionToHL( m_pCompactSurface->mass_center, massCenterHL );
	return massCenterHL;
}

void CPhysCollideCompactSurface::SetMassCenter( const Vector &massCenterHL )
{
	DebugPrint();
	ConvertPositionToIVP( massCenterHL, m_pCompactSurface->mass_center );
}

Vector CPhysCollideCompactSurface::GetOrthographicAreas() const
{
	DebugPrint();
	return m_orthoAreas;
}

void CPhysCollideCompactSurface::SetOrthographicAreas( const Vector &areas )
{
	DebugPrint();
	m_orthoAreas = areas;
}


void CPhysCollideCompactSurface::ComputeOrthographicAreas( float epsilon )
{
	DebugPrint();
	Vector mins, maxs;

	physcollision->CollideGetAABB( &mins, &maxs, this, vec3_origin, vec3_angle );
	float side = sqrt( epsilon );
	if ( side < 1e-4f )
		side = 1e-4f;

	m_orthoAreas.Init(1,1,1);
	trace_t tr;
	for ( int axis = 0; axis < 3; axis++ )
	{
		int u = (axis+1)%3;
		int v = (axis+2)%3;
		int hits = 0;
		int total = 0;
		float halfSide = side * 0.5f;
		for ( float u0 = mins[u] + halfSide; u0 < maxs[u]; u0 += side )
		{
			for ( float v0 = mins[v] + halfSide; v0 < maxs[v]; v0 += side )
			{
				Vector start, end;
				start[axis] = mins[axis]-1;
				end[axis] = maxs[axis]+1;
				start[u] = u0;
				end[u] = u0;
				start[v] = v0;
				end[v] = v0;

				physcollision->TraceBox( start, end, vec3_origin, vec3_origin, this, vec3_origin, vec3_angle, &tr );
				if ( tr.DidHit() )
				{
					hits++;
				}
				total++;
			}
		}
	
		if ( total <= 0 )
			total = 1;
		m_orthoAreas[axis] = (float)hits / (float)total;
	}
}


void CPhysCollideCompactSurface::ComputeHullInfo_r( hullinfo_t *pOut, const IVP_Compact_Ledgetree_Node *node ) const
{
	DebugPrint();
	if ( !node->is_terminal() )
	{
		if ( node->get_compact_hull() )
			pOut->hasOuterHull = true;

		ComputeHullInfo_r( pOut, node->left_son() );
		ComputeHullInfo_r( pOut, node->right_son() );
	}
	else
	{
		// terminal node, add one ledge
		pOut->convexCount++;
	}
}


void CPhysCollideCompactSurface::OutputDebugInfo() const
{
	DebugPrint();
	hullinfo_t info;

	ComputeHullInfo_r( &info, m_pCompactSurface->get_compact_ledge_tree_root() );
	const char *pOuterHull = info.hasOuterHull ? "with" : "no";
	Msg("CollisionModel: Compact Surface: %d convex pieces %s outer hull\n", info.convexCount, pOuterHull );
}

//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: Create a convex element from a point cloud
// Input  : **pVerts - array of points
//			vertCount - length of array
// Output : opaque pointer to convex element
//-----------------------------------------------------------------------------
CPhysConvex	*CPhysicsCollision::ConvexFromVertsFast( Vector **pVerts, int vertCount )
{
	DebugPrint();
	IVP_U_Vector<IVP_U_Point> points;
	int i;

	for ( i = 0; i < vertCount; i++ )
	{
		IVP_U_Point *tmp = new IVP_U_Point;
		
		ConvertPositionToIVP( *pVerts[i], *tmp );

		BEGIN_IVP_ALLOCATION();
		points.add( tmp );
		END_IVP_ALLOCATION();
	}

	BEGIN_IVP_ALLOCATION();
	IVP_Compact_Ledge *pLedge = IVP_SurfaceBuilder_Pointsoup::convert_pointsoup_to_compact_ledge( &points );
	END_IVP_ALLOCATION();

	for ( i = 0; i < points.len(); i++ )
	{
		delete points.element_at(i);
	}
	points.clear();

	return reinterpret_cast<CPhysConvex *>(pLedge);
}

CPhysConvex *CPhysicsCollision::RebuildConvexFromPlanes( CPhysConvex *pConvex, float mergeTolerance )
{
	DebugPrint();
	if ( !pConvex )
		return NULL;
	
	IVP_Compact_Ledge *pLedge = (IVP_Compact_Ledge *)pConvex;
	int triangleCount = pLedge->get_n_triangles();
	
	IVP_Compact_Triangle *pTri = pLedge->get_first_triangle();
	IVP_U_Hesse plane;
    IVP_Halfspacesoup halfspaces;
	
	for ( int j = 0; j < triangleCount; j++ )
	{
		const IVP_Compact_Edge *pEdge = pTri->get_edge( 0 );
		const IVP_U_Float_Point *p0 = IVP_Compact_Ledge_Solver::give_object_coords(pEdge, pLedge);
		const IVP_U_Float_Point *p2 = IVP_Compact_Ledge_Solver::give_object_coords(pEdge->get_next(), pLedge);
		const IVP_U_Float_Point *p1 = IVP_Compact_Ledge_Solver::give_object_coords(pEdge->get_prev(), pLedge);
		plane.calc_hesse(p0, p2, p1);
		IVP_DOUBLE testLen = plane.real_length();
		// if the triangle is less than 1mm on each side then skip it
		if ( testLen > 1e-6 )
		{
			plane.normize();
			halfspaces.add_halfspace( &plane );
		}
		
		pTri = pTri->get_next_tri();
	}
	
    IVP_Compact_Ledge *pLedgeOut = IVP_SurfaceBuilder_Halfspacesoup::convert_halfspacesoup_to_compact_ledge( &halfspaces, mergeTolerance );
	return reinterpret_cast<CPhysConvex *>( pLedgeOut );
}

CPhysConvex	*CPhysicsCollision::ConvexFromVerts( Vector **pVerts, int vertCount )
{
	DebugPrint();
	CPhysConvex	*pConvex = ConvexFromVertsFast( pVerts, vertCount );
	CPhysConvex	*pReturn = RebuildConvexFromPlanes( pConvex, 0.01f );	// remove interior coplanar verts!
	if ( pReturn )
	{
		ConvexFree( pConvex );
		return pReturn;
	}
	return pConvex;
}

// produce a convex element from planes (csg of planes)
CPhysConvex	*CPhysicsCollision::ConvexFromPlanes( float *pPlanes, int planeCount, float mergeDistance )
{
	DebugPrint();
	// NOTE: We're passing in planes with outward-facing normals
	// Ipion expects inward facing ones; we'll need to reverse plane directon
	struct listplane_t
	{
		float normal[3];
		float dist;
	};

	listplane_t *pList = (listplane_t *)pPlanes;
	IVP_U_Hesse plane;
    IVP_Halfspacesoup halfspaces;

	mergeDistance = ConvertDistanceToIVP( mergeDistance );

	for ( int i = 0; i < planeCount; i++ )
	{
		Vector tmp( -pList[i].normal[0], -pList[i].normal[1], -pList[i].normal[2] ); 
		ConvertPlaneToIVP( tmp, -pList[i].dist, plane );
		halfspaces.add_halfspace( &plane );
	}
	
    IVP_Compact_Ledge *pLedge = IVP_SurfaceBuilder_Halfspacesoup::convert_halfspacesoup_to_compact_ledge( &halfspaces, mergeDistance );
	return reinterpret_cast<CPhysConvex *>( pLedge );
}



CPhysConvex *CPhysicsCollision::ConvexFromConvexPolyhedron( const CPolyhedron &ConvexPolyhedron )
{
	DebugPrint();
	IVP_Template_Polygon polyTemplate(ConvexPolyhedron.iVertexCount, ConvexPolyhedron.iLineCount, ConvexPolyhedron.iPolygonCount );

	//convert/copy coordinates
	for( int i = 0; i != ConvexPolyhedron.iVertexCount; ++i )
		ConvertPositionToIVP( ConvexPolyhedron.pVertices[i], polyTemplate.points[i] );

	//copy lines
	for( int i = 0; i != ConvexPolyhedron.iLineCount; ++i )
		polyTemplate.lines[i].set( ConvexPolyhedron.pLines[i].iPointIndices[0], ConvexPolyhedron.pLines[i].iPointIndices[1] );

	//copy polygons
	for( int i = 0; i != ConvexPolyhedron.iPolygonCount; ++i )
	{
		polyTemplate.surfaces[i].init_surface( ConvexPolyhedron.pPolygons[i].iIndexCount ); //num vertices in a convex polygon == num lines
		polyTemplate.surfaces[i].templ_poly = &polyTemplate;

		ConvertPositionToIVP( ConvexPolyhedron.pPolygons[i].polyNormal, polyTemplate.surfaces[i].normal );

		Polyhedron_IndexedLineReference_t *pLineReferences = &ConvexPolyhedron.pIndices[ConvexPolyhedron.pPolygons[i].iFirstIndex];
		for( int j = 0; j != ConvexPolyhedron.pPolygons[i].iIndexCount; ++j )
		{
			polyTemplate.surfaces[i].lines[j] = pLineReferences[j].iLineIndex;
			polyTemplate.surfaces[i].revert_line[j] = pLineReferences[j].iEndPointIndex;
		}
	}

	//final conversion
	IVP_Compact_Ledge *pLedge = IVP_SurfaceBuilder_Polygon_Convex::convert_template_to_ledge(&polyTemplate);

	//cleanup
	for( int i = 0; i != ConvexPolyhedron.iPolygonCount; ++i )
		polyTemplate.surfaces[i].close_surface();

	return reinterpret_cast<CPhysConvex *>(pLedge);
}





struct PolyhedronMesh_Triangle
{
	struct
	{
		int iPointIndices[2];
	} Edges[3];
};



//TODO: Optimize the returned polyhedron to get away from the triangulated mesh
CPolyhedron *CPhysicsCollision::PolyhedronFromConvex( CPhysConvex * const pConvex, bool bUseTempPolyhedron )
{
	DebugPrint();
	IVP_Compact_Ledge *pLedge = (IVP_Compact_Ledge *)pConvex;
	int iTriangles = pLedge->get_n_triangles();

	PolyhedronMesh_Triangle *pTriangles = stackallocT( PolyhedronMesh_Triangle, iTriangles );
	
	int iHighestPointIndex = 0;
	const IVP_Compact_Triangle *pTri = pLedge->get_first_triangle();
	for( int i = 0; i < iTriangles; ++i )
	{
		//reverse point ordering while creating edges
		pTriangles[i].Edges[2].iPointIndices[1] = pTriangles[i].Edges[0].iPointIndices[0] = pTri->get_edge( 2 )->get_start_point_index();
		pTriangles[i].Edges[0].iPointIndices[1] = pTriangles[i].Edges[1].iPointIndices[0] = pTri->get_edge( 1 )->get_start_point_index();
		pTriangles[i].Edges[1].iPointIndices[1] = pTriangles[i].Edges[2].iPointIndices[0] = pTri->get_edge( 0 )->get_start_point_index();

		for( int j = 0; j < 3; ++j )
		{
			//get_n_points() has a whole bunch of ifdefs that apparently disable it in this case, detect number of points
			if( pTriangles[i].Edges[j].iPointIndices[0] > iHighestPointIndex )
				iHighestPointIndex = pTriangles[i].Edges[j].iPointIndices[0];
		}
		
		pTri = pTri->get_next_tri();
	}

	++iHighestPointIndex;

	//apparently points might be shared between ledges and not all points will be used. So now we get to compress them into a smaller set
	int *pPointRemapping = stackallocT( int, iHighestPointIndex );
	memset( pPointRemapping, 0, iHighestPointIndex * sizeof( int ) );
	for( int i = 0; i != iTriangles; ++i )
	{
		for( int j = 0; j != 3; ++j )
			++(pPointRemapping[pTriangles[i].Edges[j].iPointIndices[0]]);
	}

	int iInsertIndex = 0;

	for( int i = 0; i != iHighestPointIndex; ++i )
	{
		if( pPointRemapping[i] )
		{
			pPointRemapping[i] = iInsertIndex;
			++iInsertIndex;
		}
		else
		{
			pPointRemapping[i] = -1;
		}
	}

	const int iNumPoints = iInsertIndex;

	for( int i = 0; i != iTriangles; ++i )
	{
		for( int j = 0; j != 3; ++j )
		{
			for( int k = 0; k != 2; ++k )
				pTriangles[i].Edges[j].iPointIndices[k] = pPointRemapping[pTriangles[i].Edges[j].iPointIndices[k]];
		}
	}
	

	bool *bLinks = stackallocT( bool, iNumPoints * iNumPoints );
	memset( bLinks, 0, iNumPoints * iNumPoints * sizeof( bool ) );

	int iLinkCount = 0;
	for( int i = 0; i != iTriangles; ++i )
	{
		for( int j = 0; j != 3; ++j )
		{
			const int *pIndices = pTriangles[i].Edges[j].iPointIndices;
			int iLow = ((pIndices[0] > pIndices[1])?1:(0));
			++iLinkCount; //this will technically make the link count double the actual number
			bLinks[(pIndices[iLow] * iNumPoints) + pIndices[1-iLow]] = true;
		}
	}

	iLinkCount /= 2; //cut the link count in half since we overcounted

	CPolyhedron *pReturn;
	if( bUseTempPolyhedron )
		pReturn = GetTempPolyhedron( iNumPoints, iLinkCount, iLinkCount * 2, iTriangles );
	else
		pReturn = CPolyhedron_AllocByNew::Allocate( iNumPoints, iLinkCount, iLinkCount * 2, iTriangles );

	//copy/convert vertices
	const IVP_Compact_Poly_Point *pLedgePoints = pLedge->get_point_array();
	Vector *pWriteVertices = pReturn->pVertices;
	for( int i = 0; i != iHighestPointIndex; ++i )
	{
		if( pPointRemapping[i] != -1 )
			ConvertPositionToHL( pLedgePoints[i], pWriteVertices[pPointRemapping[i]] );
	}

    
	//convert lines
	iInsertIndex = 0;
	for( int i = 0; i != iNumPoints; ++i )
	{
		for( int j = i + 1; j != iNumPoints; ++j )
		{
			if( bLinks[(i * iNumPoints) + j] )
			{
				pReturn->pLines[iInsertIndex].iPointIndices[0] = i;
				pReturn->pLines[iInsertIndex].iPointIndices[1] = j;
				++iInsertIndex;
			}
		}
	}

	
	int *pStartIndices = (int *)stackalloc( iNumPoints * sizeof( int ) ); //for quicker lookup of which edges to use in polygons

	pStartIndices[0] = 0; //the lowest index point drives links, so if the first point isn't the first link, then something is extremely messed up
	Assert( pReturn->pLines[0].iPointIndices[0] == 0 );
	iInsertIndex = 1;
	for( int i = 1; i != iNumPoints; ++i )
	{
		for( int j = iInsertIndex; j != iLinkCount; ++j )
		{
			if( pReturn->pLines[j].iPointIndices[0] == i )
			{
				pStartIndices[i] = j;
				iInsertIndex = j + 1;
				break;
			}
		}
	}

	//convert polygons and setup line references as a subtask
	iInsertIndex = 0;
	for( int i = 0; i != iTriangles; ++i )
	{
		pReturn->pPolygons[i].iFirstIndex = iInsertIndex;
		pReturn->pPolygons[i].iIndexCount = 3;

		Vector *p1, *p2, *p3;
		p1 = &pReturn->pVertices[pTriangles[i].Edges[0].iPointIndices[0]];
		p2 = &pReturn->pVertices[pTriangles[i].Edges[1].iPointIndices[0]];
		p3 = &pReturn->pVertices[pTriangles[i].Edges[2].iPointIndices[0]];

		Vector v1to2, v1to3;

		v1to2 = *p2 - *p1;
		v1to3 = *p3 - *p1;

		pReturn->pPolygons[i].polyNormal = v1to3.Cross( v1to2 );
		pReturn->pPolygons[i].polyNormal.NormalizeInPlace();

		for( int j = 0; j != 3; ++j, ++iInsertIndex )
		{
			const int *pIndices = pTriangles[i].Edges[j].iPointIndices;
			int iLow = (pIndices[0] > pIndices[1])?1:0;
			int iLineIndex;
			for( iLineIndex = pStartIndices[pIndices[iLow]]; iLineIndex != iLinkCount; ++iLineIndex )
			{
				if( (pReturn->pLines[iLineIndex].iPointIndices[0] == pIndices[iLow]) &&
					(pReturn->pLines[iLineIndex].iPointIndices[1] == pIndices[1 - iLow]) )
				{
					break;
				}
			}

			pReturn->pIndices[iInsertIndex].iLineIndex = iLineIndex;
			pReturn->pIndices[iInsertIndex].iEndPointIndex = 1 - iLow;
		}
	}

	return pReturn;
}


int CPhysicsCollision::GetConvexesUsedInCollideable( const CPhysCollide *pCollideable, CPhysConvex **pOutputArray, int iOutputArrayLimit )
{
	DebugPrint();
	IVP_U_BigVector<IVP_Compact_Ledge> ledges;
	pCollideable->GetAllLedges( ledges );

	int iLedgeCount = ledges.len();
	if( iLedgeCount > iOutputArrayLimit )
		iLedgeCount = iOutputArrayLimit;

	for( int i = 0; i != iLedgeCount; ++i )
	{
		IVP_Compact_Ledge *pLedge = ledges.element_at(i); //doing as a 2 step since a single convert seems more error prone (without compile error) in this case
		pOutputArray[i] = (CPhysConvex *)pLedge;
	}

	return iLedgeCount;
}

void CPhysicsCollision::ConvexesFromConvexPolygon( const Vector &vPolyNormal, const Vector *pPoints, int iPointCount, CPhysConvex **pOutput )
{
	DebugPrint();
	IVP_U_Point *pIVP_Points = stackallocT( IVP_U_Point, iPointCount );
	IVP_U_Point **pTriangulator = stackallocT( IVP_U_Point*, iPointCount );
	IVP_U_Point **pRead = pTriangulator;
	IVP_U_Point **pWrite = pTriangulator;

	//convert coordinates
	{
		for( int i = 0; i < iPointCount; ++i )
			ConvertPositionToIVP( pPoints[i], pIVP_Points[i] );
	}

	int iOutputCount = 0;

	//chunk this out like a triangle strip
	int iForwardCounter = 1;
	int iReverseCounter = iPointCount - 1; //guaranteed to be >= 2 to start

	*pWrite = &pIVP_Points[0];
	++pWrite;
	*pWrite = &pIVP_Points[iReverseCounter];
	++pWrite;
	--iReverseCounter;

	do
	{
		//forward
		*pWrite = &pIVP_Points[iForwardCounter];
		++iForwardCounter;

		pOutput[iOutputCount] = reinterpret_cast<CPhysConvex *>(IVP_SurfaceBuilder_Pointsoup::convert_triangle_to_compace_ledge( pRead[0], pRead[1], pRead[2] ));
		Assert( pOutput[iOutputCount] );
		++iOutputCount;
		if( iForwardCounter > iReverseCounter )
			break;

		++pRead;
		++pWrite;



		//backward
		*pWrite = &pIVP_Points[iReverseCounter];
		--iReverseCounter;

		pOutput[iOutputCount] = reinterpret_cast<CPhysConvex *>(IVP_SurfaceBuilder_Pointsoup::convert_triangle_to_compace_ledge( pRead[0], pRead[1], pRead[2] ));
		Assert( pOutput[iOutputCount] );
		++iOutputCount;

		if( iForwardCounter > iReverseCounter )
			break;

		++pRead;
		++pWrite;
	} while( true );
}


//-----------------------------------------------------------------------------
// Purpose: copies the first vert int pLedge to out
// Input  : *pLedge - compact ledge
//			*out - destination float array for the vert
//-----------------------------------------------------------------------------
static void LedgeInsidePoint( IVP_Compact_Ledge *pLedge, Vector& out )
{
	IVP_Compact_Triangle *pTri = pLedge->get_first_triangle();
	const IVP_Compact_Edge *pEdge = pTri->get_edge( 0 );
	const IVP_U_Float_Point *pPoint = pEdge->get_start_point( pLedge );
	ConvertPositionToHL( *pPoint, out );
}


//-----------------------------------------------------------------------------
// Purpose: Calculate the volume of a tetrahedron with these vertices
// Input  : p0 - points of tetrahedron
//			p1 - 
//			p2 - 
//			p3 - 
// Output : float (volume in units^3)
//-----------------------------------------------------------------------------
#if !PLATFORM_64BITS // Mathlib has them on 64x.
static float TetrahedronVolume( const Vector &p0, const Vector &p1, const Vector &p2, const Vector &p3 )
{
	Vector a, b, c, cross;
	float volume = 1.0f / 6.0f;

	a = p1 - p0;
	b = p2 - p0;
	c = p3 - p0;
	cross = CrossProduct( b, c );

	volume *= DotProduct( a, cross );
	if ( volume < 0 )
		return -volume;
	return volume;
}

static float TriangleArea( const Vector &p0, const Vector &p1, const Vector &p2 )
{
	Vector e0 = p1 - p0;
	Vector e1 = p2 - p0;
	Vector cross;

	CrossProduct( e0, e1, cross );
	return 0.5f * cross.Length();
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Tetrahedronalize this ledge and compute it's volume in BSP space
// Input  : convex - the ledge
// Output : float - volume in HL units (in^3)
//-----------------------------------------------------------------------------
float CPhysicsCollision::ConvexVolume( CPhysConvex *pConvex )
{
	DebugPrint();
	IVP_Compact_Ledge *pLedge = (IVP_Compact_Ledge *)pConvex;
	int triangleCount = pLedge->get_n_triangles();

	IVP_Compact_Triangle *pTri = pLedge->get_first_triangle();

	Vector vert;
	float volume = 0;
	// vert is in HL units
	LedgeInsidePoint( pLedge, vert );

	for ( int j = 0; j < triangleCount; j++ )
	{
		Vector points[3];
		for ( int k = 0; k < 3; k++ )
		{
			const IVP_Compact_Edge *pEdge = pTri->get_edge( k );
			const IVP_U_Float_Point *pPoint = pEdge->get_start_point( pLedge );
			ConvertPositionToHL( *pPoint, points[k] );
		}
		volume += TetrahedronVolume( vert, points[0], points[1], points[2] );

		pTri = pTri->get_next_tri();
	}

	return volume;
}


float CPhysicsCollision::ConvexSurfaceArea( CPhysConvex *pConvex )
{
	DebugPrint();
	IVP_Compact_Ledge *pLedge = (IVP_Compact_Ledge *)pConvex;
	int triangleCount = pLedge->get_n_triangles();

	IVP_Compact_Triangle *pTri = pLedge->get_first_triangle();

	float area = 0;

	for ( int j = 0; j < triangleCount; j++ )
	{
		Vector points[3];
		for ( int k = 0; k < 3; k++ )
		{
			const IVP_Compact_Edge *pEdge = pTri->get_edge( k );
			const IVP_U_Float_Point *pPoint = pEdge->get_start_point( pLedge );
			ConvertPositionToHL( *pPoint, points[k] );
		}
		area += TriangleArea( points[0], points[1], points[2] );

		pTri = pTri->get_next_tri();
	}

	return area;
}

// Convert an array of convex elements to a compiled collision model (this deletes the convex elements)
CPhysCollide *CPhysicsCollision::ConvertConvexToCollide( CPhysConvex **pConvex, int convexCount )
{
	DebugPrint();
	convertconvexparams_t convertParams;
	convertParams.Defaults();
	return ConvertConvexToCollideParams( pConvex, convexCount, convertParams );
}

CPhysCollide *CPhysicsCollision::ConvertConvexToCollideParams( CPhysConvex **pConvex, int convexCount, const convertconvexparams_t &convertParams )
{
	DebugPrint();
	if ( !convexCount || !pConvex )
		return NULL;

	int validConvex = 0;
	BEGIN_IVP_ALLOCATION();
	IVP_SurfaceBuilder_Ledge_Soup builder;
	IVP_Compact_Surface *pSurface = NULL;

	for ( int i = 0; i < convexCount; i++ )
	{
		if ( pConvex[i] )
		{
			validConvex++;
			builder.insert_ledge( (IVP_Compact_Ledge *)pConvex[i] );
		}
	}
	// if the outside code does something stupid, don't crash
	if ( validConvex )
	{
		IVP_Template_Surbuild_LedgeSoup params;
		params.force_convex_hull = (IVP_Compact_Ledge *)convertParams.pForcedOuterHull;
		params.build_root_convex_hull = convertParams.buildOuterConvexHull ? IVP_TRUE : IVP_FALSE;

		// NOTE: THIS FREES THE LEDGES in pConvex!!!
		pSurface = builder.compile( &params );
		CPhysCollide *pCollide = new CPhysCollideCompactSurface( pSurface );
		if ( convertParams.buildDragAxisAreas )
		{
			pCollide->ComputeOrthographicAreas( convertParams.dragAreaEpsilon );
		}

		END_IVP_ALLOCATION();
		return pCollide;
	}

	END_IVP_ALLOCATION();

	return NULL;
}

static void InitBoxVerts( Vector *boxVerts, Vector **ppVerts, const Vector &mins, const Vector &maxs )
{
	for (int i = 0; i < 8; ++i)
	{
		boxVerts[i][0] = (i & 0x1) ? maxs[0] : mins[0];
		boxVerts[i][1] = (i & 0x2) ? maxs[1] : mins[1];
		boxVerts[i][2] = (i & 0x4) ? maxs[2] : mins[2];
		if ( ppVerts )
		{
			ppVerts[i] = &boxVerts[i];
		}
	}
}


#define FAST_BBOX 1
CPhysCollideCompactSurface *CPhysicsCollision::FastBboxCollide( const CPhysCollideCompactSurface *pCollide, const Vector &mins, const Vector &maxs )
{
	DebugPrint();
	Vector boxVerts[8];
	InitBoxVerts( boxVerts, NULL, mins, maxs );
	// copy the compact ledge at bboxCache 0
	// stuff the verts in there
	const IVP_Compact_Surface *pSurface = ConvertPhysCollideToCompactSurface( pCollide );
	Assert( pSurface );
	const IVP_Compact_Ledgetree_Node *node = pSurface->get_compact_ledge_tree_root();
	Assert( node->is_terminal() == IVP_TRUE );
	const IVP_Compact_Ledge *pLedge = node->get_compact_ledge();
	int ledgeSize = pLedge->get_size();
	IVP_Compact_Ledge *pNewLedge = (IVP_Compact_Ledge *)ivp_malloc_aligned( ledgeSize, 16 );
	memcpy( pNewLedge, pLedge, ledgeSize );
	pNewLedge->set_client_data(0);
	IVP_Compact_Poly_Point *pPoints = pNewLedge->get_point_array();
	for ( int i = 0; i < 8; i++ )
	{
		IVP_U_Float_Hesse ivp;
		ConvertPositionToIVP( boxVerts[m_bboxVertMap[i]], ivp );
		ivp.hesse_val = 0;
		pPoints[i].set4(&ivp);
	}
	CPhysConvex *pConvex = (CPhysConvex *)pNewLedge;
	return (CPhysCollideCompactSurface *)ConvertConvexToCollide( &pConvex, 1 );
}

void CPhysicsCollision::InitBBoxCache()
{
	DebugPrint();
	Vector boxVerts[8], *ppVerts[8];
	Vector mins(-16,-16,0), maxs(16,16,72);
	// init with the player box
	InitBoxVerts( boxVerts, ppVerts, mins, maxs );
	// Generate a convex hull from the verts
	CPhysConvex *pConvex = ConvexFromVertsFast( ppVerts, 8 );
	IVP_Compact_Poly_Point *pPoints = reinterpret_cast<IVP_Compact_Ledge *>(pConvex)->get_point_array();
	for ( int i = 0; i < 8; i++ )
	{
		int nearest = -1;
		float minDist = 0.1f;
		Vector tmp;
		ConvertPositionToHL( pPoints[i], tmp );
		for ( int j = 0; j < 8; j++ )
		{
			float dist = (boxVerts[j] - tmp).Length();
			if ( dist < minDist )
			{
				minDist = dist;
				nearest = j;
			}
		}
		
		m_bboxVertMap[i] = nearest;

#if _DEBUG
		for ( int k = 0; k < i; k++ )
		{
			Assert( m_bboxVertMap[k] != m_bboxVertMap[i] );
		}
#endif
		// NOTE: If this is wrong, you can disable FAST_BBOX above to fix
		//AssertMsg( nearest != -1, "CPhysCollide: Vert map is wrong\n" );
	}
	CPhysCollide *pCollide = ConvertConvexToCollide( &pConvex, 1 );
	AddBBoxCache( (CPhysCollideCompactSurface *)pCollide, mins, maxs );
}


CPhysConvex	*CPhysicsCollision::BBoxToConvex( const Vector &mins, const Vector &maxs )
{
	DebugPrint();
	Vector boxVerts[8], *ppVerts[8];
	InitBoxVerts( boxVerts, ppVerts, mins, maxs );
	// Generate a convex hull from the verts
	return ConvexFromVertsFast( ppVerts, 8 );
}

CPhysCollide *CPhysicsCollision::BBoxToCollide( const Vector &mins, const Vector &maxs )
{
	DebugPrint();
	// can't create a collision model for an empty box !
	if ( mins == maxs )
	{
		Assert(0);
		return NULL;
	}

	// find this bbox in the cache
	CPhysCollide *pCollide = GetBBoxCache( mins, maxs );
	if ( pCollide )
		return pCollide;

	// FAST_BBOX: uses an existing compact ledge as a template for fast generation
	// building convex hulls from points is slow
#if FAST_BBOX
	if ( m_bboxCache.Count() == 0 )
	{
		InitBBoxCache();
	}
	pCollide = FastBboxCollide( m_bboxCache[0].pCollide, mins, maxs );
#else
	CPhysConvex *pConvex = BBoxToConvex( mins, maxs );
	pCollide = ConvertConvexToCollide( &pConvex, 1 );
#endif
	AddBBoxCache( (CPhysCollideCompactSurface *)pCollide, mins, maxs );
	return pCollide;
}

bool CPhysicsCollision::IsBBoxCache( CPhysCollide *pCollide )
{
	DebugPrint();
	// UNDONE: Sort the list so it can be searched spatially instead of linearly?
	for ( int i = m_bboxCache.Count()-1; i >= 0; i-- )
	{
		if ( m_bboxCache[i].pCollide == pCollide )
			return true;
	}
	return false;
}

void CPhysicsCollision::AddBBoxCache( CPhysCollideCompactSurface *pCollide, const Vector &mins, const Vector &maxs )
{
	DebugPrint();
	bboxcache_t &pCache = m_bboxCache[m_bboxCache.AddToTail()];
	pCache.pCollide = pCollide;
	pCache.mins = mins;
	pCache.maxs = maxs;
}

CPhysCollideCompactSurface *CPhysicsCollision::GetBBoxCache( const Vector &mins, const Vector &maxs )
{
	DebugPrint();
	for ( int i = m_bboxCache.Count()-1; i >= 0; i-- )
	{
		if ( m_bboxCache[i].mins == mins && m_bboxCache[i].maxs == maxs )
			return m_bboxCache[i].pCollide;
	}
	return NULL;
}


void CPhysicsCollision::ConvexFree( CPhysConvex *pConvex )
{
	DebugPrint();
	if ( !pConvex )
		return;
	ivp_free_aligned( pConvex );
}

// Get the size of the collision model for serialization
int	CPhysicsCollision::CollideSize( CPhysCollide *pCollide )
{
	DebugPrint();
	return pCollide->GetSerializationSize();
}

int	CPhysicsCollision::CollideWrite( char *pDest, CPhysCollide *pCollide, bool bSwap )
{
	DebugPrint();
	return pCollide->SerializeToBuffer( pDest, bSwap );
}

CPhysCollide *CPhysicsCollision::UnserializeCollide( char *pBuffer, int size, int index )
{
	DebugPrint();
	return CPhysCollide::UnserializeFromBuffer( pBuffer, size, index );
}

class CPhysPolysoup
{
public:
	CPhysPolysoup();
#if ENABLE_IVP_MOPP
	IVP_SurfaceBuilder_Mopp m_builder;
#endif
	IVP_SurfaceBuilder_Ledge_Soup m_builderSoup;
	IVP_U_Vector<IVP_U_Point> m_points;
	IVP_U_Point m_triangle[3];

	bool m_isValid;
};

CPhysPolysoup::CPhysPolysoup()
{
	DebugPrint();
	m_isValid = false;
    m_points.add( &m_triangle[0] );
    m_points.add( &m_triangle[1] );
    m_points.add( &m_triangle[2] );
}

CPhysPolysoup *CPhysicsCollision::PolysoupCreate( void )
{
	DebugPrint();
	return new CPhysPolysoup;
}

void CPhysicsCollision::PolysoupDestroy( CPhysPolysoup *pSoup )
{
	DebugPrint();
	delete pSoup;
}

void CPhysicsCollision::PolysoupAddTriangle( CPhysPolysoup *pSoup, const Vector &a, const Vector &b, const Vector &c, int materialIndex7bits )
{
	DebugPrint();
	pSoup->m_isValid = true;
	ConvertPositionToIVP( a, pSoup->m_triangle[0] );
	ConvertPositionToIVP( b, pSoup->m_triangle[1] );
	ConvertPositionToIVP( c, pSoup->m_triangle[2] );
	IVP_Compact_Ledge *pLedge = IVP_SurfaceBuilder_Pointsoup::convert_pointsoup_to_compact_ledge(&pSoup->m_points);
	if ( !pLedge )
	{
		Warning("Degenerate Triangle\n");
		Warning("(%.2f, %.2f, %.2f), ", a.x, a.y, a.z );
		Warning("(%.2f, %.2f, %.2f), ", b.x, b.y, b.z );
		Warning("(%.2f, %.2f, %.2f)\n", c.x, c.y, c.z );
		return;
	}
	IVP_Compact_Triangle *pTriangle = pLedge->get_first_triangle();
	pTriangle->set_material_index( materialIndex7bits );
#if ENABLE_IVP_MOPP
	pSoup->m_builder.insert_ledge(pLedge);
#endif
	pSoup->m_builderSoup.insert_ledge(pLedge);
}

CPhysCollide *CPhysicsCollision::ConvertPolysoupToCollide( CPhysPolysoup *pSoup, [[maybe_unused]] bool useMOPP )
{
	DebugPrint();
	if ( !pSoup->m_isValid )
		return NULL;

	CPhysCollide *pCollide = NULL;
#if ENABLE_IVP_MOPP
	if ( useMOPP )
	{
	    IVP_Compact_Mopp *pSurface = pSoup->m_builder.compile();
		pCollide = new CPhysCollideMopp( pSurface );
	}
	else
#endif
	{
	    IVP_Compact_Surface *pSurface = pSoup->m_builderSoup.compile();
		pCollide = new CPhysCollideCompactSurface( pSurface );
	}

	Assert(pCollide);

	// There's a bug in IVP where the duplicated triangles (for 2D)
	// don't get the materials set properly, so copy them
	IVP_U_BigVector<IVP_Compact_Ledge> ledges;
	pCollide->GetAllLedges( ledges );

	for ( int i = 0; i < ledges.len(); i++ )
	{
		IVP_Compact_Ledge *pLedge = ledges.element_at( i );
		int triangleCount = pLedge->get_n_triangles();

		IVP_Compact_Triangle *pTri = pLedge->get_first_triangle();
		int materialIndex = pTri->get_material_index();
		if ( !materialIndex )
		{
			for ( int j = 0; j < triangleCount; j++ )
			{
				if ( pTri->get_material_index() != 0 )
				{
					materialIndex = pTri->get_material_index();
				}
				pTri = pTri->get_next_tri();
			}
		}
		for ( int j = 0; j < triangleCount; j++ )
		{
			pTri->set_material_index( materialIndex );
			pTri = pTri->get_next_tri();
		}
	}

	return pCollide;
}

int CPhysicsCollision::CreateDebugMesh( const CPhysCollide *pCollisionModel, Vector **outVerts )
{
	DebugPrint();
	int i;

	IVP_U_BigVector<IVP_Compact_Ledge> ledges;
	pCollisionModel->GetAllLedges( ledges );

	int vertCount = 0;
	
	for ( i = 0; i < ledges.len(); i++ )
	{
		IVP_Compact_Ledge *pLedge = ledges.element_at( i );
		vertCount += pLedge->get_n_triangles() * 3;
	}
	Vector *verts = new Vector[ vertCount ];
		
	int vertIndex = 0;
	for ( i = 0; i < ledges.len(); i++ )
	{
		IVP_Compact_Ledge *pLedge = ledges.element_at( i );
		int triangleCount = pLedge->get_n_triangles();

		IVP_Compact_Triangle *pTri = pLedge->get_first_triangle();
		for ( int j = 0; j < triangleCount; j++ )
		{
			for ( int k = 2; k >= 0; k-- )
			{
				const IVP_Compact_Edge *pEdge = pTri->get_edge( k );
				const IVP_U_Float_Point *pPoint = pEdge->get_start_point( pLedge );

				Vector* pVec = verts + vertIndex;
				ConvertPositionToHL( *pPoint, *pVec );
				vertIndex++;
			}
			pTri = pTri->get_next_tri();
		}
	}

	*outVerts = verts;
	return vertCount;
}


void CPhysicsCollision::DestroyDebugMesh( int, Vector *outVerts )
{
	DebugPrint();
	delete[] outVerts;
}


void CPhysicsCollision::SetConvexGameData( CPhysConvex *pConvex, unsigned int gameData )
{
	DebugPrint();
	IVP_Compact_Ledge *pLedge = reinterpret_cast<IVP_Compact_Ledge *>( pConvex );
	pLedge->set_client_data( gameData );
}


void CPhysicsCollision::TraceBox( const Vector &start, const Vector &end, const Vector &mins, const Vector &maxs, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr )
{
	DebugPrint();
	m_traceapi.SweepBoxIVP( start, end, mins, maxs, pCollide, collideOrigin, collideAngles, ptr );
}

void CPhysicsCollision::TraceBox( const Ray_t &ray, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr )
{
	DebugPrint();
	TraceBox( ray, MASK_ALL, NULL, pCollide, collideOrigin, collideAngles, ptr );
}

void CPhysicsCollision::TraceBox( const Ray_t &ray, unsigned int contentsMask, IConvexInfo *pConvexInfo, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr )
{
	DebugPrint();
	m_traceapi.SweepBoxIVP( ray, contentsMask, pConvexInfo, pCollide, collideOrigin, collideAngles, ptr );
}

// Trace one collide against another
void CPhysicsCollision::TraceCollide( const Vector &start, const Vector &end, const CPhysCollide *pSweepCollide, const QAngle &sweepAngles, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr )
{
	DebugPrint();
	m_traceapi.SweepIVP( start, end, pSweepCollide, sweepAngles, pCollide, collideOrigin, collideAngles, ptr );
}

void CPhysicsCollision::CollideGetAABB( Vector *pMins, Vector *pMaxs, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles )
{
	DebugPrint();
	m_traceapi.GetAABB( pMins, pMaxs, pCollide, collideOrigin, collideAngles );
}


Vector CPhysicsCollision::CollideGetExtent( const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, const Vector &direction )
{
	DebugPrint();
	if ( !pCollide )
		return collideOrigin;

	return m_traceapi.GetExtent( pCollide, collideOrigin, collideAngles, direction );
}

bool CPhysicsCollision::IsBoxIntersectingCone( const Vector &boxAbsMins, const Vector &boxAbsMaxs, const truncatedcone_t &cone )
{
	DebugPrint();
	return m_traceapi.IsBoxIntersectingCone( boxAbsMins, boxAbsMaxs, cone );
}

// Free a collide that was created with ConvertConvexToCollide()
void CPhysicsCollision::DestroyCollide( CPhysCollide *pCollide )
{
	DebugPrint();
	if ( !IsBBoxCache( pCollide ) )
	{
		delete pCollide;
	}
}

// calculate the volume of a collide by calling ConvexVolume on its parts
float CPhysicsCollision::CollideVolume( CPhysCollide *pCollide )
{
	DebugPrint();
	IVP_U_BigVector<IVP_Compact_Ledge> ledges;
	pCollide->GetAllLedges( ledges );

	float volume = 0;
	for ( int i = 0; i < ledges.len(); i++ )
	{
		volume += ConvexVolume( (CPhysConvex *)ledges.element_at(i) );
	}

	return volume;
}

// calculate the volume of a collide by calling ConvexVolume on its parts
float CPhysicsCollision::CollideSurfaceArea( CPhysCollide *pCollide )
{
	DebugPrint();
	IVP_U_BigVector<IVP_Compact_Ledge> ledges;
	pCollide->GetAllLedges( ledges );

	float area = 0;
	for ( int i = 0; i < ledges.len(); i++ )
	{
		area += ConvexSurfaceArea( (CPhysConvex *)ledges.element_at(i) );
	}

	return area;
}


// loads a set of solids into a vcollide_t
void CPhysicsCollision::VCollideLoad( vcollide_t *pOutput, int solidCount, const char *pBuffer, int bufferSize, bool swap )
{
	DebugPrint();
	memset( pOutput, 0, sizeof(*pOutput) );
	int position = 0;

	pOutput->solidCount = solidCount;
	pOutput->solids = new CPhysCollide *[solidCount];

	BEGIN_IVP_ALLOCATION();

	int currentSize = 0;
	char *tmpbuf = nullptr;

	for ( int i = 0; i < solidCount; i++ )
	{
		int size;
		memcpy( &size, pBuffer + position, sizeof(int) );
		position += sizeof(int);

		if (size > currentSize || !tmpbuf)
		{
				delete[] tmpbuf;
			tmpbuf = new char[size];
			currentSize = size;
		}

		memcpy(tmpbuf, pBuffer + position, size);

		pOutput->solids[i] = CPhysCollide::UnserializeFromBuffer( tmpbuf, size, i, swap );
		position += size;
	}

	delete[] tmpbuf;

	END_IVP_ALLOCATION();
	pOutput->isPacked = false;
	int keySize = bufferSize - position;
	pOutput->pKeyValues = new char[keySize];
	memcpy( pOutput->pKeyValues, pBuffer + position, keySize );
	pOutput->descSize = 0;
}

// destroys the set of solids created by VCollideCreateCPhysCollide
void CPhysicsCollision::VCollideUnload( vcollide_t *pVCollide )
{
	DebugPrint();
	for ( int i = 0; i < pVCollide->solidCount; i++ )
	{
#if _DEBUG
		// HACKHACK: 1024 is just "some big number"
		// GetActiveEnvironmentByIndex() will eventually return NULL when there are no more environments.
		// In HL2 & TF2, there are only 2 environments - so j > 1 is probably an error!
		for ( int j = 0; j < 1024; j++ )
		{
			IPhysicsEnvironment *pEnv = g_PhysicsInternal->GetActiveEnvironmentByIndex( j );
			if ( !pEnv )
				break;

			if ( pEnv->IsCollisionModelUsed( (const CPhysCollide *)pVCollide->solids[i] ) )
			{
 				//AssertMsg(0, "Freed collision model while in use!!!\n");
				return;
			}
		}
#endif
		delete pVCollide->solids[i];
	}
	delete[] pVCollide->solids;
	delete[] pVCollide->pKeyValues;
	memset( pVCollide, 0, sizeof(*pVCollide) );
}

// begins parsing a vcollide.  NOTE: This keeps pointers to the vcollide_t
// If you delete the vcollide_t and call members of IVCollideParse, it will crash
IVPhysicsKeyParser *CPhysicsCollision::VPhysicsKeyParserCreate( const char *pKeyData )
{
	DebugPrint();
	return CreateVPhysicsKeyParser( pKeyData, false );
}

#if PLATFORM_64BITS
IVPhysicsKeyParser *CPhysicsCollision::VPhysicsKeyParserCreate( vcollide_t *pVCollide )
{
	DebugPrint();
	static IPhysicsCollision *g_OrigPhysicsCollision;
	if (!g_OrigPhysicsCollision)
	{
		SourceSDK::FactoryLoader vphysics_loader("vphysics");
		extern bool g_pForceOriginalIVP;
		g_pForceOriginalIVP = true;
		g_OrigPhysicsCollision = vphysics_loader.GetInterface<IPhysicsCollision>(VPHYSICS_COLLISION_INTERFACE_VERSION);
		g_pForceOriginalIVP = false;
	}

	if (g_OrigPhysicsCollision)
	{
		IVPhysicsKeyParser* parser = g_OrigPhysicsCollision->VPhysicsKeyParserCreate( pVCollide );
		DEBUG_IF
		{
			IVPhysicsKeyParser* our_parser = CreateVPhysicsKeyParser( pVCollide->pKeyValues, pVCollide->isPacked );
			Msg("IVP returned: %p\n", parser);
			Msg("We returned: %p\n", our_parser);
		}

		return parser;
	}

	return CreateVPhysicsKeyParser( pVCollide->pKeyValues, pVCollide->isPacked );
}
#endif

// Free the parser created by VPhysicsKeyParserCreate
void CPhysicsCollision::VPhysicsKeyParserDestroy( IVPhysicsKeyParser *pParser )
{
	DebugPrint();
	DestroyVPhysicsKeyParser( pParser );
}

IPhysicsCollision *CPhysicsCollision::ThreadContextCreate( void )
{ 
	DebugPrint();
	return this;
}

void CPhysicsCollision::ThreadContextDestroy( IPhysicsCollision * )
{
	DebugPrint();
}


void CPhysicsCollision::CollideGetMassCenter( CPhysCollide *pCollide, Vector *pOutMassCenter )
{
	DebugPrint();
	*pOutMassCenter = pCollide->GetMassCenter();
}

void CPhysicsCollision::CollideSetMassCenter( CPhysCollide *pCollide, const Vector &massCenter )
{
	DebugPrint();
	pCollide->SetMassCenter( massCenter );
}

int CPhysicsCollision::CollideIndex( const CPhysCollide *pCollide )
{
	DebugPrint();
	if ( !pCollide )
		return 0;
	return pCollide->GetVCollideIndex();
}

Vector CPhysicsCollision::CollideGetOrthographicAreas( const CPhysCollide *pCollide )
{
	DebugPrint();
	if ( !pCollide )
		return vec3_origin;

	return pCollide->GetOrthographicAreas();
}

void CPhysicsCollision::CollideSetOrthographicAreas( CPhysCollide *pCollide, const Vector &areas )
{
	DebugPrint();
	if ( pCollide )
		pCollide->SetOrthographicAreas( areas );
}

// returns true if this collide has an outer hull built
void CPhysicsCollision::OutputDebugInfo( const CPhysCollide *pCollide )
{
	DebugPrint();
	pCollide->OutputDebugInfo();
}

bool CPhysicsCollision::GetBBoxCacheSize( int *pCachedSize, int *pCachedCount )
{
	DebugPrint();
	*pCachedSize = 0;
	*pCachedCount = m_bboxCache.Count();
	for ( int i = 0; i < *pCachedCount; i++ )
	{
		*pCachedSize += m_bboxCache[i].pCollide->GetSerializationSize();
	}
	return true;
}

class CCollisionQuery : public ICollisionQuery
{
public:
	CCollisionQuery( CPhysCollide *pCollide );
	~CCollisionQuery( void ) {}

	// number of convex pieces in the whole solid
	int		ConvexCount( void ) override;
	// triangle count for this convex piece
	int		TriangleCount( int convexIndex ) override;
	
	// get the stored game data
	unsigned int GetGameData( int convexIndex ) override;

	// Gets the triangle's verts to an array
	void	GetTriangleVerts( int convexIndex, int triangleIndex, Vector *verts ) override;
	
	// UNDONE: This doesn't work!!!
	void	SetTriangleVerts( int convexIndex, int triangleIndex, const Vector *verts ) override;
	
	// returns the 7-bit material index
	int		GetTriangleMaterialIndex( int convexIndex, int triangleIndex ) override;
	// sets a 7-bit material index for this triangle
	void	SetTriangleMaterialIndex( int convexIndex, int triangleIndex, int index7bits ) override;

private:
	IVP_Compact_Triangle *Triangle( IVP_Compact_Ledge *pLedge, int triangleIndex );

	IVP_U_BigVector <IVP_Compact_Ledge> m_ledges;
};


// create a queryable version of the collision model
ICollisionQuery *CPhysicsCollision::CreateQueryModel( CPhysCollide *pCollide )
{
	DebugPrint();
	return new CCollisionQuery( pCollide );
}

	// destroy the queryable version
void CPhysicsCollision::DestroyQueryModel( ICollisionQuery *pQuery )
{
	DebugPrint();
	delete pQuery;
}

#if PLATFORM_64BITS
float CPhysicsCollision::CollideGetRadius(const CPhysCollide* pCollide)
{
	DebugPrint();
	Error("Not Implemented!\n");
	return 0.0f;
}

void* CPhysicsCollision::VCollideAllocUserData(vcollide_t* pVCollide, size_t userDataSize)
{
	DebugPrint();
	Error("Not Implemented!\n");
	return nullptr;
}

void CPhysicsCollision::VCollideFreeUserData(vcollide_t* pVCollide)
{
	DebugPrint();
	Error("Not Implemented!\n");
}

void CPhysicsCollision::VCollideCheck(vcollide_t* pVCollide, const char* pName)
{
	DebugPrint();
	Error("Not Implemented!\n");
}

bool CPhysicsCollision::TraceBoxAA(const Ray_t& ray, const CPhysCollide* pCollide, trace_t* ptr)
{
	DebugPrint();
	Error("Not Implemented!\n");
	return false;
}

void CPhysicsCollision::DuplicateAndScale(vcollide_t* pOut, const vcollide_t* pIn, float flScale)
{
	DebugPrint();
	Error("Not Implemented!\n");
}
#endif


CCollisionQuery::CCollisionQuery( CPhysCollide *pCollide )
{
	DebugPrint();
	pCollide->GetAllLedges( m_ledges );
}


	// number of convex pieces in the whole solid
int	CCollisionQuery::ConvexCount( void )
{
	DebugPrint();
	return m_ledges.len();
}

	// triangle count for this convex piece
int CCollisionQuery::TriangleCount( int convexIndex )
{
	DebugPrint();
	IVP_Compact_Ledge *pLedge = m_ledges.element_at(convexIndex);
	if ( pLedge )
	{
		return pLedge->get_n_triangles();
	}

	return 0;
}


unsigned int CCollisionQuery::GetGameData( int convexIndex )
{
	DebugPrint();
	IVP_Compact_Ledge *pLedge = m_ledges.element_at( convexIndex );
	if ( pLedge )
		return pLedge->get_client_data();
	return 0;
}

	// Gets the triangle's verts to an array
void CCollisionQuery::GetTriangleVerts( int convexIndex, int triangleIndex, Vector *verts )
{
	DebugPrint();
	IVP_Compact_Ledge *pLedge = m_ledges.element_at( convexIndex );
	IVP_Compact_Triangle *pTriangle = Triangle( pLedge, triangleIndex );

	int vertIndex = 0;
	for ( int k = 2; k >= 0; k-- )
	{
		const IVP_Compact_Edge *pEdge = pTriangle->get_edge( k );
		const IVP_U_Float_Point *pPoint = pEdge->get_start_point( pLedge );

		Vector* pVec = verts + vertIndex;
		ConvertPositionToHL( *pPoint, *pVec );
		vertIndex++;
	}
}

// UNDONE: This doesn't work!!!
void CCollisionQuery::SetTriangleVerts( int convexIndex, int triangleIndex, const Vector *verts )
{
	DebugPrint();
	IVP_Compact_Ledge *pLedge = m_ledges.element_at( convexIndex );
	Triangle( pLedge, triangleIndex );
}

	
int CCollisionQuery::GetTriangleMaterialIndex( int convexIndex, int triangleIndex )
{
	DebugPrint();
	IVP_Compact_Ledge *pLedge = m_ledges.element_at( convexIndex );
	IVP_Compact_Triangle *pTriangle = Triangle( pLedge, triangleIndex );

	return pTriangle->get_material_index();
}

void CCollisionQuery::SetTriangleMaterialIndex( int convexIndex, int triangleIndex, int index7bits )
{
	DebugPrint();
	IVP_Compact_Ledge *pLedge = m_ledges.element_at( convexIndex );
	IVP_Compact_Triangle *pTriangle = Triangle( pLedge, triangleIndex );

	pTriangle->set_material_index( index7bits );
}

IVP_Compact_Triangle *CCollisionQuery::Triangle( IVP_Compact_Ledge *pLedge, int triangleIndex )
{
	DebugPrint();
	if ( !pLedge )
		return NULL;

	return pLedge->get_first_triangle() + triangleIndex;
}


#if 0
void TestCubeVolume( void )
{
	float volume = 0;
	Vector verts[8];
	typedef struct 
	{
		int a, b, c;
	} triangle_t;

	triangle_t triangles[12] = 
	{
		{0,1,3}, // front	0123
		{0,3,2},
		{4,5,1}, // top		4501
		{4,1,0},
		{2,3,7}, // bottom	2367
		{2,7,6},
		{1,5,7}, // right	1537
		{1,7,3},
		{4,0,2}, // left	4062
		{4,2,6},
		{5,4,6}, // back	5476
		{5,6,7}
	};

	int i = 0;
	for ( int x = -1; x <= 1; x +=2 )
		for ( int y = -1; y <= 1; y +=2 )
			for ( int z = -1; z <= 1; z +=2 )
			{
				verts[i][0] = x;
				verts[i][1] = y;
				verts[i][2] = z;
				i++;
			}


	for ( i = 0; i < 12; i++ )
	{
		triangle_t *pTri = triangles + i;
		volume += TetrahedronVolume( verts[0], verts[pTri->a], verts[pTri->b], verts[pTri->c] );
	}
	// should report a volume of 8.  This is a cube that is 2 on a side
	printf("Test volume %.4f\n", volume );
}
#endif


