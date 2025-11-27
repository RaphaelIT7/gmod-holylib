#ifndef _IVP_CLASSES
#define _IVP_CLASSES

#define IVP_HALFSPACE_OPTIMIZATION_ENABLED
#define IVP_U_MINLIST_USELONG
#define IVP_VECTOR_UNIT_FLOAT  /* set if extra data is inserted to support vector units */
#define IVP_VECTOR_UNIT_DOUBLE /* set if extra data should be insersted to utilize double vector units */

#if !defined(IVP_NO_DOUBLE) && (defined(PSXII) || defined(GEKKO) || defined(_XBOX))
#define IVP_NO_DOUBLE
#endif

#include "sourcesdk/ivp_old/ivp_time.h"

namespace GMODSDK
{

class IVP_Core;
class IVP_Anchor;
class IVP_Material
{
	// base class that may be implemented in different ways (e.g. IVP_Material_Simple);
public:
	P_MATERIAL_TYPE material_type = P_MATERIAL_TYPE_UNINITIALIZED; // indicates way of implementation
	IVP_BOOL		second_friction_x_enabled; // when we want two different friction values (e.g. for Skis) 
  
	virtual IVP_DOUBLE get_friction_factor()=0;
	virtual IVP_DOUBLE get_second_friction_factor()=0; // second friction factor when two friction values is enabled
	virtual IVP_DOUBLE get_elasticity()=0;
	// INTERN_START
	virtual IVP_DOUBLE get_adhesion()=0;  // for future releases
	// INTERN_END
	virtual const char *get_name() = 0;		// helps debugging
	virtual ~IVP_Material();
	IVP_Material() { second_friction_x_enabled = IVP_FALSE; }
};

class IVP_Synapse_Friction {  // sizeof() == 32
public:
  IVP_Synapse_Friction 	 *next, *prev;				// per object list
  IVP_Real_Object  *l_obj;				// back link to object
protected:
  short contact_point_offset;			   // back link to my controlling mindist
  short status:8;				// type IVP_SYNAPSE_POLYGON_STATUS  point, edge, tri, ball ....
public:
	
  const IVP_Compact_Edge 	*edge;		// Note: all balls share one dummy edge


	// has to be transfered to synapse_friction_polygon:

	IVP_Real_Object *get_object(){ return l_obj; };
	IVP_Synapse_Friction	   *get_next(){ return next;};
	IVP_Synapse_Friction	   *get_prev(){ return prev;};
};

class IVP_U_Quat {
public:
	IVP_DOUBLE x,y,z,w;
} IVP_ALIGN_16;

class IVP_U_Float_Point {
public:
	IVP_FLOAT k[3];
#ifdef IVP_VECTOR_UNIT_FLOAT
	union {
	IVP_FLOAT hesse_val;
	};
#endif
} IVP_ALIGN_16;

#if defined(IVP_NO_DOUBLE)
class IVP_U_Point: public IVP_U_Float_Point {
public:

#else
class IVP_U_Point {
public:
	IVP_DOUBLE k[3];
#ifdef IVP_VECTOR_UNIT_DOUBLE	
	union {
		IVP_DOUBLE hesse_val;
	};
#endif
#endif
} IVP_ALIGN_16;

using IVP_U_MINLIST_FIXED_POINT = float;
class IVP_U_Min_List_Element {
	public:
#ifdef IVP_U_MINLIST_USELONG
		unsigned short  long_next;
		unsigned short  long_prev;
#endif
		unsigned short next;
		unsigned short prev;
		IVP_U_MINLIST_FIXED_POINT value;
		void *element;
};

class IVP_U_Min_List {
	friend class IVP_U_Min_List_Enumerator;
	unsigned short	malloced_size;
	unsigned short	free_list;
	IVP_U_Min_List_Element *elems;
public:
	IVP_U_MINLIST_FIXED_POINT min_value;
#ifdef IVP_U_MINLIST_USELONG
	unsigned short	first_long;
#endif
	unsigned short	first_element;
	unsigned short	counter;

