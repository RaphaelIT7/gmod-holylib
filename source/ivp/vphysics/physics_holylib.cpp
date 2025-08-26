#include "cbase.h"

static CPhysicsHolyLib gpPhysicsHolyLib;
extern IVP_U_Vector<IVP_Real_Object>& _HOLYLIB_GetRecheckOVElements();
IVP_U_Vector<IVP_Real_Object>& CPhysicsHolyLib::GetRecheckOVVector()
{
	return _HOLYLIB_GetRecheckOVElements();
}

CPhysicsHolyLib* g_pPhysicsHolyLib = &gpPhysicsHolyLib;