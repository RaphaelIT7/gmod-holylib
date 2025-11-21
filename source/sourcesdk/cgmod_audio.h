// MODULE_DEPENDENCY=bass

#pragma once

#include <string>
#include "vaudio/ivaudio.h"
#include "IGmod_Audio.h"
#include "bass.h"
#include "bassenc.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

class CBassAudioStream : public IBassAudioStream
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

constexpr unsigned int EncoderForceShutdownPointer = 0x1; // Pointer passed to HandleFinish when a forced shutdown for all is done
class CGModAudioChannel;
class CGModAudioChannelEncoder : public IGModAudioChannelEncoder
{
public:
	virtual ~CGModAudioChannelEncoder();

	virtual void ProcessNow(bool bUseAnotherThread);
	virtual void Stop(bool bProcessQueue);
	// Returns true if there was an error, pErrorOut will either be filled or NULL
	// If it returns true, it will also invalidate/free itself so the pointer becomes invalid!
	virtual bool GetLastError(const char** pErrorOut);
	virtual bool ServerInit( const char* port, unsigned long buffer, unsigned long burst, unsigned long flags, const char** pErrorOut );
	virtual bool ServerKick( const char* client );

	virtual void SetPaused( bool bPaused );
	virtual int GetState();

	// virtual IGModAudioChannel* GetChannel();
	virtual void SetChannel( IGModAudioChannel* pChannel, const char** pErrorOut );

	virtual void WriteData(const void* pData, unsigned long nLength);
	virtual const char* GetFileName() { return m_strFileName.c_str(); };
	virtual IGModEncoderCallback* GetCallback() { return m_pCallback; };

	virtual bool CastInit(
		const char* server, const char* password, const char* content,
		const char* name, const char* url, const char* genre, const char* desc,
		char headers[4096], unsigned long bitrate, unsigned long flags, const char** pErrorOut
	);
	virtual void CastSetTitle( const char* title, const char* url );

public: // Non virtual
	CGModAudioChannelEncoder(DWORD pChannel, const char* pFileName, IGModEncoderCallback* pCallback );
	void InitEncoder(unsigned long nEncoderFlags);
	void HandleFinish(void* nSignalData); // Called each Update on the main thread, nSignalData is for the callback we store so that it can decide wether to force finish or not

private:
	static void ProcessChannel(CGModAudioChannelEncoder* pEncoder);
	void WriteToFile(const void* buffer, unsigned long length);
	void OnEncoderFreed();
	void OnEncoderDied();
	bool ServerCallback(bool connect, const char* client, char headers[1024]);

	// Callbacks
public:
	static void CALLBACK EncoderProcessCallback(HENCODE handle, DWORD channel, const void *buffer, DWORD length, void *user);
	static void CALLBACK EncoderProcessCallback(HENCODE handle, DWORD channel, const void *buffer, DWORD length, QWORD offset, void *user); // MP3 & FLAC need this one
	static void CALLBACK EncoderFreedCallback(HENCODE handle, DWORD status, void *user); // Also calls OnDecoderDied 
	static bool CALLBACK EncoderServerClientCallback(HENCODE handle, BOOL connect, const char* client, char headers[1024], void* user);

	HENCODE m_pEncoder;
	FileHandle_t m_pFileHandle;
	std::string m_strFileName;
	std::string m_strLastError;
	DWORD m_pChannel;
	GModEncoderStatus m_nStatus;
	IGModEncoderCallback* m_pCallback;
};

class CGModAudioFX : public IGModAudioFX
{
public:
	virtual ~CGModAudioFX() {};

	virtual void Free(IGModAudioChannel* pChannel);
	virtual void GetParameters( void* params );
	virtual void Reset();
	// virtual void SetBypass( bool bypass );
	virtual bool SetParameters( void* params );
	// virtual void SetPriority( int priority );
	virtual bool IsFX() { return m_bIsFX; };

	CGModAudioFX( DWORD pFXHandle, bool bIsFX );
private:
	DWORD m_pHandle;

	// If false, its a DSP handle.
	bool m_bIsFX;
};

enum ChannelType
{
	CHANNEL_URL = 0,
	CHANNEL_FILE = 1, // 1 so that CGModAudioChannel::m_nType is true/1 for files matching gmods previous var value
	CHANNEL_DUMMY = 2,
	CHANNEL_PUSH = 3,
	CHANNEL_MIXER = 3,
	CHANNEL_SPLIT = 4,
};

