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
#include "unordered_set"
#include "server.h"
#include "ivoiceserver.h"
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
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax);
	virtual void LevelShutdown() OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual void LuaThink(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void PreLuaModuleLoaded(lua_State* L, const char* pFileName) OVERRIDE;
	virtual void PostLuaModuleLoaded(lua_State* L, const char* pFileName) OVERRIDE;
	virtual const char* Name() { return "voicechat"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32; };
	virtual bool SupportsMultipleLuaStates() { return true; };
};

static ConVar voicechat_hooks("holylib_voicechat_hooks", "1", 0);

static constexpr int g_pDataBufferSize = 16384; // Used to decompress the data. 
static constexpr int g_nCompressedSize = g_pDataBufferSize / 4; // Used as char[] to stackallocate compressed buffers when compressing which later on are moved to the heap
// A buffer so that we reduce allocations. You should only use it directly in one function and NOT pass it around as other functions might override its contents!
static thread_local std::unique_ptr<char[]> g_pDataBuffer(new char[g_pDataBufferSize]); // We cannot just use it normally since else we run out of TBL space (Error: cannot allocate memory in static TLS block)
static thread_local SteamOpus::Opus_FrameDecoder g_pOpusDecoder;
static uint64_t fakeSteamID = 0x0110000100000001; // STEAM_0:1:0
// static inline void ClearDataBuffer() { memset(g_pDataBuffer, 0, g_pDefaultDecompressedSize); }

static IThreadPool* pVoiceThreadPool = NULL;
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

struct VoiceData
{
	~VoiceData() {
		if (pData)
			delete[] pData;

		if (pDecompressedData)
			delete[] pDecompressedData;
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

	inline void SetData(const char* pNewData, int iNewLength)
	{
		iLength = iNewLength;
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
	inline void SetDecompressedData(const char* pNewData, int iNewLength)
	{
		if (!voicechat_savedecompressed.GetBool())
		{
			char pCompressed[g_nCompressedSize];
			int bytes = SteamVoice::CompressIntoBuffer(
				fakeSteamID, &g_pOpusDecoder,
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

	inline char* GetData()
	{
		if ((bDecompressedChanged || !pData) && pDecompressedData)
		{
			char pCompressed[g_nCompressedSize];
			int bytes = SteamVoice::CompressIntoBuffer(
				fakeSteamID, &g_pOpusDecoder,
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

			SetData(pCompressed, bytes);
			bDecompressedChanged = false;
		}

		return pData;
	}

	// If you call this expect g_pDataBuffer to be changed.
	inline char* GetDecompressedData(int* pLength)
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
			&g_pOpusDecoder,
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

		SetDecompressedData(g_pDataBuffer.get(), bytes);

		*pLength = iDecompressedLength;
		return pDecompressedData;
	}

	inline void SetLength(int iNewLength)
	{
		iLength = iNewLength;
	}

	inline int GetLength()
	{
		if (bDecompressedChanged && pDecompressedData)
		{
			char pCompressed[g_nCompressedSize];
			int bytes = SteamVoice::CompressIntoBuffer(
				fakeSteamID, &g_pOpusDecoder,
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

	int iPlayerSlot = 0; // What if it's an invalid one ;D (It doesn't care.......)
	bool bProximity = true;
	bool bDecompressedChanged = false;
	bool bAllowLuaGC = true;

private:
	int iLength = 0;
	int iDecompressedLength = 0;

	char* pData = nullptr;
	char* pDecompressedData = nullptr;
};

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
Default__gc(VoiceData,
	VoiceData* pVoiceData = (VoiceData*)pStoredData;
	if (pVoiceData && pVoiceData->bAllowLuaGC)
		delete pVoiceData;
)

LUA_FUNCTION_STATIC(VoiceData_IsValid)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, false);

	LUA->PushBool(pData != nullptr);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetPlayerSlot)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	LUA->PushNumber(pData->iPlayerSlot);

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetLength)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	LUA->PushNumber(pData->GetLength());

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetData)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	LUA->PushString(pData->GetData(), pData->GetLength());

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetUncompressedData)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	ISteamUser* pSteamUser = Util::GetSteamUser();
	if (!pSteamUser)
		LUA->ThrowError("Failed to get SteamUser!\n");

	int iDecompressedLength = 0;
	char* pDecompressed = pData->GetDecompressedData(&iDecompressedLength);
	if (iDecompressedLength <= 0)
	{
		LUA->PushString("");
		return 1;
	}

	LUA->PushString(pDecompressed, iDecompressedLength); // Lua creates a copy so g_pDataBuffer can be discarded.
	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_SetUncompressedData)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);
	const char* pUncompressedData = LUA->CheckString(2);
	int iSize = LUA->ObjLen(2);

