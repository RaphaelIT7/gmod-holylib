// MODULE_DEPENDENCY=bass

#pragma once

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
	static void CALLBACK FileClose( void* );
	static QWORD CALLBACK FileLength( void* );
	static DWORD CALLBACK FileRead( void*, DWORD, void* );
	static BOOL CALLBACK FileSeek( QWORD, void* );

public:
	CBassAudioStream();
	bool Init( IAudioStreamEvent* );

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
	virtual ~CGModAudioChannel();

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

extern const char* g_BASSErrorStrings[];