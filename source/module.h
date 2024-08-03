#include "interface.h"
#include "detours.h"
#include <scanning/symbolfinder.hpp>
#include <vector>
#include <string>

enum Module_Compatibility
{
	LINUX32 = 1,
	LINUX64,
	WINDOWS32,
	WINDOWS64,
};


class ConVar;
class KeyValues;
class IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) {};
	virtual void LuaInit(bool bServerInit) {};
	virtual void LuaShutdown() {};
	virtual void InitDetour(bool bPreServer) {}; // bPreServer = Called before the Dedicated Server was started
	virtual void Think(bool bSimulating) {};
	virtual void Shutdown() { Detour::Remove(m_pID); };
	virtual const char* Name() = 0;
	virtual int Compatibility() = 0; // Idk give it a better name later.

public:
	unsigned int m_pID = 0; // Set by the CModuleManager!
};

class CModule
{
public:
	~CModule();
	void SetModule(IModule* module);
	void SetEnabled(bool bEnabled, bool bForced = false);
	inline IModule* GetModule() { return m_pModule; };
	inline bool IsEnabled() { return m_bEnabled; };
	inline ConVar* GetConVar() { return m_pCVar; };
	inline bool IsCompatible() { return m_bCompatible; };

protected:
	IModule* m_pModule;
	ConVar* m_pCVar;
	std::string m_strName;
	bool m_bEnabled = false;
	bool m_bCompatible = false;
};

#define LoadStatus_Init (1<<1)
#define LoadStatus_DetourInit (1<<2)
#define LoadStatus_LuaInit (1<<3)
#define LoadStatus_LuaServerInit (1<<4)

class CModuleManager
{
public:
	CModuleManager();
	void RegisterModule(IModule* mdl);
	CModule* FindModuleByConVar(ConVar* convar);
	CModule* FindModuleByName(const char* name);
	inline int GetStatus() { return m_pStatus; };
	inline CreateInterfaceFn& GetAppFactory() { return m_pAppFactory; };
	inline CreateInterfaceFn& GetGameFactory() { return m_pGameFactory; };

	void Init(CreateInterfaceFn appfn, CreateInterfaceFn gamefn);
	void LuaInit(bool bServerInit);
	void LuaShutdown();
	void InitDetour(bool bPreServer);
	void Think(bool bSimulating);
	void Shutdown();

private:
	std::vector<CModule*> m_pModules;
	int m_pStatus = 0;
	CreateInterfaceFn m_pAppFactory = NULL;
	CreateInterfaceFn m_pGameFactory = NULL;
};
extern CModuleManager g_pModuleManager;