#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include <netmessages.h>
#include "sourcesdk/baseclient.h"
#include "steam/isteamclient.h"
#include <isteamutils.h>
#include "unordered_set"

class CVoiceChatModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual void Think(bool bSimulating) OVERRIDE;
	virtual const char* Name() { return "voicechat"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32; };
};

static ConVar voicechat_hooks("holylib_voicechat_hooks", "1", 0);

static IThreadPool* pVoiceThreadPool = NULL;
static void OnVoiceThreadsChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	pVoiceThreadPool->ExecuteAll();
	pVoiceThreadPool->Stop();
	Util::StartThreadPool(pVoiceThreadPool, ((ConVar*)convar)->GetInt());
}

static ConVar voicechat_threads("holylib_voicechat_threads", "1", 0, "The number of threads to use for voicechat.LoadVoiceStream and voicechat.SaveVoiceStream if you specify async", OnVoiceThreadsChange);

static CVoiceChatModule g_pVoiceChatModule;
IModule* pVoiceChatModule = &g_pVoiceChatModule;

struct VoiceData
{
	~VoiceData() {
		if (pData)
			delete[] pData;
	}

	inline void AllocData()
	{
		if (pData)
			delete[] pData;

		pData = new char[iLength]; // We won't need additional space right?
	}

	inline void SetData(const char* pNewData, int iNewLength)
	{
		iLength = iNewLength;
		AllocData();
		memcpy(pData, pNewData, iLength);
	}

	inline VoiceData* CreateCopy()
	{
		VoiceData* data = new VoiceData;
		data->bProximity = bProximity;
		data->iPlayerSlot = iPlayerSlot;
		data->SetData(pData, iLength);

		return data;
	}

	int iPlayerSlot = 0; // What if it's an invalid one ;D (It doesn't care.......)
	char* pData = NULL;
	int iLength = 0;
	bool bProximity = true;
};

static int VoiceData_TypeID = -1;
Push_LuaClass(VoiceData, VoiceData_TypeID)
Get_LuaClass(VoiceData, VoiceData_TypeID, "VoiceData")

LUA_FUNCTION_STATIC(VoiceData__tostring)
{
	VoiceData* pData = Get_VoiceData(1, false);
	if (!pData)
	{
		LUA->PushString("VoiceData [NULL]");
		return 1;
	}

	char szBuf[64] = {};
	V_snprintf(szBuf, sizeof(szBuf), "VoiceData [%i][%i]", pData->iPlayerSlot, pData->iLength);
	LUA->PushString(szBuf);
	return 1;
}

Default__index(VoiceData);
Default__newindex(VoiceData);
Default__GetTable(VoiceData);
Default__gc(VoiceData,
	VoiceData* pVoiceData = (VoiceData*)pData->GetData();
	if (pVoiceData)
		delete pVoiceData;
)

LUA_FUNCTION_STATIC(VoiceData_IsValid)
{
	VoiceData* pData = Get_VoiceData(1, false);

	LUA->PushBool(pData != nullptr);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetPlayerSlot)
{
	VoiceData* pData = Get_VoiceData(1, true);

	LUA->PushNumber(pData->iPlayerSlot);

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetLength)
{
	VoiceData* pData = Get_VoiceData(1, true);

	LUA->PushNumber(pData->iLength);

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetData)
{
	VoiceData* pData = Get_VoiceData(1, true);

	LUA->PushString(pData->pData, pData->iLength);

	return 1;
}

static ISteamUser* g_pSteamUser;
LUA_FUNCTION_STATIC(VoiceData_GetUncompressedData)
{
	VoiceData* pData = Get_VoiceData(1, true);
	int iSize = (int)LUA->CheckNumberOpt(2, 20000); // How many bytes to allocate for the decompressed version. 20000 is default

	if (!g_pSteamUser)
		LUA->ThrowError("Failed to get SteamUser!\n");

	uint32 pDecompressedLength;
	char* pDecompressed = new char[iSize];
	g_pSteamUser->DecompressVoice(
		pData->pData, pData->iLength,
		pDecompressed, iSize,
		&pDecompressedLength, 44100
	);

	LUA->PushString(pDecompressed, pDecompressedLength);
	delete[] pDecompressed; // Lua will already have a copy of it.

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetProximity)
{
	VoiceData* pData = Get_VoiceData(1, true);

	LUA->PushBool(pData->bProximity);

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_SetPlayerSlot)
{
	VoiceData* pData = Get_VoiceData(1, true);

	pData->iPlayerSlot = (int)LUA->CheckNumber(2);

	return 0;
}

