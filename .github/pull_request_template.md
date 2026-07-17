<!--
By submitting this pull request, you agree that:
- You created this contribution or have the right to submit it.
- You retain copyright of your contribution.
- You grant the maintainer a perpetual, worldwide, non-exclusive, royalty-free, irrevocable license to use, modify, distribute, sublicense, and relicense your contribution in any form.

Please ensure code provenance is clear where relevant.
If you include or adapt logic from external sources (SDKs, libraries, or other projects), please mention it in the PR description.
-->

## Summary

<!-- What changed, and why is this the right approach? -->

## Related issue

<!-- Link an issue or discussion, for example: Closes #123. -->

## Targets tested

<!-- Check every target you actually tested. Leave untested targets unchecked. -->

- [ ] Linux dedicated server (32-bit)
- [ ] Linux dedicated server (64-bit)
- [ ] Windows dedicated server (32-bit or 64-bit)
- [ ] Windows client (32-bit or 64-bit)
- [ ] Garry's Mod `public`
- [ ] Garry's Mod `dev` / `prerelease`
- [ ] Garry's Mod `x86-64`

## Validation

<!-- Include exact build/test commands, relevant GLuaTest results, and manual reproduction steps. -->

## Compatibility and risk

<!--
Call out changes to engine interfaces, signatures, detours, object layouts, thread behavior,
Lua API behavior, module defaults, or unload/shutdown behavior. Explain rollback steps.
-->

> [!CAUTION]
> HolyLib runs inside the Source engine and exposes unsafe operations. Test changes on an
> expendable client/server instance with backups. A successful compile does not establish
> runtime compatibility across Garry's Mod branches, operating systems, or architectures.

## Checklist

- [ ] The change is focused; unrelated generated files, binaries, and submodule changes are excluded.
- [ ] New or changed engine signatures/layout assumptions identify the exact target build(s).
- [ ] New Lua-facing behavior validates untrusted input or clearly documents why it is unsafe.
- [ ] Crash, shutdown, reload, and concurrency paths were considered where relevant.
- [ ] User-facing behavior or configuration changes are documented.
- [ ] External code or ideas are attributed and their license is compatible.
- [ ] Logs, dumps, configuration, and screenshots contain no credentials, tokens, private paths, or player data.