	IVP_U_MINLIST_INDEX add(void *elem,IVP_U_MINLIST_FIXED_POINT value);
};

class IVP_Hull_Manager_Base_Gradient {
public:
	IVP_Time last_vpsi_time;
	IVP_FLOAT gradient;			// slightly higher than the real gradient 
	IVP_FLOAT center_gradient;  
	IVP_FLOAT hull_value_last_vpsi;		
	IVP_FLOAT hull_center_value_last_vpsi;	
	IVP_FLOAT hull_value_next_psi;	// optimistic, may be less
	int time_of_next_reset;  		// counting seconds only
public:
	IVP_Hull_Manager_Base_Gradient() : last_vpsi_time (0.0f) {; };
	~IVP_Hull_Manager_Base_Gradient(){;};
};

class IVP_Hull_Manager_Base: protected IVP_Hull_Manager_Base_Gradient {
protected:
	// Manages a sorted event list of synapses for one Real_Object **/
	IVP_U_Min_List sorted_synapses;
};

enum IVP_OBJECT_TYPE {

	IVP_NONE,
	IVP_CLUSTER,		// a container for other objects
	IVP_POLYGON,		// a polyhedron
	IVP_BALL,			// a ball
	IVP_OBJECT
};
class IVP_Cluster;
class IVP_Template_Object;

/************************************************************************************************
 *	Name:			IVP_Object   	
 *	Description:	An abstract object
 *	Note:		See IVP_Real_Object if you are looking for the physical side of an object
 ************************************************************************************************/
class IVP_Object {

	friend class IVP_Cluster;
	friend class IVP_Cluster_Manager;
	friend class IVP_Real_Object;
public:
	virtual ~IVP_Object();
private:
	void init(IVP_Environment *env);	// real constructor
	IVP_OBJECT_TYPE object_type;	// type of this object, e.g. used for correct casts...
protected:
	IVP_Object   *next_in_cluster;	// next object in this cluster
	IVP_Object   *prev_in_cluster;	
	IVP_Cluster  *father_cluster;	// the father cluster
	const char	 *name;			// the name of the object, helps debugging

	IVP_Object(IVP_Environment *env);		// necessary to create root cluster
	IVP_Object(IVP_Cluster *father, const IVP_Template_Object *templ);

	void assign_to_cluster(IVP_Cluster *cluster); // moves object to cluster
//	friend class IVP_Mindist;

	void set_type(IVP_OBJECT_TYPE type_in)		{ object_type = type_in; };
public:
	IVP_Environment *environment; 	// the environment to which this object belongs
	IVP_OBJECT_TYPE	get_type() const		{ return object_type; };

	const char		*get_name() const		{ return name; };
	IVP_Environment		*get_environment() const		{ return environment; };
};

/*********************************************************************
 *	Name:			IVP_Real_Object  	
 *	Description:	Base of all real objects.
 *			Real object means that this object has a mass,
 *			can collide, etc.
 *********************************************************************/
class IVP_Real_Object_Fast_Static: public IVP_Object {
protected:
	class IVP_Controller_Phantom *controller_phantom;
	class IVP_Synapse_Real *exact_synapses;		// Linked list of all exact synapses, no sphere nor hull.
	IVP_Synapse_Real	 *invalid_synapses;		 // Linkes list of all intruded synapses
	IVP_Synapse_Friction *friction_synapses;	// Linked list of contact points.
	IVP_U_Quat			 *q_core_f_object;	 // in object !!!!
	IVP_U_Float_Point 	 shift_core_f_object;
	//IVP_Real_Object_Fast_Static(IVP_Cluster *father, const IVP_Template_Object *templ): IVP_Object( father, templ){;};
public:
	const IVP_U_Float_Point *get_shift_core_f_object() const { return &shift_core_f_object; };

};

// add dynamics
class IVP_Real_Object_Fast: public IVP_Real_Object_Fast_Static {
protected:
	class IVP_Cache_Object *cache_object;		// Intermediate caches
	IVP_Hull_Manager_Base hull_manager; 		// Internal usage
public:
	struct {
	IVP_Movement_Type  object_movement_state:8; // moving, not_simulated, static only
	IVP_BOOL	  	collision_detection_enabled:2;	/* Collision detection is enabled. If it is disabled, not even
							 * the IVP_Collision_Filter is called */
	IVP_BOOL 	shift_core_f_object_is_zero:2;
	
