#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/InterfacePointers.hpp>
#include "detours.h"
#include "util.h"
#include "lua.h"
#include "player.h"
#include <sourcesdk/gamemovement.h>

class CSurfFixModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "surffix"; };
	virtual int Compatibility() { return LINUX32; };
};

static CSurfFixModule g_pSurfFixModule;
IModule* pSurfFixModule = &g_pSurfFixModule;

// Ported over from https://github.com/RaphaelIT7/obsolete-source-engine/commit/b9e4bc91086f893f4bab0decb0d358da6ada428d
// static ConVar sv_slope_fix("sv_slope_fix", "1");
static ConVar sv_ramp_fix("sv_ramp_fix", "1");
static ConVar sv_ramp_initial_retrace_length("sv_ramp_initial_retrace_length", "0.2", 0,
                                      "Amount of units used in offset for retraces", true, 0.2f, true, 5.f);
static ConVar sv_ramp_bumpcount("sv_ramp_bumpcount", "8", 0, "Helps with fixing surf/ramp bugs", true, 4, true, 16);

static bool CGameMovement_IsValidMovementTrace(CGameMovement* gamemovement, trace_t &tr)
{
    trace_t stuck;

    // Apparently we can be stuck with pm.allsolid without having valid plane info ok..
    if (tr.allsolid || tr.startsolid)
    {
        return false;
    }

    // Maybe we don't need this one
    if (CloseEnough(tr.fraction, 0.0f, FLT_EPSILON))
    {
        return false;
    }

    if (CloseEnough(tr.fraction, 0.0f, FLT_EPSILON) &&
        CloseEnough(tr.plane.normal, Vector(0.0f, 0.0f, 0.0f), FLT_EPSILON))
    {
        return false;
    }

    // Is the plane deformed or some stupid shit?
    if (fabs(tr.plane.normal.x) > 1.0f || fabs(tr.plane.normal.y) > 1.0f || fabs(tr.plane.normal.z) > 1.0f)
    {
        return false;
    }

    gamemovement->TracePlayerBBox(tr.endpos, tr.endpos, gamemovement->PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, stuck);
    if (stuck.startsolid || !CloseEnough(stuck.fraction, 1.0f, FLT_EPSILON))
    {
        return false;
    }

    return true;
}

#define	MAX_CLIP_PLANES		5
static Symbols::MoveHelperServer func_MoveHelperServer = NULL;
static Symbols::CGameMovement_ClipVelocity func_CGameMovement_ClipVelocity = NULL;
static Symbols::CBaseEntity_GetGroundEntity func_CBaseEntity_GetGroundEntity = NULL;
static Symbols::CTraceFilterSimple_ShouldHitEntity func_CTraceFilterSimple_ShouldHitEntity = NULL;
inline IMoveHelper* HolyLib_MoveHelper()
{
	return (IMoveHelper*)func_MoveHelperServer();
}

int CGameMovement::ClipVelocity(Vector& a, Vector& b, Vector& c, float d)
{
	return func_CGameMovement_ClipVelocity(this, a, b, c, d);
}

CBaseEntity* CBaseEntity::GetGroundEntity()
{
	return (CBaseEntity*)func_CBaseEntity_GetGroundEntity(this);
}

CTraceFilterSimple::CTraceFilterSimple(const IHandleEntity *passedict, int collisionGroup, ShouldHitFunc_t pExtraShouldHitFunc)
{
	m_pPassEnt = passedict;
	m_collisionGroup = collisionGroup;
	m_pExtraShouldHitCheckFunction = pExtraShouldHitFunc;
}

bool CTraceFilterSimple::ShouldHitEntity(IHandleEntity *pHandleEntity, int contentsMask)
{
	return func_CTraceFilterSimple_ShouldHitEntity(this, pHandleEntity, contentsMask);
}

inline void HolyLib_UTIL_TraceRay(const Ray_t &ray, unsigned int mask, const IHandleEntity *ignore, int collisionGroup, trace_t *ptr, ShouldHitFunc_t pExtraShouldHitCheckFn = NULL)
{
	CTraceFilterSimple traceFilter(ignore, collisionGroup, pExtraShouldHitCheckFn);

	enginetrace->TraceRay(ray, mask, &traceFilter, ptr);
	
	//if( r_visualizetraces.GetBool() )
	//{
	//	DebugDrawLine( ptr->startpos, ptr->endpos, 255, 0, 0, true, -1.0f );
	//}
}

