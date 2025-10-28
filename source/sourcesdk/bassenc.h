/*
	BASSenc 2.4 C/C++ header file
	Copyright (c) 2003-2022 Un4seen Developments Ltd.

	See the BASSENC.CHM file for more detailed documentation
*/

#ifndef BASSENC_H
#define BASSENC_H

#include "bass.h"

#if BASSVERSION!=0x204
#error conflicting BASS and BASSenc versions
#endif

#ifdef __OBJC__
typedef int BOOL32;
#define BOOL BOOL32 // override objc's BOOL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BASSENCDEF
#define BASSENCDEF(f) WINAPI f
#endif

typedef DWORD HENCODE;		// encoder handle

// Additional error codes returned by BASS_ErrorGetCode
#define BASS_ERROR_ACM_CANCEL	2000	// ACM codec selection cancelled
#define BASS_ERROR_CAST_DENIED	2100	// access denied (invalid password)
#define BASS_ERROR_SERVER_CERT	2101	// missing/invalid certificate

// Additional BASS_SetConfig options
#define BASS_CONFIG_ENCODE_PRIORITY		0x10300
#define BASS_CONFIG_ENCODE_QUEUE		0x10301
#define BASS_CONFIG_ENCODE_CAST_TIMEOUT	0x10310

// Additional BASS_SetConfigPtr options
#define BASS_CONFIG_ENCODE_ACM_LOAD		0x10302
#define BASS_CONFIG_ENCODE_CAST_PROXY	0x10311
#define BASS_CONFIG_ENCODE_CAST_BIND	0x10312
#define BASS_CONFIG_ENCODE_SERVER_CERT	0x10320
#define BASS_CONFIG_ENCODE_SERVER_KEY	0x10321

// BASS_Encode_Start flags
#define BASS_ENCODE_NOHEAD		1		// don't send a WAV header to the encoder
#define BASS_ENCODE_FP_8BIT		2		// convert floating-point sample data to 8-bit integer
#define BASS_ENCODE_FP_16BIT	4		// convert floating-point sample data to 16-bit integer
#define BASS_ENCODE_FP_24BIT	6		// convert floating-point sample data to 24-bit integer
#define BASS_ENCODE_FP_32BIT	8		// convert floating-point sample data to 32-bit integer
#define BASS_ENCODE_FP_AUTO		14		// convert floating-point sample data back to channel's format
#define BASS_ENCODE_BIGEND		16		// big-endian sample data
#define BASS_ENCODE_PAUSE		32		// start encording paused
#define BASS_ENCODE_PCM			64		// write PCM sample data (no encoder)
#define BASS_ENCODE_RF64		128		// send an RF64 header
#define BASS_ENCODE_MONO		0x100	// convert to mono (if not already)
#define BASS_ENCODE_QUEUE		0x200	// queue data to feed encoder asynchronously
#define BASS_ENCODE_WFEXT		0x400	// WAVEFORMATEXTENSIBLE "fmt" chunk
#define BASS_ENCODE_CAST_NOLIMIT 0x1000	// don't limit casting data rate
#define BASS_ENCODE_LIMIT		0x2000	// limit data rate to real-time
#define BASS_ENCODE_AIFF		0x4000	// send an AIFF header rather than WAV
#define BASS_ENCODE_DITHER		0x8000	// apply dither when converting floating-point sample data to integer
#define BASS_ENCODE_AUTOFREE	0x40000 // free the encoder when the channel is freed

// BASS_Encode_GetACMFormat flags
#define BASS_ACM_DEFAULT		1	// use the format as default selection
#define BASS_ACM_RATE			2	// only list formats with same sample rate as the source channel
#define BASS_ACM_CHANS			4	// only list formats with same number of channels (eg. mono/stereo)
#define BASS_ACM_SUGGEST		8	// suggest a format (HIWORD=format tag)

// BASS_Encode_GetCount counts
#define BASS_ENCODE_COUNT_IN			0	// sent to encoder
#define BASS_ENCODE_COUNT_OUT			1	// received from encoder
#define BASS_ENCODE_COUNT_CAST			2	// sent to cast server
#define BASS_ENCODE_COUNT_QUEUE			3	// queued
#define BASS_ENCODE_COUNT_QUEUE_LIMIT	4	// queue limit
#define BASS_ENCODE_COUNT_QUEUE_FAIL	5	// failed to queue
#define BASS_ENCODE_COUNT_IN_FP			6	// sent to encoder before floating-point conversion

