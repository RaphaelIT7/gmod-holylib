/*
	BASSenc_OPUS 2.4 C/C++ header file
	Copyright (c) 2016-2020 Un4seen Developments Ltd.

	See the BASSENC_OPUS.CHM file for more detailed documentation
*/

#ifndef BASSENC_OPUS_H
#define BASSENC_OPUS_H

#include "bassenc.h"

#if BASSVERSION!=0x204
#error conflicting BASS and BASSenc_OPUS versions
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BASSENCOPUSDEF
#define BASSENCOPUSDEF(f) WINAPI f
#endif

// BASS_Encode_OPUS_NewStream flags
#define BASS_ENCODE_OPUS_RESET	0x1000000
#define BASS_ENCODE_OPUS_CTLONLY	0x2000000

typedef DWORD BASSENCOPUSDEF(BASS_Encode_OPUS_GetVersion)(void);

typedef HENCODE BASSENCOPUSDEF(BASS_Encode_OPUS_Start)(DWORD handle, const char *options, DWORD flags, ENCODEPROC *proc, void *user);
typedef HENCODE BASSENCOPUSDEF(BASS_Encode_OPUS_StartFile)(DWORD handle, const char *options, DWORD flags, const char *filename);
typedef BOOL BASSENCOPUSDEF(BASS_Encode_OPUS_NewStream)(HENCODE handle, const char *options, DWORD flags);

#ifdef __cplusplus
}
#endif

#endif
