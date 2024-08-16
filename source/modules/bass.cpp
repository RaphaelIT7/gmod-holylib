#include "module.h"
#include <GarrysMod/Lua/Interface.h>
#include "lua.h"
#include "sourcesdk/cgmod_audio.h"

class CBassModule : public IModule
{
public:
	virtual void Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn);
	virtual void LuaInit(bool bServerInit);
	virtual void LuaShutdown();
	virtual const char* Name() { return "bass"; };
	virtual int Compatibility() { return LINUX32 | LINUX64; };
	virtual bool IsEnabledByDefault() { return false; };
};

CBassModule g_pBassModule;
IModule* pBassModule = &g_pBassModule;

IGMod_Audio* gGModAudio;
int IGModAudioChannel_TypeID;
void Push_IGModAudioChannel(IGModAudioChannel* channel)
{
	if ( !channel )
	{
		g_Lua->PushNil();
		return;
	}

	g_Lua->PushUserType(channel, IGModAudioChannel_TypeID);
}

IGModAudioChannel* Get_IGModAudioChannel(int iStackPos)
{
	if (!g_Lua->IsType(iStackPos, IGModAudioChannel_TypeID))
		return NULL;

	return g_Lua->GetUserType<IGModAudioChannel>(iStackPos, IGModAudioChannel_TypeID);
}

LUA_FUNCTION_STATIC(IGModAudioChannel__tostring)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
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

LUA_FUNCTION_STATIC(IGModAudioChannel__gc)
{
	// ToDo

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel__index)
{
	if (!g_Lua->FindOnObjectsMetaTable(1, 2))
		LUA->PushNil();

	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_Destroy)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	channel->Destroy();
	LUA->SetUserType(1, NULL);

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_Stop)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	channel->Stop();

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_Pause)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	channel->Pause();

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_Play)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	channel->Play();

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_SetVolume)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	double volume = LUA->CheckNumber(2);
	channel->SetVolume(volume);

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetVolume)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	LUA->PushNumber(channel->GetVolume());

	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_SetPlaybackRate)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	double rate = LUA->CheckNumber(2);
	channel->SetPlaybackRate(rate);

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetPlaybackRate)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	LUA->PushNumber(channel->GetPlaybackRate());

	return 1;
}

// We don't need Set/GetPos

LUA_FUNCTION_STATIC(IGModAudioChannel_SetTime)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	double time = LUA->CheckNumber(2);
	bool dontDecode = LUA->GetBool(3);
	channel->SetTime(time, dontDecode);

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetTime)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	LUA->PushNumber(channel->GetTime());

	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetBufferedTime)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	LUA->PushNumber(channel->GetBufferedTime());

	return 1;
}

// We don't need Set/Get3DFadeDistance and Set/Get3DCone

LUA_FUNCTION_STATIC(IGModAudioChannel_GetState)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	LUA->PushNumber(channel->GetState());

	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_SetLooping)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	channel->SetLooping(LUA->GetBool(2));

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_IsLooping)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	LUA->PushBool(channel->IsLooping());

	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_IsOnline)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	LUA->PushBool(channel->IsOnline());

	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_Is3D)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	LUA->PushBool(channel->Is3D());

	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_IsBlockStreamed)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	LUA->PushBool(channel->IsBlockStreamed());

	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_IsValid)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	LUA->PushBool(channel->IsValid());

	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetLength)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	LUA->PushNumber(channel->GetLength());

	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetFileName)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	LUA->PushString(channel->GetFileName());

	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetSamplingRate)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	LUA->PushNumber(channel->GetSamplingRate());

	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetBitsPerSample)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	LUA->PushNumber(channel->GetBitsPerSample());

	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetAverageBitRate)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	LUA->PushNumber(channel->GetAverageBitRate());

	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetLevel)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	float left, right = 0;
	channel->GetLevel(&left, &right);
	LUA->PushNumber(left);
	LUA->PushNumber(right);

	return 2;
}

// ToDo: Finish the function below!
/*LUA_FUNCTION_STATIC(IGModAudioChannel_FFT)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	LUA->CheckType(2, GarrysMod::Lua::Type::Table);
	GModChannelFFT_t fft = (GModChannelFFT_t)LUA->CheckNumber(3);

	float fft2[1];
	channel->FFT(fft2, fft);
	LUA->PushNumber(channel->GetAverageBitRate());

	return 1;
}*/

LUA_FUNCTION_STATIC(IGModAudioChannel_SetChannelPan)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	float pan = LUA->CheckNumber(2);
	channel->SetChannelPan(pan);

	return 0;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetChannelPan)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	LUA->PushNumber(channel->GetChannelPan());

	return 1;
}

LUA_FUNCTION_STATIC(IGModAudioChannel_GetTags)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	int unknown = LUA->CheckNumber(2);
	LUA->PushString(channel->GetTags(unknown));

	return 1;
}

// Get/Set3DEnabled is not needed.

LUA_FUNCTION_STATIC(IGModAudioChannel_Restart)
{
	IGModAudioChannel* channel = Get_IGModAudioChannel(1);
	if (!channel)
		LUA->ArgError(1, "IGModAudioChannel");

	channel->Restart();

	return 0;
}

