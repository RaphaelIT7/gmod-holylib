#ifndef _IVP_CORE
#define _IVP_CORE

#include "ivp_types.h"
#include "ivp_time.h"
#include "ivp_linear.h"
#include "ivp_vector.h"

class IVP_Vector_of_Objects:public  IVP_U_Vector<IVP_Real_Object> {
    IVP_Real_Object *elem_buffer[1];
public:
    IVP_Vector_of_Objects(): IVP_U_Vector<IVP_Real_Object>( (void **)&elem_buffer[0],1 ){;};
    void reset(){ elems = (void **)&elem_buffer[0]; memsize = 1; }
};

class IVP_Friction_Hash;
class IVP_Friction_Info_For_Core;
union IVP_Core_Friction_Info { 
    struct {
	IVP_Friction_Hash *l_friction_info_hash;
    } for_unmoveables;
    struct {
	IVP_Friction_Info_For_Core *moveable_core_friction_info;
    } for_moveables;
};

class IVP_Core_Fast_Static {
	friend class IVP_Real_Object;
public:
    /********************************************************************************
     *	type:		private, public READ ONLY
     *	section:	basic, non redundant, long time unchangeable section
     *	Note (*):	Only values marked with (*) may be changed during simulation without problems
     *			(don't forget to call calc_calc() afterwards)
     *  Note:		For memory reasons, the flags will be combined to a bit-area soon.
     *			So use the corresponding functions to read/set them.
     ********************************************************************************/
    IVP_BOOL 		fast_piling_allowed_flag:2;
    IVP_BOOL		physical_unmoveable:2;	// never any movement
    IVP_BOOL            is_in_wakeup_vec:2;
    IVP_BOOL		rot_inertias_are_equal:2;
	IVP_BOOL		pinned:2;
    
    IVP_FLOAT		upper_limit_radius;	// the radius of the object around the mass center
    IVP_FLOAT		max_surface_deviation;	// for all P of all Surfaces S:		maximum P * S

    IVP_Environment 	*environment; 	// Backlink
    
    class IVP_Constraint_Car_Object *car_wheel;  // bad bad hack for optimized wheel behaviour;
    
protected:
    IVP_U_Float_Hesse	rot_inertia;            // needed for energy calculation, mass is w part
public:
    
    IVP_U_Float_Point	rot_speed_damp_factor;	// (*) dampening of speed, one value per axis: new speed *= 1/e**(rot_speed_damp_factor * delta_time)

    /********************************************************************************
     *	type:		private, public READ ONLY
     *	section:	basic, redundant, long time unchangeable section (updated by calc_calc)
     ********************************************************************************/
private:
    IVP_U_Float_Hesse	inv_rot_inertia;  // 1.0f/rot_inertia, 4th value is inverse_mass
public:

    IVP_FLOAT		speed_damp_factor;	// (*) same for speed
    IVP_FLOAT		inv_object_diameter; 		// 2.0f/upper_limit_radius  
	IVP_U_Float_Point *spin_clipping;   // if set, than angular velocities are individually clipped against this value

    /********************************************************************************
     *	section:	internal logical section
     ********************************************************************************/
    IVP_Vector_of_Objects objects; 	// All objects for this core.

    /********************************************************************************
     *	type:	  private, internal
     *	section:  friction, systems
     ********************************************************************************/
    IVP_Core_Friction_Info core_friction_info;


    const class IVP_U_Point_4 *get_inv_masses(){ return (class IVP_U_Point_4*)& inv_rot_inertia;} // combined solution for inertias
    IVP_FLOAT get_mass() const{ return rot_inertia.hesse_val; };

    const IVP_U_Float_Point *get_rot_inertia()const{ return &rot_inertia;};
    const IVP_U_Float_Point *get_inv_rot_inertia() const{ return &inv_rot_inertia;};
    IVP_FLOAT get_inv_mass() const{ return inv_rot_inertia.hesse_val;};

};

class IVP_Core_Fast_PSI:  public IVP_Core_Fast_Static {
public:
    /********************************************************************************
     *	type		private, public READ ONLY
     *	section:	basic simulation part, non redundant part
     ********************************************************************************/

	// We currently only need movement_state and pinned.
    IVP_Movement_Type	movement_state:8;		// Movement state
    /*IVP_BOOL            temporarily_unmovable:8;

    short               impacts_since_last_PSI;
    IVP_Time     	time_of_last_psi;		// Indicates point in simulation time the last matrizes are valid at.    

     ********************************************************************************
     *	type:		private, public READ ONLY
     *	section:	basic simulation part, redundant part, calculated by calc_next_PSI_matrix
     ********************************************************************************
    IVP_FLOAT	i_delta_time;				// 1.0f / (delta_time)

    // #+# check for empty space (due to align(16))
    IVP_U_Float_Point	rot_speed_change;		// Accumulates async_rot_pushes, units like this->rot_speed.
    IVP_U_Float_Point	speed_change;			// Accumulates async_pushes, units like this->speed.

    IVP_U_Float_Point	rot_speed;			// Rotation speed around core axles [rad/s]
    IVP_U_Float_Point	speed;				// Translation speed in world system [m/s]

    IVP_U_Point pos_world_f_core_last_psi;
    IVP_U_Float_Point delta_world_f_core_psis;

    IVP_U_Quat	q_world_f_core_last_psi;	// Rotation of core using quaternions (see m_world_f_core_x_PSI)
    IVP_U_Quat 	q_world_f_core_next_psi;	// is is the quat to use at a psi

    IVP_U_Matrix	m_world_f_core_last_psi;	* The core matrix valid at PSI (rotation part is redundant, see q_world_f_core_last_psi),							 * use get_m_world_f_core_PSI() to get a valid pointer to this matrix */
};

class IVP_Core_Fast : public IVP_Core_Fast_PSI {
public:
/*    // needed for collision detection
    IVP_U_Float_Point rotation_axis_world_space;        // normized q_ncore_f_core
    IVP_FLOAT current_speed;		// speed.real_length()
    IVP_FLOAT abs_omega;		// absolute angular velocity interpolated movement
    IVP_FLOAT max_surface_rot_speed;	// abs_omega * max_surface_deviation
*/
};

class IVP_Core: public IVP_Core_Fast { 
public:
	// Nothing
};

#endif