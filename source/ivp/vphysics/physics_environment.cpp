//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tier0/threadtools.h"
#include "physics_constraint.h"
#include "physics_spring.h"
#include "physics_fluid.h"
#include "physics_shadow.h"
#include "physics_motioncontroller.h"
#include "physics_vehicle.h"
#include "physics_virtualmesh.h"
#include "tier1/utlmultilist.h"
#include "vphysics/constraints.h"
#include "vphysics/vehicles.h"
#include "vphysics/object_hash.h"
#include "vphysics/performance.h"
#include "vphysics/stats.h"
#include "vphysics/player_controller.h"
#include "vphysics_saverestore.h"
#include "vphysics_internal.h"

#include "ivu_linear_macros.hxx"
#include "ivp_collision_filter.hxx"
#include "ivp_listener_collision.hxx"
#include "ivp_listener_object.hxx"
#include "ivp_mindist.hxx"
#include "ivp_mindist_intern.hxx"
#include "ivp_friction.hxx"
#include "ivp_anomaly_manager.hxx"
#include "ivp_time.hxx"
#include "ivp_listener_psi.hxx"
#include "ivp_phantom.hxx"
#include "ivp_range_manager.hxx"
#include "ivp_clustering_visualizer.hxx"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IPhysicsObjectPairHash *CreateObjectPairHash();

IVP_Synapse_Friction *GetOppositeSynapse( IVP_Synapse_Friction *pfriction )
{
	IVP_Contact_Point *contact = pfriction->get_contact_point();
	IVP_Synapse_Friction *ptest = contact->get_synapse(0);
	if ( ptest == pfriction )
	{
		ptest = contact->get_synapse(1);
	}

	return ptest;
}

IVP_Real_Object *GetOppositeSynapseObject( IVP_Synapse_Friction *pfriction )
{
	IVP_Synapse_Friction *opposite = GetOppositeSynapse( pfriction );
	return opposite->get_object();
}

// simple delete queue
struct IDeleteQueueItem
{
	// Add a virtual destructor to silence the clang warning.
	// Note that this destructor doesn't actually do anything -- you
	// still have to use the Delete() then delete pattern.
	virtual ~IDeleteQueueItem() = 0;

	virtual void Delete() = 0;
};

IDeleteQueueItem::~IDeleteQueueItem() = default;

template <typename T>
class CDeleteProxy : public IDeleteQueueItem
{
public:
	CDeleteProxy(T *pItem) : m_pItem(pItem) {}
	void Delete() override { delete m_pItem; }
private:
	T *m_pItem;
};

class CDeleteQueue
{
public:
	void Add( IDeleteQueueItem *pItem )
	{
		m_list.AddToTail( pItem );
	}

	template <typename T>
	void QueueForDelete( T *pItem )
	{
		Add( new CDeleteProxy<T>(pItem) );
	}
	void DeleteAll()
	{
		for ( int i = m_list.Count()-1; i >= 0; --i)
		{
			m_list[i]->Delete();
			delete m_list[i];
		}
		m_list.RemoveAll();
	}

private:
	CUtlVector< IDeleteQueueItem * >	m_list;
};

class CPhysicsCollisionData : public IPhysicsCollisionData
{
public:
	CPhysicsCollisionData( IVP_Contact_Situation *contact ) : m_pContact(contact) {}

	void GetSurfaceNormal( Vector &out ) override { ConvertDirectionToHL( m_pContact->surf_normal, out ); }
	void GetContactPoint( Vector &out ) override { ConvertPositionToHL( m_pContact->contact_point_ws, out ); }
	void GetContactSpeed( Vector &out ) override { ConvertPositionToHL( m_pContact->speed, out ); }

	const IVP_Contact_Situation *m_pContact;
};

class CPhysicsFrictionData : public IPhysicsCollisionData
{
public:
	CPhysicsFrictionData( IVP_Synapse_Friction *synapse, float sign )
		: m_pPoint{synapse->get_contact_point()},
		  m_sign(sign),
		  m_pContact{nullptr}
	{
	}

	CPhysicsFrictionData( IVP_Event_Friction *pEvent )
		: m_pPoint{pEvent->friction_handle},
		  m_sign(1.0f),
		  m_pContact{pEvent->contact_situation}
	{
	}

	void GetSurfaceNormal( Vector &out ) override
	{ 
		DebugPrint();
		if ( m_pContact )
		{
			ConvertDirectionToHL( m_pContact->surf_normal, out ); 
		}
		else
		{
			IVP_U_Float_Point normal;
			IVP_Contact_Point_API::get_surface_normal_ws(m_pPoint, &normal);
			ConvertDirectionToHL( normal, out );
			out *= m_sign;
		}
	}
	void GetContactPoint( Vector &out ) override
	{
		DebugPrint();
		if ( m_pContact )
		{
			ConvertPositionToHL( m_pContact->contact_point_ws, out ); 
		}
		else
		{
			ConvertPositionToHL( *m_pPoint->get_contact_point_ws(), out ); 
		}
	}
	void GetContactSpeed( Vector &out ) override
	{
		DebugPrint();
		if ( m_pContact )
		{
			ConvertPositionToHL( m_pContact->speed, out );
		}
		else
		{
			out.Init();
		}
	}

private:
	const IVP_Contact_Point *m_pPoint;
	float m_sign;
	const IVP_Contact_Situation *m_pContact;
};


//-----------------------------------------------------------------------------
// Purpose: Routes object event callbacks to game code
//-----------------------------------------------------------------------------
class CSleepObjects : public IVP_Listener_Object
{
public:
	CSleepObjects( void ) : IVP_Listener_Object() 
	{
		m_pCallback = NULL;
		m_lastScrapeTime = 0.0f;
	}

	void SetHandler( IPhysicsObjectEvent *pListener )
	{
		m_pCallback = pListener;
	}

	void Remove( int index )
	{
		// fast remove preserves indices except for the last element (moved into the empty spot)
		m_activeObjects.FastRemove(index);
		// If this isn't the last element, shift its index over
		if ( index < m_activeObjects.Count() && m_activeObjects.IsValidIndex( index ) )
		{
			m_activeObjects[index]->SetActiveIndex( index );
		}
	}

	void DeleteObject( CPhysicsObject *pObject )
	{
		int index = pObject->GetActiveIndex();
		if ( index < m_activeObjects.Count() )
		{
			Assert( m_activeObjects[index] == pObject );
			Remove( index );
			pObject->SetActiveIndex( std::numeric_limits<int>::max() );
		}
		else
		{
			Assert(index==std::numeric_limits<int>::max());
		}
	}

    void event_object_deleted( IVP_Event_Object *pEvent ) override
	{
		CPhysicsObject *pObject = static_cast<CPhysicsObject *>(pEvent->real_object->client_data);
		if ( !pObject )
			return;

		DeleteObject(pObject);
	}

    void event_object_created( IVP_Event_Object * ) override
	{
	}

    void event_object_revived( IVP_Event_Object *pEvent ) override
	{
		CPhysicsObject *pObject = static_cast<CPhysicsObject *>(pEvent->real_object->client_data);
		if ( !pObject )
			return;

		int sleepState = pObject->GetSleepState();
		
		pObject->NotifyWake();

		// asleep, but already in active list
		if ( sleepState == OBJ_STARTSLEEP )
			return;

		// don't track static objects (like the world).  That way we only track objects that will move
		if ( pObject->GetObject()->get_movement_state() != IVP_MT_STATIC )
		{
			Assert(pObject->GetActiveIndex() == std::numeric_limits<int>::max());
			if ( pObject->GetActiveIndex() != std::numeric_limits<int>::max())
				return;

			auto index = m_activeObjects.AddToTail( pObject );
			pObject->SetActiveIndex( index );
		}
		if ( m_pCallback )
		{
			m_pCallback->ObjectWake( pObject );
		}
	}

