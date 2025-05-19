//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICS_MATERIAL_H
#define PHYSICS_MATERIAL_H
#pragma once

#include "vphysics_interface.h"
class IVP_Material;
class IVP_Material_Manager;

class IPhysicsSurfacePropsInternal : public IPhysicsSurfaceProps
{
public:
	[[nodiscard]] virtual IVP_Material *GetIVPMaterial( int materialIndex ) = 0;

	[[nodiscard]] virtual int GetIVPMaterialIndex( const IVP_Material *pIVP ) const = 0;
	[[nodiscard]] virtual IVP_Material_Manager *GetIVPManager( void ) = 0;
	[[nodiscard]] virtual int RemapIVPMaterialIndex( int ivpMaterialIndex ) const = 0;
	[[nodiscard]] virtual const char *GetReservedMaterialName( int nMaterialIndex ) const = 0;
};

extern IPhysicsSurfacePropsInternal	*physprops;

// Special material indices outside of the normal system
enum
{
	MATERIAL_INDEX_SHADOW = 0xF000,
};

#endif // PHYSICS_MATERIAL_H
