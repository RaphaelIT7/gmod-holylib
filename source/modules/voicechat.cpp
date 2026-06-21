#include "opus/opus_framedecoder.h"
#include "opus/steam_voice.h"
#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include <netmessages.h>
#include "sourcesdk/baseclient.h"
#include "steam/isteamclient.h"
#include <isteamutils.h>
#include "server.h"
#include "ivoiceserver.h"
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <deque>
#define private public // Try me.
#include "shareddefs.h"
#include "voice_gamemgr.h"
#undef private

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#include <recipientfilter.h>

class CVoiceChatModule : public IModule
{
public:
	void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) override;
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax) override;
	void LevelShutdown() override;
	void Shutdown() override;
	void InitDetour(bool bPreServer) override;
	void LuaThink(GarrysMod::Lua::ILuaInterface* pLua) override;
	void PreLuaModuleLoaded(lua_State* L, const char* pFileName) override;
	void PostLuaModuleLoaded(lua_State* L, const char* pFileName) override;
	void ClientDisconnect(edict_t* pClient) override;
	const char* Name() override { return "voicechat"; };
	int Compatibility() override { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
	bool SupportsMultipleLuaStates() override { return true; };
};

static ConVar voicechat_hooks("holylib_voicechat_hooks", "1", 0);

static constexpr int g_pDataBufferSize = 16384; // Used to decompress the data. 
static constexpr int g_nCompressedSize = g_pDataBufferSize / 4; // Used as char[] to stackallocate compressed buffers when compressing which later on are moved to the heap
// A buffer so that we reduce allocations. You should only use it directly in one function and NOT pass it around as other functions might override its contents!
static thread_local std::unique_ptr<char[]> g_pDataBuffer(new char[g_pDataBufferSize]); // We cannot just use it normally since else we run out of TBL space (Error: cannot allocate memory in static TLS block)
static thread_local SteamOpus::Opus_FrameDecoder g_pOpusDecoder;
static uint64_t fakeSteamID = 0x0110000100000001; // STEAM_0:1:0
// static inline void ClearDataBuffer() { memset(g_pDataBuffer, 0, g_pDefaultDecompressedSize); }

static IThreadPool* pVoiceThreadPool = nullptr;
static void OnVoiceThreadsChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	if (!pVoiceThreadPool)
		return;

	pVoiceThreadPool->ExecuteAll();
	pVoiceThreadPool->Stop();
	Util::StartThreadPool(pVoiceThreadPool, ((ConVar*)convar)->GetInt());
}

static ConVar voicechat_threads("holylib_voicechat_threads", "4", FCVAR_ARCHIVE, "The number of threads to use for voicechat.LoadVoiceStream and voicechat.SaveVoiceStream if you specify async", OnVoiceThreadsChange);
static ConVar voicechat_savedecompressed("holylib_voicechat_savedecompressed", "1", FCVAR_ARCHIVE, "If enabled the VoiceData will store the decompressed data improving quality though increasing memory usage when applying multiple effects since it no longer needs to decompress the data each time");

static CVoiceChatModule g_pVoiceChatModule;
IModule* pVoiceChatModule = &g_pVoiceChatModule;

// IMPORTANT: When changing this struct, you also NEED to update the VoiceDataFFI.lua file!!!
struct VoiceData
{
	VoiceData()
	{
		bProximity = true;
		bDecompressedChanged = false;
		bAllowLuaGC = true;
		bTempValue = false;
	}

	~VoiceData() {
		Empty();
	}

	inline void AllocData()
	{
		if (pData)
		{
			delete[] pData;
			pData = nullptr;
		}

		if (iLength > 0)
			pData = new char[iLength]; // We won't need additional space right?
	}

	inline void SetData(const char* pNewData, uint16_t iNewLength)
	{
		iLength = iDataLength = iNewLength;
		AllocData();
		if (pData)
			memcpy(pData, pNewData, iLength);
	}

	inline void SetDataDirect(char* pNewData)
	{
		pData = pNewData;
	}

	inline VoiceData* CreateCopy()
	{
		VoiceData* data = new VoiceData;
		data->bProximity = bProximity;
		data->iPlayerSlot = iPlayerSlot;
		if (pData)
			data->SetData(pData, iLength);

		if (pDecompressedData)
			data->SetDecompressedData(pDecompressedData, iDecompressedLength);

		return data;
	}

	// We store the Decompressed data too since when for example applying effect decompressing & compressing destroys quality.
	inline void AllocDecompressedData()
	{
		if (pDecompressedData)
		{
			delete[] pDecompressedData;
			pDecompressedData = nullptr;
		}

		if (iDecompressedLength > 0)
			pDecompressedData = new char[iDecompressedLength]; // We won't need additional space right?
	}

	// This does NOT use the g_pDataBuffer
	// pCodec lets the live FX path supply a per-player codec instead of the shared thread_local
	// one (so concurrent talkers don't share opus encoder/seq state). nullptr -> shared codec.
	inline void SetDecompressedData(const char* pNewData, int iNewLength, IVoiceCodec* pCodec = nullptr)
	{
		IVoiceCodec* codec = pCodec ? pCodec : &g_pOpusDecoder;
		if (!voicechat_savedecompressed.GetBool())
		{
			char pCompressed[g_nCompressedSize];
			int bytes = SteamVoice::CompressIntoBuffer(
				fakeSteamID, codec,
				pNewData, iNewLength,
				pCompressed, sizeof(pCompressed),
				SAMPLERATE_GMOD_OPUS
			);

			if (bytes != -1)
				SetData(pCompressed, bytes);
			else {
				if (g_pVoiceChatModule.InDebug() == 1)
				{
					Msg(PROJECT_NAME " - voicechat: VoiceData::SetDecompressedData failed to compress into buffer! (%p, %i)\n", pNewData, iNewLength);
				}
			}
			return;
		}

		iDecompressedLength = iNewLength;
		if (iDecompressedLength > g_pDataBufferSize)
			iDecompressedLength = g_pDataBufferSize;

		AllocDecompressedData();
		if (pDecompressedData)
			memcpy(pDecompressedData, pNewData, iDecompressedLength);

		bDecompressedChanged = true;
	}

	inline char* GetData(IVoiceCodec* pCodec = nullptr)
	{
		if ((bDecompressedChanged || !pData) && pDecompressedData)
		{
			IVoiceCodec* codec = pCodec ? pCodec : &g_pOpusDecoder;
			char pCompressed[g_nCompressedSize];
			int bytes = SteamVoice::CompressIntoBuffer(
				fakeSteamID, codec,
				pDecompressedData, iDecompressedLength,
				pCompressed, sizeof(pCompressed),
				SAMPLERATE_GMOD_OPUS
			);

			if (bytes == -1)
			{
				if (g_pVoiceChatModule.InDebug() == 1)
				{
					Msg(PROJECT_NAME " - voicechat: Failed to compress data in VoiceData::GetData! (%p, %i)\n", pDecompressedData, iDecompressedLength);
				}

				return pData; // We failed to update. GG
			}

			// Compressed they are like 3kb soo 64kb should never be hit
			if (bytes > USHRT_MAX)
			{
				Warning(PROJECT_NAME " - voicechat: Compressed voice data is too large! (%i)\n", bytes);
				return pData;
			}

			SetData(pCompressed, bytes);
			bDecompressedChanged = false;
		}

		return pData;
	}

	// If you call this expect g_pDataBuffer to be changed.
	// pCodec lets the live FX path supply a per-player codec (see SetDecompressedData).
	inline char* GetDecompressedData(int* pLength, IVoiceCodec* pCodec = nullptr)
	{
		if (pDecompressedData)
		{
			*pLength = iDecompressedLength;
			return pDecompressedData;
		}

		if (!pData)
		{
			*pLength = 0;
			return nullptr;
		}

		int bytes = SteamVoice::DecompressIntoBuffer(
			pCodec ? pCodec : &g_pOpusDecoder,
			pData, iLength,
			g_pDataBuffer.get(), g_pDataBufferSize
		);

		if (bytes == -1)
		{
			if (g_pVoiceChatModule.InDebug() == 1)
			{
				Msg(PROJECT_NAME " - voicechat: VoiceData::GetDecompressedData failed to decompress into buffer! (%p, %i)\n", pData, iLength);
			}

			*pLength = 0;
			return nullptr;
		}

		if (!voicechat_savedecompressed.GetBool())
		{
			*pLength = bytes;
			return g_pDataBuffer.get();
		}

		SetDecompressedData(g_pDataBuffer.get(), bytes, pCodec);

		*pLength = iDecompressedLength;
		return pDecompressedData;
	}

	inline void SetLength(uint16_t iNewLength)
	{
		iLength = MIN(iDataLength, iNewLength);
	}

	inline int GetLength(IVoiceCodec* pCodec = nullptr)
	{
		if (bDecompressedChanged && pDecompressedData)
		{
			IVoiceCodec* codec = pCodec ? pCodec : &g_pOpusDecoder;
			char pCompressed[g_nCompressedSize];
			int bytes = SteamVoice::CompressIntoBuffer(
				fakeSteamID, codec,
				pDecompressedData, iDecompressedLength,
				pCompressed, sizeof(pCompressed),
				SAMPLERATE_GMOD_OPUS
			);

			if (bytes == -1)
			{
				if (g_pVoiceChatModule.InDebug() == 1)
				{
					Msg(PROJECT_NAME " - voicechat: Failed to compress data in VoiceData::GetLength!\n");
				}

				return iLength; // We failed to update. GG
			}

			SetData(pCompressed, bytes);
			bDecompressedChanged = false;
		}

		return iLength;
	}

	inline char* GetRawDecompressedData()
	{
		return pDecompressedData;
	}

	// We mark VoiceData as temp so that code won't possibly store a dead pointer
	inline void MarkTemp()
	{
		bTempValue = true;
	}

	inline bool IsTemp()
	{
		return bTempValue;
	}

	inline void MarkDecompressedChanged()
	{
		bDecompressedChanged = true;
	}

	inline int DecompressIntoBuffer()
	{
		return SteamVoice::DecompressIntoBuffer(
			&g_pOpusDecoder,
			pData, iLength,
			g_pDataBuffer.get(), g_pDataBufferSize
		);
	}

	inline void Empty()
	{
		if (pData)
		{
			delete[] pData;
			pData = nullptr;
		}

		if (pDecompressedData)
		{
			delete[] pDecompressedData;
			pDecompressedData = nullptr;
		}

		iLength = 0;
		iDecompressedLength = 0;
		bDecompressedChanged = false;
	}

	uint8_t iPlayerSlot = 0; // What if it's an invalid one ;D (It doesn't care.......)
	unsigned char bProximity : 1;
	unsigned char bDecompressedChanged : 1;
	unsigned char bAllowLuaGC : 1;
	unsigned char bTempValue : 1;

private:
	uint16_t iLength = 0;
	uint16_t iDataLength = 0;
	uint32_t iDecompressedLength = 0;

	char* pData = nullptr;
	char* pDecompressedData = nullptr;
};

// For other modules to utilize since we don't expose the struct. Looking at you bass
char* VoiceData_GetDecompressedData(VoiceData* pData, int* pLength)
{
	return pData->GetDecompressedData(pLength);	
}

Push_LuaClass(VoiceData)
Get_LuaClass(VoiceData, "VoiceData")

LUA_FUNCTION_STATIC(VoiceData__tostring)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, false);
	if (!pData)
	{
		LUA->PushString("VoiceData [NULL]");
		return 1;
	}

	char szBuf[64] = {};
	V_snprintf(szBuf, sizeof(szBuf), "VoiceData [%i][%i]", pData->iPlayerSlot, pData->GetLength());
	LUA->PushString(szBuf);
	return 1;
}

Default__index(VoiceData);
Default__newindex(VoiceData);
Default__GetTable(VoiceData);
Default__IsValid(VoiceData);
Default__gc(VoiceData,
	VoiceData* pVoiceData = (VoiceData*)pStoredData;
	if (pVoiceData && pVoiceData->bAllowLuaGC)
		delete pVoiceData;
)

LUA_JIT_WRAPPED_1R(VoiceData_GetPlayerSlot,
	int, iPlayerSlot, LUA->PushNumber(iPlayerSlot),
	LuaUserData*, pUD, Get_VoiceData_Data(LUA, 1, true)
)
{
	VoiceData* pData = (VoiceData*)pUD->GetData();
	if (!pData)
		return -1;

	return pData->iPlayerSlot;
}

LUA_JIT_WRAPPED_1R(VoiceData_GetLength,
	int, iPlayerSlot, LUA->PushNumber(iPlayerSlot),
	LuaUserData*, pUD, Get_VoiceData_Data(LUA, 1, true)
)
{
	VoiceData* pData = (VoiceData*)pUD->GetData();
	if (!pData)
		return -1;

	return pData->GetLength();
}

LUA_JIT_WRAPPED_1R(VoiceData_GetData,
	lua_String*, pVoiceData, if (pVoiceData) { LUA->PushString(pVoiceData->data, pVoiceData->length); } else { LUA->PushNil(); },
	LuaUserData*, pUD, Get_VoiceData_Data(LUA, 1, true)
)
{
	VoiceData* pData = (VoiceData*)pUD->GetData();
	if (!pData)
		return nullptr;

	Lua::pTempStr.data = pData->GetData();
	Lua::pTempStr.length = pData->GetLength();
	return &Lua::pTempStr;
}

LUA_JIT_WRAPPED_1R(VoiceData_GetUncompressedData,
	lua_String*, pVoiceData, if (pVoiceData) { LUA->PushString(pVoiceData->data, pVoiceData->length); } else { LUA->PushNil(); },
	LuaUserData*, pUD, Get_VoiceData_Data(LUA, 1, true); if (!Util::GetSteamUser()) {LUA->ThrowError("Failed to get SteamUser!\n");}
)
{
	VoiceData* pData = (VoiceData*)pUD->GetData();
	if (!pData)
		return nullptr;

	int iDecompressedLength = 0;
	char* pDecompressed = pData->GetDecompressedData(&iDecompressedLength);
	if (iDecompressedLength <= 0)
	{
		Lua::pTempStr.data = "";
		Lua::pTempStr.length = 0;
	} else {
		Lua::pTempStr.data = pDecompressed;
		Lua::pTempStr.length = iDecompressedLength;
	}

	return &Lua::pTempStr;
}

LUA_JIT_WRAPPED_2(VoiceData_SetUncompressedData,
	LuaUserData*, pUD, Get_VoiceData_Data(LUA, 1, true); if (!Util::GetSteamUser()) {LUA->ThrowError("Failed to get SteamUser!\n");},
	GCstr*, pUncompressedData, Lua::GetGCStr(LUA, 2)
)
{
	VoiceData* pData = (VoiceData*)pUD->GetData();
	if (!pData)
		return;

	pData->SetDecompressedData(Lua::GetGCStrData(pUncompressedData), Lua::GetGCStrLength(pUncompressedData));
}

LUA_FUNCTION_STATIC(VoiceData_GetProximity)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	LUA->PushBool(pData->bProximity);
	return 1;
}

LUA_JIT_WRAPPED_2(VoiceData_SetPlayerSlot,
	LuaUserData*, pUD, Get_VoiceData_Data(LUA, 1, true),
	int, iPlayerSlot, LUA->CheckNumber(2)
)
{
	VoiceData* pData = (VoiceData*)pUD->GetData();
	if (!pData)
		return;

	pData->iPlayerSlot = iPlayerSlot;
}

LUA_JIT_WRAPPED_2(VoiceData_SetLength,
	LuaUserData*, pUD, Get_VoiceData_Data(LUA, 1, true),
	int, iLength, LUA->CheckNumber(2)
)
{
	VoiceData* pData = (VoiceData*)pUD->GetData();
	if (!pData)
		return;

	pData->SetLength((uint16_t)iLength);
}

LUA_JIT_WRAPPED_3(VoiceData_SetData,
	LuaUserData*, pUD, Get_VoiceData_Data(LUA, 1, true),
	GCstr*, pCompressedData, Lua::GetGCStr(LUA, 2),
	int, iNewLength, LUA->CheckNumberOpt(3, -1)
)
{
	VoiceData* pData = (VoiceData*)pUD->GetData();
	if (!pData || !pCompressedData)
		return;

	pData->SetData(Lua::GetGCStrData(pCompressedData), (uint16_t)(iNewLength != -1 ? iNewLength : Lua::GetGCStrLength(pCompressedData)));
}

