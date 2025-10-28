// HOLYLIB_REQUIRES_MODULE=bass

#define DLL_TOOLS
#include "interface.h"
#include "cgmod_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <tier2/tier2.h>
#include <filesystem.h>
#include "Platform.hpp"
#include <detours.h>
#include "bassenc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Set on interface init allowing us to possibly use newer features if available
static bool g_bUsesLatestBass = false;

const char* g_BASSErrorStrings[] = {
	"BASS_OK",
	"BASS_ERROR_MEM",
	"BASS_ERROR_FILEOPEN",
	"BASS_ERROR_DRIVER",
	"BASS_ERROR_BUFLOST",
	"BASS_ERROR_HANDLE",
	"BASS_ERROR_FORMAT",
	"BASS_ERROR_POSITION",
	"BASS_ERROR_INIT",
	"BASS_ERROR_START",
	"BASS_ERROR_SSL",
	"BASS_ERROR_",
	"BASS_ERROR_",
	"BASS_ERROR_",
	"BASS_ERROR_ALREADY",
	"BASS_ERROR_NOTAUDIO",
	"BASS_ERROR_",
	"BASS_ERROR_",
	"BASS_ERROR_NOCHAN",
	"BASS_ERROR_ILLTYPE",
	"BASS_ERROR_ILLPARAM",
	"BASS_ERROR_NO3D",
	"BASS_ERROR_NOEAX",
	"BASS_ERROR_DEVICE",
	"BASS_ERROR_NOPLAY",
	"BASS_ERROR_FREQ",
	"BASS_ERROR_",
	"BASS_ERROR_NOTFILE",
	"BASS_ERROR_",
	"BASS_ERROR_NOHW",
	"BASS_ERROR_",
	"BASS_ERROR_EMPTY",
	"BASS_ERROR_NONET",
	"BASS_ERROR_CREATE",
	"BASS_ERROR_NOFX",
	"BASS_ERROR_",
	"BASS_ERROR_",
	"BASS_ERROR_NOTAVAIL",
	"BASS_ERROR_DECODE",
	"BASS_ERROR_DX",
	"BASS_ERROR_TIMEOUT",
	"BASS_ERROR_FILEFORM",
	"BASS_ERROR_SPEAKER",
	"BASS_ERROR_VERSION",
	"BASS_ERROR_CODEC",
	"BASS_ERROR_ENDED",
	"BASS_ERROR_BUSY",
	"BASS_ERROR_UNSTREAMABLE"
};

const char* BassErrorToString(int errorCode) {
	if (g_BASSErrorStrings[errorCode]) {
		return g_BASSErrorStrings[errorCode];
	}

	return "unknown error";
}

/*
CBassAudioStream
*/
CBassAudioStream::CBassAudioStream()
{
	// IDK
}

CBassAudioStream::~CBassAudioStream()
{
	// IDK
	if (m_hStream)
		BASS_StreamFree(m_hStream);
}

bool CBassAudioStream::Init(IAudioStreamEvent* event)
{
	BASS_FILEPROCS fileprocs={CBassAudioStream::FileClose, CBassAudioStream::FileLength, CBassAudioStream::FileRead, CBassAudioStream::FileSeek};

	m_hStream = BASS_StreamCreateFileUser(STREAMFILE_NOBUFFER, BASS_STREAM_AUTOFREE, &fileprocs, event); // ToDo: FIX THIS. event should be a FILE* not a IAudioStreamEvent* -> Crash

	if (m_hStream == 0) {
		Warning("Couldn't create BASS audio stream (%s)", BassErrorToString(BASS_ErrorGetCode()));
		return false;
	}

	return true;
}

void CALLBACK CBassAudioStream::FileClose(void* user)
{
	FileHandle_t file = reinterpret_cast<FileHandle_t>(user);
	if (file && g_pFullFileSystem) {
		g_pFullFileSystem->Close(file);
	}
}

QWORD CALLBACK CBassAudioStream::FileLength(void* user)
{
	FileHandle_t file = reinterpret_cast<FileHandle_t>(user);
	if (!file || !g_pFullFileSystem)
		return 0;

	return static_cast<QWORD>(g_pFullFileSystem->Size(file));
}

