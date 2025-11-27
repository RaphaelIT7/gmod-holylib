//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A higher level link library for general use in the game and tools.
//
//===========================================================================//

#include <tier2/tier2.h>
#include "tier0/dbg.h"
#include "filesystem.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/IColorCorrection.h"
#include "materialsystem/idebugtextureinfo.h"
#include "materialsystem/ivballoctracker.h"
#include "inputsystem/iinputsystem.h"
#include "networksystem/inetworksystem.h"
#include "p4lib/ip4.h"
#include "mdllib/mdllib.h"
#include "filesystem/IQueuedLoader.h"
#include "Platform.hpp"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// These tier2 libraries must be set by any users of this library.
// They can be set by calling ConnectTier2Libraries or InitDefaultFileSystem.
// It is hoped that setting this, and using this library will be the common mechanism for
// allowing link libraries to access tier2 library interfaces
//-----------------------------------------------------------------------------
#if ARCHITECTURE_IS_X86
IFileSystem *g_pFullFileSystem = nullptr;
IMaterialSystem *materials = nullptr;
IMaterialSystem *g_pMaterialSystem = nullptr;
IInputSystem *g_pInputSystem = nullptr;
INetworkSystem *g_pNetworkSystem = nullptr;
IMaterialSystemHardwareConfig *g_pMaterialSystemHardwareConfig = nullptr;
IDebugTextureInfo *g_pMaterialSystemDebugTextureInfo = nullptr;
IVBAllocTracker *g_VBAllocTracker = nullptr;
IColorCorrectionSystem *colorcorrection = nullptr;
IMdlLib *mdllib = nullptr;
IQueuedLoader *g_pQueuedLoader = nullptr;
#endif
IP4 *p4 = nullptr;


//-----------------------------------------------------------------------------
// Call this to connect to all tier 2 libraries.
// It's up to the caller to check the globals it cares about to see if ones are missing
//-----------------------------------------------------------------------------
void ConnectTier2Libraries( CreateInterfaceFn *pFactoryList, int nFactoryCount )
{
	// Don't connect twice..
	Assert( !g_pFullFileSystem && !materials && !g_pInputSystem && !g_pNetworkSystem && 
		!p4 && !mdllib && !g_pMaterialSystemDebugTextureInfo && !g_VBAllocTracker &&
		!g_pMaterialSystemHardwareConfig && !g_pQueuedLoader );

	for ( int i = 0; i < nFactoryCount; ++i )
	{
		if ( !g_pFullFileSystem )
		{
			g_pFullFileSystem = ( IFileSystem * )pFactoryList[i]( FILESYSTEM_INTERFACE_VERSION, nullptr );
		}
		if ( !materials )
		{
			g_pMaterialSystem = materials = ( IMaterialSystem * )pFactoryList[i]( MATERIAL_SYSTEM_INTERFACE_VERSION, nullptr );
		}
		if ( !g_pInputSystem )
		{
			g_pInputSystem = ( IInputSystem * )pFactoryList[i]( INPUTSYSTEM_INTERFACE_VERSION, nullptr );
		}
		if ( !g_pNetworkSystem )
		{
			g_pNetworkSystem = ( INetworkSystem * )pFactoryList[i]( NETWORKSYSTEM_INTERFACE_VERSION, nullptr );
		}
		if ( !g_pMaterialSystemHardwareConfig )
		{
			g_pMaterialSystemHardwareConfig = ( IMaterialSystemHardwareConfig * )pFactoryList[i]( MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION, nullptr );
		}
		if ( !g_pMaterialSystemDebugTextureInfo )
		{
			g_pMaterialSystemDebugTextureInfo = (IDebugTextureInfo*)pFactoryList[i]( DEBUG_TEXTURE_INFO_VERSION, nullptr );
		}
		if ( !g_VBAllocTracker )
		{
			g_VBAllocTracker = (IVBAllocTracker*)pFactoryList[i]( VB_ALLOC_TRACKER_INTERFACE_VERSION, nullptr );
		}
		if ( !colorcorrection )
		{
			colorcorrection = ( IColorCorrectionSystem * )pFactoryList[i]( COLORCORRECTION_INTERFACE_VERSION, nullptr );
		}
		if ( !p4 )
		{
			p4 = ( IP4 * )pFactoryList[i]( P4_INTERFACE_VERSION, nullptr );
		}
		if ( !mdllib )
		{
			mdllib = ( IMdlLib * )pFactoryList[i]( MDLLIB_INTERFACE_VERSION, nullptr );
		}
		if ( !g_pQueuedLoader )
		{
			g_pQueuedLoader = (IQueuedLoader *)pFactoryList[i]( QUEUEDLOADER_INTERFACE_VERSION, nullptr );
		}
	}
}

void DisconnectTier2Libraries()
{

	g_pFullFileSystem = nullptr;
	materials = g_pMaterialSystem = nullptr;
	g_pMaterialSystemHardwareConfig = nullptr;
	g_pMaterialSystemDebugTextureInfo = nullptr;
	g_pInputSystem = nullptr;
	g_pNetworkSystem = nullptr;
	colorcorrection = nullptr;
	p4 = nullptr;
	mdllib = nullptr;
	g_pQueuedLoader = nullptr;
}