    void event_object_frozen( IVP_Event_Object *pEvent ) override
	{
		CPhysicsObject *pObject = static_cast<CPhysicsObject *>(pEvent->real_object->client_data);
		if ( !pObject )
			return;

		pObject->NotifySleep();
		if ( m_pCallback )
		{
			m_pCallback->ObjectSleep( pObject );
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: This walks the objects in the environment and generates friction events
	//			for any scraping that is occurring.
	//-----------------------------------------------------------------------------
	void ProcessActiveObjects( IVP_Environment *pEnvironment, IPhysicsCollisionEvent *pEvent )
	{
		// FIXME: Is this correct? Shouldn't it do next PSI - lastScrape?
		double nextTime = pEnvironment->get_old_time_of_last_PSI().get_time();
		double delta = nextTime - m_lastScrapeTime;

		// only process if we have done a PSI
		if ( delta < pEnvironment->get_delta_PSI_time() )
			return;

		double t = 0.0;
		if ( delta != 0.0 )
		{
			t = 1.0 / delta;
		}

		m_lastScrapeTime = nextTime;

		// UNDONE: This only calls friciton for one object in each pair.
		// UNDONE: Split energy in half and call for both objects?
		// UNDONE: Don't split/call if one object is static (like the world)?
		for ( auto *pObject : m_activeObjects )
		{
			IVP_Real_Object *ivpObject = pObject->GetObject();
			
			// no friction callbacks for this object
			if ( ! (pObject->CallbackFlags() & CALLBACK_GLOBAL_FRICTION) )
				continue;

			// UNDONE: IVP_Synapse_Friction is supposed to be opaque.  Is there a better way
			// to implement this?  Using the friction listener is much more work for the CPU
			// and considers sleeping objects.
			IVP_Synapse_Friction *pfriction = ivpObject->get_first_friction_synapse();
			while ( pfriction )
			{
				IVP_Contact_Point *contact = pfriction->get_contact_point();
				IVP_Synapse_Friction *pOpposite = GetOppositeSynapse( pfriction );
				IVP_Real_Object *pobj = pOpposite->get_object();
				CPhysicsObject *pScrape = (CPhysicsObject *)pobj->client_data;

				// friction callbacks for this object?
				if ( pScrape->CallbackFlags() & CALLBACK_GLOBAL_FRICTION )
				{
					float energy = IVP_Contact_Point_API::get_eliminated_energy( contact );
					if ( energy ) 
					{
						// scrape with an estimate for the energy per unit mass
						// This assumes that the game is interested in some measure of vibration
						// for sound effects.  This also assumes that more massive objects require
						// more energy to vibrate.
						energy = energy * t * ivpObject->get_core()->get_inv_mass();

						if ( energy > 0.05f )
						{
							int hitSurface = pScrape->GetMaterialIndexInternal();

							int materialIndex = pOpposite->get_material_index();
							if ( materialIndex )
							{
								// use the per-triangle material if it has one
								hitSurface = physprops->RemapIVPMaterialIndex( materialIndex );
							}

							float sign = (pfriction == contact->get_synapse(0)) ? 1.0f : -1.0f;

							CPhysicsFrictionData data(pfriction, sign);

							pEvent->Friction( pObject, ConvertEnergyToHL(energy), pObject->GetMaterialIndexInternal(), hitSurface, &data );
						}
						IVP_Contact_Point_API::reset_eliminated_energy( contact );
					}
				}
				pfriction = pfriction->get_next();
			}
		}
	}
	void DebugCheckContacts( IVP_Environment *pEnvironment )
	{
		IVP_Mindist_Manager *pManager = pEnvironment->get_mindist_manager();

		for( IVP_Mindist *mdist = pManager->exact_mindists; mdist != NULL; mdist = mdist->next )
		{
			IVP_Real_Object *obj[2];
			mdist->get_objects( obj );
			IVP_BOOL check = pEnvironment->get_collision_filter()->check_objects_for_collision_detection( obj[0], obj[1] );
			Assert(check);
			if ( !check )
			{
				Msg("Changed collision rules for %s vs. %s without calling recheck!\n", obj[0]->get_name(), obj[1]->get_name() );
			}
		}
	}
	int	GetActiveObjectCount( void ) const
	{
		return m_activeObjects.Count();
	}
	void GetActiveObjects( IPhysicsObject **pOutputObjectList ) const
	{
		for ( int i = 0; i < m_activeObjects.Count(); i++ )
		{
			pOutputObjectList[i] = m_activeObjects[i];
		}
	}
	void UpdateSleepObjects( void )
	{
		int i;

		CUtlVector<CPhysicsObject *> sleepObjects;

		for ( i = 0; i < m_activeObjects.Count(); i++ )
		{
			CPhysicsObject *pObject = m_activeObjects[i];
			
			if ( pObject->GetSleepState() != OBJ_AWAKE )
			{
				sleepObjects.AddToTail( pObject );
			}
		}

		for ( i = sleepObjects.Count()-1; i >= 0; --i )
		{
			// put fully to sleep
			sleepObjects[i]->NotifySleep();

			// remove from the active list
			DeleteObject( sleepObjects[i] );
		}
	}

private:
	CUtlVector<CPhysicsObject *>	m_activeObjects;
	double							m_lastScrapeTime;
	IPhysicsObjectEvent				*m_pCallback;
};

class CEmptyCollisionListener : public IPhysicsCollisionEvent
{
public:
	void PreCollision( vcollisionevent_t * ) override {}
	void PostCollision( vcollisionevent_t * ) override {}

	// This is a scrape event.  The object has scraped across another object consuming the indicated energy
	void Friction( IPhysicsObject *, float, int, int, IPhysicsCollisionData * ) override {}

	void StartTouch( IPhysicsObject *, IPhysicsObject *, IPhysicsCollisionData * ) override {}
	void EndTouch( IPhysicsObject *, IPhysicsObject *, IPhysicsCollisionData * ) override {}

	void FluidStartTouch( IPhysicsObject *, IPhysicsFluidController * ) override {}
	void FluidEndTouch( IPhysicsObject *, IPhysicsFluidController * ) override {}

	void ObjectEnterTrigger( IPhysicsObject *, IPhysicsObject * ) override {}
	void ObjectLeaveTrigger( IPhysicsObject *, IPhysicsObject * ) override {}

	void PostSimulationFrame() override {}
};

CEmptyCollisionListener g_EmptyCollisionListener;

#define ALL_COLLISION_FLAGS (IVP_LISTENER_COLLISION_CALLBACK_PRE_COLLISION|IVP_LISTENER_COLLISION_CALLBACK_POST_COLLISION|IVP_LISTENER_COLLISION_CALLBACK_FRICTION)
//-----------------------------------------------------------------------------
// Purpose: Routes collision event callbacks to game code
//-----------------------------------------------------------------------------
class CPhysicsListenerCollision : public IVP_Listener_Collision, public IVP_Listener_Phantom
{
public:
	CPhysicsListenerCollision();

	void SetHandler( IPhysicsCollisionEvent *pCallback )
	{
		m_pCallback = pCallback;
	}
	IPhysicsCollisionEvent *GetHandler() { return m_pCallback; }

    void event_pre_collision( IVP_Event_Collision *pEvent ) override
	{
		m_event.isCollision = false;
		m_event.isShadowCollision = false;
		IVP_Contact_Situation *contact = pEvent->contact_situation;
		CPhysicsObject *pObject1 = static_cast<CPhysicsObject *>(contact->objects[0]->client_data);
		CPhysicsObject *pObject2 = static_cast<CPhysicsObject *>(contact->objects[1]->client_data);
		if ( !pObject1 || !pObject2 )
			return;

		unsigned int flags1 = pObject1->CallbackFlags();
		unsigned int flags2 = pObject2->CallbackFlags();

		m_event.isCollision = (flags1 & flags2 & CALLBACK_GLOBAL_COLLISION) ? true : false;
		
		// only call shadow collisions if one is shadow and the other isn't (hence the xor)
		// (if both are shadow, the collisions happen in AI - if neither, then no callback)
		m_event.isShadowCollision = ((flags1^flags2) & CALLBACK_SHADOW_COLLISION) ? true : false;

		m_event.pObjects[0] = pObject1;
		m_event.pObjects[1] = pObject2;
		m_event.deltaCollisionTime = pEvent->d_time_since_last_collision;
		// This timer must have been reset or something (constructor initializes time to -1000)
		// Fake the time to 50ms (resets happen often in rolling collisions for some reason)
		if ( m_event.deltaCollisionTime > 999 )
		{
			m_event.deltaCollisionTime = 1.0;
		}
			
		CPhysicsCollisionData data(contact);
		m_event.pInternalData = &data;

		// clear out any static object collisions unless flagged to keep them
		if ( contact->objects[0]->get_movement_state() == IVP_MT_STATIC )
		{
			// don't call global if disabled
			if ( !(flags2 & CALLBACK_GLOBAL_COLLIDE_STATIC) )
			{
				m_event.isCollision = false;
			}
		}
		if ( contact->objects[1]->get_movement_state() == IVP_MT_STATIC )
		{
			// don't call global if disabled
			if ( !(flags1 & CALLBACK_GLOBAL_COLLIDE_STATIC) )
			{
				m_event.isCollision = false;
			}
		}

		if ( !m_event.isCollision && !m_event.isShadowCollision )
			return;

		// look up surface props
		for ( int i = 0; i < 2; i++ )
		{
			m_event.surfaceProps[i] = physprops->GetIVPMaterialIndex( contact->materials[i] );
			if ( m_event.surfaceProps[i] < 0 )
			{
				m_event.surfaceProps[i] = m_event.pObjects[i]->GetMaterialIndex();
			}
		}

		m_pCallback->PreCollision( &m_event );
	}

    void event_post_collision( IVP_Event_Collision *pEvent ) override
	{
		// didn't call preCollision, so don't call postCollision
		if ( !m_event.isCollision && !m_event.isShadowCollision )
			return;

		IVP_Contact_Situation *contact = pEvent->contact_situation;

		float collisionSpeed = contact->speed.dot_product(&contact->surf_normal);
		m_event.collisionSpeed = ConvertDistanceToHL( fabs(collisionSpeed) );
		CPhysicsCollisionData data(contact);
		m_event.pInternalData = &data;

		m_pCallback->PostCollision( &m_event );
	}

    void event_collision_object_deleted( class IVP_Real_Object *) override
	{
		// enable this in constructor
	}

    void event_friction_created( IVP_Event_Friction *pEvent ) override
	{
		IVP_Contact_Situation *contact = pEvent->contact_situation;
		CPhysicsObject *pObject1 = static_cast<CPhysicsObject *>(contact->objects[0]->client_data);
		CPhysicsObject *pObject2 = static_cast<CPhysicsObject *>(contact->objects[1]->client_data);

		if ( !pObject1 || !pObject2 )
			return;

		unsigned int flags1 = pObject1->CallbackFlags();
		unsigned int flags2 = pObject2->CallbackFlags();
		unsigned int allflags = flags1|flags2;

		if ( !pObject1->IsStatic() || !pObject2->IsStatic() )
		{
			if ( !pObject1->HasTouchedDynamic() && pObject2->IsMoveable() )
			{
				pObject1->SetTouchedDynamic();
			}
			if ( !pObject2->HasTouchedDynamic() && pObject1->IsMoveable() )
			{
				pObject2->SetTouchedDynamic();
			}
		}

		bool calltouch = ( allflags & CALLBACK_GLOBAL_TOUCH ) ? true : false;
		if ( !calltouch )
			return;

		if ( pObject1->IsStatic() || pObject2->IsStatic() )
		{
			if ( !( allflags & CALLBACK_GLOBAL_TOUCH_STATIC ) )
				return;
		}

		CPhysicsFrictionData data(pEvent);
		m_pCallback->StartTouch( pObject1, pObject2, &data );
	}


    void event_friction_deleted( IVP_Event_Friction *pEvent ) override
	{
		IVP_Contact_Situation *contact = pEvent->contact_situation;
		CPhysicsObject *pObject1 = static_cast<CPhysicsObject *>(contact->objects[0]->client_data);
		CPhysicsObject *pObject2 = static_cast<CPhysicsObject *>(contact->objects[1]->client_data);
		if ( !pObject1 || !pObject2 )
			return;

		unsigned int flags1 = pObject1->CallbackFlags();
		unsigned int flags2 = pObject2->CallbackFlags();

		unsigned int allflags = flags1|flags2;

		bool calltouch = ( allflags & CALLBACK_GLOBAL_TOUCH ) ? true : false;
		if ( !calltouch )
			return;

		if ( pObject1->IsStatic() || pObject2->IsStatic() )
		{
			if ( !( allflags & CALLBACK_GLOBAL_TOUCH_STATIC ) )
				return;
		}

		CPhysicsFrictionData data(pEvent);
		m_pCallback->EndTouch( pObject1, pObject2, &data );
	}

	void event_friction_pair_created( class IVP_Friction_Core_Pair *pair ) override;
	void event_friction_pair_deleted( class IVP_Friction_Core_Pair *pair ) override;
	void mindist_entered_volume( class IVP_Controller_Phantom *, class IVP_Mindist_Base * ) override {}
	void mindist_left_volume( class IVP_Controller_Phantom *, class IVP_Mindist_Base *) override {}

	void core_entered_volume( IVP_Controller_Phantom *controller, IVP_Core *pCore ) override
	{
		CPhysicsFluidController *pFluid = static_cast<CPhysicsFluidController *>( controller->client_data );
		IVP_Real_Object *pivp = pCore->objects.element_at(0);
		CPhysicsObject *pObject = static_cast<CPhysicsObject *>(pivp->client_data);
		if ( !pObject )
			return;

		if ( pFluid )
		{
			if ( (pObject->CallbackFlags() & CALLBACK_FLUID_TOUCH) )
			{
				m_pCallback->FluidStartTouch( pObject, pFluid );
			}
		}
		else
		{
			// must be a trigger
			IVP_Real_Object *pTriggerIVP = controller->get_object();
			CPhysicsObject *pTrigger = static_cast<CPhysicsObject *>(pTriggerIVP->client_data);

			if ( pTrigger )
			{
				m_pCallback->ObjectEnterTrigger( pTrigger, pObject );
			}
		}
	}

	void core_left_volume( IVP_Controller_Phantom *controller, IVP_Core *pCore ) override
	{
		CPhysicsFluidController *pFluid = static_cast<CPhysicsFluidController *>( controller->client_data );
		IVP_Real_Object *pivp = pCore->objects.element_at(0);
		CPhysicsObject *pObject = static_cast<CPhysicsObject *>(pivp->client_data);
		if ( !pObject )
			return;

		if ( pFluid )
		{
			if ( (pObject->CallbackFlags() & CALLBACK_FLUID_TOUCH) )
			{
				m_pCallback->FluidEndTouch( pObject, pFluid );
			}
		}
		else
		{
			// must be a trigger
			IVP_Real_Object *pTriggerIVP = controller->get_object();
			CPhysicsObject *pTrigger = static_cast<CPhysicsObject *>(pTriggerIVP->client_data);

			if ( pTrigger )
			{
				m_pCallback->ObjectLeaveTrigger( pTrigger, pObject );
			}
		}
	}
	void phantom_is_going_to_be_deleted_event(class IVP_Controller_Phantom *) override {}

	void EventPSI( CPhysicsEnvironment *pEnvironment )
	{
		m_pCallback->PostSimulationFrame();
		UpdatePairListPSI( pEnvironment );
	}
private:
	
	struct corepair_t
	{
		corepair_t() = default;
		corepair_t( IVP_Friction_Core_Pair *pair )
		{
			int index = ( pair->objs[0] < pair->objs[1] ) ? 0 : 1;
			core0 = pair->objs[index];
			core1 = pair->objs[!index];
			lastImpactTime= pair->last_impact_time_pair;
		}

		IVP_Core *core0;
		IVP_Core *core1;
		IVP_Time lastImpactTime;
	};

	static bool CorePairLessFunc( const corepair_t &lhs, const corepair_t &rhs ) 
	{ 
		if ( lhs.core0 != rhs.core0 )
			return ( lhs.core0 < rhs.core0 );
		else
			return ( lhs.core1 < rhs.core1 );
	}
	void UpdatePairListPSI( CPhysicsEnvironment *pEnvironment )
	{
		auto index = m_pairList.FirstInorder();
		IVP_Time currentTime = pEnvironment->GetIVPEnvironment()->get_current_time();

		while ( m_pairList.IsValidIndex(index) )
		{
			auto next = m_pairList.NextInorder( index );
			corepair_t &test = m_pairList.Element(index);
			
			// only keep 1 seconds worth of data
			if ( (currentTime - test.lastImpactTime) > 1.0 )
			{
				m_pairList.RemoveAt( index );
			}
			index = next;
		}
	}

	CUtlRBTree<corepair_t>			m_pairList;
	//float							m_pairListOldestTime;


	IPhysicsCollisionEvent			*m_pCallback;
	vcollisionevent_t				m_event;

};


CPhysicsListenerCollision::CPhysicsListenerCollision()
	: IVP_Listener_Collision( ALL_COLLISION_FLAGS ),
	//m_pairListOldestTime(0.0f),
	m_pCallback(&g_EmptyCollisionListener) 
{
	m_pairList.SetLessFunc( CorePairLessFunc );
	memset(&m_event, 0, sizeof(m_event));
}


void CPhysicsListenerCollision::event_friction_pair_created( IVP_Friction_Core_Pair *pair )
{
	corepair_t test(pair);
	auto index = m_pairList.Find( test );
	if ( m_pairList.IsValidIndex( index ) )
	{
		corepair_t &save = m_pairList.Element(index);
		// found this one already, update the time
		if ( save.lastImpactTime.get_seconds() > pair->last_impact_time_pair.get_seconds() )
		{
			pair->last_impact_time_pair = save.lastImpactTime;
		}
		else
		{
			save.lastImpactTime = pair->last_impact_time_pair;
		}
	}
	else
	{
		if ( m_pairList.Count() < 16 )
		{
			m_pairList.Insert( test );
		}
	}
}


void CPhysicsListenerCollision::event_friction_pair_deleted( IVP_Friction_Core_Pair *pair )
{
	corepair_t test(pair);
	auto index = m_pairList.Find( test );
	if ( m_pairList.IsValidIndex( index ) )
	{
		corepair_t &save = m_pairList.Element(index);
		// found this one already, update the time
		if ( save.lastImpactTime.get_seconds() < pair->last_impact_time_pair.get_seconds() )
		{
			save.lastImpactTime = pair->last_impact_time_pair;
		}
	}
	else
	{
		if ( m_pairList.Count() < 16 )
		{
			m_pairList.Insert( test );
		}
	}
}


#if defined(IVP_ENABLE_VISUALIZER)

class CCollisionVisualizer : public IVP_Clustering_Visualizer_Shortrange_Callback, public IVP_Clustering_Visualizer_Longrange_Callback
{
	IVPhysicsDebugOverlay			*m_pDebug;
public:
	CCollisionVisualizer(IVPhysicsDebugOverlay *pDebug) { m_pDebug = pDebug;}

    void visualize_request()
	{
		Vector origin, extents;
		ConvertPositionToHL( center, origin );
		float hlradius = ConvertDistanceToHL( radius);
		extents.Init( hlradius, hlradius, hlradius );
		m_pDebug->AddBoxOverlay( origin, -extents, extents, vec3_angle, 0, 255, 0, 32, 0.5f);
	}

    virtual void      devisualize_request() {}
    virtual void      enable() {}
    virtual void      disable() {}

    void visualize_request_for_node()
	{
		Vector origin, extents;
		ConvertPositionToHL( position, origin );
		ConvertPositionToHL( box_extents, extents );

		Vector boxOrigin, boxExtents;
		CPhysicsObject *pObject0 = static_cast<CPhysicsObject *>(node_object->client_data);
		pObject0->LocalToWorld( boxOrigin, origin );
		QAngle angles;
		pObject0->GetPosition( NULL, &angles );

		m_pDebug->AddBoxOverlay( boxOrigin, -extents, extents, angles, 255, 255, 0, 0, 0.5f);
	}

    void visualize_request_for_intruder_radius()
	{
		Vector origin, extents;
		ConvertPositionToHL( position, origin );
		float hlradius = ConvertDistanceToHL( sphere_radius );

		extents.Init( hlradius, hlradius, hlradius );
		m_pDebug->AddBoxOverlay( origin, -extents, extents, vec3_angle, 0, 0, 255, 32, 0.25f);
	}
};
#endif

class CCollisionSolver : public IVP_Collision_Filter, public IVP_Anomaly_Manager
{
public:
	CCollisionSolver( void ) : IVP_Anomaly_Manager(IVP_FALSE) { m_pSolver = NULL; }
	void SetHandler( IPhysicsCollisionSolver *pSolver ) { m_pSolver = pSolver; }

	// IVP_Collision_Filter
    IVP_BOOL check_objects_for_collision_detection(IVP_Real_Object *ivp0, IVP_Real_Object *ivp1) override
	{
		DebugPrint();
		if ( m_pSolver )
		{
			CPhysicsObject *pObject0 = static_cast<CPhysicsObject *>(ivp0->client_data);
			CPhysicsObject *pObject1 = static_cast<CPhysicsObject *>(ivp1->client_data);
			if ( pObject0 && pObject1 )
			{
				if ( (pObject0->CallbackFlags() & CALLBACK_ENABLING_COLLISION) && (pObject1->CallbackFlags() & CALLBACK_MARKED_FOR_DELETE) )
					return IVP_FALSE;

				if ( (pObject1->CallbackFlags() & CALLBACK_ENABLING_COLLISION) && (pObject0->CallbackFlags() & CALLBACK_MARKED_FOR_DELETE) )
					return IVP_FALSE;

				if ( !m_pSolver->ShouldCollide( pObject0, pObject1, pObject0->GetGameData(), pObject1->GetGameData() ) )
					return IVP_FALSE;
			}
		}
		return IVP_TRUE;
	}
	void environment_will_be_deleted(IVP_Environment *) override {}

	// IVP_Anomaly_Manager
	void inter_penetration( IVP_Mindist *mindist,IVP_Real_Object *ivp0, IVP_Real_Object *ivp1, IVP_DOUBLE speedChange) override
	{
		DebugPrint();
		if ( m_pSolver )
		{
			// UNDONE: project current velocity onto rescue velocity instead
			// This will cause escapes to be slow - which is probably a good
			// thing.  That's probably a better heuristic than only rescuing once
			// per PSI!
			CPhysicsObject *pObject0 = static_cast<CPhysicsObject *>(ivp0->client_data);
			CPhysicsObject *pObject1 = static_cast<CPhysicsObject *>(ivp1->client_data);
			if ( pObject0 && pObject1 )
			{
				if ( (pObject0->CallbackFlags() & CALLBACK_MARKED_FOR_DELETE) ||
					(pObject1->CallbackFlags() & CALLBACK_MARKED_FOR_DELETE) )
					return;

				// moveable object pair?
				if ( pObject0->IsMoveable() && pObject1->IsMoveable() )
				{
					// only push each pair apart once per PSI
					if ( CheckObjPair( ivp0, ivp1 ) )
						return;
				}
				IVP_Environment *env = ivp0->get_environment();
				float deltaTime = env->get_delta_PSI_time();

				if ( !m_pSolver->ShouldSolvePenetration( pObject0, pObject1, pObject0->GetGameData(), pObject1->GetGameData(), deltaTime ) )
					return;
			}
			else
			{
				return;
			}
		}

		IVP_Anomaly_Manager::inter_penetration( mindist, ivp0, ivp1, speedChange );
	}

	// return true if object should be temp. freezed
    IVP_BOOL max_collisions_exceeded_check_freezing(IVP_Anomaly_Limits *, IVP_Core *pCore) override
	{
		DebugPrint();
		if ( m_pSolver )
		{
			CPhysicsObject *pObject = static_cast<CPhysicsObject *>(pCore->objects.element_at(0)->client_data);
			return m_pSolver->ShouldFreezeObject( pObject ) ? IVP_TRUE : IVP_FALSE;
		}
		return IVP_TRUE;
	}
	// return number of additional checks to do this psi
    virtual int max_collision_checks_exceeded( int totalChecks )
	{
		DebugPrint();
		if ( m_pSolver )
		{
			return m_pSolver->AdditionalCollisionChecksThisTick( totalChecks );
		}
		return 0;
	}
	void max_velocity_exceeded(IVP_Anomaly_Limits *al, IVP_Core *pCore, IVP_U_Float_Point *velocity_in_out) override
	{
		DebugPrint();
		CPhysicsObject *pObject = static_cast<CPhysicsObject *>(pCore->objects.element_at(0)->client_data);
		if ( pObject->GetShadowController() != NULL )
			return;
		IVP_Anomaly_Manager::max_velocity_exceeded(al, pCore, velocity_in_out);
	}
	IVP_BOOL max_contacts_exceeded_check_freezing( IVP_Core **pCoreList, int coreCount )
	{
		DebugPrint();
		CUtlVector<IPhysicsObject *> list;
		list.EnsureCapacity(coreCount);
		for ( int i = 0; i < coreCount; i++ )
		{
			IVP_Core *pCore = pCoreList[i];
			CPhysicsObject *pObject = static_cast<CPhysicsObject *>(pCore->objects.element_at(0)->client_data);
			list.AddToTail(pObject);
		}

		return m_pSolver->ShouldFreezeContacts( list.Base(), list.Count() ) ? IVP_TRUE : IVP_FALSE;
	}


public:
	void EventPSI( CPhysicsEnvironment * )
	{
		DebugPrint();
		m_rescue.RemoveAll();
	}


private:
	struct realobjectpair_t
	{
		IVP_Real_Object *pObj0;
		IVP_Real_Object *pObj1;
		inline bool operator==( const realobjectpair_t &src ) const
		{
			return (pObj0 == src.pObj0) && (pObj1 == src.pObj1);
		}
	};
	// basically each moveable object pair gets 1 rescue per PSI
	// UNDONE: Add a counter to do more?
	bool CheckObjPair( IVP_Real_Object *pObj0, IVP_Real_Object *pObj1 )
	{
		realobjectpair_t tmp;
		tmp.pObj0 = pObj0 < pObj1 ? pObj0 : pObj1;
		tmp.pObj1 = pObj0 > pObj1 ? pObj0 : pObj1;

		if ( m_rescue.Find( tmp ) != m_rescue.InvalidIndex() )
			return true;
		m_rescue.AddToTail( tmp );
		return false;
	}

public:
	inline IPhysicsCollisionSolver* GetSolver() { return m_pSolver; };

private:
	IPhysicsCollisionSolver					*m_pSolver;
	// UNDONE: Linear search? should be small, but switch to rb tree if this ever gets large
	CUtlVector<realobjectpair_t>	m_rescue;
#if defined(IVP_ENABLE_VISUALIZER)
public:
	CCollisionVisualizer *pVisualizer;
#endif
};

IPhysicsCollisionSolver* CPhysicsHolyLib::GetCollisionSolver(IPhysicsEnvironment* pEnvironment)
{
	return ((CPhysicsEnvironment*)pEnvironment)->GetCollisionSolver()->GetSolver();
}

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

	void SetHandler( IPhysicsConstraintEvent *pHandler )
	{
		m_pCallback = pHandler;
	}

    void event_constraint_broken( IVP_Constraint * ) override
	{
		// IVP_Constraint is not allowed, something is broken
		Assert(0);
	}

    void event_constraint_broken( hk_Breakable_Constraint *pConstraint ) override
	{
		DebugPrint();
		if ( m_pCallback )
		{
			IPhysicsConstraint *pObj = GetClientDataForHkConstraint( pConstraint );
			m_pCallback->ConstraintBroken( pObj );
		}
	}
	void event_constraint_broken( IPhysicsConstraint *pConstraint )
	{
		DebugPrint();
		if ( m_pCallback )
		{
			m_pCallback->ConstraintBroken(pConstraint);
		}
	}

	inline IPhysicsConstraintEvent* GetCallback() { return m_pCallback; };

private:
	IPhysicsConstraintEvent *m_pCallback;
};

IPhysicsConstraintEvent* CPhysicsHolyLib::GetPhysicsListenerConstraint(IPhysicsEnvironment* pEnvironment)
{
	return ((CPhysicsEnvironment*)pEnvironment)->GetPhysicsListenerConstraint()->GetCallback();
}

#define AIR_DENSITY	2

class CDragController : public IVP_Controller_Independent
{
public:

