//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"

#include "vcollide_parse_private.h"

#include "tier1/strtools.h" 
#include "vphysics/constraints.h"
#include "vphysics/vehicles.h"
#include "filesystem_helpers.h"
#include "bspfile.h"
#include "tier1/utlbuffer.h"

#include "filesystem.h"
#include "characterset.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static void ReadVector( const char *pString, Vector& out )
{
	float x = 0, y = 0, z = 0;
	int read = sscanf( pString, "%f %f %f", &x, &y, &z );

	//AssertMsg( read == 3, "Unable to read 3D vector components from '%s'.", pString );

	out[0] = x;
	out[1] = y;
	out[2] = z;
}

// dimhotepus: Unused.
//static void ReadAngles( const char *pString, QAngle& out )
//{
//	float x = 0, y = 0, z = 0;
//	sscanf( pString, "%f %f %f", &x, &y, &z );
//	out[0] = x;
//	out[1] = y;
//	out[2] = z;
//}

static void ReadVector4D( const char *pString, Vector4D& out )
{
	float x = 0, y = 0, z = 0, w = 0;
	int read = sscanf( pString, "%f %f %f %f", &x, &y, &z, &w );

	//AssertMsg( read == 4, "Unable to read 4D vector components from '%s'.", pString );

	out[0] = x;
	out[1] = y;
	out[2] = z;
	out[3] = w;
}

class CVPhysicsParse final : public IVPhysicsKeyParser
{
public:
				~CVPhysicsParse() {}

				CVPhysicsParse( const char *pKeyData );
	void		NextBlock( void );

	const char *GetCurrentBlockName( void ) override;
	bool		Finished( void ) override;
	void		ParseSolid( solid_t *pSolid, IVPhysicsKeyHandler *unknownKeyHandler ) override;
	void		ParseFluid( fluid_t *pFluid, IVPhysicsKeyHandler *unknownKeyHandler ) override;
	void		ParseRagdollConstraint( constraint_ragdollparams_t *pConstraint, IVPhysicsKeyHandler *unknownKeyHandler ) override;
	void		ParseSurfaceTable( int *table, IVPhysicsKeyHandler *unknownKeyHandler ) override;
	void		ParseSurfaceTablePacked( CUtlVector<char> &out );
	void		ParseVehicle( vehicleparams_t *pVehicle, IVPhysicsKeyHandler *unknownKeyHandler ) override;
	void		ParseCustom( void *pCustom, IVPhysicsKeyHandler *unknownKeyHandler ) override;
	void		SkipBlock( void ) override { ParseCustom(NULL, NULL); }
#if PLATFORM_64BITS
	void		ParseCollisionRules( ragdollcollisionrules_t *pRules, IVPhysicsKeyHandler *unknownKeyHandler ) override;
	void		ParseRagdollAnimatedFriction( ragdollanimatedfriction_t *pFriction, IVPhysicsKeyHandler *unknownKeyHandler ) override;
#endif

private:
	void		ParseVehicleAxle( vehicle_axleparams_t &axle );
	void		ParseVehicleWheel( vehicle_wheelparams_t &wheel );
	void		ParseVehicleSuspension( vehicle_suspensionparams_t &suspension );
	void		ParseVehicleBody( vehicle_bodyparams_t &body );
	void		ParseVehicleEngine( vehicle_engineparams_t &engine );
	void		ParseVehicleEngineBoost( vehicle_engineparams_t &engine );
	void		ParseVehicleSteering( vehicle_steeringparams_t &steering );

	const char *m_pText;
	char m_blockName[MAX_KEYVALUE];
};


CVPhysicsParse::CVPhysicsParse( const char *pKeyData )
{
	DebugPrint();
	m_pText = pKeyData;
	NextBlock();
}

void CVPhysicsParse::NextBlock( void )
{
	DebugPrint();
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( !Q_strcmp(value, "{") )
		{
			V_strcpy_safe( m_blockName, key );
			return;
		}
	}

	// Make sure m_blockName is initialized -- should this be done?
	m_blockName[ 0 ] = 0;
}


const char *CVPhysicsParse::GetCurrentBlockName( void )
{
	DebugPrint();
	DebugMsg3("Text: %s\nBlockName: %s\n", m_pText ? m_pText : "[NULL]", (const char*)m_blockName);
	if ( m_pText )
		return m_blockName;

	return NULL;
}


