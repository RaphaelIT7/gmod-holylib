# ghostinj.dll

This dll is used by the `-usegh` [command line argument](https://github.com/RaphaelIT7/obsolete-source-engine/blob/gmod/dedicated/sys_common.cpp#L51) and it allows one to do stuff before the dedicated server is loaded.  

ToDo: Allow it to load a `ghostinj2.dll` so that if you have multiple ones, this should work with them. Or should we have a `ghostinj/` folder which where all files from it will be loaded?

### How it's called

File (with link to the line) - Function  
[sys_ded.cpp](https://github.com/RaphaelIT7/obsolete-source-engine/blob/gmod/dedicated/sys_ded.cpp#L517) - `main`  
[sys_common.cpp](https://github.com/RaphaelIT7/obsolete-source-engine/blob/gmod/dedicated/sys_common.cpp#L88) - `InitInstance`  
[sys_common.cpp](https://github.com/RaphaelIT7/obsolete-source-engine/blob/gmod/dedicated/sys_common.cpp#L53) - `Load3rdParty`  