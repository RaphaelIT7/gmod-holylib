#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include <sstream>
#include "eiface.h"
#include "sourcesdk/GModDataPack.h"
#include "sourcesdk/iluashared.h"
#include "sourcesdk/baseclient.h"
#include "sourcesdk/cnetchan.h"
#include "networkstringtable.h"
#include "picosha2/picosha2.h"
#include <atomic>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#undef isalnum // 64x loves to shit on this one AGAIN
#undef isalpha // 64x loves to shit on this one AGAIN FUCKING HELL
#undef isspace // 64x SHAT AGAIN! OMFG

class CGModDataPackModule : public IModule
{
public:
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void LevelShutdown() override;
	void Think(bool bSimulating) override;
	void InitDetour(bool bPreServer) override;
	void OnClientDisconnect(CBaseClient* pClient) override;
	const char* Name() override { return "gmoddatapack"; };
	int Compatibility() override { return LINUX32 | LINUX64; };
};

static ConVar gmoddatapack_removeserverif("holylib_gmoddatapack_removeserverif", "0", 0, "If enabled, \"if SERVER then\" code blocks are removed from client files");
static ConVar gmoddatapack_removecomments("holylib_gmoddatapack_removecomments", "0", 0, "If enabled, comments are removed from client files");
static ConVar gmoddatapack_fastnetworking("holylib_gmoddatapack_fastnetworking", "0", 0, "(Very Experimental) If enabled, it'll do funky stuff to the networking");

static CGModDataPackModule g_pGModDataPackModule;
IModule* pGModDataPackModule = &g_pGModDataPackModule;

enum TokenType {
	TK_INVALID = 0,
	TK_IF,
	TK_FUNCTION,
	TK_THEN,
	TK_END,
	TK_DO,
	TK_OR,
	TK_AND,
	TK_GREATER_OR_EQUAL,
	TK_GREATER,
	TK_LESS_OR_EQUAL,
	TK_PARENTHESIS,
	TK_LESS,
	TK_ELSE,
	TK_ELSEIF,
	TK_COMMENT,
	TK_STRING,
	TK_SOMETHING,
	TK_EOF
};

struct Token {
	TokenType type;
	std::string content;
	bool isSpace = false;
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
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
		{
			tokens.push_back({TK_SOMETHING, std::string(1, c), true});
			i++;
			continue;
		}

		if (c == '(' || c == ')')
		{
			tokens.push_back({TK_PARENTHESIS, std::string(1, c), true});
			i++;
			continue;
		}

		if (c == '>' && i+1 < content.size() && content[i+1] == '=')
		{
			tokens.push_back({TK_GREATER_OR_EQUAL, content.substr(i, 2), true});
			i+=2;
			continue;
		}

		if (c == '>')
		{
			tokens.push_back({TK_GREATER, std::string(1, c), true});
			i++;
			continue;
		}

		if (c == '<' && i+1 < content.size() && content[i+1] == '=')
		{
			tokens.push_back({TK_LESS_OR_EQUAL, content.substr(i, 2), true});
			i+=2;
			continue;
		}

		if (c == '<')
		{
			tokens.push_back({TK_LESS, std::string(1, c), true});
			i++;
			continue;
		}

		if (i+1 < content.size() && ((c == '|' && content[i+1] == '|') || (c == 'o' && content[i+1] == 'r')))
		{
			tokens.push_back({TK_OR, content.substr(i, 2), true});
			i+=2;
			continue;
		}

		if (i+1 < content.size() && ((c == '&' && content[i+1] == '&') || (c == 'a' && content[i+1] == 'n' && i+2 < content.size() && content[i+2] == 'd')))
		{
			tokens.push_back({TK_AND, content.substr(i, c == '&' ? 2 : 3), true});
			i+= c == '&' ? 2 : 3;
			continue;
		}

