#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include <sstream>
#include "eiface.h"
#include "sourcesdk/GModDataPack.h"
#include "sourcesdk/iluashared.h"
#include "networkstringtable.h"
#include "picosha2/picosha2.h"
#include <atomic>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#undef isalnum // 64x loves to shit on this one AGAIN

class CGModDataPackModule : public IModule
{
public:
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void LevelShutdown() override;
	void InitDetour(bool bPreServer) override;
	const char* Name() override { return "gmoddatapack"; };
	int Compatibility() override { return LINUX32; };
};

static ConVar gmoddatapack_removeserverif("holylib_gmoddatapack_removeserverif", "0", 0, "If enabled, \"if SERVER then\" code blocks are removed from client files");
static ConVar gmoddatapack_removecomments("holylib_gmoddatapack_removecomments", "0", 0, "If enabled, comments are removed from client files");

static CGModDataPackModule g_pGModDataPackModule;
IModule* pGModDataPackModule = &g_pGModDataPackModule;

enum TokenType {
	TK_IF,
	TK_FUNCTION,
	TK_THEN,
	TK_END,
	TK_DO,
	TK_ELSE,
	TK_ELSEIF,
	TK_COMMENT,
	TK_SOMETHING,
	TK_EOF
};

struct Token {
	TokenType type;
	std::string content;
	bool isEmpty = false;
};

static inline TokenType KeywordType(const std::string &word)
{
	if (word == "if") return TK_IF;
	if (word == "function") return TK_FUNCTION;
	if (word == "then") return TK_THEN;
	if (word == "end") return TK_END;
	if (word == "do") return TK_DO;
	if (word == "elseif") return TK_ELSEIF;
	if (word == "else") return TK_ELSE;
	return TK_SOMETHING;
}

static std::vector<Token> TokenizeContent(const std::string& content)
{
	std::vector<Token> tokens;

	size_t scope = 0;
	size_t i = 0;
	while (i < content.size())
	{
		char c = content[i];
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '(' || c == ')')
		{
			tokens.push_back({TK_SOMETHING, std::string(1, c), true});
			i++;
			continue;
		}

		if (c == '-' && i+1 < content.size() && content[i+1] == '-')
		{
			size_t start = i;
			i += 2;

			if (i+1 < content.size() && content[i] == '[' && content[i+1] == '[')
			{
				i += 2;
				while (i+1 < content.size() && !(content[i] == ']' && content[i+1] == ']'))
					i++;

				if (i+1 < content.size())
					i += 2;
			} else
			{
				while (i < content.size() && content[i] != '\n')
					i++;
			}

			tokens.push_back({TK_COMMENT, content.substr(start, i-start)});
			continue;
		}

		if (c == '/' && i+1 < content.size() && (content[i+1]=='/' || content[i+1]=='*'))
		{
			size_t start = i;
			if (content[i+1] == '/')
			{
				i += 2;
				while (i < content.size() && content[i] != '\n')
					i++;
			} else
			{
				i += 2;
				while (i+1 < content.size() && !(content[i]=='*' && content[i+1]=='/'))
					i++;

				if (i+1 < content.size())
					i += 2;
			}

			tokens.push_back({TK_COMMENT, content.substr(start, i-start)});
			continue;
		}

		if (c == '"' && (i == 0 || content[i-1] != '\\'))
		{
			int start = i;
			i++;
			while (i < content.size() && (content[i] != '"' || content[i-1] == '\\'))
				i++;

			// String
			i++;
			tokens.push_back({TK_SOMETHING, content.substr(start, i-start)});
			continue;
		}

		if (c == '[' && i+1 < content.size() && (content[i+1]=='['))
		{
			int start = i;
			i += 2;
			while (i+1 < content.size() && !(content[i]==']' && content[i+1]==']'))
				i++;

			if (i+1 < content.size())
				i += 2;

			// Long String
			tokens.push_back({TK_SOMETHING, content.substr(start, i-start)});
			continue;
		}

		if (std::isalpha(c) || c == '_')
		{
			size_t start = i;
			while (i < content.size() && (std::isalnum(content[i]) || content[i]=='_'))
				i++;

			std::string word = content.substr(start, i - start);
			tokens.push_back({KeywordType(word), word});
			continue;
		}

		tokens.push_back({TK_SOMETHING, std::string(1, c)});
		i++;
	}

	tokens.push_back({TK_EOF, ""});
	return tokens;
}

