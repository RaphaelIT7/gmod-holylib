# Clean-room gluapack functional analysis

This note records the evidence used to design HolyLib's `gmoddatapack` luapack feature. It is a functional interoperability study, not a source or byte-for-byte reconstruction. No DRM, licensing, telemetry, opt-out, or proprietary payload was copied.

## Evidence and limits

The requested stripped `gmsv_gluapack_plugin64.so` and 32-bit plugin were not present in the supplied workspace. The earlier operator report was treated as a hypothesis, not evidence. Consequently, claims about the incumbent's native detours are explicitly left open below.

Evidence that was available:

- Target Linux x86-64 `server.so`: SHA-256 `7371bc3cbbfb53796651ef80c775c88e5266c1b0d8c3bbe7537a05ceb382d4be`, GNU build ID `f7c8ebd81215316e3fd15fe93db5337043c5c0fc`.
- Target Linux x86 `server.so`: SHA-256 `562bf00dd2b26befa29df5aac88e2366971fb19e88f586520037572943875f64`, GNU build ID `ff7e4cfaa49a08c6be8c0f79d85c7203eb38e788`.
- HolyLib's complete `source/modules/gmoddatapack.cpp`, symbols, module manager, filesystem, string-table, thread-pool, and build integration.
- A publicly committed, stolen-at-runtime incumbent bootstrap at `eoan-ermine/urfim_ww2`, commit `2bb21fc383301c125753ef95640078bdf158f7ef`, path `addons/urf_plib/lua/includes/init.lua`. It is used only as behavioral evidence. HolyLib's bootstrap was written independently.
- `danielga/sourcesdk-minimal` public interfaces for signon states, cvar flags, string tables, and GMod client messages.

Offsets below are ELF virtual addresses in the two target `server.so` files, not stable signatures.

## Findings 1–12

### 1. `GModDataPack::SendFileToClient` contract — CONFIRMED for the target engine

The exported symbols are `_ZN12GModDataPack16SendFileToClientEii` on both targets. The x86-64 function is at `0x00c398f0` (831 bytes); x86 is at `0x0093f2f0` (965 bytes). Both take `(this, clientIndex, fileID)` and return `void`. They bounds-check the ID, resolve the client-Lua string table and Lua cache, create the ordinary Lua-file payload when needed, enforce the approximately 64 KiB message limit, write message opcode `4`, a 16-bit file ID, and the compressed bytes, then call `GMOD_SendToClient`.

No server-side per-file counter or end-of-list state is mutated in this function. The barrier advances because the requested file ID receives a syntactically normal `LuaFileDownload`. Suppressing the function without a replacement message can therefore strand the client at Requesting Lua.

Implementation decision: a ready client receives a normally framed, SHA-256-prefixed/LZMA-compressed stub for that exact ID. Every other client continues through HolyLib's existing async-compressed real-file path; if that path itself cannot produce a payload while luapack is active, the native trampoline is the last-resort fallback.

### 2. Global suppression versus hidden per-client state — OPEN for the incumbent

The native plugin was unavailable, so the suspected global `pack active => suppress` branch cannot be truthfully confirmed from a native xref. The recovered bootstrap contains no client-to-server READY acknowledgement and the four manifest cvars are global, which is consistent with the reported deadlock but is not proof of the native decision.

What would settle it: the stripped plugin's `SendFileToClient` detour or a runtime trace of two simultaneous clients with different pack outcomes.

Implementation decision: HolyLib does not depend on this inference. Stub delivery requires an exact per-slot `READY(generation, md5)` acknowledgement before every optimized send. Once the full bootstrap/init file has run, any request that does not qualify for a stub is passed to the recovered native `SendFileToClient` trampoline.

### 3. `AddOrUpdateFile` identity and bytes — CONFIRMED for HolyLib and target engine

The exported target symbol `_ZN12GModDataPack15AddOrUpdateFileEP7LuaFileb` is at x86-64 `0x00c39260` (1677 bytes) and x86 `0x0093eb60` (1933 bytes). It consumes `LuaFile::{name,contents}`, finds or adds the name in `client_lua_files`, hashes `contents + NUL` into string userdata, and optionally creates the per-file LZMA payload.