	unsigned int	object_listener_exists:1;   /* is set to one if an object listener exists */
	unsigned int	collision_listener_exists:1; /* all flags of object listeners functions */
	unsigned int	collision_listener_listens_to_friction:1;
	} flags;
	//IVP_Real_Object_Fast(IVP_Cluster *father, const IVP_Template_Object *templ): IVP_Real_Object_Fast_Static( father, templ){;};
};

class IVP_Compact_Ledge {
	// ATTENTION: some functions depend on EXACTLY THIS SIZE AND SHAPE of the structure
	// e.g. end of class must be 16 aligned for triangle address trick.
	// and assert(sizeof(IVP_Compact_Ledge) == 16 == sizeof(IVP_Compact_Triangle))
	friend class IVP_Compact_Ledge_Generator;
	friend class IVP_SurfaceBuilder_Ledge_Soup;
	friend class IVP_SurfaceBuilder_Mopp;
	friend class IVP_GridBuilder_Array;
	friend class CVPhysicsVirtualMeshWriter;
private:
	int c_point_offset; // byte offset from 'this' to (ledge) point array
	union {
	int ledgetree_node_offset;
	int client_data;	// if indicates a non terminal ledge
	};
	unsigned int has_chilren_flag:2;
	IVP_BOOL is_compact_flag:2;  // if false than compact ledge uses points outside this piece of memory
	unsigned int dummy:4;
	unsigned int size_div_16:24; 
	short n_triangles;
	short for_future_use;
};

class IVP_SurfaceManager {
protected:
public:	

	/******************************************************************************
	 *  Method:		set/get ledge_client_data
	 *  Description:	Allows the user to set and get a ledge specific client_data
	 *	Note:		Range of values [0..IVP_SURMAN_POLYGON_MAX_VALUE_CLIENTDATA[
	 *  Attention:	  Only works for terminal ledges
	 *****************************************************************************/
	static void set_ledge_specific_client_data( IVP_Compact_Ledge *, unsigned int value); // assert value < IVP_SURMAN_POLYGON_MAX_VALUE_CLIENTDATA
	static unsigned int get_ledge_specific_client_data( const IVP_Compact_Ledge *);
	
	
	/********************************************************************************
	 *	Name:		 	get_single_convex		
	 *	Description:	returns the only compact ledge if surface_manager is a single convex polyhedron or a ball else NULL
	 ********************************************************************************/
	virtual const IVP_Compact_Ledge *get_single_convex() const = 0;
  
	/********************************************************************************
	 *	Name:		 	get_radius_and_radius_dev_to_given_center		
	 *	Description:	gets the radius and the max( length( surface_normal cross_product surface_point ))
	 *				  if radius_deviation can not be calculated, set it to radius
	 ********************************************************************************/
	virtual void get_mass_center(IVP_U_Float_Point *mass_center_out) const = 0;
	virtual void get_radius_and_radius_dev_to_given_center(const IVP_U_Float_Point *center, IVP_FLOAT *radius, IVP_FLOAT *radius_deviation) const = 0;
	virtual void get_rotation_inertia( IVP_U_Float_Point *rotation_inertia_out ) const = 0;
  

	/********************************************************************************
	 *	Name:		 	get_all_ledges_within_radius		
	 *	Description:	the main function which a surface_manager manager has to implement!!!:
	 *				  returns all ledges which intrude into a sphere around the observer's position 
	 * 	Note:		Polygons only
	 ********************************************************************************/
	//virtual void get_all_ledges_within_radius(const IVP_U_Point *observer_position_object, IVP_DOUBLE radius,
	//					  const IVP_Compact_Ledge *root_ledge, IVP_Real_Object *other_object, const IVP_Compact_Ledge *other_reference_ledge,
	//					  IVP_U_BigVector<IVP_Compact_Ledge> *resulting_ledges) = 0;

