#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "sourcesdk/cgmod_audio.h"
#include "edict.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CBassModule : public IModule
{
public:
	void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) override;
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void Shutdown() override;
	void Think(bool bSimulating) override;
	const char* Name() override { return "bass"; };
	int Compatibility() override { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
	bool IsEnabledByDefault() override { return true; };
	bool SupportsMultipleLuaStates() override { return true; };
};

static CBassModule g_pBassModule;
IModule* pBassModule = &g_pBassModule;

static IGMod_Audio* gGModAudio;
Push_LuaClass(IGModAudioChannel)
Get_LuaClass(IGModAudioChannel, "IGModAudioChannel")

LUA_FUNCTION_STATIC(IGModAudioChannel__tostring)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, false);
	if (!channel)
	{
		LUA->PushString("IGModAudioChannel [NULL]");
		return 1;
	}

	char szBuf[128] = {};
	V_snprintf(szBuf, sizeof(szBuf), "IGModAudioChannel [%s]", channel->GetFileName()); 
	LUA->PushString(szBuf);
	return 1;
}

Default__index(IGModAudioChannel);
Default__newindex(IGModAudioChannel);
Default__GetTable(IGModAudioChannel);
Default__gc(IGModAudioChannel,
	CGModAudioChannel* channel = (CGModAudioChannel*)pStoredData;
	if (channel)
		delete channel;
)

LUA_FUNCTION_STATIC(IGModAudioChannel_Destroy)
{
	LuaUserData* pLuaData = Get_IGModAudioChannel_Data(LUA, 1, true);
	IGModAudioChannel* channel = (IGModAudioChannel*)pLuaData->GetData();

	channel->Destroy();
	pLuaData->Release(LUA);

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_Stop)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	channel->Stop();

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_Pause)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	channel->Pause();

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_Play)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	channel->Play();

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_SetVolume)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	double volume = LUA->CheckNumber(2);
	channel->SetVolume((float)volume);

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetVolume)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushNumber(channel->GetVolume());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_SetPlaybackRate)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	double rate = LUA->CheckNumber(2);
	channel->SetPlaybackRate((float)rate);

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetPlaybackRate)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushNumber(channel->GetPlaybackRate());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_SetTime)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	double time = LUA->CheckNumber(2);
	bool dontDecode = LUA->GetBool(3);
	channel->SetTime(time, dontDecode);

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetTime)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushNumber(channel->GetTime());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetBufferedTime)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushNumber(channel->GetBufferedTime());
	return 1;
}

// We don't need Set/Get3DFadeDistance and Set/Get3DCone

LUA_FUNCTION_STATIC(IGModAudioChannel_GetState)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushNumber(channel->GetState());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_SetLooping)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	channel->SetLooping(LUA->GetBool(2));

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_IsLooping)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushBool(channel->IsLooping());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_IsOnline)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushBool(channel->IsOnline());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_Is3D)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushBool(channel->Is3D());

	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_IsBlockStreamed)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushBool(channel->IsBlockStreamed());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_IsValid)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, false);

	LUA->PushBool(channel && channel->IsValid());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetLength)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushNumber(channel->GetLength());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetFileName)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushString(channel->GetFileName());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetSamplingRate)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushNumber(channel->GetSamplingRate());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetBitsPerSample)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushNumber(channel->GetBitsPerSample());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetAverageBitRate)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushNumber(channel->GetAverageBitRate());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetLevel)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	float left, right = 0;
	channel->GetLevel(&left, &right);

	LUA->PushNumber(left);
	LUA->PushNumber(right);
	return 2;
}

// ToDo: Finish the function below!
LUA_FUNCTION_STATIC(IGModAudioChannel_FFT)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->CheckType(2, GarrysMod::Lua::Type::Table);
	int fft = (int)LUA->CheckNumber(3);

	if (fft > 7)
		fft = 7;

	if (fft < 0)
		fft = 0;

	int size = 1 << (8 + fft);
	float* fft2 = new float[size];
	channel->FFT(fft2, (GModChannelFFT_t)fft);
	
	LUA->Push(2);
		for (int idx = 0; idx < size; ++idx)
		{
			LUA->PushNumber(fft2[idx]);
			Util::RawSetI(LUA, -2, idx+1);
		}
	LUA->Pop(1);

	delete[] fft2;

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_SetChannelPan)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	float pan = (float)LUA->CheckNumber(2);
	channel->SetChannelPan(pan);

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetChannelPan)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushNumber(channel->GetChannelPan());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetTags)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	int unknown = (int)LUA->CheckNumber(2);

	LUA->PushString(channel->GetTags(unknown));
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_NotImplemented)
{
	LUA->ThrowError("Won't be implemented!");
	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_Restart)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	channel->Restart();

	return 0;
}

class BassEncoderCallback : public IGModEncoderCallback
{
public:
	BassEncoderCallback(GarrysMod::Lua::ILuaInterface* pLua)
	{
		m_pLua = pLua;
		m_nCallbackReference = Util::ReferenceCreate(pLua, "BassEncoderCallback - callback reference");
	}

	~BassEncoderCallback() override
	{
		if (m_pLua && m_nCallbackReference != -1)
		{
			// Can happen if an error for example occurs inside WriteToDisk
			DevMsg("BassEncoderCallback deleted while still holding a reference!\n");
			Util::ReferenceFree(m_pLua, m_nCallbackReference, "BassEncoderCallback - callback leftover deletion");
			m_nCallbackReference = -1;
			m_pLua = nullptr;
		}
	};

	bool ShouldForceFinish(IGModAudioChannelEncoder* pEncoder, void* nSignalData) override
	{
		return m_pLua == nSignalData; // Force finish as this is our signal that our interface is shutting down!
	};

	void OnFinish(IGModAudioChannelEncoder* pEncoder, GModEncoderStatus nStatus) override
	{
		if (m_nCallbackReference == -1)
			return;

		Util::ReferencePush(m_pLua, m_nCallbackReference);
		if (nStatus == GModEncoderStatus::FINISHED) {
			m_pLua->PushBool(true);
			m_pLua->PushNil();
		} else {
			m_pLua->PushBool(false);
			m_pLua->PushString("Encoder was interrupted by Lua shutdown!");
		}
		m_pLua->CallFunctionProtected(2, 0, true);

		Util::ReferenceFree(m_pLua, m_nCallbackReference, "BassEncoderCallback - callback deletion OnFinish");
		m_nCallbackReference = -1;
		m_pLua = nullptr;
	}

	bool OnServerClient(IGModAudioChannelEncoder* pEncoder, bool connect, const char* client, char* headers) override
	{
		return true;
	};

private:
	GarrysMod::Lua::ILuaInterface* m_pLua = nullptr;
	int m_nCallbackReference = -1;
};

