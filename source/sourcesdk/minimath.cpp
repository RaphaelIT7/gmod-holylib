#include "mathlib/mathlib.h"

const Vector vec3_origin(0, 0, 0);

int Q_log2(int val)
{
	int answer = 0;
	while (val >>= 1)
		answer++;
	return answer;
}