class CGMod_Audio;
class CGModAudioChannel : public IGModAudioChannel
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

	// HolyLib specific
	virtual IGModAudioChannelEncoder* CreateEncoder( const char* pFileName, unsigned long nFlags, IGModEncoderCallback* pCallback, const char** pErrorOut );
	virtual void Update( unsigned long length );
	virtual bool CreateLink( IGModAudioChannel* pChannel, const char** pErrorOut );
	virtual bool DestroyLink( IGModAudioChannel* pChannel, const char** pErrorOut );
	virtual void SetAttribute( unsigned long nAttribute, float nValue, const char** pErrorOut );
	virtual void SetSlideAttribute( unsigned long nAttribute, float nValue, unsigned long nTime, const char** pErrorOut );
	virtual float GetAttribute( unsigned long nAttribute, const char** pErrorOut );
	virtual bool IsAttributeSliding( unsigned long nAttribute );
	virtual unsigned long GetChannelData( void* pBuffer, unsigned long nLength );
	
	// FX
	virtual bool SetFX( const char* pFXName, unsigned long nType, int nPriority, void* pParams, const char** pErrorOut );
	virtual void SetFXParameter( const char* pFXName, void* params, const char** pErrorOut );
	// virtual void GetFXParameter( const char* pFXName, void* params );
	// virtual void SetFXPriority( const char* pFXName, int priority );
	// virtual void SetFXBypass( const char* pFXName, bool bypass );
	virtual bool FXReset( const char* pFXName );
	virtual bool FXFree( const char* pFXName );

	// Push functions
	virtual bool IsPush();
	virtual void WriteData(const void* pData, unsigned long nLength, const char** pErrorOut);

	// Mixer functions
	virtual bool IsMixer();
	virtual void AddMixerChannel( IGModAudioChannel* pChannel, unsigned long nFlags, const char** pErrorOut );
	virtual void RemoveMixerChannel();
	virtual int GetMixerState();

	// Splitter functions
	virtual bool IsSplitter();
	virtual void ResetSplitStream();

public:
	CGModAudioChannel( DWORD handle, ChannelType nType, const char* pFileName = NULL );
	virtual ~CGModAudioChannel();

private:
	friend class CGModAudioChannelEncoder;
	friend class CGMod_Audio;
	friend class CGModAudioFX;

	DWORD m_pHandle;
	ChannelType m_nType; //bool m_bIsFile;

	// HolyLib specific
	std::string m_strFileName = "NULL";
	std::unordered_map<std::string, CGModAudioFX*> m_pFX;
};

extern IGMod_Audio* g_pGModAudio;
class CGMod_Audio : public IGMod_Audio
{
public:
	virtual ~CGMod_Audio();
	virtual bool Init( CreateInterfaceFn );
	virtual void Shutdown();
	virtual bool Update( unsigned int );
	virtual IBassAudioStream* CreateAudioStream( IAudioStreamEvent* );
	virtual void SetEar( Vector*, Vector*, Vector*, Vector* );
	virtual IGModAudioChannel* PlayURL( const char* url, const char* flags, int* );
	virtual IGModAudioChannel* PlayFile( const char* path, const char* flags, int* );
	virtual void SetGlobalVolume( float );
	virtual void StopAllPlayback();
	virtual const char* GetErrorString( int );
	// HolyLib
	virtual unsigned long GetVersion();
	virtual bool LoadPlugin(const char* pluginName, const char** pErrorOut);

	virtual void FinishAllAsync(void* nSignalData);
	virtual IGModAudioChannel* CreateDummyChannel(int nSampleRate, int nChannels, unsigned long nFlags, const char** pErrorOut);
	virtual IGModAudioChannel* CreatePushChannel(int nSampleRate, int nChannels, unsigned long nFlags, const char** pErrorOut);
	virtual IGModAudioChannel* CreateMixerChannel(int nSampleRate, int nChannels, unsigned long nFlags, const char** pErrorOut);
	virtual IGModAudioChannel* CreateSplitChannel(IGModAudioChannel* pChannel, unsigned long nFlags, const char** pErrorOut);

public: // Non virtual holylib functions
	void AddEncoder(CGModAudioChannelEncoder* pEncoder) {
		m_pEncoders.insert(pEncoder);
	}

	void RemoveEncoder(CGModAudioChannelEncoder* pEncoder) {
		auto it = m_pEncoders.find(pEncoder);
		if (it != m_pEncoders.end())
			m_pEncoders.erase(it);
	}

private:
	bool LoadDLL(const char* pDLLName, void** pDLLHandle);

	std::unordered_map<std::string, HPLUGIN> m_pLoadedPlugins;
	std::vector<void*> m_pLoadedDLLs;
	std::unordered_set<CGModAudioChannelEncoder*> m_pEncoders;
};

// extern const char* g_BASSErrorStrings[];