static size_t SkipEmpty(const std::vector<Token> &tokens, size_t start)
{
	while (start < tokens.size() && tokens[start].isEmpty)
		start++;

	return start;
}

// The goal is to remove parts of a Lua script without changing the line code (so that errors remain easy to debug!)
std::string ProcessTokens(std::vector<Token> &tokens)
{
	std::stringstream ss;
	size_t i = 0;

	while (i < tokens.size()) {
		const Token &tok = tokens[i];
		if (gmoddatapack_removecomments.GetBool() && tok.type == TK_COMMENT)
		{
			for (char c : tok.content)
				if (c == '\n') ss << '\n';

			i++;
			continue;
		}

		if (gmoddatapack_removeserverif.GetBool() && tok.type == TK_IF)
		{
			size_t j = i + 1;
			j = SkipEmpty(tokens, j);
			if (j < tokens.size() && tokens[j].type == TK_SOMETHING && tokens[j].content == "SERVER")
			{
				j++;
				j = SkipEmpty(tokens, j);
				if (j < tokens.size() && tokens[j].type == TK_THEN)
				{
					int depth = 1;
					i = j + 1;
					while (i < tokens.size() && depth > 0)
					{
						if (tokens[i].type == TK_THEN || tokens[i].type == TK_DO || tokens[i].type == TK_FUNCTION)
							depth++;
						else if (tokens[i].type == TK_END || tokens[i].type == TK_ELSEIF)
							depth--;
						else if (tokens[i].type == TK_SOMETHING && tokens[i].content == "\n")
							ss << '\n';
						else if (tokens[i].type == TK_COMMENT)
						{
							for (char c : tokens[i].content)
								if (c=='\n') ss << '\n';
						}
						else if (tokens[i].type == TK_ELSE && depth == 1) {
							tokens[i].content = "do";
							depth--;
							continue;
						}

						i++;
					}
					continue;
				}
			}
		}

		if (tok.type != TK_EOF)
			ss << tok.content;

		i++;
	}

	return ss.str();
}

LUA_FUNCTION_STATIC(gmoddatapack_StripCode)
{
	size_t nLength = -1;
	const char* pContent = Util::CheckLString(LUA, 1, &nLength);

	lua_State* L = luaL_newstate();
	if (luaL_loadbuffer(L, pContent, nLength, "gmoddatapack.StripCode") != LUA_OK)
	{
		LUA->PushNil();
		LUA->PushString(lua_tolstring(L, -1, nullptr));
		lua_close(L);
		return 2;
	} else {
		std::string strContent = pContent;
		std::vector<Token> tokens = TokenizeContent(strContent);
		std::string finalCode = ProcessTokens(tokens);
		LUA->PushString(finalCode.c_str(), finalCode.length());
	}
	lua_close(L);

	return 1;
}

// Copies what GMod does in GModDataPack::GetHashFromString
static std::vector<unsigned char> HashString(const char* pData, unsigned int nLength)
{
	std::vector<unsigned char> hash(32);

	picosha2::hash256_one_by_one hasher;
	hasher.process(pData, pData + nLength);
	hasher.finish();
	
	hasher.get_hash_bytes(hash.begin(), hash.end());
	return hash;
}

/*
	The goal of our LuaDataPack is to avoid compressing files on the main thread.
	Especially if you got many files & huge ones too, then it can easily take ages for them all to compres
	and GMod "conveniently" does it only when the player requests them when joining, so it normally would add up to the loading times
*/
static GModDataPack* g_pDataPack;
class LuaDataPack
{
public:
	class LuaPackEntry
	{
	public:
		~LuaPackEntry() // Lazy handling to save some bytes by not using std::string
		{
			if (content.data())
			{
				delete[] content.data();
				content = nullptr;
			}
		}

		inline bool IsReady() const
		{
			return compressed.GetWritten() != 0;
		}

		inline bool Compress()
		{
			return Bootil::Compression::LZMA::Compress(content.data(), content.length(), compressed);
		}

