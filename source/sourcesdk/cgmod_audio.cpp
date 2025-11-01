// HOLYLIB_REQUIRES_MODULE=bass

#define DLL_TOOLS
#include "util.h"
#include "interface.h"
#include "cgmod_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <tier2/tier2.h>
#include <filesystem.h>
#include "Platform.hpp"
#include <detours.h>
#include <algorithm>
#include "bassenc_flac.h"
#include "bassenc_mp3.h"
#include "bassenc_ogg.h"
#include "bassenc_opus.h"
#include "bassmix.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static IThreadPool* g_pGModAudioThreadPool = nullptr;
static void OnGModAudioThreadsChange(IConVar* convar, const char* pOldValue, float flOldValue)
{
	if (!g_pGModAudioThreadPool)
		return;

	g_pGModAudioThreadPool->ExecuteAll();
	g_pGModAudioThreadPool->Stop();
	Util::StartThreadPool(g_pGModAudioThreadPool, ((ConVar*)convar)->GetInt());
}

static ConVar cgmod_audio_threads("holylib_bass_threads", "4", FCVAR_ARCHIVE, "The number of threads used for async functions", true, 1, true, 16, OnGModAudioThreadsChange);

// Set on interface init allowing us to possibly use newer features if available
static bool g_bUsesLatestBass = false;
static bool g_bUsesBassEnc = false;

static const char* g_BASSErrorStrings[] = {
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
	"BASS_ERROR_UNSTREAMABLE",
	"BASS_ERROR_PROTOCOL",
	"BASS_ERROR_DENIED",
	"BASS_ERROR_FREEING",
	"BASS_ERROR_CANCEL",
};