LUA_FUNCTION_STATIC(VoiceData_SetLength)
{
	VoiceData* pData = Get_VoiceData(1, true);

	pData->iLength = (int)LUA->CheckNumber(2);

	return 0;
}

LUA_FUNCTION_STATIC(VoiceData_SetData)
{
	VoiceData* pData = Get_VoiceData(1, true);

	const char* pStr = LUA->CheckString(2);
	int iLength = LUA->ObjLen(2);
	if (LUA->IsType(3, GarrysMod::Lua::Type::Number))
	{
		int iNewLength = (int)LUA->GetNumber(3);
		iLength = MIN(iNewLength, iLength); // Don't allow one to go beyond the strength length
	}

	pData->SetData(pStr, iLength);

	return 0;
}

LUA_FUNCTION_STATIC(VoiceData_SetProximity)
{
	VoiceData* pData = Get_VoiceData(1, true);

	pData->bProximity = LUA->GetBool(2);

	return 0;
}

LUA_FUNCTION_STATIC(VoiceData_CreateCopy)
{
	VoiceData* pData = Get_VoiceData(1, true);

	Push_VoiceData(pData->CreateCopy());
	return 1;
}

struct VoiceStream {
	~VoiceStream()
	{
		for (auto& [_, val] : pVoiceData)
			delete val;

		pVoiceData.clear();
	}

	/*
	 * VoiceStream file structure:
	 * 
	 * 4 bytes / int - total count of VoiceData
	 * 
	 * each entry:
	 * 4 bytes / int - tick number
	 * 4 bytes / int - length of data
	 * (length) bytes / bytes - the data
	 */
	void Save(FileHandle_t fh)
	{
		// Create a copy so that the main thread can still party on it.
		std::unordered_map<int, VoiceData*> voiceDataEnties = pVoiceData;

		int count = (int)voiceDataEnties.size();
		g_pFullFileSystem->Write(&count, sizeof(int), fh); // First write the total number of voice data

		for (auto& [tickNumber, voiceData] : voiceDataEnties)
		{
			g_pFullFileSystem->Write(&tickNumber, sizeof(int), fh);

			int length = voiceData->iLength;
			char* data = voiceData->pData;

			g_pFullFileSystem->Write(&length, sizeof(int), fh);
			g_pFullFileSystem->Write(data, length, fh);
		}
	}

	static VoiceStream* Load(FileHandle_t fh)
	{
		VoiceStream* pStream = new VoiceStream;

		int count;
		g_pFullFileSystem->Read(&count, sizeof(int), fh);

		for (int i=0; i<count; ++i)
		{
			int tickNumber;
			g_pFullFileSystem->Read(&tickNumber, sizeof(int), fh);

			int length;
			g_pFullFileSystem->Read(&length, sizeof(int), fh);

			char* data = new char[length];
			g_pFullFileSystem->Read(data, length, fh);

			VoiceData* voiceData = new VoiceData;
			voiceData->iLength = length;
			voiceData->pData = data;

			pStream->SetIndex(tickNumber, voiceData);
		}

		return pStream;
	}

	/*
	 * If you push it to Lua, call ->CreateCopy() on the VoiceData,
	 * we CANT push the VoiceData we store as else the GC will do funnies & crash.
	 */
	inline VoiceData* GetIndex(int index)
	{
		auto it = pVoiceData.find(index);
		if (it == pVoiceData.end())
			return NULL;

		return it->second;
	}

	/*
	 * We assume that the given VoiceData was NEVER pushed to Lua.
	 */
	inline void SetIndex(int index, VoiceData* pData)
	{
		auto it = pVoiceData.find(index);
		if (it != pVoiceData.end())
		{
			delete it->second;
			pVoiceData.erase(it);
		}

		pVoiceData[index] = pData;
	}

	/*
	 * We create a copy of EVERY voiceData.
	 */
	inline void CreateLuaTable(GarrysMod::Lua::ILuaInterface* pLua)
	{
		pLua->PreCreateTable(0, pVoiceData.size());
			for (auto& [tickCount, voiceData] : pVoiceData)
			{
				Push_VoiceData(voiceData->CreateCopy());
				Util::RawSetI(pLua, -2, tickCount);
			}
	}

