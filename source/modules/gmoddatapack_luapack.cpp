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
		std::vector<std::string> changedPaths;
		std::atomic<bool> complete{false};
		bool success = false;
	};

	struct UploadTask
	{
		std::string url;
		std::string method;
		std::string md5;
		std::string resourcePath;
		std::string body;
		std::string error;
		int status = 0;
		bool success = false;
		std::atomic<bool> complete{false};
	};

	struct State
	{
		std::mutex registryMutex;
		std::unordered_map<std::string, FileRecord> files;
		unsigned long long revision = 0;
		bool buildRequested = false;
		IThreadPool* buildPool = nullptr;
		IThreadPool* uploadPool = nullptr;
		BuildTask* activeBuild = nullptr;
		std::vector<UploadTask*> uploads;
		std::map<std::string, Generation> generations;
		std::string currentGeneration;
		std::string salt;
		std::unordered_map<std::string, bool> pendingChanges;
		INetworkStringTableContainer* stringTables = nullptr;
		INetworkStringTable* downloadables = nullptr;
		std::string lockedDownloadUrl;
		bool downloadUrlLocked = false;
		ClientPin clients[ABSOLUTE_PLAYER_LIMIT];
		bool featureEnabledLastFrame = false;
		bool bootstrapRefresh = false;
	};

	static State state;

	static const char* clientBootstrap = R"HOLYLUAPACK(
