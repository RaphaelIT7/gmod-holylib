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
#include <cmodel_private.h>
#include "baseserver.h"
#include "clientframe.h"
#include "dt_stack.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

/*
	A module dedicated for debugging https://github.com/Facepunch/garrysmod-issues/issues/5455
	This will replace the entire CBaseServer::WriteDeltaEntities to then get to the SV_WriteEnterPVS which apparently seems to be the connected to the cause from what Rubat found out.
*/

class CNW2DebuggingModule : public IModule
{
public:
#if ARCHITECTURE_X86
	virtual void InitDetour(bool bPreServer) OVERRIDE;
#endif
	virtual const char* Name() { return "nw2debugging"; };
	virtual int Compatibility() { return WINDOWS32 | LINUX32; };
	virtual bool SupportsMultipleLuaStates() { return false; };
	virtual bool IsEnabledByDefault() { return false; };
};

static CNW2DebuggingModule g_pNW2DebuggingModule;
IModule* pNW2DebuggingModule = &g_pNW2DebuggingModule;

#if ARCHITECTURE_X86
class CEntityInfo
{
public:

	CEntityInfo() {
		m_nOldEntity = -1;
		m_nNewEntity = -1;
		m_nHeaderBase = -1;
	}
	virtual	~CEntityInfo() {};
	
	bool			m_bAsDelta;
	CClientFrame	*m_pFrom;
	CClientFrame	*m_pTo;

	UpdateType		m_UpdateType;

	int				m_nOldEntity;	// current entity index in m_pFrom
	int				m_nNewEntity;	// current entity index in m_pTo

	int				m_nHeaderBase;
	int				m_nHeaderCount;

	inline void	NextOldEntity( void ) 
	{
		if ( m_pFrom )
		{
			m_nOldEntity = m_pFrom->transmit_entity.FindNextSetBit( m_nOldEntity+1 );

			if ( m_nOldEntity < 0 )
			{
				// Sentinel/end of list....
				m_nOldEntity = ENTITY_SENTINEL;
			}
		}
		else
		{
			m_nOldEntity = ENTITY_SENTINEL;
		}
	}

	inline void	NextNewEntity( void ) 
	{
		m_nNewEntity = m_pTo->transmit_entity.FindNextSetBit( m_nNewEntity+1 );

		if ( m_nNewEntity < 0 )
		{
			// Sentinel/end of list....
			m_nNewEntity = ENTITY_SENTINEL;
		}
	}
};

#if SYSTEM_WINDOWS && !HOLYLIB_DEVELOPMENT
extern ConVar g_CV_DTWatchEnt;

//-----------------------------------------------------------------------------
// Delta timing stuff.
//-----------------------------------------------------------------------------

static ConVar		sv_deltatime( "sv_deltatime", "0", 0, "Enable profiling of CalcDelta calls" );
static ConVar		sv_deltaprint( "sv_deltaprint", "0", 0, "Print accumulated CalcDelta profiling data (only if sv_deltatime is on)" );

#if defined( DEBUG_NETWORKING )
ConVar  sv_packettrace( "sv_packettrace", "1", 0, "For debugging, print entity creation/deletion info to console." );
#endif

class CChangeTrack
{
public:
	char		*m_pName;
	int			m_nChanged;
	int			m_nUnchanged;
	
	CCycleCount	m_Count;
	CCycleCount	m_EncodeCount;
};


static CUtlLinkedList<CChangeTrack*, int> g_Tracks;
#endif

// These are the main variables used by the SV_CreatePacketEntities function.
// The function is split up into multiple smaller ones and they pass this structure around.
class CEntityWriteInfo : public CEntityInfo
{
public:
	bf_write		*m_pBuf;
	int				m_nClientEntity;

	PackedEntity	*m_pOldPack;
	PackedEntity	*m_pNewPack;

	// For each entity handled in the to packet, mark that's it has already been deleted if that's the case
	CBitVec<MAX_EDICTS>	m_DeletionFlags;
	
	CFrameSnapshot	*m_pFromSnapshot; // = m_pFrom->GetSnapshot();
	CFrameSnapshot	*m_pToSnapshot; // = m_pTo->GetSnapshot();

	CFrameSnapshot	*m_pBaseline; // the clients baseline

	CBaseServer		*m_pServer;	// the server who writes this entity

	int				m_nFullProps;	// number of properties send as full update (Enter PVS)
	bool			m_bCullProps;	// filter props by clients in recipient lists
	
	/* Some profiling data
	int				m_nTotalGap;
	int				m_nTotalGapCount; */
};

#if SYSTEM_WINDOWS && !HOLYLIB_DEVELOPMENT

//-----------------------------------------------------------------------------
// Delta timing helpers.
//-----------------------------------------------------------------------------

CChangeTrack* GetChangeTrack( const char *pName )
{
	FOR_EACH_LL( g_Tracks, i )
	{
		CChangeTrack *pCur = g_Tracks[i];

		if ( stricmp( pCur->m_pName, pName ) == 0 )
			return pCur;
	}

	CChangeTrack *pCur = new CChangeTrack;
	intp len = strlen(pName)+1;
	pCur->m_pName = new char[len];
	Q_strncpy( pCur->m_pName, pName, len );
	pCur->m_nChanged = pCur->m_nUnchanged = 0;
	
	g_Tracks.AddToTail( pCur );
	
	return pCur;
}