	//virtual void get_all_terminal_ledges(IVP_U_BigVector<IVP_Compact_Ledge> *resulting_ledges) = 0;

	/********************************************************************************
	 *	Name:		 	insert_all_ledges_hitting_ray		
	 *	Description:	Used for ray casting
	 *				  Fills all ledges which are hit by the specified ray into the ray solver
	 * 	Note:		For polygons only
	 ********************************************************************************/
	//virtual void insert_all_ledges_hitting_ray(IVP_Ray_Solver *ray_solver,
	//					   IVP_Real_Object *object) = 0;
  
	/********************************************************************************
	 *	Name:		 	add_/remove_reference_to_ledge		
	 *	Description:	Tells the surface manager that a synapse will use (stop use)
	 *			a ledge
	 * 	Note:		Polygons only
	 ********************************************************************************/
	//virtual void add_reference_to_ledge(const IVP_Compact_Ledge *){;};
	//virtual void remove_reference_to_ledge(const IVP_Compact_Ledge *){;};

	//virtual ~IVP_SurfaceManager() = 0;
	//virtual IVP_SURMAN_TYPE get_type()=0;
};

class IVP_U_Point;
class IVP_U_Matrix;
class IVP_Template_Real_Object;
class IVP_OV_Element;
class IVP_Mindist_Manager;
#define IVP_NO_COLL_GROUP_STRING_LEN 8
#pragma pack(push, 1)
class IVP_Real_Object : public IVP_Real_Object_Fast {

	friend class IVP_Anchor;
	friend class IVP_Core;
	friend class IVP_Friction_System;
	friend class IVP_Synapse_Real;
	friend class IVP_Cache_Object;
	friend class IVP_Cache_Object_Manager;
	friend class IVP_Synapse_Friction;
	friend class IVP_Contact_Point;
	friend class IVP_Simulation_Unit;
	friend class IVP_Controller_Phantom;
	friend class IVP_Example_Boundingboxes;
	
private:
	friend class IVP_Merge_Core;
	IVP_Anchor *anchors;			// Linked list of all anchors @@@ remove anchor concept

	friend class IVP_Object_Attach;
	void unlink_contact_points(IVP_BOOL silent); //FALSE means wake up the cores
	void clear_internal_references();

	friend class IVP_Mindist_Manager;
protected:	
	IVP_SurfaceManager *surface_manager;	// The surface_manager defines the surface structure for this object.
	
	/* The following section is used to calculate intermediate values */

	friend class IVP_Core_Collision;
	friend class IVP_Calc_Next_PSI_Solver;
	void update_exact_mindist_events_of_object();
	void revive_nearest_objects_grow_fs();
	void get_all_near_mindists();
	void recalc_exact_mindists_of_object();
	void recalc_invalid_mindists_of_object();
  
	virtual void set_new_quat_object_f_core( const IVP_U_Quat *new_quat_object_f_core, const IVP_U_Point *trans_object_f_core); // calls set_new_m...
public:
	virtual void set_new_m_object_f_core( const IVP_U_Matrix *new_m_object_f_core );	 /* specifies new objects coordinate system in core space */
	void init_object_core(IVP_Environment *env, const IVP_Template_Real_Object *tpop);
	void unlink_contact_points_for_object( IVP_Real_Object *other_object );
protected:

	IVP_Real_Object(IVP_Cluster *father,IVP_SurfaceManager *, const IVP_Template_Real_Object *,
			const IVP_U_Quat *q_world_f_obj, const IVP_U_Point *position);


