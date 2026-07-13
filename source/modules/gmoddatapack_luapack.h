#pragma once

#include "interface.h"
#include "public/imodule.h"

#include <string>

class CBaseClient;
class CCommand;
struct edict_t;

namespace GarrysMod::Lua
{
	class ILuaInterface;
	struct LuaFile;
}

namespace HolyLib::LuaPack
{
	struct Config
	{
		bool enabled = false;
		std::string packDirectory;
		std::string downloadUrlPolicy;
		std::string ingestUrl;
		std::string ingestMethod;
		double generationRetentionSeconds = 300.0;
		double readyDeadlineSeconds = 30.0;
	};

	const Config& GetConfig();
	bool IsEnabled();

	void Init(CreateInterfaceFn* appfn);
	void Shutdown();
	void LevelShutdown();
	void Think();
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit);

	void CaptureFile(const GarrysMod::Lua::LuaFile* file);
	void ClientConnect(int slot);
	void ClientActive(int slot);
	void ClientDisconnect(int slot);
	MODULE_RESULT ClientCommand(int slot, const CCommand* args);
}