DWORD CALLBACK CBassAudioStream::FileRead(void *buffer, DWORD length, void* user)
{
	FileHandle_t file = reinterpret_cast<FileHandle_t>(user);
	if (!file || !g_pFullFileSystem)
		return 0;

	return g_pFullFileSystem->Read(buffer, length, file);
}

BOOL CALLBACK CBassAudioStream::FileSeek(QWORD offset, void* user)
{
	FileHandle_t file = reinterpret_cast<FileHandle_t>(user);
	if (!file || !g_pFullFileSystem)
		return false;

	g_pFullFileSystem->Seek(file, offset, FILESYSTEM_SEEK_HEAD);
	return true;
}

unsigned int CBassAudioStream::Decode(void* data, unsigned int size)
{
	unsigned int result = 0;

	if (data != nullptr) {
		result = BASS_ChannelGetData(m_hStream, data, size);
	}

	return result;
}

int CBassAudioStream::GetOutputBits()
{
	Error("Not used");
	return 0;
}

int CBassAudioStream::GetOutputRate()
{
	BASS_CHANNELINFO info;

	if (BASS_ChannelGetInfo(m_hStream, &info)) {
		return info.freq;
	} else {
		return 44100;
	}
}

int CBassAudioStream::GetOutputChannels()
{
	BASS_CHANNELINFO info;

	if (BASS_ChannelGetInfo(m_hStream, &info)) {
		return info.chans;
	} else {
		return 2;
	}
}

uint CBassAudioStream::GetPosition()
{
	QWORD position = 0;

	if (m_hStream != 0) {
		position = BASS_StreamGetFilePosition(m_hStream, BASS_FILEPOS_CURRENT);
	}

	return (uint)position;
}

void CBassAudioStream::SetPosition(unsigned int pos)
{
}

unsigned long CBassAudioStream::GetHandle()
{
	return m_hStream;
}

/*
	CGMod_Audio
*/

CGMod_Audio g_CGMod_Audio;
IGMod_Audio* g_pGModAudio = &g_CGMod_Audio;

CGMod_Audio::~CGMod_Audio()
{

}

// Functions of BASSENC as we do not want to link against them
// (we then couldn't load if they were missing!)
static BASS_Encode_Start* func_BASS_Encode_Start;

