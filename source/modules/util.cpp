#include "LuaInterface.h"
#include "module.h"
#include "lua.h"
#include "Bootil/Bootil.h"
#include <lz4/lz4_compression.h>

#undef isdigit // Fix for 64x
#include <charconv>

#include "bootil/src/3rdParty/rapidjson/rapidjson.h"
#include "bootil/src/3rdParty/rapidjson/document.h"
#include "bootil/src/3rdParty/rapidjson/stringbuffer.h"
#include "bootil/src/3rdParty/rapidjson/prettywriter.h"
#include "bootil/src/3rdParty/rapidjson/writer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CUtilModule : public IModule
{
public:
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void LuaThink(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual const char* Name() { return "util"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
	virtual bool SupportsMultipleLuaStates() { return true; };
};

static CUtilModule g_pUtilModule;
IModule* pUtilModule = &g_pUtilModule;

static IThreadPool* pJsonPool = NULL;
static IThreadPool* pCompressPool = NULL;
static IThreadPool* pDecompressPool = NULL;
static void OnCompressThreadsChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	if (!pCompressPool)
		return;

	pCompressPool->ExecuteAll(); // ToDo: Do we need to do this?
	pCompressPool->Stop();
	Util::StartThreadPool(pCompressPool, ((ConVar*)convar)->GetInt());
}

static void OnDecompressThreadsChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	if (!pDecompressPool)
		return;

	pDecompressPool->ExecuteAll();
	pDecompressPool->Stop();
	Util::StartThreadPool(pDecompressPool, ((ConVar*)convar)->GetInt());
}

static void OnJsonThreadsChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	if (!pJsonPool)
		return;

	pJsonPool->ExecuteAll();
	pJsonPool->Stop();
	Util::StartThreadPool(pJsonPool, ((ConVar*)convar)->GetInt());
}

static ConVar compressthreads("holylib_util_compressthreads", "1", FCVAR_ARCHIVE, "The number of threads to use for util.AsyncCompress", true, 1, true, 16, OnCompressThreadsChange);
static ConVar decompressthreads("holylib_util_decompressthreads", "1", FCVAR_ARCHIVE, "The number of threads to use for util.AsyncDecompress", true, 1, true, 16, OnDecompressThreadsChange);
static ConVar jsonthreads("holylib_util_jsonthreads", "1", FCVAR_ARCHIVE, "The number of threads to use for util.AsyncTableToJSON & util.AsyncJSONToTable", true, 1, true, 16, OnJsonThreadsChange);

class IJobEntry
{
public:
	virtual ~IJobEntry() = default;
	virtual bool OnThink(GarrysMod::Lua::ILuaInterface* pLua) = 0;

	bool m_bCancel = false;
	GarrysMod::Lua::ILuaInterface* m_pLua = NULL;
};

class CompressEntry : public IJobEntry
{
public:
	virtual ~CompressEntry()
	{
		//if (GetCurrentThreadId() != owningThread)
		//{
		//	Warning(PROJECT_NAME ": A CompressEntry was deleted on a random thread! This should never happen!\n");
		//	return; // This will be a memory, but we would never want to potentially break the Lua state.
		//}

		if (iDataReference != -1 && m_pLua)
			Util::ReferenceFree(m_pLua, iDataReference, "CompressEntry::~CompressEntry - Data");

		if (iCallback != -1 && m_pLua)
			Util::ReferenceFree(m_pLua, iCallback, "CompressEntry::~CompressEntry - Callback");
	}

	virtual bool OnThink(GarrysMod::Lua::ILuaInterface* pLua)
	{
		if (iStatus == 0)
			return false;

		if (pLua != m_pLua)
		{
			Error(PROJECT_NAME " - util: Somehow called OnThink for the wrong Lua Interface?!?\n");
		}

		Util::ReferencePush(pLua, iCallback);
		if (iStatus == -1)
		{
			pLua->PushNil();
		} else {
			pLua->PushString((const char*)buffer.GetBase(), buffer.GetWritten());
		}
		pLua->CallFunctionProtected(1, 0, true);

		return true;
	}

	int iCallback = -1;
	bool bCompress = true;
	char iStatus = 0; // -1 = Failed | 0 = Running | 1 = Done