	ISteamUser* pSteamUser = Util::GetSteamUser();
	if (!pSteamUser)
		LUA->ThrowError("Failed to get SteamUser!\n");

	pData->SetDecompressedData(pUncompressedData, iSize);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_GetProximity)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	LUA->PushBool(pData->bProximity);

	return 1;
}

LUA_FUNCTION_STATIC(VoiceData_SetPlayerSlot)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	pData->iPlayerSlot = (int)LUA->CheckNumber(2);

	return 0;
}

LUA_FUNCTION_STATIC(VoiceData_SetLength)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

	pData->SetLength((int)LUA->CheckNumber(2));

	return 0;
}

LUA_FUNCTION_STATIC(VoiceData_SetData)
{
	VoiceData* pData = Get_VoiceData(LUA, 1, true);

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

		if (currentPos < 0)
			currentPos = 0;
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
static const int VOICESTREAM_VERSION = 1; // Current version
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
		std::unordered_map<int, VoiceData*> voiceDataEnties = pVoiceData;

		g_pFullFileSystem->Write(&VOICESTREAM_VERSION, sizeof(int), fh);

		int tickRate = std::ceil(1 / gpGlobals->interval_per_tick);
		g_pFullFileSystem->Write(&tickRate, sizeof(int), fh);

		int count = (int)voiceDataEnties.size();
		g_pFullFileSystem->Write(&count, sizeof(int), fh); // First write the total number of voice data

		for (auto& [tickNumber, voiceData] : voiceDataEnties)
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
		if (version == VOICESTREAM_VERSION_1) // Were doing this to stay compatible with the older version in the 0.7 release.
		{
			int tickRate;
			g_pFullFileSystem->Read(&tickRate, sizeof(int), fh);

			int serverTickRate = std::ceil(1 / gpGlobals->interval_per_tick);
			scaleRate = std::ceil(serverTickRate / tickRate);

			count = 0;
			g_pFullFileSystem->Read(&count, sizeof(int), fh);
		} else if (version < VOICESTREAM_VERSION) {
			delete pStream;
			return NULL;
		}

		for (int i=0; i<count; ++i)
		{
			int tickNumber;
			g_pFullFileSystem->Read(&tickNumber, sizeof(int), fh);

			int length;
			g_pFullFileSystem->Read(&length, sizeof(int), fh);

			char* data = new char[length];
			g_pFullFileSystem->Read(data, length, fh);

			VoiceData* voiceData = new VoiceData;
			voiceData->SetLength(length);
			voiceData->SetDataDirect(data);

			pStream->SetIndex(std::ceil(tickNumber * scaleRate), voiceData);
		}

		return pStream;
	}

	// We can write into a file & into a WavAudioFile struct at once.
	void SaveWave(FileHandle_t fh = nullptr, WavAudioFile* pWav = nullptr)
	{
		ISteamUser* pSteamUser = Util::GetSteamUser();
		if (!pSteamUser)
			return; // NULL;

		const int sampleRate = SAMPLERATE_GMOD_OPUS;
		const int bytesPerSample = 2; // 16-bit mono
		std::map<int, VoiceData*> sorted(pVoiceData.begin(), pVoiceData.end());

		std::vector<char> wavePCM;
		const float intervalPerTick = gpGlobals->interval_per_tick;
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
		header.blockAlign = blockAlign;
		header.bitsPerSample = bitsPerSample;
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
			return NULL;
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
			return NULL;
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
			return NULL;
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
			return NULL;
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
			return NULL;
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
						return NULL;
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
		if (index < nLowestTick || index > nHightestTick)
			return NULL;

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
		pData->bAllowLuaGC = false;

		if (index > nHightestTick)
		{
			nHightestTick = index;
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

	inline std::unordered_map<int, VoiceData*>& GetData()
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
	std::unordered_map<int, VoiceData*> pVoiceData;
	// Current tick, idea is that inside a Think hook you can call VoiceStream:GetNextTick()
	// We don't clamp it since people might for example set it to -100 and then call GetNextTick to delay the start for example.
	int nCurrentTick = 0;
	// The highest tick we have stored, we use it to skip lookups in pVoiceData to improve performance for Indexes we know don't exist.
	int nHightestTick = 0;
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
Default__gc(VoiceStream,
	VoiceStream* pVoiceData = (VoiceStream*)pStoredData;
	if (pVoiceData)
		delete pVoiceData;
)

LUA_FUNCTION_STATIC(VoiceStream_IsValid)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, false);

	LUA->PushBool(pStream != nullptr);
	return 1;
}

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
			pStream->SetIndex(tick, data->CreateCopy());
		}

		LUA->Pop(1);
	}
	LUA->Pop(1);
	return 0;
}

