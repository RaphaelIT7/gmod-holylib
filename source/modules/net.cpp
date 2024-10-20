#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"
#include <netmessages.h>

class CNetModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "net"; };
	virtual int Compatibility() { return LINUX32; };
};

static CNetModule g_pNetModule;
IModule* pNetModule = &g_pNetModule;

bf_read** pReadBF = NULL;
bf_write** pWriteBF = NULL;
bool* bStarted = NULL;
LUA_FUNCTION_STATIC(net_WriteSeek)
{
	int iPos = LUA->CheckNumber(1);
	if (iPos < 0)
		LUA->ArgError(1, "Number is not allowed to be below 0!");

	bf_write* pBF = *pWriteBF;
	if (!pBF)
		LUA->ThrowError("Tried to use net.WriteSeek with no active net message!");

	pBF->SeekToBit(24 + iPos);
	return 0;
}

LUA_FUNCTION_STATIC(net_ReadSeek)
{
	int iPos = LUA->CheckNumber(1);
	if (iPos < 0)
		LUA->ArgError(1, "Number is not allowed to be below 0!");

	bf_read* pBF = *pReadBF;
	if (!pBF)
		LUA->ThrowError("Tried to use net.ReadSeek with no active net message!");

	pBF->Seek(46 + iPos);
	return 0;
}

void CNetModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	if (Util::PushTable("net"))
	{
		Util::AddFunc(net_WriteSeek, "WriteSeek");
		Util::AddFunc(net_ReadSeek, "ReadSeek");
		Util::PopTable();
	}
}

void CNetModule::LuaShutdown()
{
	if (Util::PushTable("net"))
	{
		Util::RemoveFunc("WriteSeek");
		Util::RemoveFunc("ReadSeek");
		Util::PopTable();
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