	const char* pData = NULL;
	int iDataReference = -1; // Keeping a reference to stop GC from potentially nuking it.

	int iLength = 0;
	int iLevel = 7;
	int iDictSize = 65536;
	double iRatio = 99.98;
	Bootil::AutoBuffer buffer;
};

class LuaUtilModuleData : public Lua::ModuleData
{
public:
	std::vector<IJobEntry*> pEntries;

	// Lua Table recursive dependencies
	int iRecursiveStartTop = -1;
	bool bRecursiveNoError = false;
	std::vector<int> pRecursiveTableScope;
	char buffer[128];
};

LUA_GetModuleData(LuaUtilModuleData, g_pUtilModule, Util);

static void CompressJob(CompressEntry*& entry)
{
	if (entry->m_bCancel) // No Lua? We stop.
		return;

	if (entry->bCompress) {
		entry->iStatus = Bootil::Compression::LZMA::Compress(entry->pData, entry->iLength, entry->buffer, entry->iLevel, entry->iDictSize) ? 1 : -1;
	} else {
		entry->iStatus = Bootil::Compression::LZMA::Extract(entry->pData, entry->iLength, entry->buffer, entry->iRatio) ? 1 : -1;
	}
}

/*
 * If the Async function's arent used. We simply won't create the threadpools.
 * This should save a bit of CPU usage since we won't have a thread that is permantly in a while loop,
 * calling ThreadSleep every 10 milliseconds or so.
 */
inline void StartThread()
{
	if (pCompressPool)
		return;

	pCompressPool = V_CreateThreadPool();
	Util::StartThreadPool(pCompressPool, compressthreads.GetInt());

	pDecompressPool = V_CreateThreadPool();
	Util::StartThreadPool(pDecompressPool, decompressthreads.GetInt());
}

LUA_FUNCTION_STATIC(util_AsyncCompress)
{
	size_t iLength;
	const char* pData = Util::CheckLString(LUA, 1, &iLength);
	int iLevel = 5;
	int iDictSize = 65536;
	int iCallback = -1;
	if (LUA->IsType(2, GarrysMod::Lua::Type::Function))
	{
		LUA->Push(2);
		iCallback = Util::ReferenceCreate(LUA, "util.AsyncCompress - Callback1");
	} else {
		iLevel = (int)LUA->CheckNumberOpt(2, 5);
		iDictSize = (int)LUA->CheckNumberOpt(3, 65536);
		LUA->CheckType(4, GarrysMod::Lua::Type::Function);
		LUA->Push(4);
		iCallback = Util::ReferenceCreate(LUA, "util.AsyncCompress - Callback2");
	}

	CompressEntry* entry = new CompressEntry;
	entry->iCallback = iCallback;
	entry->iDictSize = iDictSize;
	entry->iLength = iLength;
	entry->iLevel = iLevel;
	entry->pData = pData;
	LUA->Push(1);
	entry->iDataReference = Util::ReferenceCreate(LUA, "util.AsyncCompress - Data");
	entry->m_pLua = LUA;

	GetUtilLuaData(LUA)->pEntries.push_back(entry);

	StartThread();

	pCompressPool->QueueCall(CompressJob, entry);

	return 0;
}

LUA_FUNCTION_STATIC(util_AsyncDecompress)
{
	size_t iLength;
	const char* pData = Util::CheckLString(LUA, 1, &iLength);
	LUA->CheckType(2, GarrysMod::Lua::Type::Function);
	LUA->Push(2);
	int iCallback = Util::ReferenceCreate(LUA, "util.AsyncDecompress - Callback");

	double ratio = LUA->CheckNumberOpt(3, 0.98); // 98% ratio by default

	CompressEntry* entry = new CompressEntry;
	entry->bCompress = false;
	entry->iCallback = iCallback;
	entry->iLength = iLength;
	entry->pData = pData;
	entry->iRatio = ratio;
	LUA->Push(1);
	entry->iDataReference = Util::ReferenceCreate(LUA, "util.AsyncDecompress - Data");
	entry->m_pLua = LUA;

	GetUtilLuaData(LUA)->pEntries.push_back(entry);

	StartThread();

	pDecompressPool->QueueCall(CompressJob, entry);

	return 0;
}