// This is the JIT version with (userdata, string) args while the above is (userdata, string, int)
// I was thinking about allowing JIT to just for example then pass on empty arguments like (userdata, string, int = -1) but that sucked to implement sooo we do it this way
LUA_JIT_RAW_2(VoiceData_SetData_NoLength,
	LuaUserData*, pUD,
	GCstr*, pCompressedData
)
{
	VoiceData* pData = (VoiceData*)pUD->GetData();
	if (!pData || !pCompressedData)
		return;

	pData->SetData(Lua::GetGCStrData(pCompressedData), (uint16_t)Lua::GetGCStrLength(pCompressedData));
}

LUA_FUNCTION_STATIC(VoiceData_SetProximity)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	pData->bProximity = LUA->GetBool(2);
	return 0;
}

LUA_FUNCTION_STATIC(VoiceData_CreateCopy)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	Push_VoiceData(LUA, pData->CreateCopy());
	return 1;
}

LUA_JIT_WRAPPED_1(VoiceData_Empty,
	LuaUserData*, pUD, Get_VoiceData_Data(LUA, 1, true)
)
{
	VoiceData* pData = (VoiceData*)pUD->GetData();
	if (!pData)
		return;

	pData->Empty();
	return;
}

struct WavAudioFile {
	~WavAudioFile()
	{
		if (bIsOurData)
		{
			delete[] data;
		}
	}

	void WriteData(const void* pData, int nDataLength)
	{
		if ((currentPos + nDataLength) >= dataSize)
		{
			//if (g_pVoiceChatModule.InDebug() == 1)
			{
				Warning(PROJECT_NAME " - voicechat: Almost overflowed WavAudioFile!\n");
			}
			return;
		}

		memcpy(data + currentPos, pData, nDataLength);
		currentPos += nDataLength;
	}

	int ReadData(void* pData, int nDataLength)
	{
		if ((currentPos + nDataLength) > dataSize)
			return 0;

		memcpy(pData, data + currentPos, nDataLength);
		currentPos += nDataLength;

		return nDataLength;
	}

	void Seek(int nSeek)
	{
		currentPos += nSeek;
	}

	char* GetData()
	{
		return data;
	}

	// Resets itself & resizes the data preparing for writes.
	void Resize(int nDataSize)
	{
		if (data && bIsOurData)
			delete[] data;

		data = new char[nDataSize];
		dataSize = nDataSize;
		currentPos = 0;
		bIsOurData = true;
	}

	void SetData(char* pData, int nDataLength)
	{
		if (data && bIsOurData)
			delete[] data;

		data = pData;
		dataSize = nDataLength;
		bIsOurData = false;
	}

	int CurrentPos()
	{
		if (currentPos > dataSize)
			return dataSize;

		return currentPos;
	}

private:
	char* data = nullptr;
	unsigned int dataSize = 0;
	unsigned int currentPos = 0;
	bool bIsOurData = false;
};

/*Push_LuaClass(WavAudioFile)
Get_LuaClass(WavAudioFile, "WavAudioFile")

LUA_FUNCTION_STATIC(WavAudioFile__tostring)
{
	WavAudioFile* pStream = Get_WavAudioFile(LUA, 1, false);
	if (!pStream)
	{
		LUA->PushString("WavAudioFile [NULL]");
		return 1;
	}

	char szBuf[64] = {};
	V_snprintf(szBuf, sizeof(szBuf), "WavAudioFile [%i]", pStream->data.size());
	LUA->PushString(szBuf);
	return 1;
}

Default__index(WavAudioFile);
Default__newindex(WavAudioFile);
Default__GetTable(WavAudioFile);
Default__gc(WavAudioFile,
	WavAudioFile* pWavFile = (WavAudioFile*)pStoredData;
	if (pWavFile)
		delete pWavFile;
)*/

static const int VOICESTREAM_VERSION_1 = 1;
static const int VOICESTREAM_VERSION_2 = 2;
static const int VOICESTREAM_VERSION = 2; // Current version
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
	 * 4 bytes / int - VoiceStream version number
	 * 4 bytes / int - VoiceStream tickrate
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
		unordered_map<int, VoiceData*> voiceDataEntries = pVoiceData;

		g_pFullFileSystem->Write(&VOICESTREAM_VERSION, sizeof(int), fh);

		int tickRate = (int)std::ceil(1 / gpGlobals->interval_per_tick);
		g_pFullFileSystem->Write(&tickRate, sizeof(int), fh);

		int count = (int)voiceDataEntries.size();
		g_pFullFileSystem->Write(&count, sizeof(int), fh); // First write the total number of voice data

		for (auto& [tickNumber, voiceData] : voiceDataEntries)
		{
			g_pFullFileSystem->Write(&tickNumber, sizeof(int), fh);

			int length = voiceData->GetLength();
			char* data = voiceData->GetData();

			g_pFullFileSystem->Write(&length, sizeof(int), fh);
			g_pFullFileSystem->Write(data, length, fh);
		}
	}

	static VoiceStream* Load(FileHandle_t fh)
	{
		VoiceStream* pStream = new VoiceStream;

		int version;
		g_pFullFileSystem->Read(&version, sizeof(int), fh);

		double scaleRate = 1;
		int count = version;
		if (version == VOICESTREAM_VERSION_1 || version == VOICESTREAM_VERSION_2) // Were doing this to stay compatible with the older version in the 0.7 release.
		{
			int tickRate;
			g_pFullFileSystem->Read(&tickRate, sizeof(int), fh);

			int serverTickRate = (int)std::ceil(1 / gpGlobals->interval_per_tick);
			scaleRate = (int)std::ceil(serverTickRate / tickRate);

			count = 0;
			g_pFullFileSystem->Read(&count, sizeof(int), fh);
		} else if (version < VOICESTREAM_VERSION) {
			delete pStream;
			return nullptr;
		}

		for (int i=0; i<count; ++i)
		{
			int tickNumber;
			g_pFullFileSystem->Read(&tickNumber, sizeof(int), fh);

			uint16_t length;
			if (version == VOICESTREAM_VERSION_1)
			{
				int tempLength;
				g_pFullFileSystem->Read(&tempLength, sizeof(int), fh);
				if (tempLength > USHRT_MAX)
				{
					Warning(PROJECT_NAME " - voicechat: Tried to load a voice stream that had a too large voice entry! (%i)\n", tempLength);
					delete pStream;
					return nullptr;
				}

				length = (uint16_t)tempLength;
			} else {
				g_pFullFileSystem->Read(&length, sizeof(uint16_t), fh);
			}

			char* data = new char[length];
			g_pFullFileSystem->Read(data, length, fh);

			VoiceData* voiceData = new VoiceData;
			voiceData->SetLength(length);
			voiceData->SetDataDirect(data);

			pStream->SetIndex((int)std::ceil(tickNumber * scaleRate), voiceData);
		}

		return pStream;
	}

	// We can write into a file & into a WavAudioFile struct at once.
	void SaveWave(FileHandle_t fh = nullptr, WavAudioFile* pWav = nullptr)
	{
		ISteamUser* pSteamUser = Util::GetSteamUser();
		if (!pSteamUser)
			return; // nullptr;

		const int sampleRate = SAMPLERATE_GMOD_OPUS;
		const int bytesPerSample = 2; // 16-bit mono
		std::map<int, VoiceData*> sorted(pVoiceData.begin(), pVoiceData.end());

		std::vector<char> wavePCM;
		for (auto& [tick, voiceData] : sorted)
		{
			int iLength = 0;
			char* pDecompressedData = voiceData->GetDecompressedData(&iLength);

			wavePCM.insert(wavePCM.end(), pDecompressedData, pDecompressedData + iLength);
		}

		int dataSize = wavePCM.size();
		int byteRate = sampleRate * bytesPerSample;
		int blockAlign = bytesPerSample;
		int bitsPerSample = 16;
		struct WAVHeader {
			char riff[4] = { 'R','I','F','F' };
			int fileSize;
			char wave[4] = { 'W','A','V','E' };
			char fmt[4] = { 'f','m','t',' ' };
			int fmtSize = 16;
			short audioFormat = 1; // PCM
			short numChannels = 1;
			int sampleRate;
			int byteRate;
			short blockAlign;
			short bitsPerSample;

			char data[4] = { 'd','a','t','a' };
			int dataSize;
		};

		WAVHeader header;
		header.fileSize = sizeof(WAVHeader) - 8 + dataSize;
		header.sampleRate = sampleRate;
		header.byteRate = byteRate;
		header.blockAlign = (short)blockAlign;
		header.bitsPerSample = (short)bitsPerSample;
		header.dataSize = dataSize;

		if (fh)
		{
			g_pFullFileSystem->Write(&header, sizeof(WAVHeader), fh);
			if (dataSize > 0)
				g_pFullFileSystem->Write(wavePCM.data(), dataSize, fh);
		}

		if (pWav)
		{
			pWav->Resize(sizeof(WAVHeader) + dataSize);
			pWav->WriteData(&header, sizeof(WAVHeader));

			if (dataSize > 0)
				pWav->WriteData(wavePCM.data(), dataSize);
		}

		return;
	}

	static double CatmullRom(double y0, double y1, double y2, double y3, double t) {
		return 0.5 * ((2 * y1) +
			(-y0 + y2) * t +
			(2 * y0 - 5 * y1 + 4 * y2 - y3) * t * t +
			(-y0 + 3 * y1 - 3 * y2 + y3) * t * t * t);
	}

	static std::vector<int16_t> LowPassFilter(const std::vector<int16_t>& in) {
		if (in.size() < 3) return in;

		std::vector<int16_t> out(in.size());
		out[0] = in[0];
		for (size_t i = 1; i < in.size() - 1; ++i) {
			int32_t val = (in[i - 1] + 2 * in[i] + in[i + 1]) / 4;
			out[i] = static_cast<int16_t>(std::clamp(val, -32768, 32767));
		}
		out.back() = in.back();
		return out;
	}

	static std::vector<int16_t> ResampleCubic(const std::vector<int16_t>& in, int inRate, int outRate) {
		if (in.empty() || inRate <= 0 || outRate <= 0) return {};

		double scale = static_cast<double>(outRate) / inRate;
		size_t outCount = static_cast<size_t>(in.size() * scale);
		if (outCount < 2) outCount = 2;

		std::vector<int16_t> out(outCount);

		auto getSample = [&](int64_t idx) -> int16_t {
			if (idx < 0) return in[std::min<size_t>(-idx, in.size() - 1)];
			if (static_cast<size_t>(idx) >= in.size())
				return in[std::max<size_t>(2 * in.size() - idx - 2, 0)];
			return in[idx];
		};

		double ratio = static_cast<double>(in.size() - 1) / (outCount - 1);

		for (size_t i = 0; i < outCount; ++i) {
			double src = i * ratio;
			int64_t idx = static_cast<int64_t>(src);
			double t = src - idx;

			int16_t y0 = getSample(idx - 1);
			int16_t y1 = getSample(idx);
			int16_t y2 = getSample(idx + 1);
			int16_t y3 = getSample(idx + 2);

			double val = CatmullRom(y0, y1, y2, y3, t);
			if (val > 32767.0) val = 32767.0;
			else if (val < -32768.0) val = -32768.0;

			out[i] = static_cast<int16_t>(val);
		}

		return out;
	}

	/*static std::vector<int16_t> ResampleLinear(const std::vector<int16_t>& in, int inRate, int outRate) {
		if (in.empty() || inRate <= 0 || outRate <= 0) return {};

		double scale = static_cast<double>(outRate) / inRate;
		size_t outCount = static_cast<size_t>(in.size() * scale);
		if (outCount < 2) outCount = 2;

		std::vector<int16_t> out;
		out.reserve(outCount);

		double ratio = static_cast<double>(in.size() - 1) / (outCount - 1);
		for (size_t i = 0; i < outCount; ++i) {
			double src = i * ratio;
			size_t idx = static_cast<size_t>(src);
			double frac = src - idx;

			int16_t s1 = in[idx];
			int16_t s2 = (idx + 1 < in.size()) ? in[idx + 1] : s1;

			out.push_back(static_cast<int16_t>(s1 + frac * (s2 - s1)));
		}

		return out;
	}*/

	// We CANNOT load a wav from both the FileHandle & WavAudioFile, one of them is always expected to be NULL!
	static VoiceStream* LoadWave(FileHandle_t fh = nullptr, WavAudioFile* pWav = nullptr)
	{
		struct WAVHeader {
			char riff[4];
			uint32_t fileSize;
			char wave[4];
			char fmt[4];
			uint32_t fmtSize;
			uint16_t audioFormat;
			uint16_t numChannels;
			uint32_t sampleRate;
			uint32_t byteRate;
			uint16_t blockAlign;
			uint16_t bitsPerSample;
			char data[4];
			uint32_t dataSize;
		};

		if ((!fh && !pWav) || (fh && pWav))
		{
			//if (g_pVoiceChatModule.InDebug() == 1)
			{
				Warning(PROJECT_NAME " - voicechat - LoadWave: both the FileHandle & the WaveAudioFile are NULL or valid?... How... (%p, %p)\n", fh, pWav);
			}
			return nullptr;
		}

		WAVHeader header;
		int nHeaderBytesRead = 0;
		if (fh) {
			nHeaderBytesRead = g_pFullFileSystem->Read(&header, sizeof(header), fh);
		} else {
			nHeaderBytesRead = pWav->ReadData(&header, sizeof(header));
		}

		if (nHeaderBytesRead != sizeof(header)) {
			if (g_pVoiceChatModule.InDebug() == 1)
			{
				Warning(PROJECT_NAME " - voicechat - LoadWave: invalid header!\n");
			}
			return nullptr;
		}

		// the .wav had funny shit that now causes our data to be screwed up.
		if (fh) {
			if (strncmp(header.data, "LIST", 4) == 0) {
				g_pFullFileSystem->Seek(fh, header.dataSize, FileSystemSeek_t::FILESYSTEM_SEEK_CURRENT);
				g_pFullFileSystem->Read(&header.data, sizeof(header.data), fh);
				g_pFullFileSystem->Read(&header.dataSize, sizeof(header.dataSize), fh);
			}
		} else {
			if (strncmp(header.data, "LIST", 4) == 0) {
				pWav->Seek(header.dataSize);
				pWav->ReadData(&header.data, sizeof(header.data));
				pWav->ReadData(&header.dataSize, sizeof(header.dataSize));
			}
		}

		if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0 ||
			strncmp(header.fmt, "fmt ", 4) != 0 || strncmp(header.data, "data", 4) != 0 ||
			header.audioFormat != 1) {
			if (g_pVoiceChatModule.InDebug() == 1)
			{
				Warning(PROJECT_NAME " - voicechat - LoadWave: invalid format! (%s, %s, %s, %s, %i)\n", header.riff, header.wave, header.fmt, header.data, header.audioFormat);
			}
			return nullptr;
		}

		const int inputChannels = header.numChannels;
		const int inputBitsPerSample = header.bitsPerSample;
		const int inputBytesPerSample = inputBitsPerSample / 8;
		int sampleRate = header.sampleRate;
		if (inputBitsPerSample % 8 != 0 || inputBitsPerSample > 64 || inputChannels < 1 || inputChannels > 2) {
			if (g_pVoiceChatModule.InDebug() == 1)
			{
				Warning(PROJECT_NAME " - voicechat - LoadWave: invalid sampleRate or channels! (%i, %i)\n", inputBitsPerSample, inputChannels);
			}
			return nullptr;
		}

		std::vector<char> pcmData(header.dataSize);
		uint32_t nDataBytesRead = 0;
		if (fh) {
			nDataBytesRead = (uint32_t)g_pFullFileSystem->Read(pcmData.data(), header.dataSize, fh);
		} else {
			nDataBytesRead = (uint32_t)pWav->ReadData(pcmData.data(), header.dataSize);
		}

		if (nDataBytesRead != header.dataSize) {
			if (g_pVoiceChatModule.InDebug() == 1)
			{
				Warning(PROJECT_NAME " - voicechat - LoadWave: invalid data!\n");
			}
			return nullptr;
		}

		std::vector<int16_t> monoPCM;
		const char* input = pcmData.data();
		int totalFrames = header.dataSize / (inputBytesPerSample * inputChannels);

		for (int i = 0; i < totalFrames; ++i) {
			int64_t left = 0, right = 0;

			for (int c = 0; c < inputChannels; ++c) {
				const unsigned char* src = reinterpret_cast<const unsigned char*>(
					input + (i * inputChannels + c) * inputBytesPerSample);

				int64_t sample = 0;

				switch (inputBitsPerSample) {
					case 8: {
						uint8_t s = src[0];
						sample = ((int16_t)s - 128) << 8;
						break;
					}
					case 16: {
						sample = *reinterpret_cast<const int16_t*>(src);
						break;
					}
					case 24: {
						sample = src[0] | (src[1] << 8) | (src[2] << 16);
						if (sample & 0x800000) sample |= ~0xFFFFFF;
						sample >>= 8;
						break;
					}
					case 32: {
						sample = *reinterpret_cast<const int32_t*>(src);
						sample >>= 16;
						break;
					}
					default:
					{
						if (g_pVoiceChatModule.InDebug() == 1)
						{
							Warning(PROJECT_NAME " - voicechat - LoadWave: invalid bitsPerSame! (%i)\n", inputBitsPerSample);
						}
						return nullptr;
					}
				}

				if (c == 0) left = sample;
				if (c == 1) right = sample;
			}

			int32_t mixed = (inputChannels == 2) ? (int32_t)((left + right) / 2) : (int32_t)left;
			if (mixed > 32767) mixed = 32767;
			if (mixed < -32768) mixed = -32768;

			monoPCM.push_back(static_cast<int16_t>(mixed));
		}

		if (sampleRate != SAMPLERATE_GMOD_OPUS) {
			monoPCM = ResampleCubic(LowPassFilter(monoPCM), sampleRate, SAMPLERATE_GMOD_OPUS);
			sampleRate = SAMPLERATE_GMOD_OPUS;
		}

		constexpr int bytesPerSample = sizeof(int16_t);
		const int samplesPerTick = (int)(sampleRate * gpGlobals->interval_per_tick);

		VoiceStream* pStream = new VoiceStream;
		size_t offset = 0;
		while (offset < monoPCM.size()) {
			int thisChunkSamples = MIN(samplesPerTick, static_cast<int>(monoPCM.size() - offset));
			if (thisChunkSamples <= 0)
				break;

			int tickIndex = static_cast<int>(std::ceil(offset / samplesPerTick));
			const char* decompressedBuffer = reinterpret_cast<const char*>(&monoPCM[offset]);
			int thisChunkSize = thisChunkSamples * bytesPerSample;

			VoiceData* existing = pStream->GetIndex(tickIndex);
			if (existing) { // Impossible to happen / this is old code that merged multiple ticks into one
				int pCurrentLength = 0;
				char* existingData = existing->GetDecompressedData(&pCurrentLength);
				memcpy(g_pDataBuffer.get(), existingData, pCurrentLength);

				int iNextLength = thisChunkSize;
				if ((pCurrentLength + iNextLength) > g_pDataBufferSize) // Ran out of space
					iNextLength = g_pDataBufferSize - pCurrentLength;

				memcpy(g_pDataBuffer.get() + pCurrentLength, decompressedBuffer, iNextLength);
				pCurrentLength += iNextLength;

				existing->SetDecompressedData(g_pDataBuffer.get(), pCurrentLength);

				if (g_pVoiceChatModule.InDebug() == 1)
				{
					Warning(PROJECT_NAME " - voicechat - LoadWave: Merged voicedata (%i)\n", tickIndex);
				}
			} else {
				VoiceData* voiceData = new VoiceData;
				voiceData->SetDecompressedData(decompressedBuffer, thisChunkSize);
				pStream->SetIndex(tickIndex, voiceData);
			}

			offset += thisChunkSamples;
		}

		return pStream;
	}

	/*
	 * If you push it to Lua, call ->CreateCopy() on the VoiceData,
	 * we CANT push the VoiceData we store as else the GC will do funnies & crash.
	 */
	inline VoiceData* GetIndex(int index)
	{
		if (index < nLowestTick || index > nHighestTick)
			return nullptr;

		auto it = pVoiceData.find(index);
		if (it == pVoiceData.end())
			return nullptr;

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
		pData->bAllowLuaGC = false;

		if (index > nHighestTick)
		{
			nHighestTick = index;
		}

		// Idk, I feel like some insane people might insert negative indexes xD
		if (nLowestTick > index)
		{
			nLowestTick = index;
		}
	}

	/*
	 * We create a copy of EVERY voiceData.
	 */
	inline void CreateLuaTable(GarrysMod::Lua::ILuaInterface* pLua, bool bDirect = false)
	{
		pLua->PreCreateTable(0, pVoiceData.size());
			for (auto& [tickCount, voiceData] : pVoiceData)
			{
				Push_VoiceData(pLua, bDirect ? voiceData : voiceData->CreateCopy());
				Util::RawSetI(pLua, -2, tickCount);
			}
	}

	inline int GetCount()
	{
		return (int)pVoiceData.size();
	}

	inline unordered_map<int, VoiceData*>& GetData()
	{
		return pVoiceData;
	}

	inline void ResetTick(int nResetTick = 0)
	{
		nCurrentTick = nResetTick;
	}

	inline VoiceData* GetNextTick()
	{
		return GetIndex(nCurrentTick++);
	}

	inline VoiceData* GetPreviousTick()
	{
		return GetIndex(nCurrentTick--);
	}

	inline VoiceData* GetCurrentTick()
	{
		return GetIndex(nCurrentTick);
	}

	// Yes confusing naming... Anyways
	inline int GetCurrentTickCount()
	{
		return nCurrentTick;
	}

