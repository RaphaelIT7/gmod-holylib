#ifndef GMOD_TYPES_H
#define GMOD_TYPES_H
namespace GarrysMod::Lua::Type
{
    enum
    {
#ifdef GMOD_ALLOW_DEPRECATED
        // Deprecated: Use `None` instead of `Invalid`
        Invalid = -1,
#endif

        // Default Lua Types
        None = -1,
        Nil,
        Bool,
        LightUserData,
        Number,
        String,
        Table,
        Function,
        UserData,
        Thread,

        // GMod Types
        Entity,
        Vector,
        Angle,
        PhysObj,
        Save,
        Restore,
        DamageInfo,
        EffectData,
        MoveData,
        RecipientFilter,
        UserCmd,
        ScriptedVehicle,
        Material,
        Panel,
        Particle,
        ParticleEmitter,
        Texture,
        UserMsg,
        ConVar,
        IMesh,
        Matrix,
        Sound,
        PixelVisHandle,
        DLight,
        Video,
        File,
        Locomotion,
        Path,
        NavArea,
        SoundHandle,
        NavLadder,
        ParticleSystem,
        ProjectedTexture,
        PhysCollide,
        SurfaceInfo,

        Type_Count,

#ifdef GMOD_ALLOW_OLD_TYPES
#ifdef GMOD_ALLOW_DEPRECATED
        // Deprecated: Use NONE instead of INVALID
        INVALID = Invalid,
#endif

        // Lua Types
        NONE = None,
        NIL = Nil,
        BOOL = Bool,
        LIGHTUSERDATA = LightUserData,
        NUMBER = Number,
        STRING = String,
        TABLE = Table,
        FUNCTION = Function,
        USERDATA = UserData,
        THREAD = Thread,

        // GMod Types
        ENTITY = Entity,
        VECTOR = Vector,
        ANGLE = Angle,
        PHYSOBJ = PhysObj,
        SAVE = Save,
        RESTORE = Restore,
        DAMAGEINFO = DamageInfo,
        EFFECTDATA = EffectData,
        MOVEDATA = MoveData,
        RECIPIENTFILTER = RecipientFilter,
        USERCMD = UserCmd,
        SCRIPTEDVEHICLE = ScriptedVehicle,
        MATERIAL = Material,
        PANEL = Panel,
        PARTICLE = Particle,
        PARTICLEEMITTER = ParticleEmitter,
        TEXTURE = Texture,
        USERMSG = UserMsg,
        CONVAR = ConVar,
        IMESH = IMesh,
        MATRIX = Matrix,
        SOUND = Sound,
        PIXELVISHANDLE = PixelVisHandle,
        DLIGHT = DLight,
        VIDEO = Video,
        FILE = File,
        LOCOMOTION = Locomotion,
        PATH = Path,
        NAVAREA = NavArea,
        SOUNDHANDLE = SoundHandle,
        NAVLADDER = NavLadder,
        PARTICLESYSTEM = ParticleSystem,
        PROJECTEDTEXTURE = ProjectedTexture,
        PHYSCOLLIDE = PhysCollide,

        COUNT = Type_Count
#endif
     };

//#if ( defined( GMOD ) || defined( GMOD_ALLOW_DEPRECATED ) )
    // You should use ILuaBase::GetTypeName instead of directly accessing this array
    static const char* Name[] =
    {
        "nil",
        "boolean",
        "lightuserdata",
        "number",
        "string",
        "table",
        "function",
        "userdata",
        "thread",
        "Entity",
        "Vector",
        "Angle",
        "PhysObj",
        "save",
        "restore",
        "CTakeDamageInfo",
        "CEffectData",
        "CMoveData",
        "CRecipientFilter",
        "CUserCmd",
        "Vehicle",
        "IMaterial",
        "Panel",
        "CLuaParticle",
        "CLuaEmmiter",
        "ITexture",
        "bf_read",
        "ConVar",
        "mesh",
        "matrix",
        "sound",
        "pixelvishandle",
        "dlight",
        "video",
        "File",
        "CLuaLocomotion",
        "PathFollower",
        "CNavArea",
        "SoundHandle",
        "CNavLadder",
        "ParticleSystem",
        "ProjectedTexture",
        "PhysCollide",
        "SurfaceInfo",
        nullptr
    };
//#endif
}
#endif