# LuaJIT boot-compiled closure defect audit

## Scope and comparison base

The vendored runtime is `luajit/`. The closest imported upstream base is LuaJIT
v2.1 commit `9d145d2ca3db58493859c495489a0f08f627834f` (2026-06-25
12:00:59 +0200). The fork imported that update as `d80b94d8` six minutes later.
The comparison used the upstream tree `9108b70ea6199477940589f389ce8a88523d5693`
and the fork tree `42ecac0b9ecd7364394c259debbdf26a7050264a`.

The leading KGC-string hypothesis remains possible as a consequence of memory
or stack corruption, but the fork does not contain a direct change to string
interning, string sweeping, allocation, bytecode loading/writing, or the
`lua_load` implementation. `src/lj_str.c`, `src/lj_str.h`, `src/lj_alloc.c`,
`src/lj_alloc.h`, `src/lj_load.c`, `src/lj_bcread.c`, and `src/lj_bcwrite.c`
are tree-identical to that upstream base. There were also no relevant upstream
fixes to those paths between the base and upstream v2.1 as of 2026-07-11.

## Ranked findings

### 1. High: finalizers run on the active Lua stack, not the upstream VM thread

The only substantive fork-specific collector control-flow change is in
`luajit/src/lj_gc.c:505-532`: `gc_call_finalizer` pushes the finalizer and object
onto the caller's active `lua_State`, calls `lj_vm_pcall` there, and reports a
finalizer error from the same state. Upstream uses the dedicated VM thread for
this work. The fork history is especially relevant: `1c858920` reverted the VM
thread work after observed `BC_RET`/Lua-error stack corruption, and `8688c359`
reapplied it while retaining the active-state finalizer behavior.

This does not directly omit a proto mark, but it is the most plausible
fork-specific way for allocation/finalizer pressure during boot to corrupt an
active parser stack or a root that happens to contain a constants table. It is
consistent with the boot-pressure dependency and with `jit.off()` not helping.
The diagnostic build can test it two ways: suspend GC until `ServerActivate`, or
use the stock `lua_shared` runtime so all custom collector/layout code is out of
the process.

### 2. Medium-high: GC64 object layouts were compacted and require exact binary/header agreement

The fork rearranges `GCproto.k`, `GCproto.uv`, and `GCproto.gclist` under GC64 in
`luajit/src/lj_obj.h:422-435`. It also rearranges other GC headers and expands
every C closure with ten `CFuncCallInfo` records at
`luajit/src/lj_obj.h:502-529`. These changes are internally consistent when the
VM, generated offsets, static archive, and all direct structure users come from
the same source revision. A stale archive or an engine/plugin path compiled
against a different layout could, however, misread `KBASE`, `gclist`, or an
object header and create exactly the kind of indirect KGC corruption under
investigation.

The checked-in Linux archive was refreshed by `6d600b2e` after the layout work,
so there is no positive evidence of a layout mismatch in the production
archive. The safe discriminator is still the stock-runtime switch because the
module cannot mix stock `luaL_load*` functions with a custom-runtime state; the
warning about that invariant is in `source/symbols.h:162-168`.

### 3. Medium-high: all RunStringEx compilation can be source-rewritten by a Lua hook

`source/modules/holylib.cpp:460-487` detours
`CLuaInterface::RunStringEx`, calls `HolyLib:OnLuaRunString`, and accepts either
a replacement source string or a boolean that bypasses the engine call. The
normal path then runs the engine macro pass and `luaL_loadbuffer` at
`source/lua/CLuaInterface.cpp:1664-1669`.

This is the only repository hook that can change arbitrary addon source before
compilation. It can explain a boot/runtime split if an `OnLuaRunString` listener
exists or behaves differently only during boot. The replacement string remains
anchored on the Lua stack until the trampoline returns, so the native pointer
lifetime in this hook itself is sound. `diagnosticDisableLuaRunStringHook`
removes this interception without disabling the rest of the core module.

### 4. Medium: the external C-function recorder has incomplete initialization and suspect TypeID wiring

Every C closure allocates ten recorder records, but `lj_func_newC` initializes
only `traceFunc`, `func`, `flags`, and the `givestate` bit at
`luajit/src/lj_func.c:111-122`. Other bitfields share that storage and are not
fully cleared. `lua_fillCFuncInfo` later builds flags with `|=` at
`luajit/src/lj_api.c:784-818`, so an uninitialized `allowoptout` bit can change
trace optimization behavior.

The TypeID registration has another defect: `ASMINFO_TypeID` sets a recorder but
no `asmFunc` at `source/modules/luajit.cpp:280-284`, while
`lua_fillCFuncInfo` stores `info->asmFunc` in the `func` field used to decide
whether a recorder slot exists (`luajit/src/lj_api.c:794-795`). The recorder is
therefore not reliably selectable. If it were selected, primitive cases in
`TraceRecord_TypeID` write result slot 1 while userdata cases write slot 0
(`source/modules/luajit.cpp:211-254`), another inconsistency.

