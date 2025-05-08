// Copyright (C) Ipion Software GmbH 1999-2000. All rights reserved.




#ifndef WIN32
#	pragma implementation "ivp_ball.hxx"
#endif

#include <ivp_physics.hxx>

#include <ivp_templates.hxx>
#include <ivp_listener_object.hxx>
#include <ivp_compact_ledge.hxx>
#include <ivp_compact_surface.hxx>


class IVP_SurfaceManager_Ball : public IVP_SurfaceManager {
	IVP_Compact_Ledge *compact_ledge; // dummy compact ledge
	

public:
	const IVP_Compact_Ledge *get_single_convex() const override { return compact_ledge; }
  
	/********************************************************************************
	 *	Name:		 	get_radius_and_radius_dev_to_given_center		
	 *	Description:	gets the radius and the max( length( surface_normal cross_product surface_point ))
	 *				  if radius_deviation can not be calculated, set it to radius
	 ********************************************************************************/
	void get_mass_center(IVP_U_Float_Point *mass_center_out) const override {
	mass_center_out->set_to_zero();
	}
	
	void get_radius_and_radius_dev_to_given_center(const IVP_U_Float_Point * /*center*/, IVP_FLOAT * /*radius*/, IVP_FLOAT * /*radius_deviation*/) const override {
	CORE;}
	void get_rotation_inertia( IVP_U_Float_Point * /*rotation_inertia_out*/ ) const override { CORE;}
  

	/********************************************************************************
	 *	Name:		 	get_all_ledges_within_radius		
	 *	Description:	the main function which a surface_manager manager has to implement!!!:
	 *				  returns all ledges which intrude into a sphere around the observer's position 
	 * 	Note:		Polygons only
	 ********************************************************************************/
	void get_all_ledges_within_radius(const IVP_U_Point * observer_position_object, IVP_DOUBLE radius,
						  const IVP_Compact_Ledge * /*root_ledge*/, IVP_Real_Object * /*other_object*/, const IVP_Compact_Ledge * /*other_reference_ledge*/,
						  IVP_U_BigVector<IVP_Compact_Ledge> *resulting_ledges) override {
	if (observer_position_object->quad_length() > radius*radius) return;
	resulting_ledges->add(compact_ledge);
	}

	void get_all_terminal_ledges(IVP_U_BigVector<IVP_Compact_Ledge> *resulting_ledges) override {
	CORE;
	resulting_ledges->add(compact_ledge);	
	}

	void insert_all_ledges_hitting_ray(IVP_Ray_Solver * /*ray_solver*/,
						IVP_Real_Object * /*object_to_insert*/) override
	{ CORE; } // dummy
	
	~IVP_SurfaceManager_Ball(){
	P_FREE_ALIGNED( compact_ledge );
	}
	
	IVP_SurfaceManager_Ball(){
	int size = 32;
	void *buffer = ivp_malloc_aligned( size, 16);
	memset( buffer,0,size);
	compact_ledge = (IVP_Compact_Ledge *)buffer;
	}
	
	IVP_SURMAN_TYPE get_type() override { return IVP_SURMAN_BALL;}
};

static IVP_SurfaceManager_Ball ivp_surface_manager_ball_global;

IVP_Ball::IVP_Ball(IVP_Cluster *father, const IVP_Template_Ball *tball, const IVP_Template_Real_Object *templ, const IVP_U_Quat *rotation, const IVP_U_Point *position):
	IVP_Real_Object(father, NULL, templ, rotation, position)
{
  this->set_type(IVP_BALL);
  this->extra_radius = (IVP_FLOAT)tball->radius + templ->extra_radius;
  this->surface_manager = & ivp_surface_manager_ball_global;
  this->init_object_core( father->get_environment(), templ);
  {	// fire global 'object created' event
	IVP_Event_Object event_created;
	event_created.real_object = this;
	event_created.environment = physical_core->environment;
	physical_core->environment->fire_event_object_created(&event_created);
  }
}

IVP_Ball::~IVP_Ball()
{
	;
}