LUA_FUNCTION_STATIC(IGModAudioChannel_WriteToDisk)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	const char* pFileName = LUA->CheckString(2);
	// NOTE: Next time ensure I fucking use CheckNumber and not CheckString to then cast :sob: only took 8+ hours to figure out
	unsigned long nFlags = (unsigned long)LUA->CheckNumber(3);
	LUA->CheckType(4, GarrysMod::Lua::Type::Function);
	bool bAsync = LUA->GetBool(5);

	LUA->Push(4);
	BassEncoderCallback* pCallback = new BassEncoderCallback(LUA);
	// We do not manage this pointer! GModAudio does for us

	const char* pErrorMsg = nullptr;
	IGModAudioChannelEncoder* pEncoder = channel->CreateEncoder(pFileName, nFlags, pCallback, &pErrorMsg);
	if (pErrorMsg)
	{
		LUA->PushBool(false);
		LUA->PushString(pErrorMsg);
		return 2;
	}

	pEncoder->ProcessNow(bAsync);
	if (pEncoder->GetLastError(&pErrorMsg))
	{
		LUA->PushBool(false);
		LUA->PushString(pErrorMsg);
		return 2;
	}

	LUA->PushBool(true);
	LUA->PushNil();
	return 2;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_Update)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	channel->Update(LUA->CheckNumber(2));

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_CreateLink)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	IGModAudioChannel* otherChannel = Get_IGModAudioChannel(LUA, 2, true);

	const char* pError = nullptr;
	LUA->PushBool(channel->CreateLink(otherChannel, &pError));
	if (pError) {
		LUA->PushString(pError);
	} else {
		LUA->PushNil();
	}

	return 2;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_DestroyLink)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	IGModAudioChannel* otherChannel = Get_IGModAudioChannel(LUA, 2, true);

	const char* pError = nullptr;
	LUA->PushBool(channel->DestroyLink(otherChannel, &pError));
	if (pError) {
		LUA->PushString(pError);
	} else {
		LUA->PushNil();
	}

	return 2;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_SetAttribute)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	unsigned long nAttribute = (unsigned long)LUA->CheckNumber(2);
	float nValue = (float)LUA->CheckNumber(3);

	const char* pError = nullptr;
	channel->SetAttribute(nAttribute, nValue, &pError);
	LUA->PushBool(pError == nullptr);
	if (pError) {
		LUA->PushString(pError);
	} else {
		LUA->PushNil();
	}
	return 2;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_SetSlideAttribute)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	unsigned long nAttribute = (unsigned long)LUA->CheckNumber(2);
	float nValue = (float)LUA->CheckNumber(3);
	unsigned long nTime = (unsigned long)LUA->CheckNumber(4);

	const char* pError = nullptr;
	channel->SetSlideAttribute(nAttribute, nValue, nTime, &pError);
	LUA->PushBool(pError == nullptr);
	if (pError) {
		LUA->PushString(pError);
	} else {
		LUA->PushNil();
	}
	return 2;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetAttribute)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	unsigned long nAttribute = (unsigned long)LUA->CheckNumber(2);

	const char* pError = nullptr;
	float nValue = channel->GetAttribute(nAttribute, &pError);
	if (pError) {
		LUA->PushNil();
		LUA->PushString(pError);
	} else {
		LUA->PushNumber(nValue);
		LUA->PushNil();
	}
	return 2;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_IsAttributeSliding)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	unsigned long nAttribute = (unsigned long)LUA->CheckNumber(2);


	LUA->PushBool(channel->IsAttributeSliding(nAttribute));
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetChannelData)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	unsigned short nSize = (unsigned short)LUA->CheckNumber(2);

	// Max 64kb!
	if (nSize > USHRT_MAX)
		nSize = USHRT_MAX;

	void* pBuffer = alloca(nSize);
	unsigned long nLength = channel->GetChannelData(pBuffer, nSize | BASS_DATA_FLOAT);
	if (nLength == -1) {
		LUA->PushNil();
	} else {
		LUA->PushString((char*)pBuffer, nLength);
	}
	LUA->PushNumber(nLength);
	return 2;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetChannelCount)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	const char* pError = nullptr;
	int nChannelCount = channel->GetChannelCount(&pError);
	if (pError)
	{
		LUA->PushNil();
		LUA->PushString(pError);
	} else {
		LUA->PushNumber(nChannelCount);
		LUA->PushNil();
	}
	return 2;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_SetFX)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	const char* pFXName = LUA->CheckString(2);
	int nFXType = (int)LUA->CheckNumber(3);
	int nPriority = (int)LUA->CheckNumber(4);
	LUA->CheckType(5, GarrysMod::Lua::Type::Table);

#define GETFXFLOATFIELD(name) \
	LUA->PushString(#name); \
	LUA->RawGet(5); \
	if (LUA->IsType(-1, GarrysMod::Lua::Type::Number)) { \
		pParams.name = (float)LUA->GetNumber(-1); \
	} \
	LUA->Pop(1);

#define GETFXDWORDFIELD(name) \
	LUA->PushString(#name); \
	LUA->RawGet(5); \
	if (LUA->IsType(-1, GarrysMod::Lua::Type::Number)) { \
		pParams.name = (DWORD)LUA->GetNumber(-1); \
	} \
	LUA->Pop(1);

#define GETFXBOOLFIELD(name) \
	LUA->PushString(#name); \
	LUA->RawGet(5); \
	if (LUA->IsType(-1, GarrysMod::Lua::Type::Bool)) { \
		pParams.name = LUA->GetBool(-1); \
	} \
	LUA->Pop(1);

	if (nFXType >= (int)BassFX::FX_MAX || nFXType < 0)
		LUA->ThrowError("Unknown FX type! Use one of the bass.FX_ enums!");

	const char* pError = nullptr;
	switch((BassFX)nFXType)
	{
	case BassFX::FX_CHORUS:
		{
			BASS_DX8_CHORUS pParams;
			GETFXFLOATFIELD(fWetDryMix);
			GETFXFLOATFIELD(fDepth);
			GETFXFLOATFIELD(fFeedback);
			GETFXFLOATFIELD(fFrequency);
			GETFXDWORDFIELD(lWaveform);
			GETFXFLOATFIELD(fDelay);
			GETFXDWORDFIELD(lPhase);
			channel->SetFX(pFXName, BASS_FX_DX8_CHORUS, nPriority, &pParams, &pError);
		}
		break;
	case BassFX::FX_DISTORTION:
		{
			BASS_DX8_DISTORTION pParams;
			GETFXFLOATFIELD(fGain);
			GETFXFLOATFIELD(fEdge);
			GETFXFLOATFIELD(fPostEQCenterFrequency);
			GETFXFLOATFIELD(fPostEQBandwidth);
			GETFXFLOATFIELD(fPreLowpassCutoff);
			channel->SetFX(pFXName, BASS_FX_DX8_DISTORTION, nPriority, &pParams, &pError);
		}
		break;
	case BassFX::FX_ECHO:
		{
			BASS_DX8_ECHO pParams;
			GETFXFLOATFIELD(fWetDryMix);
			GETFXFLOATFIELD(fFeedback);
			GETFXFLOATFIELD(fLeftDelay);
			GETFXFLOATFIELD(fRightDelay);
			GETFXBOOLFIELD(lPanDelay);
			channel->SetFX(pFXName, BASS_FX_DX8_ECHO, nPriority, &pParams, &pError);
		}
		break;
	case BassFX::FX_FLANGER:
		{
			BASS_DX8_FLANGER pParams;
			GETFXFLOATFIELD(fWetDryMix);
			GETFXFLOATFIELD(fDepth);
			GETFXFLOATFIELD(fFeedback);
			GETFXFLOATFIELD(fFrequency);
			GETFXDWORDFIELD(lWaveform);
			GETFXFLOATFIELD(fDelay);
			GETFXDWORDFIELD(lPhase);
			channel->SetFX(pFXName, BASS_FX_DX8_FLANGER, nPriority, &pParams, &pError);
		}
		break;
	case BassFX::FX_PARAMEQ:
		{
			BASS_DX8_PARAMEQ pParams;
			GETFXFLOATFIELD(fCenter);
			GETFXFLOATFIELD(fBandwidth);
			GETFXFLOATFIELD(fGain);
			channel->SetFX(pFXName, BASS_FX_DX8_PARAMEQ, nPriority, &pParams, &pError);
		}
		break;
	case BassFX::FX_REVERB:
		{
			BASS_DX8_REVERB pParams;
			GETFXFLOATFIELD(fInGain);
			GETFXFLOATFIELD(fReverbMix);
			GETFXFLOATFIELD(fReverbTime);
			GETFXFLOATFIELD(fHighFreqRTRatio);
			channel->SetFX(pFXName, BASS_FX_DX8_REVERB, nPriority, &pParams, &pError);
		}
		break;
	default:
		LUA->ThrowError("Unknown FX type? How! Use one of the bass.FX_ enums!");
	}

#undef GETFXFLOATFIELD
#undef GETFXDWORDFIELD
#undef GETFXBOOLFIELD

	LUA->PushBool(pError == nullptr);
	if (pError) {
		LUA->PushString(pError);
	} else {
		LUA->PushNil();
	}

	return 2;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_ResetFX)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	const char* pFXName = LUA->CheckString(2);

	LUA->PushBool(channel->FXReset(pFXName));
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_RemoveFX)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	const char* pFXName = LUA->CheckString(2);
	
	LUA->PushBool(channel->FXFree(pFXName));
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_IsPush)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushBool(channel->IsPush());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_InsertVoiceData)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	if (!channel->IsPush())
		LUA->ThrowError("Tried to insert data into a non-push channel!");