inline bool IsInt(double pNumber)
{
	return static_cast<int>(pNumber) == pNumber && INT32_MAX >= pNumber && pNumber >= INT32_MIN;
}

inline bool StrToIntFast(const char* pStr, size_t iLen, long long& lOut)
{
	auto [pEnd, ec] = std::from_chars(pStr, pStr + iLen, lOut);
	return ec == std::errc() && pEnd == pStr + iLen;
}

extern void TableToJSONRecursive(GarrysMod::Lua::ILuaInterface* pLua, LuaUtilModuleData* pData, rapidjson::Value& outValue, rapidjson::Document::AllocatorType& allocator);
void TableToJSONRecursive(GarrysMod::Lua::ILuaInterface* pLua, LuaUtilModuleData* pData, rapidjson::Value& outValue, rapidjson::Document::AllocatorType& allocator)
{
	bool bEqual = false;
	for (int iReference : pData->pRecursiveTableScope)
	{
		Util::ReferencePush(pLua, iReference);
		if (pLua->Equal(-1, -3))
		{
			bEqual = true;
			pLua->Pop(1);
			break;
		}

		pLua->Pop(1);
	}

	if (bEqual) {
		if (pData->bRecursiveNoError)
		{
			pLua->Pop(2); // Don't forget to cleanup
			return;
		}

		pLua->Pop(pLua->Top() - pData->iRecursiveStartTop); // Since we added a unknown amount to the stack, we need to throw everything back out
		pLua->ThrowError("attempt to serialize structure with cyclic reference");
	}

	pLua->Push(-2);
	pData->pRecursiveTableScope.push_back(Util::ReferenceCreate(pLua, "TableToJSONRecursive - Scope"));

	int idx = 1;
	bool wasSequential = true;
	rapidjson::Value jsonObj(rapidjson::kObjectType);
	rapidjson::Value jsonArr(rapidjson::kArrayType);
	//int tableSize = pLua->ObjLen(-2);
	//if (tableSize > 0)
	//{
	//	jsonArr.Reserve(tableSize * 8, allocator); // * 8 because we assume each key-value will take up atleast 8 bytes.
	//}

	while (pLua->Next(-2)) {
		// In bootil, you just don't give a child a name to indicate that it's sequential. 
		// so we need to support that.
		bool isSequential = false; 
		int iKeyType = pLua->GetType(-2);
		double iKey = iKeyType == GarrysMod::Lua::Type::Number ? pLua->GetNumber(-2) : 0;
		if (iKey != 0 && iKey == idx && wasSequential) // We check for wasSequential since if it becomes false we should NEVER mark isSequential as true again.
		{
			isSequential = true;
			++idx;
		}

		const char* key = NULL; // In JSON a key is ALWAYS a string
		if (!isSequential)
		{
			switch (iKeyType)
			{
				case GarrysMod::Lua::Type::String:
					key = pLua->GetString(-2); // lua_next won't nuke itself since we don't convert the value
					break;
				case GarrysMod::Lua::Type::Number:
					pLua->Push(-2);
					key = pLua->GetString(-1); // lua_next nukes itself when the key isn't an actual string
					pLua->Pop(1);
					break;
				case GarrysMod::Lua::Type::Bool:
					pLua->Push(-2);
					key = pLua->GetString(-1); // lua_next nukes itself when the key isn't an actual string
					pLua->Pop(1);
					break;
				default:
					break;
			}

			if (!key)
			{
				pLua->Pop(1); // Pop the value off the stack for lua_next to work
				continue;
			}
		}

		rapidjson::Value value;
		switch (pLua->GetType(-1))
		{
			case GarrysMod::Lua::Type::String:
				value.SetString(pLua->GetString(-1), pLua->ObjLen(-1), allocator);
				break;
			case GarrysMod::Lua::Type::Number:
				{
					double pNumber = pLua->GetNumber(-1);
					if (isfinite(pNumber)) { // Needed for math.huge
						if (IsInt(pNumber))
							value.SetInt((int)pNumber);
						else
							value.SetDouble(pNumber);
					} else {
						value.SetNull();
					}
				}
				break;
			case GarrysMod::Lua::Type::Bool:
				value.SetBool(pLua->GetBool(-1));
				break;
			case GarrysMod::Lua::Type::Table: // now make it recursive >:D
				pLua->Push(-1);
				pLua->PushNil();
				TableToJSONRecursive(pLua, pData, value, allocator);
				break;
			case GarrysMod::Lua::Type::Vector:
				{
					Vector* vec = Get_Vector(pLua, -1, true);
					if (!vec)
						break;

					int length = snprintf(pData->buffer, sizeof(pData->buffer), "[%.16g %.16g %.16g]", vec->x, vec->y, vec->z); // Do we even need to be this percice?
					value.SetString(pData->buffer, length, allocator);
				}
				break;
			case GarrysMod::Lua::Type::Angle:
				{
					QAngle* ang = Get_QAngle(pLua, -1, true);
					if (!ang)
						break;

					int length = snprintf(pData->buffer, sizeof(pData->buffer), "{%.12g %.12g %.12g}", ang->x, ang->y, ang->z);
					value.SetString(pData->buffer, length, allocator);
				}
				break;
			default:
				break; // We should fallback to nil
		}

		if (wasSequential && !isSequential)
		{
			for (rapidjson::SizeType i=0; i<jsonArr.Size(); ++i)
			{
				char indexKey[32];
				snprintf(indexKey, sizeof(indexKey), "%u", i + 1);
				rapidjson::Value k(indexKey, allocator);
				jsonObj.AddMember(k, jsonArr[i], allocator);
			}
			jsonArr.Clear();
			//jsonArr.~GenericValue();
			//new (&jsonArr) rapidjson::Value(rapidjson::kArrayType);
			//jsonObj.Reserve(tableSize * 8, allocator); // Since we use the jsonObj now, we can instead go in and reserve space there.
			wasSequential = false;
		}

		if (wasSequential && isSequential)
		{
			jsonArr.PushBack(std::move(value), allocator);
		} else {
			rapidjson::Value k(key, allocator);
			jsonObj.AddMember(std::move(k), std::move(value), allocator);
		}

		pLua->Pop(1);
	}

	outValue = std::move(wasSequential ? jsonArr : jsonObj);

	pLua->Pop(1);

	Util::ReferenceFree(pLua, pData->pRecursiveTableScope.back(), "TableToJSONRecursive - Free scope");
	pData->pRecursiveTableScope.pop_back();
}