private:
	// key = tickcount
	// value = VoiceData
	unordered_map<int, VoiceData*> pVoiceData;
	// Current tick, idea is that inside a Think hook you can call VoiceStream:GetNextTick()
	// We don't clamp it since people might for example set it to -100 and then call GetNextTick to delay the start for example.
	int nCurrentTick = 0;
	// The highest tick we have stored, we use it to skip lookups in pVoiceData to improve performance for Indexes we know don't exist.
	int nHighestTick = 0;
	int nLowestTick = 0;
};

Push_LuaClass(VoiceStream)
Get_LuaClass(VoiceStream, "VoiceStream")

LUA_FUNCTION_STATIC(VoiceStream__tostring)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, false);
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
Default__IsValid(VoiceStream);
Default__gc(VoiceStream,
	VoiceStream* pVoiceData = (VoiceStream*)pStoredData;
	if (pVoiceData)
		delete pVoiceData;
)

LUA_FUNCTION_STATIC(VoiceStream_GetData)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);
	bool bDirectData = LUA->GetBool(2);

	pStream->CreateLuaTable(LUA, bDirectData);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceStream_SetData)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);
	LUA->CheckType(2, GarrysMod::Lua::Type::Table);
	bool directData = LUA->GetBool(3);

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

		int tick = (int)LUA->GetNumber(-2); // key
		VoiceData* data = Get_VoiceData(LUA, -1, false); // value

		if (data)
		{
			pStream->SetIndex(tick, (directData && !data->IsTemp()) ? data : data->CreateCopy());
		}

		LUA->Pop(1);
	}
	LUA->Pop(1);
	return 0;
}

LUA_JIT_WRAPPED_1R(VoiceStream_GetCount,
	int, iCount, LUA->PushNumber(iCount),
	LuaUserData*, pUD, Get_VoiceStream_Data(LUA, 1, true)
)
{
	VoiceStream* pData = (VoiceStream*)pUD->GetData();
	if (!pData)
		return 0;

	return pData->GetCount();
}

LUA_FUNCTION_STATIC(VoiceStream_GetIndex)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);
	int index = (int)LUA->CheckNumber(2);
	bool directValue = LUA->GetBool(3);

	VoiceData* data = pStream->GetIndex(index);
	Push_VoiceData(LUA, data ? ((directValue && !data->IsTemp()) ? data : data->CreateCopy()) : nullptr);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceStream_SetIndex)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);
	int index = (int)LUA->CheckNumber(2);
	VoiceData* pData = Get_VoiceData(LUA, 3, true);
	bool directValue = LUA->GetBool(4);

	pStream->SetIndex(index, (directValue && !pData->IsTemp()) ? pData : pData->CreateCopy());
	return 0;
}

LUA_FUNCTION_STATIC(VoiceStream_ResetTick)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);
	int nResetTick = (int)LUA->CheckNumberOpt(2, 0);

	LUA->PushNumber(pStream->GetCurrentTickCount());
	pStream->ResetTick(nResetTick);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceStream_GetNextTick)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);
	bool bDirectData = LUA->GetBool(2);

	VoiceData* pData = pStream->GetNextTick();
	Push_VoiceData(LUA, pData ? ((bDirectData && !pData->IsTemp()) ? pData : pData->CreateCopy()) : nullptr);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceStream_GetPreviousTick)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);
	bool bDirectData = LUA->GetBool(2);

	VoiceData* pData = pStream->GetPreviousTick();
	Push_VoiceData(LUA, pData ? ((bDirectData && !pData->IsTemp()) ? pData : pData->CreateCopy()) : nullptr);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceStream_GetCurrentTick)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);
	bool bDirectData = LUA->GetBool(2);

	VoiceData* pData = pStream->GetCurrentTick();
	Push_VoiceData(LUA, pData ? ((bDirectData && !pData->IsTemp()) ? pData : pData->CreateCopy()) : nullptr);
	return 1;
}

namespace VoiceEffects
{
enum Effects {
	None = 0,
	Volume,		// "Gain" is parsed as an alias of this.
	Lowpass,	// RBJ biquad low-pass filter
	Highpass,	// RBJ biquad high-pass filter
	Bandpass,	// RBJ biquad band-pass filter (constant 0 dB peak gain)
	Distortion,	// tanh soft-clip waveshaper
	RingMod,	// sample * sin(2*pi*carrier*n/fs)
	Peaking,	// RBJ peaking EQ (resonant boost/cut at a frequency) - megaphone/phone tone
	Tremolo,	// amplitude LFO (cos) - wobble/comms flutter
	Bitcrush,	// bit-depth + sample-rate reduction - degraded/digital comms
	Noise,		// envelope-gated additive white noise - radio static/hiss
	Echo,		// feedback delay line - slapback / room echo (PA, intercom)
};

// A "preset" (e.g. HL2 radio) is just an array of primitive effects applied in order on
// ONE decompress/recompress pass. This caps the length of such a chain.
static constexpr int MAX_VOICE_EFFECT_CHAIN = 16;

struct VoiceEffectData
{
	Effects type;
	union {
		float volume;									// Volume / Gain
		struct { float freq; float q; } biquad;			// Lowpass / Highpass / Bandpass
		struct { float drive; float mix; float makeup; } distortion;	// Distortion
		struct { float carrier; float mix; } ringmod;		// RingMod
		struct { float freq; float q; float gainDb; } peaking;	// Peaking EQ
		struct { float rate; float depth; } tremolo;		// Tremolo (LFO amplitude)
		struct { float bits; float downsample; } bitcrush;	// Bitcrush (bit + rate reduction)
		struct { float level; } noise;						// Noise (gated static level 0..1)
		struct { float delay; float feedback; float mix; } echo;	// Echo (delay s, feedback 0..1, wet mix 0..1)
	} data;
};

/*
 * ===========================================================================================
 * Per-stream filter state
 * ===========================================================================================
 * Biquads (IIR delay line) and RingMod (phase accumulator) are STATEFUL. Voice arrives in
 * many small frames per talk-burst; if the filter state reset on every frame you'd get an
 * audible click/crackle at each frame boundary. So the state must persist ACROSS ApplyEffect
 * calls within a single talk session.
 *
 * EffectStageState  = the delay line / phase for ONE position in an effect chain.
 * PlayerEffectState = one EffectStageState per chain position (so e.g. Highpass->Lowpass each
 *                     keep their own independent delay line).
 *
 * LIVE path (one ApplyEffect call == one voice frame, run synchronously on the MAIN thread
 *   from inside HolyLib:PreProcessVoiceChat): state is kept in g_PlayerEffectState[], keyed by
 *   player slot, and reset on the HolyLib:OnPlayerStartTalking boundary (see CheckTalkingState)
 *   and on disconnect / level change. Because this path is always main-thread + synchronous,
 *   the shared array needs no locking.
 *
 * OFFLINE VoiceStream path (one ApplyEffect call processes the whole stream): a single
 *   PlayerEffectState lives on the stack of VoiceEffect() and is carried across the stream's
 *   ticks, so the stream is filtered as one continuous signal and it NEVER touches the shared
 *   per-slot array (safe to run on the voicechat thread pool).
 *
 * ASYNC single-VoiceData path also uses a job-local state (no cross-call continuity is possible
 *   for a one-shot async call anyway), keeping the per-slot array strictly main-thread-only.
 */
struct EffectStageState
{
	double x1, x2, y1, y2;	// Biquad Direct Form I delay line (double for IIR precision)
	double phase;			// RingMod / Tremolo phase accumulator, radians, kept in [0, 2*pi)
	uint32_t rng;			// Noise LCG state (per stage)
	double env;				// Noise squelch envelope follower (normalized |x|)
	double hold;			// Bitcrush sample-and-hold value (int16 scale)
	int holdCount;			// Bitcrush samples remaining on the current held value

	// Echo delay line. Lazily allocated (only stages that actually run an Echo pay for it), sized to
	// the delay length in samples. The default member initializers guarantee a freshly-constructed
	// (even stack-local) state starts with a null buffer, so the destructor / ResetStageState free
	// is always safe. The struct is never copied (states are held by reference in the per-slot
	// arrays), so a plain owning pointer needs no copy/move handling.
	float* echoBuf = nullptr;
	int echoLen = 0;		// ring length in samples (== delay)
	int echoPos = 0;		// write cursor