bool CVPhysicsParse::Finished( void )
{
	DebugPrint();
	if ( m_pText )
		return false;

	return true;
}


void CVPhysicsParse::ParseSolid( solid_t *pSolid, IVPhysicsKeyHandler *unknownKeyHandler )
{
	DebugPrint();
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];
	key[0] = 0;

	if ( unknownKeyHandler )
	{
		unknownKeyHandler->SetDefaults( pSolid );
	}
	else
	{
		memset( pSolid, 0, sizeof(*pSolid) );
	}
	
	// disable these until the ragdoll is created
	pSolid->params.enableCollisions = false;

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			DebugMsg2("Solid Index: %i\n", pSolid->index);
			DebugMsg2("Read surface: \"%s\"\n", pSolid->surfaceprop);
			return;
		}

		DebugMsg2("Key: %s\n", key);
		DebugMsg2("Value: %s\n", value);
		if ( !Q_stricmp( key, "index" ) )
		{
			pSolid->index = strtol(value, NULL, 10);
		}
		else if ( !Q_stricmp( key, "name" ) )
		{
			Q_strncpy( pSolid->name, value, sizeof(pSolid->name) );
		}
		else if ( !Q_stricmp( key, "parent" ) )
		{
			Q_strncpy( pSolid->parent, value, sizeof(pSolid->parent) );
		}
		else if ( !Q_stricmp( key, "surfaceprop" ) )
		{
			Q_strncpy( pSolid->surfaceprop, value, sizeof(pSolid->surfaceprop) );
		}
		else if ( !Q_stricmp( key, "mass" ) )
		{
			pSolid->params.mass = strtod(value, nullptr);
		}
		else if ( !Q_stricmp( key, "massCenterOverride" ) )
		{
			ReadVector( value, pSolid->massCenterOverride );
			pSolid->params.massCenterOverride = &pSolid->massCenterOverride;
		}
		else if ( !Q_stricmp( key, "inertia" ) )
		{
			pSolid->params.inertia = strtod(value, nullptr);
		}
		else if ( !Q_stricmp( key, "damping" ) )
		{
			pSolid->params.damping = strtod(value, nullptr);
		}
		else if ( !Q_stricmp( key, "rotdamping" ) )
		{
			pSolid->params.rotdamping = strtod(value, nullptr);
		}
		else if ( !Q_stricmp( key, "volume" ) )
		{
			pSolid->params.volume = strtod(value, nullptr);
		}
		else if ( !Q_stricmp( key, "drag" ) )
		{
			pSolid->params.dragCoefficient = strtod(value, nullptr);
		}
		else if ( !Q_stricmp( key, "rollingdrag" ) )
		{
			//AssertMsg( false, "Solid '%s' rolling drag is not implemented.", pSolid->name );
			//pSolid->params.rollingDrag = strtod(value, nullptr);
		}
		else
		{
			if ( unknownKeyHandler )
			{
				unknownKeyHandler->ParseKeyValue( pSolid, key, value );
			}
			else
			{
				// dimhotepus: Need to look through data.
				DevWarning( "vphysics - Unknown solid '%s' configuration key '%s' (%s).\n", pSolid->name, key, value );
			}
		}
	}
}

void CVPhysicsParse::ParseRagdollConstraint( constraint_ragdollparams_t *pConstraint, IVPhysicsKeyHandler *unknownKeyHandler )
{
	DebugPrint();
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	if ( unknownKeyHandler )
	{
		unknownKeyHandler->SetDefaults( pConstraint );
	}
	else
	{
		memset( pConstraint, 0, sizeof(*pConstraint) );
		pConstraint->childIndex = -1;
		pConstraint->parentIndex = -1;
	}

	// BUGBUG: xmin/xmax, ymin/ymax, zmin/zmax specify clockwise rotations.  
	// BUGBUG: HL rotations are counter-clockwise, so reverse these limits at some point!!!
	pConstraint->useClockwiseRotations = true;

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			return;
		}

		if ( !Q_stricmp( key, "parent" ) )
		{
			pConstraint->parentIndex = atoi( value );
		}
		else if ( !Q_stricmp( key, "child" ) )
		{
			pConstraint->childIndex = atoi( value );
		}
		else if ( !Q_stricmp( key, "xmin" ) )
		{
			pConstraint->axes[0].minRotation = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "xmax" ) )
		{
			pConstraint->axes[0].maxRotation = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "xfriction" ) )
		{
			pConstraint->axes[0].angularVelocity = 0;
			pConstraint->axes[0].torque = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "ymin" ) )
		{
			pConstraint->axes[1].minRotation = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "ymax" ) )
		{
			pConstraint->axes[1].maxRotation = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "yfriction" ) )
		{
			pConstraint->axes[1].angularVelocity = 0;
			pConstraint->axes[1].torque = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "zmin" ) )
		{
			pConstraint->axes[2].minRotation = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "zmax" ) )
		{
			pConstraint->axes[2].maxRotation = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "zfriction" ) )
		{
			pConstraint->axes[2].angularVelocity = 0;
			pConstraint->axes[2].torque = strtof( value, nullptr );
		}
		else
		{
			if ( unknownKeyHandler )
			{
				unknownKeyHandler->ParseKeyValue( pConstraint, key, value );
			}
			else
			{
				// dimhotepus: Need to look through data.
				DevWarning( "vphysics - Unknown ragdoll constraint configuration key '%s' (%s).\n", key, value );
			}
		}
	}
}


