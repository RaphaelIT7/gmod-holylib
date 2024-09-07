#include "public/iholylib.h"
#include "module.h"
#include "util.h"

class CHolyLib : public IHolyLib
{
public:
	virtual IServerPluginCallbacks* GetPlugin();
	virtual CreateInterfaceFn GetGameFactory();
	virtual CreateInterfaceFn GetAppFactory();
	virtual IModuleManager* GetModuleManager();
	virtual GarrysMod::Lua::ILuaInterface* GetLuaInterface();
};

CHolyLib s_HolyLib;
IHolyLib *g_pHolyLib = &s_HolyLib;

#ifdef LIB_HOLYLIB
IHolyLib* GetHolyLib()
{
	return g_pHolyLib;
}
#else
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CHolyLib, IHolyLib, INTERFACEVERSION_HOLYLIB, s_HolyLib);
#endif

extern IServerPluginCallbacks* g_pHolyLibServerPlugin;
IServerPluginCallbacks* CHolyLib::GetPlugin()
{
	return g_pHolyLibServerPlugin;
}

CreateInterfaceFn CHolyLib::GetGameFactory()
{
	return g_pModuleManager.GetGameFactory();
}

CreateInterfaceFn CHolyLib::GetAppFactory()
{
	return g_pModuleManager.GetAppFactory();
}

IModuleManager* CHolyLib::GetModuleManager()
{
	return &g_pModuleManager;
}

GarrysMod::Lua::ILuaInterface* CHolyLib::GetLuaInterface()
{
	return (GarrysMod::Lua::ILuaInterface*)g_Lua;
}