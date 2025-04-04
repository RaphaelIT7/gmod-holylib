#include "public/imodule.h"
#include <vector>
#include "unordered_set"

class CModuleManager;
class CModule : public IModuleWrapper
{
public:
	virtual ~CModule();
	virtual void SetModule(IModule* module);
	virtual void SetEnabled(bool bEnabled, bool bForced = false);
	virtual void Shutdown();
	virtual bool IsEnabled();

public:
	inline IModule* GetModule() { return m_pModule; };
	inline bool FastIsEnabled() { return m_bEnabled; };
	inline ConVar* GetConVar() { return m_pCVar; };
	inline ConVar* GetDebugConVar() { return m_pDebugCVar; };
	inline bool IsCompatible() { return m_bCompatible; };

protected:
	friend class CModuleManager;
	inline void SetID(unsigned int pID) { m_pModule->m_pID = pID; };

protected:
	IModule* m_pModule = NULL;
	ConVar* m_pCVar = NULL;
	char* m_pCVarName = NULL;
	ConVar* m_pDebugCVar = NULL;
	char* m_pDebugCVarName = NULL;
	bool m_bEnabled = false;
	bool m_bCompatible = false;
	bool m_bStartup = false;
	char* m_strDebugValue; // Workaround for a crash.
};

class CModuleManager : public IModuleManager
{
public:
	CModuleManager();
	~CModuleManager();

	virtual void LoadModules();
	virtual IModuleWrapper* RegisterModule(IModule* mdl);
	virtual IModuleWrapper* FindModuleByConVar(ConVar* convar);
	virtual IModuleWrapper* FindModuleByName(const char* name);

	virtual void SetGhostInj() { m_bGhostInj = true; };
	virtual bool IsUsingGhostInj() { return m_bGhostInj; };

	virtual void SetModuleRealm(Module_Realm realm) { m_pRealm = realm; };
	virtual Module_Realm GetModuleRealm() { return m_pRealm; };

	virtual void MarkAsBinaryModule() { m_bMarkedAsBinaryModule = true;  };
	virtual bool IsMarkedAsBinaryModule() { return m_bMarkedAsBinaryModule; };

	virtual void Setup(CreateInterfaceFn appfn, CreateInterfaceFn gamefn);
	virtual void Init();
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit);
	virtual void LuaThink(GarrysMod::Lua::ILuaInterface* pLua);
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua);
	virtual void InitDetour(bool bPreServer);
	virtual void Think(bool bSimulating);
	virtual void Shutdown();
	virtual void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax);
	virtual void OnEdictAllocated(edict_t* pEdict);
	virtual void OnEdictFreed(const edict_t* pEdict);
	virtual void OnEntityCreated(CBaseEntity* pEntity);
	virtual void OnEntitySpawned(CBaseEntity* pEntity);
	virtual void OnEntityDeleted(CBaseEntity* pEntity);
	virtual void LevelShutdown();

	inline int GetStatus() { return m_pStatus; };
	inline CreateInterfaceFn& GetAppFactory() { return m_pAppFactory; };
	inline CreateInterfaceFn& GetGameFactory() { return m_pGameFactory; };
	inline edict_t* GetEdictList() { return m_pEdictList; };
	inline int GetEdictCount() { return m_iEdictCount; };
	inline int GetClientMax() { return m_iClientMax; };
	inline std::vector<CModule*>& GetModules() { return m_pModules; };
	inline std::unordered_set<GarrysMod::Lua::ILuaInterface*>& GetLuaInterfaces() { return m_pLuaInterfaces; };

private:
	std::vector<CModule*> m_pModules;
	int m_pStatus = 0;
	Module_Realm m_pRealm = Module_Realm::SERVER;
	CreateInterfaceFn m_pAppFactory = NULL;
	CreateInterfaceFn m_pGameFactory = NULL;
	bool m_bGhostInj = false;
	bool m_bMarkedAsBinaryModule = false;

private: // ServerActivate stuff
	edict_t* m_pEdictList = NULL;
	int m_iEdictCount = 0;
	int m_iClientMax = 0;

private:
	// All Lua interfaces that were loaded.
	std::unordered_set<GarrysMod::Lua::ILuaInterface*> m_pLuaInterfaces;
	
	inline void AddLuaInterface(GarrysMod::Lua::ILuaInterface* pLua)
	{
		auto it = m_pLuaInterfaces.find(pLua);
		if (it != m_pLuaInterfaces.end())
			return;

		m_pLuaInterfaces.insert(pLua);
	}

	inline void RemoveLuaInterface(GarrysMod::Lua::ILuaInterface* pLua)
	{
		auto it = m_pLuaInterfaces.find(pLua);
		if (it == m_pLuaInterfaces.end())
			return;

		m_pLuaInterfaces.erase(it);
	}
};
extern CModuleManager g_pModuleManager;