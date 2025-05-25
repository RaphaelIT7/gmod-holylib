#include "vphysics_interface.h"
#include "utlvector.h"

class IVPhysicsDebugOverlay;
namespace GMODSDK
{
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
	struct constraint_limitedhingeparams_t;
	struct vphysics_save_iphysicsobject_t;
	class IVP_Mindist;
	struct physprerestoreparams_t;

class CPhysicsEnvironment : public IPhysicsEnvironment
{
public:
	CPhysicsEnvironment( void );
	~CPhysicsEnvironment( void );

	void SetDebugOverlay( CreateInterfaceFn debugOverlayFactory ) override;
	IVPhysicsDebugOverlay *GetDebugOverlay( void ) override;

	void			SetGravity( const Vector& gravityVector ) override;
	IPhysicsObject	*CreatePolyObject( const CPhysCollide *pCollisionModel, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t *pParams ) override;
	IPhysicsObject	*CreatePolyObjectStatic( const CPhysCollide *pCollisionModel, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t *pParams ) override;
	unsigned int	GetObjectSerializeSize( IPhysicsObject *pObject ) const override;
	void			SerializeObjectToBuffer( IPhysicsObject *pObject, unsigned char *pBuffer, unsigned int bufferSize ) override;
	IPhysicsObject *UnserializeObjectFromBuffer( void *pGameData, unsigned char *pBuffer, unsigned int bufferSize, bool enableCollisions ) override;
	


	IPhysicsSpring	*CreateSpring( IPhysicsObject *pObjectStart, IPhysicsObject *pObjectEnd, springparams_t *pParams ) override;
	IPhysicsFluidController	*CreateFluidController( IPhysicsObject *pFluidObject, fluidparams_t *pParams ) override;
	IPhysicsConstraint *CreateRagdollConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ragdollparams_t &ragdoll ) override;

