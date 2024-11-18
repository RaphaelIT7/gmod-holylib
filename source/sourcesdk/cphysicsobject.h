#include <vphysics_interface.h>
#include <ivp_types.h>

class IVP_Real_Object;
class IVP_U_Float_Point;
class IVP_Controller;

class CPhysicsObject : public IPhysicsObject
{
public:
	CPhysicsObject( void );
	virtual ~CPhysicsObject( void );

	void			Init( const CPhysCollide *pCollisionModel, IVP_Real_Object *pObject, int materialIndex, float volume, float drag, float angDrag );

	// IPhysicsObject functions
	bool			IsStatic() const override;
	bool			IsAsleep() const override;
	bool			IsTrigger() const override;
	bool			IsFluid() const override;
	bool			IsHinged() const override { return (m_hingedAxis != 0) ? true : false; }
	bool			IsCollisionEnabled() const override;
	bool			IsGravityEnabled() const override;
	bool			IsDragEnabled() const override;
	bool			IsMotionEnabled() const override;
	bool			IsMoveable() const override;
	bool			IsAttachedToConstraint( bool bExternalOnly ) const override;


	void			EnableCollisions( bool enable ) override;
	// Enable / disable gravity for this object
	void			EnableGravity( bool enable ) override;
	// Enable / disable air friction / drag for this object
	void			EnableDrag( bool enable ) override;
	void			EnableMotion( bool enable ) override;

	void			SetGameData( void *pAppData ) override;
	void			*GetGameData( void ) const override;
	void			SetCallbackFlags( unsigned short callbackflags ) override;
	unsigned short	GetCallbackFlags( void ) const override;
	void			SetGameFlags( unsigned short userFlags ) override;
	unsigned short	GetGameFlags( void ) const override;
	void			SetGameIndex( unsigned short gameIndex ) override;
	unsigned short	GetGameIndex( void ) const override;

	void			Wake() override;
	void			WakeNow();
	void			Sleep() override;
	void			RecheckCollisionFilter() override;
	void			RecheckContactPoints();

	void			SetMass( float mass ) override;
	float			GetMass( void ) const override;
	float			GetInvMass( void ) const override;
	void			SetInertia( const Vector &inertia ) override;
	Vector			GetInertia( void ) const override;
	Vector			GetInvInertia( void ) const override;

	void			GetDamping( float *speed, float *rot ) const override;
	void			SetDamping( const float *speed, const float *rot ) override;
	void			SetDragCoefficient( float *pDrag, float *pAngularDrag ) override;
	void			SetBuoyancyRatio( float ratio ) override;
	int				GetMaterialIndex() const override { return GetMaterialIndexInternal(); }
	void			SetMaterialIndex( int materialIndex ) override;
	inline int		GetMaterialIndexInternal( void ) const { return m_materialIndex; }

	unsigned int	GetContents() const override { return m_contentsMask; }
	void			SetContents( unsigned int contents ) override;

	float			GetSphereRadius() const override;
	Vector			GetMassCenterLocalSpace() const override;
	float			GetEnergy() const override;

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
	float			CalculateLinearDrag( const Vector &unitDirection ) const override;
	float			CalculateAngularDrag( const Vector &objectSpaceRotationAxis ) const override;

	bool			GetContactPoint( Vector *contactPoint, IPhysicsObject **contactObject ) const override;
	void			SetShadow( float maxSpeed, float maxAngularSpeed, bool allowPhysicsMovement, bool allowPhysicsRotation ) override;
	void			UpdateShadow( const Vector &targetPosition, const QAngle &targetAngles, bool tempDisableGravity, float timeOffset ) override;
	void			RemoveShadowController() override;
	int				GetShadowPosition( Vector *position, QAngle *angles ) const override;
	IPhysicsShadowController *GetShadowController( void ) const override;
	float			ComputeShadowControl( const hlshadowcontrol_params_t &params, float secondsToArrival, float dt ) override;

	const CPhysCollide	*GetCollide( void ) const override;
	char const		*GetName() const override;

	float			GetDragInDirection( const IVP_U_Float_Point &dir ) const;
	float			GetAngularDragInDirection( const IVP_U_Float_Point &angVelocity ) const;
	void			BecomeTrigger() override;
	void			RemoveTrigger() override;
	void			BecomeHinged( int localAxis ) override;
	void			RemoveHinged() override;

	IPhysicsFrictionSnapshot *CreateFrictionSnapshot() override;
	void			DestroyFrictionSnapshot( IPhysicsFrictionSnapshot *pSnapshot ) override;

	void			OutputDebugInfo() const override;

	// local functions
	inline	IVP_Real_Object *GetObject( void ) const { return m_pObject; }
	inline int		CallbackFlags( void ) const { return m_callbacks; }
	inline void		AddCallbackFlags( unsigned short flags ) { m_callbacks |= flags; }
	inline void		RemoveCallbackFlags( unsigned short flags ) { m_callbacks &= ~flags; }
	inline bool		HasTouchedDynamic();
	inline void		SetTouchedDynamic();
	void			NotifySleep( void );
	void			NotifyWake( void );
	int				GetSleepState( void ) const { return m_sleepState; }
	inline void		ForceSilentDelete() { m_forceSilentDelete = true; }

	inline int		GetActiveIndex( void ) const { return m_activeIndex; }
	inline void		SetActiveIndex( int index ) { m_activeIndex = index; }
	inline float	GetBuoyancyRatio( void ) const { return m_buoyancyRatio; }
	// returns true if the mass center is set to the default for the collision model
	bool			IsMassCenterAtDefault() const;

	// is this object simulated, or controlled by game logic?
	bool			IsControlledByGame() const;

	/*IVP_SurfaceManager *GetSurfaceManager( void ) const;

	void			WriteToTemplate( vphysics_save_cphysicsobject_t &objectTemplate );
	void			InitFromTemplate( CPhysicsEnvironment *pEnvironment, void *pGameData, const vphysics_save_cphysicsobject_t &objectTemplate );

	CPhysicsEnvironment	*GetVPhysicsEnvironment();
	const CPhysicsEnvironment	*GetVPhysicsEnvironment() const;*/

public:
	// NOTE: Local to vphysics, used to save/restore shadow controller
	void			RestoreShadowController( IPhysicsShadowController *pShadowController );
	friend bool		RestorePhysicsObject( const physrestoreparams_t &params, CPhysicsObject **ppObject );

	bool			IsControlling( const IVP_Controller *pController ) const;
	float			GetVolume() const;
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

public:
	unsigned short	m_materialIndex;
	unsigned short	m_activeIndex;

	unsigned short	m_callbacks;
	unsigned short	m_gameFlags;
	unsigned int	m_contentsMask;
	
	float			m_volume;
	float			m_buoyancyRatio;
	float			m_dragCoefficient;
	float			m_angDragCoefficient;
};