void CVPhysicsParse::ParseFluid( fluid_t *pFluid, IVPhysicsKeyHandler *unknownKeyHandler )
{
	DebugPrint();
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	pFluid->index = -1;
	if ( unknownKeyHandler )
	{
		unknownKeyHandler->SetDefaults( pFluid );
	}
	else
	{
		memset( pFluid, 0, sizeof(*pFluid) );
		// HACKHACK: This is a reasonable default even though it is hardcoded
		V_strcpy_safe( pFluid->surfaceprop, "water" );
	}

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			return;
		}

		if ( !Q_stricmp( key, "index" ) )
		{
			pFluid->index = atoi( value );
		}
		else if ( !Q_stricmp( key, "damping" ) )
		{
			pFluid->params.damping = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "surfaceplane" ) )
		{
			ReadVector4D( value, pFluid->params.surfacePlane );
		}
		else if ( !Q_stricmp( key, "currentvelocity" ) )
		{
			ReadVector( value, pFluid->params.currentVelocity );
		}
		else if ( !Q_stricmp( key, "contents" ) )
		{
			pFluid->params.contents = atoi( value );
		}
		else if ( !Q_stricmp( key, "surfaceprop" ) )
		{
			V_strcpy_safe( pFluid->surfaceprop, value );
		}
		else
		{
			if ( unknownKeyHandler )
			{
				unknownKeyHandler->ParseKeyValue( pFluid, key, value );
			}
			else
			{
				// dimhotepus: Need to look through data.
				DevWarning( "vphysics - Unknown fluid configuration key '%s' (%s).\n", key, value );
			}
		}
	}
}

void CVPhysicsParse::ParseSurfaceTable( int *table, IVPhysicsKeyHandler *unknownKeyHandler )
{
	DebugPrint();
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			return;
		}

		int propIndex = physprops->GetSurfaceIndex( key );
		int tableIndex = atoi(value);
		if ( tableIndex >= 0 && tableIndex < 128 )
		{
			table[tableIndex] = propIndex;
		}
	}
}

void CVPhysicsParse::ParseSurfaceTablePacked( CUtlVector<char> &out )
{
	DebugPrint();
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	[[maybe_unused]] int lastIndex = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			return;
		}

		int len = Q_strlen( key );
		int outIndex = out.AddMultipleToTail( len + 1 );
		memcpy( &out[outIndex], key, len+1 );
		int tableIndex = atoi(value);
		Assert( tableIndex == lastIndex + 1);
		lastIndex = tableIndex;
	}
}

void CVPhysicsParse::ParseVehicleAxle( vehicle_axleparams_t &axle )
{
	DebugPrint();
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	memset( &axle, 0, sizeof(axle) );
	key[0] = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;

		// parse subchunks
		if ( value[0] == '{' )
		{
			if ( !Q_stricmp( key, "wheel" ) )
			{
				ParseVehicleWheel( axle.wheels );
			}
			else if ( !Q_stricmp( key, "suspension" ) )
			{
				ParseVehicleSuspension( axle.suspension );
			}
			else
			{
				SkipBlock();
			}
		}
		else if ( !Q_stricmp( key, "offset" ) )
		{
			ReadVector( value, axle.offset );
		}
		else if ( !Q_stricmp( key, "wheeloffset" ) )
		{
			ReadVector( value, axle.wheelOffset );
		}
		else if ( !Q_stricmp( key, "torquefactor" ) )
		{
			axle.torqueFactor = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "brakefactor" ) )
		{
			axle.brakeFactor = strtof( value, nullptr );
		}
		else
		{
			// dimhotepus: Need to look through data.
			DevWarning( "vphysics - Unknown vehicle axie configuration key '%s' (%s).\n", key, value );
		}
	}
}

