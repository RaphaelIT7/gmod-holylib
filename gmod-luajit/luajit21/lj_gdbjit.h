/*
** Client for the GDB JIT API.
** Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_GDBJIT_H
#define _LJ_GDBJIT_H

#include "lj_obj.h"
#include "lj_jit.h"

#if LJ_HASJIT && defined(LUAJIT_USE_GDBJIT)

LJ_FUNC void lj_gdbjit_addtrace(jit_State *J, GCtrace *T);
LJ_FUNC void lj_gdbjit_deltrace(jit_State *J, GCtrace *T);

#else
#define lj_gdbjit_addtrace(J, T)	UNUSED(T)
#define lj_gdbjit_deltrace(J, T)	UNUSED(T)
#endif

#if LUAJIT_USE_GDBJIT
/* ELF definitions. */
typedef struct ELFheader {
  uint8_t emagic[4];
  uint8_t eclass;
  uint8_t eendian;
  uint8_t eversion;
  uint8_t eosabi;
  uint8_t eabiversion;
  uint8_t epad[7];
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uintptr_t entry;
  uintptr_t phofs;
  uintptr_t shofs;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstridx;
} ELFheader;

typedef struct ELFsectheader {
  uint32_t name;
  uint32_t type;
  uintptr_t flags;
  uintptr_t addr;
  uintptr_t ofs;
  uintptr_t size;
  uint32_t link;
  uint32_t info;
  uintptr_t align;
  uintptr_t entsize;
} ELFsectheader;

typedef struct ELFsymbol {
#if LJ_64
  uint32_t name;
  uint8_t info;
  uint8_t other;
  uint16_t sectidx;
  uintptr_t value;
  uint64_t size;
#else
  uint32_t name;
  uintptr_t value;
  uint32_t size;
  uint8_t info;
  uint8_t other;
  uint16_t sectidx;
#endif
} ELFsymbol;

/* GDB JIT entry. */
typedef struct GDBJITentry {
  struct GDBJITentry *next_entry;
  struct GDBJITentry *prev_entry;
  const char *symfile_addr;
  uint64_t symfile_size;
} GDBJITentry;

/* Minimal list of sections for the in-memory ELF object. */
enum {
  GDBJIT_SECT_NULL,
  GDBJIT_SECT_text,
  GDBJIT_SECT_eh_frame,
  GDBJIT_SECT_shstrtab,
  GDBJIT_SECT_strtab,
  GDBJIT_SECT_symtab,
  GDBJIT_SECT_debug_info,
  GDBJIT_SECT_debug_abbrev,
  GDBJIT_SECT_debug_line,
  GDBJIT_SECT__MAX
};

enum {
  GDBJIT_SYM_UNDEF,
  GDBJIT_SYM_FILE,
  GDBJIT_SYM_FUNC,
  GDBJIT_SYM__MAX
};

/* In-memory ELF object. */
typedef struct GDBJITobj {
  ELFheader hdr;      /* ELF header. */
  ELFsectheader sect[GDBJIT_SECT__MAX]; /* ELF sections. */
  ELFsymbol sym[GDBJIT_SYM__MAX]; /* ELF symbol table. */
  uint8_t space[4096];      /* Space for various section data. */
} GDBJITobj;

/* Combined structure for GDB JIT entry and ELF object. */
typedef struct GDBJITentryobj {
  GDBJITentry entry;
  size_t sz;
  GDBJITobj obj;
} GDBJITentryobj;
#endif

#endif