	~EffectStageState() { delete[] echoBuf; }
};

struct PlayerEffectState
{
	EffectStageState stages[MAX_VOICE_EFFECT_CHAIN];
};

// Keyed by player slot. Only ever touched on the main thread (the live
// SV_BroadcastVoiceData -> PreProcessVoiceChat -> sync ApplyEffect path).
static PlayerEffectState g_PlayerEffectState[MAX_PLAYERS];
// Second per-slot DSP state, for the comm (channel 1) render in per-listener routing. The comm
// stream is a separate IIR/phase context from the primary (channel 0) team/proximity stream so the
// two renders of one talker frame don't trample each other's filter delay lines. Reset in lockstep
// with g_PlayerEffectState (talk-start / disconnect / level change). Main-thread only.
static PlayerEffectState g_CommEffectState[MAX_PLAYERS];

static inline void ResetStageState(EffectStageState& s)
{
	s.x1 = s.x2 = s.y1 = s.y2 = 0.0;
	s.phase = 0.0;
	s.rng = 0x9E3779B9u;	// non-zero LCG seed (0 would stay 0)
	s.env = 0.0;
	s.hold = 0.0;
	s.holdCount = 0;
	// Drop the echo delay line: clears any lingering tail and frees the buffer on talk-start /
	// disconnect / level change. (echoBuf is nullptr on a fresh state via the member initializer,
	// so this is safe even before the first ApplyEcho.) ApplyEcho re-allocates on next use.
	delete[] s.echoBuf;
	s.echoBuf = nullptr;
	s.echoLen = 0;
	s.echoPos = 0;
}

static inline void ResetPlayerEffectState(PlayerEffectState& state)
{
	for (int i = 0; i < MAX_VOICE_EFFECT_CHAIN; ++i)
		ResetStageState(state.stages[i]);
}

// Called on talk-start / disconnect / level change to clear a slot's filter state.
static void ResetPlayerEffectState(int nPlayerSlot)
{
	if (nPlayerSlot < 0 || nPlayerSlot >= MAX_PLAYERS)
		return;

	ResetPlayerEffectState(g_PlayerEffectState[nPlayerSlot]);
	ResetPlayerEffectState(g_CommEffectState[nPlayerSlot]); // comm (channel 1) render state
}

/*
 * Per-player-slot opus codec for the LIVE (main-thread) FX path.
 *
 * The voice re-encode/decode goes through an Opus_FrameDecoder which holds STATEFUL opus
 * encoder/decoder objects plus monotonic per-frame sequence counters (m_encodeSeq / m_seq).
 * The default codec (g_pOpusDecoder) is a single thread_local instance, so on the main thread
 * ALL talkers would share it: with 2+ concurrent FX talkers their frames interleave through one
 * encoder+decoder, polluting opus inter-frame prediction and making each talker's wire sequence
 * numbers non-contiguous -> every other frame looks "lost" to clients and triggers PLC
 * concealment. Giving each player slot its own codec makes every talker an independent stream,
 * exactly like the working single-talker case.
 *
 * Main-thread-only (the live SV_BroadcastVoiceData path). Worker-thread effect jobs (async /
 * offline VoiceStream) keep using the thread_local g_pOpusDecoder, which is already per-thread.
 * Lazily created, freed on disconnect / level change / shutdown.
 */
static SteamOpus::Opus_FrameDecoder* g_pLiveCodecs[MAX_PLAYERS] = { nullptr };
// Second per-slot codec for per-listener comm routing (radio/phone) -- see the long note at
// GetCommCodec below. Declared here so FreeLiveCodec can release it alongside the primary codec.
static SteamOpus::Opus_FrameDecoder* g_pCommCodecs[MAX_PLAYERS] = { nullptr };

static IVoiceCodec* GetLiveCodec(int nPlayerSlot)
{
	if (nPlayerSlot < 0 || nPlayerSlot >= MAX_PLAYERS)
		return nullptr; // caller falls back to the shared thread_local codec

	if (!g_pLiveCodecs[nPlayerSlot])
		g_pLiveCodecs[nPlayerSlot] = new SteamOpus::Opus_FrameDecoder();

	return g_pLiveCodecs[nPlayerSlot];
}

static void FreeLiveCodec(int nPlayerSlot)
{
	if (nPlayerSlot < 0 || nPlayerSlot >= MAX_PLAYERS)
		return;

	if (g_pLiveCodecs[nPlayerSlot])
	{
		delete g_pLiveCodecs[nPlayerSlot];
		g_pLiveCodecs[nPlayerSlot] = nullptr;
	}

	if (g_pCommCodecs[nPlayerSlot])
	{
		delete g_pCommCodecs[nPlayerSlot];
		g_pCommCodecs[nPlayerSlot] = nullptr;
	}
}

// Second per-slot codec, used for per-listener comm routing (radio/phone). A single talker frame
// can need TWO different effect renders simultaneously -- e.g. proximity listeners hear the
// talker's in-person team FX while radio recipients hear a radio-processed version. Each render is
// an independent opus stream; encoding both through ONE codec would interleave two different audio
// contents through one encoder, making each listener's wire sequence numbers non-contiguous ->
// every other frame triggers PLC concealment (the exact bug GetLiveCodec fixes for concurrent
// talkers). So the comm render gets its own per-slot codec, fully independent of the primary
// (team/proximity) stream's g_pLiveCodecs codec. Freed in lockstep with the live codec
// (FreeLiveCodec). The array itself is declared up next to g_pLiveCodecs.
static IVoiceCodec* GetCommCodec(int nPlayerSlot)
{
	if (nPlayerSlot < 0 || nPlayerSlot >= MAX_PLAYERS)
		return nullptr; // caller falls back to the shared thread_local codec

	if (!g_pCommCodecs[nPlayerSlot])
		g_pCommCodecs[nPlayerSlot] = new SteamOpus::Opus_FrameDecoder();

	return g_pCommCodecs[nPlayerSlot];
}

static void FreeAllLiveCodecs()
{
	for (int i = 0; i < MAX_PLAYERS; ++i)
		FreeLiveCodec(i);
}

// ===========================================================================================
// DSP primitives. fs is ALWAYS SAMPLERATE_GMOD_OPUS (the rate the opus framedecoder uses).
// ===========================================================================================
static constexpr double kPi = 3.14159265358979323846;
static constexpr double kTwoPi = 2.0 * kPi;

// IIR feedback on decaying/near-silent voice can produce subnormal doubles which are
// pathologically slow on x86; flush them. The threshold is far below int16 quantization (1.0)
// so it is completely inaudible.
static inline double FlushDenormal(double v)
{
	return (v > -1.0e-15 && v < 1.0e-15) ? 0.0 : v;
}

// Round-to-nearest then clamp into the int16 range. NaN-safe: a NaN sample (which would make
// the float->int conversion undefined behavior) is treated as silence. +/-Inf fall through to
// the range clamps below. This is the single sink for every DSP stage, so guarding it here
// hardens all effects against a bad (NaN/Inf) parameter coming from Lua.
static inline int16_t ClampToInt16(double v)
{
	if (v != v) return 0; // NaN
	v += (v < 0.0) ? -0.5 : 0.5;
	if (v > 32767.0) return 32767;
	if (v < -32768.0) return -32768;
	return (int16_t)v;
}

static void AdjustVolume(int16_t* audioData, size_t dataSize, float volume) {
	for (size_t i = 0; i < dataSize; ++i) {
		int32_t adjustedSample = static_cast<int32_t>(audioData[i] * volume);
		audioData[i] = static_cast<int16_t>(std::clamp(adjustedSample, -32768, 32767));
	}
}

// RBJ "Audio EQ Cookbook" biquad coefficients, already normalized by a0.
struct BiquadCoeffs { double b0, b1, b2, a1, a2; };

static BiquadCoeffs ComputeBiquad(Effects type, double freq, double q)
{
	BiquadCoeffs c = { 1.0, 0.0, 0.0, 0.0, 0.0 }; // identity / pass-through fallback

	const double fs = (double)SAMPLERATE_GMOD_OPUS;
	if (!(freq > 0.0) || freq >= fs * 0.5 || !(q > 0.0)) // bad params -> pass-through (don't blow up)
		return c;

	const double w0 = kTwoPi * freq / fs;
	const double cosw0 = std::cos(w0);
	const double sinw0 = std::sin(w0);
	const double alpha = sinw0 / (2.0 * q);
	const double a0 = 1.0 + alpha;
	const double inv = 1.0 / a0;

	double b0, b1, b2;
	switch (type)
	{
	case Effects::Lowpass:
		b0 = (1.0 - cosw0) * 0.5; b1 = (1.0 - cosw0); b2 = (1.0 - cosw0) * 0.5;
		break;
	case Effects::Highpass:
		b0 = (1.0 + cosw0) * 0.5; b1 = -(1.0 + cosw0); b2 = (1.0 + cosw0) * 0.5;
		break;
	case Effects::Bandpass: // constant 0 dB peak gain variant
		b0 = alpha; b1 = 0.0; b2 = -alpha;
		break;
	default:
		return c;
	}

	c.b0 = b0 * inv;
	c.b1 = b1 * inv;
	c.b2 = b2 * inv;
	c.a1 = (-2.0 * cosw0) * inv;
	c.a2 = (1.0 - alpha) * inv;
	return c;
}

// RBJ peaking EQ: a resonant boost/cut of `gainDb` dB centered at `freq`, bandwidth set by `q`.
// Used for the megaphone "honk" and the telephone mid presence. Reuses the biquad delay line.
static BiquadCoeffs ComputePeaking(double freq, double q, double gainDb)
{
	BiquadCoeffs c = { 1.0, 0.0, 0.0, 0.0, 0.0 }; // pass-through fallback
	const double fs = (double)SAMPLERATE_GMOD_OPUS;
	if (!(freq > 0.0) || freq >= fs * 0.5 || !(q > 0.0) || !std::isfinite(gainDb))
		return c;

	const double A = std::pow(10.0, gainDb / 40.0);
	const double w0 = kTwoPi * freq / fs;
	const double cosw0 = std::cos(w0);
	const double alpha = std::sin(w0) / (2.0 * q);
	const double a0 = 1.0 + alpha / A;
	const double inv = 1.0 / a0;

	c.b0 = (1.0 + alpha * A) * inv;
	c.b1 = (-2.0 * cosw0) * inv;
	c.b2 = (1.0 - alpha * A) * inv;
	c.a1 = (-2.0 * cosw0) * inv;
	c.a2 = (1.0 - alpha / A) * inv;
	return c;
}

// Direct Form I biquad, in-place over the int16 buffer. The IIR feedback uses the UNCLAMPED
// floating output (true filter math); only the stored int16 is rounded/clamped.
static void ApplyBiquad(int16_t* samples, int nSamples, const BiquadCoeffs& c, EffectStageState& st)
{
	double x1 = st.x1, x2 = st.x2, y1 = st.y1, y2 = st.y2;
	for (int i = 0; i < nSamples; ++i)
	{
		double x0 = (double)samples[i];
		double y0 = c.b0 * x0 + c.b1 * x1 + c.b2 * x2 - c.a1 * y1 - c.a2 * y2;
		y0 = FlushDenormal(y0);

		x2 = x1; x1 = x0;
		y2 = y1; y1 = y0;

		samples[i] = ClampToInt16(y0);
	}
	st.x1 = x1; st.x2 = x2; st.y1 = y1; st.y2 = y2;
}

// Soft-clip waveshaper: out = makeup * ((1-mix)*x + mix*tanh(drive*x)), x normalized to [-1,1].
static void ApplyDistortion(int16_t* samples, int nSamples, float drive, float mix, float makeup)
{
	// Sanitize params (Lua can pass NaN/Inf). Note `drive < 1.0f` is false for NaN, so the
	// isfinite check must come first. Mirrors the NaN rejection ComputeBiquad already does.
	if (!std::isfinite(drive) || drive < 1.0f) drive = 1.0f;
	if (!std::isfinite(makeup)) makeup = 1.0f;
	mix = std::isfinite(mix) ? std::clamp(mix, 0.0f, 1.0f) : 0.0f;

	const double d = drive, m = mix, mk = makeup;
	for (int i = 0; i < nSamples; ++i)
	{
		double x = (double)samples[i] / 32768.0;
		double shaped = (1.0 - m) * x + m * std::tanh(d * x);
		samples[i] = ClampToInt16(mk * shaped * 32768.0);
	}
}

// Ring modulation against a sine carrier. The phase accumulator is carried across frames
// (mod 2*pi) so the carrier is phase-continuous and doesn't click at frame boundaries.
static void ApplyRingMod(int16_t* samples, int nSamples, float carrier, float mix, EffectStageState& st)
{
	// Sanitize params: a non-finite carrier would make `inc` NaN and permanently poison the
	// persistent phase accumulator (st.phase) for this slot until the next reset.
	if (!std::isfinite(carrier)) carrier = 0.0f;
	mix = std::isfinite(mix) ? std::clamp(mix, 0.0f, 1.0f) : 0.0f;

	const double inc = kTwoPi * (double)carrier / (double)SAMPLERATE_GMOD_OPUS;
	const double m = mix;
	double phase = st.phase;
	if (!std::isfinite(phase)) phase = 0.0; // recover a previously-poisoned state
	for (int i = 0; i < nSamples; ++i)
	{
		double s = (double)samples[i];
		double mod = std::sin(phase);
		samples[i] = ClampToInt16((1.0 - m) * s + m * (s * mod));

		phase += inc;
		if (phase >= kTwoPi || phase < 0.0)
		{
			phase = std::fmod(phase, kTwoPi);
			if (phase < 0.0) phase += kTwoPi;
		}
	}
	st.phase = phase;
}

// Additive white noise gated by a speech-activity envelope: it only hisses while the speaker is
// actually talking (silence stays clean), giving a believable radio static/squelch. level is
// 0..1 of full scale. Per-stage LCG + envelope live in the stage state (carried across frames).
static void ApplyNoise(int16_t* samples, int nSamples, float level, EffectStageState& st)
{
	level = std::isfinite(level) ? std::clamp(level, 0.0f, 1.0f) : 0.0f;
	if (level <= 0.0f)
		return;

	double env = st.env;
	uint32_t rng = st.rng ? st.rng : 0x9E3779B9u;
	for (int i = 0; i < nSamples; ++i)
	{
		double x = (double)samples[i] / 32768.0;
		env += 0.002 * (std::fabs(x) - env);          // speech-activity follower
		double gate = env * 20.0; if (gate > 1.0) gate = 1.0;
		rng = rng * 1664525u + 1013904223u;            // LCG white noise
		double white = (double)(int32_t)rng * (1.0 / 2147483648.0); // -1..1
		samples[i] = ClampToInt16((x + white * (double)level * gate) * 32768.0);
	}
	st.env = env;
	st.rng = rng;
}

// Tremolo: amplitude LFO. depth 0..1 = how deep the level dips; rate in Hz. Phase carried across
// frames so the wobble is continuous (no click at frame boundaries).
static void ApplyTremolo(int16_t* samples, int nSamples, float rate, float depth, EffectStageState& st)
{
	depth = std::isfinite(depth) ? std::clamp(depth, 0.0f, 1.0f) : 0.0f;
	if (!std::isfinite(rate) || rate <= 0.0f || depth <= 0.0f)
		return;

	const double inc = kTwoPi * (double)rate / (double)SAMPLERATE_GMOD_OPUS;
	double phase = st.phase;
	if (!std::isfinite(phase)) phase = 0.0;
	for (int i = 0; i < nSamples; ++i)
	{
		double lfo = 0.5 - 0.5 * std::cos(phase);     // 0..1
		samples[i] = ClampToInt16((double)samples[i] * (1.0 - (double)depth * lfo));
		phase += inc;
		if (phase >= kTwoPi) phase -= kTwoPi;
	}
	st.phase = phase;
}

// Bitcrush: quantize to `bits` bits and sample-and-hold each value for `downsample` output
// samples (sample-rate reduction). Gives a degraded / digital-comms character.
static void ApplyBitcrush(int16_t* samples, int nSamples, float bitsF, float downsampleF, EffectStageState& st)
{
	int bits = std::isfinite(bitsF) ? (int)bitsF : 16; if (bits < 1) bits = 1; if (bits > 16) bits = 16;
	int ds = std::isfinite(downsampleF) ? (int)downsampleF : 1; if (ds < 1) ds = 1; if (ds > 64) ds = 64;
	if (bits >= 16 && ds <= 1)
		return; // no-op config

	const double step = 65536.0 / (double)(1 << bits); // quantization step in int16 units
	double hold = st.hold;
	int hc = st.holdCount;
	for (int i = 0; i < nSamples; ++i)
	{
		if (hc <= 0)
		{
			hold = std::floor((double)samples[i] / step + 0.5) * step; // requantize
			hc = ds;
		}
		--hc;
		samples[i] = ClampToInt16(hold);
	}
	st.hold = hold;
	st.holdCount = hc;
}

// Feedback delay line (echo). delaySec = tap time, feedback = how much of the delayed signal is
// fed back into the line (0..0.95, controls how many repeats), mix = wet level added to the dry
// signal. The ring buffer lives in the stage state so the echo tail carries across the many small
// frames of a talk burst (and is cleared on talk-start via ResetStageState). Lazily (re)allocated
// when the delay length changes. Single-threaded per stage state (per-player worker affinity / main
// thread), so the raw buffer needs no locking.
static void ApplyEcho(int16_t* samples, int nSamples, float delaySec, float feedback, float mix, EffectStageState& st)
{
	delaySec = std::isfinite(delaySec) ? std::clamp(delaySec, 0.005f, 0.5f) : 0.0f;
	feedback = std::isfinite(feedback) ? std::clamp(feedback, 0.0f, 0.95f) : 0.0f;
	mix = std::isfinite(mix) ? std::clamp(mix, 0.0f, 1.0f) : 0.0f;
	if (delaySec <= 0.0f || mix <= 0.0f)
		return;

	int len = (int)((double)delaySec * (double)SAMPLERATE_GMOD_OPUS);
	if (len < 1) len = 1;

	if (st.echoBuf == nullptr || st.echoLen != len)
	{
		delete[] st.echoBuf;
		st.echoBuf = new float[len](); // zero-initialised (silent line); matches codebase new[] usage
		st.echoLen = len;
		st.echoPos = 0;
	}

	float* buf = st.echoBuf;
	int pos = st.echoPos;
	const int L = st.echoLen;
	const double m = (double)mix;
	const double fb = (double)feedback;
	for (int i = 0; i < nSamples; ++i)
	{
		double dry = (double)samples[i];
		double delayed = (double)buf[pos];           // signal from `len` samples ago
		buf[pos] = (float)(dry + fb * delayed);      // write input + feedback back into the line
		samples[i] = ClampToInt16(dry + m * delayed);// dry + wet echo
		if (++pos >= L) pos = 0;
	}
	st.echoPos = pos;
}

// Applies ONE primitive effect over the int16 buffer, using the given persistent stage state
// for stateful effects (biquad / ringmod / tremolo / noise / bitcrush / echo). Stateless effects ignore `st`.
static void ApplySingleEffect(int16_t* samples, int nSamples, const VoiceEffectData& e, EffectStageState& st)
{
	switch (e.type)
	{
	case Effects::Volume:
		AdjustVolume(samples, (size_t)nSamples, e.data.volume);
		break;
	case Effects::Lowpass:
	case Effects::Highpass:
	case Effects::Bandpass:
	{
		BiquadCoeffs c = ComputeBiquad(e.type, e.data.biquad.freq, e.data.biquad.q);
		ApplyBiquad(samples, nSamples, c, st);
		break;
	}
	case Effects::Distortion:
		ApplyDistortion(samples, nSamples, e.data.distortion.drive, e.data.distortion.mix, e.data.distortion.makeup);
		break;
	case Effects::RingMod:
		ApplyRingMod(samples, nSamples, e.data.ringmod.carrier, e.data.ringmod.mix, st);
		break;
	case Effects::Peaking:
	{
		BiquadCoeffs c = ComputePeaking(e.data.peaking.freq, e.data.peaking.q, e.data.peaking.gainDb);
		ApplyBiquad(samples, nSamples, c, st);
		break;
	}
	case Effects::Tremolo:
		ApplyTremolo(samples, nSamples, e.data.tremolo.rate, e.data.tremolo.depth, st);
		break;
	case Effects::Bitcrush:
		ApplyBitcrush(samples, nSamples, e.data.bitcrush.bits, e.data.bitcrush.downsample, st);
		break;
	case Effects::Noise:
		ApplyNoise(samples, nSamples, e.data.noise.level, st);
		break;
	case Effects::Echo:
		ApplyEcho(samples, nSamples, e.data.echo.delay, e.data.echo.feedback, e.data.echo.mix, st);
		break;
	default:
		break;
	}
}

// Decompresses ONCE, applies the whole effect chain in order over the int16 PCM, then marks
// the data dirty ONCE (recompress is lazy via GetData/GetLength). pState carries the per-chain
// filter state; pass nullptr to run stateless (each stage gets a fresh zeroed state).
static bool ApplyVoiceEffectChain(VoiceData* pData, const VoiceEffectData* pEffects, int nCount, PlayerEffectState* pState, IVoiceCodec* pCodec = nullptr)
{
	ISteamUser* pSteamUser = Util::GetSteamUser();
	if (!pSteamUser)
		return false;

	if (nCount <= 0)
		return false;

	int nDecompressedLength = 0;
	char* pDecompressedData = pData->GetDecompressedData(&nDecompressedLength, pCodec);

	if (pDecompressedData == nullptr || nDecompressedLength == 0)
		return false;

	// NOTE: Our effects use the buffer as int16 so the count is bytes/2. DON'T pass byte lengths
	// to the per-sample loops or you'll run off the end of the decompressed buffer.
	int16_t* samples = (int16_t*)pDecompressedData;
	int nSamples = nDecompressedLength / (int)sizeof(int16_t);

	EffectStageState scratch;
	ResetStageState(scratch);
	for (int i = 0; i < nCount; ++i)
	{
		// Each chain position has its own persistent stage state (IIR delay / ringmod phase).
		EffectStageState& st = (pState && i < MAX_VOICE_EFFECT_CHAIN) ? pState->stages[i] : scratch;
		ApplySingleEffect(samples, nSamples, pEffects[i], st);
	}

	if (!voicechat_savedecompressed.GetBool())
	{
		pData->SetDecompressedData(pDecompressedData, nDecompressedLength, pCodec);
		return true;
	}

	pData->MarkDecompressedChanged();
	// We don't call SetDecompressedData since we never changed the size. We only modify the existing data

	return true;
}

struct VoiceEffectJob {
	~VoiceEffectJob()
	{
		if (iReference != -1)
		{
			pLua->ReferenceFree(iReference);
			iReference = -1;
		}

		if (iCallbackReference != -1)
		{
			pLua->ReferenceFree(iCallbackReference);
			iCallbackReference = -1;
		}
	}

