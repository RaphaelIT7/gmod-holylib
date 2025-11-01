/*
	BASSenc_OGG 2.4 C/C++ header file
	Copyright (c) 2016-2020 Un4seen Developments Ltd.

	See the BASSENC_OGG.CHM file for more detailed documentation
*/

#ifndef BASSENC_OGG_H
#define BASSENC_OGG_H

#include "bassenc.h"

#if BASSVERSION!=0x204
#error conflicting BASS and BASSenc_OGG versions
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BASSENCOGGDEF
#define BASSENCOGGDEF(f) WINAPI f
#endif

// BASS_Encode_OGG_NewStream flags
#define BASS_ENCODE_OGG_RESET		0x1000000

typedef DWORD BASSENCOGGDEF(BASS_Encode_OGG_GetVersion)(void);

typedef HENCODE BASSENCOGGDEF(BASS_Encode_OGG_Start)(DWORD handle, const char *options, DWORD flags, ENCODEPROC *proc, void *user);
typedef HENCODE BASSENCOGGDEF(BASS_Encode_OGG_StartFile)(DWORD handle, const char *options, DWORD flags, const char *filename);
typedef BOOL BASSENCOGGDEF(BASS_Encode_OGG_NewStream)(HENCODE handle, const char *options, DWORD flags);

#ifdef __cplusplus
}
#endif

#endif