IEngineTrace *enginetrace = NULL;
CBaseEntityList *g_pEntityList = NULL;
static Detouring::Hook detour_CGameMovement_TryPlayerMove;
static int hook_CGameMovement_TryPlayerMove(CGameMovement* gamemovement, Vector* pFirstDest, trace_t* pFirstTrace) // Raphael: We still need to support player->m_surfaceFriction or what it's name was. I removed it since I currently can't get it.
{
	int			bumpcount, numbumps;
	Vector		dir;
	float		d;
	int			numplanes;
	Vector		planes[MAX_CLIP_PLANES];
	Vector		primal_velocity, original_velocity;
	Vector      new_velocity;
	Vector		fixed_origin;
	Vector		valid_plane;
	int			i, j, h;
	trace_t		pm;
	Vector		end;
	float		time_left, allFraction;
	int			blocked;		
	bool		stuck_on_ramp;
	bool		has_valid_plane;
	
	numbumps  = sv_ramp_bumpcount.GetInt();
	
	blocked   = 0;           // Assume not blocked
	numplanes = 0;           //  and not sliding along any planes

	stuck_on_ramp = false;
	has_valid_plane = false;

	VectorCopy(gamemovement->mv->m_vecVelocity, original_velocity);  // Store original velocity
	VectorCopy(gamemovement->mv->m_vecVelocity, primal_velocity);
	VectorCopy(gamemovement->mv->GetAbsOrigin(), fixed_origin);
	
	allFraction = 0;
	time_left = gpGlobals->frametime;   // Total time for this movement operation.

	new_velocity.Init();
	valid_plane.Init();

	for (bumpcount=0 ; bumpcount < numbumps; bumpcount++)
	{
		if ( gamemovement->mv->m_vecVelocity.Length() == 0.0 )
			break;

		if (stuck_on_ramp && sv_ramp_fix.GetBool())
		{
			if (!has_valid_plane)
			{
				if (!CloseEnough(pm.plane.normal, Vector(0.0f, 0.0f, 0.0f), FLT_EPSILON) &&
					valid_plane != pm.plane.normal)
				{
					valid_plane = pm.plane.normal;
					has_valid_plane = true;
				}
				else
				{
					for (i = numplanes; i-- > 0;)
					{
						if (!CloseEnough(planes[i], Vector(0.0f, 0.0f, 0.0f), FLT_EPSILON) &&
							fabs(planes[i].x) <= 1.0f && fabs(planes[i].y) <= 1.0f && fabs(planes[i].z) <= 1.0f &&
							valid_plane != planes[i])
						{
							valid_plane = planes[i];
							has_valid_plane = true;
							break;
						}
					}
				}
			}

			if (has_valid_plane)
			{
				if (valid_plane.z >= 0.7f && valid_plane.z <= 1.0f)
				{
					gamemovement->ClipVelocity(gamemovement->mv->m_vecVelocity, valid_plane, gamemovement->mv->m_vecVelocity, 1);
					VectorCopy(gamemovement->mv->m_vecVelocity, original_velocity);
				}
				else
				{
					gamemovement->ClipVelocity(gamemovement->mv->m_vecVelocity, valid_plane, gamemovement->mv->m_vecVelocity, 1.0f); // Raphael: Fix this line
					VectorCopy(gamemovement->mv->m_vecVelocity, original_velocity);
				}
			}
			else // We were actually going to be stuck, lets try and find a valid plane..
			{
				// this way we know fixed_origin isn't going to be stuck
				float offsets[] = {(bumpcount * 2) * -sv_ramp_initial_retrace_length.GetFloat(), 0.0f,
								   (bumpcount * 2) * sv_ramp_initial_retrace_length.GetFloat()};
				int valid_planes = 0;
				valid_plane.Init(0.0f, 0.0f, 0.0f);

				// we have 0 plane info, so lets increase our bbox and search in all 27 directions to get a valid plane!
				for (i = 0; i < 3; ++i)
				{
					for (j = 0; j < 3; ++j)
					{
						for (h = 0; h < 3; ++h)
						{
							Vector offset = {offsets[i], offsets[j], offsets[h]};

							Vector offset_mins = offset / 2.0f;
							Vector offset_maxs = offset / 2.0f;

							if (offset.x > 0.0f)
								offset_mins.x /= 2.0f;
							if (offset.y > 0.0f)
								offset_mins.y /= 2.0f;
							if (offset.z > 0.0f)
								offset_mins.z /= 2.0f;

							if (offset.x < 0.0f)
								offset_maxs.x /= 2.0f;
							if (offset.y < 0.0f)
								offset_maxs.y /= 2.0f;
							if (offset.z < 0.0f)
								offset_maxs.z /= 2.0f;

							Ray_t ray;
							ray.Init(fixed_origin + offset, end - offset, gamemovement->GetPlayerMins() - offset_mins,
									 gamemovement->GetPlayerMaxs() + offset_maxs);
							HolyLib_UTIL_TraceRay(ray, gamemovement->PlayerSolidMask(), gamemovement->mv->m_nPlayerHandle.Get(),
										  COLLISION_GROUP_PLAYER_MOVEMENT, &pm);

							// Only use non deformed planes and planes with values where the start point is not from a
							// solid
							if (fabs(pm.plane.normal.x) <= 1.0f && fabs(pm.plane.normal.y) <= 1.0f &&
								fabs(pm.plane.normal.z) <= 1.0f && pm.fraction > 0.0f && pm.fraction < 1.0f &&
								!pm.startsolid)
							{
								valid_planes++;
								valid_plane += pm.plane.normal;
							}
						}
					}
				}

				if (valid_planes && !CloseEnough(valid_plane, Vector(0.0f, 0.0f, 0.0f), FLT_EPSILON))
				{
					has_valid_plane = true;
					valid_plane.NormalizeInPlace();
					continue;
				}
			}

			if (has_valid_plane)
			{
				VectorMA(fixed_origin, sv_ramp_initial_retrace_length.GetFloat(), valid_plane, fixed_origin);
			}
			else
			{
				stuck_on_ramp = false;
				continue;
			}
		}

		// Assume we can move all the way from the current origin to the
		//  end point.
		VectorMA( gamemovement->mv->GetAbsOrigin(), time_left, gamemovement->mv->m_vecVelocity, end );

		if (pFirstDest && end == *pFirstDest)
			pm = *pFirstTrace;
		else
		{
#if defined(PLAYER_GETTING_STUCK_TESTING)
			trace_t foo;
			TracePlayerBBox(mv->GetAbsOrigin(), mv->GetAbsOrigin(), PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT,
							foo);
			if (foo.startsolid || foo.fraction != 1.0f)
			{
				Msg("bah\n");
			}
#endif
			if (stuck_on_ramp && has_valid_plane && sv_ramp_fix.GetBool())
			{
				gamemovement->TracePlayerBBox(fixed_origin, end, gamemovement->PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm);
				pm.plane.normal = valid_plane;
			}
			else
			{
				gamemovement->TracePlayerBBox(gamemovement->mv->GetAbsOrigin(), end, gamemovement->PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm);
			}
		}

		if (bumpcount && sv_ramp_fix.GetBool() && gamemovement->player->GetGroundEntity() == nullptr && !CGameMovement_IsValidMovementTrace(gamemovement, pm))
		{
			has_valid_plane = false;
			stuck_on_ramp = true;
			continue;
		}

		// If we started in a solid object, or we were in solid space
		//  the whole way, zero out our velocity and return that we
		//  are blocked by floor and wall.
		if (pm.allsolid && !sv_ramp_fix.GetBool())
		{
			// entity is trapped in another solid
			VectorCopy (vec3_origin, gamemovement->mv->m_vecVelocity);
			return 4;
		}

		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove.origin and 
		//  zero the plane counter.
		if (pm.fraction > 0.0f)
		{
			if ((!bumpcount || gamemovement->player->GetGroundEntity() != nullptr || !sv_ramp_fix.GetBool()) && numbumps > 0 && pm.fraction == 1.0f)
			{
				// There's a precision issue with terrain tracing that can cause a swept box to successfully trace
				// when the end position is stuck in the triangle.  Re-run the test with an unswept box to catch that
				// case until the bug is fixed.
				// If we detect getting stuck, don't allow the movement
				trace_t stuck;
				gamemovement->TracePlayerBBox(pm.endpos, pm.endpos, gamemovement->PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, stuck);

				if ((stuck.startsolid || stuck.fraction != 1.0f) && !bumpcount && sv_ramp_fix.GetBool())
				{
					has_valid_plane = false;
					stuck_on_ramp = true;
					continue;
				}
				else if (stuck.startsolid || stuck.fraction != 1.0f)
				{
					Msg("Player will become stuck!!! allfrac: %f pm: %i, %f, %f, %f vs stuck: %i, %f, %f\n",
						allFraction, pm.startsolid, pm.fraction, pm.plane.normal.z, pm.fractionleftsolid,
						stuck.startsolid, stuck.fraction, stuck.plane.normal.z);
					VectorCopy(vec3_origin, gamemovement->mv->m_vecVelocity);
					break;
				}
			}

#if defined(PLAYER_GETTING_STUCK_TESTING)
			trace_t foo;
			TracePlayerBBox(pm.endpos, pm.endpos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, foo);
			if (foo.startsolid || foo.fraction != 1.0f)
			{
				Msg("Player will become stuck!!!\n");
			}
#endif
			if (sv_ramp_fix.GetBool())
			{
				has_valid_plane = false;
				stuck_on_ramp = false;
			}

			// actually covered some distance
			VectorCopy(gamemovement->mv->m_vecVelocity, original_velocity);
			gamemovement->mv->SetAbsOrigin(pm.endpos);
			VectorCopy(gamemovement->mv->GetAbsOrigin(), fixed_origin);
			allFraction += pm.fraction;
			numplanes = 0;
		}

		// If we covered the entire distance, we are done
		//  and can return.
#ifdef BUILD_GMOD
		if (CloseEnough(pm.fraction, 1.0f, FLT_EPSILON))
#else
		if (pm.fraction == 1)
#endif
		{
			 break;		// moved the entire distance
		}

		// Save entity that blocked us (since fraction was < 1.0)
		//  for contact
		// Add it if it's not already in the list!!!
		HolyLib_MoveHelper()->AddToTouched( pm, gamemovement->mv->m_vecVelocity );

		// If the plane we hit has a high z component in the normal, then
		//  it's probably a floor
		if (pm.plane.normal[2] > 0.7)
		{
			blocked |= 1;		// floor
		}
		// If the plane has a zero z component in the normal, then it's a 
		//  step or wall
		if (!pm.plane.normal[2])
		{
			blocked |= 2;		// step / wall
		}

		// Reduce amount of m_flFrameTime left by total time left * fraction
		//  that we covered.
		time_left -= time_left * pm.fraction;

		// Did we run out of planes to clip against?
		if (numplanes >= MAX_CLIP_PLANES)
		{	
			// this shouldn't really happen
			//  Stop our movement if so.
			VectorCopy (vec3_origin, gamemovement->mv->m_vecVelocity);
			//Con_DPrintf("Too many planes 4\n");

			break;
		}

		// Set up next clipping plane
		VectorCopy (pm.plane.normal, planes[numplanes]);
		++numplanes;

		// modify original_velocity so it parallels all of the clip planes
		//

		// reflect player velocity 
		// Only give this a try for first impact plane because you can get yourself stuck in an acute corner by jumping in place
		//  and pressing forward and nobody was really using this bounce/reflection feature anyway...
		if ( numplanes == 1 &&
			gamemovement->player->GetMoveType() == MOVETYPE_WALK &&
			gamemovement->player->GetGroundEntity() == NULL )	
		{
			for ( i = 0; i < numplanes; i++ )
			{
				if ( planes[i][2] > 0.7  )
				{
					// floor or slope
					gamemovement->ClipVelocity( original_velocity, planes[i], new_velocity, 1 );
					VectorCopy( new_velocity, original_velocity );
				}
				else
				{
					gamemovement->ClipVelocity( original_velocity, planes[i], new_velocity, 1.0 + (1 - 1.0f) ); // Raphael: Fix this line
				}
			}

			VectorCopy( new_velocity, gamemovement->mv->m_vecVelocity );
			VectorCopy( new_velocity, original_velocity );
		}
		else
		{
			for (i=0 ; i < numplanes ; i++)
			{
				gamemovement->ClipVelocity (
					original_velocity,
					planes[i],
					gamemovement->mv->m_vecVelocity,
					1);

				for (j=0 ; j<numplanes ; j++)
					if (j != i)
					{
						// Are we now moving against this plane?
						if (gamemovement->mv->m_vecVelocity.Dot(planes[j]) < 0)
							break;	// not ok
					}
				if (j == numplanes)  // Didn't have to clip, so we're ok
					break;
			}
			
			// Did we go all the way through plane set
			if (i != numplanes)
			{	// go along this plane
				// pmove.velocity is set in clipping call, no need to set again.
				;  
			}
			else
			{	// go along the crease
				if (numplanes != 2)
				{
					VectorCopy (vec3_origin, gamemovement->mv->m_vecVelocity);
					break;
				}
				CrossProduct (planes[0], planes[1], dir);
				dir.NormalizeInPlace();
				d = dir.Dot(gamemovement->mv->m_vecVelocity);
				VectorScale (dir, d, gamemovement->mv->m_vecVelocity );
			}

			//
			// if original velocity is against the original velocity, stop dead
			// to avoid tiny occilations in sloping corners
			//
			d = gamemovement->mv->m_vecVelocity.Dot(primal_velocity);
			if (d <= 0)
			{
				//Con_DPrintf("Back\n");
				VectorCopy (vec3_origin, gamemovement->mv->m_vecVelocity);
				break;
			}
		}
	}

	if ( allFraction == 0 )
	{
		VectorCopy (vec3_origin, gamemovement->mv->m_vecVelocity);
	}

	// Check if they slammed into a wall
	float fSlamVol = 0.0f;

	float fLateralStoppingAmount = primal_velocity.Length2D() - gamemovement->mv->m_vecVelocity.Length2D();
	if ( fLateralStoppingAmount > PLAYER_MAX_SAFE_FALL_SPEED * 2.0f )
	{
		fSlamVol = 1.0f;
	}
	else if ( fLateralStoppingAmount > PLAYER_MAX_SAFE_FALL_SPEED )
	{
		fSlamVol = 0.85f;
	}

	gamemovement->PlayerRoughLandingEffects( fSlamVol );

	return blocked;
}

void CSurfFixModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	enginetrace = (IEngineTrace*)appfn[0](INTERFACEVERSION_ENGINETRACE_SERVER, NULL);
	Detour::CheckValue("get interface", "enginetrace", enginetrace != NULL);
}

void CSurfFixModule::InitDetour(bool bPreServer)
{
	if ( bPreServer ) { return; }
	if ( !gpGlobals ) { return; }

	SourceSDK::ModuleLoader server_loader("server_srv");
	Detour::Create(
		&detour_CGameMovement_TryPlayerMove, "CGameMovement::TryPlayerMove",
		server_loader.GetModule(), Symbols::CGameMovement_TryPlayerMoveSym,
		(void*)hook_CGameMovement_TryPlayerMove, m_pID
	);

	func_MoveHelperServer = (Symbols::MoveHelperServer)Detour::GetFunction(server_loader.GetModule(), Symbols::MoveHelperServerSym);
	Detour::CheckFunction((void*)func_MoveHelperServer, "MoveHelperServer");

	func_CGameMovement_ClipVelocity = (Symbols::CGameMovement_ClipVelocity)Detour::GetFunction(server_loader.GetModule(), Symbols::CGameMovement_ClipVelocitySym);
	Detour::CheckFunction((void*)func_CGameMovement_ClipVelocity, "CGameMovement:ClipVelocity");

	func_CBaseEntity_GetGroundEntity = (Symbols::CBaseEntity_GetGroundEntity)Detour::GetFunction(server_loader.GetModule(), Symbols::CBaseEntity_GetGroundEntitySym);
	Detour::CheckFunction((void*)func_CBaseEntity_GetGroundEntity, "CBaseEntity::GetGroundEntity");

	func_CTraceFilterSimple_ShouldHitEntity = (Symbols::CTraceFilterSimple_ShouldHitEntity)Detour::GetFunction(server_loader.GetModule(), Symbols::CTraceFilterSimple_ShouldHitEntitySym);
	Detour::CheckFunction((void*)func_CTraceFilterSimple_ShouldHitEntity, "CTraceFilterSimple::ShouldHitEntitySym");

	SourceSDK::FactoryLoader server_loaderfactory("server_srv");
	g_pEntityList = Detour::ResolveSymbol<CBaseEntityList>(server_loaderfactory, Symbols::g_pEntityListSym);
	Detour::CheckValue("get class", "g_pEntityList", g_pEntityList != NULL);
}