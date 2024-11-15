#include "interface.h"

enum Module_Compatibility
{
	LINUX32 = 1,
	LINUX64 = 2,
	WINDOWS32 = 4,
	WINDOWS64 = 8,
};

class ConVar;
class KeyValues;
struct edict_t;
class IModule
{
public:
	// Called when the Plugin's Init function is called.
	// On Windows
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) { (void)appfn; (void)gamefn; };

	// Implement your Lua init logic here.
	// NOTE: This will be before any global functions were added.
	virtual void LuaInit(bool bServerInit) { (void)bServerInit; };

	// Implement your Lua shutdown logic here like removing your table.
	virtual void LuaShutdown() {};

	// Called when the module should add it's detours.
	// NOTE: This is called before Init if bPreServer is true.
	virtual void InitDetour(bool bPreServer) { (void)bPreServer; };

	// Called every frame.
	virtual void Think(bool bSimulating) { (void)bSimulating; };

	// Implement your additional shutdown logic here.
	virtual void Shutdown() {};

	// Returns the name of the module.
	virtual const char* Name() = 0;

	// Returns the compatibility flags.
	// Use the Module_Compatibility enums like this:
	// LINUX32 | LINUX64 and so on
	virtual int Compatibility() = 0;

	// Whether the module should be enabled by default.
	virtual bool IsEnabledByDefault() { return true; };

	// Called when the Server is ready to accept players.
	virtual void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax) { (void)pEdictList; (void)edictCount; (void)clientMax; };

	// Called when a edict is allocated.
	// NOTE: Only Entities that are networked use edicts.
	virtual void OnEdictAllocated(edict_t* pEdict) { (void)pEdict; };

	// Called when a edict is freed.
	// NOTE: Only Entities that are networked use edicts.
	virtual void OnEdictFreed(const edict_t* pEdict) { (void)pEdict; };

public: // I would like to remove these at some point but it's more efficient if the modules themself have them.
	unsigned int m_pID = 0; // Set by the CModuleManager when registering it! Don't touch it.
	int m_iIsDebug = false;

	inline void SetDebug(int iDebug) { m_iIsDebug = iDebug; };

	// returns the debug level.
	inline int InDebug() { return m_iIsDebug; };
};

class IModuleWrapper
{
public:
	// Binds the IModuleWrapper to the IModule and Initializes itself.
	virtual void SetModule(IModule* module) = 0;

	// Enables/Disables the module.
	// It will call all the Modules functions in proper order to enable / disable.
	// Order: InitDetour(true) -> Init() -> InitDetour(false) -> LuaInit(false) -> LuaInit(true) -> ServerActivate()
	// NOTE: The order may change depending on how far the IModuleManager is in the loading order.
	virtual void SetEnabled(bool bEnabled, bool bForced = false) = 0;

	// Calls the IModule::Shutdown function and removes all Detours after it.
	virtual void Shutdown() = 0;

	// Returns true if the module is enabled. We should add this to the IModule.
	virtual bool IsEnabled() = 0;
};

#define LoadStatus_PreDetourInit (1<<0)
#define LoadStatus_Init (1<<1)
#define LoadStatus_DetourInit (1<<2)
#define LoadStatus_LuaInit (1<<3)
#define LoadStatus_LuaServerInit (1<<4)
#define LoadStatus_ServerActivate (1<<5)

class IModuleManager
{
public:
	// Registers all HolyLib modules.
	virtual void LoadModules() = 0;

	// Registers the given module.
	virtual IModuleWrapper* RegisterModule(IModule* mdl) = 0;

	// Returns the IModuleWrapper from the given ConVar.
	virtual IModuleWrapper* FindModuleByConVar(ConVar* convar) = 0;

	// Returns the IModuleWrapper from the given name.
	virtual IModuleWrapper* FindModuleByName(const char* name) = 0;

	// Marks us to be loaded by a ghostinj.
	virtual void SetGhostInj() = 0;

	// true if we were loaded by a ghostinj.
	virtual bool IsUsingGhostInj() = 0;

	// This function is sets the internal variables.
	virtual void Setup(CreateInterfaceFn appfn, CreateInterfaceFn gamefn) = 0;
	virtual void Init() = 0;
	virtual void LuaInit(bool bServerInit) = 0;
	virtual void LuaShutdown() = 0;
	virtual void InitDetour(bool bPreServer) = 0;
	virtual void Think(bool bSimulating) = 0;
	virtual void Shutdown() = 0;
	virtual void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax) = 0;
	virtual void OnEdictAllocated(edict_t* pEdict) = 0;
	virtual void OnEdictFreed(const edict_t* pEdict) = 0;
};