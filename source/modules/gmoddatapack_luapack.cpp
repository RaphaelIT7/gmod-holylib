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
#include "networkstringtabledefs.h"
#include "picosha2/picosha2.h"
#include "tier1/convar.h"

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
#include <memory>
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
		double retireAfter = 0.0;
		std::shared_ptr<Bootil::AutoBuffer> compressedStub;
		std::unordered_map<std::string, bool> files;
		unsigned int pins = 0;
	};

	struct ClientPin
	{
		std::string generation;
		double deadline = 0.0;
		bool ready = false;
		bool active = false;
		bool fallback = true;
		bool holdsPin = false;
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
		INetworkStringTableContainer* stringTables = nullptr;
		INetworkStringTable* downloadables = nullptr;
		std::string lockedDownloadUrl;
		bool downloadUrlLocked = false;
		ClientPin clients[ABSOLUTE_PLAYER_LIMIT];
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

	static std::shared_ptr<Bootil::AutoBuffer> BuildCompressedStub(const std::string& generation)
	{
		const std::string source = "return __holypack(\"" + generation + "\")()";
		std::vector<unsigned char> hash(32);
		picosha2::hash256_one_by_one hasher;
		hasher.process(source.c_str(), source.c_str() + source.length() + 1);
		hasher.finish();
		hasher.get_hash_bytes(hash.begin(), hash.end());

		auto output = std::make_shared<Bootil::AutoBuffer>();
		output->Write(hash.data(), hash.size());
		if (!Bootil::Compression::LZMA::Compress(source.c_str(), source.length() + 1, *output, 9))
			return nullptr;

		return output;
	}

	static double ServerTime()
	{
		return Util::engineserver ? Util::engineserver->Time() : 0.0;
	}

	static void ReleaseGenerationReference(ClientPin& client)
	{
		if (client.holdsPin && !client.generation.empty())
		{
			auto generation = state.generations.find(client.generation);
			if (generation != state.generations.end() && generation->second.pins > 0)
				--generation->second.pins;
		}
		client.holdsPin = false;
	}

	static void ReleasePin(ClientPin& client)
	{
		ReleaseGenerationReference(client);
		client = ClientPin();
	}

	static void MarkFallback(ClientPin& client)
	{
		ReleaseGenerationReference(client);
		client.ready = false;
		client.fallback = true;
	}

	static bool IsValidSlot(int slot)
	{
		return slot >= 0 && slot < ABSOLUTE_PLAYER_LIMIT;
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

	static ConVar* DownloadUrlConVar()
	{
		return g_pCVar ? g_pCVar->FindVar("sv_downloadurl") : nullptr;
	}

	static bool CoordinateDownloadUrl(bool publishing)
	{
		const Config& currentConfig = GetConfig();
		ConVar* downloadUrl = DownloadUrlConVar();
		if (!downloadUrl)
		{
			if (publishing)
				Warning(PROJECT_NAME " - luapack: sv_downloadurl is unavailable; refusing to publish a FastDL generation\n");
			return false;
		}

		if (V_stricmp(currentConfig.downloadUrlPolicy.c_str(), "require") == 0)
		{
			if (downloadUrl->GetString()[0] == '\0')
			{
				if (publishing)
					Warning(PROJECT_NAME " - luapack: sv_downloadurl is empty and policy=require; generation remains unpublished\n");
				return false;
			}
			return true;
		}

		if (V_stricmp(currentConfig.downloadUrlPolicy.c_str(), "lock") == 0)
		{
			if (!state.downloadUrlLocked)
			{
				state.lockedDownloadUrl = downloadUrl->GetString();
				state.downloadUrlLocked = true;
			}
			else if (state.lockedDownloadUrl != downloadUrl->GetString())
			{
				Warning(PROJECT_NAME " - luapack: restoring operator sv_downloadurl while policy=lock\n");
				downloadUrl->SetValue(state.lockedDownloadUrl.c_str());
			}
		}
		else if (V_stricmp(currentConfig.downloadUrlPolicy.c_str(), "respect") != 0)
		{
			if (publishing)
				Warning(PROJECT_NAME " - luapack: unknown download URL policy '%s'; expected respect, require, or lock\n",
					currentConfig.downloadUrlPolicy.c_str());
			return false;
		}

		return true;
	}

	static bool RegisterDownloadable(const std::string& resourcePath)
	{
		if (!state.stringTables)
			return false;

		if (!state.downloadables)
			state.downloadables = state.stringTables->FindTable("downloadables");
		if (!state.downloadables)
		{
			Warning(PROJECT_NAME " - luapack: downloadables string table is not available\n");
			return false;
		}

		int index = state.downloadables->FindStringIndex(resourcePath.c_str());
		if (index == INVALID_STRING_INDEX)
			index = state.downloadables->AddString(true, resourcePath.c_str());

		if (index == INVALID_STRING_INDEX)
		{
			Warning(PROJECT_NAME " - luapack: failed to register '%s' in downloadables\n", resourcePath.c_str());
			return false;
		}

		return true;
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
	// FCVAR_PROTECTED cannot be used here: Source transmits protected replicated cvars as a boolean,
	// which would destroy the manifest snapshot. The client-created mirror adds the non-server flags.
	static ConVar luapack_manifest(
		"holylib_gmoddatapack_luapack_manifest", "", FCVAR_REPLICATED | FCVAR_DONTRECORD | FCVAR_UNLOGGED,
		"Atomic retained-generation manifest used by the client bootstrap");

	static Config config;

	static std::string HexEncode(const std::string& value)
	{
		static const char digits[] = "0123456789abcdef";
		std::string output;
		output.reserve(value.length() * 2);
		for (unsigned char byte : value)
		{
			output.push_back(digits[(byte >> 4) & 0x0f]);
			output.push_back(digits[byte & 0x0f]);
		}
		return output;
	}

	static void PublishManifest()
	{
		if (!IsEnabled() || state.currentGeneration.empty())
		{
			luapack_manifest.SetValue("");
			return;
		}

		std::ostringstream manifest;
		manifest << "1|" << state.currentGeneration << '|' << HexEncode(GetConfig().packDirectory) << '|';
		bool first = true;
		for (const auto& pair : state.generations)
		{
			const Generation& generation = pair.second;
			if (!first)
				manifest << ';';
			first = false;
			manifest << generation.id << ',' << generation.md5 << ',' << HexEncode(generation.salt) << ','
				<< HexEncode(generation.resourcePath);
		}

		// One SetValue call is the publication barrier: clients never observe a partially-updated generation.
		luapack_manifest.SetValue(manifest.str().c_str());
	}

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
		if (appfn && appfn[0])
			state.stringTables = static_cast<INetworkStringTableContainer*>(appfn[0](INTERFACENAME_NETWORKSTRINGTABLESERVER, nullptr));
		else
		{
			SourceSDK::FactoryLoader engineLoader("engine");
			state.stringTables = engineLoader.GetInterface<INetworkStringTableContainer>(INTERFACENAME_NETWORKSTRINGTABLESERVER);
		}

		if (!state.stringTables)
			Warning(PROJECT_NAME " - luapack: INetworkStringTableContainer is unavailable; FastDL publishing will stay fail-open\n");
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
		state.downloadables = nullptr;
		state.lockedDownloadUrl.clear();
		state.downloadUrlLocked = false;
		for (ClientPin& client : state.clients)
			client = ClientPin();
		luapack_manifest.SetValue("");
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
				} else if (!CoordinateDownloadUrl(true) || !RegisterDownloadable(resourcePath)) {
					Warning(PROJECT_NAME " - luapack: Pack %s exists but was not published; clients remain on vanilla delivery\n", task->md5.c_str());
					std::lock_guard<std::mutex> lock(state.registryMutex);
					state.buildRequested = true;
				} else {
					Generation generation;
					generation.id = task->md5;
					generation.md5 = task->md5;
					generation.salt = task->salt;
					generation.resourcePath = resourcePath;
					generation.sourceRevision = task->sourceRevision;
					generation.publishedAt = ServerTime();
					generation.compressedStub = BuildCompressedStub(generation.id);
					for (const FileRecord& file : task->files)
						generation.files[NormalizePath(file.virtualPath)] = true;
					if (!generation.compressedStub)
					{
						Warning(PROJECT_NAME " - luapack: Failed to build generation stub; pack remains unpublished\n");
						delete task;
						return;
					}
					auto existing = state.generations.find(generation.id);
					if (existing != state.generations.end())
						generation.pins = existing->second.pins;

					if (!state.currentGeneration.empty() && state.currentGeneration != generation.id)
					{
						auto previous = state.generations.find(state.currentGeneration);
						if (previous != state.generations.end())
							previous->second.retireAfter = ServerTime() + GetConfig().generationRetentionSeconds;
					}

					state.generations[generation.id] = generation;
					state.currentGeneration = generation.id;
					PublishManifest();
					Msg(PROJECT_NAME " - luapack: Built immutable generation %s (%u compressed bytes, %u files)\n",
						generation.id.c_str(), task->compressed.GetWritten(), static_cast<unsigned int>(task->files.size()));
				}
			}

			delete task;
		}

		if (!IsEnabled())
		{
			state.downloadUrlLocked = false;
			for (ClientPin& client : state.clients)
			{
				if (!client.generation.empty())
					ReleasePin(client);
			}
			if (luapack_manifest.GetString()[0] != '\0')
				luapack_manifest.SetValue("");
			return;
		}

		CoordinateDownloadUrl(false);
		const double now = ServerTime();
		for (ClientPin& client : state.clients)
		{
			if (!client.generation.empty() && !client.ready && !client.fallback && now > client.deadline)
			{
				MarkFallback(client);
			}
		}

		bool retiredGeneration = false;
		for (auto generation = state.generations.begin(); generation != state.generations.end();)
		{
			if (generation->first != state.currentGeneration && generation->second.pins == 0 &&
				generation->second.retireAfter > 0.0 && now >= generation->second.retireAfter)
			{
				Msg(PROJECT_NAME " - luapack: Retired unpinned generation %s (content-addressed object retained on disk)\n",
					generation->first.c_str());
				generation = state.generations.erase(generation);
				retiredGeneration = true;
				continue;
			}
			++generation;
		}
		if (retiredGeneration)
			PublishManifest();
		if (luapack_manifest.GetString()[0] == '\0' && !state.currentGeneration.empty())
			PublishManifest();
		if (state.activeBuild)
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

	const Bootil::AutoBuffer* StubForClient(int slot, const std::string& virtualPath)
	{
		if (!IsEnabled() || !IsValidSlot(slot))
			return nullptr;

		ClientPin& client = state.clients[slot];
		if (!client.ready || client.fallback || client.generation.empty())
			return nullptr;
		auto generation = state.generations.find(client.generation);
		if (generation == state.generations.end() || !generation->second.compressedStub)
			return nullptr;

		const std::string path = NormalizePath(virtualPath);
		if (path == "includes/init.lua" || path == "lua/includes/init.lua" || generation->second.files.find(path) == generation->second.files.end())
			return nullptr;

		return generation->second.compressedStub.get();
	}

	void ClientConnect(int slot)
	{
		if (!IsValidSlot(slot))
			return;

		ReleasePin(state.clients[slot]);
		ClientPin& client = state.clients[slot];
		if (!IsEnabled() || state.currentGeneration.empty())
			return;

		auto generation = state.generations.find(state.currentGeneration);
		if (generation == state.generations.end())
			return;

		client.generation = generation->first;
		client.deadline = ServerTime() + GetConfig().readyDeadlineSeconds;
		client.fallback = false;
		client.holdsPin = true;
		++generation->second.pins;
	}

	void ClientActive(int slot)
	{
		if (!IsValidSlot(slot))
			return;

		ClientPin& client = state.clients[slot];
		client.active = true;
		ReleaseGenerationReference(client);
	}

	void ClientDisconnect(int slot)
	{
		if (IsValidSlot(slot))
			ReleasePin(state.clients[slot]);
	}

	MODULE_RESULT ClientCommand(int slot, const CCommand* args)
	{
		if (!args || args->ArgC() < 1 || V_stricmp(args->Arg(0), "holylib_luapack_ready") != 0)
			return MODULE_RESULT::CONTINUE;

		// Always consume our private acknowledgement command, including forged or stale generations.
		if (!IsEnabled() || !IsValidSlot(slot) || args->ArgC() != 3)
			return MODULE_RESULT::STOP;

		ClientPin& client = state.clients[slot];
		const std::string generationId = args->Arg(1);
		const std::string md5 = args->Arg(2);
		auto generation = state.generations.find(client.generation);
		if (!client.fallback && !client.generation.empty() && ServerTime() <= client.deadline &&
			generation != state.generations.end() && generationId == client.generation && md5 == generation->second.md5)
		{
			client.ready = true;
			Msg(PROJECT_NAME " - luapack: client slot %i acknowledged pinned generation %s\n", slot, generationId.c_str());
		}

		return MODULE_RESULT::STOP;
	}
}