LUA_FUNCTION_STATIC(util_TableToJSON)
{
	LUA->CheckType(1, GarrysMod::Lua::Type::Table);
	bool bPretty = LUA->GetBool(2);

	auto pData = GetUtilLuaData(LUA);
	pData->bRecursiveNoError = LUA->GetBool(3);

	pData->iRecursiveStartTop = LUA->Top();
	LUA->Push(1);
	LUA->PushNil();

	rapidjson::Document doc;
	TableToJSONRecursive(LUA, pData, doc, doc.GetAllocator());

	rapidjson::StringBuffer buffer;
	if (bPretty)
	{
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		writer.SetIndent( '\t', 1 );
		doc.Accept(writer);
	} else {
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);
	}

	LUA->PushString(buffer.GetString(), buffer.GetLength());
	return 1;
}

// When called, we expect the top of the stack to be the main table that we will be writing into
// nOutsideSet = If true, only one value is expected to be pushed which will be set/inserted into the caller
// nIgnoreConversions = If true, don't convert numeric string keys to numbers
extern void JSONToTableRecursive(GarrysMod::Lua::ILuaInterface* pLua, const rapidjson::Value& jsonValue, bool nOutsideSet = false, bool nIgnoreConversions = false);
void PushJSONValue(GarrysMod::Lua::ILuaInterface* pLua, const rapidjson::Value& jsonValue, bool nIgnoreConversions = false)
{
	if (jsonValue.IsObject() || jsonValue.IsArray()) {
		JSONToTableRecursive(pLua, jsonValue, false, nIgnoreConversions);
	} else if (jsonValue.IsString()) {
		const char* valueStr = jsonValue.GetString();
		size_t iStrLength = jsonValue.GetStringLength();
		if (iStrLength > 2 && valueStr[0] == '[' && valueStr[iStrLength - 1] == ']') {
			Vector vec;
			int nParsed = sscanf(valueStr + 1, "%f %f %f", &vec.x, &vec.y, &vec.z);
			if (nParsed == 3) {
				pLua->PushVector(vec);
			} else {
				pLua->PushString(valueStr, iStrLength);
			}
		} else if (iStrLength > 2 && valueStr[0] == '{' && valueStr[iStrLength - 1] == '}') {
			QAngle ang;
			int nParsed = sscanf(valueStr + 1, "%f %f %f", &ang.x, &ang.y, &ang.z);
			if (nParsed == 3) {
				pLua->PushAngle(ang);
			} else {
				pLua->PushString(valueStr, iStrLength);
			}
		} else {
			pLua->PushString(valueStr, iStrLength);
		}
	} else if (jsonValue.IsNumber()) {
		pLua->PushNumber(jsonValue.GetDouble());
	} else if (jsonValue.IsBool()) {
		pLua->PushBool(jsonValue.GetBool());
	} else {
		pLua->PushNil();
	}
}

