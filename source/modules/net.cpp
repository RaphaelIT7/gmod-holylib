#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include <netmessages.h>

class CNetModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "net"; };
	virtual int Compatibility() { return LINUX32; };
	virtual bool IsEnabledByDefault() OVERRIDE { return false; }; // Still Experimental
};

static CNetModule g_pNetModule;
IModule* pNetModule = &g_pNetModule;

bf_read** pReadBF = NULL;
bf_write** pWriteBF = NULL;
bool* bStarted = NULL;
LUA_FUNCTION_STATIC(net_WriteSeek)
{
	int iPos = (int)LUA->CheckNumber(1);
	if (iPos < 0)
		LUA->ArgError(1, "Number is not allowed to be below 0!");

	bf_write* pBF = *pWriteBF;
	if (!pBF || !*bStarted)
		LUA->ThrowError("Tried to use net.WriteSeek with no active net message!");

	pBF->SeekToBit(24 + iPos); // Why not working :<
	return 0;
}

LUA_FUNCTION_STATIC(net_ReadSeek)
{
	int iPos = (int)LUA->CheckNumber(1);
	if (iPos < 0)
		LUA->ArgError(1, "Number is not allowed to be below 0!");

	bf_read* pBF = *pReadBF;
	if (!pBF)
		LUA->ThrowError("Tried to use net.ReadSeek with no active net message!");

	pBF->Seek(46 + iPos);
	return 0;
}

void CNetModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	if (Util::PushTable(pLua, "net"))
	{
		Util::AddFunc(pLua, net_WriteSeek, "WriteSeek");
		Util::AddFunc(pLua, net_ReadSeek, "ReadSeek");
		Util::PopTable(pLua);
	}
}

void CNetModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	if (Util::PushTable(pLua, "net"))
	{
		Util::RemoveField(pLua, "WriteSeek");
		Util::RemoveField(pLua, "ReadSeek");
		Util::PopTable(pLua);
	}
}

void CNetModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::FactoryLoader server_loader("server");
	pReadBF = Detour::ResolveSymbol<bf_read*>(server_loader, Symbols::g_NetIncomingSym);
	Detour::CheckValue("get pointer", "g_NetIncoming", pReadBF != NULL);

	pWriteBF = Detour::ResolveSymbol<bf_write*>(server_loader, Symbols::g_WriteSym);
	Detour::CheckValue("get pointer", "g_Write", pWriteBF != NULL);

	bStarted = Detour::ResolveSymbol<bool>(server_loader, Symbols::g_StartedSym);
	Detour::CheckValue("get pointer", "g_Started", bStarted != NULL);
}