static const char* BassErrorToString(int errorCode) {
	// BassEnc errors (can't add them normally since they use numbers like 2100)
	if (errorCode == BASS_ERROR_ACM_CANCEL)
		return "BASS_ERROR_ACM_CANCEL";

	if (errorCode == BASS_ERROR_CAST_DENIED)
		return "BASS_ERROR_CAST_DENIED";

	if (errorCode == BASS_ERROR_SERVER_CERT)
		return "BASS_ERROR_SERVER_CERT";

	constexpr int totalErrors = sizeof(g_BASSErrorStrings) / sizeof(const char*);
	if (0 > errorCode || errorCode >= totalErrors)
		return "unknown error";

	const char* error = g_BASSErrorStrings[errorCode];
	if (error)
		return error;

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
	if (file && g_pFullFileSystem)
		g_pFullFileSystem->Close(file);
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
static BASS_ChannelRef* func_BASS_ChannelRef;
static BASS_ChannelFree* func_BASS_ChannelFree;

static BASS_Encode_Start* func_BASS_Encode_Start;
static BASS_Encode_StopEx* func_BASS_Encode_StopEx;
static BASS_Encode_SetNotify* func_BASS_Encode_SetNotify;
static BASS_Encode_IsActive* func_BASS_Encode_IsActive;
static BASS_Encode_ServerInit* func_BASS_Encode_ServerInit;
static BASS_Encode_ServerKick* func_BASS_Encode_ServerKick;
static BASS_Encode_SetPaused* func_BASS_Encode_SetPaused;
static BASS_Encode_GetChannel* func_BASS_Encode_GetChannel;
static BASS_Encode_SetChannel* func_BASS_Encode_SetChannel;
static BASS_Encode_Write* func_BASS_Encode_Write;

static BASS_Encode_MP3_Start* func_BASS_Encode_MP3_Start;
static BASS_Encode_OGG_Start* func_BASS_Encode_OGG_Start;
static BASS_Encode_OPUS_Start* func_BASS_Encode_OPUS_Start;
static BASS_Encode_FLAC_Start* func_BASS_Encode_FLAC_Start;

static BASS_Mixer_StreamCreate* func_BASS_Mixer_StreamCreate;
static BASS_Mixer_StreamAddChannel* func_BASS_Mixer_StreamAddChannel;
static BASS_Mixer_ChannelIsActive* func_BASS_Mixer_ChannelIsActive;
static BASS_Mixer_ChannelRemove* func_BASS_Mixer_ChannelRemove;
static BASS_Split_StreamCreate* func_BASS_Split_StreamCreate;
static BASS_Split_StreamReset* func_BASS_Split_StreamReset;

#define GetBassEncFunc(name, handle) \
func_##name = (name*)DLL_GetAddress(handle, #name); \
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
	BASS_SetConfig(BASS_CONFIG_ENCODE_QUEUE, 0);
	BASS_Set3DFactors(0.0680416f, 7.0f, 5.2f);
	if (GetVersion() == 33821184L)
	{
		g_bUsesLatestBass = true;
		Msg(PROJECT_NAME " - CGMod_Audio::Init found latets bass version :3\n");

		DLL_Handle pBass = DLL_LoadModule(DLL_PREEXTENSION "bass" LIBRARY_EXTENSION, RTLD_LAZY | RTLD_NOLOAD);
		if (pBass)
		{
			GetBassEncFunc(BASS_ChannelRef, pBass);
			GetBassEncFunc(BASS_ChannelFree, pBass);
		}
	}

	DLL_Handle pBassEnc;
	if (LoadDLL("bassenc", (void**)&pBassEnc))
	{
		Msg(PROJECT_NAME " - CGMod_Audio::Init found bassenc, loading functions :3\n");
		GetBassEncFunc(BASS_Encode_Start, pBassEnc);
		GetBassEncFunc(BASS_Encode_StopEx, pBassEnc);
		GetBassEncFunc(BASS_Encode_SetNotify, pBassEnc);
		GetBassEncFunc(BASS_Encode_IsActive, pBassEnc);
		GetBassEncFunc(BASS_Encode_ServerInit, pBassEnc);
		GetBassEncFunc(BASS_Encode_ServerKick, pBassEnc);
		GetBassEncFunc(BASS_Encode_SetPaused, pBassEnc);
		GetBassEncFunc(BASS_Encode_GetChannel, pBassEnc);
		GetBassEncFunc(BASS_Encode_SetChannel, pBassEnc);
		GetBassEncFunc(BASS_Encode_Write, pBassEnc);
		g_bUsesBassEnc = true;
	}

	DLL_Handle pBassEncMP3;
	if (LoadDLL("bassenc_mp3", (void**)&pBassEncMP3))
	{
		Msg(PROJECT_NAME " - CGMod_Audio::Init found bassenc_mp3, loading functions :3\n");
		GetBassEncFunc(BASS_Encode_MP3_Start, pBassEncMP3);
	}

	DLL_Handle pBassEncOGG;
	if (LoadDLL("bassenc_ogg", (void**)&pBassEncOGG))
	{
		Msg(PROJECT_NAME " - CGMod_Audio::Init found bassenc_ogg, loading functions :3\n");
		GetBassEncFunc(BASS_Encode_OGG_Start, pBassEncOGG);
	}

	DLL_Handle pBassEncOPUS;
	if (LoadDLL("bassenc_opus", (void**)&pBassEncOPUS))
	{
		Msg(PROJECT_NAME " - CGMod_Audio::Init found bassenc_opus, loading functions :3\n");
		GetBassEncFunc(BASS_Encode_OPUS_Start, pBassEncOPUS);
	}

	DLL_Handle pBassEncFLAC;
	if (LoadDLL("bassenc_flac", (void**)&pBassEncFLAC))
	{
		Msg(PROJECT_NAME " - CGMod_Audio::Init found bassenc_flac, loading functions :3\n");
		GetBassEncFunc(BASS_Encode_FLAC_Start, pBassEncFLAC);
	}

	DLL_Handle pBassMix;
	if (LoadDLL("bassmix", (void**)&pBassMix))
	{
		Msg(PROJECT_NAME " - CGMod_Audio::Init found bassmix, loading functions :3\n");
		GetBassEncFunc(BASS_Mixer_StreamCreate, pBassMix);
		GetBassEncFunc(BASS_Mixer_StreamAddChannel, pBassMix);
		GetBassEncFunc(BASS_Mixer_ChannelIsActive, pBassMix);
		GetBassEncFunc(BASS_Mixer_ChannelRemove, pBassMix);
		GetBassEncFunc(BASS_Split_StreamCreate, pBassMix);
		GetBassEncFunc(BASS_Split_StreamReset, pBassMix);
	}

	g_pGModAudioThreadPool = V_CreateThreadPool();
	Util::StartThreadPool(g_pGModAudioThreadPool, cgmod_audio_threads.GetInt());

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
	// Msg("CGMod_Audio::Shutdown start\n");
	FinishAllAsync((void*)EncoderForceShutdownPointer); // Finish all callbacks

	// Msg("CGMod_Audio::Shutdown 2\n");
	if (g_pGModAudioThreadPool)
	{
		// Msg("CGMod_Audio::Shutdown %i\n", g_pGModAudioThreadPool->GetJobCount());
		g_pGModAudioThreadPool->AbortAll(); // ToDo: Debug this and check why 64x shits itself when shutting down
		g_pGModAudioThreadPool->Stop(1);
		V_DestroyThreadPool(g_pGModAudioThreadPool);
		g_pGModAudioThreadPool = nullptr;
	}

	// Msg("CGMod_Audio::Shutdown 3\n");

	BASS_Free();
	BASS_PluginFree(0);

	// Msg("CGMod_Audio::Shutdown 4\n");

	for (void* pLoadedDLL : m_pLoadedDLLs)
	{
		DLL_UnloadModule((DLL_Handle)pLoadedDLL);
	}
	m_pLoadedDLLs.clear();

	// Msg("CGMod_Audio::Shutdown 5\n");
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

	std::unordered_set<CGModAudioChannelEncoder*> pEncoders = m_pEncoders;
	for (CGModAudioChannelEncoder* pEncoder : pEncoders)
		pEncoder->HandleFinish(NULL);
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

static DWORD BASSFlagsFromString(const std::string& flagsString, bool* autoplay) // autoplay arg doesn't exist in gmod.
{
	DWORD flags = BASS_SAMPLE_FLOAT;
	if (flagsString.empty())
	{
		flags |= BASS_STREAM_BLOCK;
		return flags;
	}

	if (flagsString.find("3d") != std::string::npos)
		flags |= BASS_SAMPLE_3D | BASS_SAMPLE_MONO;

	if (flagsString.find("mono") != std::string::npos)
		flags |= BASS_SAMPLE_MONO;

	if (flagsString.find("noplay") != std::string::npos)
		*autoplay = false;

	if (flagsString.find("noblock") == std::string::npos)
		flags |= BASS_STREAM_BLOCK;

	if (flagsString.find("decode") != std::string::npos)
	{
		flags |= BASS_STREAM_DECODE;
		*autoplay = false; // A decode channel cannot be played!
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

	return new CGModAudioChannel(stream, false);
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
		*errorCode = BASS_ERROR_FILEOPEN;
		return NULL;
	}
	
	bool autoplay = true;
	DWORD bassFlags = BASSFlagsFromString(flags, &autoplay);
	BASS_FILEPROCS fileprocs={CBassAudioStream::FileClose, CBassAudioStream::FileLength, CBassAudioStream::FileRead, CBassAudioStream::FileSeek};
	HSTREAM stream = BASS_StreamCreateFileUser(STREAMFILE_NOBUFFER, bassFlags, &fileprocs, pHandle);

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

	return new CGModAudioChannel(stream, true, filePath);
}

bool CGMod_Audio::LoadPlugin(const char* pluginName, const char** pErrorOut)
{
	if (pErrorOut)
		*pErrorOut = NULL;

	if (m_pLoadedPlugins.find(pluginName) != m_pLoadedPlugins.end())
		return true;

	HPLUGIN plugin = BASS_PluginLoad(pluginName, 0);
	if (!plugin)
	{
		int nError = BASS_ErrorGetCode();
		if (pErrorOut)
			*pErrorOut = GetErrorString(nError);

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

void CGMod_Audio::FinishAllAsync(void* nSignalData)
{
	Msg("FinishAllAsync start\n");
	if (g_pGModAudioThreadPool)
		g_pGModAudioThreadPool->ExecuteAll();

	std::unordered_set<CGModAudioChannelEncoder*> pEncoders = m_pEncoders; // Copy to avoid issues with deletion while iterating
	for (CGModAudioChannelEncoder* pEncoder : pEncoders)
		pEncoder->HandleFinish(nSignalData); // Freed inside
	Msg("FinishAllAsync done\n");
}

IGModAudioChannel* CGMod_Audio::CreateDummyChannel(int nSampleRate, int nChannels, unsigned long nFlags, const char** pErrorOut)
{
	*pErrorOut = NULL;
	HSTREAM pStream = BASS_StreamCreate(nSampleRate, nChannels, nFlags, STREAMPROC_DUMMY, NULL);
	if (!pStream)
	{
		*pErrorOut = BassErrorToString(BASS_ErrorGetCode());
		return NULL;
	}

	return new CGModAudioChannel(pStream, true, "DUMMY");
}

IGModAudioChannel* CGMod_Audio::CreateMixerChannel(int nSampleRate, int nChannels, unsigned long nFlags, const char** pErrorOut)
{
	*pErrorOut = NULL;
	if (!func_BASS_Mixer_StreamCreate)
	{
		*pErrorOut = "Missing BassMix plugin to function!";
		return NULL;
	}

	HSTREAM pStream = func_BASS_Mixer_StreamCreate(nSampleRate, nChannels, nFlags);
	if (!pStream)
	{
		*pErrorOut = BassErrorToString(BASS_ErrorGetCode());
		return NULL;
	}

	return new CGModAudioChannel(pStream, false, "MIXER", true);
}

IGModAudioChannel* CGMod_Audio::CreateSplitChannel(IGModAudioChannel* pChannel, unsigned long nFlags, const char** pErrorOut)
{
	*pErrorOut = NULL;
	if (!func_BASS_Split_StreamCreate)
	{
		*pErrorOut = "Missing BassMix plugin to function!";
		return NULL;
	}

	HSTREAM pStream = func_BASS_Split_StreamCreate(((CGModAudioChannel*)pChannel)->m_pHandle, nFlags, NULL);
	if (!pStream)
	{
		*pErrorOut = BassErrorToString(BASS_ErrorGetCode());
		return NULL;
	}

	return new CGModAudioChannel(pStream, false, "SPLIT", false, true);
}

// We gotta load some DLLs first as bass won't load shit itself when using BASS_PluginLoad >:(
bool CGMod_Audio::LoadDLL(const char* pDLLName, void** pDLLHandle)
{
	if (pDLLHandle)
		*pDLLHandle = NULL;

	std::string pFileName = DLL_PREEXTENSION;
	pFileName.append(pDLLName);
	pFileName.append(LIBRARY_EXTENSION);

	DLL_Handle handle = DLL_LoadModule(pFileName.c_str(), RTLD_LAZY);
	if (!handle)
		return false;

	if (pDLLHandle)
		*pDLLHandle = handle;

	m_pLoadedDLLs.push_back((void*)handle);
	return true;
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGMod_Audio, IGMod_Audio, "IGModAudio001", g_CGMod_Audio);

CGModAudioChannel::CGModAudioChannel( DWORD handle, bool isfile, const char* pFileName, bool isMixer, bool isSplit )
{
	BASS_ChannelSet3DAttributes(handle, BASS_3DMODE_NORMAL, 200, 1000000000, 360, 360, 0);
	BASS_Apply3D();
	m_pHandle = handle;
	m_bIsFile = isfile;
	m_bIsMixer = isMixer;
	m_bIsSplit = isSplit;

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

IGModAudioChannelEncoder* CGModAudioChannel::CreateEncoder(
	const char* pFileName, unsigned long nFlags,
	IGModEncoderCallback* pCallback, const char** pErrorOut
)
{
	*pErrorOut = NULL;
	CGModAudioChannelEncoder* pEncoder = new CGModAudioChannelEncoder(m_pHandle, pFileName, pCallback);
	if (pEncoder->GetLastError(pErrorOut))
		return NULL;

	pEncoder->InitEncoder(nFlags);
	if (pEncoder->GetLastError(pErrorOut))
		return NULL;

	return pEncoder;
}

void CGModAudioChannel::Update( unsigned long nLength )
{
	BASS_ChannelUpdate( m_pHandle, nLength );
}

bool CGModAudioChannel::CreateLink( IGModAudioChannel* pChannel, const char** pErrorOut )
{
	*pErrorOut = NULL;
	bool success = BASS_ChannelSetLink( m_pHandle, ((CGModAudioChannel*)pChannel)->m_pHandle );
	if (success)
		return true;

	*pErrorOut = BassErrorToString(BASS_ErrorGetCode());
	return false;
}

bool CGModAudioChannel::DestroyLink( IGModAudioChannel* pChannel, const char** pErrorOut )
{
	*pErrorOut = NULL;
	bool success = BASS_ChannelRemoveLink( m_pHandle, ((CGModAudioChannel*)pChannel)->m_pHandle );
	if (success)
		return true;

	*pErrorOut = BassErrorToString(BASS_ErrorGetCode());
	return false;
}

bool CGModAudioChannel::SetFX( const char* pFXName, unsigned long nType, int nPriority, void* pParams, const char** pErrorOut )
{
	*pErrorOut = NULL;
	auto it = m_pFX.find(pFXName);
	if (it != m_pFX.end())
	{
		CGModAudioFX* pFX = it->second;
		m_pFX.erase(it);
		pFX->Free(this);
	}

	HFX pFX = BASS_ChannelSetFX( m_pHandle, nType, nPriority );
	if (!pFX)
	{
		*pErrorOut = BassErrorToString(BASS_ErrorGetCode());
		return false;
	}

	CGModAudioFX* pGModFX = new CGModAudioFX( pFX, true );
	if (pParams && !pGModFX->SetParameters(pParams))
	{
		pGModFX->Free(this);
		*pErrorOut = BassErrorToString(BASS_ErrorGetCode());
		return false;
	}

	m_pFX[pFXName] = pGModFX;
	return true;
}

void CGModAudioChannel::SetFXParameter( const char* pFXName, void* params, const char** pErrorOut )
{
	*pErrorOut = NULL;
	auto it = m_pFX.find(pFXName);
	if (it == m_pFX.end())
	{
		*pErrorOut = "FX was not found by given name";
		return;
	}

	if (!it->second->SetParameters(params))
	{
		*pErrorOut = BassErrorToString(BASS_ErrorGetCode());
		return;
	}
}

bool CGModAudioChannel::FXReset( const char* pFXName )
{
	auto it = m_pFX.find(pFXName);
	if (it == m_pFX.end())
		return false;

	it->second->Reset();
}

bool CGModAudioChannel::FXFree( const char* pFXName )
{
	auto it = m_pFX.find(pFXName);
	if (it == m_pFX.end())
		return false;

	CGModAudioFX* pFX = it->second;
	m_pFX.erase(it);
	pFX->Free(this);
}

bool CGModAudioChannel::IsMixer()
{
	return m_bIsMixer;
}

void CGModAudioChannel::AddMixerChannel(IGModAudioChannel* pChannel, unsigned long nFlags, const char** pErrorOut)
{
	*pErrorOut = NULL;
	if (func_BASS_Mixer_StreamAddChannel(m_pHandle, ((CGModAudioChannel*)pChannel)->m_pHandle, nFlags))
		return;

	*pErrorOut = BassErrorToString(BASS_ErrorGetCode());
}

void CGModAudioChannel::RemoveMixerChannel()
{
	if (func_BASS_Mixer_ChannelRemove)
		func_BASS_Mixer_ChannelRemove(m_pHandle);
}

int CGModAudioChannel::GetMixerState()
{
	if (!func_BASS_Mixer_ChannelIsActive)
		return BASS_ACTIVE_STOPPED;

	return func_BASS_Mixer_ChannelIsActive(m_pHandle);
}

bool CGModAudioChannel::IsSplitter()
{
	return m_bIsSplit;
}

void CGModAudioChannel::ResetSplitStream()
{
	func_BASS_Split_StreamReset(m_pHandle);
}

// What went wrong...
// I don't know why but BASS just shits itself, like when ProcessChannel gets called it does not work at all.
CGModAudioChannelEncoder::CGModAudioChannelEncoder(DWORD pChannel, const char* pFileName, IGModEncoderCallback* pCallback)
{
	m_pCallback = pCallback;
	m_pChannel = pChannel;
	m_strFileName = pFileName;

	m_nStatus = GModEncoderStatus::NONE;
	m_strLastError = "";
	m_pEncoder = NULL;
	m_pFileHandle = NULL; // Can be NULL when given "" as a filename

	g_CGMod_Audio.AddEncoder(this);

	if (!g_bUsesBassEnc)
		m_strLastError = "Missing BassEnc plugin! (Install it manually!)";
}

bool CGModAudioChannelEncoder::GetLastError(const char** pErrorOut)
{
	*pErrorOut = NULL;
	if (m_strLastError.length() == 0)
		return false;

	static thread_local std::string g_strLastError = m_strLastError; // To avoid memory corruption.
	*pErrorOut = g_strLastError.c_str();

	delete this; // Freeing ourself!
	return true;
}

CGModAudioChannelEncoder::~CGModAudioChannelEncoder()
{
	g_CGMod_Audio.RemoveEncoder(this);

	if (m_pEncoder)
		Stop(false); // Free encoder

	// Invalidate pointers and such, fileHandle could still be valid
	OnEncoderFreed();

	if (m_pCallback)
	{
		delete m_pCallback;
		m_pCallback = nullptr;
	}
}

static inline bool IsDecodeChannel(DWORD handle)
{
	BASS_CHANNELINFO info;
	BASS_ChannelGetInfo(handle, &info);

	return (info.flags & BASS_STREAM_DECODE) == BASS_STREAM_DECODE;
}

void CGModAudioChannelEncoder::ProcessNow(bool bUseAnotherThread)
{
	if (!IsDecodeChannel(m_pChannel))
	{
		m_strLastError = "Not a decode channel! (Add decode as a flag when creating the channel!)";
		return;
	}

	if (m_nStatus != GModEncoderStatus::RUNNING)
	{
		m_strLastError = "Failed to init previously! Why are you still trying to process this";
		return;
	}

	if (bUseAnotherThread) {
		g_pGModAudioThreadPool->QueueCall(ProcessChannel, this);
	} else {
		ProcessChannel(this);
	}
}

void CGModAudioChannelEncoder::Stop(bool bProcessQueue)
{
	func_BASS_Encode_StopEx(m_pEncoder, bProcessQueue);
}

void CGModAudioChannelEncoder::ProcessChannel(CGModAudioChannelEncoder* pEncoder)
{
	while (true) // Based off bassenc convert.c example
	{
		if (!func_BASS_Encode_IsActive(pEncoder->m_pEncoder))
			break;

		char buffer[1 << 16];
		if (BASS_ChannelGetData(pEncoder->m_pChannel, buffer, sizeof(buffer)) == (unsigned long)-1)
			break;
	}

	pEncoder->Stop(true);
}

#define BASS_CheckFuncForPlugin(func, plugin) \
if (!func) { \
	m_strLastError = "Missing plugin " plugin; \
	return; \
}

void CGModAudioChannelEncoder::InitEncoder(unsigned long nEncoderFlags)
{
	// Used so that an encoder can be created by passing "mp3" without needing it to be a valid file
	if (m_strFileName.length() > 8)
	{
		m_pFileHandle = g_pFullFileSystem->Open(m_strFileName.c_str(), "wb", "DATA");
		if (!m_pFileHandle) {
			m_strLastError = BassErrorToString(BASS_ERROR_FILEOPEN);
			return;
		}
	}

	std::string fname(m_strFileName);
	std::transform(fname.begin(), fname.end(), fname.begin(), ::tolower);

	if (fname.size() >= 3 && fname.substr(fname.size() - 3) == "wav") {
		BASS_CheckFuncForPlugin(func_BASS_Encode_Start, "BASSENC")
		m_pEncoder = func_BASS_Encode_Start(m_pChannel, nullptr, BASS_ENCODE_PCM | nEncoderFlags, EncoderProcessCallback, this);
	} else if (fname.size() >= 4 && fname.substr(fname.size() - 4) == "aiff") {
		BASS_CheckFuncForPlugin(func_BASS_Encode_Start, "BASSENC")
		m_pEncoder = func_BASS_Encode_Start(m_pChannel, nullptr, BASS_ENCODE_PCM | BASS_ENCODE_AIFF | nEncoderFlags, EncoderProcessCallback, this);
	} else if (fname.size() >= 3 && fname.substr(fname.size() - 3) == "mp3") {
		BASS_CheckFuncForPlugin(func_BASS_Encode_MP3_Start, "BASSENC_MP3")
		m_pEncoder = func_BASS_Encode_MP3_Start(m_pChannel, nullptr, nEncoderFlags, EncoderProcessCallback, this);
	} else if (fname.size() >= 3 && fname.substr(fname.size() - 3) == "ogg") {
		BASS_CheckFuncForPlugin(func_BASS_Encode_OGG_Start, "BASSENC_OGG")
		m_pEncoder = func_BASS_Encode_OGG_Start(m_pChannel, nullptr, nEncoderFlags, EncoderProcessCallback, this);
	} else if (fname.size() >= 4 && fname.substr(fname.size() - 4) == "opus") {
		BASS_CheckFuncForPlugin(func_BASS_Encode_OPUS_Start, "BASSENC_OPUS")
		m_pEncoder = func_BASS_Encode_OPUS_Start(m_pChannel, nullptr, nEncoderFlags, EncoderProcessCallback, this);
	} else if (fname.size() >= 4 && fname.substr(fname.size() - 4) == "flac") {
		BASS_CheckFuncForPlugin(func_BASS_Encode_FLAC_Start, "BASSENC_FLAC")
		m_pEncoder = func_BASS_Encode_FLAC_Start(m_pChannel, nullptr, nEncoderFlags, EncoderProcessCallback, this);
	} else {
		m_strLastError = "Unknown encoder?";
		return;
	}

	if (!m_pEncoder)
	{
		m_strLastError = BassErrorToString(BASS_ErrorGetCode());
		return;
	}

	func_BASS_Encode_SetNotify(m_pEncoder, EncoderFreedCallback, this);

	m_nStatus = GModEncoderStatus::RUNNING;
}

void CGModAudioChannelEncoder::HandleFinish(void* nSignalData)
{
	if (nSignalData == (void*)EncoderForceShutdownPointer || (m_nStatus != GModEncoderStatus::FINISHED && (!m_pCallback || !m_pCallback->ShouldForceFinish(this, nSignalData))))
		return; // Not yet finished

	if (m_pCallback)
		m_pCallback->OnFinish(this, m_nStatus);

	delete this;
}

void CGModAudioChannelEncoder::WriteToFile(const void* pBuffer, unsigned long nLength)
{
	if (!m_pFileHandle)
		return;

	g_pFullFileSystem->Write(pBuffer, nLength, (FileHandle_t)m_pFileHandle);
}

void CALLBACK CGModAudioChannelEncoder::EncoderProcessCallback(HENCODE handle, DWORD channel, const void* buffer, DWORD length, void* user)
{
	((CGModAudioChannelEncoder*)user)->WriteToFile(buffer, length);
}

void CALLBACK CGModAudioChannelEncoder::EncoderProcessCallback(HENCODE handle, DWORD channel, const void* buffer, DWORD length, QWORD offset, void* user)
{
	((CGModAudioChannelEncoder*)user)->WriteToFile(buffer, length);
}

void CGModAudioChannelEncoder::OnEncoderFreed()
{
	m_pEncoder = NULL;
	
	if (m_pFileHandle)
	{
		g_pFullFileSystem->Close((FileHandle_t)m_pFileHandle);
		m_pFileHandle = NULL;
	}

	if (m_nStatus != GModEncoderStatus::DIED)
	{
		m_nStatus = GModEncoderStatus::FINISHED;
	}
}

void CGModAudioChannelEncoder::OnEncoderDied()
{
	m_nStatus = GModEncoderStatus::DIED;
	Stop(false); // Should free the channel triggering OnDecoderFreed
}

bool CGModAudioChannelEncoder::ServerCallback(bool connect, const char* client, char headers[1024])
{
	if (!m_pCallback)
		return true;

	return m_pCallback->OnServerClient(this, connect, client, headers);
}

void CALLBACK CGModAudioChannelEncoder::EncoderFreedCallback(HENCODE handle, DWORD status, void *user)
{
	if (status == BASS_ENCODE_NOTIFY_FREE) {
		((CGModAudioChannelEncoder*)user)->OnEncoderFreed();
	} else if (status == BASS_ENCODE_NOTIFY_ENCODER) {
		((CGModAudioChannelEncoder*)user)->OnEncoderDied();
	}
}

bool CGModAudioChannelEncoder::EncoderServerClientCallback(HENCODE handle, BOOL connect, const char* client, char headers[1024], void* user)
{
	return ((CGModAudioChannelEncoder*)user)->ServerCallback(connect, client, headers);
}

bool CGModAudioChannelEncoder::MakeServer( const char* port, unsigned long buffer, unsigned long burst, unsigned long flags, const char** pErrorOut )
{
	*pErrorOut = NULL;
	if (func_BASS_Encode_ServerInit(m_pEncoder, port, buffer, burst, flags, NULL, this) == 0)
	{
		*pErrorOut = BassErrorToString(BASS_ErrorGetCode());
		return false;
	}

	return true;
}

bool CGModAudioChannelEncoder::ServerKick(const char * client)
{
	return func_BASS_Encode_ServerKick( m_pChannel, client );
}

void CGModAudioChannelEncoder::SetPaused(bool bPaused)
{
	func_BASS_Encode_SetPaused( m_pEncoder, bPaused );
}

int CGModAudioChannelEncoder::GetState()
{
	return (int)func_BASS_Encode_IsActive( m_pEncoder );
}

/*IGModAudioChannel* CGModAudioChannelEncoder::GetChannel()
{
	return func_BASS_Encode_GetChannel( m_pEncoder );
}*/

void CGModAudioChannelEncoder::SetChannel(IGModAudioChannel* pChannel, const char** pErrorOut)
{
	*pErrorOut = NULL;

	if (func_BASS_Encode_SetChannel( m_pEncoder, ((CGModAudioChannel*)pChannel)->m_pHandle ) == 0)
		*pErrorOut = BassErrorToString(BASS_ErrorGetCode());
}

void CGModAudioChannelEncoder::WriteData(const void* pData, unsigned long nLength)
{
	func_BASS_Encode_Write( m_pEncoder, pData, nLength );
}

CGModAudioFX::CGModAudioFX(DWORD pFXHandle, bool bIsFX)
{
	m_pHandle = pFXHandle;
	m_bIsFX = bIsFX;
}

void CGModAudioFX::Free(IGModAudioChannel* pChannel)
{
	if (pChannel)
	{
		if (m_bIsFX) {
			BASS_ChannelRemoveFX(((CGModAudioChannel*)pChannel)->m_pHandle, m_pHandle);
		} else {
			BASS_ChannelRemoveDSP(((CGModAudioChannel*)pChannel)->m_pHandle, m_pHandle);
		}
	}

	delete this;
}

void CGModAudioFX::GetParameters(void* params)
{
	// Unused rn
	Warning(PROJECT_NAME " - CGModAudioFX::GetParameters: Called an unfinished function\n");
}

void CGModAudioFX::Reset()
{
	BASS_FXReset( m_pHandle );
}

bool CGModAudioFX::SetParameters(void* params)
{
	if (!IsFX())
		return true;

	return BASS_FXSetParameters(m_pHandle, params);
}