#if MODULE_EXISTS_VOICECHAT
	VoiceData* pVoiceData = Get_VoiceData(LUA, 2, true);
	extern char* VoiceData_GetDecompressedData(VoiceData* pData, int* pLength); // exposed for us :3

	int nLength = -1;
	char* pRawData = VoiceData_GetDecompressedData(pVoiceData, &nLength);
	if (!pRawData)
	{
		LUA->PushBool(false);
		LUA->PushNil();
		return 2;
	}

	const char* pError = nullptr;
	channel->WriteData(pRawData, nLength, &pError);

	LUA->PushBool(pError == nullptr);
	if (pError) {
		LUA->PushString(pError);
	} else {
		LUA->PushNil();
	}
	return 2;
#else
	MISSING_MODULE_ERROR(LUA, voicechat);
	return 0;
#endif
}

LUA_FUNCTION_STATIC(IGModAudioChannel_FeedEmpty)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	int durationMs = (int)LUA->CheckNumber(2);
	int sampleRate = (int)LUA->CheckNumber(3);
	int channels = (int)LUA->CheckNumber(4);

	if (!channel->IsPush())
		LUA->ThrowError("Tried to insert data into a non-push channel!");

	int nSamples = (sampleRate * durationMs) / 1000;
	int nBytes = nSamples * channels * sizeof(short);
	if (nBytes > 50000) // More than 50kb stackalloc? Hmmm... what are you doing...
	{
		LUA->PushBool(false);
		LUA->PushNil();
		return 2;
	}

	char* pSilence = (char*)_alloca(nBytes);
	memset(pSilence, 0, nBytes);
	
	const char* pError = nullptr;
	channel->WriteData(pSilence, nBytes, &pError);
	LUA->PushBool(pError == nullptr);
	if (pError) {
		LUA->PushString(pError);
	} else {
		LUA->PushNil();
	}

	return 2;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_FeedData)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	size_t nLength = (size_t)-1;
	const char* pData = Util::CheckLString(LUA, 2, &nLength);

	if (!channel->IsPush())
		LUA->ThrowError("Tried to insert data into a non-push channel!");

	const char* pError = nullptr;
	channel->WriteData(pData, nLength, &pError);
	LUA->PushBool(pError == nullptr);
	if (pError) {
		LUA->PushString(pError);
	} else {
		LUA->PushNil();
	}
	return 2;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_IsMixer)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushBool(channel->IsMixer());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_AddMixerChannel)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);
	IGModAudioChannel* otherChannel = Get_IGModAudioChannel(LUA, 2, true);
	unsigned long nFlags = (unsigned long)LUA->CheckNumberOpt(3, 0);

	if (!channel->IsMixer())
		LUA->ThrowError("Tried to call this on a non-mixer channel!");

	const char* pError = nullptr;
	channel->AddMixerChannel(otherChannel, nFlags, &pError);
	LUA->PushBool(pError == nullptr);
	if (pError) {
		LUA->PushString(pError);
	} else {
		LUA->PushNil();
	}
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_RemoveMixerChannel)
{
	IGModAudioChannel* pChannel = Get_IGModAudioChannel(LUA, 1, true);

	pChannel->RemoveMixerChannel();
	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetMixerState)
{
	IGModAudioChannel* pChannel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushNumber(pChannel->GetMixerState());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_SetMixerMatrix)
{
	IGModAudioChannel* pChannel = Get_IGModAudioChannel(LUA, 1, true);
	LUA->CheckType(2, GarrysMod::Lua::Type::Table);
	float fTime = (float)LUA->CheckNumberOpt(3, 0.0f);

	const char* pError = nullptr;
	int srcChans = pChannel->GetChannelCount(&pError);
	if (pError)
	{
		LUA->PushBool(false);
		LUA->PushString(pError);
		return 2;
	}

	int dstChans = pChannel->GetMixerChannelCount(&pError);
	if (pError)
	{
		LUA->PushBool(false);
		LUA->PushString(pError);
		return 2;
	}

	std::vector<float> matrix(srcChans * dstChans, 0.0f);
	for (int i = 0; i < srcChans; ++i)
	{
		LUA->Push(2);
		LUA->PushNumber(i + 1);
		LUA->GetTable(-2);

		if (LUA->IsType(-1, GarrysMod::Lua::Type::Table))
		{
			for (int j = 0; j < dstChans; ++j)
			{
				LUA->PushNumber(j + 1);
				LUA->GetTable(-2);

				if (LUA->IsType(-1, GarrysMod::Lua::Type::Number))
					matrix[i * dstChans + j] = (float)LUA->GetNumber(-1);

				LUA->Pop(1);
			}
		}
		else if (LUA->IsType(-1, GarrysMod::Lua::Type::Number) && dstChans == 1)
		{
			matrix[i] = (float)LUA->GetNumber(-1);
		}

		LUA->Pop(2);
	}

	const char* result = pChannel->SetMatrix(matrix.data(), fTime);
	if (result)
	{
		LUA->PushBool(false);
		LUA->PushString(result);
		return 2;
	}

	LUA->PushBool(true);
	LUA->PushNil();
	return 2;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetMixerChannelCount)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	const char* pError = nullptr;
	int nChannelCount = channel->GetMixerChannelCount(&pError);
	if (pError)
	{
		LUA->PushNil();
		LUA->PushString(pError);
	} else {
		LUA->PushNumber(nChannelCount);
		LUA->PushNil();
	}
	return 2;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_IsSplitter)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	LUA->PushBool(channel->IsSplitter());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_ResetSplitStream)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	if (!channel->IsSplitter())
		LUA->ThrowError("Tried to call this on a non-split channel!");

	channel->ResetSplitStream();
	return 0;
}

PushReferenced_LuaClass(IGModAudioChannelEncoder)
Get_LuaClass(IGModAudioChannelEncoder, "IGModAudioChannelEncoder")