	inline int GetCount()
	{
		return (int)pVoiceData.size();
	}

private:
	// key = tickcount
	// value = VoiceData
	std::unordered_map<int, VoiceData*> pVoiceData;
};

static int VoiceStream_TypeID = -1;
Push_LuaClass(VoiceStream, VoiceStream_TypeID)
Get_LuaClass(VoiceStream, VoiceStream_TypeID, "VoiceStream")

LUA_FUNCTION_STATIC(VoiceStream__tostring)
{
	VoiceStream* pStream = Get_VoiceStream(1, false);
	if (!pStream)
	{
		LUA->PushString("VoiceStream [NULL]");
		return 1;
	}

	char szBuf[64] = {};
	V_snprintf(szBuf, sizeof(szBuf), "VoiceStream [%i]", pStream->GetCount());
	LUA->PushString(szBuf);
	return 1;
}

Default__index(VoiceStream);
Default__newindex(VoiceStream);
Default__GetTable(VoiceStream);
Default__gc(VoiceStream,
	VoiceStream* pVoiceData = (VoiceStream*)pData->GetData();
	if (pVoiceData)
		delete pVoiceData;
)

LUA_FUNCTION_STATIC(VoiceStream_IsValid)
{
	VoiceStream* pStream = Get_VoiceStream(1, false);

	LUA->PushBool(pStream != nullptr);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceStream_GetData)
{
	VoiceStream* pStream = Get_VoiceStream(1, true);

	pStream->CreateLuaTable(LUA);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceStream_SetData)
{
	VoiceStream* pStream = Get_VoiceStream(1, true);
	LUA->CheckType(2, GarrysMod::Lua::Type::Table);

	LUA->Push(2);
	LUA->PushNil();
	while (LUA->Next(-2))
	{
		// We could remove this, but that would mean that the key could NEVER be 0
		if (!LUA->IsType(-2, GarrysMod::Lua::Type::Number))
		{
			LUA->Pop(1);
			continue;
		}

		int tick = LUA->GetNumber(-2); // key
		VoiceData* data = Get_VoiceData(-1, false); // value

		if (data)
		{
			pStream->SetIndex(tick, data->CreateCopy());
		}

		LUA->Pop(1);
	}
	LUA->Pop(1);
	return 0;
}

LUA_FUNCTION_STATIC(VoiceStream_GetCount)
{
	VoiceStream* pStream = Get_VoiceStream(1, true);

	LUA->PushNumber(pStream->GetCount());
	return 1;
}

LUA_FUNCTION_STATIC(VoiceStream_GetIndex)
{
	VoiceStream* pStream = Get_VoiceStream(1, true);
	int index = (int)LUA->CheckNumber(2);

	VoiceData* data = pStream->GetIndex(index);
	Push_VoiceData(data ? data->CreateCopy() : NULL);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceStream_SetIndex)
{
	VoiceStream* pStream = Get_VoiceStream(1, true);
	int index = (int)LUA->CheckNumber(2);
	VoiceData* pData = Get_VoiceData(3, true);

	pStream->SetIndex(index, pData->CreateCopy());
	return 0;
}

static Detouring::Hook detour_SV_BroadcastVoiceData;
static void hook_SV_BroadcastVoiceData(IClient* pClient, int nBytes, char* data, int64 xuid)
{
	VPROF_BUDGET("HolyLib - SV_BroadcastVoiceData", VPROF_BUDGETGROUP_HOLYLIB);

	if (g_pVoiceChatModule.InDebug())
		Msg("cl: %p\nbytes: %i\ndata: %p\n", pClient, nBytes, data);

	if (!voicechat_hooks.GetBool())
	{
		detour_SV_BroadcastVoiceData.GetTrampoline<Symbols::SV_BroadcastVoiceData>()(pClient, nBytes, data, xuid);
		return;
	}

	if (Lua::PushHook("HolyLib:PreProcessVoiceChat"))
	{
		VoiceData* pVoiceData = new VoiceData;
		pVoiceData->SetData(data, nBytes);
		pVoiceData->iPlayerSlot = pClient->GetPlayerSlot();
		Push_VoiceData(pVoiceData);
		g_Lua->Push(-2);
		g_Lua->Remove(-3);

		// Stack: -2 = VoiceData | -1 = Hook Name (string)

		Util::Push_Entity((CBaseEntity*)Util::GetPlayerByClient((CBaseClient*)pClient));
		g_Lua->Push(-3);

		bool bHandled = false;
		if (g_Lua->CallFunctionProtected(3, 1, true))
		{
			bHandled = g_Lua->GetBool(-1);
			g_Lua->Pop(1);
		}

		LuaUserData* pLuaData = Get_VoiceData_Data(-1, false);
		if (pLuaData)
		{
			delete pLuaData;
			delete pVoiceData;
		}
		g_Lua->SetUserType(-1, NULL);
		g_Lua->Pop(1); // The voice data is still there, so now finally remove it.

		if (bHandled)
			return;
	}

	detour_SV_BroadcastVoiceData.GetTrampoline<Symbols::SV_BroadcastVoiceData>()(pClient, nBytes, data, xuid);
}