		if (c == '-' && i+1 < content.size() && content[i+1] == '-')
		{
			size_t start = i;
			i += 2;

			if (i+1 < content.size() && content[i] == '[')
			{
				i += 1;
				int count = 0;
				while (i+1 < content.size() && content[i]=='=')
				{
					i++;
					count++;
				}

				if (content[i] == '[')
				{
					while (i+1 < content.size())
					{
						i++;
						if (content[i] == ']')
						{
							if (count == 0 && i+1 < content.size() && content[i+1] == ']')
								break;

							i++;
							int nextCount = 0;
							while (i+1 < content.size() && content[i]=='=')
							{
								i++;
								nextCount++;
							}

							if (nextCount == count && content[i] == ']')
								break;

							i--;
						}
					}

					if (content[i] == ']') {
						if (count > 0)
							i++; // Skip second ]
						else
							i=i+2;
					}
				} else
				{
					while (i < content.size() && content[i] != '\n')
						i++;
				}
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

		if ((c == '\'' || c == '"') && (i == 0 || (content[i-1] != '\\' || (i >= 2 && content[i-2] == '\\'))))
		{
			int start = i;
			i++;
			while (i < content.size() && (content[i] != c || (content[i-1] == '\\' && content[i-2] != '\\')))
				i++;

			// String
			i++;
			tokens.push_back({TK_STRING, content.substr(start, i-start)});
			continue;
		}

		if (c == '[' && (i == 0 || content[i-1] != '\\') && i+1 < content.size())
		{
			int start = i;
			i += 1;
			int count = 0;
			while (i+1 < content.size() && content[i]=='=')
			{
				i++;
				count++;
			}

			if (content[i]=='[')
			{
				while (i+1 < content.size())
				{
					i++;
					if (content[i] == ']')
					{
						if (count == 0 && i+1 < content.size() && content[i+1] == ']')
							break;

						i++;
						int nextCount = 0;
						while (i+1 < content.size() && content[i]=='=')
						{
							i++;
							nextCount++;
						}

						if (nextCount == count && content[i] == ']')
							break;

						i--;
					}
				}

				if (content[i] == ']') {
					if (count > 0)
						i++; // Skip second ]
					else
						i=i+2;
				}

				// Long String
				tokens.push_back({TK_STRING, content.substr(start, i-start)});
				continue;
			}
			i = start;
		}

		if (std::isalpha(c) || c == '_')
		{
			size_t start = i;
			while (i < content.size() && (std::isalnum(content[i]) || content[i]=='_'))
				i++;

			std::string strWord = content.substr(start, i - start);
			tokens.push_back({KeywordType(strWord), strWord});
			continue;
		}

		tokens.push_back({TK_SOMETHING, std::string(1, c)});
		i++;
	}

	tokens.push_back({TK_EOF, ""});
	return tokens;
}

static bool CanServerConditionBeRemoved(const std::vector<Token> &tokens, size_t start)
{
	bool isServer = false;
	int depth = 0;
	while (tokens[start].type != TK_THEN && tokens[start].type != TK_DO)
	{
		if (depth == 0)
		{
			if (tokens[start].type == TK_SOMETHING && tokens[start].content == "SERVER")
				isServer = true;

			if (tokens[start].type == TK_OR) // We don't know yet
				isServer = false;
		}

		if (tokens[start].type == TK_PARENTHESIS)
		{
			if (tokens[start].content == "(")
				++depth;
			else
				--depth;
		}

		start++;
	}

	return isServer;
}

static size_t SkipEmpty(const std::vector<Token> &tokens, size_t start)
{
	while (start < tokens.size() && (tokens[start].isSpace || tokens[start].type == TK_PARENTHESIS))
		start++;

	return start;
}

// The goal is to remove parts of a Lua script without changing the line number (so that errors remain easy to debug!)
static size_t RemoveScoped(size_t i, size_t j, std::vector<Token> &tokens, std::stringstream& ss, TokenType tok)
{
	int depth = 1;
	i = j + 1;
	bool bHasLineBreaks = false; // If it's a one line if -> "if x then x else x end" then we won't restore spaces
	while (i < tokens.size() && depth > 0)
	{
		if (tokens[i].type == TK_THEN || tokens[i].type == TK_DO || tokens[i].type == TK_FUNCTION)
			depth++;
		else if (tokens[i].type == TK_END)
		{
			depth--;
			if (tok == TK_ELSEIF && depth <= 0)
				continue;
		}
		else if (tokens[i].type == TK_SOMETHING && tokens[i].content == "\n")
		{
			bHasLineBreaks = true;
			ss << '\n';
		}
		else if (tokens[i].type == TK_COMMENT)
		{
			for (char c : tokens[i].content)
				if (c=='\n') ss << '\n';
		}
		else if (tokens[i].type == TK_ELSE && depth == 1)
		{
			if (tok == TK_IF)
				tokens[i].content = "do";

			// If we had for example "	elseif xx then" we want to restore the space before it.
			while (bHasLineBreaks && i-1 > 0 && tokens[i-1].isSpace && !(tokens[i-1].type == TK_SOMETHING && tokens[i-1].content == "\n"))
				i--;

			depth--;
			continue;
		}
		else if (tokens[i].type == TK_ELSEIF)
		{
			depth--;
			if (depth <= 0)
			{
				tokens[i].content = tok == TK_IF ? "if" : "elseif";
				// If we had for example "	elseif xx then" we want to restore the space before it.
				while (bHasLineBreaks && i-1 > 0 && tokens[i-1].isSpace && !(tokens[i-1].type == TK_SOMETHING && tokens[i-1].content == "\n"))
					i--;

				continue;
			}
		}

		i++;
	}

	return i;
}

std::string ProcessTokens(std::vector<Token> &tokens, bool bRemoveServerCode, bool bRemoveComments)
{
	std::stringstream ss;
	size_t i = 0;

	while (i < tokens.size()) {
		const Token &tok = tokens[i];
		if (bRemoveComments && tok.type == TK_COMMENT)
		{
			for (char c : tok.content)
				if (c == '\n') ss << '\n';

			i++;
			continue;
		}

		if (bRemoveServerCode && tok.type == TK_IF)
		{
			size_t j = i + 1;
			j = SkipEmpty(tokens, j);
			if (CanServerConditionBeRemoved(tokens, j))
			{
				j++;
				j = SkipEmpty(tokens, j);
				if (j < tokens.size() && tokens[j].type == TK_THEN)
				{
					i = RemoveScoped(i, j, tokens, ss, TK_IF);
					continue;
				}
			}
		}

		if (gmoddatapack_removeserverif.GetBool() && tok.type == TK_ELSEIF)
		{
			size_t j = i + 1;
			j = SkipEmpty(tokens, j);
			if (CanServerConditionBeRemoved(tokens, j))
			{
				j++;
				j = SkipEmpty(tokens, j);
				if (j < tokens.size() && tokens[j].type == TK_THEN)
				{
					i = RemoveScoped(i, j, tokens, ss, TK_ELSEIF);
					continue;
				}
			}
		}

		if (tok.type != TK_EOF)
			ss << tok.content;

		i++;
	}

	std::string pCode = ss.str();
	bool bHasCode = false;
	for (char c : pCode)
	{
		if (!std::isspace(static_cast<unsigned char>(c)))
		{
			bHasCode = true;
			break;
		}
	}

	if (!bHasCode)
		pCode = "--"; // Nothing to see :3

	return pCode;
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

static inline void CallLuaTokenizeContent(GarrysMod::Lua::ILuaInterface* LUA, std::vector<Token>& tokens, int fileID, bool bIsHook)
{
	LUA->PreCreateTable(tokens.size(), 0);
	int idx = 0;
	for (const Token& tok : tokens)
	{
		LUA->PreCreateTable(0, 3);

		LUA->PushString("isSpace");
		LUA->PushBool(tok.isSpace);
		LUA->RawSet(-3);

		LUA->PushString("type");
		LUA->PushNumber(tok.type);
		LUA->RawSet(-3);

		LUA->PushString("content");
		LUA->PushString(tok.content.c_str(), tok.content.length());
		LUA->RawSet(-3);

		Util::RawSetI(LUA, -2, ++idx);
	}

	LUA->PushNumber(fileID);

	if (LUA->CallFunctionProtected(2 + (bIsHook ? 1 : 0), 1, true))
	{
		if (LUA->IsType(-1, GarrysMod::Lua::Type::Table))
		{
			bool bInvalid = false;
			std::vector<Token> pNewTokens;

			LUA->Push(-1);
			LUA->PushNil();
			while (LUA->Next(-2))
			{
				if (!LUA->IsType(-1, GarrysMod::Lua::Type::Table))
				{
					bInvalid = true;
					LUA->Pop(2);
					break;
				}

				Token newToken;

				LUA->PushString("isSpace");
				LUA->RawGet(-2);
				newToken.isSpace = LUA->GetBool(-1);
				LUA->Pop(1);

				LUA->PushString("type");
				LUA->RawGet(-2);
				newToken.type = (TokenType)LUA->GetNumber(-1);
				LUA->Pop(1);
				if (newToken.type <= TK_INVALID || newToken.type > TK_EOF)
				{
					bInvalid = true;
					LUA->Pop(2);
					break;
				}

				LUA->PushString("content");
				LUA->RawGet(-2);

				unsigned int nLength;
				const char* pContent = LUA->GetString(-1, &nLength);
				LUA->Pop(1);
				if (!pContent)
				{
					bInvalid = true;
					LUA->Pop(2);
					break;
				}

				newToken.content = std::string(pContent, nLength);
				pNewTokens.push_back(newToken);
				LUA->Pop(1);
			}
			LUA->Pop(1);

			if (!bInvalid)
			{
				tokens.clear();
				tokens = std::move(pNewTokens);
			}
		}
		LUA->Pop(1);
	}
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
		inline bool IsReady() const
		{
			return compressed.GetWritten() != 0;
		}

		inline bool Compress()
		{
			// Leading 32 bytes are the SHA256, used by the client to verify if the content matches the hash in the fileID's string userdata!
			std::vector<unsigned char> pHash = HashString((const char*)content.data(), content.length() + 1);
			compressed.Write(pHash.data(), pHash.size());

			return Bootil::Compression::LZMA::Compress(content.data(), content.length() + 1, compressed, 9);
		}

		inline void Clear()
		{
			if (content.length() > 0)
			{
				delete[] content.data();
				content = "";
			}

			compressed.Clear();
		}

		std::string content = "";
		Bootil::AutoBuffer compressed;
		std::shared_mutex mutex; // Per entry instead of a global mutex to avoid blocking the main thread for other entries while compressing
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

		LuaPackEntry& pEntry = m_pLuaFileCache[fileID];
		std::lock_guard<std::shared_mutex> lock(pEntry.mutex);
		if (pEntry.content == content) // Nothing changed
			return;

		pEntry.compressed.Clear();
		pEntry.content = content;

		std::lock_guard<std::mutex> queueLock(m_pCompressQueueMutex);
		m_pCompressQueue.push_back(fileID);
	}

	// We only strip it, if it's valid lua (yes my token stuff is highly sensitive!)
	std::string StripContent(std::string content, bool* bError, int fileID)
	{
		*bError = false;
		lua_State* L = luaL_newstate();
		if (luaL_loadbuffer(L, content.c_str(), content.length(), "StripContent") != LUA_OK)
		{
			std::string pError = "";
			size_t nLength;
			const char* err = lua_tolstring(L, -1, &nLength);
			if (err)
				pError = std::string(err, nLength);

			lua_close(L);

			*bError = true;
			return pError;
		}
		lua_close(L);

		std::vector<Token> tokens = TokenizeContent(content);

		Lua::ScopedThreadAccess pThreadScope;
		auto LUA = pInterface.GetLua();
		if (LUA)
		{
			Lua::ThreadAccess pScope(LUA);
			if (Lua::PushHook("HolyLib:OnTokenizeContent", LUA))
			{
				CallLuaTokenizeContent(LUA, tokens, fileID, true);
			}
		}

		return ProcessTokens(tokens, gmoddatapack_removeserverif.GetBool(), gmoddatapack_removecomments.GetBool());
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

		bool bError;
		std::string strippedContent = StripContent(pEntry->content, &bError, fileID);
		if (bError)
		{
			Warning(PROJECT_NAME " - gmoddatapack: Failed to strip fileID \"%i\" due to lua errors! (%s)", fileID, strippedContent.c_str());
		} else {
			pEntry->content = strippedContent;
		}

		std::vector<unsigned char> pHash = HashString(pEntry->content.c_str(), pEntry->content.length() + 1);
		g_pDataPack->m_pClientLuaFiles->SetStringUserData(fileID, pHash.size(), pHash.data()); // New hash

		bool bSuccess = pEntry->Compress();
		if (!bSuccess)
			Warning(PROJECT_NAME " - gmoddatapack: Failed to compress lua file %i\n", fileID);
		else if (g_pGModDataPackModule.InDebug())
			Msg(PROJECT_NAME " - gmoddatapack: Compressed lua file %i (compressed/uncompressed: %i/%i)\n", fileID, pEntry->compressed.GetWritten(), pEntry->content.length());

		return bSuccess;
	}

	inline LuaPackEntry* GetPackEntry(int fileID)
	{
		if (fileID < 0 || fileID >= MAX_LUA_FILES)
			return nullptr;

		return &m_pLuaFileCache[fileID];
	}

	struct PlayerQueue
	{
		std::vector<int> pQueue;
		bool usedUnreliable = false;
		double reconnectTime = -1;
	};

public:
	static constexpr int MAX_LUA_FILES = 1 << 13;
	LuaPackEntry m_pLuaFileCache[MAX_LUA_FILES];

	PlayerQueue m_pPlayerQueue[ABSOLUTE_PLAYER_LIMIT];

	std::vector<int> m_pCompressQueue;
	std::mutex m_pCompressQueueMutex;

	std::atomic<ThreadState> m_pWorkerThreadState;
	ThreadHandle_t m_pWorkerThread = nullptr;

	Lua::ILuaInterfaceReference pInterface;
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

		for (int fileID : pWorkEntires)
		{
			LuaDataPack::LuaPackEntry* pEntry = &g_pLuaDataPack.m_pLuaFileCache[fileID];
			std::lock_guard<std::shared_mutex> lock(pEntry->mutex);
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

	// We keep it, simply because then we can re-use stuff
	//for (int i=0; i<MAX_LUA_FILES; ++i)
	//	m_pLuaFileCache[i].Clear();
}

static Detouring::Hook detour_GModDataPack_AddOrUpdateFile;
static void hook_GModDataPack_AddOrUpdateFile(GModDataPack* pDataPack, GarrysMod::Lua::LuaFile* file, bool bReCompress)
{
	g_pDataPack = pDataPack;
	g_pLuaDataPack.AddFileContents(file->GetName(), file->GetContents());
	/*
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
	lua_close(L);*/

	//if (!pDataPack->m_pClientLuaFiels)
	//	return;

	// NOTE:
	// GMod stores the SHA256 hash in the string userdata for the client to compare against
	// The SHA256 is generated by GModDataPack::GetHashFromString
}

static void SendLuaFile(int clientIdx, int fileID, LuaDataPack::LuaPackEntry* pEntry, bool bNoFastTransmit = false)
{
	if (!bNoFastTransmit && gmoddatapack_fastnetworking.GetBool() && clientIdx < ABSOLUTE_PLAYER_LIMIT)
	{
		g_pLuaDataPack.m_pPlayerQueue[clientIdx].pQueue.push_back(fileID);
		return;
	}

	char pBuffer[1 << 16];
	bf_write msg(pBuffer, sizeof(pBuffer));

	msg.WriteByte(GarrysMod::NetworkMessage::LuaFileDownload);
	msg.WriteUBitLong(fileID, 16);
	msg.WriteBytes(pEntry->compressed.GetBase(), pEntry->compressed.GetWritten());

	Util::engineserver->GMOD_SendToClient( clientIdx, msg.GetData(), msg.GetNumBitsWritten() );

	if (g_pGModDataPackModule.InDebug())
		Msg(PROJECT_NAME " - gmoddatapack: Sent FileID %i though reliable stream!\n", fileID);
}

static Detouring::Hook detour_GModDataPack_SendFileToClient;
static void hook_GModDataPack_SendFileToClient(GModDataPack* pDataPack, int clientIdx, int fileID)
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
		pEntry = g_pLuaDataPack.GetPackEntry(fileID);
		if (!pEntry)
		{
			Warning(PROJECT_NAME " - gmoddatapack: Client requested a file which we couldn't get an entry for! (%i)\n", fileID);
			return;
		}

		std::shared_lock<std::shared_mutex> lock(pEntry->mutex);
		if (pEntry->IsReady())
		{
			SendLuaFile(clientIdx, fileID, pEntry);
			return;
		}
	}

