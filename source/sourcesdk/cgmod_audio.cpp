#include "interface.h"
#include "cgmod_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <tier2/tier2.h>
#include <filesystem.h>
#include "Platform.hpp"

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
}

#ifdef SYSTEM_WINDOWS
void CALLBACK MyFileCloseProcc(void *user)
{
	fclose((FILE*)user);
}

unsigned long long CALLBACK MyFileLenProcc(void *user)
{
	struct stat finfo;
	fstat(fileno((FILE*)user), &finfo); // Crash: It crashes here because we fucked up in CBassAudioStream::Init
	return finfo.st_size;
}

unsigned long CALLBACK MyFileReadProcc(void *buffer, DWORD length, void *user)
{
	return fread(buffer, 1, length, (FILE*)user);
}

BOOL CALLBACK MyFileSeekProcc(QWORD offset, void* user)
{
	return !fseek((FILE*)user, offset, SEEK_SET);
}
#endif

void CBassAudioStream::Init(IAudioStreamEvent* event)
{
#ifdef SYSTEM_WINDOWS
	BASS_FILEPROCS fileprocs={MyFileCloseProcc, MyFileLenProcc, MyFileReadProcc, MyFileSeekProcc};

	m_hStream = BASS_StreamCreateFileUser(STREAMFILE_NOBUFFER, BASS_STREAM_AUTOFREE, &fileprocs, event); // ToDo: FIX THIS. event should be a FILE* not a IAudioStreamEvent* -> Crash

	if (m_hStream == 0) {
		Warning("Couldn't create BASS audio stream (%s)", BassErrorToString(BASS_ErrorGetCode()));
	}
#endif
}

void CALLBACK CBassAudioStream::MyFileCloseProc(void* user)
{
#ifdef SYSTEM_WINDOWS
	fclose((FILE*)user);
#endif
}

unsigned long long CALLBACK CBassAudioStream::MyFileLenProc(void* user)
{
#ifdef SYSTEM_WINDOWS
	struct stat finfo;
	fstat(fileno((FILE*)user), &finfo);
	return finfo.st_size;
#else
	return 0;
#endif
}

unsigned long CALLBACK CBassAudioStream::MyFileReadProc(void *buffer, unsigned int length, void* user)
{
#ifdef SYSTEM_WINDOWS
	return fread(buffer, 1, length, (FILE*)user);
#else
	return 0;
#endif
}

bool CALLBACK CBassAudioStream::MyFileSeekProc(unsigned long long offset, void* user)
{
#ifdef SYSTEM_WINDOWS
	return !fseek((FILE*)user, offset, SEEK_SET);
#else
	return false;
#endif
}

unsigned int CBassAudioStream::Decode(void* data, unsigned int size)
{
	unsigned int result = 0;

#ifdef SYSTEM_WINDOWS
	if (data != nullptr) {
		result = BASS_ChannelGetData(reinterpret_cast<HSTREAM>(data), data, size);
	}
#endif

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
		return 0xAC44;
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

	return position;
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

bool CGMod_Audio::Init(CreateInterfaceFn interfaceFactory)
{
	ConnectTier1Libraries( &interfaceFactory, 1 );
	ConnectTier2Libraries( &interfaceFactory, 1 );

	BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 36);

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
								//Error("Couldn't Init Bass (%i)!", BASS_ErrorGetCode());
							}
						}
					}
				}
			}
		}
	}

	BASS_SetConfig(BASS_CONFIG_BUFFER, 36);
	BASS_SetConfig(BASS_CONFIG_NET_BUFFER, 1048576);
	BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 0);
	BASS_Set3DFactors(0.0680416, 7.0, 5.2);

	return 1;
}

const char* CGMod_Audio::GetErrorString(int id)
{
	const char* error = g_BASSErrorStrings[id];
	if (error) {
		return error;
	}

	return "BASS_ERROR_UNKNOWN";
}

void CGMod_Audio::Shutdown()
{
	BASS_Free();
	BASS_PluginFree(0);
}

IBassAudioStream* CGMod_Audio::CreateAudioStream(IAudioStreamEvent* event)
{
	CBassAudioStream* pBassStream = new CBassAudioStream();
	pBassStream->Init(event);

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

	BASS_Set3DPosition(&earPos, &earVel, &earFr, &earT);
	BASS_Set3DFactors(0.0680416, 7.0, 5.2);

	BASS_Apply3D();
}

#define BASS_NOPLAY (1 << 9)
DWORD BASSFlagsFromString(const std::string& flagsString)
{
	DWORD flags = 0;
	if (flagsString.empty())
	{
		flags |= BASS_STREAM_BLOCK;
		return flags;
	}

	if (flagsString.find("3d") != std::string::npos)
	{
		flags |= BASS_SAMPLE_3D;
	}

	if (flagsString.find("mono") != std::string::npos)
	{
		flags |= BASS_SAMPLE_MONO;
	}

	if (flagsString.find("noplay") != std::string::npos)
	{
		flags |= BASS_NOPLAY; // Temp thing for now.
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

	DWORD bassFlags = BASSFlagsFromString(flags);
	HSTREAM stream = BASS_StreamCreateURL(url, 0, bassFlags, nullptr, nullptr);

	if (stream == 0) {
		*errorCode = BASS_ErrorGetCode();
		return NULL;
	}

	if (!(bassFlags & BASS_NOPLAY) && !BASS_ChannelPlay(stream, TRUE)) {
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

	char fullPath[MAX_PATH];
	g_pFullFileSystem->RelativePathToFullPath(filePath, "GAME", fullPath, sizeof(fullPath));

	DWORD bassFlags = BASSFlagsFromString(flags);
	HSTREAM stream = BASS_StreamCreateFile(FALSE, fullPath, 0, 0, bassFlags);
	//delete[] fullPath; // Causes a crash

	if (stream == 0) {
		*errorCode = BASS_ErrorGetCode();
		return NULL;
	}

	if (!(bassFlags & BASS_NOPLAY) && !BASS_ChannelPlay(stream, TRUE)) {
		*errorCode = BASS_ErrorGetCode();
		BASS_StreamFree(stream);
		return NULL;
	}

	return (IGModAudioChannel*)new CGModAudioChannel(stream, true);
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGMod_Audio, IGMod_Audio, "IGModAudio001", g_CGMod_Audio);


CGModAudioChannel::CGModAudioChannel( DWORD handle, bool isfile )
{
	BASS_ChannelSet3DAttributes(handle, BASS_3DMODE_NORMAL, 200, 1000000000, 360, 360, 0);
	BASS_Apply3D();
	this->m_pHandle = handle;
	this->m_bIsFile = isfile;
}

void CGModAudioChannel::Destroy()
{
 	if (BASS_ChannelIsActive(m_pHandle) == 1) {
		BASS_ChannelFree(m_pHandle);
	} else {
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

	BASS_ChannelSet3DPosition(m_pHandle, &earPos, earForward ? &earDir : NULL, earUp ? &earUpVec : NULL);
	BASS_Apply3D();
}

void CGModAudioChannel::GetPos(Vector* earPosition, Vector* earForward, Vector* earUp)
{
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
	double pos = BASS_ChannelBytes2Seconds(m_pHandle, time);
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
	static char fileName[MAX_PATH];
	BASS_CHANNELINFO info;
	if (BASS_ChannelGetInfo(m_pHandle, &info)) {
		strcpy(fileName, info.filename);
	} else {
		strcpy(fileName, "NULL");
	}

	return fileName;
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
		BASS_ChannelGetData(m_pHandle, data, 2147483648 + channelFFT);
	} else {
		memset(data, 0, sizeof(float) * channelFFT);
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