Default__index(IGModAudioChannelEncoder);
Default__newindex(IGModAudioChannelEncoder);
Default__GetTable(IGModAudioChannelEncoder);
Default__gc(IGModAudioChannelEncoder,
	IGModAudioChannelEncoder* channel = (IGModAudioChannelEncoder*)pStoredData;
	if (channel)
		delete channel;
)

LUA_FUNCTION_STATIC(IGModAudioChannelEncoder__tostring)
{
	IGModAudioChannelEncoder* encoder = Get_IGModAudioChannelEncoder(LUA, 1, false);
	if (!encoder)
	{
		LUA->PushString("IGModAudioChannelEncoder [NULL]");
		return 1;
	}

	char szBuf[128] = {};
	V_snprintf(szBuf, sizeof(szBuf), "IGModAudioChannelEncoder [%s]", encoder->GetFileName()); 
	LUA->PushString(szBuf);
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannelEncoder_IsValid)
{
	IGModAudioChannelEncoder* encoder = Get_IGModAudioChannelEncoder(LUA, 1, false);

	LUA->PushBool(encoder != nullptr);
	return 1;
}

class BassEncoderDeletionCallback : public IGModEncoderCallback
{
public:
	BassEncoderDeletionCallback(GarrysMod::Lua::ILuaInterface* pLua)
	{
		m_pLua = pLua;
	}

	~BassEncoderDeletionCallback() override
	{
		if (nServerClientCallback != -1)
		{
			Util::ReferenceFree(m_pLua, nServerClientCallback, "BassEncoderDeletionCallback - server client callback");
			nServerClientCallback = -1;
		}
	}

	bool ShouldForceFinish(IGModAudioChannelEncoder* pEncoder, void* nSignalData) override
	{
		return m_pLua == nSignalData;
	};

	void OnFinish(IGModAudioChannelEncoder* pEncoder, GModEncoderStatus nStatus) override
	{
		// The IGModAudioChannelEncoder deletes itself, so we gotta ensure no invalid pointers are left in Lua
		Delete_IGModAudioChannelEncoder(m_pLua, pEncoder);

		if (nServerClientCallback != -1)
		{
			Util::ReferenceFree(m_pLua, nServerClientCallback, "BassEncoderDeletionCallback - server client callback");
			nServerClientCallback = -1;
		}
	}

	bool OnServerClient(IGModAudioChannelEncoder* pEncoder, bool connect, const char* client, char headers[1024]) override
	{
		if (nServerClientCallback == -1)
			return true;

		m_pLua->ReferencePush(nServerClientCallback);
		m_pLua->PushBool(connect);
		m_pLua->PushString(client);
		if (m_pLua->CallFunctionProtected(2, 2, true))
		{
			bool bRet = m_pLua->GetBool(-1);
			unsigned int nLength;
			const char* pHeaders = m_pLua->GetString(-2, &nLength);

			if (pHeaders)
			{
				if (nLength > 1022)
					nLength = 1022;

				V_strncpy(headers, pHeaders, nLength);
			}

			return bRet;
		}

		return true;
	}

public:
	GarrysMod::Lua::ILuaInterface* m_pLua;
	int nServerClientCallback = -1;
};

LUA_FUNCTION_STATIC(IGModAudioChannelEncoder_ServerInit)
{
	IGModAudioChannelEncoder* encoder = Get_IGModAudioChannelEncoder(LUA, 1, true);

	const char* strPort = LUA->CheckString(2);
	unsigned long nBuffer = (unsigned long)LUA->CheckNumber(3);
	unsigned long nBurst = (unsigned long)LUA->CheckNumber(4);
	unsigned long nFlags = (unsigned long)LUA->CheckNumber(5);

	const char* pErrorCode = nullptr;
	LUA->PushBool(encoder->ServerInit(strPort, nBuffer, nBurst, nFlags, &pErrorCode));
	if (pErrorCode) {
		LUA->PushString(pErrorCode);
	} else {
		LUA->PushNil();
	}
	return 2;
}

LUA_FUNCTION_STATIC(IGModAudioChannelEncoder_ServerKick)
{
	IGModAudioChannelEncoder* encoder = Get_IGModAudioChannelEncoder(LUA, 1, true);
	const char* pClient = LUA->CheckString(2);

	LUA->PushBool(encoder->ServerKick(pClient));
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannelEncoder_SetServerCallback)
{
	IGModAudioChannelEncoder* encoder = Get_IGModAudioChannelEncoder(LUA, 1, true);
	LUA->CheckType(2, GarrysMod::Lua::Type::Function);

	BassEncoderDeletionCallback* pCallback = (BassEncoderDeletionCallback*)encoder->GetCallback();
	if (!pCallback) // Chance to happen: 0.0% (unless memory corruption, but at that point GG)
		LUA->ThrowError("Somehow our encoder has no callback? WHO ATE IT >:( I blame Lumi");

	if (pCallback->nServerClientCallback != -1)
		Util::ReferenceFree(LUA, pCallback->nServerClientCallback, "IGModAudioChannelEncoder:SetServerCallback - old callback");

	LUA->Push(2);
	pCallback->nServerClientCallback = Util::ReferenceCreate(LUA, "IGModAudioChannelEncoder:SetServerCallback - new callback");
	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_CreateEncoder)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	const char* pFileName = LUA->CheckString(2);
	// NOTE: Next time ensure I fucking use CheckNumber and not CheckString to then cast :sob: only took 8+ hours to figure out
	unsigned long nFlags = (unsigned long)LUA->CheckNumberOpt(3, 0);

	BassEncoderDeletionCallback* pCallback = new BassEncoderDeletionCallback(LUA);
	// We do not manage this pointer! GModAudio does for us

	const char* pErrorMsg = nullptr;
	IGModAudioChannelEncoder* pEncoder = channel->CreateEncoder(pFileName, nFlags, pCallback, &pErrorMsg);
	if (pErrorMsg)
	{
		LUA->PushBool(false);
		LUA->PushString(pErrorMsg);
		return 2;
	}

	Push_IGModAudioChannelEncoder(LUA, pEncoder);
	LUA->PushNil();
	return 2;
}

LUA_FUNCTION_STATIC(IGModAudioChannelEncoder_SetPaused)
{
	IGModAudioChannelEncoder* encoder = Get_IGModAudioChannelEncoder(LUA, 1, true);

	encoder->SetPaused(LUA->GetBool(2));
	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannelEncoder_GetState)
{
	IGModAudioChannelEncoder* encoder = Get_IGModAudioChannelEncoder(LUA, 1, true);

	LUA->PushNumber(encoder->GetState());
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannelEncoder_SetChannel)
{
	IGModAudioChannelEncoder* encoder = Get_IGModAudioChannelEncoder(LUA, 1, true);
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 2, true);

	const char* pErrorCode = nullptr;
	encoder->SetChannel(channel, &pErrorCode);
	LUA->PushBool(pErrorCode == nullptr);
	if (pErrorCode) {
		LUA->PushString(pErrorCode);
	} else {
		LUA->PushNil();
	}
	return 2;
}