	std::lock_guard<std::shared_mutex> lock(pEntry->mutex);
	if (pEntry->IsReady()) // In case the worker thread just finished this entry
	{
		SendLuaFile(clientIdx, fileID, pEntry);
		return;
	}

	DevMsg(PROJECT_NAME " - gmoddatapack: File \"%s\" isn't yet ready to be sent! Compressing on main thread...\n", fileName.c_str());
	if (g_pLuaDataPack.CompressFile(pEntry, fileID))
	{
		SendLuaFile(clientIdx, fileID, pEntry);
		return;
	}
}


LUA_FUNCTION_STATIC(gmoddatapack_StripCode)
{
	size_t nLength = -1;
	const char* pContent = Util::CheckLString(LUA, 1, &nLength);
	bool bRemoveServerCode = Util::CheckBoolOpt(LUA, 2, gmoddatapack_removeserverif.GetBool());
	bool bRemoveComments = Util::CheckBoolOpt(LUA, 3, gmoddatapack_removecomments.GetBool());


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
		if (LUA->IsType(3, GarrysMod::Lua::Type::Function))
		{
			LUA->Push(3);
			CallLuaTokenizeContent(LUA, tokens, -1, false);
		}

		std::string finalCode = ProcessTokens(tokens, bRemoveServerCode, bRemoveComments);
		LUA->PushString(finalCode.c_str(), finalCode.length());
	}
	lua_close(L);

	return 1;
}