void PrintChangeTracks()
{
	ConMsg( "\n\n" );
	ConMsg( "------------------------------------------------------------------------\n" );
	ConMsg( "CalcDelta MS / %% time / Encode MS / # Changed / # Unchanged / Class Name\n" );
	ConMsg( "------------------------------------------------------------------------\n" );

	CCycleCount total, encodeTotal;
	FOR_EACH_LL( g_Tracks, i )
	{
		CChangeTrack *pCur = g_Tracks[i];
		CCycleCount::Add( pCur->m_Count, total, total );
		CCycleCount::Add( pCur->m_EncodeCount, encodeTotal, encodeTotal );
	}

	FOR_EACH_LL( g_Tracks, j )
	{
		CChangeTrack *pCur = g_Tracks[j];
	
		ConMsg( "%6.2fms       %5.2f    %6.2fms    %4d        %4d          %s\n", 
			pCur->m_Count.GetMillisecondsF(),
			pCur->m_Count.GetMillisecondsF() * 100.0f / total.GetMillisecondsF(),
			pCur->m_EncodeCount.GetMillisecondsF(),
			pCur->m_nChanged, 
			pCur->m_nUnchanged, 
			pCur->m_pName
			);
	}

	ConMsg( "\n\n" );
	ConMsg( "Total CalcDelta MS: %.2f\n\n", total.GetMillisecondsF() );
	ConMsg( "Total Encode    MS: %.2f\n\n", encodeTotal.GetMillisecondsF() );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Entity wasn't dealt with in packet, but it has been deleted, we'll flag
//  the entity for destruction
// Input  : type - 
//			entnum - 
//			*from - 
//			*to - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
static inline bool SV_NeedsExplicitDestroy( int entnum, CFrameSnapshot *from, CFrameSnapshot *to )
{
	// Never on uncompressed packet

	if( entnum >= to->m_nNumEntities || to->m_pEntities[entnum].m_pClass == NULL ) // doesn't exits in new
	{
		if ( entnum >= from->m_nNumEntities )
			return false; // didn't exist in old

		// in old, but not in new, destroy.
		if( from->m_pEntities[ entnum ].m_pClass != NULL ) 
		{
			return true;
		}
	}

	return false;
}

#if SYSTEM_WINDOWS && !HOLYLIB_DEVELOPMENT
//-----------------------------------------------------------------------------
// Purpose: Creates a delta header for the entity
//-----------------------------------------------------------------------------
static inline void SV_UpdateHeaderDelta( 
	CEntityWriteInfo &u,
	int entnum )
{
	// Profiling info
	//	u.m_nTotalGap += entnum - u.m_nHeaderBase;
	//	u.m_nTotalGapCount++;

	// Keep track of number of headers so we can tell the client
	u.m_nHeaderCount++;
	u.m_nHeaderBase = entnum;
}


//
// Write the delta header. Also update the header delta info if bUpdateHeaderDelta is true.
//
// There are some cases where you want to tenatively write a header, then possibly back it out.
// In these cases:
// - pass in false for bUpdateHeaderDelta
// - store the return value from SV_WriteDeltaHeader
// - call SV_UpdateHeaderDelta ONLY if you want to keep the delta header it wrote
//
static inline void SV_WriteDeltaHeader(
	CEntityWriteInfo &u,
	int entnum,
	int flags )
{
	bf_write *pBuf = u.m_pBuf;

	// int startbit = pBuf->GetNumBitsWritten();

	int offset = entnum - u.m_nHeaderBase - 1;

	Assert ( offset >= 0 );

	//SyncTag_Write( u.m_pBuf, "Hdr" );

	pBuf->WriteUBitVar( offset );

	if ( flags & FHDR_LEAVEPVS )
	{
		pBuf->WriteOneBit( 1 ); // leave PVS bit
		pBuf->WriteOneBit( flags & FHDR_DELETE );
	}
	else
	{
		pBuf->WriteOneBit( 0 ); // delta or enter PVS
		pBuf->WriteOneBit( flags & FHDR_ENTERPVS );
	}
	
	SV_UpdateHeaderDelta( u, entnum );
}


// NOTE: to optimize this, it could store the bit offsets of each property in the packed entity.
// It would only have to store the offsets for the entities for each frame, since it only reaches 
// into the current frame's entities here.
static CBaseServer* hltv = NULL;
static inline void SV_WritePropsFromPackedEntity( 
	CEntityWriteInfo &u, 
	const int *pCheckProps,
	const int nCheckProps
	)
{
	PackedEntity * pTo = u.m_pNewPack;
	PackedEntity * pFrom = u.m_pOldPack;
	SendTable *pSendTable = pTo->m_pServerClass->m_pTable;

	/*CServerDTITimer timer( pSendTable, SERVERDTI_WRITE_DELTA_PROPS );
	if ( g_bServerDTIEnabled && !u.m_pServer->IsHLTV() && !u.m_pServer->IsReplay() )
	{
		ICollideable *pEnt = sv.edicts[pTo->m_nEntityIndex].GetCollideable();
		ICollideable *pClientEnt = sv.edicts[u.m_nClientEntity].GetCollideable();
		if ( pEnt && pClientEnt )
		{
			float flDist = (pEnt->GetCollisionOrigin() - pClientEnt->GetCollisionOrigin()).Length();
			ServerDTI_AddEntityEncodeEvent( pSendTable, flDist );
		}
	}*/

	const void *pToData;
	int nToBits;

	if ( pTo->IsCompressed() )
	{
		// let server uncompress PackedEntity
		pToData = u.m_pServer->UncompressPackedEntity( pTo, nToBits );
	}
	else
	{
		// get raw data direct
		pToData = pTo->GetData();
		nToBits = pTo->GetNumBits();
	}

	Assert( pToData != NULL );

	// Cull out the properties that their proxies said not to send to this client.
	int pSendProps[MAX_DATATABLE_PROPS];
	const int *sendProps = pCheckProps;
	int nSendProps = nCheckProps;
	bf_write bufStart;


	// cull properties that are removed by SendProxies for this client.
	// don't do that for HLTV relay proxies
	if ( u.m_bCullProps )
	{
		sendProps = pSendProps;

		nSendProps = SendTable_CullPropsFromProxies( 
		pSendTable, 
		pCheckProps, 
		nCheckProps, 
		u.m_nClientEntity-1,
		
		pFrom->GetRecipients(),
		pFrom->GetNumRecipients(),
		
		pTo->GetRecipients(),
		pTo->GetNumRecipients(),

		pSendProps, 
		ARRAYSIZE( pSendProps )
		);
	}
	else
	{
		// this is a HLTV relay proxy
		bufStart = *u.m_pBuf;
	}
		
	SendTable_WritePropList(
		pSendTable, 
		pToData,
		nToBits,
		u.m_pBuf, 
		pTo->m_nEntityIndex,
		
		sendProps,
		nSendProps
		);

	if ( !u.m_bCullProps && hltv )
	{
		// this is a HLTV relay proxy, cache delta bits
		// int nBits = u.m_pBuf->GetNumBitsWritten() - bufStart.GetNumBitsWritten();
		// hltv->m_DeltaCache.AddDeltaBits( pTo->m_nEntityIndex, u.m_pFromSnapshot->m_nTickCount, nBits, &bufStart );
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: See if the entity needs a "hard" reset ( i.e., and explicit creation tag )
//  This should only occur if the entity slot deleted and re-created an entity faster than
//  the last two updates toa  player.  Should never or almost never occur.  You never know though.
// Input  : type - 
//			entnum - 
//			*from - 
//			*to - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
static CThreadLocalInt<intp>* s_ReferenceTick;
static CThreadLocalInt<intp>* s_TargetTick;
static CThreadLocalPtr<CGMODDataTable>* s_CurrentTable;
static bool SV_NeedsExplicitCreate( CEntityWriteInfo &u )
{
	// Never on uncompressed packet
	if ( !u.m_bAsDelta )
	{
		return false;
	}

	const int index = u.m_nNewEntity;

	if ( index >= u.m_pFromSnapshot->m_nNumEntities )
		return true; // entity didn't exist in old frame, so create

	// Server thinks the entity was continues, but the serial # changed, so we might need to destroy and recreate it
	const CFrameSnapshotEntry *pFromEnt = &u.m_pFromSnapshot->m_pEntities[index];
	const CFrameSnapshotEntry *pToEnt = &u.m_pToSnapshot->m_pEntities[index];

	bool bNeedsExplicitCreate = (pFromEnt->m_pClass == NULL) || pFromEnt->m_nSerialNumber != pToEnt->m_nSerialNumber;

#ifdef _DEBUG
	if ( !bNeedsExplicitCreate )
	{
		// If it doesn't need explicit create, then the classnames should match.
		// This assert is analagous to the "Server / Client mismatch" one on the client.
		static int nWhines = 0;
		if ( pFromEnt->m_pClass->GetName() != pToEnt->m_pClass->GetName() )
		{
			if ( ++nWhines < 4 )
			{
				Msg( "ERROR in SV_NeedsExplicitCreate: ent %d from/to classname (%s/%s) mismatch.\n", u.m_nNewEntity, pFromEnt->m_pClass->GetName(), pToEnt->m_pClass->GetName() );
			}
		}
	}
#endif

	return bNeedsExplicitCreate;
}

#if SYSTEM_WINDOWS && !HOLYLIB_DEVELOPMENT
static inline void SV_DetermineUpdateType( CEntityWriteInfo &u )
{
	// Figure out how we want to update the entity.
	if( u.m_nOldEntity == ENTITY_SENTINEL )
	{
		// If the entity was not in the old packet (oldnum == 9999), then 
		// delta from the baseline since this is a new entity.
		u.m_UpdateType = EnterPVS;
		return;
	}
	
	if( u.m_nNewEntity == ENTITY_SENTINEL )
	{
		// If the entity was in the old list, but is not in the new list 
		// (newnum == 9999), then construct a special remove message.
		u.m_UpdateType = LeavePVS;
		return;
	}
	
	Assert( u.m_pToSnapshot->m_pEntities[ u.m_nNewEntity ].m_pClass );

	bool recreate = SV_NeedsExplicitCreate( u );
	
	if ( recreate )
	{
		u.m_UpdateType = EnterPVS;
		return;
	}

	// These should be the same! If they're not, then it should detect an explicit create message.
	Assert( u.m_pOldPack->m_pServerClass == u.m_pNewPack->m_pServerClass);
	
	// We can early out with the delta bits if we are using the same pack handles...
	if ( u.m_pOldPack == u.m_pNewPack )
	{
		Assert( u.m_pOldPack != NULL );
		u.m_UpdateType = PreserveEnt;
		return;
	}

#ifndef _X360
		int nBits;
#if defined( REPLAY_ENABLED )
	if ( !u.m_bCullProps && (hltv || replay) )
	{
		unsigned char *pBuffer = hltv ? hltv  ->m_DeltaCache.FindDeltaBits( u.m_nNewEntity, u.m_pFromSnapshot->m_nTickCount, nBits )
									  : replay->m_DeltaCache.FindDeltaBits( u.m_nNewEntity, u.m_pFromSnapshot->m_nTickCount, nBits );
#else
	if ( !u.m_bCullProps && hltv )
	{
		//unsigned char *pBuffer = hltv->m_DeltaCache.FindDeltaBits( u.m_nNewEntity, u.m_pFromSnapshot->m_nTickCount, nBits );
#endif

		/*if ( pBuffer )
		{
			if ( nBits > 0 )
			{
				// Write a header.
				SV_WriteDeltaHeader( u, u.m_nNewEntity, FHDR_ZERO );

				// just write the cached bit stream 
				u.m_pBuf->WriteBits( pBuffer, nBits );

				u.m_UpdateType = DeltaEnt;
			}
			else
			{
				u.m_UpdateType = PreserveEnt;
			}

			return; // we used the cache, great
		}*/
	}
#endif

	int checkProps[MAX_DATATABLE_PROPS];
	int nCheckProps = u.m_pNewPack->GetPropsChangedAfterTick( u.m_pFromSnapshot->m_nTickCount, checkProps, ARRAYSIZE( checkProps ) );
	
	if ( nCheckProps == -1 )
	{
		// check failed, we have to recalc delta props based on from & to snapshot
		// that should happen only in HLTV/Replay demo playback mode, this code is really expensive

		const void *pOldData, *pNewData;
		int nOldBits, nNewBits;

		if ( u.m_pOldPack->IsCompressed() )
		{
			pOldData = u.m_pServer->UncompressPackedEntity( u.m_pOldPack, nOldBits );
		}
		else
		{
			pOldData = u.m_pOldPack->GetData();
			nOldBits = u.m_pOldPack->GetNumBits();
		}

		if ( u.m_pNewPack->IsCompressed() )
		{
			pNewData = u.m_pServer->UncompressPackedEntity( u.m_pNewPack, nNewBits );
		}
		else
		{
			pNewData = u.m_pNewPack->GetData();
			nNewBits = u.m_pNewPack->GetNumBits();
		}

		nCheckProps = SendTable_CalcDelta(
			u.m_pOldPack->m_pServerClass->m_pTable, 
			pOldData,
			nOldBits,
			pNewData,
			nNewBits,
			checkProps,
			ARRAYSIZE( checkProps ),
			u.m_nNewEntity
			);
	}

#ifndef NO_VCR
	if ( vcr_verbose.GetInt() )
	{
		VCRGenericValueVerify( "checkProps", checkProps, sizeof( checkProps[0] ) * nCheckProps );
	}
#endif
	
	if ( nCheckProps > 0 )
	{
		*s_CurrentTable = u.m_pNewPack->m_pGModDataTable;
		*s_ReferenceTick = u.m_pFromSnapshot ? u.m_pFromSnapshot->m_nTickCount : 0;
		*s_TargetTick = u.m_pToSnapshot->m_nTickCount;

		// Write a header.
		SV_WriteDeltaHeader( u, u.m_nNewEntity, FHDR_ZERO );
#if defined( DEBUG_NETWORKING )
		int startBit = u.m_pBuf->GetNumBitsWritten();
#endif
		SV_WritePropsFromPackedEntity( u, checkProps, nCheckProps );
#if defined( DEBUG_NETWORKING )
		int endBit = u.m_pBuf->GetNumBitsWritten();
		TRACE_PACKET( ( "    Delta Bits (%d) = %d (%d bytes)\n", u.m_nNewEntity, (endBit - startBit), ( (endBit - startBit) + 7 ) / 8 ) );
#endif
		// If the numbers are the same, then the entity was in the old and new packet.
		// Just delta compress the differences.
		u.m_UpdateType = DeltaEnt;

		*s_CurrentTable = NULL;
		*s_ReferenceTick = 0;
		*s_TargetTick = 0;
	}
	else
	{
#ifndef _X360
		if ( !u.m_bCullProps )
		{
			if ( hltv )
			{
				// no bits changed, PreserveEnt
				// hltv->m_DeltaCache.AddDeltaBits( u.m_nNewEntity, u.m_pFromSnapshot->m_nTickCount, 0, NULL );
			}

#if defined( REPLAY_ENABLED )
			if ( replay )
			{
				// no bits changed, PreserveEnt
				replay->m_DeltaCache.AddDeltaBits( u.m_nNewEntity, u.m_pFromSnapshot->m_nTickCount, 0, NULL );
			}
#endif
		}
#endif
		u.m_UpdateType = PreserveEnt;
	}
}

static inline ServerClass* GetEntServerClass(edict_t *pEdict)
{
	return pEdict->GetNetworkable()->GetServerClass();
}

__declspec(naked) int __fastcall getValue1(const CEntityWriteInfo* u)
{
    __asm {
        mov eax, [ecx + 48]    // ecx = pointer to struct
        mov eax, [eax + 16]
        ret
    }
}

__declspec(naked) int __fastcall getValue2(const CEntityWriteInfo* u)
{
    __asm {
        mov eax, [ecx + 1076]
        mov eax, [eax + 4]
        ret
    }
}

__declspec(naked) int __fastcall getValue3(const CEntityWriteInfo* u)
{
    __asm {
        mov eax, [ecx + 1080]
        mov eax, [eax + 4]
        ret
    }
}

static CFrameSnapshotManager* framesnapshotmanager = NULL;
static inline void SV_WriteEnterPVS( CEntityWriteInfo &u )
{
	TRACE_PACKET(( "  SV Enter PVS (%d) %s\n", u.m_nNewEntity, u.m_pNewPack->m_pServerClass->m_pNetworkName ) );

	SV_WriteDeltaHeader( u, u.m_nNewEntity, FHDR_ENTERPVS );

	Assert( u.m_nNewEntity < u.m_pToSnapshot->m_nNumEntities );

	CFrameSnapshotEntry *entry = &u.m_pToSnapshot->m_pEntities[u.m_nNewEntity];

	ServerClass *pClass = entry->m_pClass;

	if ( !pClass )
	{
		Error("SV_WriteEnterPVS: GetEntServerClass failed for ent %d.\n", u.m_nNewEntity);
	}
	
	TRACE_PACKET(( "  SV Enter Class %s\n", pClass->m_pNetworkName ) );

	if ( pClass->m_ClassID >= u.m_pServer->serverclasses )
	{
		ConMsg( "pClass->m_ClassID(%i) >= %i\n", pClass->m_ClassID, u.m_pServer->serverclasses );
		Assert( 0 );
	}

	u.m_pBuf->WriteUBitLong( pClass->m_ClassID, u.m_pServer->serverclassbits );
	
	// Write some of the serial number's bits. 
	u.m_pBuf->WriteUBitLong( entry->m_nSerialNumber, NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS );

	// Get the baseline.
	// Since the ent is in the fullpack, then it must have either a static or an instance baseline.
	PackedEntity *pBaseline = u.m_bAsDelta ? framesnapshotmanager->GetPackedEntity( u.m_pBaseline, u.m_nNewEntity ) : NULL;
	const void *pFromData;
	int nFromBits;

	if ( pBaseline && (pBaseline->m_pServerClass == u.m_pNewPack->m_pServerClass) )
	{
		Assert( !pBaseline->IsCompressed() );
		pFromData = pBaseline->GetData();
		nFromBits = pBaseline->GetNumBits();
	}
	else
	{
		// Since the ent is in the fullpack, then it must have either a static or an instance baseline.
		int nFromBytes;
		if ( !u.m_pServer->GetClassBaseline( pClass, &pFromData, &nFromBytes ) )
		{
			Error( "SV_WriteEnterPVS: missing instance baseline for '%s'.", pClass->m_pNetworkName );
		}

		ErrorIfNot( pFromData,
			("SV_WriteEnterPVS: missing pFromData for '%s'.", pClass->m_pNetworkName)
		);
		
		nFromBits = nFromBytes * 8;	// NOTE: this isn't the EXACT number of bits but that's ok since it's
									// only used to detect if we overran the buffer (and if we do, it's probably
									// by more than 7 bits).
	}

	if ( u.m_pTo->from_baseline )
	{
		// remember that we sent this entity as full update from entity baseline
		u.m_pTo->from_baseline->Set( u.m_nNewEntity );
	}

	const void *pToData;
	int nToBits;

	if ( u.m_pNewPack->IsCompressed() )
	{
		pToData = u.m_pServer->UncompressPackedEntity( u.m_pNewPack, nToBits );
	}
	else
	{
		pToData = u.m_pNewPack->GetData();
		nToBits = u.m_pNewPack->GetNumBits();
	}

	// Setup gmod shit
	*s_CurrentTable = u.m_pNewPack->m_pGModDataTable;
	*s_ReferenceTick = u.m_pFromSnapshot ? u.m_pFromSnapshot->m_nTickCount : 0;
	*s_TargetTick = u.m_pToSnapshot->m_nTickCount;

	// send all changed properties when entering PVS (no SendProxy culling since we may use it as baseline
	u.m_nFullProps +=  SendTable_WriteAllDeltaProps( pClass->m_pTable, pFromData, nFromBits,
		pToData, nToBits, u.m_pNewPack->m_nEntityIndex, u.m_pBuf );

	*s_CurrentTable = NULL;
	*s_ReferenceTick = 0;
	*s_TargetTick = 0;

	if ( u.m_nNewEntity == u.m_nOldEntity )
		u.NextOldEntity();  // this was a entity recreate

	u.NextNewEntity();
}


static inline void SV_WriteLeavePVS( CEntityWriteInfo &u )
{
	int headerflags = FHDR_LEAVEPVS;
	bool deleteentity = false;
	
	if ( u.m_bAsDelta )
	{
		deleteentity = SV_NeedsExplicitDestroy( u.m_nOldEntity, u.m_pFromSnapshot, u.m_pToSnapshot );	
	}
	
	if ( deleteentity )
	{
		// Mark that we handled deletion of this index
		u.m_DeletionFlags.Set( u.m_nOldEntity );

		headerflags |= FHDR_DELETE;
	}

	TRACE_PACKET( ( "  SV Leave PVS (%d) %s %s\n", u.m_nOldEntity, 
		deleteentity ? "deleted" : "left pvs",
		u.m_pOldPack->m_pServerClass->m_pNetworkName ) );

	SV_WriteDeltaHeader( u, u.m_nOldEntity, headerflags );

	u.NextOldEntity();
}


static inline void SV_WriteDeltaEnt( CEntityWriteInfo &u )
{
	TRACE_PACKET( ( "  SV Delta PVS (%d %d) %s\n", u.m_nNewEntity, u.m_nOldEntity, u.m_pOldPack->m_pServerClass->m_pNetworkName ) );

	// NOTE: it was already written in DetermineUpdateType. By doing it this way, we avoid an expensive
	// (non-byte-aligned) copy of the data.

	u.NextOldEntity();
	u.NextNewEntity();
}


static inline void SV_PreserveEnt( CEntityWriteInfo &u )
{
	TRACE_PACKET( ( "  SV Preserve PVS (%d) %s\n", u.m_nOldEntity, u.m_pOldPack->m_pServerClass->m_pNetworkName ) );

	// updateType is preserveEnt. The client will detect this because our next entity will have a newnum
	// that is greater than oldnum, in which case the client just keeps the current entity alive.
	u.NextOldEntity();
	u.NextNewEntity();
}


static inline void SV_WriteEntityUpdate( CEntityWriteInfo &u )
{
	switch( u.m_UpdateType )
	{
		case EnterPVS:
		{
			SV_WriteEnterPVS( u );
		}
		break;

		case LeavePVS:
		{
			SV_WriteLeavePVS( u );
		}
		break;

		case DeltaEnt:
		{
			SV_WriteDeltaEnt( u );
		}
		break;

		case PreserveEnt:
		{
			SV_PreserveEnt( u );
		}
		break;
	}
}


static inline int SV_WriteDeletions( CEntityWriteInfo &u )
{
	if( !u.m_bAsDelta )
		return 0;

	int nNumDeletions = 0;

	CFrameSnapshot *pFromSnapShot = u.m_pFromSnapshot;
	CFrameSnapshot *pToSnapShot = u.m_pToSnapshot;

	int nLast = MAX( pFromSnapShot->m_nNumEntities, pToSnapShot->m_nNumEntities );
	for ( int i = 0; i < nLast; i++ )
	{
		// Packet update didn't clear it out expressly
		if ( u.m_DeletionFlags.Get( i ) ) 
			continue;

		// If the entity is marked to transmit in the u.m_pTo, then it can never be destroyed by the m_iExplicitDeleteSlots
		// Another possible fix would be to clear any slots in the explicit deletes list that were actually occupied when a snapshot was taken
		if ( u.m_pTo->transmit_entity.Get(i) )
			continue;

		// Looks like it should be gone
		bool bNeedsExplicitDelete = SV_NeedsExplicitDestroy( i, pFromSnapShot, pToSnapShot );
		if ( !bNeedsExplicitDelete && u.m_pTo )
		{
			bNeedsExplicitDelete = ( pToSnapShot->m_iExplicitDeleteSlots.Find(i) != pToSnapShot->m_iExplicitDeleteSlots.InvalidIndex() );
			// We used to do more stuff here as a sanity check, but I don't think it was necessary since the only thing that would unset the bould would be a "recreate" in the same slot which is
			// already implied by the u.m_pTo->transmit_entity.Get(i) check
		}

		// Check conditions
		if ( bNeedsExplicitDelete )
		{
			TRACE_PACKET( ( "  SV Explicit Destroy (%d)\n", i ) );

			u.m_pBuf->WriteOneBit(1);
			u.m_pBuf->WriteUBitLong( i, MAX_EDICT_BITS );
			++nNumDeletions;
		}
	}
	// No more entities..
	u.m_pBuf->WriteOneBit(0); 

	return nNumDeletions;
}
#endif

/*
=============
WritePacketEntities

Computes either a compressed, or uncompressed delta buffer for the client.
Returns the size IN BITS of the message buffer created.
=============
*/

static CBaseServer* g_pBaseServer = NULL;
static Detouring::Hook detour_CBaseServer_WriteDeltaEntities;
#if SYSTEM_WINDOWS && !HOLYLIB_DEVELOPMENT
static void hook_CBaseServer_WriteDeltaEntities( CBaseClient *client, CClientFrame *to, CClientFrame *from, bf_write &pBuf )
{
	// ToDo: Use ASM to get the CBaseServer variable from ecx
	__asm {
		mov g_pBaseServer, ecx;
	}

	VPROF_BUDGET( "CBaseServer::WriteDeltaEntities", VPROF_BUDGETGROUP_OTHER_NETWORKING );
	// Setup the CEntityWriteInfo structure.
	CEntityWriteInfo u;
	u.m_pBuf = &pBuf;
	u.m_pTo = to;
	u.m_pToSnapshot = to->GetSnapshot();
	u.m_pBaseline = client->m_pBaseline;
	u.m_nFullProps = 0;
	u.m_pServer = g_pBaseServer;
	u.m_nClientEntity = client->m_nEntityIndex;
#ifndef _XBOX
	if ( g_pBaseServer->IsHLTV() || g_pBaseServer->IsReplay() )
	{
		// cull props only on master proxy
		u.m_bCullProps = g_pBaseServer->IsActive();
	}
	else
#endif
	{
		u.m_bCullProps = true;	// always cull props for players
	}
	
	if ( from != NULL )
	{
		u.m_bAsDelta = true;	
		u.m_pFrom = from;
		u.m_pFromSnapshot = from->GetSnapshot();
		Assert( u.m_pFromSnapshot );
	}
	else
	{
		u.m_bAsDelta = false;
		u.m_pFrom = NULL;
		u.m_pFromSnapshot = NULL;
	}

	u.m_nHeaderCount = 0;
//	u.m_nTotalGap = 0;
//	u.m_nTotalGapCount = 0;

	// set from_baseline pointer if this snapshot may become a baseline update
	if ( client->m_nBaselineUpdateTick == -1 )
	{
		client->m_BaselinesSent.ClearAll();
		to->from_baseline = &client->m_BaselinesSent;
	}

	// Write the header, TODO use class SVC_PacketEntities
		
	TRACE_PACKET(( "WriteDeltaEntities (%d)\n", u.m_pToSnapshot->m_nNumEntities ));

	u.m_pBuf->WriteUBitLong( svc_PacketEntities, NETMSG_TYPE_BITS );

	u.m_pBuf->WriteUBitLong( u.m_pToSnapshot->m_nNumEntities, MAX_EDICT_BITS );
	
	if ( u.m_bAsDelta )
	{
		u.m_pBuf->WriteOneBit( 1 ); // use delta sequence

		u.m_pBuf->WriteLong( u.m_pFrom->tick_count );    // This is the sequence # that we are updating from.
	}
	else
	{
		u.m_pBuf->WriteOneBit( 0 );	// use baseline
	}

	u.m_pBuf->WriteUBitLong ( client->m_nBaselineUsed, 1 );	// tell client what baseline we are using

	// Store off current position 
	bf_write savepos = *u.m_pBuf;

	// Save room for number of headers to parse, too
	u.m_pBuf->WriteUBitLong ( 0, MAX_EDICT_BITS+DELTASIZE_BITS+1 );	
		
	int startbit = u.m_pBuf->GetNumBitsWritten();

	/*bool bIsTracing = client->IsTracing();
	if ( bIsTracing )
	{
		client->TraceNetworkData( pBuf, "Delta Entities Overhead" );
	}*/

	// Don't work too hard if we're using the optimized single-player mode.
	// if ( !g_pLocalNetworkBackdoor )
	{
		// Iterate through the in PVS bitfields until we find an entity 
		// that was either in the old pack or the new pack
		u.NextOldEntity();
		u.NextNewEntity();
		
		while ( (u.m_nOldEntity != ENTITY_SENTINEL) || (u.m_nNewEntity != ENTITY_SENTINEL) )
		{
			u.m_pNewPack = (u.m_nNewEntity != ENTITY_SENTINEL) ? framesnapshotmanager->GetPackedEntity( u.m_pToSnapshot, u.m_nNewEntity ) : NULL;
			u.m_pOldPack = (u.m_nOldEntity != ENTITY_SENTINEL) ? framesnapshotmanager->GetPackedEntity( u.m_pFromSnapshot, u.m_nOldEntity ) : NULL;
			int nEntityStartBit = pBuf.GetNumBitsWritten();

			// Figure out how we want to write this entity.
			SV_DetermineUpdateType( u  );
			SV_WriteEntityUpdate( u );

			/*if ( !bIsTracing )
				continue;

			switch ( u.m_UpdateType )
			{
			default:
			case PreserveEnt:
				break;
			case EnterPVS:
				{
					char const *eString = sv.edicts[ u.m_pNewPack->m_nEntityIndex ].GetNetworkable()->GetClassName();
					client->TraceNetworkData( pBuf, "enter [%s]", eString );
					// ETWMark1I( eString, pBuf.GetNumBitsWritten() - nEntityStartBit );
				}
				break;
			case LeavePVS:
				{
					// Note, can't use GetNetworkable() since the edict has been freed at this point
					char const *eString = u.m_pOldPack->m_pServerClass->m_pNetworkName;
					client->TraceNetworkData( pBuf, "leave [%s]", eString );
					// ETWMark1I( eString, pBuf.GetNumBitsWritten() - nEntityStartBit );
				}
				break;
			case DeltaEnt:
				{
					char const *eString = sv.edicts[ u.m_pOldPack->m_nEntityIndex ].GetNetworkable()->GetClassName();
					client->TraceNetworkData( pBuf, "delta [%s]", eString );
					// ETWMark1I( eString, pBuf.GetNumBitsWritten() - nEntityStartBit );
				}
				break;
			}*/
		}

		// Now write out the express deletions
		int nNumDeletions = SV_WriteDeletions( u );
		/*if ( bIsTracing )
		{
			client->TraceNetworkData( pBuf, "Delta: [%d] deletions", nNumDeletions );
		}*/
	}

	// get number of written bits
	int length = u.m_pBuf->GetNumBitsWritten() - startbit;

	// go back to header and fill in correct length now
	savepos.WriteUBitLong( u.m_nHeaderCount, MAX_EDICT_BITS );
	savepos.WriteUBitLong( length, DELTASIZE_BITS );

	bool bUpdateBaseline = ( (client->m_nBaselineUpdateTick == -1) && 
		(u.m_nFullProps > 0 || !u.m_bAsDelta) );

	if ( bUpdateBaseline && u.m_pBaseline )
	{
		// tell client to use this snapshot as baseline update
		savepos.WriteOneBit( 1 ); 
		client->m_nBaselineUpdateTick = to->tick_count;
	}
	else
	{
		savepos.WriteOneBit( 0 ); 
	}

	/*if ( bIsTracing )
	{
		client->TraceNetworkData( pBuf, "Delta Finish" );
	}*/
}
#else
enum FunnyUpdateTypes {
	FU_EnterPVS = 0,
	FU_LeavePVS = 1,
	FU_ExplicitCreate = 2,
	FU_ExplicitDestroy = 3,
	FU_DeltaOrPreserve = 4,
};

static inline FunnyUpdateTypes Test_SV_DetermineUpdateType( CEntityWriteInfo &u )
{
	if( u.m_nOldEntity == ENTITY_SENTINEL )
	{
		return FunnyUpdateTypes::FU_EnterPVS;
	}

	/*bool destroy = u.m_bAsDelta && SV_NeedsExplicitDestroy( u.m_nOldEntity, u.m_pFromSnapshot, u.m_pToSnapshot );
	if ( destroy )
	{
		return FunnyUpdateTypes::FU_ExplicitDestroy;
	}*/
	
	if( u.m_nNewEntity == ENTITY_SENTINEL )
	{
		return FunnyUpdateTypes::FU_LeavePVS;
	}

	bool recreate = SV_NeedsExplicitCreate( u );
	if ( recreate )
	{
		return FunnyUpdateTypes::FU_ExplicitCreate;
	}
	
	Assert( u.m_pToSnapshot->m_pEntities[ u.m_nNewEntity ].m_pClass );
	
	return FunnyUpdateTypes::FU_DeltaOrPreserve;
}

static std::unordered_map<int, int> pVisitedNewEntity;
static CFrameSnapshotManager* framesnapshotmanager = NULL;
static void hook_CBaseServer_WriteDeltaEntities(CBaseServer* pServer, CBaseClient *client, CClientFrame *to, CClientFrame *from, bf_write &pBuf )
{
	// Setup the CEntityWriteInfo structure.
	CEntityWriteInfo u;
	u.m_pBuf = &pBuf;
	u.m_pTo = to;
	u.m_pToSnapshot = to->GetSnapshot();
	u.m_pBaseline = client->m_pBaseline;
	u.m_nFullProps = 0;
	u.m_pServer = g_pBaseServer;
	u.m_nClientEntity = client->m_nEntityIndex;

	if ( from != NULL )
	{
		u.m_bAsDelta = true;
		if (!client->IsHLTV()) // Not a HLTV Client
		{
			int baselineTick = client->m_nBaselineUpdateTick;
			if (baselineTick > from->tick_count)
			{
				DevMsg("CBaseServer::WriteDeltaEntities: Baseline is newer than delta tick! %i - %i\n", baselineTick, from->tick_count);
				while(from->m_pNext && from->m_pNext->tick_count <= baselineTick) // Skip forward to the baseline frame where we already wrote EnterPVS
				{
					from = from->m_pNext;
				}
			}
		}
		u.m_pFrom = from;
		u.m_pFromSnapshot = from->GetSnapshot();

		Assert( u.m_pFromSnapshot );
	}
	else
	{
		u.m_bAsDelta = false;
		u.m_pFrom = NULL;
		u.m_pFromSnapshot = NULL;
	}

	pVisitedNewEntity.clear();
	std::unordered_map<int, int> pVisitedOldEntity;
	{
		// Iterate through the in PVS bitfields until we find an entity 
		// that was either in the old pack or the new pack
		u.NextOldEntity();
		u.NextNewEntity();
		
		int previousType = -1;
		while ( (u.m_nOldEntity != ENTITY_SENTINEL) || (u.m_nNewEntity != ENTITY_SENTINEL) )
		{
			u.m_pNewPack = (u.m_nNewEntity != ENTITY_SENTINEL) ? framesnapshotmanager->GetPackedEntity( u.m_pToSnapshot, u.m_nNewEntity ) : NULL;
			u.m_pOldPack = (u.m_nOldEntity != ENTITY_SENTINEL) ? framesnapshotmanager->GetPackedEntity( u.m_pFromSnapshot, u.m_nOldEntity ) : NULL;
			
			FunnyUpdateTypes type = Test_SV_DetermineUpdateType(u);
			
			if (pVisitedNewEntity.find(u.m_nNewEntity) != pVisitedNewEntity.end())
			{
				Msg("m_nNewEntity: Tried to write a entity that we already went over? %i - %i - %i | %i - %i\n", pVisitedNewEntity.find(u.m_nNewEntity)->second, (int)type, previousType, u.m_nNewEntity, u.m_nOldEntity);
			}

			if (pVisitedOldEntity.find(u.m_nOldEntity) != pVisitedOldEntity.end())
			{
				Msg("m_nOldEntity: Tried to write a entity that we already went over? %i - %i - %i | %i - %i\n", pVisitedOldEntity.find(u.m_nOldEntity)->second, (int)type, previousType, u.m_nNewEntity, u.m_nOldEntity);
			}
			previousType = type;

			if (type == FunnyUpdateTypes::FU_DeltaOrPreserve)
			{
				pVisitedOldEntity[u.m_nOldEntity] = (int)type;
				u.NextOldEntity();
				pVisitedNewEntity[u.m_nNewEntity] = (int)type;
				u.NextNewEntity();
				continue;
			}

			if (type == FunnyUpdateTypes::FU_LeavePVS || type == FunnyUpdateTypes::FU_ExplicitDestroy)
			{
				pVisitedOldEntity[u.m_nOldEntity] = (int)type;
				u.NextOldEntity();
				continue;
			}

			if (pVisitedOldEntity.find(u.m_nOldEntity) != pVisitedOldEntity.end())
			{
				Msg("EnterPVS: Tried to join pvs? %i - %i:%i - %i:%i - %s\n", pVisitedOldEntity.find(u.m_nOldEntity)->second, (int)type, previousType, u.m_nNewEntity, u.m_nOldEntity, (u.m_bAsDelta && SV_NeedsExplicitDestroy( u.m_nOldEntity, u.m_pFromSnapshot, u.m_pToSnapshot )) ? "true" : "false");
			}
			
			if (u.m_nNewEntity == u.m_nOldEntity)
			{
				Msg("EnterPVS: Tried to join pvs? %i:%i - %i:%i - %s\n", (int)type, previousType, u.m_nNewEntity, u.m_nOldEntity, (u.m_bAsDelta && SV_NeedsExplicitDestroy( u.m_nOldEntity, u.m_pFromSnapshot, u.m_pToSnapshot )) ? "true" : "false");
				pVisitedOldEntity[u.m_nOldEntity] = (int)previousType;
				u.NextOldEntity();
			}

			Msg("EnterPVS: joined pvs? %i:%i - %i:%i - %s\n", (int)type, previousType, u.m_nNewEntity, u.m_nOldEntity, (u.m_bAsDelta && SV_NeedsExplicitDestroy( u.m_nNewEntity, u.m_pFromSnapshot, u.m_pToSnapshot )) ? "true" : "false");
			pVisitedNewEntity[u.m_nNewEntity] = (int)type;
			u.NextNewEntity();
		}
	}

	detour_CBaseServer_WriteDeltaEntities.GetTrampoline<Symbols::CBaseServer_WriteDeltaEntities>()(pServer, client, to, from, pBuf);
}

static Detouring::Hook detour_SV_DetermineUpdateType;
static void hook_SV_DetermineUpdateType(CEntityWriteInfo& u)
{
	int origEnt = -1;
	if (u.m_nNewEntity != ENTITY_SENTINEL && u.m_nOldEntity != ENTITY_SENTINEL && u.m_nNewEntity < u.m_nOldEntity)
	{
		origEnt = u.m_nOldEntity;
		u.m_nOldEntity = u.m_nNewEntity;
	}

	detour_SV_DetermineUpdateType.GetTrampoline<Symbols::SV_DetermineUpdateType>()(u);

	if (origEnt != -1)
	{
		u.m_nOldEntity = origEnt;
	}

	if (u.m_UpdateType == UpdateType::EnterPVS)
	{
		Msg("SV_DetermineUpdateType: Entity entered PVS: %i - %i | %i - %i\n", u.m_nNewEntity, origEnt != -1 ? origEnt :  u.m_nOldEntity, u.m_pFromSnapshot ? u.m_pFromSnapshot->m_nTickCount : -1, u.m_pToSnapshot ? u.m_pToSnapshot->m_nTickCount : -1);
	}
}

static bool enterPVS = false;
static bool explicitEnterPVS = false;
static int currentPVSEnterEntity = -1;
static Detouring::Hook detour_SV_WriteEnterPVS;
static void hook_SV_WriteEnterPVS(CEntityWriteInfo& u)
{
	Msg("SV_WriteEnterPVS: Entity entered PVS %i\n", u.m_pToSnapshot->m_nTickCount);
	enterPVS = true;
	currentPVSEnterEntity = u.m_nNewEntity;
	auto it = pVisitedNewEntity.find(currentPVSEnterEntity);
	if (it != pVisitedNewEntity.end())
	{
		explicitEnterPVS = it->second == FunnyUpdateTypes::FU_ExplicitCreate;
		Msg("SV_WriteEnterPVS: Entity explicit entered PVS %i - %i\n", u.m_pToSnapshot->m_nTickCount, currentPVSEnterEntity);
	}
	detour_SV_WriteEnterPVS.GetTrampoline<Symbols::SV_DetermineUpdateType>()(u);
	explicitEnterPVS = false;
	enterPVS = false;
	currentPVSEnterEntity = -1;
}

typedef bool (*CBaseClient_UpdateAcknowledgedFramecount)(CBaseClient* pClient, int tick);
static Detouring::Hook detour_CBaseClient_UpdateAcknowledgedFramecount;
static bool hook_CBaseClient_UpdateAcknowledgedFramecount(CBaseClient* pClient, int tick)
{
	if (pClient->IsFakeClient())
	{
		return detour_CBaseClient_UpdateAcknowledgedFramecount.GetTrampoline<CBaseClient_UpdateAcknowledgedFramecount>()(pClient, tick);
	}

	int currentDelta = pClient->m_nDeltaTick;
	int currentBaseline = pClient->m_nBaselineUpdateTick;
	bool bRet = detour_CBaseClient_UpdateAcknowledgedFramecount.GetTrampoline<CBaseClient_UpdateAcknowledgedFramecount>()(pClient, tick);
	if ((currentBaseline > -1) && (tick > currentBaseline) && pClient->m_nBaselineUpdateTick == -1)
	{
		Warning("Forcing full update since baseline update was lost!\n");
		return detour_CBaseClient_UpdateAcknowledgedFramecount.GetTrampoline<CBaseClient_UpdateAcknowledgedFramecount>()(pClient, -1);
	}
	
	return bRet;
}

#include "GarrysMod/IGMODDataTable.h"

// I wrote all of this code like a year ago or even longer, when I had initially discovered the NW2 bug and tried finding the cause.
void F_Write(bf_write* bf, const CGMODVariant& var)
{
	float value = var.m_Float;
	if (var.type == 7)
	{
		value = V_atof(var.m_pString);
	}

	bf->WriteFloat(value);
}

void F_Read(bf_read* bf, CGMODVariant& var)
{
	var.m_Float = bf->ReadFloat();
	var.type = CGMODVariant_Number;
}

void F_Skip(bf_read* bf)
{
	bf->SeekRelative(32);
}

void I_Write(bf_write* bf, const CGMODVariant& var)
{
	int value = var.m_Int;
	if (var.type == 7)
	{
		value = V_atof(var.m_pString);
	}

	bf->WriteBitLong(value, 32, true);
}

void I_Read(bf_read* bf, CGMODVariant& var)
{
	//var.m_Float = bf->ReadBitLong( 32, true );
	var.type = CGMODVariant_Int;
}

void I_Skip(bf_read* bf)
{
	bf->SeekRelative(32);
}

void B_Write(bf_write* bf, const CGMODVariant& var)
{
	bool value = var.m_Bool;
	if (var.type == 7)
	{
		value = V_stricmp(var.m_pString, "true") == 0;
	}

	bf->WriteOneBit(value);
}

void B_Read(bf_read* bf, CGMODVariant& var)
{
	var.m_Bool = bf->ReadOneBit();
	var.type = CGMODVariant_Bool;
}

void B_Skip(bf_read* bf)
{
	bf->SeekRelative(1);
}

void V_Write(bf_write* bf, const CGMODVariant& var)
{
	Vector value = var.m_Vec;
	if (var.type == 7)
	{
		// This is getting out of hand xd
		sscanf(var.m_pString, "%f %f %f", &value.x, &value.y, &value.z);
	}

	bf->WriteBitVec3Coord(value);
}

void V_Read(bf_read* bf, CGMODVariant& var)
{
	bf->ReadBitVec3Coord(var.m_Vec);
	var.type = CGMODVariant_Vector;
}

void V_Skip(bf_read* bf) // GMOD actually reads the floats
{
	bf->SeekRelative(96); // 32(float) * 3
}

void A_Write(bf_write* bf, const CGMODVariant& var)
{
	QAngle value = var.m_Ang;
	if (var.type == 7)
	{
		// This is really getting out of hand
		sscanf(var.m_pString, "%f %f %f", &value.x, &value.y, &value.z);
	}

	bf->WriteBitAngles(value);
}

void A_Read(bf_read* bf, CGMODVariant& var)
{
	bf->ReadBitAngles(var.m_Ang);
	var.type = CGMODVariant_Angle;
}

void A_Skip(bf_read* bf) // GMOD actually reads the floats
{
	bf->SeekRelative(96); // 32(float) * 3
}

void E_Write(bf_write* bf, const CGMODVariant& var)
{
	int value = var.m_Ent;
	if (var.type == 7)
	{
		// Why
		value = V_atoi(var.m_pString);
	}

	bf->WriteShort(value);
}

void E_Read(bf_read* bf, CGMODVariant& var)
{
	var.m_Ent = bf->ReadShort();
	var.type = CGMODVariant_Entity;
}

void E_Skip(bf_read* bf)
{
	bf->SeekRelative(NUM_NETWORKED_EHANDLE_BITS);
}

static constexpr int GMODDT_STRING_BITS = 9; // This is why we got that 511 character limit!
void S_Write(bf_write* bf, const CGMODVariant& var)
{
	const char* string = var.m_pString;

	switch (var.type)
	{
	case CGMODVariant_Number:
		// ToDo %g
		break;
	case CGMODVariant_Vector:
		// ToDo %g %g %g
		break;
	case CGMODVariant_Angle:
		// ToDo %g %g %g
		break;
	case CGMODVariant_Int:
		// ToDo %d
		break;
	case CGMODVariant_Entity:
		// ToDo %d
		break;
	case CGMODVariant_Bool:
		string = var.m_Bool ? "true" : "false";
		break;
	default:
		break;
	}

	int length = strlen(string);
	bf->WriteUBitLong(length, GMODDT_STRING_BITS);
	bf->WriteBytes(string, length);
}

void S_Read(bf_read* bf, CGMODVariant& var)
{
	var.m_Length = bf->ReadUBitLong(GMODDT_STRING_BITS) + 1;
	if (var.m_pString)
	{
		var.m_pString = (char*)realloc(var.m_pString, var.m_Length * sizeof(char));
	}
	else {
		var.m_pString = (char*)malloc(var.m_Length * sizeof(char));
	}

	bf->ReadBytes(var.m_pString, var.m_Length);
	var.m_pString[var.m_Length] = '\0';
}

void S_Skip(bf_read* bf)
{
	int length = bf->ReadUBitLong(GMODDT_STRING_BITS);
	bf->SeekRelative(8 * length); // Skip it
}

// Only for String's since in our testing we mainly use them. Fk the others for now, I don't got the time to deal with them.
bool S_Compare(bf_read* pDelta1, bf_read* pDelta2)
{
	unsigned int lengthDelta1 = pDelta1->ReadUBitLong(GMODDT_STRING_BITS);
	unsigned int lengthDelta2 = pDelta2->ReadUBitLong(GMODDT_STRING_BITS);

	if (lengthDelta1 != lengthDelta2)
	{
		pDelta1->SeekRelative(8 * lengthDelta1);
		pDelta2->SeekRelative(8 * lengthDelta2);
		return true;
	}

	for (unsigned int i = 0; i<lengthDelta1; ++i)
	{
		if (pDelta1->ReadByte() != pDelta2->ReadByte())
		{
			int bitsLeft = lengthDelta1 - i - 1;
			if (bitsLeft > 0)
			{
				pDelta1->SeekRelative(8 * bitsLeft);
				pDelta2->SeekRelative(8 * bitsLeft);
			}
			return true;
		}
	}

	return false;
}

typedef struct
{

	void			(*Write) (bf_write* bf, const CGMODVariant& var);
	void			(*Read) (bf_read* bf, CGMODVariant& var);
	void			(*Skip) (bf_read* bf);
	bool			(*Compare) (bf_read* bf1, bf_read* bf2); // I forgot this all thoes years ago, so now I had to do all of these since we need them...
} VariantInfoType;

VariantInfoType s_VariantInfo[CGMODVariant_Count] =
{
	// CGMODVariant_NIL
	{
		NULL,
		NULL,
		NULL,
		NULL,
	},

	// CGMODVariant_Float
	{
		F_Write,
		F_Read,
		F_Skip,
		NULL,
	},

	// CGMODVariant_Int
	{
		I_Write,
		I_Read,
		I_Skip,
		NULL,
	},

	// CGMODVariant_Bool
	{
		B_Write,
		B_Read,
		B_Skip,
		NULL,
	},

	// CGMODVariant_Vector
	{
		V_Write,
		V_Read,
		V_Skip,
		NULL,
	},

	// CGMODVariant_Angle
	{
		A_Write,
		A_Read,
		A_Skip,
		NULL,
	},

	// CGMODVariant_Entity
	{
		E_Write,
		E_Read,
		E_Skip,
		NULL,
	},

	// CGMODVariant_String
	{
		S_Write,
		S_Read,
		S_Skip,
		S_Compare,
	},
};

static constexpr int GMODDT_INDEX_BITS = 12;
static constexpr int GMODDT_TYPE_BITS = 3;
static constexpr int GMODDT_INVALID_INDEX = USHRT_MAX;

// s_VariantInfo has no debug symbol so we can't just load it :sob:
static void GMODVariant_Skip(bf_read* pBuffer)
{
	unsigned int type = pBuffer->ReadUBitLong(GMODDT_TYPE_BITS);
	if (s_VariantInfo[type].Skip)
		s_VariantInfo[type].Skip(pBuffer);
}

static bool GMODVariant_Compare(bf_read* pDelta1, bf_read* pDelta2)
{
	unsigned int type1 = pDelta1->ReadUBitLong(GMODDT_TYPE_BITS);
	unsigned int type2 = pDelta2->ReadUBitLong(GMODDT_TYPE_BITS);

	if (type1 != type2)
	{
		if (s_VariantInfo[type1].Skip) {
			s_VariantInfo[type1].Skip(pDelta1);
		}

		if (s_VariantInfo[type2].Skip) {
			s_VariantInfo[type2].Skip(pDelta2);
		}
		return true;
	}

	if (s_VariantInfo[type1].Compare) {
		return s_VariantInfo[type1].Compare(pDelta1, pDelta2);
	}

	return false;
}

class CGMODDataTable : public IGMODDataTable
{
public:
	struct Entry // CGMODDataTable::Entry
	{
	public:
		int m_iChangeTick = 0;
		CGMODVariant m_pValue;
	};

	// CUtlRBTree<CUtlMap<unsigned short,CGMODDataTable::Entry,unsigned short>::Node_t,unsigned short,CUtlMap<unsigned short,CGMODDataTable::Entry,unsigned short>::CKeyLess,CUtlMemory<UtlRBTreeNode_t<CUtlMap<unsigned short,CGMODDataTable::Entry,unsigned short>::Node_t,unsigned short>,unsigned short>>::InsertRebalance
	CUtlMap<unsigned short, Entry> m_pVarData;
	CUtlMap<unsigned short, Entry> m_pOtherData; // What even uses this? Why does it have two maps? Also I'm like 99% this is wrong since I saw usage of a CUtlDict though the offset seems fine???
	bool m_bFullUpdate;
};

static unsigned short GMODDataTable_NextIndex(bf_read* pDeltaBuffer, int& iCount)
{
	if (iCount > 0) {
		--iCount; // IDA showed --*var
		return pDeltaBuffer->ReadUBitLong(GMODDT_INDEX_BITS);
	} else {
		return GMODDT_INVALID_INDEX;
	}
}

static Detouring::Hook detour_CGMODDataTable_Compare;
static bool hook_CGMODDataTable_Compare(bf_read* pDelta1, bf_read* pDelta2, CGMODDataTable* pOtherTable, int targetTick)
{
	bool changed = false;

	int delta1Count = pDelta1->ReadUBitLong(GMODDT_INDEX_BITS);
	int delta2Count = pDelta2->ReadUBitLong(GMODDT_INDEX_BITS);

	// Only took ages to figure out this order, had assumed for 6+ months it first would have read 1 bit then 12 bits. IDA my love <3

	pDelta1->SeekRelative(1);
	pDelta2->SeekRelative(1);

	int delta2BitsRead = pDelta2->GetNumBitsRead(); // Required since it seeks back?!?
	int delta2InitCount = delta2Count;

	unsigned short delta1Index = GMODDataTable_NextIndex(pDelta1, delta1Count);
	unsigned short delta2Index = GMODDataTable_NextIndex(pDelta1, delta2Count);

	bool fullUpdate = delta2Index == GMODDT_INVALID_INDEX && delta1Index != GMODDT_INVALID_INDEX;
	while (delta2Index != GMODDT_INVALID_INDEX)
	{
		//bool noFullUpdate = !fullUpdate; // Weird but ok? Fk this, probably IDA trolling.
		unsigned short changeIndex = GMODDT_INVALID_INDEX;
		if (!fullUpdate && delta1Index < delta2Index)
		{
			pDelta2->Seek(delta2BitsRead); // Seeking back???
			delta2Index = 0;
			delta2Count = delta2InitCount;
			fullUpdate = true;

			Msg("CGMODataTable_Compare: Triggered full update for %i\n", targetTick);
			continue;
		}

		if (fullUpdate && delta1Index != delta2Index)
		{
			GMODVariant_Skip(pDelta2);
			changeIndex = delta2Index;
		} else {
			if (GMODVariant_Compare(pDelta1, pDelta2))
				changeIndex = delta2Index;

			delta1Index = GMODDataTable_NextIndex(pDelta1, delta1Count);
		}

		if (changeIndex != GMODDT_INVALID_INDEX)
		{
			changed = true;

			if (pOtherTable)
			{
				// Probably checks for the result of find? Idk IDA shows me that its using pDelta2->m_nDataBits for some reason to error, it's probably having a stroke.
				// Error("GMODDataTable: Invalid key in packed entity data (%d)", changeIndex);
				int index = pOtherTable->m_pVarData.Find(changeIndex);
				if (index == pOtherTable->m_pVarData.InvalidIndex()) // Apparently we need this since else when testing we crash instantly.
				{
					pOtherTable->m_pVarData.Element(index).m_iChangeTick = targetTick;
				}
			}
		}

		delta2Index = GMODDataTable_NextIndex(pDelta2, delta2Count);
	}

	while (delta1Index != GMODDT_INVALID_INDEX)
	{
		GMODVariant_Skip(pDelta1);
		delta1Index = GMODDataTable_NextIndex(pDelta1, delta1Count);
	}

	if (explicitEnterPVS && !fullUpdate)
	{
		Msg("No full update when we had a explicit create! %i\n", currentPVSEnterEntity);
	}

	pOtherTable->m_bFullUpdate = fullUpdate;
	return fullUpdate || changed;
}
#endif

//-----------------------------------------------------------------------------
// Returns the pack data for a particular entity for a particular snapshot
//-----------------------------------------------------------------------------
PackedEntity* CFrameSnapshotManager::GetPackedEntity( CFrameSnapshot* pSnapshot, int entity )
{
	if ( !pSnapshot )
		return NULL;

	Assert( entity < pSnapshot->m_nNumEntities );
		
	PackedEntityHandle_t index = pSnapshot->m_pEntities[entity].m_pPackedData;

	if ( index == INVALID_PACKED_ENTITY_HANDLE )
		return NULL;

	PackedEntity *packedEntity = reinterpret_cast< PackedEntity * >( index );
	Assert( packedEntity->m_nEntityIndex == entity );
	return packedEntity;
}

#if SYSTEM_WINDOWS
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

int PackedEntity::GetPropsChangedAfterTick( int iTick, int *iOutProps, int nMaxOutProps )
{
	if ( m_pChangeFrameList )
	{
		return m_pChangeFrameList->GetPropsChangedAfterTick( iTick, iOutProps, nMaxOutProps );
	}
	else
	{
		// signal that we don't have a changelist
		return -1;
	}
}

__declspec(naked) const CSendProxyRecipients* __thiscall PackedEntity::GetRecipients() const
{
    __asm {
        mov eax, [ecx + 0x14]
        ret
    }
}

__declspec(naked) int __thiscall PackedEntity::GetNumRecipients() const
{
    __asm {
        mov eax, [ecx + 0x20]
        ret
    }
}

INetworkStringTable *CBaseServer::GetInstanceBaselineTable( void )
{
	if ( m_pInstanceBaselineTable == NULL )
	{
		m_pInstanceBaselineTable = ((INetworkStringTableContainer*)m_StringTables)->FindTable( INSTANCE_BASELINE_TABLENAME );
	}

	return m_pInstanceBaselineTable;
}

CThreadFastMutex* g_svInstanceBaselineMutex2;
bool CBaseServer::GetClassBaseline( ServerClass *pClass, void const **pData, int *pDatalen )
{
	ConVarRef sv_instancebaselines("sv_instancebaselines");
	if ( sv_instancebaselines.GetInt() )
	{
		ErrorIfNot( pClass->m_InstanceBaselineIndex != INVALID_STRING_INDEX,
			("SV_GetInstanceBaseline: missing instance baseline for class '%s'", pClass->m_pNetworkName)
		);
		
		g_svInstanceBaselineMutex2->Lock();
		*pData = GetInstanceBaselineTable()->GetStringUserData(
			pClass->m_InstanceBaselineIndex,
			pDatalen );
		g_svInstanceBaselineMutex2->Unlock();
		
		return *pData != NULL;
	}
	else
	{
		static char dummy[1] = {0};
		*pData = dummy;
		*pDatalen = 1;
		return true;
	}
}

typedef const char* (__thiscall *CBaseServer_UncompressPackedEntity)(CBaseServer* pServer, PackedEntity *pPackedEntity, int &bits);
static CBaseServer_UncompressPackedEntity func_CBaseServer_UncompressPackedEntity;
const char* CBaseServer::UncompressPackedEntity(PackedEntity *pPackedEntity, int &bits)
{
	return func_CBaseServer_UncompressPackedEntity(g_pBaseServer, pPackedEntity, bits);
}

/*typedef int (*SendTable_WriteAllDeltaPropsT)(const SendTable *pTable, const void *pFromData, const int nFromDataBits, const void *pToData, const int nToDataBits, const int nObjectID, bf_write *pBufOut);
static SendTable_WriteAllDeltaPropsT func_SendTable_WriteAllDeltaProps;
int SendTable_WriteAllDeltaProps(
	const SendTable *pTable,					
	const void *pFromData,
	const int	nFromDataBits,
	const void *pToData,
	const int nToDataBits,
	const int nObjectID,
	bf_write *pBufOut )
{
	return func_SendTable_WriteAllDeltaProps(pTable, pFromData, nFromDataBits, pToData, nToDataBits, nObjectID, pBufOut);
}*/

template <class T, std::ptrdiff_t N>
constexpr std::ptrdiff_t ssize(const T (&)[N]) noexcept {
  return N;
}

// This stack doesn't actually call any proxies. It uses the CSendProxyRecipients to tell
// what can be sent to the specified client.
class CPropCullStack : public CDatatableStack
{
public:
						CPropCullStack( 
							CSendTablePrecalc *pPrecalc, 
							int iClient, 
							const CSendProxyRecipients *pOldStateProxies,
							const int nOldStateProxies, 
							const CSendProxyRecipients *pNewStateProxies,
							const int nNewStateProxies
							) :
							
							CDatatableStack( pPrecalc, (unsigned char*)1, -1 ),
							m_pOldStateProxies( pOldStateProxies ),
							m_nOldStateProxies( nOldStateProxies ),
							m_pNewStateProxies( pNewStateProxies ),
							m_nNewStateProxies( nNewStateProxies )
						{
							m_pPrecalc = pPrecalc;
							m_iClient = iClient;
						}

	inline unsigned char*	CallPropProxy( CSendNode *pNode, int iProp, unsigned char *pStructBase )
	{
		if ( pNode->GetDataTableProxyIndex() == DATATABLE_PROXY_INDEX_NOPROXY )
		{
			return (unsigned char*)1;
		}
		else
		{
			Assert( pNode->GetDataTableProxyIndex() < m_nNewStateProxies );
			bool bCur = m_pNewStateProxies[ pNode->GetDataTableProxyIndex() ].m_Bits.Get( m_iClient ) != 0;

			if ( m_pOldStateProxies )
			{
				Assert( pNode->GetDataTableProxyIndex() < m_nOldStateProxies );
				bool bPrev = m_pOldStateProxies[ pNode->GetDataTableProxyIndex() ].m_Bits.Get( m_iClient ) != 0;
				if ( bPrev != bCur )
				{
					if ( bPrev )
					{
						// Old state had the data and the new state doesn't.
						return 0;
					}
					else
					{
						// Add ALL properties under this proxy because the previous state didn't have any of them.
						for ( int i=0; i < pNode->m_nRecursiveProps; i++ )
						{
							if ( m_nNewProxyProps < ssize( m_NewProxyProps ) )
							{
								m_NewProxyProps[m_nNewProxyProps] = pNode->m_iFirstRecursiveProp + i;
							}
							else
							{
								Error( "CPropCullStack::CallPropProxy - overflowed m_nNewProxyProps" );
							}

							++m_nNewProxyProps;
						}

						// Tell the outer loop that writes to m_pOutProps not to write anything from this
						// proxy since we just did.
						return 0;
					}
				}
			}

			return (unsigned char*)bCur;
		}
	}

	virtual void RecurseAndCallProxies( CSendNode *pNode, unsigned char *pStructBase )
	{
		// Remember where the game code pointed us for this datatable's data so 
		m_pProxies[ pNode->GetRecursiveProxyIndex() ] = pStructBase;

		for ( int iChild=0; iChild < pNode->GetNumChildren(); iChild++ )
		{
			CSendNode *pCurChild = pNode->GetChild( iChild );
			
			unsigned char *pNewStructBase = NULL;
			if ( pStructBase )
			{
				pNewStructBase = CallPropProxy( pCurChild, pCurChild->m_iDatatableProp, pStructBase );
			}

			RecurseAndCallProxies( pCurChild, pNewStructBase );
		}
	}
	
	inline void AddProp( int iProp )
	{
		if ( m_nOutProps < m_nMaxOutProps )
		{
			m_pOutProps[m_nOutProps] = iProp;
		}
		else
		{
			Error( "CPropCullStack::AddProp - m_pOutProps overflowed" );
		}

		++m_nOutProps;
	}


	void CullPropsFromProxies( const int *pStartProps, int nStartProps, int *pOutProps, int nMaxOutProps )
	{
		m_nOutProps = 0;
		m_pOutProps = pOutProps;
		m_nMaxOutProps = nMaxOutProps;
		m_nNewProxyProps = 0;

		Init();

		// This list will have any newly available props written into it. Write a sentinel at the end.
		m_NewProxyProps[m_nNewProxyProps] = -1; // invalid marker
		int *pCurNewProxyProp = m_NewProxyProps;

		for ( int i=0; i < nStartProps; i++ )
		{
			int iProp = pStartProps[i];

			// Fill in the gaps with any properties that are newly enabled by the proxies.
			while ( (unsigned int) *pCurNewProxyProp < (unsigned int) iProp )
			{
				AddProp( *pCurNewProxyProp );
				++pCurNewProxyProp;
			}

			// Now write this property's index if the proxies are allowing this property to be written.
			if ( IsPropProxyValid( iProp ) )
			{
				AddProp( iProp );

				// avoid that we add it twice.
				if ( *pCurNewProxyProp == iProp )
					++pCurNewProxyProp;
			}
		}

		// add any remaining new proxy props
		while ( (unsigned int) *pCurNewProxyProp < MAX_DATATABLE_PROPS )
		{
			AddProp( *pCurNewProxyProp );
			++pCurNewProxyProp;
		}
	}

	int GetNumOutProps()
	{
		return m_nOutProps;
	}


private:
	CSendTablePrecalc		*m_pPrecalc;
	int						m_iClient;	// Which client it's encoding out for.
	const CSendProxyRecipients	*m_pOldStateProxies;
	const int					m_nOldStateProxies;
	
	const CSendProxyRecipients	*m_pNewStateProxies;
	const int					m_nNewStateProxies;

	// The output property list.
	int						*m_pOutProps;
	int						m_nMaxOutProps;
	int						m_nOutProps;

	int m_NewProxyProps[MAX_DATATABLE_PROPS+1];
	int m_nNewProxyProps;
};


int SendTable_CullPropsFromProxies( 
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
	Assert( !( nNewStateProxies && !pNewStateProxies ) );
	CPropCullStack stack( pTable->m_pPrecalc, iClient, pOldStateProxies, nOldStateProxies, pNewStateProxies, nNewStateProxies );
	
	stack.CullPropsFromProxies( pStartProps, nStartProps, pOutProps, nMaxOutProps );

	ErrorIfNot( stack.GetNumOutProps() <= nMaxOutProps, ("CullPropsFromProxies: overflow in '%s'.", pTable->GetName()) );
	return stack.GetNumOutProps();
}

void SendTable_WritePropList(
	const SendTable *pTable,
	const void *pState,
	const int nBits,
	bf_write *pOut,
	const int objectID,
	const int *pCheckProps,
	const int nCheckProps
	)
{
	if ( nCheckProps == 0 )
	{
		// Write single final zero bit, signifying that there no changed properties
		pOut->WriteOneBit( 0 );
		return;
	}

	/*bool bDebugWatch = Sendprop_UsingDebugWatch();

	s_debug_info_shown = false;
	s_debug_bits_start = pOut->GetNumBitsWritten();*/
	
	CSendTablePrecalc *pPrecalc = pTable->m_pPrecalc;
	CDeltaBitsWriter deltaBitsWriter( pOut );

	bf_read inputBuffer( "SendTable_WritePropList->inputBuffer", pState, BitByte( nBits ), nBits );
	CDeltaBitsReader inputBitsReader( &inputBuffer );

	// Ok, they want to specify a small list of properties to check.
	unsigned int iToProp = inputBitsReader.ReadNextPropIndex();
	int i = 0;
	while ( i < nCheckProps )
	{
		// Seek the 'to' state to the current property we want to check.
		while ( iToProp < (unsigned int) pCheckProps[i] )
		{
			inputBitsReader.SkipPropData( pPrecalc->GetProp( iToProp ) );
			iToProp = inputBitsReader.ReadNextPropIndex();
		}

		if ( iToProp >= MAX_DATATABLE_PROPS )
		{
			break;
		}
		
		if ( iToProp == (unsigned int) pCheckProps[i] )
		{
			const SendProp *pProp = pPrecalc->GetProp( iToProp );

			// Show debug stuff.
			/*if ( bDebugWatch )
			{
				ShowEncodeDeltaWatchInfo( pTable, pProp, inputBuffer, objectID, iToProp );
			}*/

			// See how many bits the data for this property takes up.
			int nToStateBits;
			int iStartBit = pOut->GetNumBitsWritten();

			deltaBitsWriter.WritePropIndex( iToProp );
			inputBitsReader.CopyPropData( deltaBitsWriter.GetBitBuf(), pProp ); 

			nToStateBits = pOut->GetNumBitsWritten() - iStartBit;

			TRACE_PACKET( ( "    Send Field (%s) = %d (%d bytes)\n", pProp->GetName(), nToStateBits, ( nToStateBits + 7 ) / 8 ) );

			// Seek to the next prop.
			iToProp = inputBitsReader.ReadNextPropIndex();
		}

		++i;
	}

	/*if ( s_debug_info_shown )
	{
		int  bits = pOut->GetNumBitsWritten() - s_debug_bits_start;
		ConDMsg( "= %i bits (%i bytes)\n", bits, Bits2Bytes(bits) );
	}*/

	inputBitsReader.ForceFinished(); // avoid a benign assert
}

/*typedef int (*SendTable_CalcDeltaT)(const SendTable *pTable, const void *pFromState, const int nFromBits, const void *pToState, const int nToBits, int *pDeltaProps, int nMaxDeltaProps, const int objectID);
static SendTable_CalcDeltaT func_SendTable_CalcDelta;
int SendTable_CalcDelta(
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
	int result;

	__asm {
		mov ebx, pTable
		mov edi, pFromState

		push objectID
		push nMaxDeltaProps
		push pDeltaProps
		push nToBits
		push pToState
		push nFromBits

		call func_SendTable_CalcDelta
		mov result, eax
	}

	return result;
}*/

int SendTable_CalcDelta(
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
	//CServerDTITimer timer( pTable, SERVERDTI_CALCDELTA );

	int *pDeltaPropsBase = pDeltaProps;
	int *pDeltaPropsEnd = pDeltaProps + nMaxDeltaProps;

	VPROF( "SendTable_CalcDelta" );
	
	// Trivial reject.
	//if ( CompareBitArrays( pFromState, pToState, nFromBits, nToBits ) )
	//{
	//	return 0;
	//}

	CSendTablePrecalc* pPrecalc = pTable->m_pPrecalc;

	bf_read toBits( "SendTable_CalcDelta/toBits", pToState, BitByte(nToBits), nToBits );
	CDeltaBitsReader toBitsReader( &toBits );
	unsigned int iToProp = toBitsReader.ReadNextPropIndex();

	if ( pFromState )
	{
		bf_read fromBits( "SendTable_CalcDelta/fromBits", pFromState, BitByte(nFromBits), nFromBits );
		CDeltaBitsReader fromBitsReader( &fromBits );
		unsigned int iFromProp = fromBitsReader.ReadNextPropIndex();

		for ( ; iToProp < MAX_DATATABLE_PROPS; iToProp = toBitsReader.ReadNextPropIndex() )
		{
			// Skip any properties in the from state that aren't in the to state.
			while ( iFromProp < iToProp )
			{
				fromBitsReader.SkipPropData( pPrecalc->GetProp( iFromProp ) );
				iFromProp = fromBitsReader.ReadNextPropIndex();
			}

			if ( iFromProp == iToProp )
			{
				// The property is in both states, so compare them and write the index 
				// if the states are different.
				if ( fromBitsReader.ComparePropData( &toBitsReader, pPrecalc->GetProp( iToProp ) ) )
				{
					*pDeltaProps++ = iToProp;
					if ( pDeltaProps >= pDeltaPropsEnd )
					{
						break;
					}
				}

				// Seek to the next property.
				iFromProp = fromBitsReader.ReadNextPropIndex();
			}
			else
			{
				// Only the 'to' state has this property, so just skip its data and register a change.
				toBitsReader.SkipPropData( pPrecalc->GetProp( iToProp ) );
				*pDeltaProps++ = iToProp;
				if ( pDeltaProps >= pDeltaPropsEnd )
				{
					break;
				}
			}
		}

		Assert( iToProp == ~0u );

		fromBitsReader.ForceFinished();
	}
	else
	{
		for ( ; iToProp != (uint)-1; iToProp = toBitsReader.ReadNextPropIndex() )
		{
			Assert( (int)iToProp >= 0 && iToProp < MAX_DATATABLE_PROPS );

			const SendProp *pProp = pPrecalc->GetProp( iToProp );
			if ( !g_PropTypeFns[pProp->m_Type].IsEncodedZero( pProp, &toBits ) )
			{
				*pDeltaProps++ = iToProp;
				if ( pDeltaProps >= pDeltaPropsEnd )
				{
					break;
				}
			}
		}
	}

	// Return the # of properties that changed between 'from' and 'to'.
	return pDeltaProps - pDeltaPropsBase;
}

int SendTable_WriteAllDeltaProps(
	const SendTable *pTable,					
	const void *pFromData,
	const int	nFromDataBits,
	const void *pToData,
	const int nToDataBits,
	const int nObjectID,
	bf_write *pBufOut )
{
	// Calculate the delta props.
	int deltaProps[MAX_DATATABLE_PROPS];

	int nDeltaProps = SendTable_CalcDelta(
		pTable, 
		pFromData,
		nFromDataBits,
		pToData,
		nToDataBits,
		deltaProps,
		ARRAYSIZE( deltaProps ),
		nObjectID );

	// Write the properties.
	SendTable_WritePropList( 
		pTable,
		pToData,				// object data
		nToDataBits,
		pBufOut,				// output buffer
		nObjectID,
		deltaProps,
		nDeltaProps
	);

	return nDeltaProps;
}

/*
typedef void (GMCOMMON_CALLING_CONVENTION* SendTable_WritePropListT)(const SendTable *pTable,const void *pState,const int nBits,bf_write *pOut,const int objectID,const int *pCheckProps,const int nCheckProps);
static SendTable_WritePropListT func_SendTable_WritePropList;
void SendTable_WritePropList(
	const SendTable *pTable,
	const void *pState,
	const int nBits,
	bf_write *pOut,
	const int objectID,
	const int *pCheckProps,
	const int nCheckProps
	)
{
	func_SendTable_WritePropList(pTable, pState, nBits, pOut, objectID, pCheckProps, nCheckProps);
}

typedef const char* (GMCOMMON_CALLING_CONVENTION* CBaseServer_GetClassBaseline)(CBaseServer* pServer, ServerClass *pClass, void const **pData, int *pDatalen);
static CBaseServer_GetClassBaseline func_CBaseServer_GetClassBaseline;
bool CBaseServer::GetClassBaseline( ServerClass *pClass, void const **pData, int *pDatalen )
{
	return func_CBaseServer_GetClassBaseline(g_pBaseServer, pClass, pData, pDatalen);
}

typedef int (GMCOMMON_CALLING_CONVENTION* SendTable_CullPropsFromProxiesT)(const SendTable *pTable, const int *pStartProps, int nStartProps, const int iClient, const CSendProxyRecipients *pOldStateProxies, const int nOldStateProxies, const CSendProxyRecipients *pNewStateProxies, const int nNewStateProxies, int *pOutProps, int nMaxOutProps);
static SendTable_CullPropsFromProxiesT func_SendTable_CullPropsFromProxies;
int SendTable_CullPropsFromProxies( 
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
	return func_SendTable_CullPropsFromProxies(pTable, pStartProps, nStartProps, iClient, pOldStateProxies, nOldStateProxies, pNewStateProxies, nNewStateProxies, pOutProps, nMaxOutProps);
}
*/
#endif

void CNW2DebuggingModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::FactoryLoader engine_loader("engine");
#if SYSTEM_WINDOWS
	Detour::Create(
		&detour_CBaseServer_WriteDeltaEntities, "CBaseServer::WriteDeltaEntities",
		engine_loader.GetModule(), Symbol::FromSignature("\x55\x8B\xEC\x81\xEC\x84\x04\x00\x00\x53\x56\x8B\xF1"), // 55 8B EC 81 EC 84 04 00 00 53 56 8B F1
		(void*)hook_CBaseServer_WriteDeltaEntities, m_pID
	);

	//func_SendTable_WritePropList = (SendTable_WritePropListT)Detour::GetFunction(engine_loader.GetModule(), Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x40\x83\x7D\x20\x00")); // 55 8B EC 83 EC 40 83 7D 20 00
	//Detour::CheckFunction((void*)func_SendTable_WritePropList, "SendTable_WritePropList");

	func_CBaseServer_UncompressPackedEntity = (CBaseServer_UncompressPackedEntity)Detour::GetFunction(engine_loader.GetModule(), Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x50\x53\x56\x57\x8B\x7D\x08\x89\x4D\xFC")); // 55 8B EC 83 EC 50 53 56 57 8B 7D 08 89 4D FC
	Detour::CheckFunction((void*)func_CBaseServer_UncompressPackedEntity, "CBaseServer::UncompressPackedEntity");

	//func_CBaseServer_GetClassBaseline = (CBaseServer_GetClassBaseline)Detour::GetFunction(engine_loader.GetModule(), Symbol::FromSignature("\x55\x8B\xEC\xA1****\x8B\x50\x48\x57\x8B\xF9\x8D\x48\x48")); // 55 8B EC A1 ?? ?? ?? ?? 8B 50 48 57 8B F9 8D 48 48
	//Detour::CheckFunction((void*)func_CBaseServer_GetClassBaseline, "CBaseServer::GetClassBaseline");

	//func_SendTable_WriteAllDeltaProps = (SendTable_WriteAllDeltaPropsT)Detour::GetFunction(engine_loader.GetModule(), Symbol::FromSignature("\x55\x8B\xEC\xB8\x00\x40\x00\x00*****\x56")); // 55 8B EC B8 00 40 00 00 ?? ?? ?? ?? ?? 56
	//Detour::CheckFunction((void*)func_SendTable_WriteAllDeltaProps, "SendTable_WriteAllDeltaProps");

	//func_SendTable_CullPropsFromProxies = (SendTable_CullPropsFromProxiesT)Detour::GetFunction(engine_loader.GetModule(), Symbol::FromSignature("\x55\x8B\xEC\xB8\x48\x41\x00\x00*****\x53\x56")); // 55 8B EC B8 48 41 00 00 ?? ?? ?? ?? ?? 53 56
	//Detour::CheckFunction((void*)func_SendTable_CullPropsFromProxies, "SendTable_CullPropsFromProxies");

	//func_SendTable_CalcDelta = (SendTable_CalcDeltaT)Detour::GetFunction(engine_loader.GetModule(), Symbol::FromSignature("\x55\x8B\xEC\x83\xEC\x4C*******\x56\x8B\x75\x08\xC7\x45\xEC")); // 55 8B EC 83 EC 4C ?? ?? ?? ?? ?? ?? ?? 56 8B 75 08 C7 45 EC
	//Detour::CheckFunction((void*)func_SendTable_CalcDelta, "SendTable_CalcDelta");

	PropTypeFns* pPropTypeFns = Detour::ResolveSymbol<PropTypeFns>(engine_loader, Symbol::FromSignature("\x2A\x2A\x2A\x2A\x51\xFF\xD0\x8B\x45\x10\x47")); // ?? ?? ?? ?? 51 FF D0 8B 45 10 47 - funcs_10189013 - Go to SendTable_CalcDelta to find the base. Check it twice since else you might get the wrong function so g_PropTypeFns wouldn't be filled properly.
	Detour::CheckValue("get class", "pPropTypeFns", pPropTypeFns != NULL);

	if (pPropTypeFns)
	{
		for (size_t i = 0; i < DPT_NUMSendPropTypes; ++i)
			g_PropTypeFns[i] = pPropTypeFns[i]; // Crash any% speed run. I don't believe this will work
	}

	g_svInstanceBaselineMutex2 = Detour::ResolveSymbol<CThreadFastMutex>(engine_loader, Symbol::FromSignature("\x2A\x2A\x2A\x2A\x3B\x15\x2A\x2A\x2A\x2A\x2A\x2A\x8B\xCA\x33\xC0\xF0\x0F\xB1\x0B\x85\xC0\x2A\x2A\xF3\x90\x6A\x00\x52\x2A\x2A\x2A\x2A\x2A\x2A\x2A\x2A\x2A\x2A\x90\x2A\x2A\x2A\x2A\x2A\x2A\x2A\x2A\x2A\x2A\x2A\x8B\x8F")); // ?? ?? ?? ?? 3B 15 ?? ?? ?? ?? ?? ?? 8B CA 33 C0 F0 0F B1 0B 85 C0 ?? ?? F3 90 6A 00 52 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 90 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 8B 8F Symbol::FromSignature("\x2A\x2A\x2A\x2A\x83\xC4\x04\x8B\x01\xFF\x50\x04\x6A\x40"), // ?? ?? ?? ?? 83 C4 04 8B 01 FF 50 04 6A 40 || "framesnapshotmanager"
	Detour::CheckValue("get class", "g_svInstanceBaselineMutex", g_svInstanceBaselineMutex2 != NULL);
#else
	Detour::Create(
		&detour_CBaseServer_WriteDeltaEntities, "CBaseServer::WriteDeltaEntities",
		engine_loader.GetModule(), Symbol::FromName("_ZN11CBaseServer18WriteDeltaEntitiesEP11CBaseClientP12CClientFrameS3_R8bf_write"),
		(void*)hook_CBaseServer_WriteDeltaEntities, m_pID
	);

	Detour::Create(
		&detour_SV_DetermineUpdateType, "SV_DetermineUpdateType",
		engine_loader.GetModule(), Symbol::FromName("_Z22SV_DetermineUpdateTypeR16CEntityWriteInfo"),
		(void*)hook_SV_DetermineUpdateType, m_pID
	);

	Detour::Create(
		&detour_SV_WriteEnterPVS, "SV_WriteEnterPVS",
		engine_loader.GetModule(), Symbol::FromName("_Z16SV_WriteEnterPVSR16CEntityWriteInfo"),
		(void*)hook_SV_WriteEnterPVS, m_pID
	);

	Detour::Create(
		&detour_CGMODDataTable_Compare, "CGMODDataTable::Compare",
		engine_loader.GetModule(), Symbol::FromName("_ZN14CGMODDataTable7CompareEP7bf_readS1_PS_i"),
		(void*)hook_CGMODDataTable_Compare, m_pID
	);

	Detour::Create(
		&detour_CBaseClient_UpdateAcknowledgedFramecount, "CBaseClient::UpdateAcknowledgedFramecount",
		engine_loader.GetModule(), Symbol::FromName("_ZN11CBaseClient28UpdateAcknowledgedFramecountEi"),
		(void*)hook_CBaseClient_UpdateAcknowledgedFramecount, m_pID
	);
#endif

	// First time I made a symbol for a variable instead of a function :D
	// Finally figured out how it works, only took me months
	CFrameSnapshotManager** fsm = Detour::ResolveSymbol<CFrameSnapshotManager*>(engine_loader, Symbols::g_FrameSnapshotManagerSym);
	Detour::CheckValue("get class", "framesnapshotmanager", fsm != NULL);
	if (fsm)
		framesnapshotmanager = *fsm;

	s_CurrentTable = Detour::ResolveSymbol<CThreadLocalPtr<CGMODDataTable>>(engine_loader, Symbol::FromSignature("****\x6A\x00**\x6A\x00\xB9******\x6A\x00\xB9******\x8B\x43\x18\x8B\x53\x14")); // ?? ?? ?? ?? 6A 00 ?? ?? 6A 00 B9 ?? ?? ?? ?? ?? ?? 6A 00 B9 ?? ?? ?? ?? ?? ?? 8B 43 18 8B 53 14
	Detour::CheckValue("get threadlocal", "s_CurrentTable", s_CurrentTable != NULL);

	s_ReferenceTick = Detour::ResolveSymbol<CThreadLocalInt<intp>>(engine_loader, Symbol::FromSignature("******\x6A\x00\xB9******\x8B\x43\x18\x8B\x53\x14")); // ?? ?? ?? ?? ?? ?? 6A 00 B9 ?? ?? ?? ?? ?? ?? 8B 43 18 8B 53 14
	Detour::CheckValue("get threadlocal", "s_ReferenceTick", s_ReferenceTick != NULL);

	s_TargetTick = Detour::ResolveSymbol<CThreadLocalInt<intp>>(engine_loader, Symbol::FromSignature("******\x8B\x43\x18\x8B\x53\x14")); // ?? ?? ?? ?? ?? ?? 8B 43 18 8B 53 14
	Detour::CheckValue("get threadlocal", "s_TargetTick", s_TargetTick != NULL);
}
#endif