#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include <sstream>
#include "eiface.h"
#include "sourcesdk/GModDataPack.h"
#include "sourcesdk/iluashared.h"
#include "sourcesdk/baseclient.h"
#include "sourcesdk/net_chan.h"
#include "networkstringtable.h"
#include "picosha2/picosha2.h"
#include <atomic>
#include <deque>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Were doing it the Lumi way, eat all of it
#undef isalnum // 64x loves to shit on this one AGAIN
#undef isalpha // 64x loves to shit on this one AGAIN FUCKING HELL
#undef isspace // 64x SHAT AGAIN! OMFG
#undef isdigit // 64x really wants to be fucking ass

class CGModDataPackModule : public IModule
{
public:
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void LevelShutdown() override;
	void Think(bool bSimulating) override;
	void InitDetour(bool bPreServer) override;
	const char* Name() override { return "gmoddatapack"; };
	int Compatibility() override { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
	bool IsEnabledByDefault() override { return false; }; // ToDo: Figure out what inside of here is behaving very randomly
};

static ConVar gmoddatapack_removeserverif("holylib_gmoddatapack_removeserverif", "0", 0, "If enabled, \"if SERVER then\" code blocks are removed from client files");
static ConVar gmoddatapack_removecomments("holylib_gmoddatapack_removecomments", "0", 0, "If enabled, comments are removed from client files");

static CGModDataPackModule g_pGModDataPackModule;
IModule* pGModDataPackModule = &g_pGModDataPackModule;

#define TK_LIST \
	TK(TK_INVALID) \
	TK(TK_IF) \
	TK(TK_FUNCTION) \
	TK(TK_RETURN) \
	TK(TK_THEN) \
	TK(TK_END) \
	TK(TK_DO) \
	TK(TK_OR) \
	TK(TK_AND) \
	TK(TK_NOT) \
	TK(TK_EQUAL) \
	TK(TK_NOT_EQUAL) \
	TK(TK_GREATER_OR_EQUAL) \
	TK(TK_GREATER) \
	TK(TK_LESS_OR_EQUAL) \
	TK(TK_PARENTHESIS) \
	TK(TK_LESS) \
	TK(TK_ELSE) \
	TK(TK_ELSEIF) \
	TK(TK_COMMENT) \
	TK(TK_STRING) \
	TK(TK_NUMBER) \
	TK(TK_TRUE) \
	TK(TK_FALSE) \
	TK(TK_SOMETHING) \
	TK(TK_ADD) \
	TK(TK_SUB) \
	TK(TK_DIV) \
	TK(TK_MUL) \
	TK(TK_LOCAL) \
	TK(TK_LINEEND) \
	TK(TK_EOF)

enum TokenType
{
#define TK(token) token,
	TK_LIST
#undef TK
};

static const char* g_TokenNames[] =
{
#define TK(token) #token,
	TK_LIST
#undef TK
};

struct Token {
	TokenType type;
	std::string content;
	bool isSpace = false;
};

static inline TokenType KeywordType(const std::string& strWord)
{
	if (strWord == "if") return TK_IF;
	if (strWord == "function") return TK_FUNCTION;
	if (strWord == "return") return TK_RETURN;
	if (strWord == "then") return TK_THEN;
	if (strWord == "end") return TK_END;
	if (strWord == "not") return TK_NOT;
	if (strWord == "or") return TK_OR;
	if (strWord == "and") return TK_AND;
	if (strWord == "do") return TK_DO;
	if (strWord == "elseif") return TK_ELSEIF;
	if (strWord == "else") return TK_ELSE;
	if (strWord == "true") return TK_TRUE;
	if (strWord == "false") return TK_FALSE;
	if (strWord == "local") return TK_LOCAL;
	return TK_SOMETHING;
}

// NOTE: I kinda want to keep this function synconized with HolyScript's Tokenizer
static std::vector<Token> TokenizeContent(const std::string& content)
{
	std::vector<Token> tokens;

	size_t i = 0;
	while (i < content.size())
	{
		unsigned char c = (unsigned char)content[i];
		if (c == '\n')
		{
			tokens.push_back({TK_LINEEND, std::string(1, c), true});
			++i;
			continue;
		}

		if (c == ' ' || c == '\t' || c == '\r')
		{
			tokens.push_back({TK_SOMETHING, std::string(1, c), true});
			++i;
			continue;
		}

		if (c == '(' || c == ')')
		{
			tokens.push_back({TK_PARENTHESIS, std::string(1, c)});
			++i;
			continue;
		}

		if (c == '>' && i+1 < content.size() && content[i+1] == '=')
		{
			tokens.push_back({TK_GREATER_OR_EQUAL, content.substr(i, 2)});
			i += 2;
			continue;
		}

		if (c == '>')
		{
			tokens.push_back({TK_GREATER, std::string(1, c)});
			++i;
			continue;
		}

		if (c == '<' && i+1 < content.size() && content[i+1] == '=')
		{
			tokens.push_back({TK_LESS_OR_EQUAL, content.substr(i, 2)});
			i += 2;
			continue;
		}

		if (c == '<')
		{
			tokens.push_back({TK_LESS, std::string(1, c)});
			++i;
			continue;
		}

		if (i+1 < content.size() && c == '|' && content[i+1] == '|')
		{
			tokens.push_back({TK_OR, content.substr(i, 2)});
			i += 2;
			continue;
		}

		if (i+1 < content.size() && ((c == '!' && content[i+1] == '=') || (c == '~' && content[i+1] == '=')))
		{
			tokens.push_back({TK_NOT_EQUAL, content.substr(i, 2)});
			i += 2;
			continue;
		}

		if (i+1 < content.size() && ((c == '=' && content[i+1] == '=')))
		{
			tokens.push_back({TK_EQUAL, content.substr(i, 2)});
			i += 2;
			continue;
		}

		if (i+1 < content.size() && c == '&' && content[i+1] == '&')
		{
			tokens.push_back({TK_AND, content.substr(i, 2)});
			i += 2;
			continue;
		}

		if (c == '+')
		{
			tokens.push_back({TK_ADD, std::string(1, c)});
			++i;
			continue;
		}

		if (c == '*')
		{
			tokens.push_back({TK_MUL, std::string(1, c)});
			++i;
			continue;
		}

		if (c == '-' && i+1 < content.size() && content[i+1] == '-')
		{
			size_t start = i;
			i += 2;

			if (i+1 < content.size() && content[i] == '[')
			{
				++i;
				int count = 0;
				while (i+1 < content.size() && content[i]=='=')
				{
					++i;
					count++;
				}

				if (content[i] == '[')
				{
					while (i+1 < content.size())
					{
						++i;
						if (content[i] == ']')
						{
							if (count == 0 && i+1 < content.size() && content[i+1] == ']')
								break;

							++i;
							int nextCount = 0;
							while (i+1 < content.size() && content[i]=='=')
							{
								++i;
								nextCount++;
							}

							if (nextCount == count && content[i] == ']')
								break;

							i--;
						}
					}

					if (content[i] == ']') {
						if (count > 0)
							++i; // Skip second ]
						else
							i += 2;
					}
				} else
				{
					while (i < content.size() && content[i] != '\n')
						++i;
				}
			} else
			{
				while (i < content.size() && content[i] != '\n')
					++i;
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
					++i;
			} else
			{
				i += 2;
				while (i+1 < content.size() && !(content[i]=='*' && content[i+1]=='/'))
					++i;

				if (i+1 < content.size())
					i += 2;
			}

			tokens.push_back({TK_COMMENT, content.substr(start, i-start)});
			continue;
		}

		if ((c == '\'' || c == '"') && (i == 0 || (content[i-1] != '\\' || (i >= 2 && content[i-2] == '\\'))))
		{
			int start = i;
			++i;
			while (i < content.size() && content[i] != c)
				i += content[i] == '\\' ? 2 : 1;

			// String
			++i;
			tokens.push_back({TK_STRING, content.substr(start, i-start)});
			continue;
		}

		if (c == '[' && (i == 0 || content[i-1] != '\\') && i+1 < content.size())
		{
			int start = i;
			++i;
			int count = 0;
			while (i+1 < content.size() && content[i]=='=')
			{
				++i;
				count++;
			}

			if (content[i]=='[')
			{
				while (i+1 < content.size())
				{
					++i;
					if (content[i] == ']')
					{
						if (count == 0 && i+1 < content.size() && content[i+1] == ']')
							break;

						++i;
						int nextCount = 0;
						while (i+1 < content.size() && content[i]=='=')
						{
							++i;
							nextCount++;
						}

						if (nextCount == count && content[i] == ']')
							break;

						i--;
					}
				}

				if (content[i] == ']') {
					if (count > 0)
						++i; // Skip second ]
					else
						i += 2;
				}

				// Long String
				tokens.push_back({TK_STRING, content.substr(start, i-start)});
				continue;
			}
			i = start;
		}

		// if it starts with a number, it is a number.
		if (std::isdigit(c))
		{
			size_t start = i;
			while (i < content.size() && std::isdigit((unsigned char)content[i]))
				++i;

			std::string strWord = content.substr(start, i - start);
			tokens.push_back({TK_NUMBER, strWord});
			continue;
		}

		if (std::isalpha(c) || c == '_')
		{
			size_t start = i;
			while (i < content.size() && (std::isalnum((unsigned char)content[i]) || content[i]=='_'))
				++i;

			std::string strWord = content.substr(start, i - start);
			tokens.push_back({KeywordType(strWord), strWord});
			continue;
		}

		// NOTE: We check - and / down here since they may else conflict with -- or //
		if (c == '-')
		{
			tokens.push_back({TK_SUB, std::string(1, c)});
			++i;
			continue;
		}

		if (c == '/')
		{
			tokens.push_back({TK_DIV, std::string(1, c)});
			++i;
			continue;
		}

		if (c == '!')
		{
			tokens.push_back({TK_NOT, std::string(1, c)});
			++i;
			continue;
		}

		tokens.push_back({TK_SOMETHING, std::string(1, c)});
		++i;
	}

	tokens.push_back({TK_EOF, ""});
	return tokens;
}

static bool IsNotTK(const std::vector<Token>& tokens, size_t i)
{
	TokenType tk = tokens[i].type;
	if (tk == TK_NOT) // Backtracking time... yay
	{
		while (i > 0)
		{
			--i;
			if (tokens[i].isSpace)
				continue;

			if (tokens[i].type != TK_NOT)
				break;

			tk = tk == TK_NOT ? TK_INVALID : TK_NOT;
		}
	}

	return tk == TK_NOT;
}

enum ScopeFlags : unsigned char
{
	SCOPE_NONE,
	SCOPE_SERVER,
	SCOPE_CLIENT,
	SCOPE_UNCERTAIN
};

struct ScopeEvalInfo
{
	bool isEmpty = true;
	TokenType evalToken = TokenType::TK_INVALID;
	size_t start;
	size_t end;
	std::vector<ScopeFlags> flags = {ScopeFlags()}; // if we got an or we must check every side
	ScopeFlags operand = ScopeFlags::SCOPE_NONE; // Needed to figure out if an condition could ever be true/false
};

static inline void SetScopeFlag(ScopeFlags& flag, ScopeFlags value)
{
	flag = (flag == ScopeFlags::SCOPE_NONE || flag == value) ? value : ScopeFlags::SCOPE_UNCERTAIN;
}

static ScopeFlags CombineFlags(const std::vector<ScopeFlags>& flags)
{
	ScopeFlags result = ScopeFlags::SCOPE_SERVER;
	for (ScopeFlags nFlags : flags)
	{
		if (nFlags == ScopeFlags::SCOPE_CLIENT)
			return ScopeFlags::SCOPE_CLIENT;

		if (nFlags != ScopeFlags::SCOPE_SERVER)
			result = ScopeFlags::SCOPE_UNCERTAIN;
	}

	return result;
}

static void FoldOperand(ScopeEvalInfo& info)
{
	ScopeFlags& flag = info.flags.back();
	ScopeFlags operand = info.operand;
	info.operand = ScopeFlags::SCOPE_NONE;

	if (flag == ScopeFlags::SCOPE_NONE)
		flag = operand;
	else if (flag == ScopeFlags::SCOPE_SERVER || operand == ScopeFlags::SCOPE_SERVER)
		flag = ScopeFlags::SCOPE_SERVER;
	else if (flag != ScopeFlags::SCOPE_CLIENT || operand != ScopeFlags::SCOPE_CLIENT)
		flag = ScopeFlags::SCOPE_UNCERTAIN;
}

// Input start must be advanced by 1!
// We expect that the start argument was moved to the TK_THEN or TK_DO when were done!
static ScopeFlags CanConditionBeRemoved(const std::vector<Token> &tokens, size_t& start)
{
	std::deque<size_t> pScopeIDs;
	std::vector<ScopeEvalInfo> pScopes;
	pScopes.emplace_back();
	pScopes[0].start = start;
	pScopeIDs.push_back(0);

	size_t lastNonEmpty = 0; // To avoid backtracking! (Now it just is a very small save in backtracking I guess)
	while (start < tokens.size() && tokens[start].type != TK_THEN && tokens[start].type != TK_DO)
	{
		if (!tokens[start].isSpace && tokens[start].type != TK_PARENTHESIS)
			pScopes[pScopeIDs.back()].isEmpty = false;

		ScopeFlags& curFlag = pScopes[pScopeIDs.back()].operand;
		if (tokens[start].type == TK_SOMETHING && tokens[start].content == "SERVER")
			SetScopeFlag(curFlag, !IsNotTK(tokens, lastNonEmpty) ? ScopeFlags::SCOPE_SERVER : ScopeFlags::SCOPE_CLIENT);
		else if (tokens[start].type == TK_SOMETHING && tokens[start].content == "CLIENT")
			SetScopeFlag(curFlag, IsNotTK(tokens, lastNonEmpty) ? ScopeFlags::SCOPE_SERVER : ScopeFlags::SCOPE_CLIENT);
		else if (!tokens[start].isSpace && tokens[start].type != TK_PARENTHESIS && tokens[start].type != TK_NOT &&
			tokens[start].type != TK_AND && tokens[start].type != TK_OR && tokens[start].type != TK_COMMENT)
			curFlag = ScopeFlags::SCOPE_UNCERTAIN; // Annoying... (ToDo: Clean this up to proper evaluate conditions / figure out if their constant & server/client only)

		if (tokens[start].type == TK_AND)
			FoldOperand(pScopes[pScopeIDs.back()]);

		if (tokens[start].type == TK_OR)
		{
			FoldOperand(pScopes[pScopeIDs.back()]);
			pScopes[pScopeIDs.back()].flags.push_back(ScopeFlags::SCOPE_NONE);
		}

		if (tokens[start].type == TK_PARENTHESIS)
		{
			if (tokens[start].content == "(") {
				pScopes.emplace_back();
				pScopeIDs.push_back(pScopes.size()-1);
				pScopes[pScopeIDs.back()].start = start;

				pScopes[pScopeIDs.back()].evalToken = IsNotTK(tokens, lastNonEmpty) ? TK_NOT : TK_INVALID;
			} else {
				size_t prevID = pScopeIDs.back();
				pScopes[prevID].end = start;

				FoldOperand(pScopes[prevID]);
				ScopeFlags wasServer = CombineFlags(pScopes[prevID].flags);
				if (pScopes[prevID].evalToken == TK_NOT && wasServer != ScopeFlags::SCOPE_UNCERTAIN)
					wasServer = wasServer == ScopeFlags::SCOPE_SERVER ? ScopeFlags::SCOPE_CLIENT : ScopeFlags::SCOPE_SERVER;

				pScopeIDs.pop_back();
				if (pScopeIDs.empty())
					return ScopeFlags::SCOPE_NONE;

				SetScopeFlag(pScopes[pScopeIDs.back()].operand, wasServer);
			}
		}

		if (!tokens[start].isSpace)
			lastNonEmpty = start;

		start++;
	}
	pScopes[0].end = start;
	pScopeIDs.pop_back();

	FoldOperand(pScopes[0]);
	ScopeFlags result = CombineFlags(pScopes[0].flags);
	return result == ScopeFlags::SCOPE_UNCERTAIN ? ScopeFlags::SCOPE_NONE : result;
}

static inline void NukeToken(Token& tok)
{
	tok.content = "";
	tok.isSpace = true;
	tok.type = TK_SOMETHING;
}

static inline bool OpensBlock(const Token& tok)
{
	return tok.type == TK_THEN || tok.type == TK_DO || tok.type == TK_FUNCTION || (tok.type == TK_SOMETHING && tok.content == "repeat");
}

static inline bool ClosesBlock(const Token& tok)
{
	return tok.type == TK_END || (tok.type == TK_SOMETHING && tok.content == "until");
}

// The goal is to remove parts of a Lua script without changing the line number (so that errors remain easy to debug!)
static size_t RemoveScoped(size_t i, std::vector<Token>& tokens, TokenType tok, bool& hasLineBreaks, bool& hasElseIf)
{
	int depth = 1;
	++i;
	hasLineBreaks = false; // If it's a one line if -> "if x then x else x end" then we won't restore spaces
	while (i < tokens.size() && depth > 0)
	{
		Token& curTok = tokens[i];
		if (OpensBlock(curTok))
			depth++;
		else if (ClosesBlock(curTok))
		{
			depth--;
			if (tok == TK_ELSEIF && depth <= 0)
				continue;
		}
		else if (curTok.type == TK_LINEEND)
		{
			hasLineBreaks = true;
		}
		else if (curTok.type == TK_COMMENT)
		{
			int lineBreaks = 0;
			for (char c : curTok.content)
				if (c=='\n') ++lineBreaks;

			NukeToken(curTok);
			for (int j=0; j<lineBreaks; ++j)
				curTok.content.append("\n");
		}
		else if (curTok.type == TK_ELSE && depth == 1)
		{
			depth--;
			continue;
		}
		else if (curTok.type == TK_ELSEIF)
		{
			depth--;
			if (depth <= 0)
			{
				hasElseIf = true;
				continue;
			}
		}

		if (!curTok.isSpace)
			NukeToken(curTok);

		++i;
	}

	return i;
}

static inline size_t NukeIfCond(size_t i, std::vector<Token> &tokens, ScopeFlags flag)
{
	while (i < tokens.size() && tokens[i].type != TK_THEN)
		NukeToken(tokens[i++]);

	if (flag == ScopeFlags::SCOPE_SERVER && i < tokens.size())
		NukeToken(tokens[i++]);

	return i;
}

static size_t RemoveServerScoped(size_t start, size_t j, std::vector<Token> &tokens, TokenType tok)
{
	bool hasLineBreaks = false;
	bool hasElseIf = false;
	size_t i = RemoveScoped(j, tokens, tok, hasLineBreaks, hasElseIf);

	size_t pos = tokens.size();
	NukeIfCond(start, tokens, ScopeFlags::SCOPE_SERVER);
	i = i - (pos - tokens.size());

	if (i >= tokens.size())
		return i;

	if (tokens[i].type == TK_ELSEIF && tok == TK_IF)
	{
		tokens[i].type = TK_IF;
		tokens[i].content = "if";
	} else if (tokens[i].type == TK_ELSE && tok == TK_IF)
	{
		tokens[i].type = TK_DO;
		tokens[i].content = "do";
	}

	// If we had for example "elseif xx then" we want to restore the space before it.
	while (hasLineBreaks && i-1 > 0 && tokens[i-1].isSpace && tokens[i-1].type != TK_LINEEND)
		i--;

	return i;
}

static size_t FindBlockClose(const std::vector<Token>& tokens, size_t i)
{
	int depth = 1;
	for (++i; i < tokens.size(); ++i)
	{
		if (OpensBlock(tokens[i]))
			depth++;
		else if (ClosesBlock(tokens[i]) && --depth <= 0)
			return i;
		else if (tokens[i].type == TK_ELSE && depth == 1)
			return i;
		else if (tokens[i].type == TK_ELSEIF && --depth <= 0)
			return i;
	}

	return tokens.size();
}

static bool AlwaysReturns(const std::vector<Token>& tokens, size_t start, size_t end)
{
	int depth = 0;
	for (size_t i = start; i < end; ++i)
	{
		if (OpensBlock(tokens[i]))
			depth++;
		else if (ClosesBlock(tokens[i]) || tokens[i].type == TK_ELSEIF)
			depth--;
		else if (tokens[i].type == TK_RETURN && depth == 0)
			return true;
	}

	return false;
}

static void RemoveClientScoped(size_t start, size_t j, std::vector<Token> &tokens, TokenType tok)
{
	size_t close = FindBlockClose(tokens, j);
	if (close >= tokens.size() || tokens[close].type == TK_ELSEIF)
		return;

	bool isElseIf = tok == TK_ELSEIF;
	bool returns = !isElseIf && AlwaysReturns(tokens, j + 1, close);
	NukeIfCond(start, tokens, ScopeFlags::SCOPE_CLIENT);
	tokens[j].content = isElseIf ? "else" : "do";

	bool hasLineBreaks = false;
	bool hasElseIf = false;
	size_t end = close;
	if (tokens[close].type == TK_ELSE)
	{
		NukeToken(tokens[close]);
		end = RemoveScoped(close, tokens, TokenType::TK_ELSEIF, hasLineBreaks, hasElseIf);
	}

	if (returns)
		RemoveScoped(end, tokens, TokenType::TK_ELSEIF, hasLineBreaks, hasElseIf);
}

std::string ProcessTokens(std::vector<Token> &tokens, bool bRemoveServerCode, bool bRemoveComments)
{
	std::stringstream ss;
	size_t i = 0;
	while (i < tokens.size())
	{
		Token &tok = tokens[i];
		if (bRemoveComments && tok.type == TK_COMMENT)
		{
			int lineBreaks = 0;
			for (char c : tok.content)
				if (c=='\n') ++lineBreaks;

			tok.content = "";
			for (int j=0; j<lineBreaks; ++j)
				tok.content.append("\n");

			++i;
			continue;
		}

		if (bRemoveServerCode && (tok.type == TK_IF || tok.type == TK_ELSEIF))
		{
			size_t j = i+1;
			ScopeFlags flag = CanConditionBeRemoved(tokens, j);
			if (flag == ScopeFlags::SCOPE_SERVER)
			{
				if (j < tokens.size() && tokens[j].type == TK_THEN)
				{
					i = RemoveServerScoped(i, j, tokens, tok.type);
					continue;
				}
			} else if (flag == ScopeFlags::SCOPE_CLIENT && j < tokens.size() && tokens[j].type == TK_THEN) // A "do" inside the condition would make NukeIfCond run past it
				RemoveClientScoped(i, j, tokens, tok.type);
		}

		++i;
	}

	// Let's nuke empty lines, it always annoyed me that we keep spaces of nuked lines
	// this also removes any trailing spaces from any line
	size_t lineStart = 0;
	for (size_t i=0; i<tokens.size(); ++i)
	{
		Token& tok = tokens[i];
		if (tok.type == TK_LINEEND)
		{
			if (lineStart < tokens.size())
			{
				for (size_t j=lineStart; j<i; ++j)
					NukeToken(tokens[j]);
			}

			lineStart = i+1;
			continue;
		}

		if (tok.isSpace || tok.content.empty())
		{
			if (lineStart >= tokens.size())
				lineStart = i;
		} else
			lineStart = tokens.size()+1;
	}

	// Lets also turn 4 spaces to a tab since it should save some size too (no one reads the networked client scripts anyways)
	size_t spaceRun = 0;
	for (size_t i=0; i<tokens.size(); ++i)
	{
		if (tokens[i].content != " ")
		{
			spaceRun = 0;
			continue;
		}

		if (++spaceRun < 4)
			continue;

		tokens[i-3].content = "\t";
		for (size_t j=i-2; j<=i; ++j)
			tokens[j].content = "";

		spaceRun = 0;
	}

	// I dislike this way a bit but it allows us to patch tokens from the past :)
	// And we can nuke empty lines at the end
	for (size_t endPos = tokens.size()-1; endPos > 0; --endPos)
	{
		if (!tokens[endPos].isSpace && !tokens[endPos].content.empty() && tokens[endPos].type != TokenType::TK_EOF)
		{
			if (endPos+1 < tokens.size())
				tokens[endPos+1].type = TokenType::TK_EOF;

			break;
		}
	}

	for (auto& tok : tokens)
	{
		if (tok.type == TokenType::TK_EOF)
			break;

		ss << tok.content;
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

		Lua::RawSetI(LUA, -2, ++idx);
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

		inline bool IsContentReady()
		{
			return processed;
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
			hasSourceContent = false;
			sourceContent = "";
			content = "";
			compressed.Clear();
			processed = false;
			removeServerCode = false;
			removeComments = false;
		}

		std::string sourceContent = "";
		std::string content = "";
		Bootil::AutoBuffer compressed;
		std::shared_mutex mutex; // Per entry instead of a global mutex to avoid blocking the main thread for other entries while compressing
		bool hasSourceContent = false;
		bool processed = false;
		bool removeServerCode = false;
		bool removeComments = false;
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

		LuaPackEntry* pPackEntry = GetPackEntry(fileID);
		if (!pPackEntry)
		{
			Warning(PROJECT_NAME " - gmoddatapack: fileID %i for \"%s\" is out of range of our cache!\n", fileID, fileName.c_str());
			return;
		}

		LuaPackEntry& pEntry = *pPackEntry;
		std::lock_guard<std::shared_mutex> lock(pEntry.mutex);
		bool bRemoveServerCode = gmoddatapack_removeserverif.GetBool();
		bool bRemoveComments = gmoddatapack_removecomments.GetBool();
		bool bSameSource = pEntry.hasSourceContent && pEntry.sourceContent == content;
		bool bSameProcessConfig = pEntry.removeServerCode == bRemoveServerCode && pEntry.removeComments == bRemoveComments;
		if (bSameSource && pEntry.IsContentReady() && bSameProcessConfig) // Nothing changed
		{
			std::vector<unsigned char> pHash = HashString(pEntry.content.c_str(), pEntry.content.length() + 1);
			g_pDataPack->m_pClientLuaFiles->SetStringUserData(fileID, pHash.size(), pHash.data());

			return;
		}

		pEntry.compressed.Clear();
		pEntry.hasSourceContent = true;
		pEntry.sourceContent = content;
		pEntry.content = content;
		pEntry.removeServerCode = bRemoveServerCode;
		pEntry.removeComments = bRemoveComments;
		pEntry.processed = false;

		if (g_pGModDataPackModule.InDebug())
			Msg(PROJECT_NAME " - gmoddatapack: Added fileID %i into compression queue!\n", fileID);

		std::lock_guard<std::mutex> queueLock(m_pCompressQueueMutex);
		m_pCompressQueue.push_back(fileID);
	}

	// We only strip it, if it's valid lua (yes my token stuff is highly sensitive!)
	std::string StripContent(std::string content, bool* bError, int fileID, bool bRemoveServerCode, bool bRemoveComments)
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
			Lua::StateAccess pScope(LUA);
			if (pScope.IsValid() && Lua::PushHook("HolyLib:OnTokenizeContent", LUA))
			{
				CallLuaTokenizeContent(LUA, tokens, fileID, true);
			}
		}

