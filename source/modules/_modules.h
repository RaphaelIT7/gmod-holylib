// This file is generated! Do NOT touch this!
#ifndef _MODULELIST_H
#define _MODULELIST_H
#include "module.h"

extern IModule* pBassModule;
extern IModule* pBitBufModule;
extern IModule* pConCommandModule;
extern IModule* pCVarsModule;
extern IModule* pEntListModule;
extern IModule* pFileSystemModule;
extern IModule* pGameeventLibModule;
extern IModule* pGameServerModule;
extern IModule* pHolyLibModule;
extern IModule* pHttpServerModule;
extern IModule* pLuaJITModule;
extern IModule* pLuaThreadsModule;
extern IModule* pNetModule;
extern IModule* pNetworkingModule;
extern IModule* pNWModule;
extern IModule* pNW2DebuggingModule;
extern IModule* pPASModule;
extern IModule* pPhysEnvModule;
extern IModule* pPrecacheFixModule;
extern IModule* pPVSModule;
extern IModule* pServerPluginLibModule;
extern IModule* pSoundscapeModule;
extern IModule* pSourceTVLibModule;
extern IModule* pSteamWorksModule;
extern IModule* pStringTableModule;
extern IModule* pSurfFixModule;
extern IModule* pSysTimerModule;
extern IModule* pThreadPoolFixModule;
extern IModule* pUtilModule;
extern IModule* pVoiceChatModule;
extern IModule* pVProfModule;

#define HOLYLIB_MODULE_COUNT 31

void CModuleManager::LoadModules()
{
	RegisterModule(pBassModule);
	RegisterModule(pBitBufModule);
	RegisterModule(pConCommandModule);
	RegisterModule(pCVarsModule);
	RegisterModule(pEntListModule);
	RegisterModule(pFileSystemModule);
	RegisterModule(pGameeventLibModule);
	RegisterModule(pGameServerModule);
	RegisterModule(pHolyLibModule);
	RegisterModule(pHttpServerModule);
	RegisterModule(pLuaJITModule);
	RegisterModule(pLuaThreadsModule);
	RegisterModule(pNetModule);
	RegisterModule(pNetworkingModule);
	RegisterModule(pNWModule);
	RegisterModule(pNW2DebuggingModule);
	RegisterModule(pPASModule);
	RegisterModule(pPhysEnvModule);
	RegisterModule(pPrecacheFixModule);
	RegisterModule(pPVSModule);
	RegisterModule(pServerPluginLibModule);
	RegisterModule(pSoundscapeModule);
	RegisterModule(pSourceTVLibModule);
	RegisterModule(pSteamWorksModule);
	RegisterModule(pStringTableModule);
	RegisterModule(pSurfFixModule);
	RegisterModule(pSysTimerModule);
	RegisterModule(pThreadPoolFixModule);
	RegisterModule(pUtilModule);
	RegisterModule(pVoiceChatModule);
	RegisterModule(pVProfModule);
}
#endif