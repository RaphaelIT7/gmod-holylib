#include "ivp_physics.hxx"

IVP_HolyLib_Callbacks* g_pHolyLibCallbacks = NULL;

void SetIVPHolyLibCallbacks(IVP_HolyLib_Callbacks* pCallback)
{
	g_pHolyLibCallbacks = pCallback;
}