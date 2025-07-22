# Sources

Garry's Mod common used (only for 64x): https://github.com/RaphaelIT7/garrysmod_common.git  
SourceSDK minimal used: https://github.com/RaphaelIT7/sourcesdk-minimal.git  

You can find all of the sources/branches used here: https://github.com/RaphaelIT7/gmod-holylib/blob/main/.github/workflows/compile.yml#L27-L30  

# How to build HolyLib

1. Clone garrysmod_common & its submodules into the `development` folder.  
So the result should be a structure like this:  
\- `development`  
\- \- `module`  
\- \- `garrysmod_common`   

2. Enter the `development/module` folder  
3. Run `premake5.exe vs2022` or for Linux `premake5.exe gmake`  
4. Now you can open the generated project file and do your stuff.  