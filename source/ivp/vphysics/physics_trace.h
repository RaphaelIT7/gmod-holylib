//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICS_TRACE_H
#define PHYSICS_TRACE_H
#ifdef _WIN32
#pragma once
#endif


class Vector;
class QAngle;
class CGameTrace;
class CTraceRay;
class IVP_Compact_Surface;
typedef CGameTrace trace_t;
struct Ray_t;
class IVP_Compact_Surface;
class IVP_Compact_Mopp;
class IConvexInfo;
enum
{
	COLLIDE_POLY = 0,
	COLLIDE_MOPP = 1,
	COLLIDE_BALL = 2,
	COLLIDE_VIRTUAL = 3,
};

class IPhysCollide
{
public:
	virtual ~IPhysCollide() {}
	//virtual void AddReference() = 0;
	//virtual void ReleaseReference() = 0;

	// get a surface manager 
	[[nodiscard]] virtual IVP_SurfaceManager *CreateSurfaceManager( short & ) const = 0;
	virtual void GetAllLedges( IVP_U_BigVector<IVP_Compact_Ledge> &ledges ) const = 0;
	[[nodiscard]] virtual size_t GetSerializationSize() const = 0;
	[[nodiscard]] virtual size_t SerializeToBuffer( char *pDest, bool bSwap = false ) const = 0;
	[[nodiscard]] virtual int GetVCollideIndex() const = 0;
	[[nodiscard]] virtual Vector GetMassCenter() const = 0;
	virtual void SetMassCenter( const Vector &massCenter ) = 0;
	[[nodiscard]] virtual Vector GetOrthographicAreas() const = 0;
	virtual void SetOrthographicAreas( const Vector &areas ) = 0;
	[[nodiscard]] virtual float GetSphereRadius() const = 0;
	virtual void OutputDebugInfo() const = 0;
};

#define LEAFMAP_HAS_CUBEMAP					0x0001
#define LEAFMAP_HAS_SINGLE_VERTEX_SPAN		0x0002
#define LEAFMAP_HAS_MULTIPLE_VERTEX_SPANS	0x0004
struct leafmap_t
{
	void *pLeaf;
	unsigned short vertCount;
	byte	flags;
	byte	spanCount;
	unsigned short startVert[8];

	void SetHasCubemap()
	{
		flags = LEAFMAP_HAS_CUBEMAP;
	}

	void SetSingleVertexSpan( unsigned short startVertIndex, unsigned short vertCountIn )
	{
		flags = 0;
		flags |= LEAFMAP_HAS_SINGLE_VERTEX_SPAN;
		startVert[0] = startVertIndex;
		vertCount = vertCountIn;
	}

	[[nodiscard]] int MaxSpans()
	{
		return sizeof(startVert) - sizeof(startVert[0]);
	}
	[[nodiscard]] const byte *GetSpans() const
	{
		return reinterpret_cast<const byte *>(&startVert[1]);
	}
	[[nodiscard]] byte *GetSpans()
	{
		return reinterpret_cast<byte *>(&startVert[1]);
	}

	void SetRLESpans( unsigned short startVertIndex, byte spanCountIn, byte *pSpans )
	{
		flags = 0;
		if ( spanCountIn > MaxSpans() )
			return;
		if ( spanCountIn == 1 )
		{
			SetSingleVertexSpan( startVertIndex, pSpans[0] );
			return;
		}
		// write out a run length encoded list of verts to include in this model
		flags |= LEAFMAP_HAS_MULTIPLE_VERTEX_SPANS;
		startVert[0] = startVertIndex;
		vertCount = 0;
		spanCount = spanCountIn;
		byte *pSpanOut = GetSpans();
		for ( byte i = 0; i < spanCountIn; i++ )
		{
			pSpanOut[i] = pSpans[i];
			if ( !(i & 1) )
			{
				vertCount += pSpans[i];
			}
		}
	}

	[[nodiscard]] inline bool HasSpans() const { return (flags & (LEAFMAP_HAS_SINGLE_VERTEX_SPAN|LEAFMAP_HAS_MULTIPLE_VERTEX_SPANS)) ? true : false; }
	[[nodiscard]] inline bool HasCubemap() const { return (flags & LEAFMAP_HAS_CUBEMAP) ? true : false; }
	[[nodiscard]] inline bool HasSingleVertexSpan() const { return (flags & LEAFMAP_HAS_SINGLE_VERTEX_SPAN) ? true : false; }
	[[nodiscard]] inline bool HasRLESpans() const { return (flags & LEAFMAP_HAS_MULTIPLE_VERTEX_SPANS) ? true : false; }
};

struct collidemap_t
{
	int				leafCount;
	leafmap_t		leafmap[1];
};

extern void InitLeafmap( IVP_Compact_Ledge *pLeaf, leafmap_t *pLeafmapOut );

