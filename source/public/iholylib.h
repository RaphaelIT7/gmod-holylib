#pragma once

#include "interface.h"

class IServerPluginCallbacks;
class IModuleManager;

namespace GarrysMod::Lua
{
	class ILuaInterface;
}

class IHolyLib
{
public:
	virtual IServerPluginCallbacks* GetPlugin() = 0;
	virtual CreateInterfaceFn GetGameFactory() = 0;
	virtual CreateInterfaceFn GetAppFactory() = 0;
	virtual IModuleManager* GetModuleManager() = 0;
	virtual GarrysMod::Lua::ILuaInterface* GetLuaInterface() = 0;
};

#define INTERFACEVERSION_HOLYLIB "IHOLYLIB001"