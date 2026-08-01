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

namespace Bootil
{
	class AutoBuffer;
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
		unsigned int downloadableLimit = 2;
		double generationRetentionSeconds = 300.0;
		double objectRetentionSeconds = 604800.0;
		double readyDeadlineSeconds = 30.0;
		bool optimisticStubbing = false;
		unsigned int optimisticPrefixFiles = 256;
		unsigned long long optimisticPrefixBytes = 262144;
		double unreadyTtlSeconds = 900.0;
	};

	const Config& GetConfig();
	bool IsEnabled();
	bool IsInitFile(const std::string& virtualPath);

	void Init(CreateInterfaceFn* appfn);
	void Shutdown();
	void LevelShutdown();
	void Think();
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit);

	void CaptureFile(const GarrysMod::Lua::LuaFile* file);
	std::string PrepareVanillaFile(const std::string& virtualPath, const std::string& contents);
	bool ConsumeBootstrapRefresh();
	const Bootil::AutoBuffer* StubForClient(int slot, const std::string& virtualPath, size_t nativeSourceBytes);
	void ClientConnect(int slot);
	void ClientActive(int slot);
	void ClientDisconnect(int slot);
	MODULE_RESULT ClientCommand(int slot, const CCommand* args);
}