LUA_FUNCTION_STATIC(gmoddatapack_GetStoredCode)
{
	const char* pFileName = LUA->CheckString(1);
	if (!g_pDataPack)
	{
		LUA->PushNil();
		return 1;
	}

	GarrysMod::Lua::LuaFile* luaFile = Lua::GetShared()->GetCache(pFileName);
	if (!luaFile)
	{
		LUA->PushNil();
		return 1;
	}

	int fileID = g_pDataPack->m_pClientLuaFiles->FindStringIndex(luaFile->GetName());
	if (fileID == INVALID_STRING_INDEX)
	{
		LUA->PushNil();
		return 1;
	}

	std::string content = "";
	LuaDataPack::LuaPackEntry* pEntry = g_pLuaDataPack.GetPackEntry(fileID);
	if (pEntry)
	{
		std::lock_guard<std::shared_mutex> lock(pEntry->mutex);
		content = pEntry->content;
	}

	LUA->PushString(content.c_str(), content.length());
	return 1;
}

LUA_FUNCTION_STATIC(gmoddatapack_GetCompressedSize)
{
	const char* pFileName = LUA->CheckString(1);
	if (!g_pDataPack)
	{
		LUA->PushNil();
		return 1;
	}

	GarrysMod::Lua::LuaFile* luaFile = Lua::GetShared()->GetCache(pFileName);
	if (!luaFile)
	{
		LUA->PushNil();
		return 1;
	}

	int fileID = g_pDataPack->m_pClientLuaFiles->FindStringIndex(luaFile->GetName());
	if (fileID == INVALID_STRING_INDEX)
	{
		LUA->PushNil();
		return 1;
	}

	size_t size = 0;
	LuaDataPack::LuaPackEntry* pEntry = g_pLuaDataPack.GetPackEntry(fileID);
	if (pEntry)
	{
		std::lock_guard<std::shared_mutex> lock(pEntry->mutex);
		size = pEntry->compressed.GetWritten();
	}

	LUA->PushNumber(size);
	return 1;
}