	CDragController( void ) 
	{
		m_airDensity = AIR_DENSITY;
	}
	virtual ~CDragController( void ) {}

    void do_simulation_controller(IVP_Event_Sim *event,IVP_U_Vector<IVP_Core> *core_list) override
	{
		DebugPrint();
		int i;
		for( i = core_list->len()-1; i >=0; i--) 
		{
			IVP_Core *pCore = core_list->element_at(i);

			IVP_Real_Object *pivp = pCore->objects.element_at(0);
			CPhysicsObject *pPhys = static_cast<CPhysicsObject *>(pivp->client_data);
			
			float dragForce = -0.5f * pPhys->GetDragInDirection( pCore->speed ) * m_airDensity * event->delta_time;
			if ( dragForce < -1.0f )
				dragForce = -1.0f;
			if ( dragForce < 0 )
			{
				IVP_U_Float_Point dragVelocity;
				dragVelocity.set_multiple( &pCore->speed, dragForce );
				pCore->speed.add( &dragVelocity );
			}
			float angDragForce = -pPhys->GetAngularDragInDirection( pCore->rot_speed ) * m_airDensity * event->delta_time;
			if ( angDragForce < -1.0f )
				angDragForce = -1.0f;
			if ( angDragForce < 0 )
			{
				IVP_U_Float_Point angDragVelocity;
				angDragVelocity.set_multiple( &pCore->rot_speed, angDragForce );
				pCore->rot_speed.add( &angDragVelocity );
			}
		}
	}
	const char *get_controller_name() override { return "vphysics:drag"; }
    
