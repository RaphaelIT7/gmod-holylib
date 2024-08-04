# Holylib

A library that contains some functions and optimizations for gmod.  
If you need any function, make an issue for it, and I'll look into it.  

## Windows
So currently to get it working on Windows, I would have to redo most of the hooks, and It would also take a good while.  
Because of this, I'm not going to make it currently. I'm gonna slowly start adding all symbols and then someday I'm going to redo most hooks.  
But for now there won't be a release.   

## How to Install
1. Download the `ghostinj.dll`, `holylib.vdf` and `gmsv_holylib_linux.so` from the latest release.  
2. Put the `ghostinj.dll` into the main directory where `srcds_linux` is located.  
3. Put the `holylib.vdf` into the `garrysmod/addons/` directory.  
4. Put the `gmsv_holylib_linux.so` into the `garrysmod/lua/bin/` directory.
5. Add `-usegh` to the servers startup params.  
If you use a panel like Pterodactyl or something similar, you can use the gamemode field(in most cases) like this: `sandbox -usegh`  

If you already had a `ghostinj.dll`, you can rename it to `ghostinj2.dll` and it will be loaded by holylib's ghostinj.  

## Next Update
- [+] Added partial Linux 64x support.  
- - `bitbuf`  
- - `concommand`  
- - `cvars`  
- - `holylib`  
- - `networking`  
- - `serverplugin`  
- - `steamworks`  
- - `stringtable`  
- - `util`  

- [+] Added a compatibility system for all modules.  
Modules that aren't compatible are disabled by default.  
> NOTE: If some of their functions work, you can still force enable them with their ConVar or with the commandline.  

- [+] Added `filesystem` lua library.  