LUA_FUNCTION_STATIC(gmoddatapack_MarkAsTokenizeThread)
{
	Lua::ScopedThreadAccess pThreadScope;
	if (g_pLuaDataPack.pInterface.GetLua())
		Lua::RemoveLuaInterfaceReference(&g_pLuaDataPack.pInterface);

	Lua::AddLuaInterfaceReference(LUA, &g_pLuaDataPack.pInterface);
	return 0;
}

static void SendFileThroughUnreliable(int clientIdx, int fileID)
{
	LuaDataPack::LuaPackEntry* pEntry = g_pLuaDataPack.GetPackEntry(fileID);
	std::lock_guard<std::shared_mutex> lock(pEntry->mutex);
	if (!pEntry->IsReady())
	{
		DevMsg(PROJECT_NAME " - gmoddatapack: File \"%i\" isn't yet ready to be sent! Compressing on main thread...\n", fileID);
		if (!g_pLuaDataPack.CompressFile(pEntry, fileID))
			return;
	}

	// Idea: What if... we simply nuke the unreliable stream to send it?
	// The client wouldn't really complain if we send both reliable and unreliable... right?
	// Also, I am like 99% sure we can just send RequestLuaFiles once we assumed we sent everything and the client will tell us what is missing

	CBaseClient* pClient = Util::GetClientByIndex(clientIdx);
	if (pClient && pClient->GetNetChannel())
	{
		static constexpr int BUFFER_SIZE = (1 << 16) * 8;
		CNetChan* pChannel = (CNetChan*)pClient->GetNetChannel();

		int totalLength = 1 + 2 + pEntry->compressed.GetWritten();
		if (pChannel->m_StreamUnreliable.GetNumBytesLeft() < totalLength)
			pChannel->Transmit(false); // We got no space? Let's make some

		pChannel->m_StreamUnreliable.WriteUBitLong(svc_GMod_ServerToClient, NETMSG_TYPE_BITS);
		pChannel->m_StreamUnreliable.WriteUBitLong(totalLength * 8, 20);
		pChannel->m_StreamUnreliable.WriteByte(GarrysMod::NetworkMessage::LuaFileDownload);
		pChannel->m_StreamUnreliable.WriteUBitLong(fileID, 16);
		pChannel->m_StreamUnreliable.WriteBytes(pEntry->compressed.GetBase(), pEntry->compressed.GetWritten());

		if (g_pCVar)
		{
			// This is like the most common convar to limit us when sending out a lot of data.
			// So let's raise it and take the speed :hehe:
			ConVar* pVar = g_pCVar->FindVar("net_splitrate");
			if (pVar && V_stricmp(pVar->GetString(), pVar->GetDefault()) == 0)
				pVar->SetValue(100); // 100 was too much o.o let's lower it, I'm not trying to send 20MB all at once :skull: - nevermind, works now
		}

		if (g_pGModDataPackModule.InDebug())
			Msg(PROJECT_NAME " - gmoddatapack: Sent FileID %i though unreliable stream!\n", fileID);

		pChannel->Transmit(false);
		pChannel->m_fClearTime = pChannel->GetTime() + 10; // Screw you! We don't want to proceed into the SignOnState before were done!
	}
}

