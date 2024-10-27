#ifndef _IVP_TYPES
#define _IVP_TYPES

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
    IVP_MT_SLOW = 0x02,	          // slow
    IVP_MT_CALM = 0x03, 	  // slow for more than a certain time span (currently 1 second)
    IVP_MT_NOT_SIM =0x08,	  // not simulated, but can be changed to be simulated
    IVP_MT_STATIC_PHANTOM = 0x09, // like static, but object may be moved by setting the matrix directly
    IVP_MT_STATIC = 0x10,	  // static object
    IVP_MT_GET_MINDIST = 0x21     // not really a movement type, used to get all mindists with recheck_ov_element
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
#endif