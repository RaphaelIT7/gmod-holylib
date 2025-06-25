#pragma once
class IVoiceCodec
{
public:
	virtual			~IVoiceCodec() {}
	virtual bool	Init(int quality, int sampleRate) = 0;
	virtual int		GetSampleRate() = 0;
	virtual void	Release() = 0;
	virtual int		Compress(const char *pUncompressed, int nSamples, char *pCompressed, int maxCompressedBytes, bool bFinal) = 0;
	virtual int		Decompress(const char *pCompressed, int compressedBytes, char *pUncompressed, int maxUncompressedBytes) = 0;
	virtual bool	ResetState() = 0;
};