#define GetBassEncFunc(name) \
func_##name = (name*)Detour::GetFunction(bassenc.GetModule(), Symbol::FromName(#name)); \
Detour::CheckFunction((void*)func_##name, #name);

bool CGMod_Audio::Init(CreateInterfaceFn interfaceFactory)
{
	ConnectTier1Libraries( &interfaceFactory, 1 );
	ConnectTier2Libraries( &interfaceFactory, 1 );

	BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 10);

#ifdef DEDICATED
	if (!BASS_Init(0, 44100, 0, 0, NULL)) {
		Warning(PROJECT_NAME ": Couldn't Init Bass (%i)!", BASS_ErrorGetCode());
	}
#else
	if(!BASS_Init(-1, 44100, 0, 0, NULL)) {
		Warning("BASS_Init failed(%i)! Attempt 1.\n", BASS_ErrorGetCode());

		if(!BASS_Init(1, 44100, BASS_DEVICE_DEFAULT, 0, NULL)) {
			Warning("BASS_Init failed(%i)! Attempt 2.\n", BASS_ErrorGetCode());

			if(!BASS_Init(-1, 44100, BASS_DEVICE_3D | BASS_DEVICE_DEFAULT, 0, NULL)) {
				Warning("BASS_Init failed(%i)! Attempt 3.\n", BASS_ErrorGetCode());

				if(!BASS_Init(1, 44100, BASS_DEVICE_3D | BASS_DEVICE_DEFAULT, 0, NULL)) {
					Warning("BASS_Init failed(%i)! Attempt 4.\n", BASS_ErrorGetCode());

					if(!BASS_Init(-1 , 44100, 0, 0, NULL)) {
						Warning("BASS_Init failed(%i)! Attempt 5.\n", BASS_ErrorGetCode());

						if(!BASS_Init(1, 44100, 0, 0, NULL)) {
							Warning("BASS_Init failed(%i)! Attempt 6.\n", BASS_ErrorGetCode());

							if(!BASS_Init(0, 44100, 0, 0, NULL)) {
								Warning("Couldn't Init Bass (%i)!", BASS_ErrorGetCode()); // In Gmod this is an Error.
							}
						}
					}
				}
			}
		}
	}
#endif

	BASS_SetConfig(BASS_CONFIG_BUFFER, 50); // Gmod probably has a different value.
	BASS_SetConfig(BASS_CONFIG_NET_BUFFER, 1048576);
	BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 0);
	BASS_SetConfig(BASS_CONFIG_SRC, 2);
	BASS_Set3DFactors(0.0680416f, 7.0f, 5.2f);

	if (GetVersion() == 33821184L)
	{
		g_bUsesLatestBass = true;
		Msg(PROJECT_NAME " - CGMod_Audio::Init found latets bass version :3\n");

		// DLL_Handle handle = DLL_LoadModule(DLL_PREEXTENSION"bassenc" LIBRARY_EXTENSION, RTLD_LAZY);
		// Msg("dlerror: %s\nhandle %p\n", DLL_LASTERROR, handle);
		BASS_PluginLoad("bassenc", 0);
		SourceSDK::ModuleLoader bassenc(DLL_PREEXTENSION"bassenc");
		if (bassenc.IsValid())
		{
			Msg(PROJECT_NAME " - CGMod_Audio::Init found bassenc, loading functions :3\n");
			GetBassEncFunc(BASS_Encode_Start);
		}
	}

	return 1;
}

const char* CGMod_Audio::GetErrorString(int id)
{
	constexpr int totalErrors = sizeof(g_BASSErrorStrings) / sizeof(const char*);
	if (0 > id || id >= totalErrors)
		return "BASS_ERROR_UNKNOWN";

	const char* error = g_BASSErrorStrings[id];
	if (error) {
		return error;
	}

	return "BASS_ERROR_UNKNOWN";
}

unsigned long CGMod_Audio::GetVersion()
{
	return BASS_GetVersion();
}

void CGMod_Audio::Shutdown()
{
	BASS_Free();
	BASS_PluginFree(0);
}

IBassAudioStream* CGMod_Audio::CreateAudioStream(IAudioStreamEvent* event)
{
	if (true)
		return NULL;

	CBassAudioStream* pBassStream = new CBassAudioStream();
	if (!pBassStream->Init(event))
	{
		delete pBassStream;
		return NULL;
	}

	return (IBassAudioStream*)pBassStream;
}

void CGMod_Audio::SetGlobalVolume(float volume)
{
	int intVolume = static_cast<int>(volume * 10000);

	BASS_SetConfig(BASS_CONFIG_GVOL_STREAM, intVolume);
	BASS_SetConfig(BASS_CONFIG_GVOL_MUSIC, intVolume);
	BASS_SetConfig(BASS_CONFIG_GVOL_SAMPLE, intVolume);
}

void CGMod_Audio::StopAllPlayback()
{
	BASS_Stop();
	BASS_Start();
}

void CGMod_Audio::Update(unsigned int updatePeriod)
{
	BASS_Update(updatePeriod);
}

static Vector g_pLocalEarPosition;
void CGMod_Audio::SetEar(Vector* earPosition, Vector* earVelocity, Vector* earFront, Vector* earTop)
{
	BASS_3DVECTOR earPos;
	earPos.x = earPosition->x;
	earPos.y = earPosition->y;
	earPos.z = earPosition->z;

	BASS_3DVECTOR earVel;
	earVel.x = earVelocity->x;
	earVel.y = earVelocity->y;
	earVel.z = earVelocity->z;

	BASS_3DVECTOR earFr;
	earFr.x = earFront->x;
	earFr.y = earFront->y;
	earFr.z = -earFront->z;

	BASS_3DVECTOR earT;
	earT.x = earTop->x;
	earT.y = earTop->y;
	earT.z = -earTop->z;

	g_pLocalEarPosition = *earPosition;

	BASS_Set3DPosition(&earPos, &earVel, &earFr, &earT);
	BASS_Set3DFactors(0.0680416f, 7.0f, 5.2f);

	BASS_Apply3D();
}

