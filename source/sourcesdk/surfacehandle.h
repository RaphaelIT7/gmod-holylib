//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef SURFACEHANDLE_H
#define SURFACEHANDLE_H
#ifdef _WIN32
#pragma once
#endif

struct msurface2_t;
using SurfaceHandle_t = msurface2_t *;
using SurfaceHandleRestrict_t = msurface2_t * RESTRICT;
constexpr SurfaceHandle_t SURFACE_HANDLE_INVALID{ nullptr };

#define IS_SURF_VALID(surfID) ( (surfID) != SURFACE_HANDLE_INVALID )

#endif // SURFACEHANDLE_H