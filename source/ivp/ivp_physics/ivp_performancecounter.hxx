// Copyright (C) Ipion Software GmbH 1999-2000. All rights reserved.

// IVP_EXPORT_PUBLIC

/********************************************************************************
 *  Filename:	    ivp_performancecounter.hxx
 *  Description:    This file provides you with an abstract class to count 
 *					the performance of the physics engine
 ********************************************************************************/

#ifndef IVP_PERFROMANCECOUNTER_INCLUDED
#define IVP_PERFROMANCECOUNTER_INCLUDED


#define IVP_ENABLE_PERFORMANCE_COUNTER 

enum IVP_PERFORMANCE_ELEMENT {
	IVP_PE_PSI_START = 0,   // not used by pcount
	
	IVP_PE_PSI_UNIVERSE,
	IVP_PE_PSI_CONTROLLERS,
	IVP_PE_PSI_INTEGRATORS,
	IVP_PE_PSI_HULL,
	IVP_PE_PSI_SHORT_MINDISTS,
	IVP_PE_PSI_CRITICAL_MINDISTS,
	IVP_PE_PSI_END,

	IVP_PE_AT_INIT,
	IVP_PE_AT_COLLISION,
	IVP_PE_AT_INTEGRATORS,
	IVP_PE_AT_HULL,
	IVP_PE_AT_SHORT_MINDISTS,
	IVP_PE_AT_CRITICAL_MINDISTS,
	IVP_PE_AT_END,

	IVP_PE_USR1,
	IVP_PE_USR2,

	IVP_PE_MAX
};

struct IVP_PerformanceCounter {
	virtual void start_pcount() = 0;
	virtual void pcount( IVP_PERFORMANCE_ELEMENT ) = 0;
	virtual void stop_pcount() = 0;

	virtual void environment_is_going_to_be_deleted(IVP_Environment *) = 0;
	virtual void reset_and_print_performance_counters(IVP_Time current_time) = 0;

	IVP_PerformanceCounter(){}
	virtual ~IVP_PerformanceCounter(){}
};


/********************************************************************************
 *	Name:	       	a simple implementation 
 ********************************************************************************/
class IVP_PerformanceCounter_Simple : public IVP_PerformanceCounter {
public:
#ifdef WIN32
    __int64 ref_counter64;

	typedef union _LARGE_INTEGER {
		struct {
			unsigned long LowPart;
			long HighPart;
		} DUMMYSTRUCTNAME;
		struct {
			unsigned long LowPart;
			long HighPart;
		} u;
		long long QuadPart;
	} LARGE_INTEGER;

    LARGE_INTEGER counter_freq;
#endif

    IVP_PERFORMANCE_ELEMENT counting;
	int count_PSIs;

	int counter[IVP_PE_MAX][2]; // counting seconds
	IVP_Time time_of_last_reset;


	void reset_and_print_performance_counters(IVP_Time current_time) override;

	void start_pcount() override;
	void pcount( IVP_PERFORMANCE_ELEMENT ) override;
	void stop_pcount() override;

	void environment_is_going_to_be_deleted(IVP_Environment *) override;

	IVP_PerformanceCounter_Simple();
	virtual ~IVP_PerformanceCounter_Simple();
};





#endif