void JSONToTableRecursive(GarrysMod::Lua::ILuaInterface* pLua, const rapidjson::Value& jsonValue, bool nOutsideSet, bool nIgnoreConversions)
{
	if (jsonValue.IsObject()) {
		pLua->PreCreateTable(0, jsonValue.MemberCount());
		for (rapidjson::Value::ConstMemberIterator itr = jsonValue.MemberBegin(); itr != jsonValue.MemberEnd(); ++itr) {
			const char* pKeyStr = itr->name.GetString();
			size_t iKeyLen = itr->name.GetStringLength();
			
			if (nIgnoreConversions) {
				pLua->PushString(pKeyStr, iKeyLen);
			} else {
				long long lNum;
				if (StrToIntFast(pKeyStr, iKeyLen, lNum)) {
					pLua->PushNumber((double)lNum);
				} else {
					pLua->PushString(pKeyStr, iKeyLen);
				}
			}

			PushJSONValue(pLua, itr->value, nIgnoreConversions);

			pLua->SetTable(-3);
		}
	} else if (jsonValue.IsArray()) {
		int idx = 0;
		pLua->PreCreateTable(jsonValue.Size(), 0);
		for (rapidjson::SizeType i = 0; i < jsonValue.Size(); ++i)
		{
			JSONToTableRecursive(pLua, jsonValue[i], true, nIgnoreConversions);
			Util::RawSetI(pLua, -2, ++idx);
		}
	} else if (nOutsideSet) { // We got called by above jsonValue.IsArray(), so we expect ONLY ONE value to be pushed.
		PushJSONValue(pLua, jsonValue, nIgnoreConversions);
	}
}

LUA_FUNCTION_STATIC(util_JSONToTable)
{
	const char* jsonString = LUA->CheckString(1);
	bool nIgnoreConversions = LUA->GetBool(2);

	rapidjson::Document doc;
	doc.Parse(jsonString);

	if (doc.HasParseError()) {
		LUA->ThrowError("Invalid JSON string");
		return 0;
	}

	JSONToTableRecursive(LUA, doc, false, nIgnoreConversions);
	return 1;
}

LUA_FUNCTION_STATIC(util_CompressLZ4)
{
	size_t iLength;
	const char* pData = Util::CheckLString(LUA, 1, &iLength);
	int accelerationLevel = (int)LUA->CheckNumberOpt(2, 1);

	void* pDest = NULL;
	unsigned int pDestLen = 0;
	bool bSuccess = COM_Compress_LZ4(pData, iLength, &pDest, &pDestLen, accelerationLevel);
	if (!bSuccess)
	{
		LUA->PushNil();
		return 1;
	}

	LUA->PushString((const char*)pDest, pDestLen);
	free(pDest);
	
	return 1;
}

LUA_FUNCTION_STATIC(util_DecompressLZ4)
{
	size_t iLength;
	const char* pData = Util::CheckLString(LUA, 1, &iLength);

	void* pDest = NULL;
	unsigned int pDestLen = 0;
	bool bSuccess = COM_Decompress_LZ4(pData, iLength, &pDest, &pDestLen);
	if (!bSuccess)
	{
		LUA->PushNil();
		return 1;
	}

	LUA->PushString((const char*)pDest, pDestLen);
	free(pDest);

	return 1;
}

