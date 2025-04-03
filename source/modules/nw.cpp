#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "recipientfilter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CNWModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "nw"; };
	virtual int Compatibility() { return LINUX32; };
};

static CNWModule g_pNWModule;
IModule* pNWModule = &g_pNWModule;

struct LuaNetworkedVar_t // This is Gmod's current version
{
	GarrysMod::Lua::ILuaObject& m_pLuaValue; // Holds the last set Value
	GarrysMod::Lua::ILuaObject& m_pLuaProxy; // Holds the set NWProxy
	float m_flLastUpdate = -1.0f;
	int m_iNetworkStringID = -1;
};

struct LuaNetworkedEntity_t // ToDo: Get Gmod's current version
{
	EHANDLE m_pHandle; // This seems to be wrong. But why :/
	CUtlRBTree<CUtlMap<char const*, LuaNetworkedVar_t, unsigned short>::Node_t, unsigned short, CUtlMap<char const*, LuaNetworkedVar_t, unsigned short>::CKeyLess, CUtlMemory<UtlRBTreeNode_t<CUtlMap<char const*, LuaNetworkedVar_t, unsigned short>::Node_t, unsigned short>, unsigned short>> m_pVars;
};

Detouring::Hook detour_CLuaNetworkedVars_UpdateEntityVar;
void hook_CLuaNetworkedVars_UpdateEntityVar(void* unimportantclass, LuaNetworkedEntity_t& unknown1, LuaNetworkedVar_t& unknown2, float unknown3, CRecipientFilter&, bool unknown4)
{
	/*
	 * We can now send our own usermessage.
	 * This will allow us to easily fix a few issues and to improve networking
	 * Planned things:
	 * - Network Var Name Index instead of String (Will reduce networking)
	 * - Allow one to network nil
	 * - If the number has no decimals, network it as an Int.
	 */
}

void CNWModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{

}

void CNWModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	// Detour 
}