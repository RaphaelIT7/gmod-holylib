#include "interface.h"
#include <scanning/symbolfinder.hpp>
#include <vector>

class KeyValues;
class IModule
{
public:
	virtual void Init(CreateInterfaceFn* fn) = 0;
	virtual void LuaInit(bool bServerInit) = 0;
	virtual void LuaShutdown() = 0;
	virtual void InitDetour( bool bPreServer ) = 0; // bPreServer = Called before the Dedicated Server was started
	virtual void Think(bool bSimulating) = 0;
	virtual void Shutdown() = 0;
	virtual const char* Name() = 0;
	virtual void LoadConfig(KeyValues* config) = 0;

public:
	unsigned int m_pID = 0; // Set by the CModuleManager!
};

class CModuleManager
{
public:
	CModuleManager();
	void RegisterModule(IModule* mdl);

	void LoadConfig();
	void Init(CreateInterfaceFn* fn);
	void LuaInit(bool bServerInit);
	void LuaShutdown();
	void InitDetour(bool bPreServer);
	void Think(bool bSimulating);
	void Shutdown();

private:
	std::vector<IModule*> m_pModules;
	KeyValues* m_pConfig = NULL;
};
extern CModuleManager g_pModuleManager;