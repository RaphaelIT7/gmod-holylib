#pragma once

#include "public/imodule.h"
#include <vector>
#include "unordered_set"
#include "public/iconfigsystem.h"

class CModuleManager;
class CModule : public IModuleWrapper
{
public:
	virtual ~CModule();
	virtual void SetModule(IModule* module);
	virtual IModule* GetModule() { return m_pModule; };
	virtual void SetEnabled(bool bEnabled, bool bForced = false);
	virtual void Shutdown();
	virtual bool IsEnabled();
	virtual bool IsCompatible() { return m_bCompatible; };
	virtual void SetupConfig();

public:
	inline IModule* FastGetModule() { return m_pModule; };
	inline bool FastIsEnabled() { return m_bEnabled; };
	inline ConVar* GetConVar() { return m_pCVar; };
	inline ConVar* GetDebugConVar() { return m_pDebugCVar; };
	inline bool FastIsCompatible() { return m_bCompatible; };

protected:
	friend class CModuleManager;
	inline void SetID(unsigned int pID) { m_pModule->m_pID = pID; };

protected:
	IModule* m_pModule = nullptr;
	ConVar* m_pCVar = nullptr;
	char* m_pCVarName = nullptr;
	ConVar* m_pDebugCVar = nullptr;
	char* m_pDebugCVarName = nullptr;
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

	// This one function is generated inside the _modules.h file.
	virtual void LoadModules();
	virtual IModuleWrapper* RegisterModule(IModule* mdl);
	virtual IModuleWrapper* FindModuleByConVar(ConVar* convar);
	virtual IModuleWrapper* FindModuleByName(const char* name);
	virtual IModuleWrapper* GetModuleByID(int nIndex);

	virtual void SetGhostInj() { m_bGhostInj = true; };
	virtual bool IsUsingGhostInj() { return m_bGhostInj; };

	virtual void SetModuleRealm(Module_Realm realm) { m_pRealm = realm; };
	virtual Module_Realm GetModuleRealm() { return m_pRealm; };

	virtual void MarkAsBinaryModule() { m_bMarkedAsBinaryModule = true;  };
	virtual bool IsMarkedAsBinaryModule() { return m_bMarkedAsBinaryModule; };

	virtual void EnableUnsafeCode() { m_bEnabledUnsafeCode = true;  };
	virtual bool IsUnsafeCodeEnabled() { return m_bEnabledUnsafeCode; };

	virtual void Setup(CreateInterfaceFn appfn, CreateInterfaceFn gamefn);
	virtual void Init();
	// bServerInit = true should never be called by a Interface itself, its called automatically
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
	virtual void OnClientConnect(CBaseClient* pClient);
	virtual void OnClientDisconnect(CBaseClient* pClient);
	virtual void LevelInit(const char *pMapName);
	virtual void LevelShutdown();
	virtual void PreLuaModuleLoaded(lua_State* L, const char* pFileName);
	virtual void PostLuaModuleLoaded(lua_State* L, const char* pFileName);
	virtual void ClientActive(edict_t* pClient);
	virtual void ClientDisconnect(edict_t* pClient);
	virtual void ClientPutInServer(edict_t* pClient, const char* pPlayerName);
	virtual MODULE_RESULT ClientConnect(bool* bAllowConnect, edict_t* pClient, const char* pszName, const char* pszAddress, char* reject, int maxrejectlen);
	virtual MODULE_RESULT ClientCommand(edict_t *pClient, const CCommand* args);
	virtual MODULE_RESULT NetworkIDValidated(const char *pszUserName, const char *pszNetworkID);

	inline int GetStatus() { return m_pStatus; };
	inline CreateInterfaceFn& GetAppFactory() { return m_pAppFactory; };
	inline CreateInterfaceFn& GetGameFactory() { return m_pGameFactory; };
	inline edict_t* GetEdictList() { return m_pEdictList; };
	inline int GetEdictCount() { return m_iEdictCount; };
	inline int GetClientMax() { return m_iClientMax; };
	inline std::vector<CModule*>& GetModules() { return m_pModules; };
	inline std::unordered_set<GarrysMod::Lua::ILuaInterface*>& GetLuaInterfaces() { return m_pLuaInterfaces; };
	inline IConfig* GetConfig() { return m_pConfig; };
	inline const char* GetMapName() { return m_strMapName.c_str(); };

private:
	std::vector<CModule*> m_pModules;
	int m_pStatus = 0;
	Module_Realm m_pRealm = Module_Realm::SERVER;
	CreateInterfaceFn m_pAppFactory = nullptr;
	CreateInterfaceFn m_pGameFactory = nullptr;
	bool m_bGhostInj = false;
	bool m_bMarkedAsBinaryModule = false;
#if SYSTEM_LINUX
	bool m_bEnabledUnsafeCode = true;
#else
	bool m_bEnabledUnsafeCode = false;
#endif
	IConfig* m_pConfig = nullptr; // Can be nullptr at runtime so check for it!

private: // ServerActivate stuff
	edict_t* m_pEdictList = nullptr;
	int m_iEdictCount = 0;
	int m_iClientMax = 0;
	std::string m_strMapName;

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