LUA_FUNCTION_STATIC(IGModAudioChannelEncoder_InsertVoiceData)
{
	IGModAudioChannelEncoder* encoder = Get_IGModAudioChannelEncoder(LUA, 1, true);

#if MODULE_EXISTS_VOICECHAT
	VoiceData* pVoiceData = Get_VoiceData(LUA, 2, true);
	extern char* VoiceData_GetDecompressedData(VoiceData* pData, int* pLength); // exposed for us :3

	int nLength = -1;
	char* pRawData = VoiceData_GetDecompressedData(pVoiceData, &nLength);
	if (!pRawData)
	{
		LUA->PushBool(false);
		return 1;
	}

	encoder->WriteData(pRawData, nLength);
#else
	MISSING_MODULE_ERROR(LUA, voicechat);
#endif

	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannelEncoder_FeedEmpty)
{
	IGModAudioChannelEncoder* encoder = Get_IGModAudioChannelEncoder(LUA, 1, true);
	int durationMs = (int)LUA->CheckNumber(2);
	int sampleRate = (int)LUA->CheckNumber(3);
	int channels = (int)LUA->CheckNumber(4);

	int nSamples = (sampleRate * durationMs) / 1000;
	int nBytes = nSamples * channels * sizeof(short);
	if (nBytes > 50000) // More than 50kb stackalloc? Hmmm... what are you doing...
	{
		LUA->PushBool(false);
		return 1;
	}

	char* pSilence = (char*)_alloca(nBytes);
	memset(pSilence, 0, nBytes);
	encoder->WriteData(pSilence, nBytes);

	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannelEncoder_FeedData)
{
	IGModAudioChannelEncoder* encoder = Get_IGModAudioChannelEncoder(LUA, 1, true);
	size_t nLength = (size_t)-1;
	const char* pData = Util::CheckLString(LUA, 2, &nLength);

	encoder->WriteData(pData, nLength);

	LUA->PushBool(true);
	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannelEncoder_CastInit)
{
	IGModAudioChannelEncoder* encoder = Get_IGModAudioChannelEncoder(LUA, 1, true);
	const char* pServer = LUA->CheckString(2);
	const char* pPassword = LUA->CheckString(3);
	const char* pContent = LUA->CheckString(4);
	const char* pName = LUA->CheckStringOpt(5, nullptr);
	const char* pURL = LUA->CheckStringOpt(6, nullptr);
	const char* pGenre = LUA->CheckStringOpt(7, nullptr);
	const char* pDesc = LUA->CheckStringOpt(8, nullptr);
	const char* pHeaders = LUA->CheckStringOpt(9, nullptr);
	int nBitRate = (int)LUA->CheckNumberOpt(10, 0);
	unsigned long nFlags = (unsigned long)LUA->CheckNumberOpt(11, 0);

	char headers[4096];
	if (pHeaders)
		V_strncpy(headers, pHeaders, sizeof(headers));

	const char* pErrorCode = nullptr;
	encoder->CastInit(pServer, pPassword, pContent, pName, pURL, pGenre, pDesc, pHeaders ? headers : nullptr, nBitRate, nFlags, &pErrorCode);
	LUA->PushBool(pErrorCode == nullptr);
	if (pErrorCode) {
		LUA->PushString(pErrorCode);
	} else {
		LUA->PushNil();
	}
	return 2;
}

LUA_FUNCTION_STATIC(IGModAudioChannelEncoder_CastSetTitle)
{
	IGModAudioChannelEncoder* encoder = Get_IGModAudioChannelEncoder(LUA, 1, true);
	const char* pTitle = LUA->CheckStringOpt(2, nullptr);
	const char* pURL = LUA->CheckStringOpt(3, nullptr);

	encoder->CastSetTitle(pTitle, pURL);
	return 0;
}

LUA_FUNCTION_STATIC(bass_PlayFile)
{
	const char* filePath = LUA->CheckString(1);
	const char* flags = LUA->CheckString(2);
	LUA->CheckType(3, GarrysMod::Lua::Type::Function);

	if (!gGModAudio)
		LUA->ThrowError("We don't have gGModAudio!");

	int errorCode = 0;
	IGModAudioChannel* audioChannel = gGModAudio->PlayFile(filePath, flags, &errorCode);
	
	LUA->Push(3);
		if (errorCode == 0)
			Push_IGModAudioChannel(LUA, audioChannel);
		else
			LUA->PushNil();
		LUA->PushNumber(errorCode);
		if (errorCode != 0)
			LUA->PushString(gGModAudio->GetErrorString(errorCode));
		else
			LUA->PushNil();
	LUA->CallFunctionProtected(3, 0, true);

	return 0;
}

LUA_FUNCTION_STATIC(bass_PlayURL)
{
	const char* url = LUA->CheckString(1);
	const char* flags = LUA->CheckString(2);
	LUA->CheckType(3, GarrysMod::Lua::Type::Function);

	if (!gGModAudio)
		LUA->ThrowError("We don't have gGModAudio!");

	int errorCode = 0;
	IGModAudioChannel* audioChannel = gGModAudio->PlayURL(url, flags, &errorCode);
	
	LUA->Push(3);
		if (errorCode == 0)
			Push_IGModAudioChannel(LUA, audioChannel);
		else
			LUA->PushNil();
		LUA->PushNumber(errorCode);
		if (errorCode != 0)
			LUA->PushString(gGModAudio->GetErrorString(errorCode));
		else
			LUA->PushNil();
	LUA->CallFunctionProtected(3, 0, true);

	return 0;
}

extern CGlobalVars* gpGlobals;
LUA_FUNCTION_STATIC(bass_Update)
{
	LUA->PushBool(gGModAudio->Update((int)LUA->CheckNumberOpt(1, gpGlobals->absoluteframetime * 1000)));
	return 1;
}

LUA_FUNCTION_STATIC(bass_GetVersion)
{
	LUA->PushString(std::to_string(gGModAudio->GetVersion()).c_str());
	return 1;
}

LUA_FUNCTION_STATIC(bass_CreateDummyChannel)
{
	int nSampleRate = (int)LUA->CheckNumber(1);
	int nChannels = (int)LUA->CheckNumber(2);
	unsigned long nFlags = (unsigned long)LUA->CheckNumber(3);

	const char* pErrorCode = nullptr;
	IGModAudioChannel* pChannel = gGModAudio->CreateDummyChannel(nSampleRate, nChannels, nFlags, &pErrorCode);
	Push_IGModAudioChannel(LUA, pChannel);
	if (pErrorCode) {
		LUA->PushString(pErrorCode);
	} else {
		LUA->PushNil();
	}
	return 2;
}

LUA_FUNCTION_STATIC(bass_CreatePushChannel)
{
	int nSampleRate = (int)LUA->CheckNumber(1);
	int nChannels = (int)LUA->CheckNumber(2);
	unsigned long nFlags = (unsigned long)LUA->CheckNumber(3);

	const char* pErrorCode = nullptr;
	IGModAudioChannel* pChannel = gGModAudio->CreatePushChannel(nSampleRate, nChannels, nFlags, &pErrorCode);
	Push_IGModAudioChannel(LUA, pChannel);
	if (pErrorCode) {
		LUA->PushString(pErrorCode);
	} else {
		LUA->PushNil();
	}
	return 2;
}

LUA_FUNCTION_STATIC(bass_CreateMixerChannel)
{
	int nSampleRate = (int)LUA->CheckNumber(1);
	int nChannels = (int)LUA->CheckNumber(2);
	unsigned long nFlags = (unsigned long)LUA->CheckNumber(3);

	const char* pErrorCode = nullptr;
	IGModAudioChannel* pChannel = gGModAudio->CreateMixerChannel(nSampleRate, nChannels, nFlags, &pErrorCode);
	Push_IGModAudioChannel(LUA, pChannel);
	if (pErrorCode) {
		LUA->PushString(pErrorCode);
	} else {
		LUA->PushNil();
	}
	return 2;
}

