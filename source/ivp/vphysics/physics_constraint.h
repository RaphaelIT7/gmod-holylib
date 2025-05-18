//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICS_CONSTRAINT_H
#define PHYSICS_CONSTRAINT_H
#pragma once

class IVP_Environment;

class CPhysicsObject;
class IPhysicsConstraint;
class IPhysicsConstraintGroup;

[[nodiscard]] extern IPhysicsConstraint *CreateRagdollConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ragdollparams_t &ragdoll );
[[nodiscard]] extern IPhysicsConstraint *CreateHingeConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_limitedhingeparams_t &ragdoll );
[[nodiscard]] extern IPhysicsConstraint *CreateFixedConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_fixedparams_t &fixed );
[[nodiscard]] extern IPhysicsConstraint *CreateBallsocketConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ballsocketparams_t &ballsocket );
[[nodiscard]] extern IPhysicsConstraint *CreateSlidingConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_slidingparams_t &slide );
[[nodiscard]] extern IPhysicsConstraint *CreatePulleyConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_pulleyparams_t &pulley );
[[nodiscard]] extern IPhysicsConstraint *CreateLengthConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_lengthparams_t &length );

[[nodiscard]] extern IPhysicsConstraintGroup *CreatePhysicsConstraintGroup( IVP_Environment *pEnvironment, const constraint_groupparams_t &group );

[[nodiscard]] extern IPhysicsConstraint *GetClientDataForHkConstraint( class hk_Breakable_Constraint *pHkConstraint );

[[nodiscard]] extern bool IsExternalConstraint( IVP_Controller *pLCS, void *pGameData );

#endif // PHYSICS_CONSTRAINT_H