// BASS_Encode_CastInit content MIME types
#define BASS_ENCODE_TYPE_MP3	"audio/mpeg"
#define BASS_ENCODE_TYPE_OGG	"audio/ogg"
#define BASS_ENCODE_TYPE_AAC	"audio/aacp"

// BASS_Encode_CastInit flags
#define BASS_ENCODE_CAST_PUBLIC			1	// add to public directory
#define BASS_ENCODE_CAST_PUT			2	// use PUT method
#define BASS_ENCODE_CAST_SSL			4	// use SSL/TLS encryption

// BASS_Encode_CastGetStats types
#define BASS_ENCODE_STATS_SHOUT		0	// Shoutcast stats
#define BASS_ENCODE_STATS_ICE		1	// Icecast mount-point stats
#define BASS_ENCODE_STATS_ICESERV	2	// Icecast server stats

// BASS_Encode_ServerInit flags
#define BASS_ENCODE_SERVER_NOHTTP		1	// no HTTP headers
#define BASS_ENCODE_SERVER_META			2	// Shoutcast metadata
#define BASS_ENCODE_SERVER_SSL			4	// support SSL/TLS encryption
#define BASS_ENCODE_SERVER_SSLONLY		8	// require SSL/TLS encryption

typedef void (CALLBACK ENCODEPROC)(HENCODE handle, DWORD channel, const void *buffer, DWORD length, void *user);
/* Encoding callback function.
handle : The encoder
channel: The channel handle
buffer : Buffer containing the encoded data
length : Number of bytes
user   : The 'user' parameter value given when starting the encoder */

typedef void (CALLBACK ENCODEPROCEX)(HENCODE handle, DWORD channel, const void *buffer, DWORD length, QWORD offset, void *user);
/* Encoding callback function with offset info.
handle : The encoder
channel: The channel handle
buffer : Buffer containing the encoded data
length : Number of bytes
offset : File offset of the data
user   : The 'user' parameter value given when starting the encoder */

typedef DWORD (CALLBACK ENCODERPROC)(HENCODE handle, DWORD channel, void *buffer, DWORD length, DWORD maxout, void *user);
/* Encoder callback function.
handle : The encoder
channel: The channel handle
buffer : Buffer containing the PCM data (input) and receiving the encoded data (output)
length : Number of bytes in (-1=closing)
maxout : Maximum number of bytes out
user   : The 'user' parameter value given when calling BASS_Encode_StartUser
RETURN : The amount of encoded data (-1=stop) */

typedef BOOL (CALLBACK ENCODECLIENTPROC)(HENCODE handle, BOOL connect, const char *client, char *headers, void *user);
/* Client connection notification callback function.
handle : The encoder
connect: TRUE/FALSE=client is connecting/disconnecting
client : The client's address (xxx.xxx.xxx.xxx:port)
headers: Request headers (optionally response headers on return)
user   : The 'user' parameter value given when calling BASS_Encode_ServerInit
RETURN : TRUE/FALSE=accept/reject connection (ignored if connect=FALSE) */

typedef void (CALLBACK ENCODENOTIFYPROC)(HENCODE handle, DWORD status, void *user);
/* Encoder death notification callback function.
handle : The encoder
status : Notification (BASS_ENCODE_NOTIFY_xxx)
user   : The 'user' parameter value given when calling BASS_Encode_SetNotify */

// Encoder notifications
#define BASS_ENCODE_NOTIFY_ENCODER		1	// encoder died
#define BASS_ENCODE_NOTIFY_CAST			2	// cast server connection died
#define BASS_ENCODE_NOTIFY_SERVER		3	// server died
#define BASS_ENCODE_NOTIFY_CAST_TIMEOUT	0x10000 // cast timeout
#define BASS_ENCODE_NOTIFY_QUEUE_FULL	0x10001	// queue is out of space
#define BASS_ENCODE_NOTIFY_FREE			0x10002	// encoder has been freed

