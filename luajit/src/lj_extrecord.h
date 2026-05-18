/*
** External trace recorder
** Based of lj_ffrecord.c -> Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_EXTRECORD_H
#define _LJ_EXTRECORD_H

#include "lj_obj.h"
#include "lj_jit.h"
#include "lj_ffrecord.h"

LJ_FUNC void LJ_FASTCALL lj_recext_cfunc(jit_State *J, RecordFFData *rd, CFuncCallInfo* callinfo);
LJ_FUNC IRType LJ_FASTCALL lj_recext_ctype_to_irtype(lua_TraceRecorderType type);
LJ_FUNC int LJ_FASTCALL lj_extrec_callconv_to_cci(lua_CFunctionInfoCallConv conv);

#endif