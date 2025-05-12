//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "tier1/interface.h"
#include "vphysics/object_hash.h"
#include "vphysics/collision_set.h"
#include "tier1/tier1.h"
#include "ivu_vhash.hxx"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// simple 32x32 bit array
class CPhysicsCollisionSet : public IPhysicsCollisionSet
{
public:
	virtual ~CPhysicsCollisionSet() {}
	CPhysicsCollisionSet()
	{
		memset( m_bits, 0, sizeof(m_bits) );
	}
	void EnableCollisions( int index0, int index1 ) override
	{
		DebugPrint();
		Assert(index0<32&&index1<32);
		m_bits[index0] |= 1<<index1;
		m_bits[index1] |= 1<<index0;
	}
	void DisableCollisions( int index0, int index1 ) override
	{
		DebugPrint();
		Assert(index0<32&&index1<32);
		m_bits[index0] &= ~(1<<index1);
		m_bits[index1] &= ~(1<<index0);
	}

	bool ShouldCollide( int index0, int index1 ) override
	{
		DebugPrint();
		Assert(index0<32&&index1<32);
		return (m_bits[index0] & (1<<index1)) ? true : false;
	}
private:
	unsigned int m_bits[32];
};


//-----------------------------------------------------------------------------
// Main physics interface
//-----------------------------------------------------------------------------
class CPhysicsInterface : public CTier1AppSystem<IPhysics>
{
public:
	CPhysicsInterface() : m_pCollisionSetHash(NULL) {}
	void *QueryInterface( const char *pInterfaceName ) override;
	IPhysicsEnvironment *CreateEnvironment( void ) override;
	void DestroyEnvironment( IPhysicsEnvironment *pEnvironment ) override;
	IPhysicsEnvironment *GetActiveEnvironmentByIndex( int index ) override;
	IPhysicsObjectPairHash *CreateObjectPairHash() override;
	void DestroyObjectPairHash( IPhysicsObjectPairHash *pHash ) override;
	IPhysicsCollisionSet *FindOrCreateCollisionSet( unsigned int id, int maxElementCount ) override;
	IPhysicsCollisionSet *FindCollisionSet( unsigned int id ) override;
	void DestroyAllCollisionSets() override;

public:
	CUtlVector<IPhysicsEnvironment *>	m_envList;
	CUtlVector<CPhysicsCollisionSet>	m_collisionSets;
	IVP_VHash_Store						*m_pCollisionSetHash;
};


//-----------------------------------------------------------------------------
// Expose singleton
//-----------------------------------------------------------------------------
static CPhysicsInterface g_MainDLLInterface;
IPhysics *g_PhysicsInternal = &g_MainDLLInterface;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CPhysicsInterface, IPhysics, VPHYSICS_INTERFACE_VERSION, g_MainDLLInterface );


//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CPhysicsInterface::QueryInterface( const char *pInterfaceName )
{
	DebugPrint();
	// Loading the datacache DLL mounts *all* interfaces
	// This includes the backward-compatible interfaces + other vphysics interfaces
	Msg("vphysics - QueryInterface: %s\n", pInterfaceName);
	return Sys_GetFactoryThis()( pInterfaceName, nullptr );
}


//-----------------------------------------------------------------------------
// Implementation of IPhysics
//-----------------------------------------------------------------------------
IPhysicsEnvironment *CPhysicsInterface::CreateEnvironment( void )
{
	DebugPrint();
	IPhysicsEnvironment *pEnvironment = CreatePhysicsEnvironment();
	m_envList.AddToTail( pEnvironment );
	return pEnvironment;
}

void CPhysicsInterface::DestroyEnvironment( IPhysicsEnvironment *pEnvironment )
{
	DebugPrint();
	m_envList.FindAndRemove( pEnvironment );
	delete pEnvironment;
}

IPhysicsEnvironment	*CPhysicsInterface::GetActiveEnvironmentByIndex( int index )
{
	DebugPrint();
	if ( index < 0 || index >= m_envList.Count() )
		return NULL;

	return m_envList[index];
}

IPhysicsObjectPairHash *CPhysicsInterface::CreateObjectPairHash()
{
	DebugPrint();
	return ::CreateObjectPairHash();
}

void CPhysicsInterface::DestroyObjectPairHash( IPhysicsObjectPairHash *pHash )
{
	DebugPrint();
	delete pHash;
}
// holds a cache of these by id.
// NOTE: This is stuffed into vphysics.dll as a sneaky way of sharing the memory between
// client and server in single player.  So you can't have different client/server rules.
IPhysicsCollisionSet *CPhysicsInterface::FindOrCreateCollisionSet( unsigned int id, int maxElementCount )
{
	DebugPrint();
	if ( !m_pCollisionSetHash )
	{
		m_pCollisionSetHash = new IVP_VHash_Store(256);
	}
	Assert( id != 0 );
	Assert( maxElementCount <= 32 );
	if ( maxElementCount > 32 )
		return NULL;

	IPhysicsCollisionSet *pSet = FindCollisionSet( id );
	if ( pSet )
		return pSet;
	intp index = m_collisionSets.AddToTail();
	m_pCollisionSetHash->add_elem( (void *)(intp)id, (void *)(intp)(index+1) );
	return &m_collisionSets[index];
}

IPhysicsCollisionSet *CPhysicsInterface::FindCollisionSet( unsigned int id )
{
	DebugPrint();
	if ( m_pCollisionSetHash )
	{
		intp index = (intp)m_pCollisionSetHash->find_elem( (void *)(intp)id );
		if ( index > 0 )
		{
			Assert( index <= m_collisionSets.Count() );
			if ( index <= m_collisionSets.Count() )
			{
				return &m_collisionSets[index-1];
			}
		}
	}
	return NULL;
}

void CPhysicsInterface::DestroyAllCollisionSets()
{
	DebugPrint();
	m_collisionSets.Purge();
	delete m_pCollisionSetHash;
	m_pCollisionSetHash = NULL;
}

const CUtlVector<IPhysicsEnvironment*>& CPhysicsHolyLib::GetEnvironments()
{
	DebugPrint();
	return g_MainDLLInterface.m_envList;
}