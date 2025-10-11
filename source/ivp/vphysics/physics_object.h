//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICS_OBJECT_H
#define PHYSICS_OBJECT_H

#ifdef _WIN32
#pragma once
#endif

#include "vphysics_interface.h"

class IPredictedPhysicsObject;
class IVP_Real_Object;
class IVP_Environment;
class IVP_U_Float_Point;
class IVP_SurfaceManager;
class IVP_Controller;
class CPhysicsEnvironment;
struct vphysics_save_cphysicsobject_t
{
	const CPhysCollide *pCollide;
	const char *pName;
	float	sphereRadius;

	bool	isStatic;
	bool	collisionEnabled;
	bool	gravityEnabled;
	bool	dragEnabled;
	bool	motionEnabled;
	bool	isAsleep;
	bool	isTrigger;
	bool	asleepSinceCreation;		// has this been asleep since creation?
	bool	hasTouchedDynamic;
	bool	hasShadowController;
	short	collideType;
	unsigned short	gameIndex;
	int		hingeAxis;
	int		materialIndex;
	float	mass;
	Vector	rotInertia;
	float	speedDamping;
	float	rotSpeedDamping;
	Vector	massCenterOverride;

	unsigned int	callbacks;
	unsigned int	gameFlags;

	unsigned int	contentsMask;

	float			volume;
	float			dragCoefficient;
	float			angDragCoefficient;
	IPhysicsShadowController	*pShadow;
	//bool			m_shadowTempGravityDisable;

	Vector			origin;
	QAngle			angles;
	Vector			velocity;
	AngularImpulse	angVelocity;

	DECLARE_SIMPLE_DATADESC();
};

enum
{
	OBJ_AWAKE = 0,			// awake, simulating
	OBJ_STARTSLEEP = 1,		// going to sleep, but not queried yet
	OBJ_SLEEP = 2,			// sleeping, no state changes since last query
};


class CPhysicsObject : public IPhysicsObject
{
public:
	CPhysicsObject( void );
	virtual ~CPhysicsObject( void );

	void			Init( const CPhysCollide *pCollisionModel, IVP_Real_Object *pObject, int materialIndex, float volume, float drag, float angDrag );

	// IPhysicsObject functions
	[[nodiscard]] bool			IsStatic() const override;
	[[nodiscard]] bool			IsAsleep() const override;
	[[nodiscard]] bool			IsTrigger() const override;
	[[nodiscard]] bool			IsFluid() const override;
	[[nodiscard]] bool			IsHinged() const override { return (m_hingedAxis != 0) ? true : false; }
	[[nodiscard]] bool			IsCollisionEnabled() const override;
	[[nodiscard]] bool			IsGravityEnabled() const override;
	[[nodiscard]] bool			IsDragEnabled() const override;
	[[nodiscard]] bool			IsMotionEnabled() const override;
	[[nodiscard]] bool			IsMoveable() const override;
	[[nodiscard]] bool			IsAttachedToConstraint( bool bExternalOnly ) const override;


	void			EnableCollisions( bool enable ) override;
	// Enable / disable gravity for this object
	void			EnableGravity( bool enable ) override;
	// Enable / disable air friction / drag for this object
	void			EnableDrag( bool enable ) override;
	void			EnableMotion( bool enable ) override;

	void			SetGameData( void *pAppData ) override;
	void			*GetGameData( void ) const override;
	void			SetCallbackFlags( unsigned short callbackflags ) override;
	[[nodiscard]] unsigned short	GetCallbackFlags( void ) const override;
	void			SetGameFlags( unsigned short userFlags ) override;
	[[nodiscard]] unsigned short	GetGameFlags( void ) const override;
	void			SetGameIndex( unsigned short gameIndex ) override;
	[[nodiscard]] unsigned short	GetGameIndex( void ) const override;

	void			Wake() override;
	void			WakeNow();
	void			Sleep() override;
	void			RecheckCollisionFilter() override;
#if !PLATFORM_64BITS
	void			RecheckContactPoints() override;
#else
	void			RecheckContactPoints( bool bSearchForNewContacts = false ) override;
#endif

	void			SetMass( float mass ) override;
	[[nodiscard]] float			GetMass( void ) const override;
	[[nodiscard]] float			GetInvMass( void ) const override;
	void			SetInertia( const Vector &inertia ) override;
	[[nodiscard]] Vector			GetInertia( void ) const override;
	[[nodiscard]] Vector			GetInvInertia( void ) const override;

	void			GetDamping( float *speed, float *rot ) const override;
	void			SetDamping( const float *speed, const float *rot ) override;
	void			SetDragCoefficient( float *pDrag, float *pAngularDrag ) override;
	void			SetBuoyancyRatio( float ratio ) override;
	[[nodiscard]] int				GetMaterialIndex() const override { return GetMaterialIndexInternal(); }
	void			SetMaterialIndex( int materialIndex ) override;
	[[nodiscard]] inline int		GetMaterialIndexInternal( void ) const { return m_materialIndex; }