class JsonEntry : public IJobEntry
{
public:
	virtual ~JsonEntry()
	{
		if (m_pLua && m_iReference != -1)
		{
			Util::ReferenceFree(m_pLua, m_iReference, "JsonEntry(value) - util.AsyncJson");
			m_iReference = -1;
		}

		if (m_pLua && m_iCallback != -1)
		{
			Util::ReferenceFree(m_pLua, m_iCallback, "JsonEntry(callback) - util.AsyncJson");
			m_iCallback = -1;
		}

		if (m_pObject)
		{
			RawLua::DestroyTValue(m_pObject);
			m_pObject = nullptr;
		}
	}

	virtual bool OnThink(GarrysMod::Lua::ILuaInterface* pLua)
	{
		if (!m_bIsDone)
			return false;

		if (pLua != m_pLua)
		{
			Error(PROJECT_NAME " - util: Somehow called OnThink for the wrong Lua Interface?!?\n");
		}

		Util::ReferencePush(pLua, m_iCallback);
		pLua->PushString(m_strOut.GetString(), m_strOut.GetLength());
		pLua->CallFunctionProtected(1, 0, true);

		return true;
	}

	int m_iReference = -1;
	TValue* m_pObject = NULL;
	bool m_bPretty = false;

	bool m_bIsDone = false;
	rapidjson::StringBuffer m_strOut;
	int m_iCallback = -1;
};

static void JsonJob(JsonEntry*& entry)
{
	if (entry->m_bCancel)
	{
		RawLua::SetReadOnly(entry->m_pObject, false);
		return;
	}

	GarrysMod::Lua::ILuaInterface* LUA = Lua::CreateInterface();

	LuaUtilModuleData pData;
	pData.bRecursiveNoError = true;

	pData.iRecursiveStartTop = LUA->Top();
	RawLua::PushTValue(LUA->GetState(), entry->m_pObject);
	LUA->PushNil();

	rapidjson::Document doc;
	TableToJSONRecursive(LUA, &pData, doc, doc.GetAllocator());

	if (entry->m_bCancel)
	{
		Lua::DestroyInterface(LUA);
		RawLua::SetReadOnly(entry->m_pObject, false);
		return;
	}

	if (entry->m_bPretty)
	{
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(entry->m_strOut);
		writer.SetIndent('\t', 1);
		doc.Accept(writer);
	} else {
		rapidjson::Writer<rapidjson::StringBuffer> writer(entry->m_strOut);
		doc.Accept(writer);
	}

	Lua::DestroyInterface(LUA);
	RawLua::SetReadOnly(entry->m_pObject, false);

	entry->m_bIsDone = true;
}

inline void StartJsonThread()
{
	if (pJsonPool)
		return;

	pJsonPool = V_CreateThreadPool();
	Util::StartThreadPool(pJsonPool, jsonthreads.GetInt());
}

LUA_FUNCTION_STATIC(util_AsyncTableToJSON)
{
	LUA->CheckType(1, GarrysMod::Lua::Type::Table);
	LUA->CheckType(2, GarrysMod::Lua::Type::Function);
	bool bPretty = LUA->GetBool(3);

	LUA->GetField(GarrysMod::Lua::INDEX_REGISTRY, "HOLYLIB_LUAJIT");
	if (!LUA->IsType(-1, GarrysMod::Lua::Type::Bool))
	{
		LUA->Pop(1);
		LUA->ThrowError("This function is not functional without the luajit module enabled!");
		return 0;
	}
	LUA->Pop(1);

	JsonEntry* entry = new JsonEntry;
	entry->m_bPretty = bPretty;

	LUA->Push(2);
	entry->m_iCallback = LUA->ReferenceCreate();

	LUA->Push(1);
	entry->m_pObject = RawLua::CopyTValue(LUA->GetState(), RawLua::index2adr(LUA->GetState(), -1));
	entry->m_iReference = LUA->ReferenceCreate();
	entry->m_pLua = LUA;

	RawLua::SetReadOnly(entry->m_pObject, true);

	GetUtilLuaData(LUA)->pEntries.push_back(entry);

	StartJsonThread();

	pJsonPool->QueueCall(JsonJob, entry);

	return 0;
}