LUA_FUNCTION_STATIC(VoiceStream_GetCount)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);

	LUA->PushNumber(pStream->GetCount());
	return 1;
}

LUA_FUNCTION_STATIC(VoiceStream_GetIndex)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);
	int index = (int)LUA->CheckNumber(2);
	bool directValue = LUA->GetBool(3);

	VoiceData* data = pStream->GetIndex(index);
	Push_VoiceData(LUA, data ? (directValue ? data : data->CreateCopy()) : NULL);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceStream_SetIndex)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);
	int index = (int)LUA->CheckNumber(2);
	VoiceData* pData = Get_VoiceData(LUA, 3, true);
	bool directValue = LUA->GetBool(4);

	pStream->SetIndex(index, directValue ? pData : pData->CreateCopy());
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
	Push_VoiceData(LUA, pData ? (bDirectData ? pData : pData->CreateCopy()) : NULL);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceStream_GetPreviousTick)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);
	bool bDirectData = LUA->GetBool(2);

	VoiceData* pData = pStream->GetPreviousTick();
	Push_VoiceData(LUA, pData ? (bDirectData ? pData : pData->CreateCopy()) : NULL);
	return 1;
}

LUA_FUNCTION_STATIC(VoiceStream_GetCurrentTick)
{
	VoiceStream* pStream = Get_VoiceStream(LUA, 1, true);
	bool bDirectData = LUA->GetBool(2);

	VoiceData* pData = pStream->GetCurrentTick();
	Push_VoiceData(LUA, pData ? (bDirectData ? pData : pData->CreateCopy()) : NULL);
	return 1;
}

