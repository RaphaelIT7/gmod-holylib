#include "module.h"
#include "LuaInterface.h"
#include "lua.h"
#include "lua.hpp"

class CLuaJITModule : public IModule
{
public:
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "luajit"; };
	virtual int Compatibility() { return LINUX32; };
};

CLuaJITModule g_pTemplateModule;
IModule* pTemplateModule = &g_pTemplateModule;

void CLuaJITModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

}