LUA_FUNCTION_STATIC(voicechat_SendEmptyData)
{
	CBasePlayer* pPlayer = Util::Get_Player(1, true); // Should error if given invalid player.
	CBaseClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!\n");

	SVC_VoiceData voiceData;
	voiceData.m_nFromClient = (int)LUA->CheckNumberOpt(2, pClient->GetPlayerSlot());
	voiceData.m_nLength = 0;
	voiceData.m_DataOut = NULL; // Will possibly crash?
	voiceData.m_xuid = 0;

	pClient->SendNetMsg(voiceData);

	return 0;
}

LUA_FUNCTION_STATIC(voicechat_SendVoiceData)
{
	CBasePlayer* pPlayer = Util::Get_Player(1, true); // Should error if given invalid player.
	IClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!\n");

	VoiceData* pData = Get_VoiceData(2, true);

	SVC_VoiceData voiceData;
	voiceData.m_nFromClient = pData->iPlayerSlot;
	voiceData.m_nLength = pData->iLength * 8; // In Bits...
	voiceData.m_DataOut = pData->pData;
	voiceData.m_bProximity = pData->bProximity;
	voiceData.m_xuid = 0;

	pClient->SendNetMsg(voiceData);

	return 0;
}

LUA_FUNCTION_STATIC(voicechat_BroadcastVoiceData)
{
	VoiceData* pData = Get_VoiceData(1, true);

	SVC_VoiceData voiceData;
	voiceData.m_nFromClient = pData->iPlayerSlot;
	voiceData.m_nLength = pData->iLength * 8; // In Bits...
	voiceData.m_DataOut = pData->pData;
	voiceData.m_bProximity = pData->bProximity;
	voiceData.m_xuid = 0;

	if (LUA->IsType(2, GarrysMod::Lua::Type::Table))
	{
		LUA->Push(1);
		LUA->PushNil();
		while (LUA->Next(-2))
		{
			CBasePlayer* pPlayer = Util::Get_Player(-1, true);
			CBaseClient* pClient = Util::GetClientByPlayer(pPlayer);
			if (!pClient)
				LUA->ThrowError("Failed to get CBaseClient!\n");

			pClient->SendNetMsg(voiceData);

			LUA->Pop(1);
		}
		LUA->Pop(1);
	} else {
		for(CBaseClient* pClient : Util::GetClients())
			pClient->SendNetMsg(voiceData);
	}

	return 0;
}

LUA_FUNCTION_STATIC(voicechat_ProcessVoiceData)
{
	CBasePlayer* pPlayer = Util::Get_Player(1, true); // Should error if given invalid player.
	CBaseClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!\n");

	VoiceData* pData = Get_VoiceData(2, true);

	if (!DETOUR_ISVALID(detour_SV_BroadcastVoiceData))
		LUA->ThrowError("Missing valid detour for SV_BroadcastVoiceData!\n");

	detour_SV_BroadcastVoiceData.GetTrampoline<Symbols::SV_BroadcastVoiceData>()(
		pClient, pData->iLength, pData->pData, 0
	);

	return 0;
}

LUA_FUNCTION_STATIC(voicechat_CreateVoiceData)
{
	int iPlayerSlot = (int)LUA->CheckNumberOpt(1, 0);
	const char* pStr = LUA->CheckStringOpt(2, NULL);
	int iLength = (int)LUA->CheckNumberOpt(3, NULL);

	VoiceData* pData = new VoiceData;
	pData->iPlayerSlot = iPlayerSlot;

	if (pStr)
	{
		int iStrLength = LUA->ObjLen(2);
		if (iLength && iLength > iStrLength)
			iLength = iStrLength;

		if (!iLength)
			iLength = iStrLength;

		pData->SetData(pStr, iLength);
	}

	Push_VoiceData(pData);

	return 1;
}