	virtual ~IVP_Real_Object();	// to delete the object use delete_and_check_vicinity or delete_silently

/********************************************************************************
 *	The physics simulation internal public section. Handle with care:
 ********************************************************************************/
public:	
	char nocoll_group_ident[IVP_NO_COLL_GROUP_STRING_LEN];	/* Identifier for filtering collisions, used only 
				 * if the IVP_Collision_Filter_Coll_Group_Ident is
				 * the installed collision filter, see IVP_Application_Environment. */
	///////// Simulation internals

	IVP_Material 	*l_default_material;	// default material of object
	IVP_OV_Element   	*ov_element;		// used to trace objects movement within the environment->ov_tree
	IVP_FLOAT extra_radius;							// extra collision radius around the object

	///////// Core 
   // IVP_Core *physical_core; // Coordinate system, mass, speed, etc... (eventually merged)
	//IVP_Core *friction_core; // Long term (merged) core for frictions, when objs are splitted make sure to call 'recheck_ov_element'
	//IVP_Core *original_core;
	char offset[2];
	void *client_data; //not used by physics, provided for customer
};

#pragma pack(pop)

struct IVP_Time_Event {
	int index;
	IVP_Time_Event() = default;
	// dimhotepus: Make pure instead of CORE.
	virtual void simulate_time_event(IVP_Environment *) = 0; // to be implemented by application
	virtual ~IVP_Time_Event();
};

class IVP_Collision_Delegator {
public:
	/********************************************************************************
	 *	Name:		   	collision_is_going_to_be_deleted_event
	 *	Description:	for any reason a collision element is deleted,
	 *			a and b can also be calculated using the get_objects function
	 *			however it's much faster this way
	 ********************************************************************************/
  virtual void collision_is_going_to_be_deleted_event(class IVP_Collision *t) = 0;  // should remove t from its internal structures
  virtual ~IVP_Collision_Delegator(){;};
  // Note: in the destructor, it should not delete it's IVP_Collision children, but
  // call child->delegator_is_going_to_be_deleted_event(this)

  //@@CB
  virtual void change_spawned_mindist_count(int /*change*/) {;};
  virtual int get_spawned_mindist_count() {return -1;};
  //@@CB
};

class IVP_Mindist_Base;
/********************************************************************************
 *	Name:	  	IVP_Listener_Hull		
 *	Description:	super-class for IVP_Synapse and IVP_Anchor
 ********************************************************************************/
class IVP_Listener_Hull {

public:
	virtual IVP_HULL_ELEM_TYPE get_type()=0;
	virtual void hull_limit_exceeded_event(IVP_Hull_Manager *, IVP_HTIME hull_intrusion_value) = 0;
	virtual void hull_manager_is_going_to_be_deleted_event(IVP_Hull_Manager *) = 0;
			// implementations have to remove themselves from hull manager
	virtual void hull_manager_is_reset(IVP_FLOAT dt, IVP_FLOAT center_dt);  // hull_manager is reset
private:
	friend class IVP_Hull_Manager;
	unsigned int minlist_index;
};

class IVP_Synapse: public IVP_Listener_Hull {  // sizeof() == 32
public:
  IVP_Synapse 	 *next, *prev;			   // per object, only for exact/invalid synapses
  IVP_Real_Object  *l_obj;					 // back link to object
  const IVP_Compact_Edge 	*edge;		// Note: all balls share one dummy edge

protected:
	short mindist_offset;			 // back link to my controlling mindist
	short status;					 // IVP_SYNAPSE_POLYGON_STATUS point, edge, tri, ball ....
public:
   
protected:
	// hull manager
  IVP_HULL_ELEM_TYPE get_type(){ return IVP_HULL_ELEM_POLYGON; };
  virtual void hull_limit_exceeded_event(IVP_Hull_Manager *hull_manager, IVP_HTIME hull_intrusion_value);
  virtual void hull_manager_is_going_to_be_deleted_event(IVP_Hull_Manager *hull_manager);
  virtual   void hull_manager_is_reset(IVP_FLOAT dt,IVP_FLOAT center_dt);
public:

	IVP_Real_Object *get_object(){ return l_obj; };
	//IVP_SYNAPSE_POLYGON_STATUS get_status()const{ return (IVP_SYNAPSE_POLYGON_STATUS) status; };

	virtual ~IVP_Synapse(){;};			// dummy, do not call
	const IVP_Compact_Ledge *get_ledge() const;
	const IVP_Compact_Edge *get_edge() const { return edge; };

	IVP_Mindist_Base *get_synapse_mindist()const{ return  (IVP_Mindist_Base *)(mindist_offset + (char *)this);} ;
	void set_synapse_mindist( IVP_Mindist_Base *md ) { mindist_offset = (short)(((char *)md) - (char *)this); };
};

/********************************************************************************
 *	Name:	  	IVP_Synapse_Real	 	
 *	Description:	Adds additional functionality to IVP_Synapse,
 *	Attention:	No additional storage elements allowed in IVP_Synapse_Real
 ********************************************************************************/
class IVP_Synapse_Real: public IVP_Synapse {
protected:
  friend class IVP_Mindist;
  friend class IVP_Mindist_Base;
  friend class IVP_Mindist_Manager;
  friend class IVP_Contact_Point;
public:

protected:

  // debug
  void check_consistency_of_ledge(const IVP_Compact_Edge *edge) const;

  inline void remove_exact_synapse_from_object();
  inline void insert_exact_synapse_in_object();

  inline void remove_invalid_synapse_from_object();
  inline void insert_invalid_synapse_in_object(); 

  virtual ~IVP_Synapse_Real(){;};
protected:
	IVP_Synapse_Real(){};
public:
  //inline IVP_Core *get_core()const{ return l_obj->physical_core; };

  virtual void print();

  //IVP_Hull_Manager *get_hull_manager(){ return get_object()->get_hull_manager();};
  inline IVP_DOUBLE insert_in_hull_manager(IVP_DOUBLE rel_hull_time); // returns current center pos
  inline IVP_DOUBLE insert_lazy_in_hull_manager(IVP_DOUBLE rel_hull_time);
  
  IVP_Synapse_Real	   *get_next(){ return (IVP_Synapse_Real *)next;};
  IVP_Synapse_Real	   *get_prev(){ return (IVP_Synapse_Real *)prev;};
  IVP_Mindist			*get_mindist(){ return (IVP_Mindist *) get_synapse_mindist(); };
};

/********************************************************************************
 *	Name:		   	IVP_Collision
 *	Description:	Super class of all collision checkers
 ********************************************************************************/
class IVP_Collision: public IVP_Time_Event {
public:
  IVP_Collision_Delegator *delegator;
	int fvector_index[2];				// for ov_tree backlink ( see IVP_Mindist::recheck_ov_element())
	int get_fvector_index(int i) const{ return fvector_index[i]; }
	void set_fvector_index(int old, int i){
		if (fvector_index[0] == old){
			fvector_index[0] = i;
		} else {
			fvector_index[1] = i;
		}
	}

  // to identify the collision
  virtual void get_objects( IVP_Real_Object *objects_out[2] ) = 0;
  virtual void get_ledges( const IVP_Compact_Ledge *ledges_out[2] ) = 0;

  virtual void delegator_is_going_to_be_deleted_event(IVP_Collision_Delegator *) = 0;
  virtual ~IVP_Collision(){;}; // implementations should call  delegator->collision_is_going_to_be_deleted_event(this);
  //IVP_Collision( IVP_Collision_Delegator *d){ delegator = d; fvector_index[0] = fvector_index[1] = -1; };
};

class IVP_Mindist_Base : public IVP_Collision {
protected:
  IVP_COLL_TYPE coll_type:8;
  unsigned int synapse_sort_flag:2;
public:  // Read only public !!!!
  IVP_BOOL			 is_in_phantom_set:2;   // mindist is in phantom set_of_mindists already
  IVP_MINDIST_FUNCTION mindist_function:2;					  // collision / phantom
  IVP_MINIMAL_DIST_RECALC_RESULT recalc_result:2;			   // intrusion/ok
#ifdef IVP_HALFSPACE_OPTIMIZATION_ENABLED	
  IVP_BOOL disable_halfspace_optimization:2;
#endif	
  IVP_MINIMAL_DIST_STATUS mindist_status:4;
	unsigned int coll_dist_selector:8;