class CPhysCollide : public IPhysCollide
{
public:
	[[nodiscard]] static CPhysCollide *UnserializeFromBuffer( const char *pBuffer, unsigned int size, int index, bool swap = false );
	[[nodiscard]] virtual const IVP_Compact_Surface *GetCompactSurface() const { return nullptr; }
	[[nodiscard]] Vector GetOrthographicAreas() const override { return Vector(1,1,1); }
	[[nodiscard]] float GetSphereRadius() const override { return 0; }
	virtual void ComputeOrthographicAreas( [[maybe_unused]] float epsilon ) {}
	void SetOrthographicAreas( [[maybe_unused]] const Vector &areas ) override {}
	[[nodiscard]] virtual const collidemap_t *GetCollideMap() const { return nullptr; }
};

class ITraceObject
{
public:
	[[nodiscard]] virtual unsigned short SupportMap( const Vector &dir, Vector *pOut ) const = 0;
	[[nodiscard]] virtual Vector GetVertByIndex( int index ) const = 0;
	[[nodiscard]] virtual float Radius( void ) const = 0;
};

// This is the size of the vertex hash
#define CONVEX_HASH_SIZE		512
// The little hashing trick below allows 64K verts per hash entry
#define	MAX_CONVEX_VERTS		((CONVEX_HASH_SIZE * (1<<16))-1)

class CPhysicsTrace
{
public:
	CPhysicsTrace() = default;
	~CPhysicsTrace() = default;

	// Calculate the intersection of a swept box (mins/maxs) against an IVP object.  All coords are in HL space.
	void SweepBoxIVP( const Vector &start, const Vector &end, const Vector &mins, const Vector &maxs, const CPhysCollide *pSurface, const Vector &surfaceOrigin, const QAngle &surfaceAngles, trace_t *ptr );
	void SweepBoxIVP( const Ray_t &raySrc, unsigned int contentsMask, IConvexInfo *pConvexInfo, const CPhysCollide *pSurface, const Vector &surfaceOrigin, const QAngle &surfaceAngles, trace_t *ptr );

	// Calculate the intersection of a swept compact surface against another compact surface.  All coords are in HL space.
	// NOTE: BUGBUG: swept surface must be single convex!!!
	void SweepIVP( const Vector &start, const Vector &end, const CPhysCollide *pSweptSurface, const QAngle &sweptAngles, const CPhysCollide *pSurface, const Vector &surfaceOrigin, const QAngle &surfaceAngles, trace_t *ptr );

	// get an AABB for an oriented collide
	void GetAABB( Vector *pMins, Vector *pMaxs, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles );

	// get the support map/extent for a collide along the axis given by "direction"
	[[nodiscard]] Vector GetExtent( const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, const Vector &direction );

	[[nodiscard]] bool IsBoxIntersectingCone( const Vector &boxAbsMins, const Vector &boxAbsMaxs, const truncatedcone_t &cone );
};


class CVisitHash
{
public:
	CVisitHash();
	[[nodiscard]] inline unsigned short VertIndexToID( int vertIndex );
	inline void VisitVert( int vertIndex );
	[[nodiscard]] inline bool WasVisited( int vertIndex );
	inline void NewVisit( void );

private:

	// Store the current increment and the vertex ID (rotating hash) to guarantee no collisions
	struct vertmarker_t
	{
		unsigned short visitID;
		unsigned short vertID;
	};

	vertmarker_t		m_vertVisit[CONVEX_HASH_SIZE];
	unsigned short		m_vertVisitID;
	unsigned short		m_isInUse;
};

// Calculate the intersection of a swept box (mins/maxs) against an IVP object.  All coords are in HL space.
inline unsigned short CVisitHash::VertIndexToID( int vertIndex )
{
	// A little hashing trick here:
	// rotate the hash key each time you wrap around at 64K
	// That way, the index will not collide until you've hit 64K # hash entries times
	int high = vertIndex >> 16;
	return (unsigned short) ((vertIndex + high) & 0xFFFF);
}

inline void CVisitHash::VisitVert( int vertIndex ) 
{
	int index = vertIndex & (CONVEX_HASH_SIZE-1);
	m_vertVisit[index].visitID = m_vertVisitID;
	m_vertVisit[index].vertID = VertIndexToID(vertIndex);
}

inline bool CVisitHash::WasVisited( int vertIndex ) 
{
	unsigned short hashIndex = vertIndex & (CONVEX_HASH_SIZE-1);
	unsigned short id = VertIndexToID(vertIndex);
	if ( m_vertVisit[hashIndex].visitID == m_vertVisitID && m_vertVisit[hashIndex].vertID == id )
		return true;

	return false;
}

inline void CVisitHash::NewVisit( void ) 
{ 
	m_vertVisitID++;
	if ( m_vertVisitID == 0 )
	{
		memset( m_vertVisit, 0, sizeof(m_vertVisit) );
	}
}



[[nodiscard]] extern IVP_SurfaceManager *CreateSurfaceManager( const CPhysCollide *pCollisionModel, short &collideType );
extern void OutputCollideDebugInfo( const CPhysCollide *pCollisionModel );

#endif // PHYSICS_TRACE_H
