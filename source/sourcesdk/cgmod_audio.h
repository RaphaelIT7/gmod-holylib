#include <string>
#include "vaudio/ivaudio.h"
#include "IGmod_Audio.h"
#include "bass.h"

class CBassAudioStream : IBassAudioStream
{
public:
	virtual ~CBassAudioStream();
	virtual unsigned int Decode( void*, uint );
	virtual int GetOutputBits();
	virtual int GetOutputRate();
	virtual int GetOutputChannels();
	virtual uint GetPosition();
	virtual void SetPosition( uint distance );
	virtual unsigned long GetHandle();
	virtual void CALLBACK MyFileCloseProc( void* );
	virtual unsigned long long CALLBACK MyFileLenProc( void* );
	virtual unsigned long CALLBACK MyFileReadProc( void*, uint, void* );
	virtual bool CALLBACK MyFileSeekProc( unsigned long long, void* );

public:
	CBassAudioStream();
	void Init( IAudioStreamEvent* );

private:
	HSTREAM m_hStream;
};

class CGModAudioChannel : IGModAudioChannel
{
public:
	virtual void Destroy();
	virtual void Stop();
	virtual void Pause();
	virtual void Play();
	virtual void SetVolume(float);
	virtual float GetVolume();
	virtual void SetPlaybackRate(float);
	virtual float GetPlaybackRate();
	virtual void SetPos( Vector*, Vector* = NULL, Vector* = NULL );
	virtual void GetPos( Vector*, Vector*, Vector* );
	virtual void SetTime( double, bool );
	virtual double GetTime();
	virtual double GetBufferedTime();
	virtual void Set3DFadeDistance( float, float );
	virtual void Get3DFadeDistance( float*, float* );
	virtual void Set3DCone( int, int, float );
	virtual void Get3DCone( int*, int*, float* );
	virtual int GetState();
	virtual void SetLooping( bool );
	virtual bool IsLooping();
	virtual bool IsOnline();
	virtual bool Is3D();
	virtual bool IsBlockStreamed();
	virtual bool IsValid();
	virtual double GetLength();
	virtual const char* GetFileName();
	virtual int GetSamplingRate();
	virtual int GetBitsPerSample();
	virtual float GetAverageBitRate();
	virtual void GetLevel( float*, float* );
	virtual void FFT( float*, GModChannelFFT_t );
	virtual void SetChannelPan( float );
	virtual float GetChannelPan();
	virtual const char* GetTags( int );
	virtual void Set3DEnabled( bool );
	virtual bool Get3DEnabled();
	virtual void Restart();
public:
	CGModAudioChannel( DWORD handle, bool isfile );

private:
	DWORD m_pHandle;
	bool m_bIsFile;
};

extern IGMod_Audio* g_pGModAudio;
class CGMod_Audio : public IGMod_Audio
{
public:
	virtual ~CGMod_Audio();
	virtual bool Init( CreateInterfaceFn );
	virtual void Shutdown();
	virtual void Update( unsigned int );
	virtual IBassAudioStream* CreateAudioStream( IAudioStreamEvent* );
	virtual void SetEar( Vector*, Vector*, Vector*, Vector* );
	virtual IGModAudioChannel* PlayURL( const char* url, const char* flags, int* );
	virtual IGModAudioChannel* PlayFile( const char* path, const char* flags, int* );
	virtual void SetGlobalVolume( float );
	virtual void StopAllPlayback();
	virtual const char* GetErrorString( int );
};

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