LUA_FUNCTION_STATIC(voicechat_IsHearingClient)
{
	CBasePlayer* pPlayer = Util::Get_Player(1, true);
	IClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!\n");

	CBasePlayer* pTargetPlayer = Util::Get_Player(2, true);
	IClient* pTargetClient = Util::GetClientByPlayer(pTargetPlayer);
	if (!pTargetClient)
		LUA->ThrowError("Failed to get CBaseClient for target player!\n");

	LUA->PushBool(pClient->IsHearingClient(pTargetClient->GetPlayerSlot()));

	return 1;
}

LUA_FUNCTION_STATIC(voicechat_IsProximityHearingClient)
{
	CBasePlayer* pPlayer = Util::Get_Player(1, true);
	IClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!\n");

	CBasePlayer* pTargetPlayer = Util::Get_Player(2, true);
	IClient* pTargetClient = Util::GetClientByPlayer(pTargetPlayer);
	if (!pTargetClient)
		LUA->ThrowError("Failed to get CBaseClient for target player!\n");

	LUA->PushBool(pClient->IsProximityHearingClient(pTargetClient->GetPlayerSlot()));

	return 1;
}

LUA_FUNCTION_STATIC(voicechat_CreateVoiceStream)
{
	Push_VoiceStream(new VoiceStream);
	return 1;
}

enum VoiceStreamTaskStatus {
	VoiceStreamTaskStatus_FAILED_FILE_NOT_FOUND = -2,
	VoiceStreamTaskStatus_FAILED_INVALID_TYPE = -1,
	VoiceStreamTaskStatus_NONE = 0,
	VoiceStreamTaskStatus_DONE = 1
};

enum VoiceStreamTaskType {
	VoiceStreamTask_NONE,
	VoiceStreamTask_SAVE,
	VoiceStreamTask_LOAD
};

struct VoiceStreamTask {
	~VoiceStreamTask()
	{
		if (iReference != -1)
		{
			g_Lua->ReferenceFree(iReference);
			iReference = -1;
		}

		if (iCallback != -1)
		{
			g_Lua->ReferenceFree(iCallback);
			iCallback = -1;
		}
	}

	char pFileName[MAX_PATH];
	char pGamePath[MAX_PATH];

	VoiceStreamTaskType iType = VoiceStreamTask_NONE;
	VoiceStreamTaskStatus iStatus = VoiceStreamTaskStatus_NONE;

	VoiceStream* pStream = NULL;
	int iReference = -1; // A reference to the pStream to stop the GC from kicking in.
	int iCallback = -1;
};

static std::unordered_set<VoiceStreamTask*> g_pVoiceStreamTasks; // Won't need a mutex since only the main thread accesses it.
static void VoiceStreamJob(VoiceStreamTask*& task)
{
	switch(task->iType)
	{
		case VoiceStreamTask_LOAD:
		{
			FileHandle_t fh = g_pFullFileSystem->Open(task->pFileName, "rb", task->pGamePath);
			if (fh)
			{
				task->pStream = VoiceStream::Load(fh);
				g_pFullFileSystem->Close(fh);
			} else {
				task->iStatus = VoiceStreamTaskStatus_FAILED_FILE_NOT_FOUND;
			}
			break;
		}
		case VoiceStreamTask_SAVE:
		{
			FileHandle_t fh = g_pFullFileSystem->Open(task->pFileName, "wb", task->pGamePath);
			if (fh)
			{
				task->pStream->Save(fh);
				g_pFullFileSystem->Close(fh);
			} else {
				task->iStatus = VoiceStreamTaskStatus_FAILED_FILE_NOT_FOUND;
			}
			break;
		}
		default:
		{
			Warning("holylib - VoiceChat(VoiceStreamJob): Managed to get a job without a type. How.\n");
			task->iStatus = VoiceStreamTaskStatus_FAILED_INVALID_TYPE;
			return;
		}
	}

	if (task->iStatus == VoiceStreamTaskStatus_NONE) // Wasn't set already? then just set it to done.
	{
		task->iStatus = VoiceStreamTaskStatus_DONE;
	}
}