	[[nodiscard]] unsigned int	GetContents() const override { return m_contentsMask; }
	void			SetContents( unsigned int contents ) override;

	[[nodiscard]] float			GetSphereRadius() const override;
	Vector			GetMassCenterLocalSpace() const override;
	[[nodiscard]] float			GetEnergy() const override;

	void			SetPosition( const Vector &worldPosition, const QAngle &angles, bool isTeleport = false ) override;
	void			SetPositionMatrix( const matrix3x4_t& matrix, bool isTeleport = false ) override;
	void			GetPosition( Vector *worldPosition, QAngle *angles ) const override;
	void			GetPositionMatrix( matrix3x4_t *positionMatrix ) const override;

	void			SetVelocity( const Vector *velocity, const AngularImpulse *angularVelocity ) override;
	void			SetVelocityInstantaneous( const Vector *velocity, const AngularImpulse *angularVelocity ) override;
	void			AddVelocity( const Vector *velocity, const AngularImpulse *angularVelocity ) override;
	void			GetVelocity( Vector *velocity, AngularImpulse *angularVelocity ) const override;
	void			GetImplicitVelocity( Vector *velocity, AngularImpulse *angularVelocity ) const override;
	void			GetVelocityAtPoint( const Vector &worldPosition, Vector *pVelocity ) const override;

	void			LocalToWorld( Vector *worldPosition, const Vector &localPosition ) const override;
	void			WorldToLocal( Vector *localPosition, const Vector &worldPosition ) const override;
	void			LocalToWorldVector( Vector *worldVector, const Vector &localVector ) const override;
	void			WorldToLocalVector( Vector *localVector, const Vector &worldVector ) const override;

	void			ApplyForceCenter( const Vector &forceVector ) override;
	void			ApplyForceOffset( const Vector &forceVector, const Vector &worldPosition ) override;
	void			ApplyTorqueCenter( const AngularImpulse & ) override;
	void			CalculateForceOffset( const Vector &forceVector, const Vector &worldPosition, Vector *centerForce, AngularImpulse *centerTorque ) const override;
	void			CalculateVelocityOffset( const Vector &forceVector, const Vector &worldPosition, Vector *centerVelocity, AngularImpulse *centerAngularVelocity ) const override;
	[[nodiscard]] float			CalculateLinearDrag( const Vector &unitDirection ) const override;
	[[nodiscard]] float			CalculateAngularDrag( const Vector &objectSpaceRotationAxis ) const override;

	[[nodiscard]] bool			GetContactPoint( Vector *contactPoint, IPhysicsObject **contactObject ) const override;
	void			SetShadow( float maxSpeed, float maxAngularSpeed, bool allowPhysicsMovement, bool allowPhysicsRotation ) override;
	void			UpdateShadow( const Vector &targetPosition, const QAngle &targetAngles, bool tempDisableGravity, float timeOffset ) override;
	void			RemoveShadowController() override;
	[[nodiscard]] int			GetShadowPosition( Vector *position, QAngle *angles ) const override;
	[[nodiscard]] IPhysicsShadowController *GetShadowController( void ) const override;
	[[nodiscard]] float			ComputeShadowControl( const hlshadowcontrol_params_t &params, float secondsToArrival, float dt ) override;

	[[nodiscard]] const CPhysCollide	*GetCollide( void ) const override;
	[[nodiscard]] char const		*GetName() const override;

	[[nodiscard]] float			GetDragInDirection( const IVP_U_Float_Point &dir ) const;
	[[nodiscard]] float			GetAngularDragInDirection( const IVP_U_Float_Point &angVelocity ) const;
	void			BecomeTrigger() override;
	void			RemoveTrigger() override;
	void			BecomeHinged( int localAxis ) override;
	void			RemoveHinged() override;

	[[nodiscard]] IPhysicsFrictionSnapshot *CreateFrictionSnapshot() override;
	void			DestroyFrictionSnapshot( IPhysicsFrictionSnapshot *pSnapshot ) override;

	void			OutputDebugInfo() const override;

#if !PLATFORM_64BITS
	float			GetBuoyancyRatio() const override { return m_buoyancyRatio; };
#endif

	// local functions
	[[nodiscard]] inline	IVP_Real_Object *GetObject( void ) const { return m_pObject; }
	[[nodiscard]] inline int		CallbackFlags( void ) const { return m_callbacks; }
	inline void		AddCallbackFlags( unsigned short flags ) { m_callbacks |= flags; }
	inline void		RemoveCallbackFlags( unsigned short flags ) { m_callbacks &= ~flags; }
	[[nodiscard]] inline bool		HasTouchedDynamic();
	inline void		SetTouchedDynamic();
	void			NotifySleep( void );
	void			NotifyWake( void );
	[[nodiscard]] int				GetSleepState( void ) const { return m_sleepState; }
	inline void		ForceSilentDelete() { m_forceSilentDelete = true; }