- [#] Fixed `surffix` module not actually working  
- [#] Fixed a crash in the `surffix` module when noclipping thru a player.  
I forgot to pass the first argument to Gmod which caused this issue.  

- [#] Future improved the filesystem my reducing the usage of `std::string`.  
- [#] Fixed up the sourcesdk to not have all these file name differences and fixed a few things -> (32x)`keyvalues.h` | (64x)`KeyValues.h`  
- [#] Fixed `serverplugins` module not removing all detours on shutdown.  

You can see all changes here:  
https://github.com/RaphaelIT7/gmod-holylib/compare/Release0.3...main

### QoL updates
- [#] Reduced file size by ~1MB  
- [#] Changed print formatting  
```txt
Registered module holylib         (Enabled: true,  Compatible: true )
Registered module gameevent       (Enabled: true,  Compatible: true )
[...]
```


# Modules
Each module has its own convar `holylib_enable_[module name]` which allows you to enable/disable specific modules.  
You can add `-holylib_enable_[module name] 0` to the startup to disable modules on startup.  

## holylib
This module contains the HolyLib library.   

### Functions

#### bool HolyLib.Reconnect(Player ply)
Player ply - The Player to reconnect.  

Returns `true` if the player was successfully reconnected.  

#### void HolyLib.HideServer(bool hide)
bool hide - `true` to hide the server from the serverlist.  

## gameevent
This module contains additional functions for the gameevent library.  

### Functions

#### (int or table) gameevent.GetListeners(string name)
string name(optional) - The event to return the count of listeners for.  
If name is not a string, it will return a table containing all events and their listener count:  
```lua
{
	["player_spawn"] = 1
}
```

#### bool gameevent.RemoveListener(string name)
string name - The event to remove the Lua gameevent listener from.  

Returns `true` if the listener was successfully removed from the given event.

## threadpoolfix
This module modifies `CThreadPool::ExecuteToPriority` to not call `CThreadPool::SuspendExecution` when it doesn't have any jobs.  
This is a huge speed improvement for adding searchpaths / mounting legacy addons.  
> NOTE: This requires the `ghostinj.dll` to be installed!

## precachefix
This module removes the host error when the model or generic precache stringtable overflows.  
Instead it will throw a warning.  

If these stringtables overflow, expect the models that failed to precache to be an error.  

### Hooks
This module calls these hooks from (`hook.Run`)  

#### HolyLib:OnModelPrecache(string model, number idx)
string model - The model that failed to precache.  
number idx - The index the model was precache in.  

#### HolyLib:OnModelPrecacheFail(string model)
string model - The model that failed to precache.  

#### HolyLib:OnGenericPrecache(string file, number idx)
string file - The file that failed to precache.  
number idx - The index the file was precache in.  

#### HolyLib:OnGenericPrecacheFail(string file)
string file - The file that failed to precache.  

## stringtable
This module adds a new library called `stringtable`, which will contain all functions to handle stringtables,  
and it will a hook for when the stringtables are created, since they are created while Lua was already loaded.  

NOTE: For now, all functions below are just a bind to their C++ versions -> [INetworkStringTable](https://github.com/RaphaelIT7/obsolete-source-engine/blob/gmod/public/networkstringtabledefs.h#L32)  

### Functions

#### INetworkStringTable stringtable.CreateStringTable(string tablename, number maxentries, number userdatafixedsize = 0, number userdatanetworkbits = 0)
string tablename - The name for the stringtable we want to create.  
number maxentries - The maximal amount of entries.  
number userdatafixedsize(default `0`) - The size of the userdata.  
number userdatanetworkbits(default `0`) - The networkbits to use for the userdata.  

Returns `nil` or a `INetworkStringTable`.  

#### stringtable.RemoveAllTables()
Nuke all stringtables. BOOOM  

#### INetworkStringTable stringtable.FindTable(string tablename)
string tablename - The table to search for  

Returns `nil` or the `INetworkStringTable`  

#### INetworkStringTable stringtable.GetTable(number tableid)
number tableid - The tableid of the table to get  

Returns `nil` or the `INetworkStringTable`  

#### number stringtable.GetNumTables()
Returns the number of stringtables that exist.  

#### INetworkStringTable stringtable.CreateStringTableEx(string tablename, number maxentries, number userdatafixedsize = 0, number userdatanetworkbits = 0, bool bIsFilenames = false )
string tablename - The name for the stringtable we want to create.  
number maxentries - The maximal amount of entries.  
number userdatafixedsize(default `0`) - The size of the userdata.  
number userdatanetworkbits(default `0`) - The networkbits to use for the userdata.  
bool bIsFilenames(default `false`) - If the stringtable will contain filenames.  

#### stringtable.SetAllowClientSideAddString(INetworkStringTable table, bool bAllowClientSideAddString)
INetworkStringTable table - The table to set it on  
bool bAllowClientSideAddString - If clients should be able to modify the stringtable.  

#### bool stringtable.IsCreationAllowed()
Returns whether you're allowed to create stringtables.  

#### stringtable.IsLocked()
Returns if the stringtable is locked or not.  

#### stringtable.AllowCreation(bool bAllowCreation)
Sets whether you're allowed to create stringtable.  

Example:  
```lua
stringtable.AllowCreation(true)
stringtable.CreateStringTable("example", 8192)
stringtable.AllowCreation(false)
```

> NOTE: Please use the `HolyLib:OnStringTableCreation` hook to add custom stringtables.  

#### stringtable.RemoveTable(INetworkStringTable table)
Deletes that specific stringtable.  

### INetworkStringTable
This is a metatable that is pushed by this module. It contains the functions listed below  

#### string INetworkStringTable:GetTableName() 
Returns the name of the stringtable  

#### number INetworkStringTable:GetTableId() 
Returns the id of the stringtable  

#### number INetworkStringTable:GetNumStrings() 
Returns the number of strings this stringtable has  

#### number INetworkStringTable:GetMaxStrings() 
Returns the maximum number of string this stringtable has  

#### number INetworkStringTable:GetEntryBits() 
ToDo: I have no idea  

#### INetworkStringTable:SetTick(number tick) 
number tick - The tick to set the stringtable to  

Returns the number of strings this stringtable has  

#### bool INetworkStringTable:ChangedSinceTick(number tick) 
number tick - The tick to set the stringtable to  

Returns whether or not the stringtable changed since that tick.  

#### number INetworkStringTable:AddString(bool bIsServer, const char* pStr) 
bool bIsServer - Weather or not the server is adding this value? (Idk, added it so you have more control.)  
string pStr - The string to add  

Returns the index of the added string.  

#### string INetworkStringTable:GetString(number index) 
number index - The index to get the string from  

Returns the string from that index  

#### number INetworkStringTable:FindStringIndex(string pStr) 
string pStr - The string to find the index of  

Returns the index of the given string.  

#### INetworkStringTable:DeleteAllStrings()
Deletes all strings from the stringtable.  

#### INetworkStringTable:SetMaxEntries(number maxEntries)
number maxEntries - The new limit for entries.  

Sets the new Entry limit for that stringtable.  

The new limit will work but for some stringtables, it might cause issues, where the limits are hardcoded clientside.  
A list of known stringtables with hardcoded limits:  
- `modelprecache` -> Limited by `CClientState::model_precache`  
- `genericprecache` -> Limited by `CClientState::generic_precache`  
- `soundprecache` -> Limited by `CClientState::sound_precache`  
- `decalprecache` -> Limited by `CClientState::decal_precache`  
- `networkvars` -> Limited by the internal net message used.  

> NOTE: If there are already more entries than the new limit, they won't be removed.  
> (This could cause issues, so make sure you know what you're doing.)  

#### bool INetworkStringTable:DeleteString(number index)
Deletes the given string at the given index.  

Returns `true` if the string was deleted.  

> NOTE: Currently this deletes all strings and readds all except the one at the given index. This is slow and I need to improve it later.  
> BUG: It doesn't readd the userdata the strings.  

#### bool INetworkStringTable:IsValid()
Returns `true` if the stringtable is still valid.  

### Enums
This module adds these enums  

#### number stringtable.INVALID_STRING_INDEX
This value is returned if the index of a string is invalid, like if INetworkStringTable:AddString fails.

### Hooks
This module calls these hooks from (`hook.Run`)  

#### HolyLib:OnStringTableCreation()
You can create stringtables inside this hook.  
If you want to create stringtables outside this hook, use `stringtable.AllowCreation`  

## pvs
This adds a bunch of PVS related functions.  

### Functions

#### pvs.ResetPVS()
Resets the current PVS.  

#### bool pvs.CheckOriginInPVS(Vector vec)
Checks if the given position is inside the current PVS.  

#### pvs.AddOriginToPVS(Vector vec)
Adds the given Origin to the PVS. Gmod already has this function.  

#### number pvs.GetClusterCount()
Returns the number of clusters that exist.  

#### number pvs.GetClusterForOrigin(Vector vec)
Returns the cluster id of the given origin.  

#### bool pvs.CheckAreasConnected(number area1, number area2)
Returns whether or not the given areas are connected.  
We don't validate if you pass valid areas!  

#### number pvs.GetArea(Vector vec)
Returns the area id of the given origin.  

#### pvs.GetPVSForCluster(number clusterID)
Sets the current PVS to that of the given cluster.  
We don't validate if the passed cluster id is valid!  

#### bool pvs.CheckBoxInPVS(Vector mins, Vector maxs)
Returns whether or not the given box is inside the PVS.  

#### pvs.AddEntityToPVS(Entity ent or table ents)
Adds the given entity index to the PVS  

#### pvs.OverrideStateFlags(Entity ent or table ents, number flags, bool force)
table ents - A sequential table containing all the entities which states flags should be overridden.  
bool force - Allows you to set the flags directly. It won't make sure that the value is correct!  
Overrides the StateFlag for this Snapshot.  
The value will be reset in the next one.  
NOTE: If you use force, you should know what your doing or else it might cause a crash.  

#### pvs.SetStateFlags(Entity ent or table ents, number flags, bool force)
table ents - A sequential table containing all the entities which states should be set.  
bool force - Allows you to set the flags directly. It won't make sure that the value is correct!  
Sets the State flag for this entity.  
Unlike `OverrideStateFlag`, this won't be reset after the snapshot.  
NOTE: If you use force, you should know what your doing or else it might cause a crash.  

#### number pvs.GetStateFlags(Entity ent, bool force)
bool force - Allows you to get all flags instead of only the ones for networking.  
Returns the state flags for this entity.  

#### bool pvs.RemoveEntityFromTransmit(Entity ent or table ents)
table ents - A sequential table containing all the entities that should be removed from being transmitted.  
Returns true if the entity or all entities were successfully removed from being transmitted.  

> NOTE: Only use this function inside the `HolyLib:CheckTransmit` hook!  

#### pvs.RemoveAllEntityFromTransmit()
Removes all Entities from being transmitted.  

> NOTE: Only use this function inside the `HolyLib:CheckTransmit` hook!  

#### pvs.AddEntityToTransmit(Entity ent or table ents, bool always)
table ents - A sequential table containing all the entities that should be transmitted.  
bool always - If the entity should always be transmitted? (Verify)  

Adds the given Entity to be transmitted.

> NOTE: Only use this function inside the `HolyLib:CheckTransmit` hook!  

#### (REMOVED AGAIN) pvs.IsEmptyBaseline()
Returns `true` if the baseline is empty.  
This should always be the case after a full update.  

> NOTE: Only use this function inside the `HolyLib:CheckTransmit` hook!  
> REMOVED: This function was removed since I can't get it to work / It would be a bit more complicated than first anticipated.  

#### pvs.SetPreventTransmitBulk(Entity ent or table ents, Player ply or table plys or RecipientFilter filter, bool notransmit)
table ents - A sequential table containing all the entities that should be affected.  
table plys - A sequential table containing all the players that it should set it for.  
bool notransmit - If the entity should stop being transmitted.  

Adds the given Entity to be transmitted.

### Enums

#### pvs.FL_EDICT_DONTSEND  
The Entity won't be networked.  

#### pvs.FL_EDICT_ALWAYS  
The Entity will always be networked.  

#### pvs.FL_EDICT_PVSCHECK  
The Entity will only be networked if it's inside the PVS.  

#### pvs.FL_EDICT_FULLCHECK  
The Entity's `ShouldTransmit` function will be called, and its return value will be used.  

### Hooks

#### HolyLib:CheckTransmit(Entity ply, table entities)
entity ply - The player that everything is transmitted to.  
table enitites - The Entities that get transmitted. Only available if `holylib_pvs_postchecktransmit` is set to `2` or higher.  

> NOTE: This hook is only called when `holylib_pvs_postchecktransmit` is enabled!

### ConVars

#### holylib_pvs_postchecktransmit (default `0`)
If enabled, it will add/call the `HolyLib:CheckTransmit` hook.  
If set to `2` it will also pass a table containing all entitites to the hook (The second argument)  

## surffix
This module ports over [Momentum Mod's](https://github.com/momentum-mod/game/blob/develop/mp/src/game/shared/momentum/mom_gamemovement.cpp#L2393-L2993) surf fixes.  

### ConVars

#### sv_ramp_fix (default `1`)
If enabled, it will enable additional checks to make sure that the player is not stuck in a ramp.  

#### sv_ramp_initial_retrace_length (default `0.2`)

#### sv_ramp_bumpcount (default `8`)

## filesystem
This module contains multiple optimizations for the filesystem.  

### ConVars

#### holylib_filesystem_easydircheck (default `0`)
If enabled, it will check if the file contains a `.` after the last `/`.  
If so it will cause `CBaseFileSystem::IsDirectory` to return false since we assume it's a file.  
This will cause `file.IsDir` to fail on folders with names like these `test/test1.23`.  

#### holylib_filesystem_searchcache (default `1`)
If enabled, it will cause the filesystem to use a cache for the searchpaths.  
When you try to open a file with a path like `GAME` which has multiple searchpaths, it will check each one until its found.  
Now, the first time it searches for it, if it finds it, we add the file and the searchpath to a cache and the next time the same file is searched for, we try to use our cache search path.  

This will improve `file.Open`, `file.Time` and `file.Exists`.  
The more searchpaths exist, the bigger the improvement for that path will be.
Example (I got 101 legacy addons):
```
lua_run local a = SysTime() for k=1, 1000 do file.Exists("garrysmod.ver", "GAME") end print(SysTime()-a)

Disabled: 1.907318733
Enabled: 0.035948700999995
```

You also can test it using the `MOD` path. The performance of `file.Exists` for any search path and `MOD` should be somewhat near each other since, it checks the same amount of searchpaths while this is enabled.  
```
lua_run local a = SysTime() for k=1, 1000 do file.Exists("garrysmod.ver", "GAME") end print(SysTime()-a)
0.033513544999998

lua_run local a = SysTime() for k=1, 1000 do file.Exists("garrysmod.ver", "MOD") end print(SysTime()-a)
0.037827891999996
```
##### NOTES
- If the file doesn't exist, it will still go thru all search paths to search for it again!  
- I don't know if this has any bugs, but while using this for ~1 Month on a server, I didn't find any issues.  
- It will also improve the `MOD` search path since it also has multiple search paths.  

#### holylib_filesystem_optimizedfixpath (default `1`)
If enabled, it will optimize the `CBaseFileSystem::FixPath` function by caching the `BASE_PATH`.  

#### holylib_filesystem_earlysearchcache (default `1`)
If enabled, it will check the searchcache inside `CBaseFileSystem::OpenForRead`.  

#### holylib_filesystem_forcepath (default `1`)
If enabled, it will force the pathID for specific files.  

#### holylib_filesystem_predictpath (default `1`)
If enabled, it will try to predict the path for a file.  

Example:  
Your loading a model.  
First you load the `example.mdl` file.  
Then you load the `example.phy` file.   
Here we can check if the `example.mdl` file is in the searchcache.  
If so, we try to use the searchpath of that file for the `.phy` file and since all model files should be in the same folder, this will work for most cases.  
If we fail to predict a path, it will end up using one additional search path.  

#### holylib_filesystem_splitgamepath (default `1`)
If enabled, it will split each `GAME` path into multiple search paths, depending on it's contents.  
Then when you try to find a file with the `GAME` search path, it will change the pathID to the content path.  

Example:  
File: `cfg/game.cfg`  
Path: `GAME`  
becomes:  
File: `cfg/game.cfg`  
Path: `CONTENT_CONFIGS`    

This will reduce the amount of searchpaths it has to go through which improves performance.  

Content paths:  
- `materials/` -> `CONTENT_MATERIALS`  
- `models/` -> `CONTENT_MODELS`  
- `sound/` -> `CONTENT_SOUNDS`  
- `maps/` -> `CONTENT_MAPS`  
- `resource/` -> `CONTENT_RESOURCE`  
- `scripts/` -> `CONTENT_SCRIPTS`  
- `cfg/` -> `CONTENT_CONFIGS`  
- `gamemodes/` -> `LUA_GAMEMODES`  
- `lua/includes/` -> `LUA_INCLUDES`  

#### holylib_filesystem_splitluapath (default `0`)
Does the same for `lsv` to save performance.  

> BUG: This currently breaks workshop addons.  

Lua paths:  
- `sandbox/` -> `LUA_GAMEMODE_SANDBOX`  
- `effects/` -> `LUA_EFFECTS`  
- `entities/` -> `LUA_ENTITIES`  
- `weapons/` -> `LUA_WEAPONS`  
- `lua/derma/` -> `LUA_DERMA`  
- `lua/drive/` -> `LUA_DRIVE`  
- `lua/entities/` -> `LUA_LUA_ENTITIES`  
- `vgui/` -> `LUA_VGUI`  
- `postprocess/` -> `LUA_POSTPROCESS`  
- `matproxy/` -> `LUA_MATPROXY`  
- `autorun/` -> `LUA_AUTORUN`  

#### holylib_filesystem_splitfallback (default `1`)
If enabled, it will fallback to the original searchpath if it failed to find something in the split path.  
This is quite slow, so disabling this will improve performance to find files that doesn't exist.  

#### holylib_filesystem_predictexistance (default `1`)
If enabled, it will try to predict the path of a file, but if the file doesn't exist in the predicted path, we'll just say it doesn't exist.  
Doesn't rely on `holylib_filesystem_predictpath` but it also works with it together.  

#### holylib_filesystem_fixgmodpath (default `1`)
If enabled, it will fix up weird gamemode paths like sandbox/gamemode/sandbox/gamemode which gmod likes to use.  
Currently it fixes these paths:  
- `[Active gamemode]/gamemode/[anything]/[active gamemode]/gamemode/` -> (Example: `sandbox/gamemode/spawnmenu/sandbox/gamemode/spawnmenu/`)  
- `include/include/`  

#### (EXPERIMENTAL) holylib_filesystem_cachefilehandle (default `0`)
If enabled, it will cache the file handle and return it if needed.  
> NOTE: This will probably cause issues if you open the same file multiple times.  
> WARNING: This is a noticeable improvement, but it seems to break .bsp files :/  

#### holylib_filesystem_debug (default `0`)
If enabled, it will print all filesyste suff.  

### ConCommands

#### holylib_filesystem_dumpsearchcache
Dumps the searchcache into the console.  
ToDo: Allow one to dump it into a file.  

#### holylib_filesystem_getpathfromid
Dumps the path for the given searchpath id.  
The id is the one listed with each file in the dumped searchcache.  

#### holylib_filesystem_nukesearchcache
Nukes the searchcache.  

### Functions
This module also adds a `filesystem` library which should generally be faster than gmod's functions, because gmod has some weird / slow things in them.  
It also gives you full access to the filesystem and doesn't restrict you to specific directories.  

#### (FSASYNC Enum) filesystem.AsyncRead(string fileName, string gamePath, function callBack(string fileName, string gamePath, FSASYNC status, string content), bool sync)
Reads a file async and calls the callback with the contents.  

#### filesystem.CreateDir(string dirName, string gamePath = "DATA")
Creates a directory in the given path.  

#### filesystem.Delete(string fileName, string gamePath = "DATA")
Deletes the given file.  

#### bool filesystem.Exists(string fileName, string gamePath)
Returns `true` if the given file exists.  

#### table(Files), table(Folders) filesystem.Find(string filePath, string gamePath, string sorting = "nameasc")
Finds and returns a table containing all files and folders in the given path.  

#### bool filesystem.IsDir(string fileName, string gamePath)
Returns `true` if the given file is a directory.  

#### File filesystem.Open(string fileName, string fileMode, string gamePath = "GAME")
Opens the given file or returns `nil` on failure.  

#### bool filesystem.Rename(string origFileName, string newFileName, string gamePath = "DATA")
Renames the given file and returns `true` on success.  

#### number filesystem.Sizestring fileName, string gamePath = "GAME")
Returns the size of the given file.  

#### number fileystem.Time(string fileName, string gamePath = "GAME")
Returns the unix time of the last modification of the given file / folder.  

#### filesystem.AddSearchPath(string folderPath, string gamePath, bool addToTail = false)
Adds the given folderPath to the gamePath searchpaths.  

#### bool filesystem.RemoveSearchPath(string folderPath, string gamePath)
Removes the given searchpath and returns `true` on success.  

#### filesystem.RemoveSearchPaths(string gamePath)
Removes all searchpaths with that gamePath  

Example:  
`filesystem.RemoveSearchPaths("GAME")` -- Removes all `GAME` searchpaths.  

#### filesystem.RemoveAllSearchPaths()
Removes all searchpaths.  

#### string filesystem.RelativePathToFullPath(string filePath, string gamePath)
Returns the full path for the given file or `nil` on failure.  

#### string filesystem.FullPathToRelativePath(string fullPath, string gamePath = nil)
Returns the relative path for the given file.  

## util
This module adds two new functions to the `util` library.  

### Functions

#### util.AsyncCompress(string data, number level = 5, number dictSize = 65536, function callback)
Works like util.Compress but it's async and allows you to set the level and dictSize.  
The defaults for level and dictSize are the same as gmod's util.Compress.  
Both AsyncCompress and AsyncDecompress use the same thread.  

#### util.AsyncCompress(string data, function callback)
Same as above, but uses the default values for level and dictSize.  

#### util.AsyncDecompress(string data, function callback)
Works like util.Decompress but it's async.  

## concommand
This module unblocks `quit` and `exit` for `RunConsoleCommand`.  

### ConVars

#### holylib_concommand_disableblacklist (default `0`)
If enabled, it completely disables the concommand/convar blacklist.  

## vprof
This module adds VProf to gamemode calls and adds two convars.

### ConVars

#### holylib_vprof_exportreport (default `1`)
If enabled, vprof results will be dumped into a file in the vprof/ folder  

#### holylib_sv_stressbots (default `0`)
Sets the value of `sv_stressbots`.  
`sv_stressbots` is a hidden convar which is very useful for performance tests with bots.  

### cvars
This module adds one function to the `cvars` library.  

#### Functions

##### table cvars.GetAll()
Returns a sequential table with all ConVar's that exist.  

## sourcetv
This module plans to add a new `sourcetv` library and a new class `HLTVPlayer` will allow a SourceTV client to send net messages to the server.  

### Functions

#### bool sourcetv.IsActive()
Returns `true` if sourcetv is active.

#### bool sourcetv.IsRecording()
Returns `true` if sourcetv is recording.  

#### bool sourcetv.IsMasterProxy()
Returns `true` if sourcetv server is the master proxy.  

#### bool sourcetv.IsRelay()
Returns `true` if the sourcetv server is a relay.  

#### number sourcetv.GetClientCount()
Returns the number of sourctv clients connected.  

#### number sourcetv.GetHLTVSlot()  
Returns the slot of the sourcetv client/bot.  

#### number sourcetv.StartRecord(string fileName)
string fileName - The name for the recording.  

Returns one of the `RECORD_` enums.  

#### string sourcetv.GetRecordingFile()
Returns the filename of the current recording. or `nil`.  

#### number sourcetv.StopRecord()  
Stops the active record.  

Returns one of the `RECORD_` enums.  
If successfully stopped, it will return `sourcetv.RECORD_OK`.

#### table sourcetv.GetAll()
Returns a table that contains all HLTV clients. It will return `nil` on failure.  

#### HLTVClient sourcetv.GetClient(number slot)
Returns the HLTVClient at that slot or `nil` on failure.  

### HLTVClient
This is a metatable that is pushed by this module. It contains the functions listed below  

#### string HLTVClient:\_\_tostring__()
Returns the a formated string.  
Format: `HLTVClient [%i][%s]`  
`%i` -> UserID  
`%s` -> ClientName  


#### string HLTVClient:GetName()
Returns the name of the client.  

#### string HLTVClient:GetSteamID()
Returns the steamid of the client.  

> NOTE: Currently broken / will return `STEAM_ID_PENDING`

#### number HLTVClient:GetUserID()
Returns the userid of the client.  

#### number HLTVClient:GetSlot()
Returns the slot of the client. Use this for `sourcetv.GetClient`.  

#### void HLTVClient:Reconnect()
Reconnects the HLTV client.  

#### void HLTVClient:ClientPrint(string message)
Prints the given message into the client's console.  

> NOTE: It won't add `\n` at the end of the message, so you will need to add it yourself.  

#### bool HLTVClient:IsValid()
Returns `true` if the client is still valid.  

#### bool (REMOVED) HLTVClient:SendLua(string code)
Sends the given code to the client to be executed.  
Returns `true` on success.  

> NOTE: This function is currently broken and will be readded when it's fixed.  

### Enums

#### sourcetv.RECORD_OK = 0
The recording was started.  

#### sourcetv.RECORD_NOSOURCETV = -1  
SourceTV is not active!  

#### sourcetv.RECORD_NOTMASTER = -2  
the sourcetv server is not the master!  

#### sourcetv.RECORD_ACTIVE = -3  
there already is an active record!  

> NOTE: Should we allow multiple active record? I think I could implement it. If wanted, make a request for it.  

#### sourcetv.RECORD_NOTACTIVE = -4  
there is no active recording to stop!  

#### sourcetv.RECORD_INVALIDPATH = -5  
The filepath for the recording is invalid!  

#### sourcetv.RECORD_FILEEXISTS = -6  
A file with that name already exists!  

### Hooks

#### HolyLib:OnSourceTVNetMessage(HLTVClient client, bf_read buffer)
Called when a HLTVClient sends a net message to the server.  

Example:  
```lua
util.AddNetworkString("Example")
hook.Add("HolyLib:OnSourceTVNetMessage", "Example", function(client, bf)
	local name = util.NetworkIDToString(bf:ReadShort())) -- Reads the header
	print(name, bf:ReadString())
end)

---- Client

net.Start("Example")
	net.WriteString("Hello World from HLTVClient");
net.SendToServer()
```

#### HolyLib:OnSourceTVCommand(HLTVClient client, string cmd, table args, string argumentString)
Called when a HLTVClient sends a command.  
return `true` to tell it that the command was handled in Lua.  

Example:  
```lua
hook.Add("HolyLib:OnSourceTVCommand", "Example", function(client, name, args, argString)
	if name == "Example" then
		client:ClientPrint("Hello World from HLTVServer\n")
		return true
	end
end)
```

### ConVars

#### holylib_sourcetv_allownetworking (default `0`)
If enabled, HLTV Clients can send net messages to the server and `HolyLib:OnSourceTVNetMessage` will be called.  

#### holylib_sourcetv_allowcommands (default `0`)
If enabled, HLTV Clients can send commands to the server and `HolyLib:OnSourceTVCommand` will be called.  

## bitbuf
This module adds a `bf_read` and later `bf_write` class.  

### bf_read
This class will later be used to read net messages from HLTV clients.  
> ToDo: Finish the documentation below and make it more detailed.  

#### string bf_read:\_\_tostring__()
Returns the a formated string.  
Format: `bf_read [%i]`  
`%i` -> size of data in bits.  

#### bf_read:\_\_gc()
Deletes the buffer internally.  

#### number bf_read:GetNumBitsLeft()
Returns the number of bits left.  

#### number bf_read:GetNumBitsRead()
Returns the number of bits read.  

#### number bf_read:GetNumBits()
Returns the size of the data in bits.  

#### number bf_read:GetNumBytesLeft()
Returns the number of bytes left.  

#### number bf_read:GetNumBytesRead()
Returns the number of bytes read.  

#### number bf_read:GetNumBytes()
Returns the size of the data in bytes.  

#### number bf_read:GetCurrentBit()
Returns the current position/bit.  

#### bool bf_read:IsOverflowed()
Returns `true` if the buffer is overflowed.  

#### number bf_read:PeekUBitLong(number numBits)

#### number bf_read:ReadBitAngle(number numBits)

#### angle bf_read:ReadBitAngles()
Reads and Angle.  

#### number bf_read:ReadBitCoord()

#### number bf_read:ReadBitCoordBits()

#### number bf_read:ReadBitCoordMP(bool bIntegral, bool bLowPrecision)

#### number bf_read:ReadBitCoordMPBits(bool bIntegral, bool bLowPrecision)

#### number bf_read:ReadBitFloat()

#### number bf_read:ReadBitLong(number numBits, bool bSigned)
Reads a number with the given number of bits.  

#### number bf_read:ReadBitNormal()

#### string bf_read:ReadBits(number numBits)
Reads the given number of bits.  

#### vector bf_read:ReadBitVec3Coord()
Reads a Vector.  

#### vector bf_read:ReadBitVec3Normal()
Reads a normalizted Vector.

#### number bf_read:ReadByte()
Reads a byte.  

#### string bf_read:ReadBytes(number numBytes)
Reads the given number of bytes.  

#### number bf_read:ReadChar()
Reads a char.  

#### number bf_read:ReadFloat()
Reads a float.  

#### number bf_read:ReadLong()
Reads a long.  

#### number bf_read:ReadLongLong()
Reads a long long.  

#### bool bf_read:ReadOneBit()
Reads one bit.  

#### number bf_read:ReadSBitLong(number numBits)
Reads a number with the given amout of bits.  

#### number bf_read:ReadShort()
Reads a short.  

#### number bf_read:ReadSignedVarInt32()

#### number bf_read:ReadSignedVarInt64()

#### string bf_read:ReadString()
Reads a string.  

#### number bf_read:ReadUBitLong(number numBits)
Read a number with the given amount of bits.  

#### number bf_read:ReadUBitVar()

#### number bf_read:ReadVarInt32()

#### number bf_read:ReadVarInt64()

#### number bf_read:ReadWord()

## Networking
This module tries to optimize anything related to networking.  
Currently, this only has one optimization which was ported from [sigsegv-mvm](https://github.com/rafradek/sigsegv-mvm/blob/910b92456c7578a3eb5dff2a7e7bf4bc906677f7/src/mod/perf/sendprop_optimize.cpp#L35-L144) into here.  

## steamworks
This module adds a few functions related to steam.  

### Functions

#### steamworks.Shutdown()
Shutdowns the Steam Server.  

#### steamworks.Activate()
Starts/Activates the Steam Server.  

#### steamworks.IsConnected()
Returns `true` if the Steam Server is connected.  

### Hooks

#### HolyLib:OnSteamDisconnect(number result)  
Called when our Steam server loses connection to steams servers.  

#### HolyLib:OnSteamConnect()  
Called when our Steam server successfully connected to steams servers.  

# Unfinished Modules

## serverplugins
This module adds two new `IServerPluginCallbacks` functions:  
`virtual void OnLuaInit( GarrysMod::Lua::ILuaInterface* LUA )`  
`virtual void OnLuaShutdown( GarrysMod::Lua::ILuaInterface* LUA )`  

## pas
This module plans to add a few PAS related functions like `table pas.FindInPAS(Vector pos or Entity ent)`.  
If you got an Idea for a function to add, feel free to comment it into [its issue](https://github.com/RaphaelIT7/gmod-holylib/issues/1).

### Functions

#### (Planned) table pas.FindInPAS(Vector vec / Entity ent)
Vector vec - The position to find all entities in.  
Entity ent - The Entity which should be used to find all entities.  

# Issues implemented / fixed
`gameevent.GetListeners` -> https://github.com/Facepunch/garrysmod-requests/issues/2377  
`stringtable.FindTable("modelprecache"):GetNumStrings()` -> https://github.com/Facepunch/garrysmod-requests/issues/82  
`threadpoolfix` -> https://github.com/Facepunch/garrysmod-issues/issues/5932  
`HolyLib.Reconnect(ply)` -> https://github.com/Facepunch/garrysmod-requests/issues/2089  
`pvs.AddEntityToPVS(ent)` -> https://github.com/Facepunch/garrysmod-requests/issues/245  
`util.AsyncCompress(data, nil, nil, callback)` -> https://github.com/Facepunch/garrysmod-requests/issues/2165   
It now throws a warning instead of crashing -> https://github.com/Facepunch/garrysmod-issues/issues/56  
`HolyLib.Reconnect(ply)` -> https://github.com/Facepunch/garrysmod-requests/issues/2089  
`concommand` module -> https://github.com/Facepunch/garrysmod-requests/issues/1534  
`vprof` module -> https://github.com/Facepunch/garrysmod-requests/issues/2374  
`cvars.GetAll` -> https://github.com/Facepunch/garrysmod-requests/issues/341  
`sourcetv` module -> https://github.com/Facepunch/garrysmod-requests/issues/2298  
`bitbuf` module(unfinished) -> https://github.com/Facepunch/garrysmod-requests/issues/594  
`HLTV` class / `sourcetv` module -> https://github.com/Facepunch/garrysmod-requests/issues/2237  
`surffix` module -> https://github.com/Facepunch/garrysmod-requests/issues/2306

# Things planned to add:
https://github.com/Facepunch/garrysmod-requests/issues/1884  
(Maybe)https://github.com/Facepunch/garrysmod-requests/issues/132  
https://github.com/Facepunch/garrysmod-requests/issues/77  
(Maybe)https://github.com/Facepunch/garrysmod-requests/issues/603  
https://github.com/Facepunch/garrysmod-requests/issues/439  
https://github.com/Facepunch/garrysmod-requests/issues/140  
https://github.com/Facepunch/garrysmod-requests/issues/1323  
(Should be possible?)https://github.com/Facepunch/garrysmod-requests/issues/756  
(Gonna make a seperate ConVar for it)https://github.com/Facepunch/garrysmod-requests/issues/2120  
(Maybe)https://github.com/Facepunch/garrysmod-requests/issues/1633  
https://github.com/Facepunch/garrysmod-requests/issues/1472  
(Maybe)https://github.com/Facepunch/garrysmod-requests/issues/1671  

# ToDo
- Fix the CBaseEntity class having variables that don't exist and add Gmod's variables to it.  
- Make this readme better.  

# Some things for later

## NW Networking  
NW uses a usermessage `NetworkedVar`  
Write order:  
- WriteLong -> Entity handle  
- Char -> Type ID  
- String -> Var name  
- (Value) -> Var value. (Depends on what type it is)  

## NW2 Networking  
NW2 writes it to the baseline.  
Write order:  
- 12 bits - Number of net messages networked.  
- 1 bit - Magic bool. Idk if true, it wil call `GM:EntityNetworkedVarChanged` (Verify)  

loop:  
- 12 bits - var key index  
- 3 bits - var type  
- x bits -var value  