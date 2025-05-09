//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICS_WORLD_H
#define PHYSICS_WORLD_H
#pragma once

#include "vphysics_interface.h"
#include "ivu_types.hxx"
#include "tier1/utlvector.h"

class IVP_Environment;
class CSleepObjects;
class CPhysicsListenerCollision;
class CPhysicsListenerConstraint;
class IVP_Listener_Collision;
class IVP_Listener_Constraint;
class IVP_Listener_Object;
class IVP_Controller;
class CPhysicsFluidController;
class CCollisionSolver;
class CPhysicsObject;
class CDeleteQueue;
class IVPhysicsDebugOverlay;
struct constraint_limitedhingeparams_t;
struct vphysics_save_iphysicsobject_t;

class CPhysicsEnvironment : public IPhysicsEnvironment
{
public:
	CPhysicsEnvironment( void );
	~CPhysicsEnvironment( void );

	void SetDebugOverlay( CreateInterfaceFn debugOverlayFactory ) override;
	[[nodiscard]] IVPhysicsDebugOverlay *GetDebugOverlay( void ) override;

	void			SetGravity( const Vector& gravityVector ) override;
	[[nodiscard]] IPhysicsObject	*CreatePolyObject( const CPhysCollide *pCollisionModel, intp materialIndex, const Vector& position, const QAngle& angles, objectparams_t *pParams ) override;
	[[nodiscard]] IPhysicsObject	*CreatePolyObjectStatic( const CPhysCollide *pCollisionModel, intp materialIndex, const Vector& position, const QAngle& angles, objectparams_t *pParams ) override;
	[[nodiscard]] unsigned int	GetObjectSerializeSize( IPhysicsObject *pObject ) const override;
	void			SerializeObjectToBuffer( IPhysicsObject *pObject, unsigned char *pBuffer, unsigned int bufferSize ) override;
	[[nodiscard]] IPhysicsObject *UnserializeObjectFromBuffer( void *pGameData, unsigned char *pBuffer, unsigned int bufferSize, bool enableCollisions ) override;
	