	VoiceEffectData pEffects[MAX_VOICE_EFFECT_CHAIN];	// The chain of primitives to apply, in order.
	int nEffectCount = 0;
	VoiceData* pVoiceData = nullptr;
	VoiceStream* pStreamData = nullptr;
	int iCallbackReference = -1;
	int iReference = -1; // Reference to the VoiceData or VoiceStream
	GarrysMod::Lua::ILuaInterface* pLua = nullptr;
	bool bContinueOnFailure = true;
	bool bAsync = false; // If true this runs on the thread pool -> must NOT touch g_PlayerEffectState.
	bool bIsDone = false; // Will be true, if we failed bFailed will also be true
	bool bFailed = false;
};

static void VoiceEffect(VoiceEffectJob*& pJob)
{
	if (pJob->pStreamData != nullptr)
	{
		// Offline stream: carry ONE filter state across the whole stream so the IIR filters
		// stay continuous (no click at every tick boundary). This state is local to the job
		// (never the shared per-slot array) so it is safe even on the thread pool. Iterate in
		// ascending tick order for the IIR/phase state to be meaningful.
		PlayerEffectState streamState;
		ResetPlayerEffectState(streamState);

		std::map<int, VoiceData*> sorted(pJob->pStreamData->GetData().begin(), pJob->pStreamData->GetData().end());
		bool bAnyFailed = false;
		for (auto& [tick, voiceData] : sorted)
		{
			if (!ApplyVoiceEffectChain(voiceData, pJob->pEffects, pJob->nEffectCount, &streamState))
			{
				bAnyFailed = true; // accumulate; don't let a later success mask an earlier failure
				if (!pJob->bContinueOnFailure)
					break;
			}
		}
		pJob->bFailed = bAnyFailed;
	} else {
		if (pJob->pVoiceData)
		{
			// Live per-frame path uses the persistent per-slot state (continuous across the
			// many small frames of a talk burst, reset on HolyLib:OnPlayerStartTalking). Only
			// the synchronous main-thread path may touch the shared array; async jobs use a
			// throwaway local state instead.
			PlayerEffectState localState;
			PlayerEffectState* pState;
			IVoiceCodec* pCodec = nullptr; // nullptr -> shared thread_local codec (offline/async)
			int slot = pJob->pVoiceData->iPlayerSlot; // uint8_t -> always >= 0
			if (!pJob->bAsync && slot < MAX_PLAYERS)
			{
				pState = &g_PlayerEffectState[slot];
				pCodec = GetLiveCodec(slot); // per-talker codec on the live main-thread path
			} else {
				ResetPlayerEffectState(localState);
				pState = &localState;
			}

			pJob->bFailed = !ApplyVoiceEffectChain(pJob->pVoiceData, pJob->pEffects, pJob->nEffectCount, pState, pCodec);
		} else {
			pJob->bFailed = true;
		}
	}

	pJob->bIsDone = true;
}
}

static bool g_bIsPlayerMuted[MAX_PLAYERS] = {0};
static bool g_bIsPlayerDeafened[MAX_PLAYERS] = {0};
static bool g_bIsPlayerTalking[MAX_PLAYERS] = {0};
static double g_fLastPlayerTalked[MAX_PLAYERS] = {0};
static ConVar voicechat_stopdelay("holylib_voicechat_stopdelay", "1", FCVAR_ARCHIVE, "How many seconds before a player is marked as stopped talking");
static void CheckTalkingState(int nPlayerSlot, bool bIsTalking)
{
	if (bIsTalking)
	{
		if (!g_bIsPlayerTalking[nPlayerSlot]) // Started to talk
		{
			g_bIsPlayerTalking[nPlayerSlot] = true;

			// New talk burst -> clear any leftover per-stream filter state so the first frame
			// starts from a clean delay line / phase (prevents a click on the burst boundary).
			VoiceEffects::ResetPlayerEffectState(nPlayerSlot);

			if (Lua::PushHook("HolyLib:OnPlayerStartTalking"))
			{
				CBasePlayer* pPlayer = (CBasePlayer*)Util::GetCBaseEntityFromIndex(nPlayerSlot + 1);
				Util::Push_Entity(g_Lua, pPlayer);

				g_Lua->CallFunctionProtected(2, 0, true);
			}
		}
		g_fLastPlayerTalked[nPlayerSlot] = gpGlobals->curtime;
	} else {
		if (gpGlobals->curtime > (g_fLastPlayerTalked[nPlayerSlot] + voicechat_stopdelay.GetFloat()) && g_bIsPlayerTalking[nPlayerSlot])
		{ // Stopped talking, tied to holylib_voicechat_stopdelay convar
			g_bIsPlayerTalking[nPlayerSlot] = false;

			if (Lua::PushHook("HolyLib:OnPlayerStoppedTalking"))
			{
				CBasePlayer* pPlayer = (CBasePlayer*)Util::GetCBaseEntityFromIndex(nPlayerSlot + 1);
				Util::Push_Entity(g_Lua, pPlayer);

				g_Lua->CallFunctionProtected(2, 0, true);
			}
		}
	}
}

// Forward declarations: the threaded voice-FX pipeline is defined further down (it needs the
// SV_BroadcastVoiceData detour declared below), but these module lifecycle hooks reference it.
namespace ThreadedVoiceFX { static void FreeSlot(int slot); static void StopWorkers(); }

void CVoiceChatModule::ClientDisconnect(edict_t* pClient)
{
	if (pClient->m_EdictIndex > MAX_PLAYERS)
		return;

	// We gotta prevent the hook from firing when the player already disconnected, so we reset these here
	g_bIsPlayerTalking[pClient->m_EdictIndex-1] = false;
	g_bIsPlayerMuted[pClient->m_EdictIndex-1] = false;
	g_bIsPlayerDeafened[pClient->m_EdictIndex-1] = false;
	g_fLastPlayerTalked[pClient->m_EdictIndex-1] = 0.0;
	VoiceEffects::ResetPlayerEffectState(pClient->m_EdictIndex-1);
	VoiceEffects::FreeLiveCodec(pClient->m_EdictIndex-1);
	ThreadedVoiceFX::FreeSlot(pClient->m_EdictIndex-1);
}

void CVoiceChatModule::ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
	for (int i = 0; i < gpGlobals->maxClients; ++i)
	{
		g_bIsPlayerTalking[i] = false;
		g_bIsPlayerMuted[i] = false;
		g_bIsPlayerDeafened[i] = false;
		g_fLastPlayerTalked[i] = 0.0;
		VoiceEffects::ResetPlayerEffectState(i);
	}

	// Free ALL live codecs (full slot range, not just [0, maxClients)): a codec can be lazily
	// allocated for a manually-set high VoiceData slot via Lua, which the per-client loop misses.
	ThreadedVoiceFX::StopWorkers(); // join FX workers + free their per-slot codecs/state
	VoiceEffects::FreeAllLiveCodecs();
}

void CVoiceChatModule::LevelShutdown()
{
	for (int i = 0; i < gpGlobals->maxClients; ++i)
	{
		g_bIsPlayerTalking[i] = false;
		g_bIsPlayerMuted[i] = false;
		g_bIsPlayerDeafened[i] = false;
		g_fLastPlayerTalked[i] = 0.0;
		VoiceEffects::ResetPlayerEffectState(i);
	}

	// Free ALL live codecs (full slot range, not just [0, maxClients)): a codec can be lazily
	// allocated for a manually-set high VoiceData slot via Lua, which the per-client loop misses.
	ThreadedVoiceFX::StopWorkers(); // join FX workers + free their per-slot codecs/state
	VoiceEffects::FreeAllLiveCodecs();
}

static Detouring::Hook detour_CVoiceGameMgrHelper_CanPlayerHearPlayer;
static bool hook_CVoiceGameMgrHelper_CanPlayerHearPlayer(void* voicegamemgrhelper, CBasePlayer* listener, CBasePlayer* talker, bool& bProximity)
{
	if (g_bIsPlayerDeafened[listener->edict()->m_EdictIndex-1])
	{
		if (g_pVoiceChatModule.InDebug() == 1)
			Msg(PROJECT_NAME " - voicechat: client %i voice packet was skipped since their deaf!\n", listener->edict()->m_EdictIndex-1);

		bProximity = false;
		return false;
	}

	return detour_CVoiceGameMgrHelper_CanPlayerHearPlayer.GetTrampoline<Symbols::CVoiceGameMgrHelper_CanPlayerHearPlayer>()(voicegamemgrhelper, listener, talker, bProximity);
}

static Detouring::Hook detour_SV_BroadcastVoiceData;
static void hook_SV_BroadcastVoiceData(IClient* pClient, int nBytes, char* data, int64 xuid)
{
	VPROF_BUDGET("HolyLib - SV_BroadcastVoiceData", VPROF_BUDGETGROUP_HOLYLIB);

	if (g_bIsPlayerMuted[pClient->GetPlayerSlot()])
	{
		if (g_pVoiceChatModule.InDebug() == 1)
			Msg(PROJECT_NAME " - voicechat: client %i voice packet was skipped since their muted!\n", pClient->GetPlayerSlot());

		return;
	}

	if (g_pVoiceChatModule.InDebug() >= 2)
		Msg(PROJECT_NAME " - voicechat: cl: %p\nbytes: %i\ndata: %p\n", pClient, nBytes, data);

	CheckTalkingState(pClient->GetPlayerSlot(), true);

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
		pVoiceData->MarkTemp();

		CBaseEntity* pPlayer = (CBaseEntity*)Util::GetPlayerByClient((CBaseClient*)pClient);
		Util::Push_Entity(g_Lua, pPlayer);
		LuaUserData* pLuaData = Push_VoiceData(g_Lua, pVoiceData);

		bool bHandled = false;
		if (g_Lua->CallFunctionProtected(3, 1, true))
		{
			bHandled = g_Lua->GetBool(-1);
			g_Lua->Pop(1);
		}

		if (pLuaData)
		{
			pLuaData->Release(g_Lua);
		}

		delete pVoiceData;

		Util::servergameclients->GMOD_OnReceivedVoicePacket( pPlayer->edict() );

		if (bHandled)
			return;
	}

	detour_SV_BroadcastVoiceData.GetTrampoline<Symbols::SV_BroadcastVoiceData>()(pClient, nBytes, data, xuid);
}

// ===========================================================================================
// Threaded voice-FX pipeline. Moves the expensive opus decode + DSP + re-encode OFF the main
// thread while keeping per-talker quality and the broadcast itself on the main thread.
//
// Correctness model (this is the whole point -- it avoids the 3 failure modes of HolyLib's
// generic async path):
//   * Per-player WORKER AFFINITY (slot % N). Every frame for a given player is handled by the
//     SAME worker, in FIFO order. That worker therefore has EXCLUSIVE, lock-free access to that
//     player's opus codec (g_codecs[slot]) and IIR/ring-mod filter state (g_state[slot]) -> no
//     data race, no shared-codec opus corruption, and frame ORDER is preserved.
//   * Only decode/DSP/encode run on the worker. The actual SV_BroadcastVoiceData runs on the
//     MAIN thread in LuaThink (DrainAndBroadcast), where Source networking is safe. Adds at most
//     ~1 tick of latency (imperceptible for voice).
//   * Codec/state for a slot are created, used and freed ONLY by its owning worker (free is sent
//     as an in-band job on disconnect), so the main thread never touches them while a worker might.
// ===========================================================================================
namespace ThreadedVoiceFX
{
	struct FXJob
	{
		int slot = 0;
		int inLen = 0;
		char in[g_nCompressedSize];
		VoiceEffects::VoiceEffectData effects[VoiceEffects::MAX_VOICE_EFFECT_CHAIN];
		int nEffects = 0;
		bool resetFirst = false; // new talk burst -> zero the filter state before this frame
		bool freeSlot = false;   // teardown marker: free this slot's codec+state, do not broadcast
		// filled by the worker:
		int outLen = 0;
		char out[g_nCompressedSize];
		bool broadcast = false;
	};

	static const int kMaxWorkers = 8;
	static SteamOpus::Opus_FrameDecoder* g_codecs[MAX_PLAYERS] = { nullptr }; // worker-owned (per slot)
	static VoiceEffects::PlayerEffectState g_state[MAX_PLAYERS];              // worker-owned (per slot)

	struct Worker
	{
		std::thread th;
		std::mutex m;
		std::condition_variable cv;
		std::deque<FXJob*> q;
		char pcm[g_pDataBufferSize]; // worker-local decode scratch
	};
	static Worker g_workers[kMaxWorkers];
	static int g_nWorkers = 0;
	static std::atomic<bool> g_run{ false };

	static std::mutex g_doneM;
	static std::deque<FXJob*> g_done; // completed jobs awaiting main-thread broadcast (per-player order preserved)

	static double g_lastEnqueue[MAX_PLAYERS] = { 0 }; // main-thread only: burst-start detection

