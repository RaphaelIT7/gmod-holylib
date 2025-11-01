#pragma once

#include <interface.h>
#include <mathlib/vector.h>

#define INTERFACEVERSION_GMODAUDIO			"IGModAudio001"

enum GModChannelFFT_t {
	FFT_256 = 0,
	FFT_512 = 1,
	FFT_1024 = 2,
	FFT_2048 = 3,
	FFT_4096 = 4,
	FFT_8192 = 5,
	FFT_16384 = 6,
	FFT_32768 = 7,
};

enum GModEncoderStatus {
	NONE = 0,
	RUNNING = 1,
	FINISHED = 2,
	DIED = 3,
};

// HolyLib specific
// NOTE: Always call GetLastError after any function call that throws one to check for errors!
class IGModAudioChannel;
class IGModAudioChannelEncoder
{
public:
	virtual ~IGModAudioChannelEncoder() {};

	// ProcessNow uses BASS_ChannelGetData to pull data, so this function only works on decode channels!
	// Will throw an error to check with GetLastError
	virtual void ProcessNow(bool bUseAnotherThread) = 0;

	virtual void Stop(bool bProcessQueue) = 0;

	// Returns true if there was an error, pErrorOut will either be filled or NULL
	// If it returns true, it will also invalidate/free itself so the pointer becomes invalid!
	// NOTE: This is only for fatal errors like on init, most other functions have a pErrorOut argument for light errors that can be ignored
	virtual bool GetLastError(const char** pErrorOut) = 0;

	// Wasn't exposed since CreateEncoder already calls it so it has no real use
	// virtual void InitEncoder(unsigned long nEncoderFlags) = 0;

	virtual bool MakeServer( const char* port, unsigned long buffer, unsigned long burst, unsigned long flags, const char** pErrorOut ) = 0;

	virtual void SetPaused( bool bPaused ) = 0;
	virtual int GetState() = 0;

	// virtual IGModAudioChannel* GetChannel() = 0;
	virtual void SetChannel( IGModAudioChannel* pChannel, const char** pErrorOut ) = 0;

	virtual void WriteData(const void* pData, unsigned long nLength) = 0;

	virtual const char* GetFileName() = 0;
};

class IGModAudioChannelEncoder;
class IGModEncoderCallback // Callback struct
{
public:
	virtual ~IGModEncoderCallback() {};
	virtual bool ShouldForceFinish(IGModAudioChannelEncoder* pEncoder, void* nSignalData) = 0;
	virtual void OnFinish(IGModAudioChannelEncoder* pEncoder, GModEncoderStatus nStatus) = 0;
};

class IGModAudioChannel
{
public:
	virtual void Destroy() = 0;
	virtual void Stop() = 0;
	virtual void Pause() = 0;
	virtual void Play() = 0;
	virtual void SetVolume(float) = 0;
	virtual float GetVolume() = 0;
	virtual void SetPlaybackRate(float) = 0;
	virtual float GetPlaybackRate() = 0;
	virtual void SetPos( Vector*, Vector*, Vector* ) = 0;
	virtual void GetPos( Vector*, Vector*, Vector* ) = 0;
	virtual void SetTime( double, bool ) = 0;
	virtual double GetTime() = 0;
	virtual double GetBufferedTime() = 0;
	virtual void Set3DFadeDistance( float, float ) = 0;
	virtual void Get3DFadeDistance( float*, float* ) = 0;
	virtual void Set3DCone( int, int, float ) = 0;
	virtual void Get3DCone( int*, int*, float* ) = 0;
	virtual int GetState() = 0;
	virtual void SetLooping( bool ) = 0;
	virtual bool IsLooping() = 0;
	virtual bool IsOnline() = 0;
	virtual bool Is3D() = 0;
	virtual bool IsBlockStreamed() = 0;
	virtual bool IsValid() = 0;
	virtual double GetLength() = 0;
	virtual const char* GetFileName() = 0;
	virtual int GetSamplingRate() = 0;
	virtual int GetBitsPerSample() = 0;
	virtual float GetAverageBitRate() = 0;
	virtual void GetLevel( float*, float* ) = 0;
	virtual void FFT( float*, GModChannelFFT_t ) = 0;
	virtual void SetChannelPan( float ) = 0;
	virtual float GetChannelPan() = 0;
	virtual const char* GetTags( int ) = 0;
	virtual void Set3DEnabled( bool ) = 0;
	virtual bool Get3DEnabled() = 0;
	virtual void Restart() = 0;
	// HolyLib specific
	// Uses the "DATA" path for writes! Returns NULL on success, else the error message
	// Does NOT require the channel to be a decoder channel!
	// Call IGModAudioChannelEncoder->GetLastError and check if its even valid! (Else it will be invalidated/freed on the next tick)
	virtual IGModAudioChannelEncoder* CreateEncoder( const char* pFileName, unsigned long nFlags, IGModEncoderCallback* pCallback, const char** pErrorOut ) = 0;
	virtual void Update( unsigned long length ) = 0; // Updates the playback buffer
	virtual bool CreateLink( IGModAudioChannel* pChannel, const char** pErrorOut ) = 0;
	virtual bool DestroyLink( IGModAudioChannel* pChannel, const char** pErrorOut ) = 0;
};

class IAudioStreamEvent;
#ifndef CALLBACK
#ifdef _WIN32
#define CALLBACK _stdcall
#else
#define CALLBACK
#endif
#endif

abstract_class IBassAudioStream
{
public:
	virtual ~IBassAudioStream() {};
	virtual unsigned int Decode( void*, uint ) = 0;
	virtual int GetOutputBits() = 0;
	virtual int GetOutputRate() = 0;
	virtual int GetOutputChannels() = 0;
	virtual uint GetPosition() = 0;
	virtual void SetPosition( uint distance ) = 0;
	virtual unsigned long GetHandle() = 0; // unsigned long -> DWORD -> HSTREAM
};

abstract_class IGMod_Audio
{
public:
	virtual ~IGMod_Audio() {};
	virtual bool Init( CreateInterfaceFn ) = 0;
	virtual void Shutdown() = 0;
	virtual void Update( unsigned int ) = 0;
	virtual IBassAudioStream* CreateAudioStream( IAudioStreamEvent* ) = 0;
	virtual void SetEar( Vector*, Vector*, Vector*, Vector* ) = 0;
	virtual IGModAudioChannel* PlayURL( const char* url, const char* flags, int* ) = 0;
	virtual IGModAudioChannel* PlayFile( const char* path, const char* flags, int* ) = 0;
	virtual void SetGlobalVolume( float ) = 0;
	virtual void StopAllPlayback() = 0;
	virtual const char* GetErrorString( int ) = 0;
	// HolyLib specific ones
	virtual unsigned long GetVersion() = 0; // Returns bass version
	virtual bool LoadPlugin(const char* pluginName, const char** pErrorOut) = 0;
	virtual void FinishAllAsync(void* nSignalData) = 0; // Called on Lua shutdown to finish all callbacks/async tasks for that interface
	virtual IGModAudioChannel* CreateDummyChannel(int nSampleRate, int nChannels, unsigned long nFlags, const char** pErrorOut) = 0;
};

#undef CALLBACK // Solves another error with minwindef.h