	[[nodiscard]] IPhysicsSpring	*CreateSpring( IPhysicsObject *pObjectStart, IPhysicsObject *pObjectEnd, springparams_t *pParams ) override;
	[[nodiscard]] IPhysicsFluidController	*CreateFluidController( IPhysicsObject *pFluidObject, fluidparams_t *pParams ) override;
	[[nodiscard]] IPhysicsConstraint *CreateRagdollConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ragdollparams_t &ragdoll ) override;

	[[nodiscard]] IPhysicsConstraint *CreateHingeConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_hingeparams_t &hinge ) override;
	virtual [[nodiscard]] IPhysicsConstraint *CreateLimitedHingeConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_limitedhingeparams_t &hinge );
	[[nodiscard]] IPhysicsConstraint *CreateFixedConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_fixedparams_t &fixed ) override;
	[[nodiscard]] IPhysicsConstraint *CreateSlidingConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_slidingparams_t &sliding ) override;
	[[nodiscard]] IPhysicsConstraint *CreateBallsocketConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ballsocketparams_t &ballsocket ) override;
	[[nodiscard]] IPhysicsConstraint *CreatePulleyConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_pulleyparams_t &pulley ) override;
	[[nodiscard]] IPhysicsConstraint *CreateLengthConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_lengthparams_t &length ) override;

	[[nodiscard]] IPhysicsConstraintGroup *CreateConstraintGroup( const constraint_groupparams_t &group ) override;
	void DestroyConstraintGroup( IPhysicsConstraintGroup *pGroup ) override;

	void			Simulate( float deltaTime ) override;
	[[nodiscard]] float			GetSimulationTimestep() const override;
	void			SetSimulationTimestep( float timestep ) override;
	[[nodiscard]] float			GetSimulationTime() const override;
	[[nodiscard]] float			GetNextFrameTime() const override;
	bool			IsInSimulation() const override;

	void DestroyObject( IPhysicsObject * ) override;
	void DestroySpring( IPhysicsSpring * ) override;
	void DestroyFluidController( IPhysicsFluidController * ) override;
	void DestroyConstraint( IPhysicsConstraint * ) override;

	void SetCollisionEventHandler( IPhysicsCollisionEvent *pCollisionEvents ) override;
	void SetObjectEventHandler( IPhysicsObjectEvent *pObjectEvents ) override;
	void SetConstraintEventHandler( IPhysicsConstraintEvent *pConstraintEvents ) override;

	[[nodiscard]] IPhysicsShadowController *CreateShadowController( IPhysicsObject *pObject, bool allowTranslation, bool allowRotation ) override;
	void DestroyShadowController( IPhysicsShadowController * ) override;
	[[nodiscard]] IPhysicsMotionController *CreateMotionController( IMotionEvent *pHandler ) override;
	void DestroyMotionController( IPhysicsMotionController *pController ) override;
	[[nodiscard]] IPhysicsPlayerController *CreatePlayerController( IPhysicsObject *pObject ) override;
	void DestroyPlayerController( IPhysicsPlayerController *pController ) override;
	[[nodiscard]] IPhysicsVehicleController *CreateVehicleController( IPhysicsObject *pVehicleBodyObject, const vehicleparams_t &params, unsigned int nVehicleType, IPhysicsGameTrace *pGameTrace ) override;
	void DestroyVehicleController( IPhysicsVehicleController *pController ) override;

	void SetQuickDelete( bool bQuick ) override
	{
		m_deleteQuick = bQuick;
	}
	[[nodiscard]] virtual bool ShouldQuickDelete() const { return m_deleteQuick; }
	virtual void TraceBox( trace_t *ptr, const Vector &mins, const Vector &maxs, const Vector &start, const Vector &end );
	void SetCollisionSolver( IPhysicsCollisionSolver *pCollisionSolver ) override;
	void GetGravity( Vector *pGravityVector ) const override;
	[[nodiscard]] intp GetActiveObjectCount() const override;
	void GetActiveObjects( IPhysicsObject **pOutputObjectList ) const override;
	[[nodiscard]] const IPhysicsObject **GetObjectList( intp *pOutputObjectCount ) const override;
	[[nodiscard]] bool TransferObject( IPhysicsObject *pObject, IPhysicsEnvironment *pDestinationEnvironment ) override;

	[[nodiscard]] IVP_Environment	*GetIVPEnvironment( void ) { return m_pPhysEnv; }
	void		ClearDeadObjects( void );
	[[nodiscard]] IVP_Controller *GetDragController() { return m_pDragController; }
	[[nodiscard]] const IVP_Controller *GetDragController() const { return m_pDragController; }
	void SetAirDensity( float density ) override;
	[[nodiscard]] float GetAirDensity( void ) const override;
	void ResetSimulationClock( void ) override;
	[[nodiscard]] IPhysicsObject *CreateSphereObject( float radius, intp materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams, bool isStatic ) override;
	void CleanupDeleteList() override;
	void EnableDeleteQueue( bool enable ) override { m_queueDeleteObject = enable; }
	// debug
	[[nodiscard]] bool IsCollisionModelUsed( CPhysCollide *pCollide ) const override;

	// trace against the physics world
	void TraceRay( const Ray_t &ray, unsigned int fMask, IPhysicsTraceFilter *pTraceFilter, trace_t *pTrace ) override;
	void SweepCollideable( const CPhysCollide *pCollide, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
		const QAngle &vecAngles, unsigned int fMask, IPhysicsTraceFilter *pTraceFilter, trace_t *pTrace ) override;

	// performance tuning
	void GetPerformanceSettings( physics_performanceparams_t *pOutput ) const override;
	void SetPerformanceSettings( const physics_performanceparams_t *pSettings ) override;

	// perf/cost statistics
	void ReadStats( physics_stats_t *pOutput ) override;
	void ClearStats() override;
	void EnableConstraintNotify( bool bEnable ) override;
	// debug
	void DebugCheckContacts(void) override;

	// Save/restore
	[[nodiscard]] bool Save( const physsaveparams_t &params  ) override;
	void PreRestore( const physprerestoreparams_t &params ) override;
	[[nodiscard]] bool Restore( const physrestoreparams_t &params ) override;
	void PostRestore() override;
	void PhantomAdd( CPhysicsObject *pObject );
	void PhantomRemove( CPhysicsObject *pObject );

	void AddPlayerController( IPhysicsPlayerController *pController );
	void RemovePlayerController( IPhysicsPlayerController *pController );
	[[nodiscard]] IPhysicsPlayerController *FindPlayerController( IPhysicsObject *pObject );

	[[nodiscard]] IPhysicsCollisionEvent *GetCollisionEventHandler();
	// a constraint is being disabled - report the game DLL as "broken"
	void NotifyConstraintDisabled( IPhysicsConstraint *pConstraint );

private:
	IVP_Environment					*m_pPhysEnv;
	IVP_Controller					*m_pDragController;
	IVPhysicsDebugOverlay			*m_pDebugOverlay;			// Interface to use for drawing debug overlays.
	CUtlVector<IPhysicsObject *>	m_objects;
	CUtlVector<IPhysicsObject *>	m_deadObjects;
	CUtlVector<CPhysicsFluidController *> m_fluids;
	CUtlVector<IPhysicsPlayerController *> m_playerControllers;
	CSleepObjects					*m_pSleepEvents;
	CPhysicsListenerCollision		*m_pCollisionListener;
	CCollisionSolver				*m_pCollisionSolver;
	CPhysicsListenerConstraint		*m_pConstraintListener;
	CDeleteQueue					*m_pDeleteQueue;
	intp							m_lastObjectThisTick;
	bool							m_deleteQuick;
	bool							m_inSimulation;
	bool							m_queueDeleteObject;
	bool							m_fixedTimestep;
	bool							m_enableConstraintNotify;
};

extern [[nodiscard]] IPhysicsEnvironment *CreatePhysicsEnvironment( void );

class IVP_Synapse_Friction;
class IVP_Real_Object;
extern [[nodiscard]] IVP_Real_Object *GetOppositeSynapseObject( IVP_Synapse_Friction *pfriction );
extern [[nodiscard]] IPhysicsObjectPairHash *CreateObjectPairHash();

#endif // PHYSICS_WORLD_H
