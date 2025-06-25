#pragma once
#include "opus/include/opus.h"
#include <iterator>
#include <cstdint>
#include <algorithm>
#include <deque>
#include <vector>
#include <opus/ivoicecodec.h>

namespace SteamOpus {

    #define SAMPLERATE_GMOD_OPUS 24000
    #define FRAME_SIZE_GMOD 480

    #define CHK_BUF_ACCESS(varName, start, end, type)  \
        if(start + sizeof(type) > end) \
            return -1;                 \
        type varName = *(type*)start;\
        start += sizeof(type);

    #define CHK_BUF_WRITE(start, end, type, val) \
        if(start + sizeof(type) > end) \
            return -1;                  \
        *(type*)start = val;            \
        start += sizeof(type);

    enum opcodes {
        OP_CODEC_OPUSPLC = 6,
        OP_SAMPLERATE = 11,
        OP_SILENCE = 0
    };

    class Opus_FrameDecoder : public IVoiceCodec {
    private:
        Opus_FrameDecoder(const Opus_FrameDecoder&) {}
        Opus_FrameDecoder& operator=(const Opus_FrameDecoder&) = delete;

    public:
        Opus_FrameDecoder() {
            int error = 0;

            dec = opus_decoder_create(SAMPLERATE_GMOD_OPUS, 1, &error);
            enc = opus_encoder_create(SAMPLERATE_GMOD_OPUS, 1, OPUS_APPLICATION_VOIP, &error);
        }

        virtual bool Init(int quality, int sampleRate) {
            return true;
        }

        virtual int	GetSampleRate() {
            return SAMPLERATE_GMOD_OPUS;
        }

        virtual bool ResetState() {
            opus_decoder_ctl(dec, OPUS_RESET_STATE);
            opus_encoder_ctl(enc, OPUS_RESET_STATE);
            return true;
        }

        virtual void Release() {}

        virtual int	Compress(const char* pUncompressed, int nSamples, char* pCompressed, int maxCompressedBytes, bool bFinal) {
            if (!nSamples) return 0;

            const char* const pCompressedBase = pCompressed;
            char* pCompressedEnd = pCompressed + maxCompressedBytes;

            if (sample_buf.size() + nSamples < FRAME_SIZE_GMOD && !bFinal) {
                sample_buf.insert(sample_buf.end(), (const uint16_t*)pUncompressed, (const uint16_t*)pUncompressed + nSamples);
                return 0;
            }

            std::vector<uint16_t> temp_buf(sample_buf.begin(), sample_buf.end());
            sample_buf.clear();

            uint32_t remainder = (temp_buf.size() + nSamples) % FRAME_SIZE_GMOD;
            temp_buf.insert(temp_buf.end(), (const uint16_t*)pUncompressed, (const uint16_t*)pUncompressed + nSamples);

            if (remainder) {
                if (bFinal) {
                    // if bFinal do not dump in queue and fill instead
                    std::fill_n(std::back_inserter(temp_buf), FRAME_SIZE_GMOD - remainder, 0);
                } else {
                    // Dump left overs in queue
                    sample_buf.insert(sample_buf.end(), temp_buf.end() - remainder, temp_buf.end());
                    temp_buf.erase(temp_buf.end() - remainder, temp_buf.end());
                }
            }

            for (uint32_t i = 0; i < temp_buf.size(); i += FRAME_SIZE_GMOD) {
                uint16_t* chunk = temp_buf.data() + i;

                if (pCompressed + sizeof(uint16_t) > pCompressedEnd)
                    return -1;

                uint16_t* chunk_len = (uint16_t*)pCompressed;
                pCompressed += sizeof(uint16_t);

                CHK_BUF_WRITE(pCompressed, pCompressedEnd, uint16_t, m_encodeSeq++);

                int bytes_written = opus_encode(enc, (opus_int16*)chunk, FRAME_SIZE_GMOD, (unsigned char*)pCompressed, std::min<uint64_t>(0x7FFF, pCompressedEnd - pCompressed));
                if (bytes_written < 0)
                    return -1;

                *chunk_len = bytes_written;
                pCompressed += bytes_written;
            }

            if (bFinal) {
                opus_encoder_ctl(enc, OPUS_RESET_STATE);
                m_encodeSeq = 0;
                CHK_BUF_WRITE(pCompressed, pCompressedEnd, uint16_t, 0xFFFF);
            }

            return pCompressed - pCompressedBase;
        }

        virtual int	Decompress(const char* pCompressed, int compressedBytes, char* pUncompressed, int maxUncompressedBytes) {
            const char* const pUncompressedOrig = pUncompressed;
            const char* const pEnd = pCompressed + compressedBytes;
            const char* const pUncompressedEnd = pUncompressed + maxUncompressedBytes;

            while (pCompressed + sizeof(uint16_t) <= pEnd) {
                CHK_BUF_ACCESS(len, pCompressed, pEnd, uint16_t);

                if (len == 0xFFFF) {
                    opus_decoder_ctl(dec, OPUS_RESET_STATE);
                    m_seq = 0;
                    continue;
                }

                CHK_BUF_ACCESS(seq, pCompressed, pEnd, uint16_t);

                if (seq < m_seq) {
                    opus_decoder_ctl(dec, OPUS_RESET_STATE);
                } else if (seq > m_seq) {
                    uint32_t lostFrames = std::min(seq - m_seq, 10);

                    for (uint32_t i = 0; i < lostFrames; i++) {
                        if (pUncompressedEnd - pUncompressed <= 0)
                            return -1;

                        int samples = opus_decode(dec, 0, 0, (opus_int16*)pUncompressed, (pUncompressedEnd - pUncompressed) / 2, 0);
                        if (samples < 0)
                            break;

                        pUncompressed += samples * 2;
                    }
                    m_seq = seq;
                }

                m_seq = seq + 1;

                if (len == 0 || pCompressed + len > pEnd)
                    return -1;

                int samples = opus_decode(dec, (const unsigned char*)pCompressed, len, (opus_int16*)pUncompressed, (pUncompressedEnd - pUncompressed) / 2, 0);
                if (samples < 0)
                    return -1;

                pUncompressed += samples * 2;
                pCompressed += len;
            }

            // Return number of samples written to pUncompressed
            return (pUncompressed - pUncompressedOrig) / sizeof(uint16_t);
        }

        virtual ~Opus_FrameDecoder() {
            opus_decoder_destroy(dec);
            opus_encoder_destroy(enc);
        }

    private:
        uint16_t m_seq = 0;
        uint16_t m_encodeSeq = 0;
        OpusDecoder* dec = nullptr;
        OpusEncoder* enc = nullptr;
        std::deque<uint16_t> sample_buf;
    };
}