		inline void Clear()
		{
			if (content.data())
			{
				delete[] content.data();
				content = nullptr;
			}

			compressed.Clear();
		}

		std::string_view content;
		Bootil::AutoBuffer compressed;
	};

	void Initialize();
	void Shutdown();

	void AddFileContents(std::string fileName, std::string content)
	{
		if (!g_pDataPack)
			return;

		int fileID = g_pDataPack->m_pClientLuaFiles->FindStringIndex(fileName.c_str());
		if (fileID == INVALID_STRING_INDEX)
			fileID = g_pDataPack->m_pClientLuaFiles->AddString(true, fileName.c_str());

		if (fileID == INVALID_STRING_INDEX)
		{
			Warning(PROJECT_NAME " - gmoddatapack: Failed to add string \"%s\" to client_lua_files table? Are we full?\n", fileName.c_str());
			return;
		}

		bool bError;
		std::string strippedContent = StripContent(content, &bError);
		if (bError)
		{
			Warning(PROJECT_NAME " - gmoddatapack: Failed to strip file due to lua errors! (%s)", strippedContent.c_str());
		} else {
			content = strippedContent;
		}

		std::lock_guard<std::shared_mutex> lock(m_pLuaFileCacheMutex);
		LuaPackEntry& pEntry = m_pLuaFileCache[fileID];
		pEntry.compressed.Clear();
		pEntry.content = content;

		std::vector<unsigned char> pHash = HashString(content.c_str(), content.length());
		g_pDataPack->m_pClientLuaFiles->SetStringUserData(fileID, pHash.size(), pHash.data()); // New hash

		std::lock_guard<std::mutex> queueLock(m_pCompressQueueMutex);
		m_pCompressQueue.push_back(fileID);
	}

	// We only strip it, if it's valid lua (yes my token stuff is highly sensitive!)
	std::string StripContent(std::string content, bool* bError)
	{
		*bError = false;
		lua_State* L = luaL_newstate();
		if (luaL_loadbuffer(L, content.c_str(), content.length(), "StripContent") != LUA_OK)
		{
			std::string pError = lua_tolstring(L, -1, nullptr);
			lua_close(L);

			*bError = true;
			return pError;
		} else {
			lua_close(L);

			std::vector<Token> tokens = TokenizeContent(content);
			return ProcessTokens(tokens);
		}
	}

	bool CompressFile(LuaPackEntry* pEntry, int fileID)
	{
		// We do it already in AddFileContents
		//bool bError;
		//std::string strippedContent = StripContent(pEntry->content.data(), &bError);
		//if (!bError)
		//{
		//	pEntry->content
		//}

		bool bSuccess = pEntry->Compress();
		if (!bSuccess)
			Warning(PROJECT_NAME " - gmoddatapack: Failed to compress lua file %i", fileID);

		return bSuccess;
	}

	inline LuaPackEntry* GetPackEntry(int fileID)
	{
		if (fileID < 0 || fileID >= MAX_LUA_FILES)
			return nullptr;

		return &m_pLuaFileCache[fileID];
	}

public:
	static constexpr int MAX_LUA_FILES = 1 << 13;
	LuaPackEntry m_pLuaFileCache[MAX_LUA_FILES];
	std::shared_mutex m_pLuaFileCacheMutex;

	std::vector<int> m_pCompressQueue;
	std::mutex m_pCompressQueueMutex;

	std::atomic<ThreadState> m_pWorkerThreadState;
	ThreadHandle_t m_pWorkerThread = nullptr;
};
static LuaDataPack g_pLuaDataPack;

static SIMPLETHREAD_RETURNVALUE WorkerThread(void* pData)
{
	while (g_pLuaDataPack.m_pWorkerThreadState.load() == ThreadState::STATE_RUNNING)
	{
		ThreadSleep(50);

		std::vector<int> pWorkEntires;
		{
			std::lock_guard<std::mutex> lock(g_pLuaDataPack.m_pCompressQueueMutex);
			if (!g_pLuaDataPack.m_pCompressQueue.empty())
			{
				pWorkEntires = std::move(g_pLuaDataPack.m_pCompressQueue);
				g_pLuaDataPack.m_pCompressQueue.clear();
			}
		}

		std::lock_guard<std::shared_mutex> lock(g_pLuaDataPack.m_pLuaFileCacheMutex);
		for (int fileID : pWorkEntires)
		{
			LuaDataPack::LuaPackEntry* pEntry = &g_pLuaDataPack.m_pLuaFileCache[fileID];
			if (pEntry->IsReady()) // Already done? Either we did it, or the main thread.
				continue;
				
			g_pLuaDataPack.CompressFile(pEntry, fileID);
		}
	}

	g_pLuaDataPack.m_pWorkerThreadState.store(ThreadState::STATE_NOTRUNNING);
	return 0;
}

