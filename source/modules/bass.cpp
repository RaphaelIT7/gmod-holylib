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
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn) OVERRIDE;
	virtual void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) OVERRIDE;
	virtual void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual void Think(bool bSimulating) OVERRIDE;
	virtual const char* Name() { return "bass"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
	virtual bool IsEnabledByDefault() { return true; };
	virtual bool SupportsMultipleLuaStates() { return true; };
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

	virtual ~BassEncoderCallback() {
		if (m_pLua && m_nCallbackReference != -1)
		{
			Msg("BassEncoderCallback deleted while still holding a reference!\n");
			Util::ReferenceFree(m_pLua, m_nCallbackReference, "BassEncoderCallback - callback leftover deletion");
			m_nCallbackReference = -1;
			m_pLua = NULL;
		}
	};

	virtual bool ShouldForceFinish(IGModAudioChannelEncoder* pEncoder, void* nSignalData) {
		return m_pLua == nSignalData; // Force finish as this is our signal that our interface is shutting down!
	};

	virtual void OnFinish(IGModAudioChannelEncoder* pEncoder, GModEncoderStatus nStatus)
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
		m_pLua = NULL;
	}

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

	const char* pErrorMsg = NULL;
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

	const char* pError = NULL;
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

	const char* pError = NULL;
	LUA->PushBool(channel->DestroyLink(otherChannel, &pError));
	if (pError) {
		LUA->PushString(pError);
	} else {
		LUA->PushNil();
	}

	return 2;
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

LUA_FUNCTION_STATIC(IGModAudioChannelEncoder_MakeServer)
{
	IGModAudioChannelEncoder* encoder = Get_IGModAudioChannelEncoder(LUA, 1, true);

	const char* strPort = LUA->CheckString(2);
	unsigned long nBuffer = (unsigned long)LUA->CheckNumber(3);
	unsigned long nBurst = (unsigned long)LUA->CheckNumber(4);
	unsigned long nFlags = (unsigned long)LUA->CheckNumber(5);

	const char* pErrorCode = nullptr;
	LUA->PushBool(encoder->MakeServer(strPort, nBuffer, nBurst, nFlags, &pErrorCode));
	if (pErrorCode) {
		LUA->PushString(pErrorCode);
	} else {
		LUA->PushNil();
	}
	return 2;
}

class BassEncoderDeletionCallback : public IGModEncoderCallback
{
public:
	BassEncoderDeletionCallback(GarrysMod::Lua::ILuaInterface* pLua) {
		m_pLua = pLua;
	}

	virtual bool ShouldForceFinish(IGModAudioChannelEncoder* pEncoder, void* nSignalData) {
		return m_pLua == nSignalData;
	};

	virtual void OnFinish(IGModAudioChannelEncoder* pEncoder, GModEncoderStatus nStatus)
	{
		// The IGModAudioChannelEncoder deletes itself, so we gotta ensure no invalid pointers are left in Lua
		Delete_IGModAudioChannelEncoder(m_pLua, pEncoder);
	}
private:
	GarrysMod::Lua::ILuaInterface* m_pLua;
};

LUA_FUNCTION_STATIC(IGModAudioChannel_CreateEncoder)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(LUA, 1, true);

	const char* pFileName = LUA->CheckString(2);
	// NOTE: Next time ensure I fucking use CheckNumber and not CheckString to then cast :sob: only took 8+ hours to figure out
	unsigned long nFlags = (unsigned long)LUA->CheckNumber(3);

	BassEncoderDeletionCallback* pCallback = new BassEncoderDeletionCallback(LUA);
	// We do not manage this pointer! GModAudio does for us

	const char* pErrorMsg = NULL;
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
	gGModAudio->Update((int)LUA->CheckNumberOpt(1, gpGlobals->absoluteframetime * 1000));
	return 0;
}

LUA_FUNCTION_STATIC(bass_GetVersion)
{
	LUA->PushString(std::to_string(gGModAudio->GetVersion()).c_str());
	return 1;
}

LUA_FUNCTION_STATIC(bass_CreateDummyChannel)
{
	int nSampleRate = LUA->CheckNumber(1);
	int nChannels = LUA->CheckNumber(2);
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

LUA_FUNCTION_STATIC(bass_LoadPlugin)
{
	const char* pError = NULL;
	LUA->PushBool(gGModAudio->LoadPlugin(LUA->CheckString(1), &pError));
	if (pError) {
		LUA->PushString(pError);
	} else {
		LUA->PushNil();
	}

	return 2;
}

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

	gGModAudio = (IGMod_Audio*)gmod_audio_loader.GetFactory()(INTERFACEVERSION_GMODAUDIO, NULL); // Engine should Initialize it. If not, we need to do it.
	Detour::CheckValue("get interface", "IGMod_Audio", gGModAudio != NULL);

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
		Util::AddFunc(pLua, IGModAudioChannelEncoder_MakeServer, "MakeServer");
		Util::AddFunc(pLua, IGModAudioChannelEncoder_SetPaused, "SetPaused");
		Util::AddFunc(pLua, IGModAudioChannelEncoder_GetState, "GetState");
		Util::AddFunc(pLua, IGModAudioChannelEncoder_SetChannel, "SetChannel");
		Util::AddFunc(pLua, IGModAudioChannelEncoder_InsertVoiceData, "InsertVoiceData");
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
	pLua->Pop(1);

	Util::StartTable(pLua);
		Util::AddFunc(pLua, bass_PlayFile, "PlayFile");
		Util::AddFunc(pLua, bass_PlayURL, "PlayURL");
		Util::AddFunc(pLua, bass_Update, "Update");
		Util::AddFunc(pLua, bass_GetVersion, "GetVersion");
		Util::AddFunc(pLua, bass_CreateDummyChannel, "CreateDummyChannel");
		// Util::AddFunc(pLua, bass_LoadPlugin, "LoadPlugin");
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