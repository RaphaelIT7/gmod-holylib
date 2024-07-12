#include "interface.h"
#include <scanning/symbolfinder.hpp>
#include <vector>
#include <string>
#include "convar.h"

class KeyValues;
class IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) = 0;
	virtual void LuaInit(bool bServerInit) = 0;
	virtual void LuaShutdown() = 0;
	virtual void InitDetour( bool bPreServer) = 0; // bPreServer = Called before the Dedicated Server was started
	virtual void Think(bool bSimulating) = 0;
	virtual void Shutdown() = 0;
	virtual const char* Name() = 0;

public:
	unsigned int m_pID = 0; // Set by the CModuleManager!
};

class CModule
{
public:
	~CModule() {
		if ( m_pCVar )
			delete m_pCVar; // Could this cause a crash? idk.
	}
	void SetModule(IModule* module)
	{
		m_pModule = module;
		m_strName = "holylib_enable_";
		m_strName = m_strName + module->Name();
		m_pCVar = new ConVar(m_strName.c_str(), "1", 0);
	};
	inline IModule* GetModule() { return m_pModule; };
	inline bool IsEnabled() { return m_pCVar->GetBool(); };

protected:
	IModule* m_pModule;
	ConVar* m_pCVar;
	std::string m_strName;
};

class CModuleManager
{
public:
	CModuleManager();
	void RegisterModule(IModule* mdl);

	void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn);
	void LuaInit(bool bServerInit);
	void LuaShutdown();
	void InitDetour(bool bPreServer);
	void Think(bool bSimulating);
	void Shutdown();

private:
	std::vector<CModule*> m_pModules;
};
extern CModuleManager g_pModuleManager;