DWORD BASSFlagsFromString(const std::string& flagsString, bool* autoplay) // autoplay arg doesn't exist in gmod.
{
	DWORD flags = BASS_SAMPLE_FLOAT;
	if (flagsString.empty())
	{
		flags |= BASS_STREAM_BLOCK;
		return flags;
	}

	if (flagsString.find("3d") != std::string::npos)
	{
		flags |= BASS_SAMPLE_3D | BASS_SAMPLE_MONO;
	}

	if (flagsString.find("mono") != std::string::npos)
	{
		flags |= BASS_SAMPLE_MONO;
	}

	if (flagsString.find("noplay") != std::string::npos)
	{
		*autoplay = false;
	}

	if (flagsString.find("noblock") == std::string::npos)
	{
		flags |= BASS_STREAM_BLOCK;
	}

	return flags;
}

IGModAudioChannel* CGMod_Audio::PlayURL(const char* url, const char* flags, int* errorCode)
{
	*errorCode = 0;
	if (url == nullptr || flags == nullptr) {
		*errorCode = -1;
		return NULL;
	}

	bool autoplay = true;
	DWORD bassFlags = BASSFlagsFromString(flags, &autoplay);
	HSTREAM stream = BASS_StreamCreateURL(url, 0, bassFlags, nullptr, nullptr);

	if (stream == 0) {
		*errorCode = BASS_ErrorGetCode();
		return NULL;
	}

	if (autoplay && !BASS_ChannelPlay(stream, TRUE)) {
		*errorCode = BASS_ErrorGetCode();
		BASS_StreamFree(stream);
		return NULL;
	}

	return (IGModAudioChannel*)new CGModAudioChannel(stream, false);
}

// The original function is too fucked up. https://i.imgur.com/xz5xAIJ.png
IGModAudioChannel* CGMod_Audio::PlayFile(const char* filePath, const char* flags, int* errorCode)
{
	*errorCode = 0;
	if (filePath == nullptr || flags == nullptr) {
		*errorCode = -1;
		return NULL;
	}

	// char fullPath[MAX_PATH];
	// g_pFullFileSystem->RelativePathToFullPath(filePath, "GAME", fullPath, sizeof(fullPath));

	// This doesn't match gmod iirc though it should be fine
	// This way it should fully work with vpk & gma files too without issue
	// As it no longer relies on a file having to exist on disk.
	FileHandle_t pHandle = g_pFullFileSystem->Open(filePath, "rb", "GAME");
	if (!pHandle)
	{
		*errorCode = -1;
		return NULL;
	}
	
	BASS_FILEPROCS fileprocs={CBassAudioStream::FileClose, CBassAudioStream::FileLength, CBassAudioStream::FileRead, CBassAudioStream::FileSeek};
	HSTREAM stream = BASS_StreamCreateFileUser(STREAMFILE_NOBUFFER, 0, &fileprocs, pHandle);

	bool autoplay = true;
	DWORD bassFlags = BASSFlagsFromString(flags, &autoplay);
	// HSTREAM stream = BASS_StreamCreateFile(FALSE, filePath, 0, 0, bassFlags);
	//delete[] fullPath; // Causes a crash

	if (stream == 0) {
		*errorCode = BASS_ErrorGetCode();
		return NULL;
	}

	if (autoplay && !BASS_ChannelPlay(stream, TRUE)) {
		*errorCode = BASS_ErrorGetCode();
		BASS_StreamFree(stream);
		return NULL;
	}

	return (IGModAudioChannel*)new CGModAudioChannel(stream, true, filePath);
}