	IVP_CONTROLLER_PRIORITY get_controller_priority() override
	{ 
		return IVP_CP_MOTION;
	}
	float GetAirDensity() const { return m_airDensity; }
	void SetAirDensity( float density ) { m_airDensity = density; }

private:
	float	m_airDensity;
};

//
// Default implementation of the debug overlay interface so that we never return NULL from GetDebugOverlay.
//
class CVPhysicsDebugOverlay : public IVPhysicsDebugOverlay
{
public:
	void AddEntityTextOverlay(int, int, float, int, int, int, int, const char *, ...) override {}
	void AddBoxOverlay(const Vector&, const Vector&, const Vector&, QAngle const&, int, int, int, int, float) override {}
	void AddTriangleOverlay(const Vector&, const Vector&, const Vector&, int, int, int, int, bool, float) override {}
	void AddLineOverlay(const Vector&, const Vector&, int, int, int, bool, float) override {}
	void AddTextOverlay(const Vector&, float, const char *, ...) override {}
	void AddTextOverlay(const Vector&, int, float, const char *, ...) override {}
	void AddScreenTextOverlay(float, float,float, int, int, int, int, const char *) override {}
	void AddSweptBoxOverlay(const Vector&, const Vector&, const Vector&, const Vector&, const QAngle &, int, int, int, int, float) override {}
	void AddTextOverlayRGB(const Vector&, int, float, float, float, float, float, const char *, ...) override {}
};

static CVPhysicsDebugOverlay s_DefaultDebugOverlay;


CPhysicsEnvironment::CPhysicsEnvironment( void )
// assume that these lists will have at least one object
{
	m_pDebugOverlay = nullptr;
	// set this to true to force the 
	m_deleteQuick = false;
	m_queueDeleteObject = false;
	m_inSimulation = false;
	m_fixedTimestep = true;	// try to simulate using fixed timesteps
	m_enableConstraintNotify = false;

	// build a default environment
	IVP_Environment_Manager *env_manager =
		IVP_Environment_Manager::get_environment_manager();
	
	BEGIN_IVP_ALLOCATION();
	m_pCollisionSolver = new CCollisionSolver;
	END_IVP_ALLOCATION();

	IVP_Application_Environment appl_env;
	appl_env.collision_filter = m_pCollisionSolver;
	appl_env.material_manager = physprops->GetIVPManager();
	appl_env.anomaly_manager = m_pCollisionSolver;

	BEGIN_IVP_ALLOCATION();
	m_pPhysEnv = env_manager->create_environment( &appl_env, "JAY", 0xBEEF );
	END_IVP_ALLOCATION();

	// UNDONE: Revisit brush/terrain/object shrinking and tune this number to something larger
	// UNDONE: Expose this to callers, also via physcollision
	m_pPhysEnv->set_global_collision_tolerance( ConvertDistanceToIVP( g_PhysicsUnits.globalCollisionTolerance - 1e-4f ) );	// just under 1/4 inch tolerance
	m_pSleepEvents = new CSleepObjects;

	m_pDeleteQueue = new CDeleteQueue;

	BEGIN_IVP_ALLOCATION();
	m_pPhysEnv->add_listener_object_global( m_pSleepEvents );
	END_IVP_ALLOCATION();

	m_pCollisionListener = new CPhysicsListenerCollision;
	
	BEGIN_IVP_ALLOCATION();
	m_pPhysEnv->add_listener_collision_global( m_pCollisionListener );
	END_IVP_ALLOCATION();

	m_pConstraintListener = new CPhysicsListenerConstraint;

	BEGIN_IVP_ALLOCATION();
	m_pPhysEnv->add_listener_constraint_global( m_pConstraintListener );
	END_IVP_ALLOCATION();

	m_pDragController = new CDragController;

	physics_performanceparams_t perf;
	perf.Defaults();
	CPhysicsEnvironment::SetPerformanceSettings( &perf );
	m_pPhysEnv->client_data = this;
	m_lastObjectThisTick = 0;

	if (g_pHolyLibCallbacks)
	{
		g_pHolyLibCallbacks->OnEnvironmentCreated( this );
	}
}

CPhysicsEnvironment::~CPhysicsEnvironment( void )
{
	if (g_pHolyLibCallbacks)
	{
		g_pHolyLibCallbacks->OnEnvironmentDestroyed( this );
	}

	// no callbacks during shutdown
	SetCollisionSolver( NULL );
	m_pPhysEnv->remove_listener_object_global( m_pSleepEvents );

	// don't bother waking up other objects as we clear them out
	SetQuickDelete( true );

	// delete/remove the listeners
	m_pPhysEnv->remove_listener_collision_global( m_pCollisionListener );
	m_pPhysEnv->remove_listener_constraint_global( m_pConstraintListener );

	// Clean out the list of physics objects
	for ( int i = m_objects.Count()-1; i >= 0; --i )
	{
		CPhysicsObject *pObject = static_cast<CPhysicsObject *>(m_objects[i]);
		PhantomRemove( pObject );
		delete pObject;
	}

	m_objects.RemoveAll();
	ClearDeadObjects();

	// Clean out the list of fluids
	m_fluids.PurgeAndDeleteElements();
	
	delete m_pDragController;
	delete m_pConstraintListener;
	delete m_pCollisionListener;
	delete m_pDeleteQueue;
	delete m_pSleepEvents;
	delete m_pPhysEnv;

	// must be deleted after the environment (calls back in destructor)
	delete m_pCollisionSolver;
}

IPhysicsCollisionEvent *CPhysicsEnvironment::GetCollisionEventHandler() 
{ 
	DebugPrint();
	return m_pCollisionListener->GetHandler(); 
}

void CPhysicsEnvironment::NotifyConstraintDisabled( IPhysicsConstraint *pConstraint )
{
	DebugPrint();
	if ( m_enableConstraintNotify )
	{
		m_pConstraintListener->event_constraint_broken( pConstraint );
	}
}

void CPhysicsEnvironment::DebugCheckContacts(void)
{
	DebugPrint();
	if ( m_pSleepEvents )
	{
		m_pSleepEvents->DebugCheckContacts( m_pPhysEnv );
	}
}

void CPhysicsEnvironment::SetDebugOverlay( CreateInterfaceFn debugOverlayFactory )
{
	DebugPrint();
	m_pDebugOverlay = NULL;
	if (debugOverlayFactory)
	{
		//m_pDebugOverlay = ( IVPhysicsDebugOverlay * )debugOverlayFactory( VPHYSICS_DEBUG_OVERLAY_INTERFACE_VERSION, NULL );
	}

	if (!m_pDebugOverlay)
	{
		m_pDebugOverlay = &s_DefaultDebugOverlay;
	}

#if defined(IVP_ENABLE_VISUALIZER)
	m_pCollisionSolver->pVisualizer = new CCollisionVisualizer( m_pDebugOverlay );
	INSTALL_SHORTRANGE_CALLBACK(m_pCollisionSolver->pVisualizer);
	INSTALL_LONGRANGE_CALLBACK(m_pCollisionSolver->pVisualizer);
#endif
}


IVPhysicsDebugOverlay *CPhysicsEnvironment::GetDebugOverlay( void )
{
	DebugPrint();
	return m_pDebugOverlay;
}


void CPhysicsEnvironment::SetGravity( const Vector& gravityVector )
{
	DebugPrint();
    IVP_U_Point gravity; 

	ConvertPositionToIVP( gravityVector, gravity );
    m_pPhysEnv->set_gravity( &gravity );
	// BUGBUG: global collision tolerance has a constant that depends on gravity.
	m_pPhysEnv->set_global_collision_tolerance( m_pPhysEnv->get_global_collision_tolerance(), gravity.real_length() );
	DevMsg(1,"Set Gravity %.1f (%.3f tolerance)\n", gravityVector.Length(), IVP2HL(m_pPhysEnv->get_global_collision_tolerance()) );
}


void CPhysicsEnvironment::GetGravity( Vector *pGravityVector ) const
{
	DebugPrint();
    const IVP_U_Point *gravity = m_pPhysEnv->get_gravity();

	ConvertPositionToHL( *gravity, *pGravityVector );
}


IPhysicsObject *CPhysicsEnvironment::CreatePolyObject( const CPhysCollide *pCollisionModel, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t *pParams )
{
	DebugPrint();
	IPhysicsObject *pObject = ::CreatePhysicsObject( this, pCollisionModel, materialIndex, position, angles, pParams, false );
	if ( pObject )
	{
		m_objects.AddToTail( pObject );
	}
	return pObject;
}

IPhysicsObject *CPhysicsEnvironment::CreatePolyObjectStatic( const CPhysCollide *pCollisionModel, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t *pParams )
{
	DebugPrint();
	DebugMsg2("Name: %s\n", pParams->pName);
	IPhysicsObject *pObject = ::CreatePhysicsObject( this, pCollisionModel, materialIndex, position, angles, pParams, true );
	if ( pObject )
	{
		m_objects.AddToTail( pObject );
	}
	return pObject;
}

unsigned int CPhysicsEnvironment::GetObjectSerializeSize( IPhysicsObject * ) const
{
	DebugPrint();
	return sizeof(vphysics_save_cphysicsobject_t);
}

void CPhysicsEnvironment::SerializeObjectToBuffer( IPhysicsObject *pObject, unsigned char *pBuffer, unsigned int bufferSize )
{
	DebugPrint();
	CPhysicsObject *pPhysics = static_cast<CPhysicsObject *>(pObject);
	if ( bufferSize >= sizeof(vphysics_save_cphysicsobject_t))
	{
		vphysics_save_cphysicsobject_t *pTemplate = reinterpret_cast<vphysics_save_cphysicsobject_t *>(pBuffer);
		pPhysics->WriteToTemplate( *pTemplate );
	}
}

IPhysicsObject *CPhysicsEnvironment::UnserializeObjectFromBuffer( void *pGameData, unsigned char *pBuffer, unsigned int bufferSize, bool enableCollisions )
{
	DebugPrint();
	IPhysicsObject *pObject = ::CreateObjectFromBuffer( this, pGameData, pBuffer, bufferSize, enableCollisions );
	if ( pObject )
	{
		m_objects.AddToTail( pObject );
	}
	return pObject;
}

const IPhysicsObject **CPhysicsEnvironment::GetObjectList( int *pOutputObjectCount ) const
{
	DebugPrint();
	int iCount = m_objects.Count();
	if( pOutputObjectCount ) 
		*pOutputObjectCount = iCount;

	if( iCount )
		return (const IPhysicsObject **)m_objects.Base();
	else
		return NULL;
}



extern void ControlPhysicsShadowControllerAttachment_Silent( IPhysicsShadowController *pController, IVP_Real_Object *pivp, bool bAttach );
extern void ControlPhysicsPlayerControllerAttachment_Silent( IPhysicsPlayerController *pController, IVP_Real_Object *pivp, bool bAttach );

bool CPhysicsEnvironment::TransferObject( IPhysicsObject *pObject, IPhysicsEnvironment *pDestinationEnvironment )
{
	DebugPrint();
	int iIndex = m_objects.Find( pObject );
	if( iIndex == -1 || (pObject->GetCallbackFlags() & CALLBACK_MARKED_FOR_DELETE ) )
		return false;

	CPhysicsObject *pPhysics = static_cast<CPhysicsObject *>(pObject);
	//pPhysics->Wake();
	//pPhysics->NotifyWake();

	void *pGameData = pObject->GetGameData();

	//Find any controllers attached to this object
	IPhysicsShadowController *pController = pObject->GetShadowController();
	IPhysicsPlayerController *pPlayerController = NULL;

	if( (pObject->GetCallbackFlags() & CALLBACK_IS_PLAYER_CONTROLLER) != 0 )
	{
		pPlayerController = FindPlayerController( pObject );
	}




	//temporarily (and silently) detach any physics controllers we found because destroying the object would destroy them
	if( pController )
	{
		//detach the controller from the object
		((CPhysicsObject *)pObject)->m_pShadow = NULL;

		IVP_Real_Object *pivp = ((CPhysicsObject *)pObject)->GetObject();
		ControlPhysicsShadowControllerAttachment_Silent( pController, pivp, false );
	}
	else if( pPlayerController )
	{
		RemovePlayerController( pPlayerController );		
		pObject->SetCallbackFlags( pObject->GetCallbackFlags() & ~CALLBACK_IS_PLAYER_CONTROLLER );

		IVP_Real_Object *pivp = ((CPhysicsObject *)pObject)->GetObject();
		ControlPhysicsPlayerControllerAttachment_Silent( pPlayerController, pivp, false );
	}
	

	//templatize the object
	vphysics_save_cphysicsobject_t objectTemplate{};
	pPhysics->WriteToTemplate( objectTemplate );

	//these should be detached already
	Assert( objectTemplate.pShadow == NULL );
	Assert( objectTemplate.hasShadowController == false );

	//destroy the existing version of the object
	m_objects.FastRemove( iIndex );
	pPhysics->ForceSilentDelete();
	m_pSleepEvents->DeleteObject( pPhysics );
	pPhysics->CPhysicsObject::~CPhysicsObject();
	
	//now recreate in place in the destination environment
	CPhysicsEnvironment *pDest = static_cast<CPhysicsEnvironment *>(pDestinationEnvironment);
	CreateObjectFromBuffer_UseExistingMemory( pDest, pGameData, (unsigned char *)&objectTemplate, sizeof(objectTemplate), pPhysics );
	pDest->m_objects.AddToTail( pObject );

	//even if this is going to sleep in a second, put it active right away to fix some object hitching problems
	pPhysics->Wake();
	pPhysics->NotifyWake();
	/*int iActiveIndex = pDest->m_pSleepEvents->m_activeObjects.AddToTail( pPhysics );
	pPhysics->SetActiveIndex( iActiveIndex );*/
	
	pDest->m_pPhysEnv->force_psi_on_next_simulation(); //avoids an object pause

	if( pController )
	{
		//re-attach the controller to the new object
		((CPhysicsObject *)pObject)->m_pShadow = pController;

		IVP_Real_Object *pivp = ((CPhysicsObject *)pObject)->GetObject();
		ControlPhysicsShadowControllerAttachment_Silent( pController, pivp, true );		
	}
	else if( pPlayerController )
	{
		IVP_Real_Object *pivp = ((CPhysicsObject *)pObject)->GetObject();
		pObject->SetCallbackFlags( pObject->GetCallbackFlags() | CALLBACK_IS_PLAYER_CONTROLLER );
		ControlPhysicsPlayerControllerAttachment_Silent( pPlayerController, pivp, true );		

		pDest->AddPlayerController( pPlayerController );
	}
	
    return true;	
}

int GlobalSCOPE = 0;

IPhysicsSpring *CPhysicsEnvironment::CreateSpring( IPhysicsObject *pObjectStart, IPhysicsObject *pObjectEnd, springparams_t *pParams )
{
	DebugPrint();
	return ::CreateSpring( m_pPhysEnv, static_cast<CPhysicsObject *>(pObjectStart), static_cast<CPhysicsObject *>(pObjectEnd), pParams );
}

IPhysicsFluidController *CPhysicsEnvironment::CreateFluidController( IPhysicsObject *pFluidObject, fluidparams_t *pParams )
{
	DebugPrint();
	CPhysicsFluidController *pFluid = ::CreateFluidController( m_pPhysEnv, static_cast<CPhysicsObject *>(pFluidObject), pParams );
	m_fluids.AddToTail( pFluid );
	return pFluid;
}

IPhysicsConstraint *CPhysicsEnvironment::CreateRagdollConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ragdollparams_t &ragdoll )
{
	DebugPrint();
	return ::CreateRagdollConstraint( m_pPhysEnv, (CPhysicsObject *)pReferenceObject, (CPhysicsObject *)pAttachedObject, pGroup, ragdoll );
}