		return ProcessTokens(tokens, bRemoveServerCode, bRemoveComments);
	}

	void ProcessContent(LuaPackEntry* pEntry, int fileID)
	{
		bool bError;
		std::string strippedContent = StripContent(pEntry->content, &bError, fileID, pEntry->removeServerCode, pEntry->removeComments);
		if (bError)
		{
			Warning(PROJECT_NAME " - gmoddatapack: Failed to strip fileID \"%i\" due to lua errors! (%s)", fileID, strippedContent.c_str());
		} else {
			pEntry->content = strippedContent;
		}
		pEntry->processed = true;

		if (ThreadInMainThread()) // SetStringUserData is NOT thread safe!
		{
			std::vector<unsigned char> pHash = HashString(pEntry->content.c_str(), pEntry->content.length() + 1);
			g_pDataPack->m_pClientLuaFiles->SetStringUserData(fileID, pHash.size(), pHash.data());
		} else {
			std::lock_guard<std::mutex> lock(m_pStringTableUpdateQueueMutex);
			m_pStringTableUpdateQueue.push_back(fileID);
		}
	}

	bool CompressFile(LuaPackEntry* pEntry, int fileID)
	{
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

public:
	static constexpr int MAX_LUA_FILES = 1 << 13;
	LuaPackEntry m_pLuaFileCache[MAX_LUA_FILES];

	std::vector<int> m_pCompressQueue;
	std::mutex m_pCompressQueueMutex;

	std::vector<int> m_pStringTableUpdateQueue;
	std::mutex m_pStringTableUpdateQueueMutex;

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

		// First pass
		// We do this first to have less overhead by being able to faster call from this thread to the main thread
		if (pWorkEntires.empty())
		{
			if (g_pGModDataPackModule.InDebug() >= 2)
				Msg(PROJECT_NAME " - gmoddatapack: Worker thread got no work!\n");

			continue;
		}

		{
			// We lock the lua state if we have one set.
			// We mainly split this into two passes to avoid blocking the main thread for long!
			Lua::ScopedThreadAccess pThreadScope;
			
			// Let's try to avoid an deadlock, we check here due to the main thread possibly shutting down so we cannot expect the StateAccess below to work.
			if (g_pLuaDataPack.m_pWorkerThreadState.load() != ThreadState::STATE_RUNNING)
				break;

			auto LUA = g_pLuaDataPack.pInterface.GetLua();
			Lua::StateAccess pState(LUA);
			for (int fileID : pWorkEntires)
			{
				if (g_pLuaDataPack.m_pWorkerThreadState.load() != ThreadState::STATE_RUNNING)
					break;

				LuaDataPack::LuaPackEntry* pEntry = g_pLuaDataPack.GetPackEntry(fileID);
				if (!pEntry)
					continue;

				std::lock_guard<std::shared_mutex> lock(pEntry->mutex);
				if (pEntry->IsContentReady()) // Already done? Either we did it, or the main thread.
					continue;

				g_pLuaDataPack.ProcessContent(pEntry, fileID);
			}
		}

		// Second pass
		for (int fileID : pWorkEntires)
		{
			if (g_pLuaDataPack.m_pWorkerThreadState.load() != ThreadState::STATE_RUNNING)
				break;

			LuaDataPack::LuaPackEntry* pEntry = g_pLuaDataPack.GetPackEntry(fileID);
			if (!pEntry)
				continue;

			std::lock_guard<std::shared_mutex> lock(pEntry->mutex);
			if (pEntry->IsReady() || !pEntry->IsContentReady()) // Already done? Either we did it, or the main thread.
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
	g_pLuaDataPack.pInterface.InvalidateInterface();
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

static void SendLuaFile(int clientIdx, int fileID, LuaDataPack::LuaPackEntry* pEntry)
{
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
	if (!pEntry->IsContentReady())
		g_pLuaDataPack.ProcessContent(pEntry, fileID);

	if (pEntry->IsContentReady() && g_pLuaDataPack.CompressFile(pEntry, fileID))
	{
		SendLuaFile(clientIdx, fileID, pEntry);
		return;
	}
}


LUA_FUNCTION_STATIC(gmoddatapack_StripCode)
{
	size_t nLength;
	const char* pContent = Lua::CheckLString(LUA, 1, &nLength);
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
		if (LUA->IsType(4, GarrysMod::Lua::Type::Function))
		{
			LUA->Push(4);
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

static double g_nLastSend = 0;
void CGModDataPackModule::Think(bool bSimulating)
{
	// We do this since SetStringUserData isn't thread safe and may crash in very rare race conditions
	std::lock_guard<std::mutex> lock(g_pLuaDataPack.m_pStringTableUpdateQueueMutex);
	for (int fileID : g_pLuaDataPack.m_pStringTableUpdateQueue)
	{
		LuaDataPack::LuaPackEntry* pEntry = g_pLuaDataPack.GetPackEntry(fileID);
		if (!pEntry)
			continue;

		std::lock_guard<std::shared_mutex> entryLock(pEntry->mutex);

		std::vector<unsigned char> pHash = HashString(pEntry->content.c_str(), pEntry->content.length() + 1);
		g_pDataPack->m_pClientLuaFiles->SetStringUserData(fileID, pHash.size(), pHash.data());
	}

	g_pLuaDataPack.m_pStringTableUpdateQueue.clear();
}

/*
RaphaelIT7:
This is no longer needed as like Gmod we just flood the reliable stream
But I still feel like GMod should've been doing something like this :/
bool GMODDataPack_SetSignOnState(CBaseClient* cl, int state)
{
	int slot = cl->GetPlayerSlot();
	if (slot < 0 || slot >= ABSOLUTE_PLAYER_LIMIT)
		return false;

	if (state != SIGNONSTATE_PRESPAWN)
		return false;

	return !g_pLuaDataPack.m_pPlayerQueue[slot].pQueue.empty();
}*/

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

		for (size_t i=0; i<(size_t)TK_EOF; ++i)
			Util::AddValue(pLua, (double)i, g_TokenNames[i]);

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