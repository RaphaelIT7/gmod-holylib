#ifndef PHYSICS_HOLYLIB_H
#define PHYSICS_HOLYLIB_H
#ifdef _WIN32
#pragma once
#endif

#include "ivp_holylib.hxx"
#include <utlvector.h>

class IPhysicsObject;
class CPhysicsHolyLib
{
public:
	bool HasObject(IPhysicsEnvironment* pEnvironment, IPhysicsObject* pObject);
	bool IsValidObject(IPhysicsObject* pObject);
	const CUtlVector<IPhysicsEnvironment*>& GetEnvironments();
	IPhysicsEnvironment* GetEnvironmentFromObject(IPhysicsObject* pObject);
};
extern CPhysicsHolyLib* g_pPhysicsHolyLib;

#endif // PHYSICS_HOLYLIB_H