IPhysicsConstraint *CPhysicsEnvironment::CreateHingeConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_hingeparams_t &hinge )
{
	DebugPrint();
	constraint_limitedhingeparams_t limitedhinge(hinge);
	return ::CreateHingeConstraint( m_pPhysEnv, (CPhysicsObject *)pReferenceObject, (CPhysicsObject *)pAttachedObject, pGroup, limitedhinge );
}

IPhysicsConstraint *CPhysicsEnvironment::CreateLimitedHingeConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_limitedhingeparams_t &hinge )
{
	DebugPrint();
	return ::CreateHingeConstraint( m_pPhysEnv, (CPhysicsObject *)pReferenceObject, (CPhysicsObject *)pAttachedObject, pGroup, hinge );
}

IPhysicsConstraint *CPhysicsEnvironment::CreateFixedConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_fixedparams_t &fixed )
{
	DebugPrint();
	return ::CreateFixedConstraint( m_pPhysEnv, (CPhysicsObject *)pReferenceObject, (CPhysicsObject *)pAttachedObject, pGroup, fixed );
}

IPhysicsConstraint *CPhysicsEnvironment::CreateSlidingConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_slidingparams_t &sliding )
{
	DebugPrint();
	return ::CreateSlidingConstraint( m_pPhysEnv, (CPhysicsObject *)pReferenceObject, (CPhysicsObject *)pAttachedObject, pGroup, sliding );
}