namespace VoiceEffects
{
enum Effects {
	None = 0,
	Volume,
};

struct VoiceEffectData
{
	Effects type;
	union {
		float volume;
	} data;
};

static void AdjustVolume(int16_t* audioData, size_t dataSize, float volume) {
	for (size_t i = 0; i < dataSize; ++i) {
		int32_t adjustedSample = static_cast<int32_t>(audioData[i] * volume);
		audioData[i] = static_cast<int16_t>(std::clamp(adjustedSample, -32768, 32767));
	}
}

static bool ApplyVoiceEffect(VoiceData* pData, VoiceEffectData& pEffect, int index = -1)
{
	ISteamUser* pSteamUser = Util::GetSteamUser();
	if (!pSteamUser)
		return false;

	int nDecompressedLength = 0;
	char* pDecompressedData = pData->GetDecompressedData(&nDecompressedLength);

	if (pDecompressedData == nullptr || nDecompressedLength == 0)
		return false;

	// NOTE: Our effects already use it as int_16 so DONT pass g_pDataBufferSize since that is meant for function that treat it as char*
	// if you fk this up you'll modify the 20kb directly after our buffer which will fk up shit
	int uncompressedSizeForInt16 = nDecompressedLength / sizeof(int16_t); // Divided by int16_t since our g_pDataBuffer uses int16_t and not char!
	switch(pEffect.type)
	{
	case Effects::Volume:
		AdjustVolume((int16_t*)pDecompressedData, uncompressedSizeForInt16, pEffect.data.volume);
		break;
	default:
		break;
	}

	if (!voicechat_savedecompressed.GetBool())
	{
		pData->SetDecompressedData(pDecompressedData, nDecompressedLength);
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

	VoiceEffectData pEffect;
	VoiceData* pVoiceData = nullptr;
	VoiceStream* pStreamData = nullptr;
	int iCallbackReference = -1;
	int iReference = -1; // Reference to the VoiceData or VoiceStream
	GarrysMod::Lua::ILuaInterface* pLua = NULL;
	bool bContinueOnFailure = true;
	bool bIsDone = false; // Will be true, if we failed bFailed will also be true
	bool bFailed = false;
};

static void VoiceEffect(VoiceEffectJob*& pJob)
{
	if (pJob->pStreamData != nullptr)
	{
		for (auto it = pJob->pStreamData->GetData().begin(); it != pJob->pStreamData->GetData().end(); ++it)
		{
			pJob->bFailed = !ApplyVoiceEffect(it->second, pJob->pEffect, it->first);
			if (pJob->bFailed && !pJob->bContinueOnFailure)
				break;
		}
	} else {
		if (pJob->pVoiceData)
		{
			pJob->bFailed = !ApplyVoiceEffect(pJob->pVoiceData, pJob->pEffect);
		} else {
			pJob->bFailed = true;
		}
	}

	pJob->bIsDone = true;
}
}

static CPlayerBitVec* g_PlayerModEnable;
static CPlayerBitVec* g_BanMasks;
static CPlayerBitVec* g_SentGameRulesMasks;
static CPlayerBitVec* g_SentBanMasks;
static CPlayerBitVec* g_bWantModEnable;
static double g_fLastPlayerTalked[ABSOLUTE_PLAYER_LIMIT] = {0};
static double g_fLastPlayerUpdated[ABSOLUTE_PLAYER_LIMIT] = {0};
static bool g_bIsPlayerTalking[ABSOLUTE_PLAYER_LIMIT] = {0};
static CVoiceGameMgr* g_pManager = NULL;
static ConVar voicechat_updateinterval("holylib_voicechat_updateinterval", "0.1", FCVAR_ARCHIVE, "How often we call PlayerCanHearPlayersVoice for the actively talking players. This interval is unique to each player");
static ConVar voicechat_managerupdateinterval("holylib_voicechat_managerupdateinterval", "0.1", FCVAR_ARCHIVE, "How often we loop through all players to check their voice states. We still check the player's interval to reduce calls if they already have been updated in the last x(your defined interval) seconds.");
static ConVar voicechat_stopdelay("holylib_voicechat_stopdelay", "1", FCVAR_ARCHIVE, "How many seconds before a player is marked as stopped talking");
static ConVar voicechat_canhearhimself("holylib_voicechat_canhearhimself", "1", FCVAR_ARCHIVE, "If enabled, we assume the player can always hear himself and thus we save one call for PlayerCanHearPlayersVoice");
static void UpdatePlayerTalkingState(CBasePlayer* pPlayer, bool bIsTalking = false)
{
	if (!g_pManager) // Skip if we have no manager.
		return;

	int iClient = pPlayer->edict()->m_EdictIndex-1;
	double fTime = Util::engineserver->Time();
	if (bIsTalking)
	{
		g_fLastPlayerTalked[iClient] = fTime;
	} else {
		// We update anyways, just to ensure that the code won't break, but we won't call the lua hook since we know their not talking and don't need it.
		if (g_bIsPlayerTalking[iClient] && (g_fLastPlayerTalked[iClient] + voicechat_stopdelay.GetFloat()) > fTime) // They are talking, and we have no reason to update so just skip it.
		{
			if (g_pVoiceChatModule.InDebug() == 2)
			{
				Msg("Skipping voice player update since their not talking! (%i, %f, %f, %f)\n", iClient, fTime, g_fLastPlayerTalked[iClient], voicechat_stopdelay.GetFloat());
			}

			return;
		}
	}

	if ((g_bIsPlayerTalking[iClient] == bIsTalking || !bIsTalking) && (g_fLastPlayerUpdated[iClient] + voicechat_updateinterval.GetFloat()) > fTime)
	{
		if (g_pVoiceChatModule.InDebug() == 2)
		{
			Msg("Skipping voice player update! (%i, %s, %s, %f, %f, %f)\n", iClient, bIsTalking ? "true" : "false", g_bIsPlayerTalking[iClient] ? "true" : "false", g_fLastPlayerUpdated[iClient], fTime, voicechat_updateinterval.GetFloat());
		}

		return;
	}

	if (g_pVoiceChatModule.InDebug() == 2)
	{
		Msg("Doing voice player update! (%i, %s, %s, %f, %f, %f)\n", iClient, bIsTalking ? "true" : "false", g_bIsPlayerTalking[iClient] ? "true" : "false", g_fLastPlayerUpdated[iClient], fTime, voicechat_updateinterval.GetFloat());
	}

	CSingleUserRecipientFilter user( pPlayer );

	// Request the state of their "VModEnable" cvar.
	if((*g_bWantModEnable)[iClient])
	{
		UserMessageBegin( user, "RequestState" );
		MessageEnd();
		// Since this is reliable, only send it once
		(*g_bWantModEnable)[iClient] = false;
	}

	ConVarRef sv_alltalk("sv_alltalk");
	bool bAllTalk = !!sv_alltalk.GetInt();

	CPlayerBitVec gameRulesMask;
	CPlayerBitVec ProximityMask;
	bool bProximity = false;
	if(bIsTalking && (*g_PlayerModEnable)[iClient] )
	{
		bool bCanHearHimself = voicechat_canhearhimself.GetBool();
		// Build a mask of who they can hear based on the game rules.
		for(int iOtherClient=0; iOtherClient < g_pManager->m_nMaxPlayers; iOtherClient++)
		{
			CBaseEntity *pEnt = Util::GetCBaseEntityFromEdict(Util::engineserver->PEntityOfEntIndex(iOtherClient + 1));
			if(pEnt && pEnt->IsPlayer() && 
				(bCanHearHimself && (iOtherClient == iClient) || (bAllTalk || g_pManager->m_pHelper->CanPlayerHearPlayer((CBasePlayer*)pEnt, pPlayer, bProximity ))) )
			{
				gameRulesMask[iOtherClient] = true;
				ProximityMask[iOtherClient] = bProximity;
			}
		}
	}

	// If this is different from what the client has, send an update. 
	if((gameRulesMask != g_SentGameRulesMasks[iClient] || 
		g_BanMasks[iClient] != g_SentBanMasks[iClient]))
	{
		g_SentGameRulesMasks[iClient] = gameRulesMask;
		g_SentBanMasks[iClient] = g_BanMasks[iClient];

		UserMessageBegin( user, "VoiceMask" );
			int dw;
			for(dw=0; dw < VOICE_MAX_PLAYERS_DW; dw++)
			{
				WRITE_LONG(gameRulesMask.GetDWord(dw));
				WRITE_LONG(g_BanMasks[dw].GetDWord(iClient));
			}
			WRITE_BYTE( !!(*g_PlayerModEnable)[iClient] );
		MessageEnd();
	}

	// Tell the engine.
	for(int iOtherClient=0; iOtherClient < g_pManager->m_nMaxPlayers; iOtherClient++)
	{
		bool bCanHear = gameRulesMask[iOtherClient] && !g_BanMasks[iOtherClient][iClient];
		g_pVoiceServer->SetClientListening( iOtherClient+1, iClient+1, bCanHear );

		if ( bCanHear )
		{
			g_pVoiceServer->SetClientProximity( iOtherClient+1, iClient+1, !!ProximityMask[iOtherClient] );
		}
	}

	g_fLastPlayerUpdated[iClient] = fTime;
	if (bIsTalking || (g_fLastPlayerTalked[iClient] + voicechat_stopdelay.GetFloat()) > fTime)
	{
		g_bIsPlayerTalking[iClient] = true;
	} else {
		g_bIsPlayerTalking[iClient] = bIsTalking;
	}

	if (g_pVoiceChatModule.InDebug() == 2)
	{
		Msg("Updated voice player! (%i, %s, %s, %f, %f, %f, %f)\n", iClient, bIsTalking ? "true" : "false", g_bIsPlayerTalking[iClient] ? "true" : "false", fTime, g_fLastPlayerTalked[iClient], (g_fLastPlayerTalked[iClient] + voicechat_stopdelay.GetFloat()), voicechat_stopdelay.GetFloat());
	}
}

static Detouring::Hook detour_CVoiceGameMgr_Update;
#if SYSTEM_WINDOWS && ARCHITECTURE_X86
static void __fastcall hook_CVoiceGameMgr_Update(double frametime)
{
	__asm {
		mov g_pManager, eax;
	}
#else
static void hook_CVoiceGameMgr_Update(CVoiceGameMgr* pManager, double frametime)
{
	g_pManager = pManager;
#endif
	g_pManager->m_UpdateInterval += frametime;
	if(g_pManager->m_UpdateInterval < voicechat_managerupdateinterval.GetFloat())
		return;

	if (g_pVoiceChatModule.InDebug() == 3)
	{
		Msg("Doing voice manager update!\n");
	}

	g_pManager->m_UpdateInterval = 0;
	for(int iClient=0; iClient < g_pManager->m_nMaxPlayers; iClient++)
	{
		CBaseEntity *pEnt = Util::GetCBaseEntityFromEdict(Util::engineserver->PEntityOfEntIndex(iClient + 1));
		if(!pEnt || !pEnt->IsPlayer())
			continue;

		CBasePlayer *pPlayer = (CBasePlayer*)pEnt;

		UpdatePlayerTalkingState(pPlayer, false);
	}
}

void CVoiceChatModule::ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
	for (int i = 0; i < ABSOLUTE_PLAYER_LIMIT; ++i)
	{
		g_fLastPlayerTalked[i] = 0.0;
		g_fLastPlayerUpdated[i] = 0.0;
		g_bIsPlayerTalking[i] = false;
	}
}

void CVoiceChatModule::LevelShutdown()
{
	for (int i = 0; i < ABSOLUTE_PLAYER_LIMIT; ++i)
	{
		g_fLastPlayerTalked[i] = 0.0;
		g_fLastPlayerUpdated[i] = 0.0;
		g_bIsPlayerTalking[i] = false;
	}
	g_pManager = NULL;
}

static Detouring::Hook detour_SV_BroadcastVoiceData;
static void hook_SV_BroadcastVoiceData(IClient* pClient, int nBytes, char* data, int64 xuid)
{
	VPROF_BUDGET("HolyLib - SV_BroadcastVoiceData", VPROF_BUDGETGROUP_HOLYLIB);

	if (g_pVoiceChatModule.InDebug() == 1)
		Msg("cl: %p\nbytes: %i\ndata: %p\n", pClient, nBytes, data);

#if SYSTEM_LINUX
	UpdatePlayerTalkingState(Util::GetPlayerByClient((CBaseClient*)pClient), true);
#endif

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

		Util::Push_Entity(g_Lua, (CBaseEntity*)Util::GetPlayerByClient((CBaseClient*)pClient));
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

		if (bHandled)
			return;
	}