  IVP_Synapse_Real		 synapse[2];
  IVP_FLOAT		   sum_extra_radius;   // sum of extra radius around each object

  // calculated by IVP_Mindist::recalc_mindist
  IVP_FLOAT	   len_numerator; 
#ifdef IVP_HALFSPACE_OPTIMIZATION_ENABLED
	IVP_FLOAT			 contact_dot_diff_center; // contact plane * ( core0->pos - core1->pos),
	IVP_DOUBLE			sum_angular_hull_time; // sum of center hull time, needs IVP_DOUBLE 
#endif	

  IVP_U_Float_Point contact_plane;  //  needed for halfspace optimization, normized surface normal, only valid if recalc_result == IVP_MDRR_OK  

		IVP_Synapse *get_mindist_synapse(int i)	 { return &synapse[i]; };
  const IVP_Synapse *get_mindist_synapse(int i)const{ return &synapse[i]; };
  
  IVP_FLOAT get_length() const { return len_numerator; }; // distance, only valid if recalc_result == IVP_MDRR_OK

  // IVP_Collision implementations
  virtual void get_objects( IVP_Real_Object *objects_out[2] );
  virtual void get_ledges( const IVP_Compact_Ledge *ledges_out[2] );

  
  IVP_Mindist_Base(IVP_Collision_Delegator *del);
  virtual ~IVP_Mindist_Base(){;};
};

class IVP_Mindist: public IVP_Mindist_Base // 
{
	friend class IVP_Mindist_Manager;
	friend class IVP_Contact_Point;
	friend class IVP_Mindist_Event_Solver;
	friend class IVP_Mindist_Minimize_Solver;
	friend class IVP_Synapse;
	friend class IVP_Synapse_Real;
protected:	
	IVP_Time_CODE recalc_time_stamp;	// compared to env->get_current_time_code
public:
	IVP_Mindist *next;	// in mindist_manager (exact , no hull nor sphere)
	IVP_Mindist *prev;
	const IVP_Compact_Edge *last_visited_triangle;  // optimization for cars
protected:
	virtual void mindist_rescue_push();

	void mindist_hull_limit_exceeded_event(IVP_HTIME hull_intrusion_value);
	void hull_manager_is_reset(IVP_FLOAT dt,IVP_FLOAT center_dt);
public:
  //IVP_Synapse_Real *get_synapse(int i) const { return (IVP_Synapse_Real *)&synapse[i]; };
  //IVP_Synapse_Real *get_sorted_synapse(int i) const { return (IVP_Synapse_Real *)&synapse[synapse_sort_flag ^ i]; };
	//IVP_DOUBLE get_coll_dist(){ return ivp_mindist_settings.coll_dists[coll_dist_selector]; }
					
	IVP_Mindist(IVP_Environment *env, IVP_Collision_Delegator *del);
  virtual ~IVP_Mindist();

  virtual IVP_BOOL is_recursive() { return IVP_FALSE; }; //@@CB

  //IVP_Environment *get_environment(){ return get_synapse(0)->get_object()->get_environment();};