IPhysicsConstraint *CPhysicsEnvironment::CreateBallsocketConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ballsocketparams_t &ballsocket )
{
	DebugPrint();
	return ::CreateBallsocketConstraint( m_pPhysEnv, (CPhysicsObject *)pReferenceObject, (CPhysicsObject *)pAttachedObject, pGroup, ballsocket );
}

IPhysicsConstraint *CPhysicsEnvironment::CreatePulleyConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_pulleyparams_t &pulley )
{
	DebugPrint();
	return ::CreatePulleyConstraint( m_pPhysEnv, (CPhysicsObject *)pReferenceObject, (CPhysicsObject *)pAttachedObject, pGroup, pulley );
}

IPhysicsConstraint *CPhysicsEnvironment::CreateLengthConstraint( IPhysicsObject *pReferenceObject, IPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_lengthparams_t &length )
{
	DebugPrint();
	return ::CreateLengthConstraint( m_pPhysEnv, (CPhysicsObject *)pReferenceObject, (CPhysicsObject *)pAttachedObject, pGroup, length );
}

IPhysicsConstraintGroup *CPhysicsEnvironment::CreateConstraintGroup( const constraint_groupparams_t &group )
{
	DebugPrint();
	return CreatePhysicsConstraintGroup( m_pPhysEnv, group );
}

void CPhysicsEnvironment::Simulate( float deltaTime )
{
	DebugPrint();
	LOCAL_THREAD_LOCK();

	if ( !m_pPhysEnv )
		return;

	ClearDeadObjects();
#if defined(DEBUG_CHECK_CONTATCTS_AUTO)
	m_pSleepEvents->DebugCheckContacts( m_pPhysEnv );
#endif

	// save this to catch objects deleted without being simulated
	m_lastObjectThisTick = m_objects.Count()-1;

	// stop updating objects that went to sleep during the previous frame.
	m_pSleepEvents->UpdateSleepObjects();

	// Trap interrupts and clock changes
	// don't simulate less than .1 ms
	if ( deltaTime <= 1.0 && deltaTime > 0.0001 )
	{
		if ( deltaTime > 0.1 )
		{
			deltaTime = 0.1f;
		}

		m_pCollisionSolver->EventPSI( this );
		m_pCollisionListener->EventPSI( this );

		if (g_pHolyLibCallbacks)
		{
			g_pHolyLibCallbacks->SimulationBegin();
		}
		m_inSimulation = true;
		BEGIN_IVP_ALLOCATION();
		if ( !m_fixedTimestep || !hk_Math::almost_equal(deltaTime, m_pPhysEnv->get_delta_PSI_time()) )
		{
			m_fixedTimestep = false;
			m_pPhysEnv->simulate_dtime( deltaTime );
		}
		else
		{
			m_pPhysEnv->simulate_time_step();
		}
		END_IVP_ALLOCATION();
		m_inSimulation = false;
		if (g_pHolyLibCallbacks)
		{
			g_pHolyLibCallbacks->SimulationFinish();
		}
	}

	// If the queue is disabled, it's only used during simulation.
	// Flush it as soon as possible (which is now)
	if ( !m_queueDeleteObject )
	{
		ClearDeadObjects();
	}

	if ( m_pCollisionListener->GetHandler() )
	{
		m_pSleepEvents->ProcessActiveObjects( m_pPhysEnv, m_pCollisionListener->GetHandler() );
	}
	//visualize_collisions();
	VirtualMeshPSI();
	// dimhotepus: Comment pure API. 
	// GetNextFrameTime();
}

void CPhysicsEnvironment::ResetSimulationClock()
{
	DebugPrint();
	// UNDONE: You'd think that all of this would make the system deterministic, but
	// it doesn't.
	extern void SeedRandomGenerators();

	m_pPhysEnv->reset_time();
	m_pPhysEnv->get_time_manager()->env_set_current_time( m_pPhysEnv, IVP_Time(0) );
	m_pPhysEnv->reset_time();
	m_fixedTimestep = true;
	SeedRandomGenerators();
}

float CPhysicsEnvironment::GetSimulationTimestep( void ) const
{
	DebugPrint();
	return m_pPhysEnv->get_delta_PSI_time();
}

void CPhysicsEnvironment::SetSimulationTimestep( float timestep )
{
	DebugPrint();
	m_pPhysEnv->set_delta_PSI_time( timestep );
}

float CPhysicsEnvironment::GetSimulationTime( void ) const
{
	DebugPrint();
	return (float)m_pPhysEnv->get_current_time().get_time();
}

float CPhysicsEnvironment::GetNextFrameTime( void ) const
{
	DebugPrint();
	return (float)m_pPhysEnv->get_next_PSI_time().get_time();
}


// true if currently running the simulator (i.e. in a callback during physenv->Simulate())
bool CPhysicsEnvironment::IsInSimulation( void ) const
{
	DebugPrint();
	return m_inSimulation;
}

void CPhysicsEnvironment::DestroyObject( IPhysicsObject *pObject )
{
	DebugPrint();
	if ( !pObject )
	{
		DevMsg("Deleted NULL vphysics object\n");
		return;
	}

	// search from the end because we usually delete the most recent objects during run time
	int index = -1;
	for ( int i = m_objects.Count(); --i >= 0; )
	{
		if ( m_objects[i] == pObject )
		{
			index = i;
			break;
		}
	}

	if ( index != -1 )
	{
		m_objects.FastRemove( index );
	}
	else
	{
		DevMsg(1,"error deleting physics object\n");
		CPhysicsObject *pPhysics = static_cast<CPhysicsObject *>(pObject);
		if ( pPhysics->GetCallbackFlags() & CALLBACK_MARKED_FOR_DELETE )
		{
			// deleted twice
			Assert(0);
			return;
		}
		// bad ptr?
		Assert(0);
		return;
	}

	CPhysicsObject *pPhysics = static_cast<CPhysicsObject *>(pObject);
	// add this flag so we can optimize some cases
	pPhysics->AddCallbackFlags( CALLBACK_MARKED_FOR_DELETE );
	
	// was created/destroyed without simulating.  No need to wake the neighbors!
	if ( index > m_lastObjectThisTick )
	{
		pPhysics->ForceSilentDelete();
	}

	if ( m_inSimulation || m_queueDeleteObject )
	{
		for ( int i = 0; i < m_deadObjects.Count(); i++ )
		{
			auto *pObj = (CPhysicsObject *)m_deadObjects.Element(i);

			if (pObj == pObject)
			{
				Warning("Adding the same object to the deleteObjects list MULTIPLE TIMES!\n");
				break;
			}
		}

		// don't delete while simulating
		m_deadObjects.AddToTail( pObject );
	}
	else
	{
		m_pSleepEvents->DeleteObject( pPhysics );
		delete pObject;
	}
}

void CPhysicsEnvironment::DestroySpring( IPhysicsSpring *pSpring )
{
	DebugPrint();
	delete pSpring;
}
void CPhysicsEnvironment::DestroyFluidController( IPhysicsFluidController *pFluid )
{
	DebugPrint();
	m_fluids.FindAndRemove( (CPhysicsFluidController *)pFluid );
	delete pFluid;
}


void CPhysicsEnvironment::DestroyConstraint( IPhysicsConstraint *pConstraint )
{
	DebugPrint();
	if ( !m_deleteQuick && pConstraint )
	{
		IPhysicsObject *pObj0 = pConstraint->GetReferenceObject();
		if ( pObj0 )
		{
			pObj0->Wake();
		}

		IPhysicsObject *pObj1 = pConstraint->GetAttachedObject();
		if ( pObj1 )
		{
			pObj1->Wake();
		}
	}
	if ( m_inSimulation )
	{
		pConstraint->Deactivate();
		m_pDeleteQueue->QueueForDelete( pConstraint );
	}
	else
	{
		delete pConstraint;
	}
}

void CPhysicsEnvironment::DestroyConstraintGroup( IPhysicsConstraintGroup *pGroup )
{
	DebugPrint();
	delete pGroup;
}

void CPhysicsEnvironment::TraceBox( trace_t *, const Vector &, const Vector &, const Vector &, const Vector & )
{
	DebugPrint();
	// UNDONE: Need this?
}

