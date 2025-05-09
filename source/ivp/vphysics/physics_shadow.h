//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICS_SHADOW_H
#define PHYSICS_SHADOW_H
#pragma once

class CPhysicsObject;
class IPhysicsShadowController;
class IPhysicsPlayerController;

extern [[nodiscard]] IPhysicsShadowController *CreateShadowController( CPhysicsObject *pObject, bool allowTranslation, bool allowRotation );
extern [[nodiscard]] IPhysicsPlayerController *CreatePlayerController( CPhysicsObject *pObject );
extern void DestroyPlayerController( IPhysicsPlayerController *pController );


extern void ComputeController( IVP_U_Float_Point &currentSpeed, const IVP_U_Float_Point &delta, const IVP_U_Float_Point &maxSpeed, float scaleDelta, float damping );

#include "ivp_physics.hxx"

struct shadowcontrol_params_t
{
	shadowcontrol_params_t() 
	{ 
		targetPosition.set_to_zero(); 
		targetRotation.set(0.0, 0.0, 0.0f); 
		lastPosition.set_to_zero(); 
		lastImpulse.set_to_zero();
		maxAngular = FP_NAN;
		maxDampAngular = FP_NAN;
		maxSpeed = FP_NAN;
		maxDampSpeed = FP_NAN;
		dampFactor = FP_NAN;
		teleportDistance = FP_NAN;
	}

	IVP_U_Point			targetPosition;
	IVP_U_Quat			targetRotation;
	IVP_U_Point			lastPosition;		// estimate for where the object should be, used to compute error for teleport
	IVP_U_Float_Point	lastImpulse;		// Impulse applied last frame
	float				maxAngular;
	float				maxDampAngular;
	float				maxSpeed;
	float				maxDampSpeed;
	float				dampFactor;
	float				teleportDistance;
};


[[nodiscard]] float ComputeShadowControllerHL( CPhysicsObject *pObject, const hlshadowcontrol_params_t &params, float secondsToArrival, float dt );
[[nodiscard]] float ComputeShadowControllerIVP( IVP_Real_Object *pivp, shadowcontrol_params_t &params, float secondsToArrival, float dt );

#endif // PHYSICS_SHADOW_H