void CGModDataPackModule::OnClientDisconnect(CBaseClient* pClient)
{
	int slot = pClient->GetPlayerSlot();
	if (slot < 0 || slot >= ABSOLUTE_PLAYER_LIMIT)
		return;

	g_pLuaDataPack.m_pPlayerQueue[slot].pQueue.clear();
	g_pLuaDataPack.m_pPlayerQueue[slot].reconnectTime = -1;
}

static double g_nLastSend = 0;
void CGModDataPackModule::Think(bool bSimulating)
{
	double currentTime = Util::engineserver->Time();
	if (currentTime < (g_nLastSend + 0.05))
		return;

	g_nLastSend = currentTime;
	for (int i=0; i<ABSOLUTE_PLAYER_LIMIT; ++i)
	{
		LuaDataPack::PlayerQueue& pPlayerInfo = g_pLuaDataPack.m_pPlayerQueue[i];
		if (pPlayerInfo.reconnectTime != -1 && currentTime > pPlayerInfo.reconnectTime)
		{
			CBaseClient* pClient = Util::GetClientByIndex(i);
			if (pClient && pClient->GetNetChannel())
				pClient->Reconnect();

			pPlayerInfo.reconnectTime = -1;
			pPlayerInfo.usedUnreliable = false;
		}

		if (pPlayerInfo.pQueue.empty())
			continue;

		// As the name SendFileThroughUnreliable implies, it's not reliable, when only a few files are left, it's more efficient to send them though the slower reliable stream.
		if (pPlayerInfo.pQueue.size() < 100 && !pPlayerInfo.usedUnreliable)
		{
			for (int fileID : pPlayerInfo.pQueue)
			{
				LuaDataPack::LuaPackEntry* pEntry = g_pLuaDataPack.GetPackEntry(fileID);
				std::lock_guard<std::shared_mutex> lock(pEntry->mutex);
				SendLuaFile(i, fileID, pEntry, true);
			}
			pPlayerInfo.pQueue.clear();
		} else {
			pPlayerInfo.usedUnreliable = true;
			for (int j=0; j<10; ++j)
			{
				SendFileThroughUnreliable(i, pPlayerInfo.pQueue.back());
				pPlayerInfo.pQueue.pop_back();

				if (pPlayerInfo.pQueue.empty())
					break;
			}

			CBaseClient* pClient = Util::GetClientByIndex(i);
			if (pClient && pClient->GetNetChannel())
			{
				CNetChan* pChannel = (CNetChan*)pClient->GetNetChannel();
				pChannel->ProcessStream();

				if (pPlayerInfo.pQueue.empty())
				{
					pPlayerInfo.reconnectTime = currentTime + 1; // Give some time for processing
					DevMsg(PROJECT_NAME " - gmoddatapack: Marked for reconnect! (%s)\n", pClient->GetClientName());
				}
			}
		}
	}
}