void CPhysicsEnvironment::SetCollisionSolver( IPhysicsCollisionSolver *pSolver )
{
	DebugPrint();
	m_pCollisionSolver->SetHandler( pSolver );
}


void CPhysicsEnvironment::ClearDeadObjects( void )
{
	DebugPrint();
	// dimhotepus: Can't use range-for loop here as DeleteObject may recursively
	// call ClearDeadObjects and m_deadObjects will be modified when iterating.
	for ( int i = 0; i < m_deadObjects.Count(); i++ )
	{
		auto *pObject = (CPhysicsObject *)m_deadObjects.Element(i);

		m_pSleepEvents->DeleteObject( pObject );
		delete pObject;
	}
	m_deadObjects.Purge();
	m_pDeleteQueue->DeleteAll();
}

void CPhysicsEnvironment::AddPlayerController( IPhysicsPlayerController *pController )
{
	DebugPrint();
	if ( m_playerControllers.Find(pController) != -1 )
	{
		Assert(0);
		return;
	}
	m_playerControllers.AddToTail( pController );
}

void CPhysicsEnvironment::RemovePlayerController( IPhysicsPlayerController *pController )
{
	DebugPrint();
	m_playerControllers.FindAndRemove( pController );
}

IPhysicsPlayerController *CPhysicsEnvironment::FindPlayerController( IPhysicsObject *pPhysicsObject )
{
	DebugPrint();
	for ( int i = m_playerControllers.Count()-1; i >= 0; --i )
	{
		if ( m_playerControllers[i]->GetObject() == pPhysicsObject )
			return m_playerControllers[i];
	}
	return NULL;
}


void CPhysicsEnvironment::SetCollisionEventHandler( IPhysicsCollisionEvent *pCollisionEvents )
{
	DebugPrint();
	m_pCollisionListener->SetHandler( pCollisionEvents );
}


void CPhysicsEnvironment::SetObjectEventHandler( IPhysicsObjectEvent *pObjectEvents )
{
	DebugPrint();
	m_pSleepEvents->SetHandler( pObjectEvents );
}

void CPhysicsEnvironment::SetConstraintEventHandler( IPhysicsConstraintEvent *pConstraintEvents )
{
	DebugPrint();
	m_pConstraintListener->SetHandler( pConstraintEvents );
}


IPhysicsShadowController *CPhysicsEnvironment::CreateShadowController( IPhysicsObject *pObject, bool allowTranslation, bool allowRotation )
{
	DebugPrint();
	return ::CreateShadowController( static_cast<CPhysicsObject*>(pObject), allowTranslation, allowRotation );
}

void CPhysicsEnvironment::DestroyShadowController( IPhysicsShadowController *pController )
{
	DebugPrint();
	delete pController;
}

IPhysicsPlayerController *CPhysicsEnvironment::CreatePlayerController( IPhysicsObject *pObject )
{
	DebugPrint();
	IPhysicsPlayerController *pController = ::CreatePlayerController( static_cast<CPhysicsObject*>(pObject) );
	AddPlayerController( pController );
	return pController;
}

void CPhysicsEnvironment::DestroyPlayerController( IPhysicsPlayerController *pController )
{
	DebugPrint();
	RemovePlayerController( pController );
	::DestroyPlayerController( pController );
}

IPhysicsMotionController *CPhysicsEnvironment::CreateMotionController( IMotionEvent *pHandler )
{
	DebugPrint();
	return ::CreateMotionController( this, pHandler );
}

void CPhysicsEnvironment::DestroyMotionController( IPhysicsMotionController *pController )
{
	DebugPrint();
	delete pController;
}

IPhysicsVehicleController *CPhysicsEnvironment::CreateVehicleController( IPhysicsObject *pVehicleBodyObject, const vehicleparams_t &params, unsigned int nVehicleType, IPhysicsGameTrace *pGameTrace )
{
	DebugPrint();
	return ::CreateVehicleController( this, static_cast<CPhysicsObject*>(pVehicleBodyObject), params, nVehicleType, pGameTrace );
}

void CPhysicsEnvironment::DestroyVehicleController( IPhysicsVehicleController *pController )
{
	DebugPrint();
	delete pController;
}

int	CPhysicsEnvironment::GetActiveObjectCount( void ) const
{
	DebugPrint();
	return m_pSleepEvents->GetActiveObjectCount();
}


void CPhysicsEnvironment::GetActiveObjects( IPhysicsObject **pOutputObjectList ) const
{
	DebugPrint();
	m_pSleepEvents->GetActiveObjects( pOutputObjectList );
}

void CPhysicsEnvironment::SetAirDensity( float density )
{
	DebugPrint();
	CDragController *pDrag = ((CDragController *)m_pDragController);
	if ( pDrag )
	{
		pDrag->SetAirDensity( density );
	}
}

float CPhysicsEnvironment::GetAirDensity( void ) const
{
	DebugPrint();
	const CDragController *pDrag = ((CDragController *)m_pDragController);
	if ( pDrag )
	{
		return pDrag->GetAirDensity();
	}
	return 0;
}

void CPhysicsEnvironment::CleanupDeleteList()
{
	DebugPrint();
	ClearDeadObjects();
}

bool CPhysicsEnvironment::IsCollisionModelUsed( CPhysCollide *pCollide ) const
{
	DebugPrint();
	for ( int i = m_deadObjects.Count()-1; i >= 0; --i )
	{
		if ( m_deadObjects[i]->GetCollide() == pCollide )
			return true;
	}
	
	for ( int i = m_objects.Count()-1; i >= 0; --i )
	{
		if ( m_objects[i]->GetCollide() == pCollide )
			return true;
	}

	return false;
}


// manage phantoms
void CPhysicsEnvironment::PhantomAdd( CPhysicsObject *pObject )
{
	DebugPrint();
	IVP_Controller_Phantom *pPhantom = pObject->GetObject()->get_controller_phantom();
	if ( pPhantom )
	{
		pPhantom->add_listener_phantom( m_pCollisionListener );
	}
}

void CPhysicsEnvironment::PhantomRemove( CPhysicsObject *pObject )
{
	DebugPrint();
	IVP_Controller_Phantom *pPhantom = pObject->GetObject()->get_controller_phantom();
	if ( pPhantom )
	{
		pPhantom->remove_listener_phantom( m_pCollisionListener );
	}
}


//-------------------------------------

IPhysicsObject *CPhysicsEnvironment::CreateSphereObject( float radius, int materialIndex, const Vector& position, const QAngle& angles, objectparams_t *pParams, bool isStatic )
{
	DebugPrint();
	IPhysicsObject *pObject = ::CreatePhysicsSphere( this, radius, materialIndex, position, angles, pParams, isStatic );
	m_objects.AddToTail( pObject );
	return pObject;
}

void CPhysicsEnvironment::TraceRay( const Ray_t &, unsigned int, IPhysicsTraceFilter *, trace_t * )
{
	DebugPrint();
}

void CPhysicsEnvironment::SweepCollideable( const CPhysCollide *, const Vector &, const Vector &, 
		const QAngle &, unsigned int, IPhysicsTraceFilter *, trace_t * )
{
	DebugPrint();
}


void CPhysicsEnvironment::GetPerformanceSettings( physics_performanceparams_t *pOutput ) const
{
	DebugPrint();
	if ( !pOutput )
		return;

	IVP_Anomaly_Limits *limits = m_pPhysEnv->get_anomaly_limits();
	if ( limits  )
	{
		// UNDONE: Expose these values for tuning
		pOutput->maxVelocity = ConvertDistanceToHL( limits->max_velocity );
		pOutput->maxAngularVelocity = ConvertAngleToHL(limits->max_angular_velocity_per_psi) * m_pPhysEnv->get_inv_delta_PSI_time();
		pOutput->maxCollisionsPerObjectPerTimestep = limits->max_collisions_per_psi;
		pOutput->maxCollisionChecksPerTimestep = limits->max_collision_checks_per_psi;
		pOutput->minFrictionMass = limits->min_friction_mass;
		pOutput->maxFrictionMass = limits->max_friction_mass;
	}

	IVP_Range_Manager *range = m_pPhysEnv->get_range_manager();
	if ( range )
	{
		pOutput->lookAheadTimeObjectsVsWorld = range->look_ahead_time_world;
		pOutput->lookAheadTimeObjectsVsObject = range->look_ahead_time_intra;
	}
}

void CPhysicsEnvironment::SetPerformanceSettings( const physics_performanceparams_t *pSettings )
{
	DebugPrint();
	if ( !pSettings )
		return;

	IVP_Anomaly_Limits *limits = m_pPhysEnv->get_anomaly_limits();
	if ( limits  )
	{
		// UNDONE: Expose these values for tuning
		limits->max_velocity = ConvertDistanceToIVP( pSettings->maxVelocity );
		limits->max_collisions_per_psi = pSettings->maxCollisionsPerObjectPerTimestep;
		limits->max_collision_checks_per_psi = pSettings->maxCollisionChecksPerTimestep;
		limits->max_angular_velocity_per_psi = ConvertAngleToIVP(pSettings->maxAngularVelocity) * m_pPhysEnv->get_delta_PSI_time();
		limits->min_friction_mass = clamp(pSettings->minFrictionMass, 1.0f, VPHYSICS_MAX_MASS );
		limits->max_friction_mass = clamp(pSettings->maxFrictionMass, 1.0f, VPHYSICS_MAX_MASS );
	}

	IVP_Range_Manager *range = m_pPhysEnv->get_range_manager();
	if ( range )
	{
		range->look_ahead_time_world = pSettings->lookAheadTimeObjectsVsWorld;
		range->look_ahead_time_intra = pSettings->lookAheadTimeObjectsVsObject;
	}
}


// perf/cost statistics
void CPhysicsEnvironment::ReadStats( physics_stats_t *pOutput )
{
	DebugPrint();
	if ( !pOutput )
		return;

	IVP_Statistic_Manager *stats = m_pPhysEnv->get_statistic_manager();
	pOutput->maxRescueSpeed = ConvertDistanceToHL( stats->max_rescue_speed );
	pOutput->maxSpeedGain = ConvertDistanceToHL( stats->max_speed_gain );
	pOutput->impactSysNum = stats->impact_sys_num;
	pOutput->impactCounter = stats->impact_counter;
	pOutput->impactSumSys = stats->impact_sum_sys;
	pOutput->impactHardRescueCount = stats->impact_hard_rescue_counter;
	pOutput->impactRescueAfterCount = stats->impact_rescue_after_counter;
	
	pOutput->impactDelayedCount = stats->impact_delayed_counter;
	pOutput->impactCollisionChecks = stats->impact_coll_checks;
	pOutput->impactStaticCount = stats->impact_unmov;

	pOutput->totalEnergyDestroyed = stats->sum_energy_destr;
	pOutput->collisionPairsTotal = stats->sum_of_mindists;
	pOutput->collisionPairsCreated = stats->mindists_generated;
	pOutput->collisionPairsDestroyed = stats->mindists_deleted;

	pOutput->potentialCollisionsObjectVsObject = stats->range_intra_exceeded;
	pOutput->potentialCollisionsObjectVsWorld = stats->range_world_exceeded;

	pOutput->frictionEventsProcessed = stats->processed_fmindists;
}