LUA_FUNCTION_STATIC(voicechat_LoadVoiceStream)
{
	const char* pFileName = LUA->CheckString(1);
	const char* pGamePath = LUA->CheckStringOpt(2, "DATA");
	bool bAsync = LUA->GetBool(3);
	if (bAsync)
	{
		LUA->CheckType(4, GarrysMod::Lua::Type::Function);
	}

	VoiceStreamTask* task = new VoiceStreamTask;
	V_strncpy(task->pFileName, pFileName, sizeof(task->pFileName));
	V_strncpy(task->pGamePath, pGamePath, sizeof(task->pGamePath));
	task->iType = VoiceStreamTask_LOAD;

	if (bAsync)
	{
		LUA->Push(4);
		task->iCallback = Util::ReferenceCreate("voicechat.LoadVoiceStream - callback");
		g_pVoiceStreamTasks.insert(task);
		pVoiceThreadPool->QueueCall(&VoiceStreamJob, task);
		return 0;
	} else {
		VoiceStreamJob(task);
		Push_VoiceStream(task->pStream);
		LUA->PushNumber((int)task->iStatus);
		delete task;
		return 2;
	}
}

LUA_FUNCTION_STATIC(voicechat_SaveVoiceStream)
{
	VoiceStream* pStream = Get_VoiceStream(1, true);
	const char* pFileName = LUA->CheckString(2);
	const char* pGamePath = LUA->CheckStringOpt(3, "DATA");
	bool bAsync = LUA->GetBool(4);
	if (bAsync)
	{
		LUA->CheckType(5, GarrysMod::Lua::Type::Function);
	}

	VoiceStreamTask* task = new VoiceStreamTask;
	V_strncpy(task->pFileName, pFileName, sizeof(task->pFileName));
	V_strncpy(task->pGamePath, pGamePath, sizeof(task->pGamePath));
	task->iType = VoiceStreamTask_SAVE;
	
	LUA->Push(1);
	task->iReference = Util::ReferenceCreate("voicechat.SaveVoiceStream - VoiceStream");
	task->pStream = pStream;

	if (bAsync)
	{
		LUA->Push(5);
		task->iCallback = Util::ReferenceCreate("voicechat.SaveVoiceStream - callback");
		g_pVoiceStreamTasks.insert(task);
		pVoiceThreadPool->QueueCall(&VoiceStreamJob, task);
		return 0;
	} else {
		VoiceStreamJob(task);
		LUA->PushNumber((int)task->iStatus);
		delete task;
		return 1;
	}
}

void CVoiceChatModule::Think(bool bSimulating)
{
	if (g_pVoiceStreamTasks.size() <= 0)
		return;

	for (auto it = g_pVoiceStreamTasks.begin(); it != g_pVoiceStreamTasks.end(); )
	{
		VoiceStreamTask* pTask = *it;
		if (pTask->iStatus == VoiceStreamTaskStatus_NONE)
		{
			it++;
			continue;
		}

		if (pTask->iCallback == -1)
		{
			Warning("holylib - VoiceChat(Think): somehow managed to get a task without a callback!\n");
			if (pTask->pStream != NULL && pTask->iType != VoiceStreamTask_SAVE)
				delete pTask->pStream;

			delete pTask;
			it = g_pVoiceStreamTasks.erase(it);
			continue;
		}

		g_Lua->ReferencePush(pTask->iCallback);
		Push_VoiceStream(pTask->pStream); // Lua GC will take care of deleting.
		g_Lua->PushBool(pTask->iStatus == VoiceStreamTaskStatus_DONE);

		g_Lua->CallFunctionProtected(2, 0, true);
		
		delete pTask;
		it = g_pVoiceStreamTasks.erase(it);
	}
}

void CVoiceChatModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	VoiceData_TypeID = g_Lua->CreateMetaTable("VoiceData");
		Util::AddFunc(VoiceData__tostring, "__tostring");
		Util::AddFunc(VoiceData__index, "__index");
		Util::AddFunc(VoiceData__newindex, "__newindex");
		Util::AddFunc(VoiceData__gc, "__gc");
		Util::AddFunc(VoiceData_GetTable, "GetTable");
		Util::AddFunc(VoiceData_IsValid, "IsValid");
		Util::AddFunc(VoiceData_GetData, "GetData");
		Util::AddFunc(VoiceData_GetLength, "GetLength");
		Util::AddFunc(VoiceData_GetPlayerSlot, "GetPlayerSlot");
		Util::AddFunc(VoiceData_SetData, "SetData");
		Util::AddFunc(VoiceData_SetLength, "SetLength");
		Util::AddFunc(VoiceData_SetPlayerSlot, "SetPlayerSlot");
		Util::AddFunc(VoiceData_GetUncompressedData, "GetUncompressedData");
		Util::AddFunc(VoiceData_GetProximity, "GetProximity");
		Util::AddFunc(VoiceData_SetProximity, "SetProximity");
		Util::AddFunc(VoiceData_CreateCopy, "CreateCopy");
	g_Lua->Pop(1);

	VoiceStream_TypeID = g_Lua->CreateMetaTable("VoiceStream");
		Util::AddFunc(VoiceStream__tostring, "__tostring");
		Util::AddFunc(VoiceStream__index, "__index");
		Util::AddFunc(VoiceStream__newindex, "__newindex");
		Util::AddFunc(VoiceStream__gc, "__gc");
		Util::AddFunc(VoiceStream_GetTable, "GetTable");
		Util::AddFunc(VoiceStream_IsValid, "IsValid");
		Util::AddFunc(VoiceStream_GetData, "GetData");
		Util::AddFunc(VoiceStream_SetData, "SetData");
		Util::AddFunc(VoiceStream_GetCount, "GetCount");
		Util::AddFunc(VoiceStream_GetIndex, "GetIndex");
		Util::AddFunc(VoiceStream_SetIndex, "SetIndex");
	g_Lua->Pop(1);

	Util::StartTable();
		Util::AddFunc(voicechat_SendEmptyData, "SendEmptyData");
		Util::AddFunc(voicechat_SendVoiceData, "SendVoiceData");
		Util::AddFunc(voicechat_BroadcastVoiceData, "BroadcastVoiceData");
		Util::AddFunc(voicechat_ProcessVoiceData, "ProcessVoiceData");
		Util::AddFunc(voicechat_CreateVoiceData, "CreateVoiceData");
		Util::AddFunc(voicechat_IsHearingClient, "IsHearingClient");
		Util::AddFunc(voicechat_IsProximityHearingClient, "IsProximityHearingClient");
		Util::AddFunc(voicechat_CreateVoiceStream, "CreateVoiceStream");
		Util::AddFunc(voicechat_LoadVoiceStream, "LoadVoiceStream");
		Util::AddFunc(voicechat_SaveVoiceStream, "SaveVoiceStream");
	Util::FinishTable("voicechat");
}

void CVoiceChatModule::LuaShutdown()
{
	Util::NukeTable("voicechat");
}

void CVoiceChatModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	SourceSDK::ModuleLoader engine_loader("engine");
	Detour::Create(
		&detour_SV_BroadcastVoiceData, "SV_BroadcastVoiceData",
		engine_loader.GetModule(), Symbols::SV_BroadcastVoiceDataSym,
		(void*)hook_SV_BroadcastVoiceData, m_pID
	);
}

static HSteamPipe hSteamPipe = NULL;
static HSteamUser hSteamUser = NULL;
void CVoiceChatModule::ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
	if (!g_pSteamUser)
	{
		if (SteamUser())
		{
			g_pSteamUser = SteamUser();
			if (g_pVoiceChatModule.InDebug())
				Msg("holylib: SteamUser returned valid stuff?\n");
			return;
		}

		//if (Util::get != NULL)
		//	g_pSteamUser = Util::get->SteamUser();

		ISteamClient* pSteamClient = SteamGameServerClient();

		if (pSteamClient)
		{
			hSteamPipe = pSteamClient->CreateSteamPipe();
			hSteamUser = pSteamClient->CreateLocalUser(&hSteamPipe, k_EAccountTypeAnonUser);
			g_pSteamUser = pSteamClient->GetISteamUser(hSteamUser, hSteamPipe, "SteamUser023");
		}
	}
}

void CVoiceChatModule::Shutdown()
{
	ISteamClient* pSteamClient = SteamGameServerClient();
	if (pSteamClient)
	{
		pSteamClient->ReleaseUser(hSteamPipe, hSteamUser);
		pSteamClient->BReleaseSteamPipe(hSteamPipe);
		hSteamPipe = NULL;
		hSteamUser = NULL;
	}

	g_pSteamUser = NULL;
}