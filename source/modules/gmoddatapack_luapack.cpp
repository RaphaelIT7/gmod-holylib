#include "httplib.h"
#include "modules/gmoddatapack_luapack.h"

#include "LuaInterface.h"
#include "detours.h"
#include "eiface.h"
#include "filesystem.h"
#include "lua.h"
#include "module.h"
#include "sourcesdk/iluashared.h"
#include "sourcesdk/tier2.h"

#undef isalnum
#undef isalpha
#undef isspace
#undef isdigit
#undef min
#undef max

#include "util.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
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

	struct Generation
	{
		std::string id;
		std::string md5;
		std::string salt;
		std::string resourcePath;
		unsigned long long sourceRevision = 0;
		double publishedAt = 0.0;
	};

	struct BuildTask
	{
		std::vector<FileRecord> files;
		std::string packDirectory;
		std::string salt;
		unsigned long long sourceRevision = 0;
		Bootil::AutoBuffer uncompressed;
		Bootil::AutoBuffer compressed;
		std::string md5;
		std::string error;
		std::atomic<bool> complete{false};
		bool success = false;
	};

	struct State
	{
		std::mutex registryMutex;
		std::unordered_map<std::string, FileRecord> files;
		unsigned long long revision = 0;
		bool buildRequested = false;
		IThreadPool* buildPool = nullptr;
		BuildTask* activeBuild = nullptr;
		std::map<std::string, Generation> generations;
		std::string currentGeneration;
		std::string salt;
	};

	static State state;

	static std::string NormalizePath(std::string path)
	{
		std::replace(path.begin(), path.end(), '\\', '/');
		while (!path.empty() && (path[0] == '@' || path[0] == '/'))
			path.erase(path.begin());

		return path;
	}

	static bool StartsWith(const std::string& value, const char* prefix)
	{
		const size_t prefixLength = strlen(prefix);
		return value.length() >= prefixLength && value.compare(0, prefixLength, prefix) == 0;
	}

	static std::string StripAddonPrefix(std::string path)
	{
		if (!StartsWith(path, "addons/"))
			return path;

		const size_t slash = path.find('/', strlen("addons/"));
		return slash == std::string::npos ? path : path.substr(slash + 1);
	}

	static std::string LocalKeyFormOne(std::string path)
	{
		path = StripAddonPrefix(NormalizePath(path));
		if (StartsWith(path, "gamemodes/"))
		{
			const size_t entities = path.find("/entities/", strlen("gamemodes/"));
			if (entities != std::string::npos)
				path = path.substr(entities + strlen("/entities/"));
			else
				path.erase(0, strlen("gamemodes/"));
		}

		if (StartsWith(path, "lua/"))
			path.erase(0, strlen("lua/"));

		return path;
	}

	static std::string LocalKeyFormTwo(std::string path)
	{
		path = StripAddonPrefix(NormalizePath(path));
		if (StartsWith(path, "gamemodes/"))
			path.erase(0, strlen("gamemodes/"));
		if (StartsWith(path, "lua/"))
			path.erase(0, strlen("lua/"));

		return path;
	}

	static bool HexToBytes(const std::string& hex, unsigned char* output, size_t outputLength)
	{
		if (hex.length() != outputLength * 2)
			return false;

		for (size_t i = 0; i < outputLength; ++i)
		{
			const char pair[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
			char* end = nullptr;
			const unsigned long value = strtoul(pair, &end, 16);
			if (!end || *end != '\0' || value > 0xff)
				return false;

			output[i] = static_cast<unsigned char>(value);
		}

		return true;
	}

	static bool WritePackKey(Bootil::AutoBuffer& output, const std::string& salt, const std::string& value)
	{
		unsigned char key[16];
		const std::string hash = Bootil::Hasher::MD5::String(salt + value);
		if (!HexToBytes(hash, key, sizeof(key)))
			return false;

		output.Write(key, sizeof(key));
		return true;
	}

	static bool ValidatePack(const BuildTask* task)
	{
		if (!task || task->uncompressed.GetWritten() < 1)
			return false;

		const unsigned char* data = static_cast<const unsigned char*>(task->uncompressed.GetBase());
		const size_t dataLength = task->uncompressed.GetWritten();
		if (data[0] != 1)
			return false;

		size_t offset = 1;
		size_t fileIndex = 0;
		while (offset < dataLength)
		{
			if (dataLength - offset < (16 * 3) + 4 || fileIndex >= task->files.size())
				return false;

			offset += 16 * 3;
			const unsigned int contentLength =
				(static_cast<unsigned int>(data[offset]) << 24) |
				(static_cast<unsigned int>(data[offset + 1]) << 16) |
				(static_cast<unsigned int>(data[offset + 2]) << 8) |
				static_cast<unsigned int>(data[offset + 3]);
			offset += 4;

			if (contentLength > dataLength - offset || contentLength != task->files[fileIndex].contents.length())
				return false;
			if (memcmp(data + offset, task->files[fileIndex].contents.data(), contentLength) != 0)
				return false;

			offset += contentLength;
			++fileIndex;
		}

		return offset == dataLength && fileIndex == task->files.size();
	}

	static void BuildPack(BuildTask*& task)
	{
		if (!task)
			return;

		const unsigned char version = 1;
		task->uncompressed.Write(&version, sizeof(version));
		for (const FileRecord& file : task->files)
		{
			if (file.contents.length() > std::numeric_limits<unsigned int>::max())
			{
				task->error = "a Lua source file exceeds the pack format's u32 length";
				task->complete.store(true);
				return;
			}

			if (!WritePackKey(task->uncompressed, task->salt, file.sourcePath) ||
				!WritePackKey(task->uncompressed, task->salt, LocalKeyFormOne(file.virtualPath)) ||
				!WritePackKey(task->uncompressed, task->salt, LocalKeyFormTwo(file.virtualPath)))
			{
				task->error = "failed to encode an MD5 index key";
				task->complete.store(true);
				return;
			}

			const unsigned int contentLength = static_cast<unsigned int>(file.contents.length());
			const unsigned char length[4] = {
				static_cast<unsigned char>((contentLength >> 24) & 0xff),
				static_cast<unsigned char>((contentLength >> 16) & 0xff),
				static_cast<unsigned char>((contentLength >> 8) & 0xff),
				static_cast<unsigned char>(contentLength & 0xff),
			};
			task->uncompressed.Write(length, sizeof(length));
			task->uncompressed.Write(file.contents.data(), file.contents.length());
		}

		if (!ValidatePack(task))
		{
			task->error = "internal build-to-parse validation failed";
			task->complete.store(true);
			return;
		}

		task->md5 = Bootil::Hasher::MD5::Easy(task->uncompressed.GetBase(), task->uncompressed.GetWritten());
		std::transform(task->md5.begin(), task->md5.end(), task->md5.begin(), [](unsigned char value) {
			return static_cast<char>(std::tolower(value));
		});
		if (!Bootil::Compression::LZMA::Compress(
			task->uncompressed.GetBase(), task->uncompressed.GetWritten(), task->compressed, 9))
		{
			task->error = "LZMA compression failed";
			task->complete.store(true);
			return;
		}

		task->success = true;
		task->complete.store(true);
	}

	static std::string DataDirectory(const std::string& packDirectory)
	{
		return "data/" + packDirectory;
	}

	static std::string ReadOrCreateSalt(const std::string& packDirectory)
	{
		const std::string directory = DataDirectory(packDirectory);
		const std::string path = directory + "/salt.txt";
		g_pFullFileSystem->CreateDirHierarchy(directory.c_str(), "MOD");

		FileHandle_t input = g_pFullFileSystem->Open(path.c_str(), "rb", "MOD");
		if (input != FILESYSTEM_INVALID_HANDLE)
		{
			const unsigned int size = g_pFullFileSystem->Size(input);
			if (size > 0 && size <= 128)
			{
				std::string salt(size, '\0');
				if (g_pFullFileSystem->Read(salt.data(), size, input) == static_cast<int>(size))
				{
					g_pFullFileSystem->Close(input);
					return salt;
				}
			}
			g_pFullFileSystem->Close(input);
		}

		const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
		std::ostringstream entropy;
		entropy << packDirectory << ':' << now << ':' << &state;
		const std::string salt = Bootil::Hasher::MD5::String(entropy.str());

		FileHandle_t output = g_pFullFileSystem->Open(path.c_str(), "wb", "MOD");
		if (output != FILESYSTEM_INVALID_HANDLE)
		{
			g_pFullFileSystem->Write(salt.data(), salt.length(), output);
			g_pFullFileSystem->Close(output);
		}

		return salt;
	}

	static bool WriteImmutableObject(const BuildTask* task, std::string& resourcePath)
	{
		const std::string directory = DataDirectory(task->packDirectory);
		resourcePath = directory + "/" + task->md5 + ".bsp";
		if (g_pFullFileSystem->FileExists(resourcePath.c_str(), "MOD"))
			return true;

		g_pFullFileSystem->CreateDirHierarchy(directory.c_str(), "MOD");
		const std::string temporaryPath = resourcePath + ".tmp." + std::to_string(task->sourceRevision);
		FileHandle_t output = g_pFullFileSystem->Open(temporaryPath.c_str(), "wb", "MOD");
		if (output == FILESYSTEM_INVALID_HANDLE)
			return false;

		const int expected = static_cast<int>(task->compressed.GetWritten());
		const int written = g_pFullFileSystem->Write(task->compressed.GetBase(), expected, output);
		g_pFullFileSystem->Close(output);
		if (written != expected)
			return false;

		return g_pFullFileSystem->RenameFile(temporaryPath.c_str(), resourcePath.c_str(), "MOD");
	}

	static void EnsureBuildPool()
	{
		if (state.buildPool)
			return;

		state.buildPool = V_CreateThreadPool();
		Util::StartThreadPool(state.buildPool, 1);
	}

	static void StartBuild()
	{
		BuildTask* task = new BuildTask;
		{
			std::lock_guard<std::mutex> lock(state.registryMutex);
			if (state.files.empty())
			{
				delete task;
				return;
			}

			task->files.reserve(state.files.size());
			for (const auto& pair : state.files)
			{
				// The bootstrap must always arrive as a real file; it cannot resolve itself from the pack.
				if (pair.first == "includes/init.lua" || pair.first == "lua/includes/init.lua")
					continue;

				task->files.push_back(pair.second);
			}
			task->sourceRevision = state.revision;
			state.buildRequested = false;
		}

		std::sort(task->files.begin(), task->files.end(), [](const FileRecord& left, const FileRecord& right) {
			return left.virtualPath < right.virtualPath;
		});
		task->packDirectory = GetConfig().packDirectory;
		if (state.salt.empty())
			state.salt = ReadOrCreateSalt(task->packDirectory);
		task->salt = state.salt;

		EnsureBuildPool();
		state.activeBuild = task;
		state.buildPool->QueueCall(&BuildPack, task);
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
		if (state.buildPool)
		{
			Util::DestroyThreadPool(state.buildPool);
			state.buildPool = nullptr;
		}
		delete state.activeBuild;
		state.activeBuild = nullptr;

		std::lock_guard<std::mutex> lock(state.registryMutex);
		state.files.clear();
		state.buildRequested = false;
		state.generations.clear();
		state.currentGeneration.clear();
		state.salt.clear();
	}

	void LevelShutdown()
	{
		Shutdown();
	}

	void Think()
	{
		if (state.activeBuild && state.activeBuild->complete.load())
		{
			BuildTask* task = state.activeBuild;
			state.activeBuild = nullptr;

			if (!task->success)
			{
				Warning(PROJECT_NAME " - luapack: Failed to build pack: %s\n", task->error.c_str());
			} else if (IsEnabled()) {
				std::string resourcePath;
				if (!WriteImmutableObject(task, resourcePath))
				{
					Warning(PROJECT_NAME " - luapack: Failed to atomically write pack %s\n", task->md5.c_str());
				} else {
					Generation generation;
					generation.id = task->md5;
					generation.md5 = task->md5;
					generation.salt = task->salt;
					generation.resourcePath = resourcePath;
					generation.sourceRevision = task->sourceRevision;
					generation.publishedAt = Util::engineserver ? Util::engineserver->Time() : 0.0;
					state.generations[generation.id] = generation;
					state.currentGeneration = generation.id;
					Msg(PROJECT_NAME " - luapack: Built immutable generation %s (%u compressed bytes, %u files)\n",
						generation.id.c_str(), task->compressed.GetWritten(), static_cast<unsigned int>(task->files.size()));
				}
			}

			delete task;
		}

		if (!IsEnabled() || state.activeBuild)
			return;

		bool shouldBuild = false;
		{
			std::lock_guard<std::mutex> lock(state.registryMutex);
			shouldBuild = state.buildRequested;
		}
		if (shouldBuild)
			StartBuild();
	}
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