bool GMODDataPack_SetSignOnState(CBaseClient* cl, int state)
{
	int slot = cl->GetPlayerSlot();
	if (slot < 0 || slot >= ABSOLUTE_PLAYER_LIMIT)
		return false;

	if (state != SIGNONSTATE_PRESPAWN)
		return false;

	return !g_pLuaDataPack.m_pPlayerQueue[slot].pQueue.empty();
}

#if SYSTEM_WINDOWS
DETOUR_THISCALL_START()
	DETOUR_THISCALL_ADDFUNC2(hook_GModDataPack_AddOrUpdateFile, AddOrUpdateFile, GModDataPack*, GarrysMod::Lua::LuaFile*, bool);
	DETOUR_THISCALL_ADDFUNC2(hook_GModDataPack_SendFileToClient, SendFileToClient, GModDataPack*, int, int);
DETOUR_THISCALL_FINISH()
#endif

void CGModDataPackModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	DETOUR_PREPARE_THISCALL();
	SourceSDK::FactoryLoader server_loader("server");
	Detour::Create(
		&detour_GModDataPack_AddOrUpdateFile, "GModDataPack::AddOrUpdateFile",
		server_loader.GetModule(), Symbols::GModDataPack_AddOrUpdateFileSym,
		(void*)DETOUR_THISCALL(hook_GModDataPack_AddOrUpdateFile, AddOrUpdateFile), m_pID
	);

	Detour::Create(
		&detour_GModDataPack_SendFileToClient, "GModDataPack::SendFileToClient",
		server_loader.GetModule(), Symbols::GModDataPack_SendFileToClientSym,
		(void*)DETOUR_THISCALL(hook_GModDataPack_SendFileToClient, SendFileToClient), m_pID
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
		Util::AddFunc(pLua, gmoddatapack_GetStoredCode, "GetStoredCode");
		Util::AddFunc(pLua, gmoddatapack_GetCompressedSize, "GetCompressedSize");
		Util::AddFunc(pLua, gmoddatapack_MarkAsTokenizeThread, "MarkAsTokenizeThread");
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