LUA_FUNCTION_STATIC(bass_CreateSplitChannel)
{
	IGModAudioChannel* pChannel = Get_IGModAudioChannel(LUA, 1, true);
	unsigned long nFlags = (unsigned long)LUA->CheckNumber(2);

	const char* pErrorCode = nullptr;
	IGModAudioChannel* pNewChannel = gGModAudio->CreateSplitChannel(pChannel, nFlags, &pErrorCode);
	Push_IGModAudioChannel(LUA, pNewChannel);
	if (pErrorCode) {
		LUA->PushString(pErrorCode);
	} else {
		LUA->PushNil();
	}
	return 2;
}

#if 0
LUA_FUNCTION_STATIC(bass_LoadPlugin)
{
	const char* pError = nullptr;
	LUA->PushBool(gGModAudio->LoadPlugin(LUA->CheckString(1), &pError));
	if (pError) {
		LUA->PushString(pError);
	} else {
		LUA->PushNil();
	}

	return 2;
}
#endif

void CBassModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	/*SourceSDK::FactoryLoader gmod_audio_loader("gmod_audio"); // Probably a broken dll/so file.
	if (!gmod_audio_loader.GetFactory())
	{
		if (g_pGModAudio)
		{
			DevMsg(1, PROJECT_NAME ": Falling back to use our own IGMod_Audio\n");
			gGModAudio = g_pGModAudio;
			gGModAudio->Init(*appfn); // The engine didn't...
			return;
		}

		Warning("Failed to get Factory for gmod_audio file\n");
		return;
	}

	gGModAudio = (IGMod_Audio*)gmod_audio_loader.GetFactory()(INTERFACEVERSION_GMODAUDIO, nullptr); // Engine should Initialize it. If not, we need to do it.
	Detour::CheckValue("get interface", "IGMod_Audio", gGModAudio != nullptr);

	gGModAudio->Init(*appfn); // The engine didn't...
	*/

	gGModAudio = g_pGModAudio;
	gGModAudio->Init(*appfn);
	// Always use our own Interface to not create funnies with the engine.
}

void CBassModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::IGModAudioChannelEncoder, pLua->CreateMetaTable("IGModAudioChannelEncoder"));
		Util::AddFunc(pLua, IGModAudioChannelEncoder__tostring, "__tostring");
		Util::AddFunc(pLua, IGModAudioChannelEncoder__gc, "__gc");
		Util::AddFunc(pLua, IGModAudioChannelEncoder__index, "__index");
		Util::AddFunc(pLua, IGModAudioChannelEncoder__newindex, "__newindex");
		Util::AddFunc(pLua, IGModAudioChannelEncoder_IsValid, "IsValid");
		Util::AddFunc(pLua, IGModAudioChannelEncoder_GetTable, "GetTable");
		Util::AddFunc(pLua, IGModAudioChannelEncoder_ServerInit, "ServerInit");
		Util::AddFunc(pLua, IGModAudioChannelEncoder_ServerKick, "ServerKick");
		Util::AddFunc(pLua, IGModAudioChannelEncoder_SetServerCallback, "SetServerCallback");
		Util::AddFunc(pLua, IGModAudioChannelEncoder_SetPaused, "SetPaused");
		Util::AddFunc(pLua, IGModAudioChannelEncoder_GetState, "GetState");
		Util::AddFunc(pLua, IGModAudioChannelEncoder_SetChannel, "SetChannel");

		// Functions to push data
		Util::AddFunc(pLua, IGModAudioChannelEncoder_InsertVoiceData, "InsertVoiceData");
		Util::AddFunc(pLua, IGModAudioChannelEncoder_FeedEmpty, "FeedEmpty");
		Util::AddFunc(pLua, IGModAudioChannelEncoder_FeedData, "FeedData");

		// Cast functions
		Util::AddFunc(pLua, IGModAudioChannelEncoder_CastInit, "CastInit");
		Util::AddFunc(pLua, IGModAudioChannelEncoder_CastSetTitle, "CastSetTitle");
	pLua->Pop(1);

	Lua::GetLuaData(pLua)->RegisterMetaTable(Lua::IGModAudioChannel, pLua->CreateMetaTable("IGModAudioChannel"));
		Util::AddFunc(pLua, IGModAudioChannel__tostring, "__tostring");
		Util::AddFunc(pLua, IGModAudioChannel__gc, "__gc");
		Util::AddFunc(pLua, IGModAudioChannel__index, "__index");
		Util::AddFunc(pLua, IGModAudioChannel__newindex, "__newindex");
		Util::AddFunc(pLua, IGModAudioChannel_GetTable, "GetTable");
		Util::AddFunc(pLua, IGModAudioChannel_Destroy, "Destroy");
		Util::AddFunc(pLua, IGModAudioChannel_Stop, "Stop");
		Util::AddFunc(pLua, IGModAudioChannel_Pause, "Pause");
		Util::AddFunc(pLua, IGModAudioChannel_Play, "Play");
		Util::AddFunc(pLua, IGModAudioChannel_SetVolume, "SetVolume");
		Util::AddFunc(pLua, IGModAudioChannel_GetVolume, "GetVolume");
		Util::AddFunc(pLua, IGModAudioChannel_SetPlaybackRate, "SetPlaybackRate");
		Util::AddFunc(pLua, IGModAudioChannel_GetPlaybackRate, "GetPlaybackRate");
		Util::AddFunc(pLua, IGModAudioChannel_SetTime, "SetTime");
		Util::AddFunc(pLua, IGModAudioChannel_GetTime, "GetTime");
		Util::AddFunc(pLua, IGModAudioChannel_GetBufferedTime, "GetBufferedTime");
		Util::AddFunc(pLua, IGModAudioChannel_GetState, "GetState");
		Util::AddFunc(pLua, IGModAudioChannel_SetLooping, "SetLooping");
		Util::AddFunc(pLua, IGModAudioChannel_IsLooping, "IsLooping");
		Util::AddFunc(pLua, IGModAudioChannel_IsOnline, "IsOnline");
		Util::AddFunc(pLua, IGModAudioChannel_Is3D, "Is3D");
		Util::AddFunc(pLua, IGModAudioChannel_IsBlockStreamed, "IsBlockStreamed");
		Util::AddFunc(pLua, IGModAudioChannel_IsValid, "IsValid");
		Util::AddFunc(pLua, IGModAudioChannel_GetLength, "GetLength");
		Util::AddFunc(pLua, IGModAudioChannel_GetFileName, "GetFileName");
		Util::AddFunc(pLua, IGModAudioChannel_GetSamplingRate, "GetSamplingRate");
		Util::AddFunc(pLua, IGModAudioChannel_GetBitsPerSample, "GetBitsPerSample");
		Util::AddFunc(pLua, IGModAudioChannel_GetAverageBitRate, "GetAverageBitRate");
		Util::AddFunc(pLua, IGModAudioChannel_GetLevel, "GetLevel");
		Util::AddFunc(pLua, IGModAudioChannel_FFT, "FFT");
		Util::AddFunc(pLua, IGModAudioChannel_SetChannelPan, "SetChannelPan");
		Util::AddFunc(pLua, IGModAudioChannel_GetChannelPan, "GetChannelPan");
		Util::AddFunc(pLua, IGModAudioChannel_GetTags, "GetTags");
		Util::AddFunc(pLua, IGModAudioChannel_Restart, "Restart");

		Util::AddFunc(pLua, IGModAudioChannel_NotImplemented, "Get3DFadeDistance");
		Util::AddFunc(pLua, IGModAudioChannel_NotImplemented, "Set3DFadeDistance");
		Util::AddFunc(pLua, IGModAudioChannel_NotImplemented, "Get3DCone");
		Util::AddFunc(pLua, IGModAudioChannel_NotImplemented, "Set3DCone");
		Util::AddFunc(pLua, IGModAudioChannel_NotImplemented, "GetPos");
		Util::AddFunc(pLua, IGModAudioChannel_NotImplemented, "SetPos");
		Util::AddFunc(pLua, IGModAudioChannel_NotImplemented, "Get3DEnabled");
		Util::AddFunc(pLua, IGModAudioChannel_NotImplemented, "Set3DEnabled");

		// HolyLib specific
		Util::AddFunc(pLua, IGModAudioChannel_WriteToDisk, "WriteToDisk");
		Util::AddFunc(pLua, IGModAudioChannel_CreateEncoder, "CreateEncoder");
		Util::AddFunc(pLua, IGModAudioChannel_Update, "Update");
		Util::AddFunc(pLua, IGModAudioChannel_CreateLink, "CreateLink");
		Util::AddFunc(pLua, IGModAudioChannel_DestroyLink, "DestroyLink");
		Util::AddFunc(pLua, IGModAudioChannel_SetAttribute, "SetAttribute");
		Util::AddFunc(pLua, IGModAudioChannel_SetSlideAttribute, "SetSlideAttribute");
		Util::AddFunc(pLua, IGModAudioChannel_GetAttribute, "GetAttribute");
		Util::AddFunc(pLua, IGModAudioChannel_IsAttributeSliding, "IsAttributeSliding");
		Util::AddFunc(pLua, IGModAudioChannel_GetChannelData, "GetChannelData");
		Util::AddFunc(pLua, IGModAudioChannel_GetChannelCount, "GetChannelCount");

		Util::AddFunc(pLua, IGModAudioChannel_SetFX, "SetFX");
		Util::AddFunc(pLua, IGModAudioChannel_ResetFX, "ResetFX");
		Util::AddFunc(pLua, IGModAudioChannel_RemoveFX, "RemoveFX");

		// Functions to push data
		Util::AddFunc(pLua, IGModAudioChannel_IsPush, "IsPush");
		Util::AddFunc(pLua, IGModAudioChannel_InsertVoiceData, "InsertVoiceData");
		Util::AddFunc(pLua, IGModAudioChannel_FeedEmpty, "FeedEmpty");
		Util::AddFunc(pLua, IGModAudioChannel_FeedData, "FeedData");

		Util::AddFunc(pLua, IGModAudioChannel_IsMixer, "IsMixer");
		Util::AddFunc(pLua, IGModAudioChannel_AddMixerChannel, "AddMixerChannel");
		Util::AddFunc(pLua, IGModAudioChannel_RemoveMixerChannel, "RemoveMixerChannel");
		Util::AddFunc(pLua, IGModAudioChannel_GetMixerState, "GetMixerState");
		Util::AddFunc(pLua, IGModAudioChannel_SetMixerMatrix, "SetMixerMatrix");
		Util::AddFunc(pLua, IGModAudioChannel_GetMixerChannelCount, "GetMixerChannelCount");

		Util::AddFunc(pLua, IGModAudioChannel_IsSplitter, "IsSplitter");
		Util::AddFunc(pLua, IGModAudioChannel_ResetSplitStream, "ResetSplitStream");
	pLua->Pop(1);

	Util::StartTable(pLua);
		Util::AddFunc(pLua, bass_PlayFile, "PlayFile");
		Util::AddFunc(pLua, bass_PlayURL, "PlayURL");
		Util::AddFunc(pLua, bass_Update, "Update");
		Util::AddFunc(pLua, bass_GetVersion, "GetVersion");
		Util::AddFunc(pLua, bass_CreateDummyChannel, "CreateDummyChannel");
		Util::AddFunc(pLua, bass_CreatePushChannel, "CreatePushChannel");
		Util::AddFunc(pLua, bass_CreateMixerChannel, "CreateMixerChannel");
		Util::AddFunc(pLua, bass_CreateSplitChannel, "CreateSplitChannel");
		// Util::AddFunc(pLua, bass_LoadPlugin, "LoadPlugin");

		Util::AddValue(pLua, BASS_ACTIVE_STOPPED, "BASS_ACTIVE_STOPPED");
		Util::AddValue(pLua, BASS_ACTIVE_PLAYING, "BASS_ACTIVE_PLAYING");
		Util::AddValue(pLua, BASS_ACTIVE_STALLED, "BASS_ACTIVE_STALLED");
		Util::AddValue(pLua, BASS_ACTIVE_PAUSED, "BASS_ACTIVE_PAUSED");
		Util::AddValue(pLua, BASS_ACTIVE_PAUSED_DEVICE, "BASS_ACTIVE_PAUSED_DEVICE");

		Util::AddValue(pLua, BASS_SAMPLE_8BITS, "BASS_SAMPLE_8BITS");
		Util::AddValue(pLua, BASS_SAMPLE_MONO, "BASS_SAMPLE_MONO");
		Util::AddValue(pLua, BASS_SAMPLE_LOOP, "BASS_SAMPLE_LOOP");
		Util::AddValue(pLua, BASS_SAMPLE_3D, "BASS_SAMPLE_3D");
		Util::AddValue(pLua, BASS_SAMPLE_SOFTWARE, "BASS_SAMPLE_SOFTWARE");
		Util::AddValue(pLua, BASS_SAMPLE_MUTEMAX, "BASS_SAMPLE_MUTEMAX");
		Util::AddValue(pLua, BASS_SAMPLE_NOREORDER, "BASS_SAMPLE_NOREORDER");
		Util::AddValue(pLua, BASS_SAMPLE_FX, "BASS_SAMPLE_FX");
		Util::AddValue(pLua, BASS_SAMPLE_FLOAT, "BASS_SAMPLE_FLOAT");
		Util::AddValue(pLua, BASS_SAMPLE_OVER_VOL, "BASS_SAMPLE_OVER_VOL");
		Util::AddValue(pLua, BASS_SAMPLE_OVER_POS, "BASS_SAMPLE_OVER_POS");
		Util::AddValue(pLua, BASS_SAMPLE_OVER_DIST, "BASS_SAMPLE_OVER_DIST");

		Util::AddValue(pLua, BASS_STREAM_PRESCAN, "BASS_STREAM_PRESCAN");
		Util::AddValue(pLua, BASS_STREAM_AUTOFREE, "BASS_STREAM_AUTOFREE");
		Util::AddValue(pLua, BASS_STREAM_RESTRATE, "BASS_STREAM_RESTRATE");
		Util::AddValue(pLua, BASS_STREAM_BLOCK, "BASS_STREAM_BLOCK");
		Util::AddValue(pLua, BASS_STREAM_DECODE, "BASS_STREAM_DECODE");
		Util::AddValue(pLua, BASS_STREAM_STATUS, "BASS_STREAM_STATUS");

		Util::AddValue(pLua, BASS_ENCODE_NOHEAD, "BASS_ENCODE_NOHEAD");
		Util::AddValue(pLua, BASS_ENCODE_FP_8BIT, "BASS_ENCODE_FP_8BIT");
		Util::AddValue(pLua, BASS_ENCODE_FP_16BIT, "BASS_ENCODE_FP_16BIT");
		Util::AddValue(pLua, BASS_ENCODE_FP_24BIT, "BASS_ENCODE_FP_24BIT");
		Util::AddValue(pLua, BASS_ENCODE_FP_32BIT, "BASS_ENCODE_FP_32BIT");
		Util::AddValue(pLua, BASS_ENCODE_FP_AUTO, "BASS_ENCODE_FP_AUTO");
		Util::AddValue(pLua, BASS_ENCODE_BIGEND, "BASS_ENCODE_BIGEND");
		Util::AddValue(pLua, BASS_ENCODE_PAUSE, "BASS_ENCODE_PAUSE");
		Util::AddValue(pLua, BASS_ENCODE_PCM, "BASS_ENCODE_PCM");
		Util::AddValue(pLua, BASS_ENCODE_RF64, "BASS_ENCODE_RF64");
		Util::AddValue(pLua, BASS_ENCODE_MONO, "BASS_ENCODE_MONO");
		Util::AddValue(pLua, BASS_ENCODE_QUEUE, "BASS_ENCODE_QUEUE");
		Util::AddValue(pLua, BASS_ENCODE_WFEXT, "BASS_ENCODE_WFEXT");
		Util::AddValue(pLua, BASS_ENCODE_CAST_NOLIMIT, "BASS_ENCODE_CAST_NOLIMIT");
		Util::AddValue(pLua, BASS_ENCODE_LIMIT, "BASS_ENCODE_LIMIT");
		Util::AddValue(pLua, BASS_ENCODE_AIFF, "BASS_ENCODE_AIFF");
		Util::AddValue(pLua, BASS_ENCODE_DITHER, "BASS_ENCODE_DITHER");
		Util::AddValue(pLua, BASS_ENCODE_AUTOFREE, "BASS_ENCODE_AUTOFREE");

		Util::AddValue(pLua, BASS_ENCODE_SERVER_NOHTTP, "BASS_ENCODE_SERVER_NOHTTP");
		Util::AddValue(pLua, BASS_ENCODE_SERVER_META, "BASS_ENCODE_SERVER_META");
		Util::AddValue(pLua, BASS_ENCODE_SERVER_SSL, "BASS_ENCODE_SERVER_SSL");
		Util::AddValue(pLua, BASS_ENCODE_SERVER_SSLONLY, "BASS_ENCODE_SERVER_SSLONLY");

		Util::AddValue(pLua, BASS_ENCODE_CAST_PUBLIC, "BASS_ENCODE_SERVER_SSLONLY");
		Util::AddValue(pLua, BASS_ENCODE_CAST_PUT, "BASS_ENCODE_CAST_PUT");
		Util::AddValue(pLua, BASS_ENCODE_CAST_SSL, "BASS_ENCODE_CAST_SSL");

		Util::AddValue(pLua, BASS_ATTRIB_FREQ, "BASS_ATTRIB_FREQ");
		Util::AddValue(pLua, BASS_ATTRIB_VOL, "BASS_ATTRIB_VOL");
		Util::AddValue(pLua, BASS_ATTRIB_PAN, "BASS_ATTRIB_PAN");
		Util::AddValue(pLua, BASS_ATTRIB_EAXMIX, "BASS_ATTRIB_EAXMIX");
		Util::AddValue(pLua, BASS_ATTRIB_NOBUFFER, "BASS_ATTRIB_NOBUFFER");
		Util::AddValue(pLua, BASS_ATTRIB_VBR, "BASS_ATTRIB_VBR");
		Util::AddValue(pLua, BASS_ATTRIB_CPU, "BASS_ATTRIB_CPU");
		Util::AddValue(pLua, BASS_ATTRIB_SRC, "BASS_ATTRIB_SRC");
		Util::AddValue(pLua, BASS_ATTRIB_NET_RESUME, "BASS_ATTRIB_NET_RESUME");
		Util::AddValue(pLua, BASS_ATTRIB_SCANINFO, "BASS_ATTRIB_SCANINFO");
		Util::AddValue(pLua, BASS_ATTRIB_NORAMP, "BASS_ATTRIB_NORAMP");
		Util::AddValue(pLua, BASS_ATTRIB_BITRATE, "BASS_ATTRIB_BITRATE");
		Util::AddValue(pLua, BASS_ATTRIB_BUFFER, "BASS_ATTRIB_BUFFER");
		Util::AddValue(pLua, BASS_ATTRIB_GRANULE, "BASS_ATTRIB_GRANULE");
		Util::AddValue(pLua, BASS_ATTRIB_USER, "BASS_ATTRIB_USER");
		Util::AddValue(pLua, BASS_ATTRIB_TAIL, "BASS_ATTRIB_TAIL");
		Util::AddValue(pLua, BASS_ATTRIB_PUSH_LIMIT, "BASS_ATTRIB_PUSH_LIMIT");
		Util::AddValue(pLua, BASS_ATTRIB_DOWNLOADPROC, "BASS_ATTRIB_DOWNLOADPROC");
		Util::AddValue(pLua, BASS_ATTRIB_VOLDSP, "BASS_ATTRIB_VOLDSP");
		Util::AddValue(pLua, BASS_ATTRIB_VOLDSP_PRIORITY, "BASS_ATTRIB_VOLDSP_PRIORITY");

		Util::AddValue(pLua, BASS_SLIDE_LOG, "BASS_SLIDE_LOG");

		Util::AddValue(pLua, BASS_MIXER_CHAN_BUFFER, "BASS_MIXER_CHAN_BUFFER");
		Util::AddValue(pLua, BASS_MIXER_CHAN_DOWNMIX, "BASS_MIXER_CHAN_DOWNMIX");
		Util::AddValue(pLua, BASS_MIXER_CHAN_LIMIT, "BASS_MIXER_CHAN_LIMIT");
		Util::AddValue(pLua, BASS_MIXER_CHAN_MATRIX, "BASS_MIXER_CHAN_MATRIX");
		Util::AddValue(pLua, BASS_MIXER_CHAN_NORAMPIN, "BASS_MIXER_CHAN_NORAMPIN");
		Util::AddValue(pLua, BASS_MIXER_CHAN_PAUSE, "BASS_MIXER_CHAN_PAUSE");

		Util::AddValue(pLua, (int)BassFX::FX_CHORUS, "FX_CHORUS");
		Util::AddValue(pLua, (int)BassFX::FX_DISTORTION, "FX_DISTORTION");
		Util::AddValue(pLua, (int)BassFX::FX_ECHO, "FX_ECHO");
		Util::AddValue(pLua, (int)BassFX::FX_FLANGER, "FX_FLANGER");
		Util::AddValue(pLua, (int)BassFX::FX_PARAMEQ, "FX_PARAMEQ");
		Util::AddValue(pLua, (int)BassFX::FX_REVERB, "FX_REVERB");
	Util::FinishTable(pLua, "bass");
}

void CBassModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	// Finish all callbacks
	// We pass pLua so that our BassEncoderCallback can check if its their state and force a finish
	gGModAudio->FinishAllAsync(pLua);

	Util::NukeTable(pLua, "bass");
}

void CBassModule::Shutdown()
{
	gGModAudio->Shutdown(); // If the engine didn't call Init, it most likely won't call shutdown.
}

void CBassModule::Think(bool bSimulating)
{
	gGModAudio->Update((int)(gpGlobals->absoluteframetime * 1000)); // gpGlobals->absoluteframetime should be in seconds so we need to turn it to ms.
}