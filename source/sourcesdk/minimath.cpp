#include "mathlib/mathlib.h"
#include "Platform.hpp"

const Vector vec3_origin(0, 0, 0);
const QAngle vec3_angle(0, 0, 0);

size_t Q_log2(unsigned int val) // I hate 64x
{
	int answer = 0;
	while (val >>= 1)
		answer++;
	return answer;
}

int Q_log2(int val)
{
	return Q_log2((unsigned int)val);
}

#if ARCHITECTURE_IS_X86_64
#ifndef PLATFORM_PPC
float VectorNormalize(Vector& vec)
{
#ifdef _VPROF_MATHLIB
	VPROF_BUDGET("_VectorNormalize", "Mathlib");
#endif
	Assert(s_bMathlibInitialized);
	float radius = sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);

	// FLT_EPSILON is added to the radius to eliminate the possibility of divide by zero.
	float iradius = 1.f / (radius + FLT_EPSILON);

	vec.x *= iradius;
	vec.y *= iradius;
	vec.z *= iradius;

	return radius;
}
#endif
#endif