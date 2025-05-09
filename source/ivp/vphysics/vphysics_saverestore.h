//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef VPHYSICS_SAVERESTORE_H
#define VPHYSICS_SAVERESTORE_H

#if defined( _WIN32 )
#pragma once
#endif


#include "datamap.h"
#include "tier1/utlmap.h"
#include "isaverestore.h"
#include "tier1/utlvector.h"


//-------------------------------------

class ISave;
class IRestore;

class CPhysicsObject;
class CPhysicsFluidController;
class CPhysicsSpring;
class CPhysicsConstraint;
class CPhysicsConstraintGroup;
class CShadowController;
class CPlayerController;
class CPhysicsMotionController;
class CVehicleController;
struct physsaveparams_t;
struct physrestoreparams_t;
class ISaveRestoreOps;

//-----------------------------------------------------------------------------
// Purpose: Fixes up pointers beteween vphysics objects
//-----------------------------------------------------------------------------

class CVPhysPtrSaveRestoreOps : public CDefSaveRestoreOps
{
public:
	CVPhysPtrSaveRestoreOps();
	void Save( const SaveRestoreFieldInfo_t &fieldInfo, ISave *pSave ) override;
	void PreRestore();
	void PostRestore();
	void Restore( const SaveRestoreFieldInfo_t &fieldInfo, IRestore *pRestore ) override;
};

extern CVPhysPtrSaveRestoreOps g_VPhysPtrSaveRestoreOps;

#define DEFINE_VPHYSPTR(name) \
	{ FIELD_CUSTOM, #name, { offsetof(classNameTypedef,name), 0 }, 1, FTYPEDESC_SAVE, nullptr, &g_VPhysPtrSaveRestoreOps, nullptr, nullptr, 0, nullptr, 0, 0.0f }

#define DEFINE_VPHYSPTR_ARRAY(name,count) \
	{ FIELD_CUSTOM, #name, { offsetof(classNameTypedef,name), 0 }, count, FTYPEDESC_SAVE, nullptr, &g_VPhysPtrSaveRestoreOps, nullptr, nullptr, 0, nullptr, 0, 0.0f }


//-----------------------------------------------------------------------------

class CVPhysPtrUtlVectorSaveRestoreOps : public CVPhysPtrSaveRestoreOps
{
public:
	void Save( const SaveRestoreFieldInfo_t &fieldInfo, ISave *pSave ) override;
	void Restore( const SaveRestoreFieldInfo_t &fieldInfo, IRestore *pRestore ) override;

private:
	typedef CUtlVector<intp> VPhysPtrVector;
};

extern CVPhysPtrUtlVectorSaveRestoreOps g_VPhysPtrUtlVectorSaveRestoreOps;

#define DEFINE_VPHYSPTR_UTLVECTOR(name) \
	{ FIELD_CUSTOM, #name, { offsetof(classNameTypedef,name), 0 }, 1, FTYPEDESC_SAVE, nullptr, &g_VPhysPtrUtlVectorSaveRestoreOps, nullptr, nullptr, 0, nullptr, 0, 0.0f }


//-----------------------------------------------------------------------------

typedef bool (*PhysSaveFunc_t)( const physsaveparams_t &params, void *pCastedObject ); // second parameter for convenience
typedef bool (*PhysRestoreFunc_t)( const physrestoreparams_t &params, void **ppCastedObject );

[[nodiscard]] bool SavePhysicsObject( const physsaveparams_t &params, CPhysicsObject *pObject );
[[nodiscard]] bool RestorePhysicsObject( const physrestoreparams_t &params, CPhysicsObject **ppObject );

[[nodiscard]] bool SavePhysicsFluidController( const physsaveparams_t &params, CPhysicsFluidController *pFluidObject );
[[nodiscard]] bool RestorePhysicsFluidController( const physrestoreparams_t &params, CPhysicsFluidController **ppFluidObject );

[[nodiscard]] bool SavePhysicsSpring( const physsaveparams_t &params, CPhysicsSpring *pSpring );
[[nodiscard]] bool RestorePhysicsSpring( const physrestoreparams_t &params, CPhysicsSpring **ppSpring );

[[nodiscard]] bool SavePhysicsConstraint( const physsaveparams_t &params, CPhysicsConstraint *pConstraint );
[[nodiscard]] bool RestorePhysicsConstraint( const physrestoreparams_t &params, CPhysicsConstraint **ppConstraint );

[[nodiscard]] bool SavePhysicsConstraintGroup( const physsaveparams_t &params, CPhysicsConstraintGroup *pConstraintGroup );
[[nodiscard]] bool RestorePhysicsConstraintGroup( const physrestoreparams_t &params, CPhysicsConstraintGroup **ppConstraintGroup );
void PostRestorePhysicsConstraintGroup();

[[nodiscard]] bool SavePhysicsShadowController( const physsaveparams_t &params, IPhysicsShadowController *pShadowController );
[[nodiscard]] bool RestorePhysicsShadowController( const physrestoreparams_t &params, IPhysicsShadowController **ppShadowController );
[[nodiscard]] bool RestorePhysicsShadowControllerInternal( const physrestoreparams_t &params, IPhysicsShadowController **ppShadowController, CPhysicsObject *pObject );

[[nodiscard]] bool SavePhysicsPlayerController( const physsaveparams_t &params, CPlayerController *pPlayerController );
[[nodiscard]] bool RestorePhysicsPlayerController( const physrestoreparams_t &params, CPlayerController **ppPlayerController );

[[nodiscard]] bool SavePhysicsMotionController( const physsaveparams_t &params, IPhysicsMotionController *pMotionController );
[[nodiscard]] bool RestorePhysicsMotionController( const physrestoreparams_t &params, IPhysicsMotionController **ppMotionController );

[[nodiscard]] bool SavePhysicsVehicleController( const physsaveparams_t &params, CVehicleController *pVehicleController );
[[nodiscard]] bool RestorePhysicsVehicleController( const physrestoreparams_t &params, CVehicleController **ppVehicleController );

//-----------------------------------------------------------------------------

[[nodiscard]] ISaveRestoreOps* MaterialIndexDataOps();

#endif // VPHYSICS_SAVERESTORE_H
