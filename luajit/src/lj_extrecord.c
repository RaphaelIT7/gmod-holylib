/*
** External trace recorder
** Based of lj_ffrecord.c -> Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_ffrecord_c
#define LUA_CORE

#include "lj_obj.h"

#if LJ_HASJIT

#include "lj_err.h"
#include "lj_buf.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_frame.h"
#include "lj_bc.h"
#include "lj_ff.h"
#include "lj_ir.h"
#include "lj_jit.h"
#include "lj_ircall.h"
#include "lj_iropt.h"
#include "lj_trace.h"
#include "lj_record.h"
#include "lj_ffrecord.h"
#include "lj_crecord.h"
#include "lj_dispatch.h"
#include "lj_vm.h"
#include "lj_strscan.h"
#include "lj_strfmt.h"
#include "lj_serialize.h"
#include "lj_extrecord.h"

/* Some local macros to save typing. Undef'd at the end. */
#define IR(ref)			(&J->cur.ir[(ref)])

/* Pass IR on to next optimization in chain (FOLD). */
#define emitir(ot, a, b)	(lj_ir_set(J, (ot), (a), (b)), lj_opt_fold(J))

#define emitconv(a, dt, st, flags) \
  emitir(IRT(IR_CONV, (dt)), (a), (st)|((dt) << 5)|(flags))

IRType LJ_FASTCALL lj_recext_ctype_to_irtype(lua_TraceRecorderType type)
{
  switch(type) {
    case TR_TYPE_VOID:
      return IRT_NIL;
    case TR_TYPE_LUASTATE:
      return IRT_THREAD;
    case TR_TYPE_TABLE:
      return IRT_TAB;
    case TR_TYPE_USERDATA:
      return IRT_UDATA;
    case TR_TYPE_USERDATA_VALUE:
      return IRT_UDATA;
    case TR_TYPE_I8:
      return IRT_I8;
    case TR_TYPE_U8:
      return IRT_U8;
    case TR_TYPE_I16:
      return IRT_I16;
    case TR_TYPE_U16:
      return IRT_U16;
    case TR_TYPE_INT:
      return IRT_INT;
    case TR_TYPE_U32:
      return IRT_U32;
    case TR_TYPE_I64:
      return IRT_I64;
    case TR_TYPE_U64:
      return IRT_U64;
    case TR_TYPE_BOOL:
      return IRT_U8;
    case TR_TYPE_TRUE:
      return IRT_TRUE;
    case TR_TYPE_FALSE:
      return IRT_FALSE;
    case TR_TYPE_FLOAT:
      return IRT_FLOAT;
    case TR_TYPE_DOUBLE:
      return IRT_NUM;
    case TR_TYPE_STRING:
      return IRT_STR;
    case TR_TYPE_CHARS:
      return IRT_STR;
    default:
      return IRT_NIL;
  }
}

lua_TraceRecorderType LJ_FASTCALL lj_recext_irtype_to_ctype(IRType type)
{
  switch (type)
  {
    case IRT_NIL:
      return TR_TYPE_NIL;
    case IRT_THREAD:
      return TR_TYPE_LUASTATE;
    case IRT_TAB:
      return TR_TYPE_TABLE;
    case IRT_UDATA:
      return TR_TYPE_USERDATA;
    case IRT_I8:
      return TR_TYPE_I8;
    case IRT_U8:
      return TR_TYPE_U8;
    case IRT_I16:
      return TR_TYPE_I16;
    case IRT_U16:
      return TR_TYPE_U16;
    case IRT_INT:
      return TR_TYPE_INT;
    case IRT_U32:
      return TR_TYPE_U32;
    case IRT_I64:
      return TR_TYPE_I64;
    case IRT_U64:
      return TR_TYPE_U64;
    case IRT_FLOAT:
      return TR_TYPE_FLOAT;
    case IRT_NUM:
      return TR_TYPE_DOUBLE;
    case IRT_STR:
      return TR_TYPE_STRING;
    case IRT_TRUE:
      return TR_TYPE_TRUE;
    case IRT_FALSE:
      return TR_TYPE_FALSE;
    //case IRT_BOOL:
    //  return TR_TYPE_BOOL;
    case IRT_PTR:
      return TR_TYPE_PTR;
    default:
      return TR_TYPE_VOID;
  }
}

