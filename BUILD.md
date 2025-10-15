# Sources

Garry's Mod common used (only for 64x): https://github.com/RaphaelIT7/garrysmod_common.git<br> 
SourceSDK minimal used: https://github.com/RaphaelIT7/sourcesdk-minimal.git<br>

You can find all of the sources/branches used here: https://github.com/RaphaelIT7/gmod-holylib/blob/main/.github/workflows/compile.yml#L27-L30<br>

# How to build HolyLib

1. Clone garrysmod_common & its submodules into the `development` folder.<br>
So the result should be a structure like this:<br>
\- `development`<br>
\- \- `module`<br>
\- \- `garrysmod_common`<br>

2. Clone the sourcesdk-minimal into the `garrysmod_common/sourcesdk-minimal` folder (remove all files before cloning it)<br>
This needs to be done since in the garrysmod_common, the submodule points at the original sourcesdk-minimal and not at our fork.<br>

3. Enter the `development/module` folder<br>
4. Run `premake5.exe vs2022` or for Linux `premake5.exe gmake`<br>
5. Now you can open the generated project file and do your stuff.<br>
