#ifndef _IVP_TYPES
#define _IVP_TYPES

namespace GMODSDK
{

class IVP_Hull_Manager;
class IVP_Real_Object;
class IVP_Compact_Edge;
class IVP_Environment;

using IVP_FLOAT = float;
using IVP_DOUBLE = double;
using IVP_Time_CODE = int;
using IVP_HTIME = IVP_FLOAT;

enum IVP_BOOL {
	IVP_FALSE = 0,
	IVP_TRUE = 1
};

#define IVP_ALIGN_16

enum IVP_RETURN_TYPE {
	IVP_FAULT = 0,
	IVP_OK = 1
};

enum IVP_Movement_Type {
	IVP_MT_UNDEFINED = 0,
	IVP_MT_MOVING = 0x01,	  // fast 
	IVP_MT_SLOW = 0x02,			  // slow
	IVP_MT_CALM = 0x03, 	  // slow for more than a certain time span (currently 1 second)
	IVP_MT_NOT_SIM =0x08,	  // not simulated, but can be changed to be simulated
	IVP_MT_STATIC_PHANTOM = 0x09, // like static, but object may be moved by setting the matrix directly
	IVP_MT_STATIC = 0x10,	  // static object
	IVP_MT_GET_MINDIST = 0x21	 // not really a movement type, used to get all mindists with recheck_ov_element
};

#define IVP_MTIS_SIMULATED(var) ( var < IVP_MT_NOT_SIM )

enum IVP_HULL_ELEM_TYPE {
	IVP_HULL_ELEM_POLYGON,
	IVP_HULL_ELEM_ANCHOR,
	IVP_HULL_ELEM_OO_WATCHER,
	IVP_HULL_ELEM_OO_CONNECTOR
};

enum IVP_MINDIST_EVENT_HINT {
	IVP_EH_NOW,
	IVP_EH_SMALL_DELAY,
	IVP_EH_BIG_DELAY
};

enum IVP_MRC_TYPE {
	IVP_MRC_UNINITIALIZED = 0,
	IVP_MRC_OK = 1,
	IVP_MRC_ENDLESS_LOOP=2,
	IVP_MRC_BACKSIDE=3,
	IVP_MRC_ALREADY_CALCULATED=4,	// see status for details
	IVP_MRC_ILLEGAL=5
};

class IVP_Mindist;
class IVP_Mindist_Minimize_Solver {
public:
	IVP_Mindist *mindist;
	int P_Finish_Counter; 	// global for debug/termination purposes
};

enum P_MATERIAL_TYPE
{
	P_MATERIAL_TYPE_UNINITIALIZED = -1,
	P_MATERIAL_TYPE_TERMINAL,
	P_MATERIAL_TYPE_LAST
};

// flag telling how the mindist is linked
enum IVP_MINIMAL_DIST_STATUS {
	IVP_MD_UNINITIALIZED = 0,
	IVP_MD_INVALID = 2,   // invalid mindist, eg endless loop, collision
	IVP_MD_EXACT = 3,
	IVP_MD_HULL_RECURSIVE = 4,   // invalid recursive mindists which spawned childs
	IVP_MD_HULL = 5   // -> synapses = hull synapses
};

// result of last recalc_mindist
enum IVP_MINIMAL_DIST_RECALC_RESULT {
  IVP_MDRR_OK = 0,
  IVP_MDRR_INTRUSION = 1
};


// result of the last recalc_next_event
enum IVP_COLL_TYPE {  // if last 4 bits == 0 then collision
	IVP_COLL_NONE =0x00,
	IVP_COLL_PP_COLL  =0x10,
	IVP_COLL_PP_PK  =0x11,

	IVP_COLL_PF_COLL  =0x20,
	IVP_COLL_PF_NPF =0x21,
	// PF_PK nicht noetig, da Flaeche Objekt abschirmt

	IVP_COLL_PK_COLL  =0x30,
	IVP_COLL_PK_PF  =0x31,
	IVP_COLL_PK_KK  =0x32,
	IVP_COLL_PK_NOT_MORE_PARALLEL = 0x33,
	
	IVP_COLL_KK_COLL  =0x40,
	IVP_COLL_KK_PARALLEL=0x41,
	IVP_COLL_KK_PF  =0x42
};

// type of mindist
enum IVP_MINDIST_FUNCTION {
  IVP_MF_COLLISION = 0,
  IVP_MF_PHANTOM =1
};

enum IVP_SYNAPSE_POLYGON_STATUS {
	IVP_ST_POINT = 0,
	IVP_ST_EDGE  = 1,
	IVP_ST_TRIANGLE =2,
	IVP_ST_BALL = 3,
	IVP_ST_MAX_LEGAL = 4,	// max legal status, should be 2**x
	IVP_ST_BACKSIDE = 5			// unknown, intrusion
};

#define IVP_IF(flag)	if (flag)
#define IVP_LOOP_LIST_SIZE 256
using IVP_U_MINLIST_INDEX = unsigned int;

constexpr inline float P_FLOAT_RES{1e-6f};	// float resolution for numbers < 1.0
constexpr inline float P_FLOAT_MAX{1e16f};

#define IVP_U_MINLIST_UNUSED ( (1<<16) -1 )
#define IVP_U_MINLIST_LONG_UNUSED ( (1<<16) -2 )
#define IVP_U_MINLIST_MAX_ALLOCATION ( (1<<16) - 4 )

};

#endif