int LJ_FASTCALL lj_extrec_callconv_to_cci(lua_CFunctionInfoCallConv conv)
{
  switch (conv) {
    case CFUNC_CALLCONV_FASTCALL:
      return CCI_CC_FASTCALL;
    case CFUNC_CALLCONV_STDCALL:
      return CCI_CC_STDCALL;
    case CFUNC_CALLCONV_THISCALL:
      return CCI_CC_THISCALL;
    default:
      return CCI_CC_CDECL;
  }
}

static TRef recext_processtype(jit_State *J, TRef ref, TRef val, TValue* value, lua_TraceRecorderType type)
{
  if (type == TR_TYPE_CHARS) {
    return emitir(IRT(IR_STRREF, IRT_PGC), val, lj_ir_kint(J, 0));
  } else if (type == TR_TYPE_USERDATA) {
    // We guard on the type as we don't possibly want to get passed different userdata's and re-use the same trace
    TRef tr = emitir(IRT(IR_FLOAD, IRT_U16), val, IRFL_UDATA_UDTYPE);
    emitir(IRTGI(IR_EQ), tr, lj_ir_kint(J, udataV(value)->guard));
    return val;
  } else if (type == TR_TYPE_USERDATA_VALUE) {
    TRef tr = emitir(IRT(IR_FLOAD, IRT_U16), val, IRFL_UDATA_UDTYPE);
    emitir(IRTGI(IR_EQ), tr, lj_ir_kint(J, udataV(value)->guard));
    return emitir(IRT(IR_FLOAD, IRT_PTR), val, IRFL_UDATA_VALUE);
  } else {
    return val;
  }
}

static inline IRFieldID lj_extrec_cfield_to_ir(lua_TraceField field)
{
  switch(field)
  {
#define CHECK_FIELD(field) case TR_FIELD_##field: return IRFL_##field;
TR_FIELDS(CHECK_FIELD)
#undef CHECK_FIELD
    default:
      return IRFL__MAX;
  }
}

#define TR2J(tr) ((jit_State*)tr->J)
#define TR2RD(tr) ((RecordFFData*)tr->rd)

#define EMIT_IRT(o, t)		(IRT((o), (entry.guard == 1 ? IRT_GUARD : 0)|(t)))