void CVPhysicsParse::ParseVehicleWheel( vehicle_wheelparams_t &wheel )
{
	DebugPrint();
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;
		
		if ( !Q_stricmp( key, "radius" ) )
		{
			wheel.radius = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "mass" ) )
		{
			wheel.mass = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "inertia" ) )
		{
			wheel.inertia = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "damping" ) )
		{
			wheel.damping = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "rotdamping" ) )
		{
			wheel.rotdamping = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "frictionscale" ) )
		{
			wheel.frictionScale = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "material" ) )
		{
			wheel.materialIndex = physprops->GetSurfaceIndex( value );
		}
		else if ( !Q_stricmp( key, "skidmaterial" ) )
		{
			wheel.skidMaterialIndex = physprops->GetSurfaceIndex( value );
		}
		else if ( !Q_stricmp( key, "brakematerial" ) )
		{
			wheel.brakeMaterialIndex = physprops->GetSurfaceIndex( value );
		}
		else
		{
			DevWarning( "vphysics - Unknown vehicle wheel parser configuration key '%s' (%s).\n", key, value );
		}
	}
}


void CVPhysicsParse::ParseVehicleSuspension( vehicle_suspensionparams_t &suspension )
{
	DebugPrint();
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;
		
		if ( !Q_stricmp( key, "springconstant" ) )
		{
			suspension.springConstant = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "springdamping" ) )
		{
			suspension.springDamping = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "stabilizerconstant" ) )
		{
			suspension.stabilizerConstant = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "springdampingcompression" ) )
		{
			suspension.springDampingCompression = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "maxbodyforce" ) )
		{
			suspension.maxBodyForce = strtof( value, nullptr );
		}
		else
		{
			DevWarning( "vphysics - Unknown vehicle suspension parser configuration key '%s' (%s).\n", key, value);
		}
	}
}


void CVPhysicsParse::ParseVehicleBody( vehicle_bodyparams_t &body )
{
	DebugPrint();
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;
		
		if ( !Q_stricmp( key, "massCenterOverride" ) )
		{
			ReadVector( value, body.massCenterOverride );
		}
		else if ( !Q_stricmp( key, "addgravity" ) )
		{
			body.addGravity = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "maxAngularVelocity" ) )
		{
			body.maxAngularVelocity = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "massOverride" ) )
		{
			body.massOverride = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "tiltforce" ) )
		{
			body.tiltForce = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "tiltforceheight" ) )
		{
			body.tiltForceHeight = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "countertorquefactor" ) )
		{
			body.counterTorqueFactor = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "keepuprighttorque" ) )
		{
			body.keepUprightTorque = strtof( value, nullptr );
		}
		else
		{
			DevWarning( "vphysics - Unknown vehicle body parser configuration key '%s' (%s).\n", key, value );
		}
	}
}


void CVPhysicsParse::ParseVehicleEngineBoost( vehicle_engineparams_t &engine )
{
	DebugPrint();
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;

		// parse subchunks
		if ( !Q_stricmp( key, "force" ) )
		{
			engine.boostForce = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "duration" ) )
		{
			engine.boostDuration = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "delay" ) )
		{
			engine.boostDelay = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "maxspeed" ) )
		{
			engine.boostMaxSpeed = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "torqueboost" ) )
		{
			engine.torqueBoost = atoi( value ) != 0 ? true : false;
		}
		else
		{
			DevWarning( "vphysics - Unknown vehicle engine boost parser configuration key '%s' (%s).\n", key, value );
		}
	}
}

