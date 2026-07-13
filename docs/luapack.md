# Bundled clientside Lua over FastDL

HolyLib's `gmoddatapack` module can bundle registered clientside Lua into one immutable LZMA pack and put that object in the Source `downloadables` table. The engine downloads it during the normal HTTP FastDL resource phase. The multi-megabyte body is never sent over the game netchannel.

The feature is experimental and defaults off. The `gmoddatapack` module itself must also be enabled.

## Safety model

- A connecting slot is pinned to the current immutable generation.
- The client mounts and validates downloaded packs before sending `READY(generation, md5)`.
- Only a ready slot receives tiny per-file stubs. A non-ready, expired, corrupt, downloads-disabled, or otherwise uncertain slot receives normal HolyLib/vanilla client Lua.
- Every stub is an ordinary reliable `LuaFileDownload` for the requested file ID, so the Requesting Lua barrier advances.
- Publishing creates the object and registers it in `downloadables` before replacing the one replicated manifest snapshot.
- Old generations remain addressable until pins drain and their retention TTL passes. Content-addressed files remain on disk for operator/CDN cache policy.
- Encryption, DRM, licensing, telemetry, and external defaults do not exist. The only optional outbound request is the operator-configured ingest hook.

## Configuration

| Name | Default | Meaning |
|---|---:|---|
| `holylib_enable_gmoddatapack` | module default off | Enable the existing module and detours. |
| `holylib_gmoddatapack_luapack_enable` | `0` | Master switch. `0` preserves stock HolyLib behavior. |
| `holylib_gmoddatapack_luapack_packdir` | `holylib/luapack` | Directory below `garrysmod/data`; use a stable relative path. |
| `holylib_gmoddatapack_luapack_downloadurl_policy` | `respect` | `respect`, `require`, or `lock` as described below. |
| `holylib_gmoddatapack_luapack_ingest_url` | empty | Optional HTTP endpoint receiving the compressed object body. |
| `holylib_gmoddatapack_luapack_ingest_method` | `PUT` | Method used by the optional ingest request. |
| `holylib_gmoddatapack_luapack_retention_ttl` | `300` | Seconds an unpinned superseded manifest entry remains retained. |
| `holylib_gmoddatapack_luapack_ready_deadline` | `30` | Maximum seconds to accept a connecting/refresh READY acknowledgement. |
| `holylib_gmoddatapack_luapack_manifest` | empty | Internal atomic replicated snapshot; do not set manually. |

`sv_downloadurl` remains operator-owned:

- `respect` never writes it.
- `require` refuses optimized publication while it is empty.
- `lock` remembers its value while luapack is active and restores accidental changes. It does not invent a URL.

The ingest worker is asynchronous and non-fatal. This repository's `cpp-httplib` build is not linked to OpenSSL, so built-in ingestion accepts `http://` only and refuses to downgrade `https://`. Operators needing HTTPS can handle the pluggable server hook `HolyLib:OnLuaPackBuilt(generation, resourcePath, md5, compressedSize)` in their existing trusted uploader. Do not place credentials in archived cvars or commit them to configuration.

## Kill switch

Run this from the server console/RCON, or as a superadmin player:

```text
holylib_gmoddatapack_luapack_kill
```

The command sets the master switch to `0`. Stub decisions stop immediately; the next frame clears the replicated manifest. Existing and new file requests use normal Lua networking without a restart.

## FastDL layout

For a generation with MD5 `abc...`, HolyLib writes and registers:

```text
garrysmod/data/<packdir>/abc....bsp
downloadables entry: data/<packdir>/abc....bsp
client cache: download/data/<packdir>/abc....bsp
```

Mirror `garrysmod/data/<packdir>/` into the same relative `data/<packdir>/` path below the configured FastDL origin. CDN replication, overseas routing, cache invalidation, and `.bz2` generation are operator responsibilities. Do not configure a pipeline that removes a generation while it can still appear in the retained manifest.

## Rollout and verification

1. Deploy to one staging server with both module and feature flags off. Verify ordinary joins first.
2. Configure a reachable `sv_downloadurl` and mirror pipeline. Keep `downloadurl_policy=require` during validation.
3. Enable `holylib_enable_gmoddatapack 1`, leave luapack off, and verify the existing async-compression path.
4. Off-peak, set `holylib_gmoddatapack_luapack_enable 1`. Confirm the log reports one immutable generation, the object exists on FastDL, and the manifest is non-empty.
5. Join with downloads enabled. Confirm one `.bsp` HTTP object, a READY log for the pinned generation, and successful completion of Requesting Lua.
6. Start a throttled join on generation G1, modify a registered Lua file to publish G2, and confirm that join still acknowledges/uses G1 while new joins use G2.
7. Join with `cl_downloadfilter none`. Confirm the client prints guidance, sends no READY, receives normal per-file Lua, and reaches the game rather than remaining in limbo.
8. Corrupt or remove the FastDL object and repeat. Confirm MD5/decompression/missing failures stay vanilla.
9. Exercise the kill switch during a join and confirm subsequent requests are real files, not stubs.
10. Watch netchannel and HTTP byte counts at production population before expanding rollout.

## Risks and non-goals

Clientside Lua is in every player's join path, so this has the highest practical blast radius in the module. Engine interfaces, exported names, and init ordering can drift between GMod branches; stage every engine update and retain the kill switch.

This feature does not change the pack body delivery channel, does not use `sv_allowdownload`, and does not add a netchannel body fallback. Tiny READY and autorefresh metadata messages are control-plane only. CDN reachability, overseas edge behavior, TTL, purge, and origin upload policy remain outside the module.

See [the clean-room functional analysis](luapack-gluapack-re.md) for evidence and open runtime questions.

