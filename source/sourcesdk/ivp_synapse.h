#ifndef _IVP_SYNAPSE
#define _IVP_SYNAPSE

#include "ivp_types.h"

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
  IVP_Synapse 	 *next, *prev;		       // per object, only for exact/invalid synapses
  IVP_Real_Object  *l_obj;                     // back link to object
  const IVP_Compact_Edge 	*edge;		// Note: all balls share one dummy edge

protected:
    short mindist_offset;             // back link to my controlling mindist
    short status;                     // IVP_SYNAPSE_POLYGON_STATUS point, edge, tri, ball ....
/*public:
   
protected:
    // hull manager
  IVP_HULL_ELEM_TYPE get_type(){ return IVP_HULL_ELEM_POLYGON; };
  virtual void hull_limit_exceeded_event(IVP_Hull_Manager *hull_manager, IVP_HTIME hull_intrusion_value);
  virtual void hull_manager_is_going_to_be_deleted_event(IVP_Hull_Manager *hull_manager);
  virtual   void hull_manager_is_reset(IVP_FLOAT dt,IVP_FLOAT center_dt);
public:

    IVP_Real_Object *get_object(){ return l_obj; };
    IVP_SYNAPSE_POLYGON_STATUS get_status()const{ return (IVP_SYNAPSE_POLYGON_STATUS) status; };

    virtual ~IVP_Synapse(){;};			// dummy, do not call
    const IVP_Compact_Ledge *get_ledge() const;
    const IVP_Compact_Edge *get_edge() const { return edge; };

    IVP_Mindist_Base *get_synapse_mindist()const{ return  (IVP_Mindist_Base *)(mindist_offset + (char *)this);} ;
    void set_synapse_mindist( IVP_Mindist_Base *md ) { mindist_offset = ((char *)md) - (char *)this; };

    void init_synapse_real( IVP_Mindist_Base *min, IVP_Real_Object *object_to_link ){
	set_synapse_mindist(min);
	l_obj = object_to_link;
	IVP_IF(1){      next = prev = this;    }
    }*/
};

#endif