	detour_SV_BroadcastVoiceData.GetTrampoline<Symbols::SV_BroadcastVoiceData>()(pClient, nBytes, data, xuid);
}

LUA_FUNCTION_STATIC(voicechat_SendEmptyData)
{
	CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true); // Should error if given invalid player.
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
	CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true); // Should error if given invalid player.
	IClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!\n");

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
		LUA->Push(1);
		LUA->PushNil();
		while (LUA->Next(-2))
		{
			CBasePlayer* pPlayer = Util::Get_Player(LUA, -1, true);
			CBaseClient* pClient = Util::GetClientByPlayer(pPlayer);
			if (!pClient)
				LUA->ThrowError("Failed to get CBaseClient!\n");

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

LUA_FUNCTION_STATIC(voicechat_ProcessVoiceData)
{
	CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true); // Should error if given invalid player.
	CBaseClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!\n");

	VoiceData* pData = Get_VoiceData(LUA, 2, true);

	if (!DETOUR_ISVALID(detour_SV_BroadcastVoiceData))
		LUA->ThrowError("Missing valid detour for SV_BroadcastVoiceData!\n");

	UpdatePlayerTalkingState(pPlayer, true);
	detour_SV_BroadcastVoiceData.GetTrampoline<Symbols::SV_BroadcastVoiceData>()(
		pClient, pData->GetLength(), pData->GetData(), 0
	);

	return 0;
}

