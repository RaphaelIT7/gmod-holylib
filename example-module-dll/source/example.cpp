#include <Platform.hpp>
#include "public/iholylib.h"
#include "public/imodule.h"
#define GMOD_USE_ILUAINTERFACE // HolyLib uses the ILuaInterface for everything, not the ILuaBase.
#include "GarrysMod/Lua/Interface.h"

// Small version of HolyLib's util namespace with just the Lua functions as a helper
namespace Util
{
	inline void StartTable(GarrysMod::Lua::ILuaInterface* LUA) {
		LUA->CreateTable();
	}

	inline void AddFunc(GarrysMod::Lua::ILuaInterface* LUA, GarrysMod::Lua::CFunc Func, const char* Name) {
		LUA->PushString(Name);
		LUA->PushCFunction(Func);
		LUA->RawSet(-3);
	}

	inline void AddValue(GarrysMod::Lua::ILuaInterface* LUA, double value, const char* Name) {
		LUA->PushString(Name);
		LUA->PushNumber(value);
		LUA->RawSet(-3);
	}

	inline void FinishTable(GarrysMod::Lua::ILuaInterface* LUA, const char* Name) {
		LUA->SetField(GarrysMod::Lua::INDEX_GLOBAL, Name);
	}

	inline void NukeTable(GarrysMod::Lua::ILuaInterface* LUA, const char* pName)
	{
		LUA->PushNil();
		LUA->SetField(GarrysMod::Lua::INDEX_GLOBAL, pName);
	}

	inline bool PushTable(GarrysMod::Lua::ILuaInterface* LUA, const char* pName)
	{
		LUA->GetField(GarrysMod::Lua::INDEX_GLOBAL, pName);
		if (LUA->IsType(-1, GarrysMod::Lua::Type::Table))
			return true;

		LUA->Pop(1);
		return false;
	}

	inline void PopTable(GarrysMod::Lua::ILuaInterface* LUA)
	{
		LUA->Pop(1);
	}

	inline void RemoveField(GarrysMod::Lua::ILuaInterface* LUA, const char* pName)
	{
		LUA->PushNil();
		LUA->SetField(-2, pName);
	}

	inline bool HasField(GarrysMod::Lua::ILuaInterface* LUA, const char* pName, int iType)
	{
		LUA->GetField(-1, pName);
		return LUA->IsType(-1, iType);
	}
}

class CExampleModule : public IModule
{
public:
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	const char* Name() override { return "example"; };
	int Compatibility() override { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
	bool SupportsMultipleLuaStates() override { return true; }; // This currently is unused
};

LUA_FUNCTION_STATIC(example_hello)
{
	LUA->PushString("Hello World");
	return 1;
}

void CExampleModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Util::StartTable(pLua);
		Util::AddFunc(pLua, example_hello, "hello");
	Util::FinishTable(pLua, "example");
}

void CExampleModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "example");
}

static CExampleModule g_pExampleModule;
extern "C" bool HolyLib_EntryPoint(IHolyLib* pHolyLib)
{
	
	pHolyLib->GetModuleManager()->RegisterModule(&g_pExampleModule);
	return true; // if we return false, HolyLib will throw a warning that we failed to load and will unload us again.
}