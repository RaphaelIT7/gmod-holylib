/*
	BASSenc_MP3 2.4 C/C++ header file
	Copyright (c) 2018 Un4seen Developments Ltd.

	See the BASSENC_MP3.CHM file for more detailed documentation
*/

#ifndef BASSENC_MP3_H
#define BASSENC_MP3_H

#include "bassenc.h"

#if BASSVERSION!=0x204
#error conflicting BASS and BASSenc_MP3 versions
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BASSENCMP3DEF
#define BASSENCMP3DEF(f) WINAPI f
#endif

typedef DWORD BASSENCMP3DEF(BASS_Encode_MP3_GetVersion)(void);

typedef HENCODE BASSENCMP3DEF(BASS_Encode_MP3_Start)(DWORD handle, const char *options, DWORD flags, ENCODEPROCEX *proc, void *user);
typedef HENCODE BASSENCMP3DEF(BASS_Encode_MP3_StartFile)(DWORD handle, const char *options, DWORD flags, const char *filename);

#ifdef __cplusplus
}
#endif

#endif