	IPhysicsConstraint *CreateHingeConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_hingeparams_t &hinge ) override;
	virtual IPhysicsConstraint *CreateLimitedHingeConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_limitedhingeparams_t &hinge );
	IPhysicsConstraint *CreateFixedConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_fixedparams_t &fixed ) override;
	IPhysicsConstraint *CreateSlidingConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_slidingparams_t &sliding ) override;
	IPhysicsConstraint *CreateBallsocketConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ballsocketparams_t &ballsocket ) override;
	IPhysicsConstraint *CreatePulleyConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_pulleyparams_t &pulley ) override;
	IPhysicsConstraint *CreateLengthConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_lengthparams_t &length ) override;

	IPhysicsConstraintGroup *CreateConstraintGroup( const constraint_groupparams_t &group ) override;
	void DestroyConstraintGroup( IPhysicsConstraintGroup *pGroup ) override;

	void			Simulate( float deltaTime ) override;
	float			GetSimulationTimestep() const override;
	void			SetSimulationTimestep( float timestep ) override;
	float			GetSimulationTime() const override;
	float			GetNextFrameTime() const override;
	bool			IsInSimulation() const override;

	void DestroyObject( IPhysicsObject * ) override;
	void DestroySpring( IPhysicsSpring * ) override;
	void DestroyFluidController( IPhysicsFluidController * ) override;
	void DestroyConstraint( IPhysicsConstraint * ) override;

	void SetCollisionEventHandler( IPhysicsCollisionEvent *pCollisionEvents ) override;
	void SetObjectEventHandler( IPhysicsObjectEvent *pObjectEvents ) override;
	void SetConstraintEventHandler( IPhysicsConstraintEvent *pConstraintEvents ) override;

	IPhysicsShadowController *CreateShadowController( IPhysicsObject *pObject, bool allowTranslation, bool allowRotation ) override;
	void DestroyShadowController( IPhysicsShadowController * ) override;
	IPhysicsMotionController *CreateMotionController( IMotionEvent *pHandler ) override;
	void DestroyMotionController( IPhysicsMotionController *pController ) override;
	IPhysicsPlayerController *CreatePlayerController( IPhysicsObject *pObject ) override;
	void DestroyPlayerController( IPhysicsPlayerController *pController ) override;
	IPhysicsVehicleController *CreateVehicleController( IPhysicsObject *pVehicleBodyObject, const vehicleparams_t &params, unsigned int nVehicleType, IPhysicsGameTrace *pGameTrace ) override;
	void DestroyVehicleController( IPhysicsVehicleController *pController ) override;

	void SetQuickDelete( bool bQuick ) override
	{
		m_deleteQuick = bQuick;
	}
	virtual bool ShouldQuickDelete() const { return m_deleteQuick; }
	virtual void TraceBox( trace_t *ptr, const Vector &mins, const Vector &maxs, const Vector &start, const Vector &end );
	void SetCollisionSolver( IPhysicsCollisionSolver *pCollisionSolver ) override;
	void GetGravity( Vector *pGravityVector ) const override;
	int	 GetActiveObjectCount() const override;
	void GetActiveObjects( IPhysicsObject **pOutputObjectList ) const override;
	const IPhysicsObject **GetObjectList( intp *pOutputObjectCount ) const;
	bool TransferObject( IPhysicsObject *pObject, IPhysicsEnvironment *pDestinationEnvironment ) override;

	IVP_Environment	*GetIVPEnvironment( void ) { return m_pPhysEnv; }
	void		ClearDeadObjects( void );
	IVP_Controller *GetDragController() { return m_pDragController; }
	const IVP_Controller *GetDragController() const { return m_pDragController; }
	void SetAirDensity( float density ) override;
	float GetAirDensity( void ) const override;
	void ResetSimulationClock( void ) override;
	IPhysicsObject *CreateSphereObject( float radius, int materialIndex, const Vector &position, const QAngle &angles, objectparams_t *pParams, bool isStatic ) override;
	void CleanupDeleteList() override;
	void EnableDeleteQueue( bool enable ) override { m_queueDeleteObject = enable; }
	// debug
	bool IsCollisionModelUsed( CPhysCollide *pCollide ) const override;

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
	bool Save( const physsaveparams_t &params  ) override;
	void PreRestore( const physprerestoreparams_t &params );
	bool Restore( const physrestoreparams_t &params ) override;
	void PostRestore() override;
	void PhantomAdd( CPhysicsObject *pObject );
	void PhantomRemove( CPhysicsObject *pObject );

	void AddPlayerController( IPhysicsPlayerController *pController );
	void RemovePlayerController( IPhysicsPlayerController *pController );
	IPhysicsPlayerController *FindPlayerController( IPhysicsObject *pObject );

	IPhysicsCollisionEvent *GetCollisionEventHandler();
	// a constraint is being disabled - report the game DLL as "broken"
	void NotifyConstraintDisabled( IPhysicsConstraint *pConstraint );

public:
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

class IVP_Constraint;
class IVP_Listener_Constraint
{
public:
	virtual void event_constraint_broken(IVP_Constraint *) = 0;
#ifdef HAVANA_CONSTRAINTS
	virtual void event_constraint_broken(class hk_Breakable_Constraint*) = 0;
#endif // HAVANA_CONSTRAINTS
};

class CPhysicsListenerConstraint : public IVP_Listener_Constraint
{
public:
	CPhysicsListenerConstraint()
	{
		m_pCallback = NULL;
	}
	virtual ~CPhysicsListenerConstraint()
	{
	}
public:
	IPhysicsConstraintEvent *m_pCallback;
};

class IVP_Anomaly_Limits;
class IVP_Core;
class IVP_Anomaly_Manager {
	IVP_BOOL delete_this_if_env_is_deleted;
public:
	virtual void max_velocity_exceeded(IVP_Anomaly_Limits *, IVP_Core *, IVP_U_Float_Point *velocity_in_out);
	virtual void max_angular_velocity_exceeded( IVP_Anomaly_Limits *, IVP_Core *, IVP_U_Float_Point *angular_velocity_in_out);
	virtual void inter_penetration( IVP_Mindist *mindist,IVP_Real_Object *, IVP_Real_Object *, IVP_DOUBLE);
	virtual IVP_BOOL max_collisions_exceeded_check_freezing(IVP_Anomaly_Limits *, IVP_Core *);

