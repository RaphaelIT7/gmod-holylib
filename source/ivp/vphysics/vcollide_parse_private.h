//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef VCOLLIDE_PARSE_PRIVATE_H
#define VCOLLIDE_PARSE_PRIVATE_H
#ifdef _WIN32
#pragma once
#endif

#include "vcollide_parse.h"

#define MAX_KEYVALUE	1024

class IVPhysicsKeyParser;
class CPackedPhysicsDescription;

[[nodiscard]] const char			*ParseKeyvalue( const char *pBuffer, OUT_Z_ARRAY char (&key)[MAX_KEYVALUE], OUT_Z_ARRAY char (&value)[MAX_KEYVALUE] );
[[nodiscard]] IVPhysicsKeyParser	*CreateVPhysicsKeyParser( const char *pKeyData, bool bIsPacked = false );
void				DestroyVPhysicsKeyParser( IVPhysicsKeyParser * );
[[nodiscard]] const char			*PackVCollideText( IPhysicsCollision *physcollision, const char *pTextIn, int *pSizeOut, bool storeSolidNames, bool storeSurfacepropsAsNames );
[[nodiscard]] CPackedPhysicsDescription *CreatePackedDescription( const char *pPackedBuffer, int packedSize );
void				DestroyPackedDescription( CPackedPhysicsDescription *pPhysics );

#endif // VCOLLIDE_PARSE_PRIVATE_H
