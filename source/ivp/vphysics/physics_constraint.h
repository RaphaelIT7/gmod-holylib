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

extern [[nodiscard]] IPhysicsConstraint *CreateRagdollConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ragdollparams_t &ragdoll );
extern [[nodiscard]] IPhysicsConstraint *CreateHingeConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_limitedhingeparams_t &ragdoll );
extern [[nodiscard]] IPhysicsConstraint *CreateFixedConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_fixedparams_t &fixed );
extern [[nodiscard]] IPhysicsConstraint *CreateBallsocketConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_ballsocketparams_t &ballsocket );
extern [[nodiscard]] IPhysicsConstraint *CreateSlidingConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_slidingparams_t &slide );
extern [[nodiscard]] IPhysicsConstraint *CreatePulleyConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_pulleyparams_t &pulley );
extern [[nodiscard]] IPhysicsConstraint *CreateLengthConstraint( IVP_Environment *pEnvironment, CPhysicsObject *pReferenceObject, CPhysicsObject *pAttachedObject, IPhysicsConstraintGroup *pGroup, const constraint_lengthparams_t &length );

extern [[nodiscard]] IPhysicsConstraintGroup *CreatePhysicsConstraintGroup( IVP_Environment *pEnvironment, const constraint_groupparams_t &group );

extern [[nodiscard]] IPhysicsConstraint *GetClientDataForHkConstraint( class hk_Breakable_Constraint *pHkConstraint );

extern [[nodiscard]] bool IsExternalConstraint( IVP_Controller *pLCS, void *pGameData );

#endif // PHYSICS_CONSTRAINT_H
