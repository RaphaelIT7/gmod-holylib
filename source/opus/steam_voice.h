#pragma once
#include <cstdint>
#include <opus/ivoicecodec.h>
#include <checksum_crc.h>

namespace SteamVoice {
	enum {
		OP_SILENCE = 0,
		OP_CODEC_OPUSPLC = 6,
		OP_SAMPLERATE = 11
	};

	//Outputs bytes written or -1 on corruption
	int DecompressIntoBuffer(IVoiceCodec* codec, const char* compressedData, int compressedLen, char* decompressedOut, int maxDecompressed) {
		const char* curRead = compressedData;
		const char* maxRead = compressedData + compressedLen;
		char* curWrite = decompressedOut;
		char* maxWrite = decompressedOut + maxDecompressed;

		//Strip steamid at beginning of packet and crc at end of packet
		curRead += sizeof(uint64_t);
		maxRead -= sizeof(uint32_t);

		while (curRead < maxRead) {
			//Check to make sure we have one byte of buffer space remaining at least
			if (curRead + sizeof(char) > maxRead)
				return -1;

			//Get the current packet opcode
			char opcode = *curRead;
			curRead += sizeof(char);

			switch (opcode) {
			case OP_SILENCE: {
				//Contains a number of silence samples to add to the decompressed data. Skip for now.
				if (curRead + sizeof(uint16_t) > maxRead)
					return -1;

				curRead += sizeof(uint16_t);
				break;
			}
			case OP_SAMPLERATE: {
				//Contains the samplerate for the stream. Always 24000 as far as I can tell.
				if (curRead + sizeof(uint16_t) > maxRead)
					return -1;

				//Something like this could be used to read the sample rate if we needed it. 
				//uint16_t sampleRate = *(uint16_t*)curRead;

				curRead += sizeof(uint16_t);
				break;
			}
			case OP_CODEC_OPUSPLC: {
				//Contains length plus a number of steam opus frames
				if (curRead + sizeof(uint16_t) > maxRead)
					return -1;

				uint16_t frameDataLen = *(uint16_t*)curRead;
				curRead += sizeof(uint16_t);
				if (curRead + frameDataLen > maxRead)
					return -1;

				int decompressedSamples = codec->Decompress(curRead, frameDataLen, curWrite, maxWrite-curWrite);
				if (decompressedSamples <= 0)
					return -1;

				curWrite += decompressedSamples*2;
				curRead += frameDataLen;
				break;
			}
			default:
				return -1;
			}
		}

		return curWrite - decompressedOut;
	}

	//Outputs number of bytes written or -1 on failure
	int CompressIntoBuffer(uint64_t steamid, IVoiceCodec* codec, const char* inputData, int inputLen, char* compressedOut, int maxCompressed, int sampleRate) {
		char* curWrite = compressedOut;
		char* maxWrite = compressedOut + maxCompressed;

		if (curWrite + sizeof(uint64_t) > maxWrite)
			return -1;

		*(uint64_t*)curWrite = steamid;
		curWrite += sizeof(uint64_t);

		//Write sample rate operation
		if (curWrite + sizeof(char) + sizeof(uint16_t) > maxWrite)
			return -1;

		*curWrite = OP_SAMPLERATE;
		curWrite += sizeof(char);
		*(uint16_t*)curWrite = sampleRate;
		curWrite += sizeof(uint16_t);

		//Write opus codec operation
		if (curWrite + sizeof(char) + sizeof(uint16_t) > maxWrite)
			return -1;

		*curWrite = OP_CODEC_OPUSPLC;
		curWrite += sizeof(char);

		//Setup address to write to with compression length 
		uint16_t* outLenAddr = (uint16_t*)curWrite;
		curWrite += sizeof(uint16_t);

		int compressedBytes = codec->Compress(inputData, inputLen / 2, curWrite, maxWrite - curWrite, false);

		if (compressedBytes < 0)
			return -1;

		curWrite += compressedBytes;
		*outLenAddr = compressedBytes;

		if (curWrite + sizeof(CRC32_t) > maxWrite)
			return -1;

		CRC32_t crc = CRC32_ProcessSingleBuffer(compressedOut, curWrite - compressedOut);
		*(CRC32_t*)(curWrite) = crc;

		curWrite += sizeof(CRC32_t);

		return curWrite - compressedOut;
	}
}