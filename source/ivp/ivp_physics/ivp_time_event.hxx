// Copyright (C) Ipion Software GmbH 1999-2000. All rights reserved.


// IVP_EXPORT_PUBLIC

#ifndef IVP_TIME_EVENT_INCLUDED
#define IVP_TIME_EVENT_INCLUDED

class IVP_Environment;

struct IVP_Time_Event {
    int index;

    IVP_Time_Event() : index{-1} {}

    // dimhotepus: Make pure instead of CORE.
    virtual void simulate_time_event(IVP_Environment *) = 0; // to be implemented by application

    virtual ~IVP_Time_Event();
};

#endif
