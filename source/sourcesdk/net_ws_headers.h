//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef NET_WS_HEADERS_H
#define NET_WS_HEADERS_H
#ifdef _WIN32
#pragma once
#endif


#ifdef _WIN32
#if !defined( _X360 )
#include "winlite.h"
#endif
#endif

#include "vstdlib/random.h"
#include "convar.h"
#include "tier0/icommandline.h"
#include "proto_oob.h"
#include "net_chan.h"
#include "inetmsghandler.h"
#include "protocol.h" // CONNECTIONLESS_HEADER
#include "tier0/tslist.h"
#include "tier1/mempool.h"

#if defined(_WIN32)

#if !defined( _X360 )
#include <winsock.h>
#else
#include "winsockx.h"
#endif

// #include <process.h>
typedef int socklen_t;

#elif defined POSIX

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define WSAEWOULDBLOCK		EWOULDBLOCK
#define WSAEMSGSIZE			EMSGSIZE
#define WSAEADDRNOTAVAIL	EADDRNOTAVAIL
#define WSAEAFNOSUPPORT		EAFNOSUPPORT
#define WSAECONNRESET		ECONNRESET
#define WSAECONNREFUSED     ECONNREFUSED
#define WSAEADDRINUSE		EADDRINUSE
#define WSAENOTCONN			ENOTCONN

#define ioctlsocket ioctl
#define closesocket close

#undef SOCKET
typedef int SOCKET;
#define FAR

#endif

#endif // NET_WS_HEADERS_H