lua_TraceEntry lj_tr_emit(lua_TraceRecorder* tr, lua_TraceRecorderEntry entry)
{
  jit_State *J = TR2J(tr);
  switch (entry.op)
  {
    case TR_GUARD_USERDATA:
    {
      TRef obj = (TRef)entry.a;
      TRef tag = (TRef)entry.b;
      return (lua_TraceEntry)emitir(IRTG(IR_EQ, IRT_U8), obj, lj_ir_kint(J, (int)tag));
    }
    case TR_GUARD_USERDATA_TYPE:
    {
      TRef obj = (TRef)entry.a;
      TRef type = emitir(IRT(IR_FLOAD, IRT_U8), obj, IRFL_UDATA_UDTYPE);
      return (lua_TraceEntry)emitir(IRTG(IR_EQ, IRT_U8), type, lj_ir_kint(J, (int)entry.b));
    }
    case TR_GUARD_USERDATA_FLAGS:
    {
      TRef obj = (TRef)entry.a;
      TRef flags = emitir(IRT(IR_FLOAD, IRT_U8), obj, IRFL_UDATA_FLAGS);
      return (lua_TraceEntry)emitir(IRTG(IR_EQ, IRT_U8), flags, lj_ir_kint(J, (int)entry.b));
    }
    case TR_LOAD_USERDATA_VAL:
    {
      TRef obj = (TRef)entry.a;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_FLOAD, IRT_PTR), obj, IRFL_UDATA_VALUE);
    }
    case TR_FLOAD:
    {
      TRef base = (TRef)entry.a;
      IRFieldID field = lj_extrec_cfield_to_ir((lua_TraceField)entry.b);
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_FLOAD, t), base, field);
    }
    case TR_FREF:
    {
      TRef base = (TRef)entry.a;
      IRFieldID field = lj_extrec_cfield_to_ir((lua_TraceField)entry.b);
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_FREF, t), base, field);
    }
    case TR_FSTORE:
    {
      TRef fieldRef = (TRef)entry.a;
      TRef storeVal = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_FSTORE, t), fieldRef, storeVal);
    }
    case TR_XLOAD:
    {
      TRef base = (TRef)entry.a;
      size_t offset = entry.b;
      TRef val = emitir(IRT(IR_ADD, IRT_PTR), base, lj_ir_kintp(J, offset));
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_XLOAD, t), val, 0);
    }
    case TR_XREF:
    {
      TRef base = (TRef)entry.a;
      size_t offset = entry.b;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_ADD, IRT_PTR), base, lj_ir_kintp(J, offset));
    }
    case TR_XSTORE:
    {
      // We expect a to be the returned TRef from TR_XREF
      TRef pointerRef = (TRef)entry.a;
      TRef storeVal = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_XSTORE, t), pointerRef, storeVal);
    }
    case TR_CALL_ARG:
    {
      TRef prev = (TRef)entry.a;
      TRef next = (TRef)entry.b;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_CARG, IRT_NIL), prev, next); // Type does not matter here I think
    }
    case TR_CALL_CC:
    {
      TRef funcPtr = (TRef)entry.a;
      lua_CFunctionInfoCallConv callconv = (lua_CFunctionInfoCallConv)entry.b;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_CALLCC, IRT_NIL), funcPtr, lj_ir_kint(J, lj_extrec_callconv_to_cci(callconv)));
    }
    case TR_CALL_EXT:
    {
      TRef argChain = (TRef)entry.a;
      TRef funcPtr = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_CALLXS, t), argChain, funcPtr);
    }
    case TR_CALL_INFO:
    {
      CFuncCallInfo* info = (CFuncCallInfo*)entry.aptr;
      lj_recext_cfunc(J, TR2RD(tr), info);
      return 0;
    }
    case TR_ADD:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_ADD, t), left, right);
    }
    case TR_SUB:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_SUB, t), left, right);
    }
    case TR_MUL:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_MUL, t), left, right);
    }
    case TR_DIV:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_DIV, t), left, right);
    }
    case TR_MOD:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_MOD, t), left, right);
    }
    case TR_NEG:
    {
      TRef val = (TRef)entry.a;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_NEG, t), val, 0);
    }
    case TR_ABS:
    {
      TRef val = (TRef)entry.a;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_ABS, t), val, 0);
    }
    case TR_MIN:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_MIN, t), left, right);
    }
    case TR_MAX:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_MAX, t), left, right);
    }
    case TR_BAND:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_BAND, t), left, right);
    }
    case TR_BOR:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_BOR, t), left, right);
    }
    case TR_BXOR:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_BXOR, t), left, right);
    }
    case TR_BNOT:
    {
      TRef val = (TRef)entry.a;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_BNOT, t), val, 0);
    }
    case TR_BSHL:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_BSHL, t), left, right);
    }
    case TR_BSHR:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_BSHR, t), left, right);
    }
    case TR_BSAR:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_BSAR, t), left, right);
    }
    case TR_BROL:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_BROL, t), left, right);
    }
    case TR_BROR:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_BROR, t), left, right);
    }
    case TR_BSWAP:
    {
      TRef val = (TRef)entry.a;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_BSWAP, t), val, 0);
    }
    case TR_EQ:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_EQ, t), left, right);
    }
    case TR_NE:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_NE, t), left, right);
    }
    case TR_LT:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_LT, t), left, right);
    }
    case TR_LE:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_LE, t), left, right);
    }
    case TR_GT:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_GT, t), left, right);
    }
    case TR_GE:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_GE, t), left, right);
    }
    case TR_ULT:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_ULT, t), left, right);
    }
    case TR_ULE:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_ULE, t), left, right);
    }
    case TR_UGT:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_UGT, t), left, right);
    }
    case TR_UGE:
    {
      TRef left = (TRef)entry.a;
      TRef right = (TRef)entry.b;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_UGE, t), left, right);
    }
    case TR_CONV:
    {
      TRef val = (TRef)entry.a;
      IRType st = irt_type(IR(val)->t);
      IRType dt = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_CONV, dt), val, st|((dt) << 5));
    }
    case TR_TOSTR:
    {
      TRef val = (TRef)entry.a;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_TOSTR, IRT_STR), val, tref_isnum(val) ? IRTOSTR_NUM : IRTOSTR_INT);
    }
    case TR_STRTO:
    {
      TRef val = (TRef)entry.a;
      IRType t = lj_recext_ctype_to_irtype(entry.type);
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_STRTO, t), val, 0);
    }
    case TR_TOBIT:
    {
      TRef val = (TRef)entry.a;
      TRef bits = (TRef)entry.b;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_TOBIT, IRT_INT), val, bits);
    }
    case TR_AREF:
    {
      TRef tableRef = (TRef)entry.a;
      TRef indexRef = (TRef)entry.b;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_AREF, IRT_PGC), tableRef, indexRef);
    }
    case TR_HREF:
    {
      TRef tableRef = (TRef)entry.a;
      TRef keyRef = (TRef)entry.b;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_HREF, IRT_PGC), tableRef, keyRef);
    }
    case TR_HREFK:
    {
      TRef tableRef = (TRef)entry.a;
      TRef keyRef = (TRef)entry.b;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_HREFK, IRT_PGC), tableRef, keyRef);
    }
    case TR_UREFO:
    {
      TRef funcRef = (TRef)entry.a;
      uint32_t upvalue = (uint32_t)entry.b;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_UREFO, IRT_PTR), funcRef, upvalue);
    }
    case TR_UREFC:
    {
      // funcRef is expected to be a constant as based on this assupltion upvalue will be one too then
      TRef funcRef = (TRef)entry.a;
      uint32_t upvalue = (uint32_t)entry.b;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_UREFC, IRT_PTR), funcRef, upvalue);
    }
    case TR_TMPREF:
    {
      TRef val = (TRef)entry.a;
      int mode = (int)entry.b; // IRTMPREF_ mode
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_TMPREF, IRT_PTR), val, mode);
    }
    case TR_STRREF:
    {
      TRef strRef = (TRef)entry.a;
      TRef indexRef = (TRef)entry.b;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_STRREF, IRT_PTR), strRef, indexRef);
    }
    case TR_LREF:
    {
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_LREF, IRT_THREAD), 0, 0);
    }
    case TR_SNEW:
    {
      TRef ptrRef = (TRef)entry.a;
      TRef lenRef = (TRef)entry.b;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_SNEW, IRT_STR), ptrRef, lenRef);
    }
    case TR_TNEW:
    {
      TRef asize = (TRef)entry.a;
      TRef hbits = (TRef)entry.a;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_TNEW, IRT_TAB), asize, hbits);
    }
    case TR_TDUP:
    {
      TRef tableRef = (TRef)entry.a;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_TDUP, IRT_TAB), tableRef, 0);
    }
    case TR_CNEW:
    {
      TRef ctypeRef = (TRef)entry.a;
      TRef sizeRef = (TRef)entry.b;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_CNEW, IRT_CDATA), ctypeRef, sizeRef);
    }
    case TR_CNEWI:
    {
      TRef ctypeRef = (TRef)entry.a;
      TRef initRef = (TRef)entry.b;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_CNEWI, IRT_CDATA), ctypeRef, initRef);
    }
    case TR_UDNEW:
    {
      TRef sizeRef = (TRef)entry.a;
      TRef metaRef = (TRef)entry.b;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_UDNEW, IRT_UDATA), sizeRef, metaRef);
    }
    case TR_TBAR:
    {
      TRef tableRef = (TRef)entry.a;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_TBAR, IRT_NIL), tableRef, 0);
    }
    case TR_OBAR:
    {
      TRef objRef = (TRef)entry.a;
      TRef valRef = (TRef)entry.b;
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_OBAR, IRT_NIL), objRef, valRef);
    }
    case TR_XBAR:
    {
      return (lua_TraceEntry)emitir(EMIT_IRT(IR_XBAR, IRT_NIL), 0, 0);
    }
    default:
      return 0;
  }
}