LUA_FUNCTION_STATIC(voicechat_CreateVoiceData)
{
	int iPlayerSlot = (int)LUA->CheckNumberOpt(1, 0);
	const char* pStr = LUA->CheckStringOpt(2, NULL);
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
	CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true);
	IClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!\n");

	CBasePlayer* pTargetPlayer = Util::Get_Player(LUA, 2, true);
	IClient* pTargetClient = Util::GetClientByPlayer(pTargetPlayer);
	if (!pTargetClient)
		LUA->ThrowError("Failed to get CBaseClient for target player!\n");

	LUA->PushBool(pClient->IsHearingClient(pTargetClient->GetPlayerSlot()));

	return 1;
}

LUA_FUNCTION_STATIC(voicechat_IsProximityHearingClient)
{
	CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true);
	IClient* pClient = Util::GetClientByPlayer(pPlayer);
	if (!pClient)
		LUA->ThrowError("Failed to get CBaseClient!\n");

	CBasePlayer* pTargetPlayer = Util::Get_Player(LUA, 2, true);
	IClient* pTargetClient = Util::GetClientByPlayer(pTargetPlayer);
	if (!pTargetClient)
		LUA->ThrowError("Failed to get CBaseClient for target player!\n");

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

	WavAudioFile* pWavFile = NULL;
	VoiceStream* pStream = NULL;
	int iReference = -1; // A reference to the pStream to stop the GC from kicking in.
	int iCallback = -1;
	GarrysMod::Lua::ILuaInterface* pLua = NULL;
};