void LuaDataPack::Initialize()
{
	Shutdown();

	// If we load AFTER the server already started, we build our cache ourselves again.
	if (g_pDataPack)
	{
		for (int fileID=0; fileID<g_pDataPack->m_pClientLuaFiles->GetNumStrings(); ++fileID)
		{
			std::string fileName = g_pDataPack->m_pClientLuaFiles->GetString(fileID);
			GarrysMod::Lua::LuaFile* luaFile = Lua::GetShared()->GetCache(fileName);
			if (!luaFile)
				continue;

			LuaPackEntry* pEntry = GetPackEntry(fileID);
			if (!pEntry || pEntry->IsReady())
				continue;

			AddFileContents(fileName, luaFile->contents);
		}
	}

	m_pWorkerThreadState = ThreadState::STATE_RUNNING;
	m_pWorkerThread = CreateSimpleThread((ThreadFunc_t)WorkerThread, this);
}

void LuaDataPack::Shutdown()
{
	if (m_pWorkerThread)
	{
		if (m_pWorkerThreadState.load() != ThreadState::STATE_NOTRUNNING)
		{
			m_pWorkerThreadState.store(ThreadState::STATE_SHOULD_SHUTDOWN);
			while (m_pWorkerThreadState.load() != ThreadState::STATE_NOTRUNNING) // Wait for shutdown
				ThreadSleep(0);
		}

		ReleaseThreadHandle(m_pWorkerThread);
		m_pWorkerThread = nullptr;
	}

	for (int i=0; i<MAX_LUA_FILES; ++i)
		m_pLuaFileCache[i].Clear();
}

static Detouring::Hook detour_GModDataPack_AddOrUpdateFile;
static void hook_GModDataPack_AddOrUpdateFile(GModDataPack* pDataPack, GarrysMod::Lua::LuaFile* file, bool bReCompress)
{
	g_pDataPack = pDataPack;
	if (g_Lua && Lua::PushHook("HolyLib:AddOrUpdateFileToDataPack")) // Allows one to override the clientside content
	{
		g_Lua->PushString(file->GetName());
		g_Lua->PushString(file->GetSource());
		g_Lua->PushString(file->GetContents());
		g_Lua->CallFunctionProtected(4, 0, true);
	}

	lua_State* L = luaL_newstate();
	if (luaL_loadbuffer(L, file->contents.c_str(), file->contents.length(), file->GetName()) != LUA_OK)
	{
		const char* pError = lua_tolstring(L, -1, nullptr);
		Warning(PROJECT_NAME " - gmoddatapack: File \"%s\" contains invalid lua code! (%s)\n", file->GetName(), pError);

		detour_GModDataPack_AddOrUpdateFile.GetTrampoline<Symbols::GModDataPack_AddOrUpdateFile>()(pDataPack, file, bReCompress);
	} else {
		std::string content = file->GetContents();
		std::vector<Token> tokens = TokenizeContent(content);
		std::string finalCode = ProcessTokens(tokens);
		file->SetContents(finalCode.c_str());

		detour_GModDataPack_AddOrUpdateFile.GetTrampoline<Symbols::GModDataPack_AddOrUpdateFile>()(pDataPack, file, bReCompress);

		// file->SetContents(content);
		// We restore the original content in case it's a shared file.
		// Since we forced it to re-compress the file buffer now contains our processed content, which will be sent to clients.
	}
	lua_close(L);

	//if (!pDataPack->m_pClientLuaFiels)
	//	return;

	// NOTE:
	// GMod stores the SHA256 hash in the string userdata for the client to compare against
	// The SHA256 is generated by GModDataPack::GetHashFromString
}

