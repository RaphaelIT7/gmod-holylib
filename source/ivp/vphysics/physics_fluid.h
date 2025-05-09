//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICS_FLUID_H
#define PHYSICS_FLUID_H
#pragma once

#include "vphysics_interface.h"

class IVP_Compact_Surface;
class IVP_Environment;
class IVP_Listener_Phantom;
class CBuoyancyAttacher;
class IVP_Liquid_Surface_Descriptor;
class CPhysicsObject;
class CPhysicsObject;

class CPhysicsFluidController : public IPhysicsFluidController
{
public:
	CPhysicsFluidController( CBuoyancyAttacher *pBuoy, IVP_Liquid_Surface_Descriptor *pLiquid, CPhysicsObject *pObject, int nContents );
	~CPhysicsFluidController( void );

	void SetGameData( void *pGameData ) override;
	void *GetGameData( void ) const override;
	void GetSurfacePlane( Vector *pNormal, float *pDist ) const override;
	[[nodiscard]] float GetDensity() const override;
	void WakeAllSleepingObjects() override; 
	[[nodiscard]] int	GetContents() const override;

	[[nodiscard]] class IVP_Real_Object *GetIVPObject();
	[[nodiscard]] const class IVP_Real_Object *GetIVPObject() const;

private:
	CBuoyancyAttacher				*m_pBuoyancy;
	IVP_Liquid_Surface_Descriptor	*m_pLiquidSurface;
	CPhysicsObject					*m_pObject;
	int								m_nContents;
	void							*m_pGameData;
};

extern [[nodiscard]] CPhysicsFluidController *CreateFluidController( IVP_Environment *pEnvironment, CPhysicsObject *pFluidObject, fluidparams_t *pParams );


#endif // PHYSICS_FLUID_H