/*LUA_FUNCTION_STATIC(util_AsyncDecompress)
{
	size_t iLength;
	const char* pData = Util::CheckLString(LUA, 1, &iLength);
	LUA->CheckType(2, GarrysMod::Lua::Type::Function);
	LUA->Push(2);
	int iCallback = Util::ReferenceCreate(LUA, "util.AsyncDecompress - Callback");

	double ratio = LUA->CheckNumberOpt(3, 0.98); // 98% ratio by default

	CompressEntry* entry = new CompressEntry;
	entry->bCompress = false;
	entry->iCallback = iCallback;
	entry->iLength = iLength;
	entry->pData = pData;
	entry->iRatio = ratio;
	LUA->Push(1);
	entry->iDataReference = Util::ReferenceCreate(LUA, "util.AsyncDecompress - Data");
	entry->pLua = LUA;

	StartJsonThread();

	pJsonPool->QueueCall(CompressJob, entry);

	return 0;
}*/

void CUtilModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Lua::GetLuaData(pLua)->SetModuleData(m_pID, new LuaUtilModuleData);

	if (Util::PushTable(pLua, "util"))
	{
		Util::AddFunc(pLua, util_AsyncCompress, "AsyncCompress");
		Util::AddFunc(pLua, util_AsyncDecompress, "AsyncDecompress");
		Util::AddFunc(pLua, util_TableToJSON, "FancyTableToJSON");
		Util::AddFunc(pLua, util_JSONToTable, "FancyJSONToTable");
		Util::AddFunc(pLua, util_CompressLZ4, "CompressLZ4");
		Util::AddFunc(pLua, util_DecompressLZ4, "DecompressLZ4");
		Util::AddFunc(pLua, util_AsyncTableToJSON, "AsyncTableToJSON");
		// Util::AddFunc(pLua, util_AsyncJSONToTable, "AsyncJSONToTable");
		Util::PopTable(pLua);
	}
}

void CUtilModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	auto pData = GetUtilLuaData(pLua);

	for (IJobEntry* entry : pData->pEntries)
	{
		entry->m_bCancel = true;
	}

	if (pCompressPool)
		pCompressPool->ExecuteAll();

	if (pDecompressPool)
		pDecompressPool->ExecuteAll();

	if (pJsonPool)
		pJsonPool->ExecuteAll();

	for (IJobEntry* entry : pData->pEntries)
	{
		delete entry;
	}
	pData->pEntries.clear();

	if (Util::PushTable(pLua, "util"))
	{
		Util::RemoveField(pLua, "AsyncCompress");
		Util::RemoveField(pLua, "AsyncDecompress");
		Util::RemoveField(pLua, "FancyTableToJSON");
		Util::RemoveField(pLua, "FancyJSONToTable");
		Util::RemoveField(pLua, "CompressLZ4");
		Util::RemoveField(pLua, "DecompressLZ4");
		Util::RemoveField(pLua, "AsyncTableToJSON");
		// Util::RemoveField(pLua, "AsyncJSONToTable");
		Util::PopTable(pLua);
	}
}

void CUtilModule::LuaThink(GarrysMod::Lua::ILuaInterface* pLua)
{
	VPROF_BUDGET("HolyLib - CUtilModule::LuaThink", VPROF_BUDGETGROUP_HOLYLIB);

	auto pData = GetUtilLuaData(pLua);
	if (pData->pEntries.size() == 0)
		return;

	for(auto it = pData->pEntries.begin(); it != pData->pEntries.end(); )
	{
		IJobEntry* entry = *it;
		if (entry->OnThink(pLua))
		{
			delete entry;
			it = pData->pEntries.erase(it);
		} else {
			it++;
		}
	}
}

void CUtilModule::Shutdown()
{
	if (pCompressPool)
	{
		Util::DestroyThreadPool(pCompressPool);
		pCompressPool = NULL;
	}

	if (pDecompressPool)
	{
		Util::DestroyThreadPool(pDecompressPool);
		pDecompressPool = NULL;
	}

	if (pJsonPool)
	{
		Util::DestroyThreadPool(pJsonPool);
		pJsonPool = NULL;
	}
}