	static void ProcessOne(Worker& w, FXJob* job)
	{
		const int slot = job->slot;
		if (slot < 0 || slot >= MAX_PLAYERS)
			return;

		if (job->freeSlot)
		{
			if (g_codecs[slot]) { delete g_codecs[slot]; g_codecs[slot] = nullptr; }
			VoiceEffects::ResetPlayerEffectState(g_state[slot]);
			job->broadcast = false;
			return;
		}

		if (!g_codecs[slot])
			g_codecs[slot] = new SteamOpus::Opus_FrameDecoder();
		SteamOpus::Opus_FrameDecoder* codec = g_codecs[slot];

		if (job->resetFirst)
			VoiceEffects::ResetPlayerEffectState(g_state[slot]);

		int nbytes = SteamVoice::DecompressIntoBuffer(codec, job->in, job->inLen, w.pcm, g_pDataBufferSize);
		if (nbytes <= 0)
		{
			// Degenerate/near-silent frame (too small to hold a valid opus chunk). Pass the original
			// bytes through unmodified, exactly like the sync path's ProcessVoiceData-on-failure.
			memcpy(job->out, job->in, job->inLen);
			job->outLen = job->inLen;
			job->broadcast = true;
			return;
		}

		int16_t* samples = (int16_t*)w.pcm;
		int nSamples = nbytes / (int)sizeof(int16_t);
		for (int i = 0; i < job->nEffects && i < VoiceEffects::MAX_VOICE_EFFECT_CHAIN; ++i)
			VoiceEffects::ApplySingleEffect(samples, nSamples, job->effects[i], g_state[slot].stages[i]);

		int outBytes = SteamVoice::CompressIntoBuffer(fakeSteamID, codec, w.pcm, nbytes, job->out, g_nCompressedSize, SAMPLERATE_GMOD_OPUS);
		if (outBytes <= 0)
		{
			memcpy(job->out, job->in, job->inLen); // re-encode failed -> fall back to original
			job->outLen = job->inLen;
		} else {
			job->outLen = outBytes;
		}
		job->broadcast = true;
	}

	static void WorkerLoop(Worker* w)
	{
		for (;;)
		{
			FXJob* job = nullptr;
			{
				std::unique_lock<std::mutex> lk(w->m);
				w->cv.wait(lk, [&]{ return !g_run.load() || !w->q.empty(); });
				if (!g_run.load() && w->q.empty())
					return;
				job = w->q.front();
				w->q.pop_front();
			}
			ProcessOne(*w, job);
			{
				std::lock_guard<std::mutex> lk(g_doneM);
				g_done.push_back(job);
			}
		}
	}

	static void EnsureWorkers()
	{
		if (g_run.load())
			return;
		g_nWorkers = voicechat_threads.GetInt();
		if (g_nWorkers < 1) g_nWorkers = 1;
		if (g_nWorkers > kMaxWorkers) g_nWorkers = kMaxWorkers;
		g_run.store(true);
		for (int i = 0; i < g_nWorkers; ++i)
			g_workers[i].th = std::thread(WorkerLoop, &g_workers[i]);
	}

	static void StopWorkers()
	{
		if (!g_run.load())
			return;
		g_run.store(false);
		for (int i = 0; i < g_nWorkers; ++i)
			g_workers[i].cv.notify_all();
		for (int i = 0; i < g_nWorkers; ++i)
			if (g_workers[i].th.joinable())
				g_workers[i].th.join();

		// Workers are stopped -> safe to free everything from the main thread.
		for (int i = 0; i < g_nWorkers; ++i)
		{
			for (FXJob* j : g_workers[i].q) delete j;
			g_workers[i].q.clear();
		}
		{
			std::lock_guard<std::mutex> lk(g_doneM);
			for (FXJob* j : g_done) delete j;
			g_done.clear();
		}
		for (int i = 0; i < MAX_PLAYERS; ++i)
		{
			if (g_codecs[i]) { delete g_codecs[i]; g_codecs[i] = nullptr; }
		}
	}

	// Main thread. Snapshots the compressed packet + effect chain and queues it to the slot's worker.
	static void Enqueue(int slot, const char* data, int len, const VoiceEffects::VoiceEffectData* fx, int nfx)
	{
		if (slot < 0 || slot >= MAX_PLAYERS || nfx <= 0)
			return;
		if (len <= 0 || len > g_nCompressedSize)
			return; // oversized/empty packet -> skip FX (extremely rare; engine still sent nothing extra)

		EnsureWorkers();

		FXJob* j = new FXJob();
		j->slot = slot;
		j->inLen = len;
		memcpy(j->in, data, len);
		j->nEffects = (nfx > VoiceEffects::MAX_VOICE_EFFECT_CHAIN) ? VoiceEffects::MAX_VOICE_EFFECT_CHAIN : nfx;
		for (int i = 0; i < j->nEffects; ++i)
			j->effects[i] = fx[i];

		double now = gpGlobals->curtime;
		if (now - g_lastEnqueue[slot] > 0.5) // gap since last frame -> treat as a fresh talk burst
			j->resetFirst = true;
		g_lastEnqueue[slot] = now;

		Worker& w = g_workers[slot % g_nWorkers];
		{
			std::lock_guard<std::mutex> lk(w.m);
			w.q.push_back(j);
		}
		w.cv.notify_one();
	}

	// Main thread. Queue an in-band teardown so the owning worker frees this slot's codec/state.
	static void FreeSlot(int slot)
	{
		if (slot < 0 || slot >= MAX_PLAYERS || !g_run.load())
			return;
		FXJob* j = new FXJob();
		j->slot = slot;
		j->freeSlot = true;
		Worker& w = g_workers[slot % g_nWorkers];
		{
			std::lock_guard<std::mutex> lk(w.m);
			w.q.push_back(j);
		}
		w.cv.notify_one();
	}

	// Main thread (LuaThink). Broadcast every finished frame in order via the engine trampoline.
	static void DrainAndBroadcast()
	{
		std::deque<FXJob*> done;
		{
			std::lock_guard<std::mutex> lk(g_doneM);
			if (g_done.empty())
				return;
			done.swap(g_done);
		}

		const bool bValidDetour = DETOUR_ISVALID(detour_SV_BroadcastVoiceData);
		for (FXJob* j : done)
		{
			if (j->broadcast && bValidDetour)
			{
				CBaseClient* cl = Util::GetClientByIndex(j->slot);
				if (cl && cl->IsConnected())
				{
					detour_SV_BroadcastVoiceData.GetTrampoline<Symbols::SV_BroadcastVoiceData>()(
						(IClient*)cl, j->outLen, j->out, 0
					);
				}
			}
			delete j;
		}
	}
}

LUA_FUNCTION_STATIC(voicechat_SendEmptyData)
{
	CBaseClient* pClient = Util::Get_Client(LUA, 1, true);

	SVC_VoiceData voiceData;
	voiceData.m_nFromClient = (int)LUA->CheckNumberOpt(2, pClient->GetPlayerSlot());
	voiceData.m_nLength = 0;
	voiceData.m_DataOut = nullptr; // Will possibly crash?
	voiceData.m_xuid = 0;

	pClient->SendNetMsg(voiceData);

	return 0;
}

LUA_FUNCTION_STATIC(voicechat_SendVoiceData)
{
	CBaseClient* pClient = Util::Get_Client(LUA, 1, true);
	VoiceData* pData = Get_VoiceData(LUA, 2, true);

	SVC_VoiceData voiceData;
	voiceData.m_nFromClient = pData->iPlayerSlot;
	voiceData.m_nLength = pData->GetLength() * 8; // In Bits...
	voiceData.m_DataOut = pData->GetData();
	voiceData.m_bProximity = pData->bProximity;
	voiceData.m_xuid = 0;

	pClient->SendNetMsg(voiceData);

	return 0;
}

LUA_FUNCTION_STATIC(voicechat_BroadcastVoiceData)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	SVC_VoiceData voiceData;
	voiceData.m_nFromClient = pData->iPlayerSlot;
	voiceData.m_nLength = pData->GetLength() * 8; // In Bits...
	voiceData.m_DataOut = pData->GetData();
	voiceData.m_bProximity = pData->bProximity;
	voiceData.m_xuid = 0;

	if (LUA->IsType(2, GarrysMod::Lua::Type::Table))
	{
		LUA->Push(2);
		LUA->PushNil();
		while (LUA->Next(-2))
		{
			CBaseClient* pClient = Util::Get_Client(LUA, -1, true);
			pClient->SendNetMsg(voiceData);

			LUA->Pop(1);
		}
		LUA->Pop(1);
	} else {
		for(IClient* pClient : Util::GetClients())
			pClient->SendNetMsg(voiceData);
	}

	return 0;
}

// voicechat.BroadcastVoiceDataChannel(VoiceData, recipientTable, channel)
//
// Like BroadcastVoiceData, but re-encodes through a per-talker-slot codec selected by `channel`
// instead of the shared thread_local codec. This is what makes per-listener comm routing possible:
// a single talker frame can be broadcast as TWO independent streams in the same tick -- e.g. the
// in-person/team FX to proximity listeners on channel 0 and a radio-processed copy to radio
// recipients on channel 1 -- without the two encodes polluting each other's opus sequence numbers
// (which would cause PLC concealment every other frame). See GetCommCodec.
//
//   channel 0 -> g_pLiveCodecs[slot]  (the primary/proximity stream; same codec ProcessVoiceData
//                                       uses, so a talker's in-person voice stays one continuous
//                                       stream whether or not they are also on comms this tick)
//   channel 1 -> g_pCommCodecs[slot]  (the comm/radio/phone stream)
//
// A recipient table is REQUIRED -- comm routing is always targeted. The VoiceData's bProximity
// flag is honored (set it false for radio/phone so the client plays it flat/2D, true for the
// in-person stream so it stays positional).
LUA_FUNCTION_STATIC(voicechat_BroadcastVoiceDataChannel)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);
	LUA->CheckType(2, GarrysMod::Lua::Type::Table);
	int channel = (int)LUA->CheckNumberOpt(3, 0);

	IVoiceCodec* pCodec = (channel == 1)
		? VoiceEffects::GetCommCodec(pData->iPlayerSlot)
		: VoiceEffects::GetLiveCodec(pData->iPlayerSlot);

	SVC_VoiceData voiceData;
	voiceData.m_nFromClient = pData->iPlayerSlot;
	voiceData.m_nLength = pData->GetLength(pCodec) * 8; // In Bits...
	voiceData.m_DataOut = pData->GetData(pCodec);
	voiceData.m_bProximity = pData->bProximity;
	voiceData.m_xuid = 0;

	LUA->Push(2);
	LUA->PushNil();
	while (LUA->Next(-2))
	{
		CBaseClient* pClient = Util::Get_Client(LUA, -1, true);
		pClient->SendNetMsg(voiceData);

		LUA->Pop(1);
	}
	LUA->Pop(1);

	return 0;
}

LUA_FUNCTION_STATIC(voicechat_ProcessVoiceData)
{
	CBaseClient* pClient = Util::Get_Client(LUA, 1, true);
	VoiceData* pData = Get_VoiceData(LUA, 2, true);

	if (!DETOUR_ISVALID(detour_SV_BroadcastVoiceData))
		LUA->ThrowError("Missing valid detour for SV_BroadcastVoiceData!\n");

	// Re-encode through the talker's per-slot live codec so this matches the decode done in
	// ApplyEffect and stays independent of other concurrent talkers (see GetLiveCodec). For an
	// invalid slot GetLiveCodec returns nullptr -> the shared codec (unchanged behavior).
	IVoiceCodec* pCodec = VoiceEffects::GetLiveCodec(pData->iPlayerSlot);
	detour_SV_BroadcastVoiceData.GetTrampoline<Symbols::SV_BroadcastVoiceData>()(
		pClient, pData->GetLength(pCodec), pData->GetData(pCodec), 0
	);

	return 0;
}

LUA_FUNCTION_STATIC(voicechat_CreateVoiceData)
{
	int iPlayerSlot = (int)LUA->CheckNumberOpt(1, 0);
	const char* pStr = LUA->CheckStringOpt(2, nullptr);
	int iLength = (int)LUA->CheckNumberOpt(3, 0);

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

	Push_VoiceData(LUA, pData);

	return 1;
}

LUA_FUNCTION_STATIC(voicechat_IsHearingClient)
{
	CBaseClient* pClient = Util::Get_Client(LUA, 1, true);
	CBaseClient* pTargetClient = Util::Get_Client(LUA, 2, true);

	LUA->PushBool(pClient->IsHearingClient(pTargetClient->GetPlayerSlot()));

	return 1;
}

LUA_FUNCTION_STATIC(voicechat_IsProximityHearingClient)
{
	CBaseClient* pClient = Util::Get_Client(LUA, 1, true);
	CBaseClient* pTargetClient = Util::Get_Client(LUA, 2, true);

	LUA->PushBool(pClient->IsProximityHearingClient(pTargetClient->GetPlayerSlot()));

	return 1;
}

LUA_FUNCTION_STATIC(voicechat_CreateVoiceStream)
{
	Push_VoiceStream(LUA, new VoiceStream);
	return 1;
}

enum VoiceStreamTaskStatus {
	VoiceStreamTaskStatus_FAILED_INVALID_FILE = -4,
	VoiceStreamTaskStatus_FAILED_INVALID_VERSION = -3,
	VoiceStreamTaskStatus_FAILED_FILE_NOT_FOUND = -2,
	VoiceStreamTaskStatus_FAILED_INVALID_TYPE = -1,
	VoiceStreamTaskStatus_NONE = 0,
	VoiceStreamTaskStatus_DONE = 1
};

enum VoiceStreamTaskType {
	VoiceStreamTask_NONE,
	VoiceStreamTask_SAVE,
	VoiceStreamTask_LOAD,
	VoiceStreamTask_LOADWAV,
};

struct VoiceStreamTask {
	~VoiceStreamTask()
	{
		if (iReference != -1)
		{
			pLua->ReferenceFree(iReference);
			iReference = -1;
		}

		if (iCallback != -1)
		{
			pLua->ReferenceFree(iCallback);
			iCallback = -1;
		}

		if (pWavFile)
		{
			// We push it using LUA->PushString, so we expect that when our Task is deleted that either our wav data was pushed to Lua or it was discarded.
			delete pWavFile;
		}
	}

	char pFileName[MAX_PATH] = {0};
	char pGamePath[MAX_PATH] = {0};

	VoiceStreamTaskType iType = VoiceStreamTask_NONE;
	VoiceStreamTaskStatus iStatus = VoiceStreamTaskStatus_NONE;

	WavAudioFile* pWavFile = nullptr;
	VoiceStream* pStream = nullptr;
	int iReference = -1; // A reference to the pStream to stop the GC from kicking in.
	int iCallback = -1;
	GarrysMod::Lua::ILuaInterface* pLua = nullptr;
};

class LuaVoiceModuleData : public Lua::ModuleData
{
public:
	unordered_set<VoiceStreamTask*> pVoiceStreamTasks;
	unordered_set<VoiceEffects::VoiceEffectJob*> pVoiceEffectTasks;
};

LUA_GetModuleData(LuaVoiceModuleData, g_pVoiceChatModule, VoiceChat)

static std::string_view getFileExtension(const std::string_view& fileName) {
	size_t lastDotPos = fileName.find_last_of('.');
	if (lastDotPos == std::string::npos || lastDotPos == fileName.length() - 1)
		return "";

	return fileName.substr(lastDotPos + 1);
}

static void VoiceStreamJob(VoiceStreamTask*& task)
{
	switch(task->iType)
	{
		case VoiceStreamTask_LOAD:
		{
			FileHandle_t fh = g_pFullFileSystem->Open(task->pFileName, "rb", task->pGamePath);
			if (fh)
			{
				bool bIsWave = getFileExtension(task->pFileName) == "wav";
				if (bIsWave)
				{
					task->pStream = VoiceStream::LoadWave(fh);
					if (task->pStream == nullptr)
						task->iStatus = VoiceStreamTaskStatus_FAILED_INVALID_FILE;
				} else {
					task->pStream = VoiceStream::Load(fh);
					if (task->pStream == nullptr)
						task->iStatus = VoiceStreamTaskStatus_FAILED_INVALID_VERSION;
				}

				g_pFullFileSystem->Close(fh);
			} else {
				task->iStatus = VoiceStreamTaskStatus_FAILED_FILE_NOT_FOUND;
			}
			break;
		}
		case VoiceStreamTask_SAVE:
		{
			bool bIsWave = getFileExtension(task->pFileName) == "wav";
			FileHandle_t fh = g_pFullFileSystem->Open(task->pFileName, "wb", task->pGamePath);
			if (fh)
			{
				if (bIsWave)
				{
					task->pStream->SaveWave(fh, task->pWavFile);
					//task->pWavFile = task->pStream->SaveWave(fh);

					//if (task->pWavFile == nullptr)
					//	task->iStatus = VoiceStreamTaskStatus_FAILED_INVALID_FILE;
				} else {
					task->pStream->Save(fh);
				}

				g_pFullFileSystem->Close(fh);
			} else {
				if (task->pWavFile)
				{
					task->pStream->SaveWave(nullptr, task->pWavFile);
					break;
				}

				task->iStatus = VoiceStreamTaskStatus_FAILED_FILE_NOT_FOUND;
			}
			break;
		}
		case VoiceStreamTask_LOADWAV:
		{
			task->pStream = VoiceStream::LoadWave(nullptr, task->pWavFile);
			if (task->pStream == nullptr)
				task->iStatus = VoiceStreamTaskStatus_FAILED_INVALID_FILE;
			break;
		}
		default:
		{
			Warning(PROJECT_NAME " - VoiceChat(VoiceStreamJob): Managed to get a job without a valid type. How.\n");
			task->iStatus = VoiceStreamTaskStatus_FAILED_INVALID_TYPE;
			return;
		}
	}

	if (task->iStatus == VoiceStreamTaskStatus_NONE) // Wasn't set already? then just set it to done.
	{
		task->iStatus = VoiceStreamTaskStatus_DONE;
	}
}