lua_TraceRecorderType lj_tr_type(lua_TraceEntry entry)
{
  return lj_recext_irtype_to_ctype(tref_type(entry));
}

lua_TraceEntry lj_tr_getbase(lua_TraceRecorder* tr, int idx)
{
  return (lua_TraceEntry)TR2J(tr)->base[idx];
}

void lj_tr_setbase(lua_TraceRecorder* tr, int idx, lua_TraceEntry entry)
{
  TR2J(tr)->base[idx] = (TRef)entry;
}

void* lj_tr_getrtbase(lua_TraceRecorder* tr, int idx)
{
  return (void*)&TR2RD(tr)->argv[idx];
}

lua_TraceEntry lj_tr_kint(lua_TraceRecorder* tr, int val)
{
  return (lua_TraceEntry)lj_ir_kint(TR2J(tr), val);
}

lua_TraceEntry lj_tr_k64(lua_TraceRecorder* tr, size_t val)
{
  return (lua_TraceEntry)lj_ir_k64(TR2J(tr), IR_KNUM, val);
}

lua_TraceEntry lj_tr_kint64(lua_TraceRecorder* tr, size_t val)
{
  return (lua_TraceEntry)lj_ir_kint64(TR2J(tr), val);
}

lua_TraceEntry lj_tr_knum(lua_TraceRecorder* tr, lua_Number val)
{
  return (lua_TraceEntry)lj_ir_knum(TR2J(tr), val);
}