class LuaVoiceModuleData : public Lua::ModuleData
{
public:
	std::unordered_set<VoiceStreamTask*> pVoiceStreamTasks;
	std::unordered_set<VoiceEffects::VoiceEffectJob*> pVoiceEffectTasks;
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
					if (task->pStream == NULL)
						task->iStatus = VoiceStreamTaskStatus_FAILED_INVALID_FILE;
				} else {
					task->pStream = VoiceStream::Load(fh);
					if (task->pStream == NULL)
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

					//if (task->pWavFile == NULL)
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
			if (task->pStream == NULL)
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

	const char* pWaveData = LUA->CheckString(1);
	bool bAsync = LUA->IsType(2, GarrysMod::Lua::Type::Function);
	bool bPromiseToNeverModify = LUA->GetBool(3);

	VoiceStreamTask* task = new VoiceStreamTask;
	task->pWavFile = new WavAudioFile;
	task->iType = VoiceStreamTask_LOAD;
	task->pLua = LUA;

	int pWaveDataLength = LUA->ObjLen(1);
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
	int iClient = -1;
	if (LUA->IsType(1, GarrysMod::Lua::Type::Number))
	{
		iClient = LUA->GetNumber(1);
	} else {
		CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true);
		iClient = pPlayer->edict()->m_EdictIndex-1;
	}

	if (iClient < 0 || iClient > ABSOLUTE_PLAYER_LIMIT)
		LUA->ThrowError("Failed to get a valid Client index!");

	LUA->PushBool(g_bIsPlayerTalking[iClient]);
	return 1;
}

LUA_FUNCTION_STATIC(voicechat_LastPlayerTalked)
{
	int iClient = -1;
	if (LUA->IsType(1, GarrysMod::Lua::Type::Number))
	{
		iClient = LUA->GetNumber(1);
	} else {
		CBasePlayer* pPlayer = Util::Get_Player(LUA, 1, true);
		iClient = pPlayer->edict()->m_EdictIndex-1;
	}

	if (iClient < 0 || iClient > ABSOLUTE_PLAYER_LIMIT)
		LUA->ThrowError("Failed to get a valid Client index!");

	LUA->PushNumber(g_fLastPlayerTalked[iClient]);
	return 1;
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
	pJob->pEffect.type = VoiceEffects::Effects::None;
	if (bIsVoiceData)
	{
		pJob->pVoiceData = Get_VoiceData(LUA, 2, false);
	} else {
		pJob->pStreamData = Get_VoiceStream(LUA, 2, false);
	}

	LUA->GetField(1, "EffectName");
	const char* pEffectName = LUA->GetString(-1);
	if (V_stricmp(pEffectName, "Volume") == 0)
	{
		pJob->pEffect.type = VoiceEffects::Effects::Volume;

		LUA->GetField(1, "Volume");
		pJob->pEffect.data.volume = (float)LUA->GetNumber(-1);
		LUA->Pop(1);
	}
	LUA->Pop(1);

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

void CVoiceChatModule::LuaThink(GarrysMod::Lua::ILuaInterface* pLua)
{
	LuaVoiceModuleData* pData = GetVoiceChatLuaData(pLua);

	if (pData->pVoiceStreamTasks.size() <= 0)
		return;

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
		Util::AddFunc(pLua, VoiceData_GetTable, "GetTable");
		Util::AddFunc(pLua, VoiceData_IsValid, "IsValid");
		Util::AddFunc(pLua, VoiceData_GetData, "GetData");
		Util::AddFunc(pLua, VoiceData_GetLength, "GetLength");
		Util::AddFunc(pLua, VoiceData_GetPlayerSlot, "GetPlayerSlot");
		Util::AddFunc(pLua, VoiceData_SetData, "SetData");
		Util::AddFunc(pLua, VoiceData_SetLength, "SetLength");
		Util::AddFunc(pLua, VoiceData_SetPlayerSlot, "SetPlayerSlot");
		Util::AddFunc(pLua, VoiceData_GetUncompressedData, "GetUncompressedData");
		Util::AddFunc(pLua, VoiceData_SetUncompressedData, "SetUncompressedData");
		Util::AddFunc(pLua, VoiceData_GetProximity, "GetProximity");
		Util::AddFunc(pLua, VoiceData_SetProximity, "SetProximity");
		Util::AddFunc(pLua, VoiceData_CreateCopy, "CreateCopy");
	pLua->Pop(1);

	Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::VoiceStream, pLua->CreateMetaTable("VoiceStream"));
		Util::AddFunc(pLua, VoiceStream__tostring, "__tostring");
		Util::AddFunc(pLua, VoiceStream__index, "__index");
		Util::AddFunc(pLua, VoiceStream__newindex, "__newindex");
		Util::AddFunc(pLua, VoiceStream__gc, "__gc");
		Util::AddFunc(pLua, VoiceStream_GetTable, "GetTable");
		Util::AddFunc(pLua, VoiceStream_IsValid, "IsValid");
		Util::AddFunc(pLua, VoiceStream_GetData, "GetData");
		Util::AddFunc(pLua, VoiceStream_SetData, "SetData");
		Util::AddFunc(pLua, VoiceStream_GetCount, "GetCount");
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
		Util::AddFunc(pLua, WavAudioFile_GetTable, "GetTable");
	pLua->Pop(1);*/

	Util::StartTable(pLua);
		Util::AddFunc(pLua, voicechat_SendEmptyData, "SendEmptyData");
		Util::AddFunc(pLua, voicechat_SendVoiceData, "SendVoiceData");
		Util::AddFunc(pLua, voicechat_BroadcastVoiceData, "BroadcastVoiceData");
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
	Util::FinishTable(pLua, "voicechat");
}

void CVoiceChatModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "voicechat");
}

void CVoiceChatModule::Shutdown()
{
	V_DestroyThreadPool(pVoiceThreadPool);
	pVoiceThreadPool = NULL;
}

IVoiceServer* g_pVoiceServer = NULL;
void CVoiceChatModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	if (appfn[0])
	{
		g_pVoiceServer = (IVoiceServer*)appfn[0](INTERFACEVERSION_VOICESERVER, NULL);
	} else {
		SourceSDK::FactoryLoader engine_loader("engine");
		g_pVoiceServer = engine_loader.GetInterface<IVoiceServer>(INTERFACEVERSION_VOICESERVER);
	}

	Detour::CheckValue("get interface", "g_pVoiceServer", g_pVoiceServer != NULL);
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

#if SYSTEM_LINUX
	SourceSDK::FactoryLoader server_loader("server");
	Detour::Create(
		&detour_CVoiceGameMgr_Update, "CVoiceGameMgr::Update",
		server_loader.GetModule(), Symbols::CVoiceGameMgr_UpdateSym,
		(void*)hook_CVoiceGameMgr_Update, m_pID
	);

	g_PlayerModEnable = Detour::ResolveSymbol<CPlayerBitVec>(server_loader, Symbols::g_PlayerModEnableSym);
	Detour::CheckValue("get class", "g_PlayerModEnable", g_PlayerModEnable != NULL);

	g_BanMasks = Detour::ResolveSymbol<CPlayerBitVec>(server_loader, Symbols::g_BanMasksSym);
	Detour::CheckValue("get class", "g_BanMasks", g_BanMasks != NULL);

	g_SentGameRulesMasks = Detour::ResolveSymbol<CPlayerBitVec>(server_loader, Symbols::g_SentGameRulesMasksSym);
	Detour::CheckValue("get class", "g_SentGameRulesMasks", g_SentGameRulesMasks != NULL);

	g_SentBanMasks = Detour::ResolveSymbol<CPlayerBitVec>(server_loader, Symbols::g_SentBanMasksSym);
	Detour::CheckValue("get class", "g_SentBanMasks", g_SentBanMasks != NULL);

	g_bWantModEnable = Detour::ResolveSymbol<CPlayerBitVec>(server_loader, Symbols::g_bWantModEnableSym);
	Detour::CheckValue("get class", "g_bWantModEnable", g_bWantModEnable != NULL);
#endif
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