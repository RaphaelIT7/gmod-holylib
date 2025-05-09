//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"

#include "tier0/platform.h"
#include "mathlib/mathlib.h"

#include "materialsystem/imaterialsystem.h"

#ifdef _WIN32
#include "winlite.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace {

void ivu_string_print_function(const char *message) { Msg("%s", message); }

}  // namespace

#ifdef _WIN32
BOOL WINAPI DllMain(HMODULE module, DWORD call_reason, LPVOID) {
  if (call_reason == DLL_PROCESS_ATTACH) {
    ::DisableThreadLibraryCalls(module);
    // ivp_set_message_print_function( ivu_string_print_function );
    MathLib_Init(GAMMA, TEXGAMMA, 0.0f, OVERBRIGHT, false, false, false, false);
  }
  return TRUE;
}

#endif  // _WIN32

#ifdef POSIX
void __attribute__((constructor)) vphysics_init();

void vphysics_init() {
  // ivp_set_message_print_function( ivu_string_print_function );
  MathLib_Init(GAMMA, TEXGAMMA, 0.0f, OVERBRIGHT, false, false, false, false);
}
#endif
