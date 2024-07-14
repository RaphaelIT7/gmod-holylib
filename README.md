# Holylib

A library that will contain some functions and optimizations for gmod.  
If you need any function, make an issue for it, and I'll look into it.  

# How to Install
1. Download the `ghostinj.dll`, `holylib.vdf` and `gmsv_holylib_linux.so` from the latest release.  
2. Put the `ghostinj.dll` into the main directory where `srcds_linux` is located.  
3. Put the `holylib.vdf` into the `garrysmod/addons/` directory.  
4. Put the `gmsv_holylib_linux.so` into the `garrysmod/lua/bin/` directory.
5. Add `-usegh` to the servers startup params.  
If you use a panel like Pterodactyl or something similar, you can use the gamemode field(in most cases) like this: `sandbox -usegh`  

If you already had a `ghostinj.dll`, you can rename it to `ghostinj2.dll` and it will be loaded by holylib's ghostinj.  

# Modules
Each module has its own convar `holylib_enable_[module name]` which allows you to enable/disable specific modules.  

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
This module removes the host error when the model or generic precache stringtable overflows. Instead it will throw a warning.  
If these stringtables overflow, expect the models that failed to precache to be errors.  

### Hooks
This module calls these hooks from (`hook.Run`)  

#### HolyLib:OnModelPrecacheFail(string model)
string model - The model that failed to precache.  

#### HolyLib:OnGenericPrecacheFail(string model)
string model - The model that failed to precache.  

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

Returns `nil` or the `INetworkStringTable`  

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

### Enums
This module adds these enums  

#### number stringtable.INVALID_STRING_INDEX
This value is returned if the index of a string is invalid, like if INetworkStringTable:AddString fails.

### Hooks
This module calls these hooks from (`hook.Run`)  

#### HolyLib:OnStringtableCreation()
NOTE: This is currently broken, since it's actually called before Lua :/


## pvs
This module plans to implement multiple PVS related functions.  

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

#### pvs.AddEntityToPVS(Entity ent)
Adds the given entity index to the PVS  

#### pvs.OverrideStateFlags(Entity ent, number flags, bool force)
bool force - Allows you to set the flags directly. It won't make sure that the value is correct!  
Overrides the StateFlag for this Snapshot.  
The value will be reset in the next one.  
NOTE: If you use force, you should know what your doing or else it might cause a crash.  

#### pvs.SetStateFlags(Entity ent, number flags, bool force)
bool force - Allows you to set the flags directly. It won't make sure that the value is correct!  
Sets the State flag for this entity.  
Unlike `OverrideStateFlag`, this won't be reset after the snapshot.  
NOTE: If you use force, you should know what your doing or else it might cause a crash.  

#### number pvs.GetStateFlags(Entity ent, bool force)
bool force - Allows you to get all flags instead of only the ones for networking.  
Returns the state flags for this entity.  

### Enums

#### pvs.FL_EDICT_DONTSEND  
The Entity won't be networked.  

#### pvs.FL_EDICT_ALWAYS  
The Entity will always be networked.  

#### pvs.FL_EDICT_PVSCHECK  
The Entity will only be networked if it's inside the PVS.  

#### pvs.FL_EDICT_FULLCHECK  
The Entity's `ShouldTransmit` function will be called, and its return value will be used.  

## filesystem
This module currently only contains two optimizations for the filesystem.  

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

## util

### Functions

#### util.AsyncCompress(string data, number level = 5, number dictSize = 65536, function callback)
Works like util.Compress but it's async and allows you to set the level and dictSize.  
The defaults for level and dictSize are the same as gmod's util.Compress.  
Both AsyncCompress and AsyncDecompress use the same thread.  

#### util.AsyncDecompress(string data, function callback)
Works like util.Decompress but it's async.  

# Unfinished Modules

## serverplugins
This module adds two new `IServerPluginCallbacks` functions:  
`virtual void OnLuaInit( GarrysMod::Lua::ILuaInterface* LUA )`  
`virtual void OnLuaShutdown( GarrysMod::Lua::ILuaInterface* LUA )`  

## sourcetv
This module plans to add a new `sourcetv` library and a new class `HLTVPlayer` will allow a SourceTV client to send net messages to the server.  

# Issues implemented / fixed
`gameevent.GetListeners` -> https://github.com/Facepunch/garrysmod-requests/issues/2377  
`stringtable.FindTable("modelprecache"):GetNumStrings()` -> https://github.com/Facepunch/garrysmod-requests/issues/82  
https://github.com/Facepunch/garrysmod-issues/issues/5932  
`HolyLib.Reconnect(ply)` -> https://github.com/Facepunch/garrysmod-requests/issues/2089  
`pvs.AddEntityToPVS(ent)` -> https://github.com/Facepunch/garrysmod-requests/issues/245  
`util.AsyncCompress(data, nil, nil, callback)` -> https://github.com/Facepunch/garrysmod-requests/issues/2165   
It now throws a warning instead of crashing -> https://github.com/Facepunch/garrysmod-issues/issues/56  

# Things planned to add:
https://github.com/Facepunch/garrysmod-requests/issues/1884  
https://github.com/Facepunch/garrysmod-requests/issues/2298  
https://github.com/Facepunch/garrysmod-requests/issues/2237  
(Maybe)https://github.com/Facepunch/garrysmod-requests/issues/132  