These are real defects, but they are trace-only and therefore rank below the GC
and compilation paths for a symptom that survives `jit.off()`. The fast-function
kill switch leaves stock `SysTime` and `TypeID` registrations untouched.

### 5. Medium-low: custom userdata lookup/write paths affect the exact operations in the repro

The interpreter adds direct userdata metatable/usertable access before normal
`__index` and `__newindex` handling in `luajit/src/lj_meta.c:146-167` and
`luajit/src/lj_meta.c:211-241`. The recorder has a separate implementation of
the same behavior in `luajit/src/lj_record.c:1546-1590`. This code is relevant
to entity fields and method calls.

It does not by itself explain why byte-identical boot and runtime closures
differ: on a userdata, constant `TGETS` and a string-valued `TGETV` both fall
back to `lj_meta_tget`. Also, the x64 interpreter's `TGETS` read path is unchanged
from upstream; the fork's x64 VM changes in this area are readonly checks on
table writes (`luajit/src/vm_x64.dasc:3854-4060`). This path could still amplify
a malformed/dangling KGC key, so it remains a secondary suspect.

### 6. Low for this repro, but build-relevant: LuaJIT source and checked-in archive are one commit out of sync

The last archive refresh is `6d600b2e` (2026-06-25). The later source-only commit
`a6c98df1` changes four `continue` tests from bitwise OR to bitwise AND at
`luajit/src/lj_parse.c:2437-2587`. Consequently, the plugin workflow still links
the pre-fix parser even though the source tree displays the fix. This can emit
bad loop targets for chunks containing `continue`, but it should affect boot and
runtime compilation equally and does not explain a byte-identical A/B by itself.

### 7. Low: FFIOverrideCompat changes include attribution, not compilation

At the audited head, C++ loaded the compatibility script only inside
`enableFFIOverrides`, so it did not apply when that option was false. The Lua
wrapper called the captured stock `include` function; it did not call
`CompileString`, `CompileFile`, or `RunString`. Its extra Lua frame did explain
errors being attributed to `HolyLib:FFIOverrideCompat.lua`.

The correctness commit now adds a second in-script option guard at
`source/lua/scripts/FFIOverrideCompat.lua:1-9`. When enabled, it restores stock
`include` before `includes/util.lua`, defers reinstalling the TypeID-based checks
to the next tick, and tail-calls the captured include at
`source/lua/scripts/FFIOverrideCompat.lua:34-57`. Thus wrapped files continue
through the exact stock engine include path and the wrapper frame is absent from
error attribution. The diagnostic wrapper switch disables only this wrapper;
the immediate `isvector`/`isangle` hardening remains installed.

## KGC/proto lifetime audit details

- Lexer-created strings are interned, inserted into the parser constants table,
  and followed by a GC check in `luajit/src/lj_parse.c:241-249`.
- Each function's constants table is anchored on the active Lua stack in
  `luajit/src/lj_parse.c:1612-1635`.
- `fs_fixup_k` copies every collectable constant into the finished proto and
  applies `lj_gc_objbarrier` in `luajit/src/lj_parse.c:1364-1409`.
- The chunk name is separately anchored for the parse in
  `luajit/src/lj_parse.c:2735-2762`.
- Collector traversal marks the chunk name and every `proto_kgc` entry in
  `luajit/src/lj_gc.c:281-287`.
- LuaJIT's collector is non-moving. Interned strings are either retained at
  their address or unlinked and freed during string sweep; the fork adds no
  relocation or alternate string arena.

Those paths match upstream. A live failure where `t.ARC9` differs from
`t[("ARC" .. "9")]` therefore remains strong evidence that the proto's constant
reference or its pointed-to string has been corrupted, but the audit found no
fork-specific missing KGC anchor. The active-stack finalizer and GC64 layout
coupling are the best indirect mechanisms found.

## Diagnostic options

All options default to `false`, preserving the pre-diagnostic behavior. The
first four live in the `luajit` object in
`garrysmod/holylib/cfg/x64/modules.json`; the last lives in the `holylib`
object.

| Option | Effect when true | Boot log key |
| --- | --- | --- |
| `diagnosticDisableFastFunctions` | Leaves stock `SysTime`/`TypeID`; skips custom C-function trace registrations | `fastFunctions=disabled` |
| `diagnosticDisableFFICompatIncludeWrapper` | Keeps TypeID-based checks but does not replace global `include` | `ffiCompatIncludeWrapper=disabled` |
| `diagnosticUseStockLuaJITRuntime` | Skips the custom LuaJIT takeover, including custom load/compiler, object layout, allocator, and GC paths | `customRuntime=disabled` |
| `diagnosticSuspendGCThroughBoot` | Stops GC in Lua init and restarts it at `ServerActivate` | `suspendGCThroughBoot=enabled`, then `suspended GC through server boot` / `restarted GC after server boot` |
| `diagnosticDisableLuaRunStringHook` | Removes the `HolyLib:OnLuaRunString` source interception | `luaRunStringHook=disabled` |

The state lines are emitted in `source/modules/luajit.cpp:468-476` and
`source/modules/holylib.cpp:587-588`.