LUA_FUNCTION_STATIC(bass_PlayFile)
{
	const char* filePath = LUA->CheckString(1);
	const char* flags = LUA->CheckString(2);
	LUA->CheckType(3, GarrysMod::Lua::Type::Function);
	LUA->Push(3);
	int callback = LUA->ReferenceCreate();

	if (!gGModAudio)
		LUA->ThrowError("We don't have gGModAudio!");

	int errorCode = 0;
	IGModAudioChannel* audioChannel = gGModAudio->PlayFile(filePath, flags, &errorCode);
	
	LUA->ReferencePush(callback);
	LUA->ReferenceFree(callback);
		if (errorCode == 0)
			LUA->PushUserType(audioChannel, GarrysMod::Lua::Type::SoundHandle);
		else
			LUA->PushNil();
		LUA->PushNumber(errorCode);
		if (errorCode != 0)
			LUA->PushString(gGModAudio->GetErrorString(errorCode));
		else
			LUA->PushNil();
	g_Lua->CallFunctionProtected(3, 0, true);

	return 0;
}

LUA_FUNCTION_STATIC(bass_PlayURL)
{
	const char* url = LUA->CheckString(1);
	const char* flags = LUA->CheckString(2);
	LUA->CheckType(3, GarrysMod::Lua::Type::Function);
	LUA->Push(3);
	int callback = LUA->ReferenceCreate();

	if (!gGModAudio)
		LUA->ThrowError("We don't have gGModAudio!");

	int errorCode = 0;
	IGModAudioChannel* audioChannel = gGModAudio->PlayURL(url, flags, &errorCode);
	
	LUA->ReferencePush(callback);
	LUA->ReferenceFree(callback);
		if (errorCode == 0)
			LUA->PushUserType(audioChannel, GarrysMod::Lua::Type::SoundHandle);
		else
			LUA->PushNil();
		LUA->PushNumber(errorCode);
		if (errorCode != 0)
			LUA->PushString(gGModAudio->GetErrorString(errorCode));
		else
			LUA->PushNil();
	g_Lua->CallFunctionProtected(3, 0, true);

	return 0;
}

void CBassModule::Init(CreateInterfaceFn* appfn, CreateInterfaceFn* gamefn)
{
	SourceSDK::FactoryLoader gmod_audio_loader("gmod_audio"); // Probably a broken dll/so file.
	if (!gmod_audio_loader.GetFactory())
	{
		if (g_pGModAudio)
		{
			gGModAudio = g_pGModAudio;
			return;
		}

		Warning("Failed to get Factory for gmod_audio file\n");
		return;
	}

	gGModAudio = (IGMod_Audio*)gmod_audio_loader.GetFactory()(INTERFACEVERSION_GMODAUDIO, NULL); // Engine should Initialize it. If not, we need to do it.
	Detour::CheckValue("get interface", "IGMod_Audio", gGModAudio != NULL);
}

void CBassModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	IGModAudioChannel_TypeID = g_Lua->CreateMetaTable("IGModAudioChannel");
		Util::AddFunc(IGModAudioChannel__tostring, "__tostring");
		Util::AddFunc(IGModAudioChannel__gc, "__gc");
		Util::AddFunc(IGModAudioChannel__index, "__index");
		Util::AddFunc(IGModAudioChannel_Destroy, "Destroy");
		Util::AddFunc(IGModAudioChannel_Stop, "Stop");
		Util::AddFunc(IGModAudioChannel_Pause, "Pause");
		Util::AddFunc(IGModAudioChannel_Play, "Play");
		Util::AddFunc(IGModAudioChannel_SetVolume, "SetVolume");
		Util::AddFunc(IGModAudioChannel_GetVolume, "GetVolume");
		Util::AddFunc(IGModAudioChannel_SetPlaybackRate, "SetPlaybackRate");
		Util::AddFunc(IGModAudioChannel_GetPlaybackRate, "GetPlaybackRate");
		Util::AddFunc(IGModAudioChannel_SetTime, "SetTime");
		Util::AddFunc(IGModAudioChannel_GetTime, "GetTime");
		Util::AddFunc(IGModAudioChannel_GetBufferedTime, "GetBufferedTime");
		Util::AddFunc(IGModAudioChannel_GetState, "GetState");
		Util::AddFunc(IGModAudioChannel_SetLooping, "SetLooping");
		Util::AddFunc(IGModAudioChannel_IsLooping, "IsLooping");
		Util::AddFunc(IGModAudioChannel_IsOnline, "IsOnline");
		Util::AddFunc(IGModAudioChannel_Is3D, "Is3D");
		Util::AddFunc(IGModAudioChannel_IsBlockStreamed, "IsBlockStreamed");
		Util::AddFunc(IGModAudioChannel_IsValid, "IsValid");
		Util::AddFunc(IGModAudioChannel_GetLength, "GetLength");
		Util::AddFunc(IGModAudioChannel_GetFileName, "GetFileName");
		Util::AddFunc(IGModAudioChannel_GetSamplingRate, "GetSamplingRate");
		Util::AddFunc(IGModAudioChannel_GetBitsPerSample, "GetBitsPerSample");
		Util::AddFunc(IGModAudioChannel_GetAverageBitRate, "GetAverageBitRate");
		Util::AddFunc(IGModAudioChannel_GetLevel, "GetLevel");
		//Util::AddFunc(IGModAudioChannel_FFT, "FFT"); // Soon
		Util::AddFunc(IGModAudioChannel_SetChannelPan, "SetChannelPan");
		Util::AddFunc(IGModAudioChannel_GetChannelPan, "GetChannelPan");
		Util::AddFunc(IGModAudioChannel_GetTags, "GetTags");
		Util::AddFunc(IGModAudioChannel_Restart, "Restart");
	g_Lua->Pop(1);

	Util::StartTable();
		Util::AddFunc(bass_PlayFile, "PlayFile");
		Util::AddFunc(bass_PlayURL, "PlayURL");
	Util::FinishTable("bass");
}

void CBassModule::LuaShutdown()
{
	g_Lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		g_Lua->PushNil();
		g_Lua->SetField(-2, "bass");
	g_Lua->Pop(1);
}