static void SendLuaFile(GModDataPack* pDataPack, int userID, int fileID, std::string strFileName, LuaDataPack::LuaPackEntry* pEntry)
{
	char pBuffer[1 << 16];
	bf_write msg(pBuffer, sizeof(pBuffer));

	msg.WriteByte(GarrysMod::NetworkMessage::LuaFileDownload);
	msg.WriteUBitLong(fileID, 16);
	msg.WriteBytes(pEntry->compressed.GetBase(), pEntry->compressed.GetWritten());

	Util::engineserver->GMOD_SendToClient( userID, msg.GetData(), msg.GetNumBitsWritten() );
}

static Detouring::Hook detour_GModDataPack_SendFileToClient;
static void hook_GModDataPack_SendFileToClient(GModDataPack* pDataPack, int userID, int fileID)
{
	if (!pDataPack || !pDataPack->m_pClientLuaFiles)
	{
		Warning(PROJECT_NAME " - gmoddatapack: Invalid datapack or missing client_lua_files stringtable?\n");
		return;
	}

	INetworkStringTable* clientFiles = pDataPack->m_pClientLuaFiles;
	if (fileID <= 0 || fileID >= clientFiles->GetNumStrings()) // NOTE: GMod only checks < 0 BUT the index 0 is used for paths... too lazy to report it rn
	{
		Warning(PROJECT_NAME " - gmoddatapack: Client requesting crazy file number (%i)\n", fileID);
		return;
	}

	std::string fileName = clientFiles->GetString(fileID);
	GarrysMod::Lua::LuaFile* luaFile = Lua::GetShared()->GetCache(fileName);
	if (!luaFile)
	{
		DevWarning(PROJECT_NAME " - gmoddatapack: Client requested file but doesn't exist! \"%s\"\n", fileName.c_str());
		return;
	}

	LuaDataPack::LuaPackEntry* pEntry;
	{
		std::shared_lock<std::shared_mutex> lock(g_pLuaDataPack.m_pLuaFileCacheMutex);
		pEntry = g_pLuaDataPack.GetPackEntry(fileID);
		if (!pEntry)
		{
			Warning(PROJECT_NAME " - gmoddatapack: Client requested a file which we couldn't get an entry for! (%i)\n", fileID);
			return;
		}

		if (pEntry->IsReady())
		{
			SendLuaFile(pDataPack, userID, fileID, fileName, pEntry);
			return;
		}
	}

	std::lock_guard<std::shared_mutex> lock(g_pLuaDataPack.m_pLuaFileCacheMutex);
	if (pEntry->IsReady()) // In case the worker thread just finished this entry
	{
		SendLuaFile(pDataPack, userID, fileID, fileName, pEntry);
		return;
	}

	DevMsg(PROJECT_NAME " - gmoddatapack: File \"%s\" isn't yet ready to be sent! Compressing on main thread...\n", fileName.c_str());
	if (g_pLuaDataPack.CompressFile(pEntry, fileID))
	{
		SendLuaFile(pDataPack, userID, fileID, fileName, pEntry);
		return;
	}
}

void CGModDataPackModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::FactoryLoader server_loader("server");
	Detour::Create(
		&detour_GModDataPack_AddOrUpdateFile, "GModDataPack::AddOrUpdateFile",
		server_loader.GetModule(), Symbols::GModDataPack_AddOrUpdateFileSym,
		(void*)hook_GModDataPack_AddOrUpdateFile, m_pID
	);

	Detour::Create(
		&detour_GModDataPack_SendFileToClient, "GModDataPack::SendFileToClient",
		server_loader.GetModule(), Symbols::GModDataPack_SendFileToClientSym,
		(void*)hook_GModDataPack_SendFileToClient, m_pID
	);
}

void CGModDataPackModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	if (pLua == g_Lua)
		g_pLuaDataPack.Initialize();

	Util::StartTable(pLua);
		Util::AddFunc(pLua, gmoddatapack_StripCode, "StripCode");
	Util::FinishTable(pLua, "gmoddatapack");
}

void CGModDataPackModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "gmoddatapack");
}

void CGModDataPackModule::LevelShutdown()
{
	g_pLuaDataPack.Shutdown();
}