bool CGMod_Audio::LoadPlugin(const char* pluginName)
{
	if (m_pLoadedPlugins.find(pluginName) != m_pLoadedPlugins.end())
		return true;

	HPLUGIN plugin = BASS_PluginLoad(pluginName, 0);
	if (!plugin)
	{
		int nError = BASS_ErrorGetCode();
		if (nError != BASS_ERROR_FILEOPEN) {
			Warning(PROJECT_NAME " CGMod_Audio: Failed to load plugin %s (%s)\n", pluginName, GetErrorString(nError));
		} else {
			DevMsg(PROJECT_NAME " CGMod_Audio: Skipping plugin load %s since it was never installed\n", pluginName);
		}

		return false;
	}

	m_pLoadedPlugins[pluginName] = plugin;
	DevMsg(PROJECT_NAME " CGMod_Audio: Successfully loaded plugin %s\n", pluginName);

	return true;
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGMod_Audio, IGMod_Audio, "IGModAudio001", g_CGMod_Audio);


CGModAudioChannel::CGModAudioChannel( DWORD handle, bool isfile, const char* pFileName )
{
	BASS_ChannelSet3DAttributes(handle, BASS_3DMODE_NORMAL, 200, 1000000000, 360, 360, 0);
	BASS_Apply3D();
	m_pHandle = handle;
	m_bIsFile = isfile;

	if (pFileName)
		m_strFileName = pFileName;
}

CGModAudioChannel::~CGModAudioChannel()
{

}

#define OLD_BASS 1
void CGModAudioChannel::Destroy()
{
#if !OLD_BASS
 	if (BASS_ChannelIsActive(m_pHandle) == 1) {
		BASS_ChannelFree(m_pHandle);
	} else 
#endif
	{
		int streamFlags = BASS_ChannelFlags(m_pHandle, 0, 0);
		if (!(streamFlags & BASS_STREAM_DECODE)) {
			BASS_StreamFree(m_pHandle);
		} else {
			BASS_MusicFree(m_pHandle);
		}
	}

	delete this;
}

void CGModAudioChannel::Stop()
{
	BASS_ChannelStop(m_pHandle);
}

void CGModAudioChannel::Pause()
{
	BASS_ChannelPause(m_pHandle);
}

void CGModAudioChannel::Play()
{
	BASS_ChannelPlay(m_pHandle, false);
}

void CGModAudioChannel::SetVolume(float volume)
{
	BASS_ChannelSetAttribute(m_pHandle, BASS_ATTRIB_VOL, volume);
}

float CGModAudioChannel::GetVolume()
{
	float volume = 0.0f;
	BASS_ChannelGetAttribute(m_pHandle, BASS_ATTRIB_VOL, &volume);

	return volume;
}

void CGModAudioChannel::SetPlaybackRate(float speed)
{
	BASS_ChannelSetAttribute(m_pHandle, BASS_ATTRIB_FREQ, speed);
}

float CGModAudioChannel::GetPlaybackRate()
{
	float currentPlaybackRate = 0;
	BASS_ChannelGetAttribute(m_pHandle, BASS_ATTRIB_FREQ, &currentPlaybackRate);

	return currentPlaybackRate;
}

void CGModAudioChannel::SetPos(Vector* earPosition, Vector* earForward, Vector* earUp)
{
	BASS_3DVECTOR earPos;
	earPos.x = earPosition->x;
	earPos.y = earPosition->y;
	earPos.z = earPosition->z;

	BASS_3DVECTOR earDir;
	if (earForward)
	{
		earDir.x = earForward->x;
		earDir.y = earForward->y;
		earDir.z = earForward->z;
	}

	BASS_3DVECTOR earUpVec;
	if (earUp)
	{
		earUpVec.x = earUp->x;
		earUpVec.y = earUp->y;
		earUpVec.z = earUp->z;
	}

	if ((g_pLocalEarPosition - *earPosition).LengthSqr() < 1.0) {
	//	BASS_ChannelFlags(m_pHandle, BASS_SAMPLE_3D, 0);
	}

	BASS_ChannelSet3DPosition(m_pHandle, &earPos, earForward ? &earDir : NULL, earUp ? &earUpVec : NULL);
	BASS_Apply3D();
}

