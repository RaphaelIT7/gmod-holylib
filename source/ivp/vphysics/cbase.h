//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// system
#include <cstdio>

// Valve
#include "tier0/dbg.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "tier1/utlvector.h"
#include "convert.h"
#include "tier0/commonmacros.h"

// vphysics
#include "vphysics_interface.h"
#include "vphysics_saverestore.h"
#include "vphysics_internal.h"
#include "physics_material.h"
#include "physics_environment.h"
#include "physics_object.h"

#undef max
#undef min // Fix valve includes.

// ivp
#include "ivp_physics.hxx"
#include "ivp_core.hxx"
#include "ivp_templates.hxx"

// havok

// Other

// dimhotepus: Add type-safe interface.
#define stackallocT( type_, _size )		static_cast<type_*>( stackalloc( sizeof(type_) * (_size) ) )

// dimhotepus: ssize support.
#ifdef __cpp_lib_ssize
// C++20 ssize
using std::ssize;
#else
#include <type_traits>

template <class C>
constexpr auto ssize(const C& c) noexcept(noexcept(c.size()))
    -> std::common_type_t<std::ptrdiff_t,
                          std::make_signed_t<decltype(c.size())>> {
  using R = std::common_type_t<std::ptrdiff_t,
                               std::make_signed_t<decltype(c.size())>>;
  return static_cast<R>(c.size());
}

template <class T, std::ptrdiff_t N>
constexpr std::ptrdiff_t ssize(const T (&)[N]) noexcept {
  return N;
}
#endif

#include <memory> // for std::make_shared