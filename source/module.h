#include "interface.h"
#include <vector>
#include "detours.h"
#include "public/imodule.h"

class CModule : public IModuleWrapper
{
public:
	~CModule();
	virtual void SetModule(IModule* module);
	virtual void SetEnabled(bool bEnabled, bool bForced = false);
	virtual void Shutdown();
	inline IModule* GetModule() { return m_pModule; };
	inline bool IsEnabled() { return m_bEnabled; };
	inline ConVar* GetConVar() { return m_pCVar; };
	inline bool IsCompatible() { return m_bCompatible; };
	inline void SetID(unsigned int pID) { m_pModule->m_pID = pID; };

protected:
	IModule* m_pModule = NULL;
	ConVar* m_pCVar = NULL;
	char* m_pCVarName = NULL;
	bool m_bEnabled = false;
	bool m_bCompatible = false;
	bool m_bStartup = false;
};

class CModuleManager : public IModuleManager
{
public:
	CModuleManager();
	virtual void RegisterModule(IModule* mdl);
	virtual IModuleWrapper* FindModuleByConVar(ConVar* convar);
	virtual IModuleWrapper* FindModuleByName(const char* name);

	virtual void SetGhostInj() { m_bGhostInj = true; };
	virtual bool IsUsingGhostInj() { return m_bGhostInj; };

	virtual void Setup(CreateInterfaceFn appfn, CreateInterfaceFn gamefn);
	virtual void Init();
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual void InitDetour(bool bPreServer);
	virtual void Think(bool bSimulating);
	virtual void Shutdown();

	inline int GetStatus() { return m_pStatus; };
	inline CreateInterfaceFn& GetAppFactory() { return m_pAppFactory; };
	inline CreateInterfaceFn& GetGameFactory() { return m_pGameFactory; };
private:
	std::vector<CModule*> m_pModules;
	int m_pStatus = 0;
	CreateInterfaceFn m_pAppFactory = NULL;
	CreateInterfaceFn m_pGameFactory = NULL;
	bool m_bGhostInj = false;
};
extern CModuleManager g_pModuleManager;