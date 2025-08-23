#ifndef PHYSICS_HOLYLIB_H
#define PHYSICS_HOLYLIB_H
#ifdef _WIN32
#pragma once
#endif

#include "ivp_holylib.hxx"
#include <ivp_physics.hxx>
#include "ivu_vector.hxx"
#include <utlvector.h>

class IPhysicsObject;
class IPhysicsCollisionSolver;
class IPhysicsConstraintEvent;
class CPhysicsHolyLib
{
public:
	bool HasObject(IPhysicsEnvironment* pEnvironment, IPhysicsObject* pObject);
	bool IsValidObject(IPhysicsObject* pObject);
	IPhysicsCollisionSolver* GetCollisionSolver(IPhysicsEnvironment* pEnvironment);
	IPhysicsConstraintEvent* GetPhysicsListenerConstraint(IPhysicsEnvironment* pEnvironment);
	const CUtlVector<IPhysicsEnvironment*>& GetEnvironments();
	IPhysicsEnvironment* GetEnvironmentFromObject(IPhysicsObject* pObject);
	IVP_U_Vector<IVP_Real_Object>& GetRecheckOVVector();
};
extern CPhysicsHolyLib* g_pPhysicsHolyLib;

#endif // PHYSICS_HOLYLIB_H