  virtual void exact_mindist_went_invalid(IVP_Mindist_Manager *mm);
  void update_exact_mindist_events(IVP_BOOL allow_hull_conversion, IVP_MINDIST_EVENT_HINT allow_events_at_now);
  virtual void do_impact(); // does everything, including recalculation of everything, PSI synchronization etc ...
};

class IVP_U_Vector_Base
{
public:
	unsigned short memsize;
	unsigned short n_elems;
	void **elems;
	void increment_mem();
};

template<class T>
class IVP_U_Vector: public IVP_U_Vector_Base {
public:
	void ensure_capacity(){
	if (n_elems>=memsize){
		this->increment_mem();
	}
	};
protected:
	//  special vector with preallocated elems
	IVP_U_Vector(void **ielems, int size){
	//IVP_ASSERT (ielems == (void **)(this +1));
	//IVP_ASSERT (size >= 0 && size <= 0xFFFFU);
	elems = ielems;
	memsize = (unsigned short)size;
	n_elems = 0;
	}
	
public:
	IVP_U_Vector(int size = 0){
	//IVP_ASSERT (size >= 0 && size <= 0xFFFFU);
	memsize = (unsigned short)size;
	n_elems = 0;
	if (size){		// will be optimized by most compilers
		elems = (void **)malloc(size*sizeof(void *));
	}else{	
		elems = (void **)0;
	}
	};
	
	void clear(){
	if ( elems != (void **) (this+1)){
		void *dummy=(void *)elems;
		free( dummy);
		elems=0;
		memsize = 0;
	}
	n_elems = 0;
	};

	
	void remove_all(){
	n_elems = 0;
	};
	
	~IVP_U_Vector(){
	this->clear();
	};
	
	int len() const {
	return n_elems;
	};
	
	int index_of(T *elem){
	int i;
	for (i=n_elems-1;i>=0;i--){
		if (elems[i] == elem) break;
	}
	return i;
	};
	
	int add(T *elem){
	ensure_capacity();
	//IVP_ASSERT( index_of(elem) == -1);
	elems[n_elems] = (void *)elem;
	return n_elems++;
	};

	int install(T *elem){
	int old_index = index_of(elem);
	if ( old_index != -1) return old_index;
	ensure_capacity();
	elems[n_elems] = (void *)elem;
	return n_elems++;
	};

	void swap_elems(int index1, int index2){
	//IVP_ASSERT((index1>=0)&&(index1<n_elems));
	//IVP_ASSERT((index2>=0)&&(index2<n_elems));
	void *buffer = elems[index1];
	elems[index1] = elems[index2];
	elems[index2] = buffer;
	return;
	}	 
	
	void insert_after(int index, T *elem){
	//IVP_ASSERT((index>=0)&&(index<n_elems));
	index++;
	ensure_capacity();
	int j = n_elems;
	while(j>index){
		elems[j] = elems[j - 1];
		--j;
	}
	elems[index] = (void *)elem;
	n_elems++;
	};

	
	void remove_at(int index){
	//IVP_ASSERT((index>=0)&&(index<n_elems));
	int j = index;
	while(j<n_elems-1){
		elems[j] = elems[j+1];
		j++;
	}
	n_elems--;
	};

	void reverse() {
	for (int i=0; i<(this->n_elems/2); i++) {
		this->swap_elems(i, this->n_elems-1-i);
	}
	return;
	}

	void remove_at_and_allow_resort(int index){
	//IVP_ASSERT((index>=0)&&(index<n_elems));
	n_elems--;
	elems[ index ] = elems[ n_elems ];
	};

	void remove_allow_resort(T *elem){
	int index = this->index_of(elem);
	//IVP_ASSERT(index>=0);
	n_elems--;
	elems[ index ] = elems[ n_elems ];
	};
	
	void remove(T *elem){
	int index = this->index_of(elem);
	//IVP_ASSERT(index>=0);
	n_elems--;
	while (index < n_elems){
		elems[index] = (elems+1)[index];
		index++;
	}
	};

	T* element_at(int index) const {
	//IVP_ASSERT(index>=0 && index < n_elems);
	return (T *)elems[index];
	};
};

class IVP_Mindist_Manager
{
	friend class IVP_Real_Object;
	IVP_BOOL	scanning_universe;			// set to true during queries to the IVP_Universe_Manager
public:
	IVP_Environment *environment; // backlink, to be set, for objects and phys properties
	
	IVP_Mindist *exact_mindists;
	IVP_U_Vector<IVP_Mindist> wheel_look_ahead_mindists;
	
	IVP_Mindist *invalid_mindists;

	void recalc_all_exact_mindists_events();
};

};

#endif