static void EnsureVoiceThreadPool()
{
	if (!pVoiceThreadPool)
	{
		pVoiceThreadPool = V_CreateThreadPool();
		Util::StartThreadPool(pVoiceThreadPool, voicechat_threads.GetInt());
	}
}

#define AddVoiceJobToPool(func, pTask) EnsureVoiceThreadPool(); pVoiceThreadPool->QueueCall(&func, pTask);

LUA_FUNCTION_STATIC(voicechat_LoadVoiceStream)
{
	LuaVoiceModuleData* pData = GetVoiceChatLuaData(LUA);

	const char* pFileName = LUA->CheckString(1);
	const char* pGamePath = LUA->CheckStringOpt(2, "DATA");
	bool bAsync = LUA->IsType(3, GarrysMod::Lua::Type::Function);

	VoiceStreamTask* task = new VoiceStreamTask;
	V_strncpy(task->pFileName, pFileName, sizeof(task->pFileName));
	V_strncpy(task->pGamePath, pGamePath, sizeof(task->pGamePath));
	task->iType = VoiceStreamTask_LOAD;
	task->pLua = LUA;

	if (bAsync)
	{
		LUA->Push(3);
		task->iCallback = Util::ReferenceCreate(LUA, "voicechat.LoadVoiceStream - callback");

		pData->pVoiceStreamTasks.insert(task);
		AddVoiceJobToPool(VoiceStreamJob, task);
		return 0;
	} else {
		VoiceStreamJob(task);
		Push_VoiceStream(LUA, task->pStream);
		LUA->PushNumber((int)task->iStatus);
		delete task;
		return 2;
	}
}

LUA_FUNCTION_STATIC(voicechat_LoadVoiceStreamFromWaveString)
{
	LuaVoiceModuleData* pData = GetVoiceChatLuaData(LUA);

	size_t pWaveDataLength;
	const char* pWaveData = Util::CheckLString(LUA, 1, &pWaveDataLength);
	bool bAsync = LUA->IsType(2, GarrysMod::Lua::Type::Function);
	bool bPromiseToNeverModify = LUA->GetBool(3);

	VoiceStreamTask* task = new VoiceStreamTask;
	task->pWavFile = new WavAudioFile;
	task->iType = VoiceStreamTask_LOAD;
	task->pLua = LUA;

	if (!bPromiseToNeverModify)
	{
		task->pWavFile->Resize(pWaveDataLength);
		task->pWavFile->WriteData(pWaveData, pWaveDataLength);
	} else {
		// Instead of creating a copy of the data we store the pointer saving memory & making this faster
		// Though they have to keep their promise to not modify the data while we use this!
		task->pWavFile->SetData((char*)pWaveData, pWaveDataLength);

		LUA->Push(1);
		task->iReference = Util::ReferenceCreate(LUA, "voicechat.LoadVoiceStreamFromWavString - data");
	}

	if (bAsync)
	{
		LUA->Push(3);
		task->iCallback = Util::ReferenceCreate(LUA, "voicechat.LoadVoiceStreamFromWavString - callback");

		pData->pVoiceStreamTasks.insert(task);
		AddVoiceJobToPool(VoiceStreamJob, task);
		return 0;
	} else {
		VoiceStreamJob(task);
		Push_VoiceStream(LUA, task->pStream);
		LUA->PushNumber((int)task->iStatus);
		delete task;
		return 2;
	}
}

LUA_FUNCTION_STATIC(voicechat_SaveVoiceStream)
{
	/*
		Default version:

		-- Either you set the last false / returnWaveData to true to receive the wavData as a string, or you provide no file name
		voicechat.SaveVoiceStream(stream, "file.wav", "DATA", function(stream, status, wav) end, false)

		Argument Overload version:

		-- This will always return wav data since we never get a file name.
		voicechat.SaveVoiceStream(stream, function(stream, status, wav) end)
	*/

	LuaVoiceModuleData* pData = GetVoiceChatLuaData(LUA);

	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);
	bool bIsOverloadFunction = LUA->IsType(2, GarrysMod::Lua::Type::Function); // If our second arg is a function then we got the overload version.
	bool bAsync = bIsOverloadFunction;
	const char* pFileName = nullptr;
	const char* pGamePath = nullptr;
	bool bReturnWaveData = bAsync;
	if (!bAsync)
	{
		pFileName = LUA->CheckStringOpt(2, nullptr);
		pGamePath = LUA->CheckStringOpt(3, "DATA");
		bAsync = LUA->IsType(4, GarrysMod::Lua::Type::Function);
		bReturnWaveData = LUA->GetBool(5) || !pFileName; // if no pFileName was given / if nil then it should also return wav data.
	}

	if (!bReturnWaveData && !pFileName)
		LUA->CheckType(2, GarrysMod::Lua::Type::String); // Will error xd

	VoiceStreamTask* task = new VoiceStreamTask;
	if (pFileName) {
		V_strncpy(task->pFileName, pFileName, sizeof(task->pFileName));
	} else {
		V_memset(task->pFileName, 0, sizeof(task->pFileName));
	}
	V_strncpy(task->pGamePath, pGamePath, sizeof(task->pGamePath));
	task->iType = VoiceStreamTask_SAVE;
	
	if (bReturnWaveData || !pFileName) {
		task->pWavFile = new WavAudioFile;
	}
	task->pStream = pStream;
	task->pLua = LUA;

	if (bAsync)
	{
		LUA->Push(1);
		task->iReference = Util::ReferenceCreate(LUA, "voicechat.SaveVoiceStream - VoiceStream");

		if (bIsOverloadFunction) {
			LUA->Push(2);
		} else {
			LUA->Push(4);
		}
		task->iCallback = Util::ReferenceCreate(LUA, "voicechat.SaveVoiceStream - callback");
		pData->pVoiceStreamTasks.insert(task);
		AddVoiceJobToPool(VoiceStreamJob, task);
		return 0;
	} else {
		VoiceStreamJob(task);
		LUA->PushNumber((int)task->iStatus);
		if (task->pWavFile)
		{
			LUA->PushString(task->pWavFile->GetData(), task->pWavFile->CurrentPos());
		}
		delete task;
		return 1;
	}
}

LUA_FUNCTION_STATIC(voicechat_IsPlayerTalking)
{
	int iClient = Util::Get_ClientIndex(LUA, 1, true);

	LUA->PushBool(g_bIsPlayerTalking[iClient]);
	return 1;
}

LUA_FUNCTION_STATIC(voicechat_LastPlayerTalked)
{
	int iClient = Util::Get_ClientIndex(LUA, 1, true);

	LUA->PushNumber(g_fLastPlayerTalked[iClient]);
	return 1;
}

// Reads a number field from the effect table at absolute stack index `idx`, falling back if
// it isn't present / isn't a number. Leaves the stack balanced.
static float GetEffectNumber(GarrysMod::Lua::ILuaInterface* LUA, int idx, const char* field, float fallback)
{
	LUA->GetField(idx, field);
	float v = LUA->IsType(-1, GarrysMod::Lua::Type::Number) ? (float)LUA->GetNumber(-1) : fallback;
	LUA->Pop(1);
	return v;
}

// Parses ONE effect table at absolute stack index `idx` into `out`. Returns true if the
// "EffectName" named a known effect. Leaves the stack balanced.
static bool ParseVoiceEffectTable(GarrysMod::Lua::ILuaInterface* LUA, int idx, VoiceEffects::VoiceEffectData& out)
{
	using namespace VoiceEffects;
	out.type = Effects::None;

	LUA->GetField(idx, "EffectName");
	const char* pEffectName = LUA->IsType(-1, GarrysMod::Lua::Type::String) ? LUA->GetString(-1) : nullptr;
	LUA->Pop(1);

	if (!pEffectName)
		return false;

	if (V_stricmp(pEffectName, "Volume") == 0 || V_stricmp(pEffectName, "Gain") == 0)
	{
		out.type = Effects::Volume; // Gain is an alias of Volume
		out.data.volume = GetEffectNumber(LUA, idx, "Volume", GetEffectNumber(LUA, idx, "volume", 1.0f));
		return true;
	}
	else if (V_stricmp(pEffectName, "Lowpass") == 0)
	{
		out.type = Effects::Lowpass;
		out.data.biquad.freq = GetEffectNumber(LUA, idx, "freq", 3400.0f);
		out.data.biquad.q = GetEffectNumber(LUA, idx, "q", 0.707f);
		return true;
	}
	else if (V_stricmp(pEffectName, "Highpass") == 0)
	{
		out.type = Effects::Highpass;
		out.data.biquad.freq = GetEffectNumber(LUA, idx, "freq", 300.0f);
		out.data.biquad.q = GetEffectNumber(LUA, idx, "q", 0.707f);
		return true;
	}
	else if (V_stricmp(pEffectName, "Bandpass") == 0)
	{
		out.type = Effects::Bandpass;
		out.data.biquad.freq = GetEffectNumber(LUA, idx, "freq", 1700.0f);
		out.data.biquad.q = GetEffectNumber(LUA, idx, "q", 0.707f);
		return true;
	}
	else if (V_stricmp(pEffectName, "Distortion") == 0)
	{
		out.type = Effects::Distortion;
		out.data.distortion.drive = GetEffectNumber(LUA, idx, "drive", 2.0f);
		out.data.distortion.mix = GetEffectNumber(LUA, idx, "mix", 0.5f);
		out.data.distortion.makeup = GetEffectNumber(LUA, idx, "makeup", 1.0f);
		return true;
	}
	else if (V_stricmp(pEffectName, "RingMod") == 0)
	{
		out.type = Effects::RingMod;
		out.data.ringmod.carrier = GetEffectNumber(LUA, idx, "carrier", 30.0f);
		out.data.ringmod.mix = GetEffectNumber(LUA, idx, "mix", 1.0f);
		return true;
	}
	else if (V_stricmp(pEffectName, "Peaking") == 0)
	{
		out.type = Effects::Peaking;
		out.data.peaking.freq = GetEffectNumber(LUA, idx, "freq", 1000.0f);
		out.data.peaking.q = GetEffectNumber(LUA, idx, "q", 1.0f);
		out.data.peaking.gainDb = GetEffectNumber(LUA, idx, "gainDb", GetEffectNumber(LUA, idx, "gain", 6.0f));
		return true;
	}
	else if (V_stricmp(pEffectName, "Tremolo") == 0)
	{
		out.type = Effects::Tremolo;
		out.data.tremolo.rate = GetEffectNumber(LUA, idx, "rate", 8.0f);
		out.data.tremolo.depth = GetEffectNumber(LUA, idx, "depth", 0.5f);
		return true;
	}
	else if (V_stricmp(pEffectName, "Bitcrush") == 0)
	{
		out.type = Effects::Bitcrush;
		out.data.bitcrush.bits = GetEffectNumber(LUA, idx, "bits", 8.0f);
		out.data.bitcrush.downsample = GetEffectNumber(LUA, idx, "downsample", 2.0f);
		return true;
	}
	else if (V_stricmp(pEffectName, "Noise") == 0 || V_stricmp(pEffectName, "Static") == 0)
	{
		out.type = Effects::Noise;
		out.data.noise.level = GetEffectNumber(LUA, idx, "level", GetEffectNumber(LUA, idx, "noise", 0.02f));
		return true;
	}
	else if (V_stricmp(pEffectName, "Echo") == 0 || V_stricmp(pEffectName, "Delay") == 0)
	{
		out.type = Effects::Echo;
		out.data.echo.delay = GetEffectNumber(LUA, idx, "delay", 0.15f);     // seconds
		out.data.echo.feedback = GetEffectNumber(LUA, idx, "feedback", 0.4f); // 0..0.95
		out.data.echo.mix = GetEffectNumber(LUA, idx, "mix", 0.5f);          // wet 0..1
		return true;
	}

	return false;
}

LUA_FUNCTION_STATIC(voicechat_ApplyEffect)
{
	LuaVoiceModuleData* pData = GetVoiceChatLuaData(LUA);

	LUA->CheckType(1, GarrysMod::Lua::Type::Table);

	bool bIsAsync = LUA->IsType(3, GarrysMod::Lua::Type::Function);
	bool bIsVoiceData = true;
	if (!LUA->IsType(2, Lua::GetLuaData(LUA)->GetMetaTable(Lua::LuaTypes::VoiceData)))
	{
		bIsVoiceData = false;
		LUA->CheckType(2, Lua::GetLuaData(LUA)->GetMetaTable(Lua::LuaTypes::VoiceStream));
	}

	VoiceEffects::VoiceEffectJob* pJob = new VoiceEffects::VoiceEffectJob();
	pJob->pLua = LUA;
	pJob->bAsync = bIsAsync;
	if (bIsVoiceData)
	{
		pJob->pVoiceData = Get_VoiceData(LUA, 2, false);
	} else {
		pJob->pStreamData = Get_VoiceStream(LUA, 2, false);
	}

	// Argument 1 is EITHER a single effect table { EffectName = ... } OR an array of them
	// { {EffectName=...}, {EffectName=...}, ... } applied in order. We detect a single effect
	// by the presence of a string "EffectName" field at the top level.
	LUA->GetField(1, "EffectName");
	bool bSingleEffect = LUA->IsType(-1, GarrysMod::Lua::Type::String);
	LUA->Pop(1);

	if (bSingleEffect)
	{
		if (ParseVoiceEffectTable(LUA, 1, pJob->pEffects[0]))
			pJob->nEffectCount = 1;
	} else {
		int nEntries = LUA->ObjLen(1);
		for (int i = 1; i <= nEntries && pJob->nEffectCount < VoiceEffects::MAX_VOICE_EFFECT_CHAIN; ++i)
		{
			LUA->PushNumber(i);
			LUA->RawGet(1);
			if (LUA->IsType(-1, GarrysMod::Lua::Type::Table))
			{
				int top = LUA->Top(); // absolute index of the element we just pushed
				if (ParseVoiceEffectTable(LUA, top, pJob->pEffects[pJob->nEffectCount]))
					++pJob->nEffectCount;
			}
			LUA->Pop(1);
		}
	}

	LUA->GetField(1, "ContinueOnFailure");
	if (LUA->IsType(-1, GarrysMod::Lua::Type::Bool))
	{
		pJob->bContinueOnFailure = LUA->GetBool(-1);
	}
	LUA->Pop(1);

	if (bIsAsync)
	{
		LUA->Push(2);
		pJob->iReference = Util::ReferenceCreate(LUA, "voicechat.ApplyEffect - VoiceData/VoiceStream");

		LUA->Push(3);
		pJob->iCallbackReference = Util::ReferenceCreate(LUA, "voicechat.ApplyEffect - Callback");

		pData->pVoiceEffectTasks.insert(pJob);
		AddVoiceJobToPool(VoiceEffects::VoiceEffect, pJob);
		return 0;
	} else {
		VoiceEffects::VoiceEffect(pJob);
		LUA->PushBool(!pJob->bFailed);
		delete pJob;
		return 1;
	}
}