void CPhysicsEnvironment::ClearStats()
{
	DebugPrint();
	IVP_Statistic_Manager *stats = m_pPhysEnv->get_statistic_manager();
	stats->clear_statistic();
}

void CPhysicsEnvironment::EnableConstraintNotify( bool bEnable )
{
	DebugPrint();
	m_enableConstraintNotify = bEnable;
}

#if PLATFORM_64BITS
void CPhysicsEnvironment::PreSave(const physpresaverestoreparams_t& params)
{
	DebugPrint();
	Error("not implemented!\n");
}

void CPhysicsEnvironment::PostSave()
{
	DebugPrint();
	Error("not implemented!\n");
}

void CPhysicsEnvironment::SetAlternateGravity(const Vector& gravityVector)
{
	DebugPrint();
	Error("not implemented!\n");
}

void CPhysicsEnvironment::GetAlternateGravity(Vector* pGravityVector) const
{
	DebugPrint();
	Error("not implemented!\n");
}

float CPhysicsEnvironment::GetDeltaFrameTime(int maxTicks) const
{
	DebugPrint();
	Error("not implemented!\n");
	return 0.0f;
}

void CPhysicsEnvironment::ForceObjectsToSleep(IPhysicsObject** pList, int listCount)
{
	DebugPrint();
	Error("not implemented!\n");
}

void CPhysicsEnvironment::SetPredicted(bool bPredicted)
{
	DebugPrint();
	Error("not implemented!\n");
}

bool CPhysicsEnvironment::IsPredicted(void)
{
	DebugPrint();
	Error("not implemented!\n");
	return false;
}

void CPhysicsEnvironment::SetPredictionCommandNum(int iCommandNum)
{
	DebugPrint();
	Error("not implemented!\n");
}

int CPhysicsEnvironment::GetPredictionCommandNum(void)
{
	DebugPrint();
	Error("not implemented!\n");
	return 0;
}

void CPhysicsEnvironment::DoneReferencingPreviousCommands(int iCommandNum)
{
	DebugPrint();
	Error("not implemented!\n");
}

void CPhysicsEnvironment::RestorePredictedSimulation(void)
{
	DebugPrint();
	Error("not implemented!\n");
}

void CPhysicsEnvironment::DestroyCollideOnDeadObjectFlush(CPhysCollide*)
{
	DebugPrint();
	Error("not implemented!\n");
}
#endif

IPhysicsEnvironment *CreatePhysicsEnvironment( void )
{
	return new CPhysicsEnvironment;
}


// This wraps IVP_Collision_Filter_Exclusive_Pair since we're reusing it
// as a general void * pair hash and it's API implies that has something
// to do with collision detection
class CVoidPairHash : private IVP_Collision_Filter_Exclusive_Pair
{
public:
	void AddPair( void *pObject0, void *pObject1 )
	{
		DebugPrint();
		// disabled pairs are stored int the collision filter's hash
		disable_collision_between_objects( (IVP_Real_Object *)pObject0, (IVP_Real_Object *)pObject1 );
	}

	void RemovePair( void *pObject0, void *pObject1 )
	{
		DebugPrint();
		// enabling removes the stored hash pair
		enable_collision_between_objects( (IVP_Real_Object *)pObject0, (IVP_Real_Object *)pObject1 );
	}

	bool HasPair( void *pObject0, void *pObject1 )
	{
		DebugPrint();
		// If collision is enabled, the pair is NOT present, so invert the test here.
		return check_objects_for_collision_detection( (IVP_Real_Object *)pObject0, (IVP_Real_Object *)pObject1 ) ? false : true;
	}
};


class CObjectPairHash : public IPhysicsObjectPairHash
{
public:
	CObjectPairHash() 
	{
		m_pObjectHash = new IVP_VHash_Store(1024);
	}

	~CObjectPairHash() 
	{
		delete m_pObjectHash;
	}

	// converts the void * stored in the hash to a list in the multilist
	unsigned short HashToListIndex( void *pHash )
	{
		DebugPrint();
		if ( !pHash )
			return m_objectList.InvalidIndex();

        uintp hash = (uintp)pHash;
		// mask off the extra bit we added to avoid zeros
		hash &= 0xFFFF;
		return (unsigned short)hash;
	}

	// converts the list in the multilist to a void * we can put in the hash
	void *ListIndexToHash( unsigned short listIndex )
	{
		DebugPrint();
		unsigned int hash = (unsigned int)listIndex;

		// set the high bit, so zero means "not there"
		hash |= 0x80000000;
		return (void *)(intp)hash;
	}

	// Lookup this object and get a multilist entry
	unsigned short GetListForObject( void *pObject )
	{
		DebugPrint();
		return HashToListIndex( m_pObjectHash->find_elem( pObject ) );
	}

	// new object, set up his list
	void SetListForObject( void *pObject, unsigned short listIndex )
	{
		DebugPrint();
		Assert( !m_pObjectHash->find_elem( pObject ) );
		m_pObjectHash->add_elem( pObject, ListIndexToHash(listIndex) );
	}

	// last entry is gone, remove the object
	void DestroyListForObject( void *pObject, unsigned short listIndex )
	{
		DebugPrint();
		if ( m_objectList.IsValidList( listIndex ) )
		{
			m_objectList.DestroyList( listIndex );
			m_pObjectHash->remove_elem( pObject );
		}
	}

	// Add this object to the list of disabled objects
	void AddToObjectList( void *pObject, void *pAdd )
	{
		DebugPrint();
		unsigned short listIndex = GetListForObject( pObject );
		if ( !m_objectList.IsValidList( listIndex ) )
		{
			listIndex = m_objectList.CreateList();
			SetListForObject( pObject, listIndex );
		}
		
		m_objectList.AddToTail( listIndex, pAdd );
	}

	// Remove one object from a particular object's list (linear time)
	void RemoveFromObjectList( void *pObject, void *pRemove )
	{
		DebugPrint();
		unsigned short listIndex = GetListForObject( pObject );
		if ( !m_objectList.IsValidList( listIndex ) )
			return;

		for ( auto item = m_objectList.Head(listIndex);
			item != m_objectList.InvalidIndex();
			item = m_objectList.Next(item) )
		{
			if ( m_objectList[item] == pRemove )
			{
				// found it, remove
				m_objectList.Remove( listIndex, item );
				if ( m_objectList.Head(listIndex) == m_objectList.InvalidIndex() )
				{
					DestroyListForObject( pObject, listIndex );
				}
				return;
			}
		}
	}

	// add a pair (constant time)
	void AddObjectPair( void *pObject0, void *pObject1 ) override
	{
		DebugPrint();
		if ( IsObjectPairInHash(pObject0,pObject1) )
			return;

		// add the pair to the hash
		m_pairHash.AddPair( pObject0, pObject1 );
		AddToObjectList( pObject0, pObject1 );
		AddToObjectList( pObject1, pObject0 );
	}

	// remove a pair (linear time x 2)
	void RemoveObjectPair( void *pObject0, void *pObject1 ) override
	{
		DebugPrint();
		if ( !IsObjectPairInHash(pObject0,pObject1) )
			return;
		
		// remove the pair from the hash
		m_pairHash.RemovePair( pObject0, pObject1 );
		RemoveFromObjectList( pObject0, pObject1 );
		RemoveFromObjectList( pObject1, pObject0 );
	}
	
	// check for pair presence (fast constant time)
	bool IsObjectPairInHash( void *pObject0, void *pObject1 ) override
	{
		DebugPrint();
		return m_pairHash.HasPair( pObject0, pObject1 );
	}

	void RemoveAllPairsForObject( void *pObject ) override
	{
		DebugPrint();
		unsigned short listIndex = GetListForObject( pObject );
		if ( !m_objectList.IsValidList( listIndex ) )
			return;

		unsigned short item = m_objectList.Head(listIndex);
		while ( item != m_objectList.InvalidIndex() )
		{
			unsigned short next = m_objectList.Next(item);
			void *pOther = m_objectList[item];
			m_objectList.Remove( listIndex, item );
			// remove the matching entry
			RemoveFromObjectList( pOther, pObject );
			m_pairHash.RemovePair( pOther, pObject );
			item = next;
		}
		DestroyListForObject( pObject, listIndex );
	}

	// Gets the # of dependencies for a particular entity
	int GetPairCountForObject( void *pObject0 ) override
	{
		DebugPrint();
		unsigned short listIndex = GetListForObject( pObject0 );
		if ( !m_objectList.IsValidList( listIndex ) )
			return 0;

		int nCount = 0;
		for ( auto item = m_objectList.Head(listIndex); item != m_objectList.InvalidIndex(); 
				item = m_objectList.Next(item) )
		{
			++nCount;
		}
		return nCount;
	}

	// Gets all dependencies for a particular entity
	int GetPairListForObject( void *pObject0, int nMaxCount, void **ppObjectList ) override
	{
		DebugPrint();
		unsigned short listIndex = GetListForObject( pObject0 );
		if ( !m_objectList.IsValidList( listIndex ) )
			return 0;

		int nCount = 0;
		for ( auto item = m_objectList.Head(listIndex); item != m_objectList.InvalidIndex(); 
				item = m_objectList.Next(item) )
		{
			ppObjectList[nCount] = m_objectList[item];
			if ( ++nCount >= nMaxCount )
				break;
		}
		return nCount;
	}

	bool IsObjectInHash( void *pObject0 ) override
	{
		DebugPrint();
		return m_pObjectHash->find_elem(pObject0) != nullptr;
	}

private:
	// this is a hash of object pairs
	CVoidPairHash		m_pairHash;
	// this is a hash of pObjects with each element storing an index to the head of its list of disabled collisions
	IVP_VHash_Store		*m_pObjectHash;

	// this is the list of disabled collisions for each object.  Uses multilist
	CUtlMultiList<void *, unsigned short>	m_objectList;
};

IPhysicsObjectPairHash *CreateObjectPairHash()
{
	DebugPrint();
	return new CObjectPairHash;
}
