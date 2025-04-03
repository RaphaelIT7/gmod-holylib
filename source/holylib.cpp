#include "public/iholylib.h"
#include "module.h"
#include "util.h"
#include "plugin.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IHolyUtil* g_pHolyUtil;
extern CServerPlugin* g_pHolyLibServerPlugin;
class CHolyLib : public IHolyLib
{
public:
	virtual IServerPluginCallbacks* GetPlugin()
	{
		return g_pHolyLibServerPlugin;
	}

	virtual CreateInterfaceFn GetGameFactory()
	{
		return g_pModuleManager.GetGameFactory();
	}

	virtual CreateInterfaceFn GetAppFactory()
	{
		return g_pModuleManager.GetAppFactory();
	}

	virtual IModuleManager* GetModuleManager()
	{
		return &g_pModuleManager;
	}

	virtual GarrysMod::Lua::ILuaInterface* GetLuaInterface()
	{
		return g_Lua;
	}

	virtual void PreLoad()
	{
		g_pHolyLibServerPlugin->GhostInj();
	}

	virtual IHolyUtil* GetHolyUtil()
	{
		return g_pHolyUtil;
	}
};

static CHolyLib s_HolyLib;
IHolyLib *g_pHolyLib = &s_HolyLib;

#ifdef LIB_HOLYLIB
IHolyLib* GetHolyLib()
{
	return g_pHolyLib;
}
#else
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CHolyLib, IHolyLib, INTERFACEVERSION_HOLYLIB, s_HolyLib);
#endif