void CVPhysicsParse::ParseVehicleEngine( vehicle_engineparams_t &engine )
{
	DebugPrint();
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;

		// parse subchunks
		if ( value[0] == '{' )
		{
			if ( !Q_stricmp( key, "boost" ) )
			{
				ParseVehicleEngineBoost( engine );
			}
			else
			{
				SkipBlock();
			}
		}
		else if ( !Q_stricmp( key, "gear" ) )
		{
			// Protect against exploits/overruns
			if ( engine.gearCount < ssize(engine.gearRatio) )
			{
				engine.gearRatio[engine.gearCount] = strtof( value, nullptr );
				engine.gearCount++;
			}
			else
			{
				DevWarning( "vphysics - Out of range (%d > max %zd) gear in vehicle configuration parser '%s' (%s).\n",
					engine.gearCount + 1, ssize(engine.gearRatio), key, value );
				//AssertMsg( 0, "Out of range (%d > max %zd) gear in vehicle configuration parser '%s' (%s).\n",
				//	engine.gearCount + 1, ssize(engine.gearRatio), key, value );
			}
		}
		else if ( !Q_stricmp( key, "horsepower" ) )
		{
			engine.horsepower = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "maxSpeed" ) )
		{
			engine.maxSpeed = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "maxReverseSpeed" ) )
		{
			engine.maxRevSpeed = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "axleratio" ) )
		{
			engine.axleRatio = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "maxRPM" ) )
		{
			engine.maxRPM = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "throttleTime" ) )
		{
			engine.throttleTime = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "AutoTransmission" ) )
		{
			engine.isAutoTransmission = atoi( value ) != 0 ? true : false;
		}
		else if ( !Q_stricmp( key, "shiftUpRPM" ) )
		{
			engine.shiftUpRPM = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "shiftDownRPM" ) )
		{
			engine.shiftDownRPM = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "autobrakeSpeedGain" ) )
		{
			engine.autobrakeSpeedGain = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "autobrakeSpeedFactor" ) )
		{
			engine.autobrakeSpeedFactor = strtof( value, nullptr );
		}
		else
		{
			DevWarning( "vphysics - Unknown vehicle engine parser configuration key '%s' (%s). ", key, value );
		}
	}
}


void CVPhysicsParse::ParseVehicleSteering( vehicle_steeringparams_t &steering )
{
	DebugPrint();
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;

		// parse subchunks
		if ( !Q_stricmp( key, "degreesSlow" ) )
		{
			steering.degreesSlow = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "degreesFast" ) )
		{
			steering.degreesFast = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "degreesBoost" ) )
		{
			steering.degreesBoost = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "fastcarspeed" ) )
		{
			steering.speedFast = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "slowcarspeed" ) )
		{
			steering.speedSlow = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "slowsteeringrate" ) )
		{
			steering.steeringRateSlow = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "faststeeringrate" ) )
		{
			steering.steeringRateFast = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "steeringRestRateSlow" ) )
		{
			steering.steeringRestRateSlow = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "steeringRestRateFast" ) )
		{
			steering.steeringRestRateFast = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "throttleSteeringRestRateFactor" ) )
		{
			steering.throttleSteeringRestRateFactor = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "boostSteeringRestRateFactor" ) )
		{
			steering.boostSteeringRestRateFactor = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "boostSteeringRateFactor" ) )
		{
			steering.boostSteeringRateFactor = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "steeringExponent" ) )
		{
			steering.steeringExponent = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "turnThrottleReduceSlow" ) )
		{
			steering.turnThrottleReduceSlow = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "turnThrottleReduceFast" ) )
		{
			steering.turnThrottleReduceFast = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "brakeSteeringRateFactor" ) )
		{
			steering.brakeSteeringRateFactor = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "powerSlideAccel" ) )
		{
			steering.powerSlideAccel = strtof( value, nullptr );
		}
		else if ( !Q_stricmp( key, "skidallowed" ) )
		{
			steering.isSkidAllowed = atoi( value ) != 0 ? true : false;
		}
		else if ( !Q_stricmp( key, "dustcloud" ) )
		{
			steering.dustCloud = atoi( value ) != 0 ? true : false;
		}
		else
		{
			DevWarning( "vphysics - Unknown vehicle steering parser configuration key '%s' (%s). ", key, value );
		}
	}
}