-- HolyLib luapack bootstrap. The server does not send pack bodies through the netchannel.
do
	local function bootstrap()
		if _G.__holypack_bootstrapped then return end

		local flags = (FCVAR_REPLICATED or 0) + (FCVAR_PROTECTED or 0) +
			(FCVAR_DONTRECORD or 0) + (FCVAR_UNLOGGED or 0) + (FCVAR_UNREGISTERED or 0)
		local ok, manifestConVar = pcall(CreateConVar, "holylib_gmoddatapack_luapack_manifest", "", flags)
		manifestConVar = ok and manifestConVar or GetConVar("holylib_gmoddatapack_luapack_manifest")
		local snapshot = manifestConVar and manifestConVar:GetString() or ""
		if snapshot == "" then return end

		local function warn(message)
			MsgC(Color(255, 170, 40), "[HolyLib luapack] ", color_white, message .. "\n")
		end

		local function fromHex(value)
			if #value % 2 ~= 0 or value:find("[^0-9a-fA-F]") then return nil end
			return (value:gsub("..", function(pair) return string.char(tonumber(pair, 16)) end))
		end

		local function toHex(value)
			return (value:gsub(".", function(byte) return string.format("%02x", string.byte(byte)) end))
		end

		local version, currentGeneration, packDirectoryHex, generationList = snapshot:match("^(%d+)|([^|]+)|([^|]*)|(.*)$")
		local packDirectory = packDirectoryHex and fromHex(packDirectoryHex)
		if version ~= "1" or not currentGeneration or not packDirectory then
			warn("ignored an invalid manifest snapshot; vanilla Lua delivery remains active")
			return
		end

		local manifests = {}
		for entry in string.gmatch(generationList or "", "[^;]+") do
			local generation, md5, saltHex, resourceHex = entry:match("^([^,]+),([^,]+),([^,]+),([^,]+)$")
			local salt, resource = saltHex and fromHex(saltHex), resourceHex and fromHex(resourceHex)
			if generation and md5 and salt and resource then
				manifests[generation] = {generation = generation, md5 = string.lower(md5), salt = salt, resource = resource}
			end
		end

		local function manifestFromSnapshot(value, wantedGeneration)
			local refreshVersion, _, _, refreshList = value:match("^(%d+)|([^|]+)|([^|]*)|(.*)$")
			if refreshVersion ~= "1" then return nil end
			for entry in string.gmatch(refreshList or "", "[^;]+") do
				local generation, md5, saltHex, resourceHex = entry:match("^([^,]+),([^,]+),([^,]+),([^,]+)$")
				if generation == wantedGeneration then
					local salt, resource = fromHex(saltHex), fromHex(resourceHex)
					if salt and resource then return {generation = generation, md5 = string.lower(md5), salt = salt, resource = resource} end
				end
			end
		end

		local function parsePack(contents, manifest)
			if #contents < 1 or string.byte(contents, 1) ~= 1 then return nil, "unsupported pack version" end
			local pack = {vfs = {}, vfsLCL = {}, salt = manifest.salt, manifest = manifest}
			local cursor = 2
			while cursor <= #contents do
				if #contents - cursor + 1 < 52 then return nil, "truncated entry header" end
				local sourceKey = toHex(string.sub(contents, cursor, cursor + 15)); cursor = cursor + 16
				local localKeyOne = toHex(string.sub(contents, cursor, cursor + 15)); cursor = cursor + 16
				local localKeyTwo = toHex(string.sub(contents, cursor, cursor + 15)); cursor = cursor + 16
				local a, b, c, d = string.byte(contents, cursor, cursor + 3); cursor = cursor + 4
				local length = a * 16777216 + b * 65536 + c * 256 + d
				if length < 0 or cursor + length - 1 > #contents then return nil, "truncated entry payload" end
				local source = string.sub(contents, cursor, cursor + length - 1); cursor = cursor + length
				pack.vfs[sourceKey] = source
				pack.vfsLCL[localKeyOne] = source
				pack.vfsLCL[localKeyTwo] = source
			end
			return pack
		end

		local packs = {}
		local downloadFilter = GetConVar("cl_downloadfilter")
		for generation, manifest in pairs(manifests) do
			local compressed = file.Read("download/" .. manifest.resource, "GAME")
			if not compressed then
				if downloadFilter and downloadFilter:GetString() == "none" then
					warn("pack " .. generation .. " is missing because downloads are disabled; set cl_downloadfilter to mapsonly or all. This join will use vanilla Lua delivery")
				else
					warn("pack " .. generation .. " is unavailable; this join will use vanilla Lua delivery")
				end
			else
				local contents = util.Decompress(compressed)
				if not contents then
					warn("pack " .. generation .. " could not be decompressed; this join will use vanilla Lua delivery")
				elseif string.lower(util.MD5(contents)) ~= manifest.md5 then
					warn("pack " .. generation .. " failed its generation MD5 check; this join will use vanilla Lua delivery")
				else
					local pack, parseError = parsePack(contents, manifest)
					if pack then packs[generation] = pack else warn("pack " .. generation .. " is invalid (" .. parseError .. "); this join will use vanilla Lua delivery") end
				end
			end
		end

		if not next(packs) then return end
		_G.__holypack_bootstrapped = true
		_G.__holypack_packs = packs

		local originalCompileFile, originalInclude, originalRunString = CompileFile, include, RunString
		local selectedGeneration

		local function normalizedForms(path)
			path = string.gsub(path or "", "^@", "")
			path = string.gsub(path, "\\", "/")
			local first = string.gsub(path, "^addons/[^/]+/", "")
			first = string.gsub(first, "^gamemodes/[^/]+/entities/", "")
			first = string.gsub(first, "^gamemodes/", "")
			first = string.gsub(first, "^lua/", "")
			local second = string.gsub(path, "^addons/[^/]+/", "")
			second = string.gsub(second, "^gamemodes/", "")
			second = string.gsub(second, "^lua/", "")
			return path, first, second
		end

		local function findSource(pack, path)
			local sourcePath, first, second = normalizedForms(path)
			local salted = function(value) return string.lower(util.MD5(pack.salt .. value)) end
			return pack.vfs[salted(sourcePath)] or pack.vfsLCL[salted(first)] or pack.vfsLCL[salted(second)]
		end

		local function compilePacked(pack, path)
			local source = findSource(pack, path)
			if not source then return nil end
			local compiled = CompileString(source, path, false)
			if type(compiled) ~= "function" then
				warn("failed to compile packed file " .. tostring(path) .. ": " .. tostring(compiled))
				return nil
			end
			return compiled
		end

		function _G.__holypack(generation)
			local pack = packs[generation]
			if not pack then error("HolyLib luapack generation is not mounted: " .. tostring(generation), 2) end
			selectedGeneration = generation
			local info = debug.getinfo(2, "S")
			local sourcePath = info and info.source or ""
			local compiled = compilePacked(pack, string.gsub(sourcePath, "^@", ""))
			if not compiled then error("HolyLib luapack has no entry for " .. tostring(sourcePath), 2) end
			return compiled
		end

		function _G.CompileFile(path)
			local pack = selectedGeneration and packs[selectedGeneration]
			return (pack and compilePacked(pack, path)) or originalCompileFile(path)
		end

		function _G.include(path)
			local pack = selectedGeneration and packs[selectedGeneration]
			local compiled = pack and compilePacked(pack, path)
			if compiled then return compiled() end
			return originalInclude(path)
		end

		function _G.RunString(code, identifier, handleError)
			local pack = selectedGeneration and packs[selectedGeneration]
			local packed = pack and identifier and findSource(pack, identifier)
			return originalRunString(packed or code, identifier, handleError)
		end

		net.Receive("gmsv_holylib_luapack_autorefresh", function()
			local generation = net.ReadString()
			local refreshSnapshot = net.ReadString()
			local downloadUrl = net.ReadString()
			local changedPaths = {}
			for index = 1, net.ReadUInt(16) do changedPaths[index] = net.ReadString() end

			local manifest = manifestFromSnapshot(refreshSnapshot, generation)
			if not manifest or downloadUrl == "" then
				warn("autorefresh generation " .. tostring(generation) .. " has no usable FastDL manifest; future files will use vanilla delivery")
				return
			end

			local url = string.gsub(downloadUrl, "/+$", "") .. "/" .. string.gsub(manifest.resource, "^/+", "")
			http.Fetch(url, function(compressed)
				local contents = util.Decompress(compressed or "")
				if not contents or string.lower(util.MD5(contents)) ~= manifest.md5 then
					warn("autorefresh generation " .. generation .. " failed decompression or MD5 validation; future files will use vanilla delivery")
					return
				end

				local pack, parseError = parsePack(contents, manifest)
				if not pack then
					warn("autorefresh generation " .. generation .. " is invalid (" .. tostring(parseError) .. "); future files will use vanilla delivery")
					return
				end

				local previous = selectedGeneration and packs[selectedGeneration]
				if previous then
					for _, path in ipairs(changedPaths) do
						local sourcePath, first, second = normalizedForms(path)
						local salted = function(value) return string.lower(util.MD5(previous.salt .. value)) end
						previous.vfs[salted(sourcePath)] = nil
						previous.vfsLCL[salted(first)] = nil
						previous.vfsLCL[salted(second)] = nil
					end
				end

				packs[generation] = pack
				selectedGeneration = generation
				RunConsoleCommand("holylib_luapack_ready", generation, manifest.md5)
				for _, path in ipairs(changedPaths) do
					local _, localPath = normalizedForms(path)
					if string.find(localPath, "^autorun/") then include(localPath) end
				end
			end, function(message)
				warn("autorefresh FastDL fetch failed for generation " .. generation .. ": " .. tostring(message) .. "; future files will use vanilla delivery")
			end)
		end)

		for generation, pack in pairs(packs) do
			RunConsoleCommand("holylib_luapack_ready", generation, pack.manifest.md5)
		end
	end

	local ok, message = xpcall(bootstrap, debug.traceback)
	if not ok then MsgC(Color(255, 80, 80), "[HolyLib luapack] bootstrap failed; vanilla Lua delivery remains active: " .. tostring(message) .. "\n") end
