/*
	BASSenc_FLAC 2.4 C/C++ header file
	Copyright (c) 2017-2020 Un4seen Developments Ltd.

	See the BASSENC_FLAC.CHM file for more detailed documentation
*/

#ifndef BASSENC_FLAC_H
#define BASSENC_FLAC_H

#include "bassenc.h"

#if BASSVERSION!=0x204
#error conflicting BASS and BASSenc_FLAC versions
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BASSENCFLACDEF
#define BASSENCFLACDEF(f) WINAPI f
#endif

// BASS_Encode_FLAC_NewStream flags
#define BASS_ENCODE_FLAC_RESET		0x1000000

typedef DWORD BASSENCFLACDEF(BASS_Encode_FLAC_GetVersion)(void);

typedef HENCODE BASSENCFLACDEF(BASS_Encode_FLAC_Start)(DWORD handle, const char *options, DWORD flags, ENCODEPROCEX *proc, void *user);
typedef HENCODE BASSENCFLACDEF(BASS_Encode_FLAC_StartFile)(DWORD handle, const char *options, DWORD flags, const char *filename);
typedef BOOL BASSENCFLACDEF(BASS_Encode_FLAC_NewStream)(HENCODE handle, const char *options, DWORD flags);

#ifdef __cplusplus
}
#endif

#endif
