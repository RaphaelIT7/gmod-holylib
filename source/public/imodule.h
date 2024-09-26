#ifndef _IMODULE
#define _IMODULE

#include "interface.h"
#include <vector>

enum Module_Compatibility
{
	LINUX32 = 1,
	LINUX64 = 2,
	WINDOWS32 = 4,
	WINDOWS64 = 8,
};

class ConVar;
class KeyValues;
class IModuleWrapper;
class IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) {};
	virtual void LuaInit(bool bServerInit) {};
	virtual void LuaShutdown() {};
	virtual void InitDetour(bool bPreServer) {};
	virtual void Think(bool bSimulating) {};
	virtual void Shutdown() {};
	virtual const char* Name() = 0;
	virtual int Compatibility() = 0;
	virtual bool IsEnabledByDefault() { return true; };
	inline void SetDebug(int iDebug) { m_iIsDebug = iDebug; };
	inline int InDebug() { return m_iIsDebug; };
	inline void SetWrapper(IModuleWrapper* wrapper) { m_pWrapper = wrapper; }
public:
	unsigned int m_pID = 0; // Set by the CModuleManager when registering it! Don't touch it.
	int m_iIsDebug = false;
	IModuleWrapper* m_pWrapper;
};

class IModuleWrapper
{
public:
	virtual void SetModule(IModule* module) = 0;
	virtual IModule* GetModule() = 0;
	virtual void SetEnabled(bool bEnabled, bool bForced = false) = 0;
	virtual bool IsEnabled() = 0;
	virtual void Shutdown() = 0;
};

#define LoadStatus_PreDetourInit (1<<1)
#define LoadStatus_Init (1<<2)
#define LoadStatus_DetourInit (1<<3)
#define LoadStatus_LuaInit (1<<4)
#define LoadStatus_LuaServerInit (1<<5)

class IModuleManager
{
public:
	virtual void RegisterModule(IModule* mdl) = 0;
	virtual IModuleWrapper* FindModuleByConVar(ConVar* convar) = 0;
	virtual IModuleWrapper* FindModuleByName(const char* name) = 0;
	virtual IModuleWrapper* FindModuleByModule(IModule* name) = 0;

	virtual void SetGhostInj() = 0;
	virtual bool IsUsingGhostInj() = 0;

	virtual void Setup(CreateInterfaceFn appfn, CreateInterfaceFn gamefn) = 0;
	virtual void Init() = 0;
	virtual void LuaInit(bool bServerInit) = 0;
	virtual void LuaShutdown() = 0;
	virtual void InitDetour(bool bPreServer) = 0;
	virtual void Think(bool bSimulating) = 0;
	virtual void Shutdown() = 0;
};

#endif