void CVPhysicsParse::ParseVehicle( vehicleparams_t *pVehicle, IVPhysicsKeyHandler *unknownKeyHandler )
{
	DebugPrint();
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	if ( unknownKeyHandler )
	{
		unknownKeyHandler->SetDefaults( pVehicle );
	}
	else
	{
		memset( pVehicle, 0, sizeof(*pVehicle) );
	}

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			return;
		}

		// parse subchunks
		if ( value[0] == '{' )
		{
			if ( !Q_stricmp( key, "axle" ) )
			{
				// Protect against exploits/overruns
				if ( pVehicle->axleCount < ssize(pVehicle->axles) )
				{
					ParseVehicleAxle( pVehicle->axles[pVehicle->axleCount] );
					pVehicle->axleCount++;
				}
				else
				{
					Assert( 0 );
				}
			}
			else if ( !Q_stricmp( key, "body" ) )
			{
				ParseVehicleBody( pVehicle->body );
			}
			else if ( !Q_stricmp( key, "engine" ) )
			{
				ParseVehicleEngine( pVehicle->engine );
			}
			else if ( !Q_stricmp( key, "steering" ) )
			{
				ParseVehicleSteering( pVehicle->steering );
			}
			else
			{
				SkipBlock();
			}
		}
		else if ( !Q_stricmp( key, "wheelsperaxle" ) )
		{
			pVehicle->wheelsPerAxle = atoi( value );
		}
		else
		{
			DevWarning( "vphysics - Unknown vehicle parser configuration key '%s' (%s). ", key, value );
		}
	}
}

void CVPhysicsParse::ParseCustom( void *pCustom, IVPhysicsKeyHandler *unknownKeyHandler )
{
	DebugPrint();
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	int indent = 0;
	if ( unknownKeyHandler )
	{
		unknownKeyHandler->SetDefaults( pCustom );
	}

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );

		if ( m_pText )
		{
			if ( key[0] == '{' )
			{
				indent++;
			}
			else if ( value[0] == '{' )
			{
				// They've got a named block here
				// Increase our indent, and let them parse the key
				indent++;
				if ( unknownKeyHandler )
				{
					unknownKeyHandler->ParseKeyValue( pCustom, key, value );
				}
			}
			else if ( key[0] == '}' )
			{
				indent--;
				if ( indent < 0 )
				{
					NextBlock();
					return;
				}
			}
			else
			{
				if ( unknownKeyHandler )
				{
					unknownKeyHandler->ParseKeyValue( pCustom, key, value );
				}
				// dimhotepus: Do not log missed things here as it is expected.
			}
		}
	}
}

#if PLATFORM_64BITS
void CVPhysicsParse::ParseCollisionRules(ragdollcollisionrules_t* pRules, IVPhysicsKeyHandler* unknownKeyHandler)
{
	DebugPrint();

	if ( unknownKeyHandler )
	{
		unknownKeyHandler->SetDefaults( pRules );
	}

	Warning(PROJECT_NAME " - vphysics: called a function that isn't implemented!\n");

	// ToDo: Verify that these actually exist in Gmod 64x and that they are used.

	NextBlock();
}

void CVPhysicsParse::ParseRagdollAnimatedFriction(ragdollanimatedfriction_t* pFriction, IVPhysicsKeyHandler* unknownKeyHandler)
{
	DebugPrint();

	if ( unknownKeyHandler )
	{
		unknownKeyHandler->SetDefaults( pFriction );
	} else {
		V_memset( pFriction, 0, sizeof(*pFriction) );
	}

	Warning(PROJECT_NAME " - vphysics: called a function that isn't implemented!\n");

	NextBlock();
}
#endif

IVPhysicsKeyParser *CreateVPhysicsKeyParser( const char *pKeyData, bool bIsPacked )
{
	DebugPrint();
	if (bIsPacked)
	{
		Warning("vphysics - CreateVPhysicsKeyParser: Packed vcollide's are unsupported!\n");
		return nullptr;
	}

	return new CVPhysicsParse( pKeyData );
}

void DestroyVPhysicsKeyParser( IVPhysicsKeyParser *pParser )
{
	DebugPrint();
	delete pParser;
}

//-----------------------------------------------------------------------------
// Helper functions for parsing script file
//-----------------------------------------------------------------------------
static characterset_t	g_BreakSet, g_BreakSetIncludingColons;
static void InitializeCharacterSets()
{
	static bool s_CharacterSetInitialized = false;
	if (!s_CharacterSetInitialized)
	{
		CharacterSetBuild( &g_BreakSet, "{}()'" );
		CharacterSetBuild( &g_BreakSetIncludingColons, "{}()':" );
		s_CharacterSetInitialized = true;
	}
}

