// Copyright (C) Ipion Software GmbH 1999-2000. All rights reserved.

#include <ivp_physics.hxx>

#ifndef WIN32
#	pragma implementation "ivp_performancecounter.hxx"
#endif
#include <ivp_performancecounter.hxx>



void IVP_PerformanceCounter_Simple::reset_and_print_performance_counters(IVP_Time current_time){
	IVP_DOUBLE diff = count_PSIs;
	if (diff == 0.0) return;

	IVP_DOUBLE collision =  counter[IVP_PE_PSI_UNIVERSE][0]  + counter[IVP_PE_PSI_SHORT_MINDISTS][0] + 
							counter[IVP_PE_PSI_CRITICAL_MINDISTS][0]  + counter[IVP_PE_PSI_HULL][0] +
							counter[IVP_PE_AT_INIT][0];
	IVP_DOUBLE dynamics = counter[IVP_PE_PSI_CONTROLLERS][0] + counter[IVP_PE_PSI_INTEGRATORS][0] ;
	IVP_DOUBLE sum = collision + dynamics;

	IVP_DOUBLE factor = .001 / diff;

	ivp_message(	"TOT %2.1f%% %2.2f COLL %2.2f  DYN %2.2f	 det:  UNIV: %2.2f CONTR: %2.2f INTEGR: %2.2f "
			"HULL: %2.2f SHORT: %2.2f CRITIC: %2.2f AT %2.2f\n",
			sum * factor * 66.0 * (100.0 * 0.001),
			sum * factor, collision * factor , dynamics * factor,
	counter[IVP_PE_PSI_UNIVERSE][0] * factor,
	counter[IVP_PE_PSI_CONTROLLERS][0] * factor,
	counter[IVP_PE_PSI_INTEGRATORS][0] * factor,
	counter[IVP_PE_PSI_HULL][0] * factor,
	counter[IVP_PE_PSI_SHORT_MINDISTS][0] * factor,
	counter[IVP_PE_PSI_CRITICAL_MINDISTS][0] * factor,
	counter[IVP_PE_AT_INIT][0] * factor);

	P_MEM_CLEAR_M4(this);
	time_of_last_reset = current_time;
}


void IVP_PerformanceCounter_Simple::environment_is_going_to_be_deleted(IVP_Environment *){
	P_DELETE_THIS(this);
}

IVP_PerformanceCounter_Simple::~IVP_PerformanceCounter_Simple(){
}

#if !defined(WIN32)
IVP_PerformanceCounter_Simple::IVP_PerformanceCounter_Simple(){
	P_MEM_CLEAR_M4(this);
}
#endif

#if defined(WIN32) 
#ifndef WIN32_LEAN_AND_MEAN
#define	WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

IVP_PerformanceCounter_Simple::IVP_PerformanceCounter_Simple(){
	P_MEM_CLEAR_M4(this);

	QueryPerformanceFrequency(reinterpret_cast<::LARGE_INTEGER*>(&counter_freq));  // address of current frequency
}

void IVP_PerformanceCounter_Simple::pcount(IVP_PERFORMANCE_ELEMENT el){
	::LARGE_INTEGER Profile_Counter;

	if (el == IVP_PE_PSI_UNIVERSE ){
		count_PSIs++;
	}

	QueryPerformanceCounter(&Profile_Counter);

	long long diff0 = Profile_Counter.QuadPart - ref_counter64;
	ref_counter64 = Profile_Counter.QuadPart;

	counter[counting][0] += 1e6 * double(diff0) / double (counter_freq.QuadPart);
	counting = el;
}

void IVP_PerformanceCounter_Simple::start_pcount(){
	counting = IVP_PE_PSI_START;
}

void IVP_PerformanceCounter_Simple::stop_pcount(){}

#else
void IVP_PerformanceCounter_Simple::pcount( IVP_PERFORMANCE_ELEMENT ){}
void IVP_PerformanceCounter_Simple::start_pcount(){}
void IVP_PerformanceCounter_Simple::stop_pcount(){}
#endif
