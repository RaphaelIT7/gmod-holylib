#ifndef _IVP_FRICTION
#define _IVP_FRICTION
#include "sourcesdk/ivp_linear.h"

class IVP_Contact_Point;
class IVP_Friction_Solver_Long_Term;
class IVP_Mindist_Manager;
class IVP_Core;
class IVP_Friction_Info_For_Core;
class IV_U_Point_PCore;
class IVP_Environment;
class IVP_Cache_Ledge_Point;
class IVP_Compact_Edge;
class IVP_Mindist;
class IVP_Material;

#define IVP_SLOWLY_TURN_ON_KEEPER 20

#define IVP_MAX_FRICTION_MATRIX 150
#define IVP_MAX_ADHESION_GAUSS 0.01f //in percent of weight

#define IVP_MINIMAL_REAL_FRICTION_LEN 0.005f
//#define NOFRICTIONFORCE
//#define NOEASING 
//#define NO_MUTUAL_ENERGYDESTROY
//#define INTEGRATE_ENERGY_DAMP 0.9f //leave 
#define MAX_ENERGY_DESTROY 0.1f
#define IVP_EASE_EVERY_NTH_PSI 5
//#define IVP_USE_S_VALS_FOR_PRETENSION

//#define IVP_DISTANCE_KEEPERS
//#define IVP_PANELTY_SOLVER


struct IVP_Contact_Situation
{
    // can be requested in a collision or friction situations
    IVP_U_Float_Point surf_normal; 		// 16bytes: the normized normal of the contact surface (world space), pointing to second obj
    IVP_U_Float_Point speed;			// 16bytes: the speed (world space) of contact surface1 relative to contact surface0
    IVP_U_Point contact_point_ws;		// 16bytes[ND]:
    IVP_Real_Object* objects[2];		// 8bytes: link to involved objects
    const class IVP_Compact_Edge* compact_edges[2]; // 8bytes: link to involved Edges (needs source code license for more info
    IVP_Material* materials[2];	        // 8bytes: materials (might be virtual materials for point and edge cases)
    IVP_Contact_Situation() = default;
};

// set completely (except union) by recalc friction s_vals
class IVP_Impact_Solver_Long_Term : public IVP_Contact_Situation {
public:
    short    index_in_fs;
    short    impacts_while_system;
    IVP_BOOL coll_time_is_valid : 8;
    IVP_BOOL friction_is_broken : 2;
    // next block is mantained by calc_coll_dist and needed by impact

    union {
        struct { // impact data
            IVP_FLOAT rescue_speed_addon;
            IVP_FLOAT distance_reached_in_time;
            IVP_FLOAT percent_energy_conservation; // #+# maybe move to IVP_Contact_Point, maybe short 1.0f subtract energy destroyed
        } impact;
        // next block is used for friction and set by IVP_Friction_Solver::setup_coords_mindists
        struct { // friction data
            IVP_Friction_Info_For_Core* friction_infos[2]; //two friction infos for one distance, to get all distances of a object  x2
            int has_negative_pull_since;                   // copy of contact point to avoid cache misses
            IVP_FLOAT dist_len;                            // copy of distance of two objects now    
        } friction;
    };
    IVP_FLOAT  virtual_mass;             // in direction of surface normal
    IVP_FLOAT  inv_virtual_mass;         // in direction of surface normal

    IVP_Core* contact_core[2];                    // == 0 for unmoveable cores

    IVP_U_Float_Point span_friction_v[2]; // orthogonal vectors in world coords
    // vector of v[0]*s[0] + v[1]*s[1] is real world force vector of force that has to be done on first obj (is pointing to second obj)

    IVP_U_Float_Point contact_point_cs[2];   // impact/friction position in core coordinates
    IVP_U_Float_Point contact_cross_nomal_cs[2]; // crossproduct of contact_point_cs and normal_cs


    // *******************
    // ******** functions:
    // *******************    
    void init_tmp_contact_info() { impacts_while_system = 0; coll_time_is_valid = IVP_FALSE; friction_is_broken = IVP_FALSE; };

    IVP_Impact_Solver_Long_Term() { init_tmp_contact_info(); };

    inline IVP_DOUBLE get_closing_speed() const;  // based on current core speed

    void do_impact_long_term(IVP_Core* pushed_cores[2], IVP_FLOAT rescue_speed_val, IVP_Contact_Point* cp); // returns 0 if O.K. , returns 1 if false impact
    static IVP_Core* find_second_critical_impact_core(IVP_Core* core1, IVP_Core* core0);

    // the real start of an impact
    static void do_impact_of_two_objects(IVP_Mindist* mindist, IVP_Real_Object* obj0, IVP_Real_Object* obj1);
    static IVP_Core* get_best_merge_core(IVP_Core* core0, IVP_Core* core1, IVP_Core* core2);
};

#endif