HolyLib already detours that exact symbol and retains `LuaFile::{name,source,contents}` while its worker processes and compresses the normal per-file cache. There was no need for a second detour or new module.

Implementation decision: capture the original registered virtual path, source path, and bytes in the existing hook, then snapshot that registry into an off-thread whole-pack build.

### 4. Stub and resolver — PARTLY CONFIRMED, PARTLY OPEN

The public bootstrap's resolver is named `gluapack`; it hashes `debug.getinfo(2, "S").source` after stripping `@`, compiles the VFS entry, and returns the function. It replaces `CompileFile`. The exact native replacement payload was not available for confirmation; `return gluapack()()` remains consistent with the resolver but is not claimed as binary-verified.

The prior reconstruction is corrected in one respect: the recovered incumbent bootstrap does **not** override `include` or `RunString`.

Implementation decision: use a generation-explicit, independently named stub `return __holypack("<generation>")()` and cover `CompileFile`, `include`, and identifier-based `RunString`, always falling back to the originals when a key is absent.

### 5. Pack layout, compression, crypto, and MD5 — CONFIRMED client-side; server producer remains OPEN

The recovered parser skips a one-byte version and repeatedly reads three 16-byte keys, a four-byte big-endian length, and source bytes. It hex-encodes the three binary keys for lookup. It RC4-decrypts using a hex-derived 16-byte key, discards the first 256 decrypted bytes, calls `util.Decompress`, then compares `util.MD5(uncompressedContents)` with `gluapack_md5`.

That confirms the client order `decrypt -> discard RC4 prefix -> decompress -> MD5(uncompressed pack)`. It does not independently prove how the unavailable native producer constructs each key.

Implementation decision: retain the compatible simple layout and MD5 point, use one whole-pack Bootil LZMA stream, and omit encryption entirely. First-party transport security belongs at the operator's HTTP/CDN layer; correctness never depends on a secret.

### 6. Key normalization and salt — CONFIRMED from the bootstrap

The incumbent defines `saltedMD5(value) = util.MD5(gluapack_salt .. value)`. Its two local-path forms apply these anchored removals:

1. `^addons/[^/]+/`, `^gamemodes/[^/]+/entities/`, `^gamemodes/`, `^lua/`
2. `^addons/[^/]+/`, `^gamemodes/`, `^lua/`

Implementation decision: emit the source key plus both normalized virtual-path keys. HolyLib persists a non-secret random salt below the configured data directory.

### 7. Client path, resources, and `sv_downloadurl` — CLIENT HALF CONFIRMED

The recovered bootstrap reads `download/data/gluapack/<gluapack_file>.bsp` through the `GAME` search path. Native `AddResource`/`downloadables` registration and the incumbent's `sv_downloadurl` guard could not be recovered without the plugin.

HolyLib already demonstrates the correct `INetworkStringTableContainer` acquisition in `precachefix`, and the `downloadables` table identity in `stringtable`.

Implementation decision: atomically write `garrysmod/data/<packdir>/<md5>.bsp`, then add `data/<packdir>/<md5>.bsp` to `downloadables`. Publication is refused unless the object exists and registration succeeds. `respect` never changes `sv_downloadurl`; `require` needs a non-empty operator value; `lock` restores the captured operator value while active.

### 8. Replicated manifest — CONFIRMED format weakness, timing OPEN

The recovered bootstrap independently creates `gluapack_file`, `gluapack_md5`, `gluapack_key`, and `gluapack_salt` with the combined replicated/protected/dont-record/unlogged/unregistered client flags. These are separate values and therefore can be observed across different publication instants. Native set timing relative to signon remains open.

Source SDK correction: a **server** cvar carrying a manifest cannot use `FCVAR_PROTECTED`; Source replicates a protected value as a boolean rather than its text. `FCVAR_UNREGISTERED` also conflicts with normal server registration. HolyLib therefore uses one registered `FCVAR_REPLICATED | FCVAR_DONTRECORD | FCVAR_UNLOGGED` server cvar, while the client-created mirror requests the additional local flags.

Implementation decision: publish one snapshot containing the current ID and every retained immutable generation. One `SetValue` is the publication barrier.

### 9. Generation and autorefresh — MESSAGE FORMAT CONFIRMED; NATIVE PINNING OPEN