	[[nodiscard]] inline int		GetActiveIndex( void ) const { return m_activeIndex; }
	inline void		SetActiveIndex( int index ) { m_activeIndex = index; }
#if PLATFORM_64BITS
	[[nodiscard]] inline float	GetBuoyancyRatio( void ) const { return m_buoyancyRatio; }
#endif
	// returns true if the mass center is set to the default for the collision model
	[[nodiscard]] bool			IsMassCenterAtDefault() const;

	// is this object simulated, or controlled by game logic?
	[[nodiscard]] bool			IsControlledByGame() const;

	[[nodiscard]] IVP_SurfaceManager *GetSurfaceManager( void ) const;

	void			WriteToTemplate( vphysics_save_cphysicsobject_t &objectTemplate );
	void			InitFromTemplate( CPhysicsEnvironment *pEnvironment, void *pGameData, const vphysics_save_cphysicsobject_t &objectTemplate );

	[[nodiscard]] CPhysicsEnvironment	*GetVPhysicsEnvironment();
	[[nodiscard]] const CPhysicsEnvironment	*GetVPhysicsEnvironment() const;

#if PLATFORM_64BITS
	void			SetSphereRadius(float radius) override;

	// EnableGravity still determines whether to apply gravity
	// This flag determines which gravity constant to use for an alternate gravity effect
	void			SetUseAlternateGravity( bool bSet ) override;
	void			SetCollisionHints( uint32 collisionHints ) override;
	uint32			GetCollisionHints() const override;

	IPredictedPhysicsObject *GetPredictedInterface( void ) const override;
	void			SyncWith( IPhysicsObject *pOther ) override;
#endif

private:
	// NOTE: Local to vphysics, used to save/restore shadow controller
	void			RestoreShadowController( IPhysicsShadowController *pShadowController );
	friend bool		RestorePhysicsObject( const physrestoreparams_t &params, CPhysicsObject **ppObject );

	[[nodiscard]] bool			IsControlling( const IVP_Controller *pController ) const;
	[[nodiscard]] float			GetVolume() const;
	void			SetVolume( float volume );
	
	// the mass has changed, recompute the drag information
	void			RecomputeDragBases();

	void			ClampVelocity();

	void			*m_pGameData;
	IVP_Real_Object	*m_pObject;
	const CPhysCollide *m_pCollide;
	IPhysicsShadowController	*m_pShadow;

	Vector			m_dragBasis;
	Vector			m_angDragBasis;

	// these 5 should pack into a short
	// pack new bools here
	bool			m_shadowTempGravityDisable : 5;
	bool			m_hasTouchedDynamic : 1;
	bool			m_asleepSinceCreation : 1;
	bool			m_forceSilentDelete : 1;
	unsigned char	m_sleepState : 2;
	unsigned char	m_hingedAxis : 3;
	unsigned char	m_collideType : 3;
	unsigned short	m_gameIndex;

private:
	// dimhotepus: unsigned short -> int
	int	m_activeIndex;
	// dimhotepus: unsigned short -> int
	int		m_materialIndex;

	unsigned short	m_callbacks;
	unsigned short	m_gameFlags;
	unsigned int	m_contentsMask;
	
	float			m_volume;
	float			m_buoyancyRatio;
	float			m_dragCoefficient;
	float			m_angDragCoefficient;

	uint32 m_collisionHints = 0;

	friend CPhysicsObject *CreatePhysicsObject( CPhysicsEnvironment *pEnvironment, const CPhysCollide *pCollisionModel, int materialIndex, const Vector &position, const QAngle& angles, objectparams_t *pParams, bool isStatic );
	friend bool CPhysicsEnvironment::TransferObject( IPhysicsObject *pObject, IPhysicsEnvironment *pDestinationEnvironment ); //need direct access to m_pShadow for Portal mod's physics object transfer system
};

// If you haven't ever touched a dynamic object, there's no need to search for contacting objects to 
// wakeup when you are deleted.  So cache a bit here when contacts are generated
inline bool CPhysicsObject::HasTouchedDynamic()
{
	return m_hasTouchedDynamic;
}

inline void CPhysicsObject::SetTouchedDynamic()
{
	m_hasTouchedDynamic = true;
}

[[nodiscard]] extern CPhysicsObject *CreatePhysicsObject( CPhysicsEnvironment *pEnvironment, const CPhysCollide *pCollisionModel, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams, bool isStatic );
[[nodiscard]] extern CPhysicsObject *CreatePhysicsSphere( CPhysicsEnvironment *pEnvironment, float radius, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams, bool isStatic );
extern void PostRestorePhysicsObject();
[[nodiscard]] extern IPhysicsObject *CreateObjectFromBuffer( CPhysicsEnvironment *pEnvironment, void *pGameData, unsigned char *pBuffer, unsigned int bufferSize, bool enableCollisions );
extern IPhysicsObject *CreateObjectFromBuffer_UseExistingMemory( CPhysicsEnvironment *pEnvironment, void *pGameData, unsigned char *pBuffer, unsigned int bufferSize, CPhysicsObject *pExistingMemory );

#endif // PHYSICS_OBJECT_H
