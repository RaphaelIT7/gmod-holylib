#ifndef _IVP_TYPE
#define _IVP_TYPE

#include "ivp_types.h"

class IVP_Time {
    double seconds;
public:
    void operator+=(double val){
	seconds += val;
    }
    double get_seconds() const { return seconds; };
    double get_time() const { return seconds; }; // for debugging
    double operator-(const IVP_Time &b) const { return float(this->seconds - b.seconds); }
    void operator-=(const IVP_Time b) { this->seconds -= b.seconds; }
    IVP_Time operator+(double val) const { IVP_Time result; result.seconds = this->seconds + val; return result;}

    IVP_Time() = default;
    IVP_Time(double time){ seconds = time; };
};

#endif