end
)HOLYLUAPACK";

	static const char* serverBridge = R"HLPACKSERVER(
util.AddNetworkString("gmsv_holylib_luapack_autorefresh")
concommand.Add("holylib_gmoddatapack_luapack_kill", function(caller)
	if IsValid(caller) and not caller:IsSuperAdmin() then
		caller:ChatPrint("HolyLib luapack kill-switch requires superadmin")
		return
	end
	RunConsoleCommand("holylib_gmoddatapack_luapack_enable", "0")
	Msg("[HolyLib luapack] kill-switch activated; all clients now use vanilla Lua delivery\n")
end, nil, "Immediately disable bundled delivery and restore per-file vanilla Lua networking", FCVAR_DONTRECORD)
hook.Add("HolyLib:LuaPackPublished", "HolyLib:LuaPackAutorefreshBridge", function(generation, manifest, recipients, changedPaths)
	local targets = {}
	for _, index in ipairs(recipients or {}) do
		local target = Player(index)
		if IsValid(target) then targets[#targets + 1] = target end
	end
	if #targets == 0 then return end

	net.Start("gmsv_holylib_luapack_autorefresh")
		net.WriteString(generation)
		net.WriteString(manifest)
		net.WriteString(GetConVar("sv_downloadurl"):GetString())
		net.WriteUInt(math.min(#changedPaths, 65535), 16)
		for index = 1, math.min(#changedPaths, 65535) do net.WriteString(changedPaths[index]) end
	net.Send(targets)
end)
)HLPACKSERVER";

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

	static void EnsureUploadPool()
	{
		if (state.uploadPool)
			return;

		state.uploadPool = V_CreateThreadPool();
		Util::StartThreadPool(state.uploadPool, 1);
	}

	static void UploadPack(UploadTask*& task)
	{
		if (!task)
			return;

		const size_t scheme = task->url.find("://");
		const size_t pathStart = scheme == std::string::npos ? std::string::npos : task->url.find('/', scheme + 3);
		if (scheme == std::string::npos)
		{
			task->error = "ingest URL has no scheme";
			task->complete.store(true);
			return;
		}

		const std::string origin = pathStart == std::string::npos ? task->url : task->url.substr(0, pathStart);
		const std::string path = pathStart == std::string::npos ? "/" : task->url.substr(pathStart);
		if (origin.compare(0, strlen("http://"), "http://") != 0)
		{
			// cpp-httplib is not linked to OpenSSL in HolyLib. Keeping this explicit avoids silently
			// downgrading an operator-configured HTTPS ingest endpoint.
			task->error = "this build supports http:// ingest only (HTTPS is never downgraded)";
			task->complete.store(true);
			return;
		}

		httplib::Client client(origin);
		client.set_connection_timeout(10, 0);
		client.set_read_timeout(30, 0);
		client.set_write_timeout(30, 0);

		httplib::Request request;
		request.method = task->method;
		request.path = path;
		request.body = task->body;
		request.headers.emplace("Content-Type", "application/octet-stream");
		request.headers.emplace("X-HolyLib-LuaPack-MD5", task->md5);
		request.headers.emplace("X-HolyLib-LuaPack-Path", task->resourcePath);

		auto response = client.send(request);
		if (!response)
		{
			task->error = httplib::to_string(response.error());
			task->complete.store(true);
			return;
		}

		task->status = response->status;
		task->success = response->status >= 200 && response->status < 300;
		if (!task->success)
			task->error = "HTTP status " + std::to_string(response->status);
		task->complete.store(true);
	}

	static void QueueIngest(const BuildTask* build, const Generation& generation)
	{
		const Config& currentConfig = GetConfig();
		if (currentConfig.ingestUrl.empty())
			return;

		std::string method = currentConfig.ingestMethod;
		std::transform(method.begin(), method.end(), method.begin(), [](unsigned char value) {
			return static_cast<char>(std::toupper(value));
		});
		if (method.empty() || method.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") != std::string::npos)
		{
			Warning(PROJECT_NAME " - luapack: Ignoring invalid ingest method\n");
			return;
		}

		UploadTask* upload = new UploadTask;
		upload->url = currentConfig.ingestUrl;
		upload->method = method;
		upload->md5 = generation.md5;
		upload->resourcePath = generation.resourcePath;
		upload->body.assign(static_cast<const char*>(build->compressed.GetBase()), build->compressed.GetWritten());
		EnsureUploadPool();
		state.uploads.push_back(upload);
		state.uploadPool->QueueCall(&UploadPack, upload);
	}

	static void NotifyPackBuilt(const BuildTask* build, const Generation& generation)
	{
		if (g_Lua && Lua::PushHook("HolyLib:OnLuaPackBuilt"))
		{
			g_Lua->PushString(generation.id.c_str());
			g_Lua->PushString(generation.resourcePath.c_str());
			g_Lua->PushString(generation.md5.c_str());
			g_Lua->PushNumber(build->compressed.GetWritten());
			g_Lua->CallFunctionProtected(5, 0, true);
		}
		QueueIngest(build, generation);
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
			for (const auto& change : state.pendingChanges)
				task->changedPaths.push_back(change.first);
			state.pendingChanges.clear();
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

	static void NotifyAutorefresh(const std::string& previousGeneration, const BuildTask* task)
	{
		if (!g_Lua || previousGeneration.empty() || previousGeneration == state.currentGeneration ||
			!task || task->changedPaths.empty())
			return;

		std::vector<int> recipients;
		for (int slot = 0; slot < ABSOLUTE_PLAYER_LIMIT; ++slot)
		{
			const ClientPin& client = state.clients[slot];
			if (client.active && client.ready && !client.fallback && client.generation == previousGeneration)
				recipients.push_back(slot);
		}
		if (recipients.empty() || !Lua::PushHook("HolyLib:LuaPackPublished"))
			return;

		auto generation = state.generations.find(state.currentGeneration);
		if (generation == state.generations.end())
			return;

		for (int slot : recipients)
		{
			ClientPin& client = state.clients[slot];
			ReleaseGenerationReference(client);
			client.generation = state.currentGeneration;
			client.deadline = ServerTime() + GetConfig().readyDeadlineSeconds;
			client.ready = false;
			client.fallback = false;
			client.holdsPin = true;
			++generation->second.pins;
		}

		g_Lua->PushString(state.currentGeneration.c_str());
		g_Lua->PushString(luapack_manifest.GetString());
		g_Lua->PreCreateTable(recipients.size(), 0);
		for (size_t index = 0; index < recipients.size(); ++index)
		{
			g_Lua->PushNumber(recipients[index] + 1);
			Util::RawSetI(g_Lua, -2, index + 1);
		}
		g_Lua->PreCreateTable(task->changedPaths.size(), 0);
		for (size_t index = 0; index < task->changedPaths.size(); ++index)
		{
			g_Lua->PushString(task->changedPaths[index].c_str());
			Util::RawSetI(g_Lua, -2, index + 1);
		}
		g_Lua->CallFunctionProtected(5, 0, true);
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
		state.featureEnabledLastFrame = IsEnabled();
	}

	void Shutdown()
	{
		if (state.buildPool)
		{
			Util::DestroyThreadPool(state.buildPool);
			state.buildPool = nullptr;
		}
		if (state.uploadPool)
		{
			Util::DestroyThreadPool(state.uploadPool);
			state.uploadPool = nullptr;
		}
		delete state.activeBuild;
		state.activeBuild = nullptr;
		for (UploadTask* upload : state.uploads)
			delete upload;
		state.uploads.clear();

		std::lock_guard<std::mutex> lock(state.registryMutex);
		state.files.clear();
		state.pendingChanges.clear();
		state.buildRequested = false;
		state.generations.clear();
		state.currentGeneration.clear();
		state.salt.clear();
		state.downloadables = nullptr;
		state.lockedDownloadUrl.clear();
		state.downloadUrlLocked = false;
		state.featureEnabledLastFrame = false;
		state.bootstrapRefresh = false;
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
		const bool enabled = IsEnabled();
		if (enabled != state.featureEnabledLastFrame)
		{
			state.featureEnabledLastFrame = enabled;
			state.bootstrapRefresh = true;
			if (enabled)
			{
				std::lock_guard<std::mutex> lock(state.registryMutex);
				state.buildRequested = true;
			} else {
				// The disabled state must not retain a second copy of all registered Lua. Clearing
				// also prevents a runtime re-enable from publishing a stale pre-refresh snapshot.
				std::lock_guard<std::mutex> lock(state.registryMutex);
				state.files.clear();
				state.pendingChanges.clear();
				state.buildRequested = false;
			}
		}

		for (auto upload = state.uploads.begin(); upload != state.uploads.end();)
		{
			UploadTask* task = *upload;
			if (!task->complete.load())
			{
				++upload;
				continue;
			}

			if (task->success)
				Msg(PROJECT_NAME " - luapack: Optional ingest accepted %s (HTTP %i)\n", task->md5.c_str(), task->status);
			else
				Warning(PROJECT_NAME " - luapack: Optional ingest failed for %s: %s (pack remains published locally)\n",
					task->md5.c_str(), task->error.c_str());
			delete task;
			upload = state.uploads.erase(upload);
		}

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

					const std::string previousGeneration = state.currentGeneration;
					if (!previousGeneration.empty() && previousGeneration != generation.id)
					{
						auto previous = state.generations.find(previousGeneration);
						if (previous != state.generations.end())
							previous->second.retireAfter = ServerTime() + GetConfig().generationRetentionSeconds;
					}

					state.generations[generation.id] = generation;
					state.currentGeneration = generation.id;
					PublishManifest();
					NotifyAutorefresh(previousGeneration, task);
					NotifyPackBuilt(task, generation);
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
		if (!bServerInit && pLua == g_Lua)
			pLua->RunString("HolyLib luapack server bridge", "", serverBridge, true, true);
	}

	void CaptureFile(const GarrysMod::Lua::LuaFile* file)
	{
		if (!IsEnabled() || !file)
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
		state.pendingChanges[virtualPath] = true;
	}

	std::string PrepareVanillaFile(const std::string& virtualPath, const std::string& contents)
	{
		const std::string path = NormalizePath(virtualPath);
		if (!IsEnabled() || (path != "includes/init.lua" && path != "lua/includes/init.lua"))
			return contents;

		return std::string(clientBootstrap) + "\n" + contents;
	}

	bool ConsumeBootstrapRefresh()
	{
		const bool refresh = state.bootstrapRefresh;
		state.bootstrapRefresh = false;
		return refresh;
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
			if (client.active)
				ReleaseGenerationReference(client);
			Msg(PROJECT_NAME " - luapack: client slot %i acknowledged pinned generation %s\n", slot, generationId.c_str());
		}

		return MODULE_RESULT::STOP;
	}
}