// voicechat.ApplyEffect runs the decode/DSP/encode synchronously on the main thread. For the live
// per-frame broadcast path under load (many simultaneous FX talkers) that opus work can dominate
// the tick. ApplyEffectThreaded(client, voiceData, effectChain) instead snapshots the packet +
// chain and hands the heavy work to a per-player worker thread; the result is broadcast on the next
// main-thread tick (see ThreadedVoiceFX). Quality is identical (same codec, same DSP) -- only the
// timing moves off-thread. Call it from HolyLib:PreProcessVoiceChat and return true to suppress the
// original packet, exactly like ApplyEffect + ProcessVoiceData. Returns true if the job was queued.
LUA_FUNCTION_STATIC(voicechat_ApplyEffectThreaded)
{
	CBaseClient* pClient = Util::Get_Client(LUA, 1, true);
	VoiceData* pData = Get_VoiceData(LUA, 2, true);
	LUA->CheckType(3, GarrysMod::Lua::Type::Table);

	// Parse the chain: argument 3 is EITHER a single { EffectName = ... } OR an array of them
	// (mirrors voicechat_ApplyEffect's detection).
	VoiceEffects::VoiceEffectData effects[VoiceEffects::MAX_VOICE_EFFECT_CHAIN];
	int nEffects = 0;

	LUA->GetField(3, "EffectName");
	bool bSingleEffect = LUA->IsType(-1, GarrysMod::Lua::Type::String);
	LUA->Pop(1);

	if (bSingleEffect)
	{
		if (ParseVoiceEffectTable(LUA, 3, effects[0]))
			nEffects = 1;
	} else {
		int nEntries = LUA->ObjLen(3);
		for (int i = 1; i <= nEntries && nEffects < VoiceEffects::MAX_VOICE_EFFECT_CHAIN; ++i)
		{
			LUA->PushNumber(i);
			LUA->RawGet(3);
			if (LUA->IsType(-1, GarrysMod::Lua::Type::Table))
			{
				int top = LUA->Top();
				if (ParseVoiceEffectTable(LUA, top, effects[nEffects]))
					++nEffects;
			}
			LUA->Pop(1);
		}
	}

	// Snapshot the raw compressed packet via the public accessors (the members are private). The
	// hook's VoiceData is unmodified here, so GetData()/GetLength() return the original bytes.
	char* raw = pData->GetData();
	int rawLen = pData->GetLength();
	if (nEffects <= 0 || !raw || rawLen <= 0)
	{
		LUA->PushBool(false);
		return 1;
	}

	ThreadedVoiceFX::Enqueue(pClient->GetPlayerSlot(), raw, rawLen, effects, nEffects);
	LUA->PushBool(true);
	return 1;
}

// voicechat.ApplyEffectChannel(VoiceData, effectChain, channel)
//
// Like ApplyEffect, but routes the decode + (deferred) encode and the DSP filter state through a
// per-talker-slot channel selected by `channel`, instead of always using the primary one. This is
// what makes per-listener comm routing correct: a single talker frame can be rendered TWO different
// ways in the same tick -- e.g. team FX for proximity listeners (channel 0) and radio FX for radio
// recipients (channel 1) -- as two fully independent opus streams. Pair each render with
// BroadcastVoiceDataChannel(VoiceData, recipients, channel) using the SAME channel so the deferred
// encode picks the matching codec. Reusing ApplyEffect (always channel 0) for both renders would
// run both through one encoder + one DSP state and corrupt each other (PLC every other frame +
// smeared filters). Returns true on success.
//
//   channel 0 -> g_pLiveCodecs[slot] + g_PlayerEffectState[slot]  (identical to ApplyEffect)
//   channel 1 -> g_pCommCodecs[slot] + g_CommEffectState[slot]
//
// Synchronous, main-thread only (mirrors the sync ApplyEffect path). Do NOT mix with the threaded
// path for the same talker on the same frame.
LUA_FUNCTION_STATIC(voicechat_ApplyEffectChannel)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);
	LUA->CheckType(2, GarrysMod::Lua::Type::Table);
	int channel = (int)LUA->CheckNumberOpt(3, 0);

	// Parse the chain: argument 2 is EITHER a single { EffectName = ... } OR an array of them
	// (mirrors voicechat_ApplyEffect / voicechat_ApplyEffectThreaded).
	VoiceEffects::VoiceEffectData effects[VoiceEffects::MAX_VOICE_EFFECT_CHAIN];
	int nEffects = 0;

	LUA->GetField(2, "EffectName");
	bool bSingleEffect = LUA->IsType(-1, GarrysMod::Lua::Type::String);
	LUA->Pop(1);

	if (bSingleEffect)
	{
		if (ParseVoiceEffectTable(LUA, 2, effects[0]))
			nEffects = 1;
	} else {
		int nEntries = LUA->ObjLen(2);
		for (int i = 1; i <= nEntries && nEffects < VoiceEffects::MAX_VOICE_EFFECT_CHAIN; ++i)
		{
			LUA->PushNumber(i);
			LUA->RawGet(2);
			if (LUA->IsType(-1, GarrysMod::Lua::Type::Table))
			{
				int top = LUA->Top();
				if (ParseVoiceEffectTable(LUA, top, effects[nEffects]))
					++nEffects;
			}
			LUA->Pop(1);
		}
	}

	if (nEffects <= 0)
	{
		LUA->PushBool(false);
		return 1;
	}

	int slot = pData->iPlayerSlot; // uint8_t -> always >= 0
	VoiceEffects::PlayerEffectState* pState = nullptr;
	IVoiceCodec* pCodec = nullptr; // nullptr -> shared thread_local codec (invalid slot)
	if (slot < MAX_PLAYERS)
	{
		if (channel == 1)
		{
			pState = &VoiceEffects::g_CommEffectState[slot];
			pCodec = VoiceEffects::GetCommCodec(slot);
		} else {
			pState = &VoiceEffects::g_PlayerEffectState[slot];
			pCodec = VoiceEffects::GetLiveCodec(slot);
		}
	}

	bool ok = VoiceEffects::ApplyVoiceEffectChain(pData, effects, nEffects, pState, pCodec);
	LUA->PushBool(ok);
	return 1;
}

LUA_FUNCTION_STATIC(voicechat_IsPlayerMuted)
{
	int iClient = Util::Get_ClientIndex(LUA, 1, true);
	LUA->PushBool(g_bIsPlayerMuted[iClient]);
	return 1;
}

LUA_FUNCTION_STATIC(voicechat_SetPlayerMuted)
{
	int iClient = Util::Get_ClientIndex(LUA, 1, true);
	g_bIsPlayerMuted[iClient] = LUA->GetBool(2);
	return 0;
}

LUA_FUNCTION_STATIC(voicechat_IsPlayerDeaf)
{
	int iClient = Util::Get_ClientIndex(LUA, 1, true);
	LUA->PushBool(g_bIsPlayerDeafened[iClient]);
	return 1;
}

LUA_FUNCTION_STATIC(voicechat_SetPlayerDeaf)
{
	int iClient = Util::Get_ClientIndex(LUA, 1, true);
	g_bIsPlayerDeafened[iClient] = LUA->GetBool(2);
	return 0;
}

void CVoiceChatModule::LuaThink(GarrysMod::Lua::ILuaInterface* pLua)
{
	// Broadcast any voice frames the FX worker threads finished since last tick (main thread).
	ThreadedVoiceFX::DrainAndBroadcast();

	LuaVoiceModuleData* pData = GetVoiceChatLuaData(pLua);
	if (!pData)
		return;

	for (int i=0; i<gpGlobals->maxClients; ++i)
		CheckTalkingState(i, false);

	for (auto it = pData->pVoiceStreamTasks.begin(); it != pData->pVoiceStreamTasks.end(); )
	{
		VoiceStreamTask* pTask = *it;
		if (pTask->iStatus == VoiceStreamTaskStatus_NONE)
		{
			it++;
			continue;
		}

		pLua->ReferencePush(pTask->iCallback);
		Push_VoiceStream(pLua, pTask->pStream); // Lua GC will take care of deleting.
		pLua->PushBool(pTask->iStatus == VoiceStreamTaskStatus_DONE);
		if (pTask->iType == VoiceStreamTask_SAVE && pTask->pWavFile)
		{
			pLua->PushString(pTask->pWavFile->GetData(), pTask->pWavFile->CurrentPos());
		}

		pLua->CallFunctionProtected(2, 0, true);
		
		delete pTask;
		it = pData->pVoiceStreamTasks.erase(it);
	}

	for (auto it = pData->pVoiceEffectTasks.begin(); it != pData->pVoiceEffectTasks.end(); )
	{
		VoiceEffects::VoiceEffectJob* pJob = *it;
		if (!pJob->bIsDone)
		{
			it++;
			continue;
		}

		pLua->ReferencePush(pJob->iCallbackReference);
		pLua->ReferencePush(pJob->iReference);
		pLua->PushBool(!pJob->bFailed);

		pLua->CallFunctionProtected(2, 0, true);
		
		delete pJob;
		it = pData->pVoiceEffectTasks.erase(it);
	}
}

void CVoiceChatModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Lua::GetLuaData(pLua)->SetModuleData(m_pID, new LuaVoiceModuleData);

	Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::VoiceData, pLua->CreateMetaTable("VoiceData"));
		Util::AddFunc(pLua, VoiceData__tostring, "__tostring");
		Util::AddFunc(pLua, VoiceData__index, "__index");
		Util::AddFunc(pLua, VoiceData__newindex, "__newindex");
		Util::AddFunc(pLua, VoiceData__gc, "__gc");
		LUA_REGISTER_JIT(pLua, VoiceData_GetTable, "GetTable");
		LUA_REGISTER_JIT(pLua, VoiceData_IsValid, "IsValid");
		LUA_REGISTER_JIT(pLua, VoiceData_GetData, "GetData");
		LUA_REGISTER_JIT(pLua, VoiceData_GetLength, "GetLength");
		LUA_REGISTER_JIT(pLua, VoiceData_GetPlayerSlot, "GetPlayerSlot");
		LUA_REGISTER_JIT(pLua, VoiceData_SetData, "SetData");
		LUA_REGISTER_OVERLOAD(pLua, VoiceData_SetData_NoLength, "SetData");
		LUA_REGISTER_JIT(pLua, VoiceData_SetLength, "SetLength");
		LUA_REGISTER_JIT(pLua, VoiceData_SetPlayerSlot, "SetPlayerSlot");
		LUA_REGISTER_JIT(pLua, VoiceData_GetUncompressedData, "GetUncompressedData");
		LUA_REGISTER_JIT(pLua, VoiceData_SetUncompressedData, "SetUncompressedData");
		Util::AddFunc(pLua, VoiceData_GetProximity, "GetProximity");
		Util::AddFunc(pLua, VoiceData_SetProximity, "SetProximity");
		Util::AddFunc(pLua, VoiceData_CreateCopy, "CreateCopy");
		LUA_REGISTER_JIT(pLua, VoiceData_Empty, "Empty");
	pLua->Pop(1);

	Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::VoiceStream, pLua->CreateMetaTable("VoiceStream"));
		Util::AddFunc(pLua, VoiceStream__tostring, "__tostring");
		Util::AddFunc(pLua, VoiceStream__index, "__index");
		Util::AddFunc(pLua, VoiceStream__newindex, "__newindex");
		Util::AddFunc(pLua, VoiceStream__gc, "__gc");
		LUA_REGISTER_JIT(pLua, VoiceStream_GetTable, "GetTable");
		LUA_REGISTER_JIT(pLua, VoiceStream_IsValid, "IsValid");
		Util::AddFunc(pLua, VoiceStream_GetData, "GetData");
		Util::AddFunc(pLua, VoiceStream_SetData, "SetData");
		LUA_REGISTER_JIT(pLua, VoiceStream_GetCount, "GetCount");
		Util::AddFunc(pLua, VoiceStream_GetIndex, "GetIndex");
		Util::AddFunc(pLua, VoiceStream_SetIndex, "SetIndex");

		Util::AddFunc(pLua, VoiceStream_ResetTick, "ResetTick");
		Util::AddFunc(pLua, VoiceStream_GetNextTick, "GetNextTick");
		Util::AddFunc(pLua, VoiceStream_GetCurrentTick, "GetCurrentTick");
		Util::AddFunc(pLua, VoiceStream_GetPreviousTick, "GetPreviousTick");
	pLua->Pop(1);

	/*Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::WavAudioFile, pLua->CreateMetaTable("WavAudioFile"));
		Util::AddFunc(pLua, WavAudioFile__tostring, "__tostring");
		Util::AddFunc(pLua, WavAudioFile__index, "__index");
		Util::AddFunc(pLua, WavAudioFile__newindex, "__newindex");
		Util::AddFunc(pLua, WavAudioFile__gc, "__gc");
		LUA_REGISTER_JIT(pLua, WavAudioFile_GetTable, "GetTable");
	pLua->Pop(1);*/

	Util::StartTable(pLua);
		Util::AddFunc(pLua, voicechat_SendEmptyData, "SendEmptyData");
		Util::AddFunc(pLua, voicechat_SendVoiceData, "SendVoiceData");
		Util::AddFunc(pLua, voicechat_BroadcastVoiceData, "BroadcastVoiceData");
		Util::AddFunc(pLua, voicechat_BroadcastVoiceDataChannel, "BroadcastVoiceDataChannel");
		Util::AddFunc(pLua, voicechat_ProcessVoiceData, "ProcessVoiceData");
		Util::AddFunc(pLua, voicechat_CreateVoiceData, "CreateVoiceData");
		Util::AddFunc(pLua, voicechat_IsHearingClient, "IsHearingClient");
		Util::AddFunc(pLua, voicechat_IsProximityHearingClient, "IsProximityHearingClient");
		Util::AddFunc(pLua, voicechat_CreateVoiceStream, "CreateVoiceStream");
		Util::AddFunc(pLua, voicechat_LoadVoiceStream, "LoadVoiceStream");
		Util::AddFunc(pLua, voicechat_LoadVoiceStreamFromWaveString, "LoadVoiceStreamFromWaveString");
		Util::AddFunc(pLua, voicechat_SaveVoiceStream, "SaveVoiceStream");
		Util::AddFunc(pLua, voicechat_IsPlayerTalking, "IsPlayerTalking");
		Util::AddFunc(pLua, voicechat_LastPlayerTalked, "LastPlayerTalked");
		Util::AddFunc(pLua, voicechat_ApplyEffect, "ApplyEffect");
		Util::AddFunc(pLua, voicechat_ApplyEffectThreaded, "ApplyEffectThreaded");
		Util::AddFunc(pLua, voicechat_ApplyEffectChannel, "ApplyEffectChannel");
		Util::AddFunc(pLua, voicechat_SetPlayerMuted, "SetPlayerMuted");
		Util::AddFunc(pLua, voicechat_IsPlayerMuted, "IsPlayerMuted");
		Util::AddFunc(pLua, voicechat_IsPlayerDeaf, "IsPlayerDeaf");
		Util::AddFunc(pLua, voicechat_SetPlayerDeaf, "SetPlayerDeaf");
	Util::FinishTable(pLua, "voicechat");
}

void CVoiceChatModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "voicechat");
}

void CVoiceChatModule::Shutdown()
{
	if (pVoiceThreadPool)
	{
		Util::DestroyThreadPool(pVoiceThreadPool);
		pVoiceThreadPool = nullptr;
	}

	ThreadedVoiceFX::StopWorkers(); // join FX workers + free their per-slot codecs/state
	VoiceEffects::FreeAllLiveCodecs();
}

IVoiceServer* g_pVoiceServer = nullptr;
void CVoiceChatModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	if (appfn[0])
	{
		g_pVoiceServer = (IVoiceServer*)appfn[0](INTERFACEVERSION_VOICESERVER, nullptr);
	} else {
		SourceSDK::FactoryLoader engine_loader("engine");
		g_pVoiceServer = engine_loader.GetInterface<IVoiceServer>(INTERFACEVERSION_VOICESERVER);
	}

	Detour::CheckValue("get interface", "g_pVoiceServer", g_pVoiceServer != nullptr);
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

	SourceSDK::ModuleLoader server_loader("server");
	Detour::Create(
		&detour_CVoiceGameMgrHelper_CanPlayerHearPlayer, "CVoiceGameMgrHelper::CanPlayerHearPlayer",
		server_loader.GetModule(), Symbols::CVoiceGameMgrHelper_CanPlayerHearPlayerSym,
		(void*)hook_CVoiceGameMgrHelper_CanPlayerHearPlayer, m_pID
	);
}

void CVoiceChatModule::PreLuaModuleLoaded(lua_State* L, const char* pFileName)
{
	std::string_view strFileName = pFileName;
	if (strFileName.find("voicebox") !=std::string::npos)
	{
		Msg(PROJECT_NAME " - voicechat: Removing SV_BroadcastVoiceData hook before voicebox is loaded\n");
		detour_SV_BroadcastVoiceData.Disable();
		detour_SV_BroadcastVoiceData.Destroy();
	}
}

void CVoiceChatModule::PostLuaModuleLoaded(lua_State* L, const char* pFileName)
{
	std::string_view strFileName = pFileName;
	if (strFileName.find("voicebox") !=std::string::npos)
	{
		Msg(PROJECT_NAME " - voicechat: Recreating SV_BroadcastVoiceData hook after voicebox was loaded\n");
		SourceSDK::ModuleLoader engine_loader("engine");
		Detour::Create(
			&detour_SV_BroadcastVoiceData, "SV_BroadcastVoiceData",
			engine_loader.GetModule(), Symbols::SV_BroadcastVoiceDataSym,
			(void*)hook_SV_BroadcastVoiceData, m_pID
		);
	}
}