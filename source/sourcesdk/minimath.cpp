#include "mathlib/mathlib.h"

const Vector vec3_origin(0, 0, 0);

int Q_log2(int val)
{
	int answer = 0;
	while (val >>= 1)
		answer++;
	return answer;
}

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