lua_TraceEntry lj_tr_kptr(lua_TraceRecorder* tr, void* val)
{
  return (lua_TraceEntry)lj_ir_kptr(TR2J(tr), val);
}

lua_TraceEntry lj_tr_knull(lua_TraceRecorder* tr, lua_TraceRecorderType type)
{
  return (lua_TraceEntry)lj_ir_knull(TR2J(tr), lj_recext_ctype_to_irtype(type));
}

lua_TraceEntry lj_tr_refnil(lua_TraceRecorder* tr)
{
  UNUSED(tr);
  return (lua_TraceEntry)TREF_NIL;
}

lua_TraceEntry lj_tr_reffalse(lua_TraceRecorder* tr)
{
  UNUSED(tr);
  return (lua_TraceEntry)TREF_FALSE;
}

lua_TraceEntry lj_tr_reftrue(lua_TraceRecorder* tr)
{
  UNUSED(tr);
  return (lua_TraceEntry)TREF_TRUE;
}

/* RaphaelIT7: We now support tracing of C functions :hehe: */
void LJ_FASTCALL lj_recext_cfunc(jit_State *J, RecordFFData *rd, CFuncCallInfo* callinfo)
{
  if (!callinfo) {
    lj_recff_nyi(J, rd);
    return;
  }

  uint32_t stackargs = 0;
  for (stackargs = 0; stackargs < LUA_CFUNCINFO_MAXARGS; ++stackargs) {
    if (tref_isnil(J->base[stackargs]))
      break;
  }

  if (callinfo->traceFunc)
  {
    lua_TraceRecorder recorder;
    recorder.J = J;
    recorder.rd = rd;
    ptrdiff_t rets = (ptrdiff_t)callinfo->traceFunc(&recorder);
    if (rets < 0) // We failed.
    {
      lj_recff_nyi(J, rd);
      return;
    }

    // ToDo: implement processing
    rd->nres = rets;
    return;
  }

  if (!callinfo->func) {
    lj_recff_nyi(J, rd);
    return;
  }

  uint32_t args = CCI_NARGS(callinfo);
  TRef tr = TREF_NIL;
  if (args > 0) {
    for (uint32_t i=(callinfo->givestate ? 1 : 0); i < args; ++i) {
      if (!tref_istype(J->base[i], lj_recext_ctype_to_irtype(callinfo->argType[i]))) {
        lj_recff_nyi(J, rd);
        return;
      }
    }

    if (callinfo->givestate) {
      tr = emitir(IRT(IR_LREF, IRT_THREAD), 0, 0);
    } else {
      tr = recext_processtype(J, tr, J->base[0], &rd->argv[0], callinfo->argType[0]);
    }

    for (uint32_t i=1; i < args; ++i) {
      tr = emitir(IRT(IR_CARG, IRT_NIL), tr, recext_processtype(J, tr, J->base[i], &rd->argv[i], callinfo->argType[i]));
    }
  }
  
  //if (CCI_OP(callinfo) == IR_CALLS)
  //  J->needsnap = 1;

  TRef funcPtr = lj_ir_kptr(J, callinfo->func);
  funcPtr = emitir(IRT(IR_CALLCC, IRT_NIL), funcPtr, lj_ir_kint(J, callinfo->flags & CCI_CC_MASK));

  if (callinfo->allowoptout)
    funcPtr = emitir(IRT(IR_CALLCSE, IRT_NIL), funcPtr, lj_ir_kint(J, 0));
  
  IRType retType = lj_recext_ctype_to_irtype(callinfo->retType);
  TRef result = emitir(IRT(IR_CALLXS, retType), tr, funcPtr);
  if (callinfo->retType == TR_TYPE_VOID) {
    J->base[0] = TREF_NIL;
    rd->nres = 0;
  } else {
    if (retType == IRT_FLOAT) {
      result = emitconv(result, IRT_NUM, retType, 0);
    } else if (callinfo->retType == TR_TYPE_CHARS) {
      TRef strlen = lj_ir_call(J, IRCALL_strlen, result);
      result = emitir(IRT(IR_SNEW, IRT_STR), result, strlen);
    } else if (callinfo->retType == TR_TYPE_STRING) {
      emitir(IRTG(IR_NE, IRT_TRUE), result, lj_ir_kint(J, 0)); // GUARD to avoid null pointer crashes
      TRef strlen = emitir(IRT(IR_FLOAD, IRT_UINTP), result, IRFL_LSTR_LEN);
      result = emitir(IRT(IR_FLOAD, IRT_PTR), result, IRFL_LSTR_DATA);
      result = emitir(IRT(IR_SNEW, IRT_STR), result, strlen);
    } else if (callinfo->retType == TR_TYPE_BOOL) {
      //RaphaelIT7: GG - You can't specify a bool return value / I got no idea yet
      //WARNING: This is EXPENSIVE!!! If we return false we will mismatch & die (not really but it'll exit the trace)
      result = emitir(IRTG(callinfo->retbool == 1 ? IR_NE : IR_EQ, IRT_TRUE), result, lj_ir_kint(J, 0));
      //result = emitconv(result, IRT_BOOL, retType, 0); // yes... I did attempt to add IRT_BOOL... didn't go well
      //result = emitir(IRTG(IR_NE, IRT_U8), result, lj_ir_kint(J, 0));
    }

    J->base[0] = result;
    rd->nres = 1;
  }
  UNUSED(rd);
}

#endif