static Vector vector_origin(0, 0, 0);
void CGModAudioChannel::GetPos(Vector* earPosition, Vector* earForward, Vector* earUp)
{
	if (!Is3D())
	{
		*earPosition = vector_origin;
		*earForward = vector_origin;
		*earUp = vector_origin;
		return;
	}

	BASS_3DVECTOR earPos, earDir, earUpVec;
	BASS_ChannelGet3DPosition(m_pHandle, &earPos, &earDir, &earUpVec);

	earPosition->x = earPos.x;
	earPosition->y = earPos.y;
	earPosition->z = earPos.z;

	earForward->x = earDir.x;
	earForward->y = earDir.y;
	earForward->z = earDir.z;

	earUp->x = earUpVec.x;
	earUp->y = earUpVec.y;
	earUp->z = earUpVec.z;
}

void CGModAudioChannel::SetTime(double time, bool dont_decode)
{
	QWORD pos = BASS_ChannelSeconds2Bytes(m_pHandle, time);
	//double currentPos = BASS_ChannelGetPosition(handle, BASS_POS_BYTE);

	DWORD mode = dont_decode ? BASS_POS_DECODE : BASS_POS_BYTE;
	BASS_ChannelSetPosition(m_pHandle, pos, mode);
}

double CGModAudioChannel::GetTime()
{
	return BASS_ChannelBytes2Seconds(m_pHandle, BASS_ChannelGetPosition(m_pHandle, BASS_POS_BYTE));
}

double CGModAudioChannel::GetBufferedTime()
{
	if (m_bIsFile)
	{
		return GetLength();
	} else {
		float bufferedTime = 0.0f;
		BASS_ChannelGetAttribute(m_pHandle, BASS_ATTRIB_BUFFER, &bufferedTime);
		return bufferedTime;
	}
}

void CGModAudioChannel::Set3DFadeDistance(float min, float max)
{
	BASS_ChannelSet3DAttributes(m_pHandle, BASS_3DMODE_NORMAL, min, max, -1, -1, -1);
	BASS_Apply3D();
}

void CGModAudioChannel::Get3DFadeDistance(float* min, float* max)
{
	BASS_ChannelGet3DAttributes(m_pHandle, BASS_3DMODE_NORMAL, min, max, 0, 0, 0);
}

void CGModAudioChannel::Set3DCone(int innerAngle, int outerAngle, float outerVolume)
{
	BASS_ChannelSet3DAttributes(m_pHandle, BASS_3DMODE_NORMAL, -1, -1, innerAngle, outerAngle, outerVolume);
	BASS_Apply3D();
}

void CGModAudioChannel::Get3DCone(int* innerAngle, int* outerAngle, float* outerVolume)
{
	BASS_ChannelGet3DAttributes(m_pHandle, BASS_3DMODE_NORMAL, 0, 0, (DWORD*)innerAngle, (DWORD*)outerAngle, outerVolume);
}

int CGModAudioChannel::GetState()
{
	return BASS_ChannelIsActive(m_pHandle);
}

void CGModAudioChannel::SetLooping(bool looping)
{
	BASS_ChannelFlags(m_pHandle, BASS_SAMPLE_LOOP, looping ? BASS_SAMPLE_LOOP : 0);
}

bool CGModAudioChannel::IsLooping()
{
	DWORD flags = BASS_ChannelFlags(m_pHandle, 0, 0);

	return (flags & BASS_SAMPLE_LOOP) != 0;
}

bool CGModAudioChannel::IsOnline()
{
	return !m_bIsFile;
}

bool CGModAudioChannel::Is3D()
{
	DWORD flags = BASS_ChannelFlags(m_pHandle, 0, 0);

	return (flags & BASS_SAMPLE_3D) != 0;
}

bool CGModAudioChannel::IsBlockStreamed()
{
	DWORD flags = BASS_ChannelFlags(m_pHandle, 0, 0);

	return (flags & BASS_STREAM_BLOCK) != 0;
}

bool CGModAudioChannel::IsValid()
{
	return m_pHandle != 0;
}

double CGModAudioChannel::GetLength()
{
	return BASS_ChannelBytes2Seconds(m_pHandle, BASS_ChannelGetLength(m_pHandle, BASS_POS_BYTE));
}

