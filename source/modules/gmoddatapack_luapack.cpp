#include "httplib.h"
#include "modules/gmoddatapack_luapack.h"

#include "LuaInterface.h"
#include "detours.h"
#include "filesystem.h"
#include "lua.h"
#include "module.h"
#include "sourcesdk/iluashared.h"

#undef isalnum
#undef isalpha
#undef isspace
#undef isdigit
#undef min
#undef max

#include "util.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <vector>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace HolyLib::LuaPack
{
	struct FileRecord
	{
		std::string virtualPath;
		std::string sourcePath;
		std::string contents;
		unsigned long long revision = 0;
	};

	struct State
	{
		std::mutex registryMutex;
		std::unordered_map<std::string, FileRecord> files;
		unsigned long long revision = 0;
		bool buildRequested = false;
	};

	static State state;

	static std::string NormalizePath(std::string path)
	{
		std::replace(path.begin(), path.end(), '\\', '/');
		while (!path.empty() && (path[0] == '@' || path[0] == '/'))
			path.erase(path.begin());

		return path;
	}

	static ConVar luapack_enable(
		"holylib_gmoddatapack_luapack_enable", "0", FCVAR_ARCHIVE,
		"Bundle clientside Lua for FastDL delivery. Disabled by default; vanilla Lua delivery remains the fallback");
	static ConVar luapack_packdir(
		"holylib_gmoddatapack_luapack_packdir", "holylib/luapack", FCVAR_ARCHIVE,
		"Directory below garrysmod/data used for immutable Lua pack objects");
	static ConVar luapack_downloadurl_policy(
		"holylib_gmoddatapack_luapack_downloadurl_policy", "respect", FCVAR_ARCHIVE,
		"sv_downloadurl policy: respect, require, or lock");
	static ConVar luapack_ingest_url(
		"holylib_gmoddatapack_luapack_ingest_url", "", FCVAR_ARCHIVE,
		"Optional operator-controlled HTTP ingest URL called after publishing a pack");
	static ConVar luapack_ingest_method(
		"holylib_gmoddatapack_luapack_ingest_method", "PUT", FCVAR_ARCHIVE,
		"HTTP method for the optional pack ingest hook");
	static ConVar luapack_retention_ttl(
		"holylib_gmoddatapack_luapack_retention_ttl", "300", FCVAR_ARCHIVE,
		"Minimum seconds to retain an immutable generation after its last pinned client leaves",
		true, 30.0f, true, 86400.0f);
	static ConVar luapack_ready_deadline(
		"holylib_gmoddatapack_luapack_ready_deadline", "30", FCVAR_ARCHIVE,
		"Seconds a connecting client has to acknowledge its pinned generation before using vanilla delivery",
		true, 1.0f, true, 300.0f);

	static Config config;

	static void RefreshConfig()
	{
		config.enabled = luapack_enable.GetBool();
		config.packDirectory = luapack_packdir.GetString();
		config.downloadUrlPolicy = luapack_downloadurl_policy.GetString();
		config.ingestUrl = luapack_ingest_url.GetString();
		config.ingestMethod = luapack_ingest_method.GetString();
		config.generationRetentionSeconds = luapack_retention_ttl.GetFloat();
		config.readyDeadlineSeconds = luapack_ready_deadline.GetFloat();
	}

	const Config& GetConfig()
	{
		RefreshConfig();
		return config;
	}

	bool IsEnabled()
	{
		return luapack_enable.GetBool();
	}

	void Init(CreateInterfaceFn* appfn)
	{
		(void)appfn;
		RefreshConfig();
	}

	void Shutdown()
	{
		std::lock_guard<std::mutex> lock(state.registryMutex);
		state.files.clear();
		state.buildRequested = false;
	}

	void LevelShutdown()
	{
		std::lock_guard<std::mutex> lock(state.registryMutex);
		state.files.clear();
		state.buildRequested = false;
	}
	void Think() {}
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
	{
		(void)pLua;
		(void)bServerInit;
	}

	void CaptureFile(const GarrysMod::Lua::LuaFile* file)
	{
		if (!file)
			return;

		const std::string virtualPath = NormalizePath(file->name);
		if (virtualPath.empty())
			return;

		std::lock_guard<std::mutex> lock(state.registryMutex);
		FileRecord& record = state.files[virtualPath];
		const std::string sourcePath = NormalizePath(file->source.empty() ? file->name : file->source);
		if (record.contents == file->contents && record.sourcePath == sourcePath)
			return;

		record.virtualPath = virtualPath;
		record.sourcePath = sourcePath;
		record.contents = file->contents;
		record.revision = ++state.revision;
		state.buildRequested = true;
	}

	void ClientConnect(int slot)
	{
		(void)slot;
	}

	void ClientActive(int slot)
	{
		(void)slot;
	}

	void ClientDisconnect(int slot)
	{
		(void)slot;
	}

	MODULE_RESULT ClientCommand(int slot, const CCommand* args)
	{
		(void)slot;
		(void)args;
		return MODULE_RESULT::CONTINUE;
	}
}