The public bootstrap registers `gmsv_gluapack_autorefresh`, reads one path string, and removes the path's normalized keys from its tables. It does not re-include autorun files in that recovered version. The native `Autorefresh detected! Repacking...` flow and absence of hidden connection pinning cannot be proven without the plugin.

Implementation decision: pin the current immutable generation at `ClientConnect`; keep its manifest/stub mapping while a slot holds the pin; publish a new generation without modifying old records. Loaded ready clients receive only generation/path metadata and fetch the new object over HTTP. Autorun paths are re-included after validated mount.

### 10. Bootstrap injection point — FILE POSITION CONFIRMED; NATIVE DETOURS OPEN

The public artifact places the bootstrap at the top of `lua/includes/init.lua`, before the normal GMod init body. The unavailable plugin's `CLuaInterface::Init` / `CLuaManager::Startup` detours and byte signatures could not be inspected. The target x86-64 `CLuaManager::Startup` was located from `CLuaManager::Startup Lua already exsits?` at approximately `0x00c20120`, but that alone does not reveal the plugin injection instruction.

Implementation decision: compose through HolyLib's existing `AddOrUpdateFile` ownership by prepending a self-contained bootstrap only to `includes/init.lua`; that file is always excluded from stubbing. This avoids adding unverified Lua-manager detours. `// TODO(review):` live-test on the target branch that the init file executes before the first non-init Lua request; safety is unaffected because no READY means vanilla delivery.

### 11. Failure handling and the `cl_downloadfilter=none` limbo — CONFIRMED

The recovered `failed(message, disconnect, openHelp)` installs a no-op resolver when `disconnect` is truthy. Missing-pack and MD5-mismatch sites pass a disconnect reason. At label `::failed::`, however, `RunConsoleCommand("disconnect")` executes only when `cl_downloadfilter ~= "none"`; the code then requires three base modules and returns. Thus the exact downloads-disabled branch does not disconnect and cannot install the real packed Lua state: it is a genuine limbo path.

Implementation decision: HolyLib never acknowledges a missing, undecodable, undecompressible, unparsable, or MD5-mismatched generation. It prints actionable download-filter guidance and continues the ordinary init body. Because the server has not received READY, every requested file is delivered normally. No client waits silently and no forced disconnect is required for recovery.

### 12. Sigscan validity — CONFIRMED for HolyLib symbols; incumbent comparison OPEN

HolyLib resolves `AddOrUpdateFile` and `SendFileToClient` by the exported Itanium names above on Linux and its existing Windows symbols. Those names match both supplied target server binaries. HolyLib does not need new signatures for luapack.

The incumbent signatures for those functions, `CLuaInterface::Init`, `CLuaManager::Startup`, and `AddResource` cannot be extracted without its plugin, so cross-plugin signature drift remains open.

What would settle it: provide both stripped plugin architectures plus the exact target `lua_shared.so`; record hook setup xrefs and compare patterns byte-for-byte against these target modules.

## Design traceability

| HolyLib choice | Evidence |
|---|---|
| Normal compressed stub message, never suppression | Findings 1 and 4 |
| Per-client READY gate and native last-resort fallback | Findings 1, 2, and 11 |
| Existing module/detours and worker infrastructure | Findings 3 and 12 |
| Three salted path keys, versioned entry stream | Findings 5 and 6 |
| No encryption or secret | Finding 5; clean-room/no-DRM requirement |
| Atomic retained-generation manifest | Findings 8 and 9 |
| Init-file bootstrap excluded from stubbing | Finding 10 |
| Missing/corrupt pack means no READY, not limbo | Finding 11 |
| Only HTTP carries the pack body | Findings 1, 7, and 9 |

## Remaining runtime checks

- `// TODO(review):` capture a target-branch join to verify `includes/init.lua` is processed before any file that could receive a stub.
- `// TODO(review):` capture one fallback and one ready join to confirm the client accepts a requested file whose normal string-table hash describes the real source while its delivered payload is the stub.
- `// TODO(review):` if the stripped plugin is supplied, complete native findings 2, 3 (incumbent half), 4 (exact stub), 5 (producer), 7 (registration), 8 (set timing), 9 (pinning), 10 (detours), and 12 (signature comparison) before claiming binary equivalence.