const char* ParseFileInternal_SDK( const char* pFileBytes, char* pTokenOut, bool* pWasQuoted, characterset_t *pCharSet, size_t nMaxTokenLen )
{
	pTokenOut[0] = 0;

	if (pWasQuoted)
		*pWasQuoted = false;

	if (!pFileBytes)
		return 0;
	
	if ( nMaxTokenLen <= 1 )
		return 0;

	InitializeCharacterSets();

	// YWB:  Ignore colons as token separators in COM_Parse
	static bool com_ignorecolons = false;  
	characterset_t& breaks = pCharSet ? *pCharSet : (com_ignorecolons ? g_BreakSet : g_BreakSetIncludingColons);
	
	int c;
	unsigned int len = 0;
	
// skip whitespace
skipwhite:

	while ( (c = *pFileBytes) <= ' ')
	{
		if (c == 0)
			return 0;                    // end of file;
		pFileBytes++;
	}
	
// skip // comments
	if (c=='/' && pFileBytes[1] == '/')
	{
		while (*pFileBytes && *pFileBytes != '\n')
			pFileBytes++;
		goto skipwhite;
	}
	
// skip c-style comments
	if (c=='/' && pFileBytes[1] == '*' )
	{
		// Skip "/*"
		pFileBytes += 2;

		while ( *pFileBytes  )
		{
			if ( *pFileBytes == '*' &&
				 pFileBytes[1] == '/' )
			{
				pFileBytes += 2;
				break;
			}

			pFileBytes++;
		}

		goto skipwhite;
	}

// handle quoted strings specially
	if (c == '\"')
	{
		if (pWasQuoted)
			*pWasQuoted = true;

		pFileBytes++;
		while (1)
		{
			c = *pFileBytes++;
			if (c=='\"' || !c)
			{
				pTokenOut[len] = 0;
				return pFileBytes;
			}
			pTokenOut[len] = c;
			len++;
			
			// Ensure buffer length is not overrunning!
			if ( len == nMaxTokenLen - 1 )
			{
				pTokenOut[len] = 0;
				Assert( 0 );
				return pFileBytes;
			}
		}
	}

// parse single characters
	if ( IN_CHARACTERSET( breaks, c ) )
	{
		pTokenOut[len] = c;
		len += ( len < nMaxTokenLen-1 ) ? 1 : 0;
		pTokenOut[len] = 0;
		return pFileBytes+1;
	}

// parse a regular word
	do
	{
		pTokenOut[len] = c;
		pFileBytes++;
		len++;
		
		// Ensure buffer length is not overrunning!
		if ( len == nMaxTokenLen - 1 )
		{
			pTokenOut[ len ] = 0;
			Assert( 0 );
			return pFileBytes;
		}

		c = *pFileBytes;
		if ( IN_CHARACTERSET( breaks, c ) )
			break;
	} while (c>32);
	
	pTokenOut[len] = 0;
	return pFileBytes;
}

template <size_t count>
const char* ParseFile_SDK( const char* pFileBytes, OUT_Z_ARRAY char (&pTokenOut)[count], bool* pWasQuoted, characterset_t *pCharSet = NULL, unsigned int nMaxTokenLen = (unsigned int)-1 )
{
	(void)nMaxTokenLen; // Avoid unreferenced variable warnings.
	return ParseFileInternal_SDK( pFileBytes, pTokenOut, pWasQuoted, pCharSet, count );
}

template <size_t count>
char* ParseFile_SDK( char* pFileBytes, OUT_Z_ARRAY char (&pTokenOut)[count], bool* pWasQuoted )	// (same exact thing as the const version)
{
	return const_cast<char*>( ParseFileInternal_SDK( pFileBytes, pTokenOut, pWasQuoted, NULL, count ) );
}

const char *ParseKeyvalue( const char *pBuffer, OUT_Z_ARRAY char (&key)[MAX_KEYVALUE], OUT_Z_ARRAY char (&value)[MAX_KEYVALUE] )
{
	DebugPrint();
	// Make sure value is always null-terminated.
	value[0] = 0;

	pBuffer = ParseFile_SDK( pBuffer, key, NULL );

	// no value on a close brace
	if ( key[0] == '}' && key[1] == 0 )
	{
		value[0] = 0;
		return pBuffer;
	}

	Q_strlower( key );
	
	pBuffer = ParseFile_SDK( pBuffer, value, NULL );
	Q_strlower( value );

	return pBuffer;
}