const char* CGModAudioChannel::GetFileName()
{
	return m_strFileName.c_str();
}

int CGModAudioChannel::GetSamplingRate()
{
	BASS_CHANNELINFO info;
	if (!BASS_ChannelGetInfo(m_pHandle, &info)) {
		return 0;
	}

	return info.freq;
}

int CGModAudioChannel::GetBitsPerSample()
{
	BASS_CHANNELINFO info;
	if (!BASS_ChannelGetInfo(m_pHandle, &info)) {
		return 0;
	}

	return info.origres;
}

float CGModAudioChannel::GetAverageBitRate()
{
	float averageBitRate = 0.0f;
	BASS_ChannelGetAttribute(m_pHandle, BASS_ATTRIB_BITRATE, &averageBitRate);

	return averageBitRate;
}

void CGModAudioChannel::GetLevel(float* leftLevel, float* rightLevel)
{
	if (BASS_ChannelIsActive(m_pHandle) == BASS_ACTIVE_PLAYING) {
		DWORD levels = BASS_ChannelGetLevel(m_pHandle);
		
		*leftLevel = LOWORD(levels) / 32768.0f;
		*rightLevel = HIWORD(levels) / 32768.0f;
	} else {
		*leftLevel = 0.0f;
		*rightLevel = 0.0f;
	}
}

void CGModAudioChannel::FFT(float *data, GModChannelFFT_t channelFFT)
{
	if (BASS_ChannelIsActive(m_pHandle) == BASS_ACTIVE_PLAYING) {
		BASS_ChannelGetData(m_pHandle, data, BASS_DATA_FFT256 << channelFFT);
	} else {
		memset(data, 0, sizeof(float) * (1 << (8 + (int)channelFFT)));
	}
}

void CGModAudioChannel::SetChannelPan(float pan)
{
	BASS_ChannelSetAttribute(m_pHandle, BASS_ATTRIB_PAN, pan);
}

float CGModAudioChannel::GetChannelPan()
{
	float result = 0.0f;
	BASS_ChannelGetAttribute(m_pHandle, BASS_ATTRIB_PAN, &result);

	return result;
}

const char* CGModAudioChannel::GetTags(int format)
{
	return BASS_ChannelGetTags(m_pHandle, format);
}

void CGModAudioChannel::Set3DEnabled(bool enabled)
{
	BASS_ChannelSet3DAttributes(m_pHandle, enabled ? BASS_3DMODE_NORMAL : BASS_3DMODE_OFF, -1, -1, -1, -1, 1);
	BASS_Apply3D();
}

bool CGModAudioChannel::Get3DEnabled()
{
	DWORD mode;
	BASS_ChannelGet3DAttributes(m_pHandle, &mode, 0, 0, 0, 0, 0);

	return mode != BASS_3DMODE_OFF;
}

void CGModAudioChannel::Restart()
{
	BASS_ChannelPlay(m_pHandle, true);
}

void CALLBACK EncodeCallback(HENCODE handle, DWORD channel, const void *buffer, DWORD length, void *user) {
	g_pFullFileSystem->Write(buffer, length, (FileHandle_t)user);
}

const char* CGModAudioChannel::EncodeToDisk(const char* pFileName, const char* pCommand, unsigned long nFlags)
{
	FileHandle_t pHandle = g_pFullFileSystem->Open(pFileName, "rb", "DATA");
	if (!pHandle)
		return g_CGMod_Audio.GetErrorString(BASS_ERROR_FILEOPEN);

	if (!func_BASS_Encode_Start)
		return "Missing BASSENC";

	HENCODE encoder = func_BASS_Encode_Start(m_pHandle, pCommand, nFlags, EncodeCallback, pHandle);
	if (!encoder) {
		int err = BASS_ErrorGetCode();
		g_pFullFileSystem->Close(pHandle);
		return g_CGMod_Audio.GetErrorString(BASS_ErrorGetCode());
	}

	g_pFullFileSystem->Close(pHandle);
	return NULL;
}