typedef DWORD BASSENCDEF(BASS_Encode_GetVersion)(void);

typedef HENCODE BASSENCDEF(BASS_Encode_Start)(DWORD handle, const char *cmdline, DWORD flags, ENCODEPROC *proc, void *user);
typedef HENCODE BASSENCDEF(BASS_Encode_StartLimit)(DWORD handle, const char *cmdline, DWORD flags, ENCODEPROC *proc, void *user, DWORD limit);
typedef HENCODE BASSENCDEF(BASS_Encode_StartUser)(DWORD handle, const char *filename, DWORD flags, ENCODERPROC *proc, void *user);
typedef BOOL BASSENCDEF(BASS_Encode_AddChunk)(HENCODE handle, const char *id, const void *buffer, DWORD length);
typedef BOOL BASSENCDEF(BASS_Encode_Write)(DWORD handle, const void *buffer, DWORD length);
typedef BOOL BASSENCDEF(BASS_Encode_Stop)(DWORD handle);
typedef BOOL BASSENCDEF(BASS_Encode_StopEx)(DWORD handle, BOOL queue);
typedef BOOL BASSENCDEF(BASS_Encode_SetPaused)(DWORD handle, BOOL paused);
typedef DWORD BASSENCDEF(BASS_Encode_IsActive)(DWORD handle);
typedef BOOL BASSENCDEF(BASS_Encode_SetNotify)(DWORD handle, ENCODENOTIFYPROC *proc, void *user);
typedef QWORD BASSENCDEF(BASS_Encode_GetCount)(HENCODE handle, DWORD count);
typedef BOOL BASSENCDEF(BASS_Encode_SetChannel)(DWORD handle, DWORD channel);
typedef DWORD BASSENCDEF(BASS_Encode_GetChannel)(HENCODE handle);
typedef BOOL BASSENCDEF(BASS_Encode_UserOutput)(HENCODE handle, QWORD offset, const void *buffer, DWORD length);

#ifdef _WIN32
typedef DWORD BASSENCDEF(BASS_Encode_GetACMFormat)(DWORD handle, void *form, DWORD formlen, const char *title, DWORD flags);
typedef HENCODE BASSENCDEF(BASS_Encode_StartACM)(DWORD handle, const void *form, DWORD flags, ENCODEPROC *proc, void *user);
typedef HENCODE BASSENCDEF(BASS_Encode_StartACMFile)(DWORD handle, const void *form, DWORD flags, const char *filename);
#endif

#ifdef __APPLE__
typedef HENCODE BASSENCDEF(BASS_Encode_StartCA)(DWORD handle, DWORD ftype, DWORD atype, DWORD flags, DWORD bitrate, ENCODEPROCEX *proc, void *user);
typedef HENCODE BASSENCDEF(BASS_Encode_StartCAFile)(DWORD handle, DWORD ftype, DWORD atype, DWORD flags, DWORD bitrate, const char *filename);
typedef void *BASSENCDEF(BASS_Encode_GetCARef)(DWORD handle);
#endif

#ifndef _WIN32_WCE
typedef BOOL BASSENCDEF(BASS_Encode_CastInit)(HENCODE handle, const char *server, const char *pass, const char *content, const char *name, const char *url, const char *genre, const char *desc, const char *headers, DWORD bitrate, DWORD flags);
typedef BOOL BASSENCDEF(BASS_Encode_CastSetTitle)(HENCODE handle, const char *title, const char *url);
typedef BOOL BASSENCDEF(BASS_Encode_CastSendMeta)(HENCODE handle, DWORD type, const void *data, DWORD length);
typedef const char *BASSENCDEF(BASS_Encode_CastGetStats)(HENCODE handle, DWORD type, const char *pass);

typedef DWORD BASSENCDEF(BASS_Encode_ServerInit)(HENCODE handle, const char *port, DWORD buffer, DWORD burst, DWORD flags, ENCODECLIENTPROC *proc, void *user);
typedef BOOL BASSENCDEF(BASS_Encode_ServerKick)(HENCODE handle, const char *client);
#endif

#ifdef __cplusplus
}
#endif

#ifdef __OBJC__
#undef BOOL
#endif

#endif