	virtual void environment_will_be_deleted(IVP_Environment *);
	virtual IVP_FLOAT get_push_speed_penetration( IVP_Real_Object *, IVP_Real_Object *);

	void solve_inter_penetration_simple( IVP_Real_Object *, IVP_Real_Object *);

	IVP_Anomaly_Manager(IVP_BOOL delete_this_if_env_is_deleted = IVP_TRUE);
	virtual ~IVP_Anomaly_Manager();
};

class IVP_Collision_Filter {
public:
	virtual IVP_BOOL check_objects_for_collision_detection(IVP_Real_Object *object0, IVP_Real_Object *object1) = 0;
	virtual void environment_will_be_deleted(IVP_Environment *environment) = 0;

	virtual ~IVP_Collision_Filter();
};

class CCollisionSolver : public IVP_Collision_Filter, public IVP_Anomaly_Manager
{
public:
	CCollisionSolver( void ) : IVP_Anomaly_Manager(IVP_FALSE) { m_pSolver = NULL; }
	void SetHandler( IPhysicsCollisionSolver *pSolver ) { m_pSolver = pSolver; }

	virtual int max_collision_checks_exceeded( int totalChecks )
	{
		if ( m_pSolver )
		{
			return m_pSolver->AdditionalCollisionChecksThisTick( totalChecks );
		}
		return 0;
	}

public:
	struct realobjectpair_t
	{
		IVP_Real_Object *pObj0;
		IVP_Real_Object *pObj1;
		inline bool operator==( const realobjectpair_t &src ) const
		{
			return (pObj0 == src.pObj0) && (pObj1 == src.pObj1);
		}
	};

	IPhysicsCollisionSolver					*m_pSolver;
	// UNDONE: Linear search? should be small, but switch to rb tree if this ever gets large
	CUtlVector<realobjectpair_t>	m_rescue;
#if defined(IVP_ENABLE_VISUALIZER)
public:
	CCollisionVisualizer *pVisualizer;
#endif
};

class CCollisionEvent : public IPhysicsCollisionEvent, public IPhysicsCollisionSolver, public IPhysicsObjectEvent
{
};

class IVP_Event_Object;
class IVP_Listener_Object
{
public:
	virtual void event_object_deleted(IVP_Event_Object*) = 0;
	virtual void event_object_created(IVP_Event_Object*) = 0;
	virtual void event_object_revived(IVP_Event_Object*) = 0;
	virtual void event_object_frozen(IVP_Event_Object*) = 0;

	virtual ~IVP_Listener_Object() {};
};

class CSleepObjects : public IVP_Listener_Object
{
public:
	CSleepObjects(void) : IVP_Listener_Object()
	{
		m_pCallback = NULL;
		m_lastScrapeTime = 0.0f;
	}

	void SetHandler(IPhysicsObjectEvent* pListener)
	{
		m_pCallback = pListener;
	}

	void Remove(intp index)
	{
	/*	// fast remove preserves indices except for the last element (moved into the empty spot)
		m_activeObjects.FastRemove(index);
		// If this isn't the last element, shift its index over
		if (index < m_activeObjects.Count())
		{
			m_activeObjects[index]->SetActiveIndex(index);
		}
	*/
	}

	void DeleteObject(CPhysicsObject* pObject)
	{
	/*	int index = pObject->GetActiveIndex();
		if (index < m_activeObjects.Count())
		{
			Assert(m_activeObjects[index] == pObject);
			Remove(index);
			pObject->SetActiveIndex( std::numeric_limits<int>::max() );
		}
		else
		{
			Assert(index == std::numeric_limits<int>::max() );
		}
	*/
	}

private:
	CUtlVector<GMODSDK::CPhysicsObject*>	m_activeObjects;
	float							m_lastScrapeTime;
	IPhysicsObjectEvent* m_pCallback;
};

};