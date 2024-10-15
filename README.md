# Holylib

A library that contains some functions and optimizations for gmod.  
If you need any function, make an issue for it, and I'll look into it.  

## Windows
So currently to get it working on Windows, I would have to redo most of the hooks, and It would also take a good while.  
Because of this, I'm not going to make it currently. I'm gonna slowly start adding all symbols and then someday I'm going to redo most hooks.  
But for now there won't be a release.   

## How to Install (Linux 32x)
1. Download the `ghostinj.dll`, `holylib.vdf` and `gmsv_holylib_linux.so` from the latest release.  
2. Put the `ghostinj.dll` into the main directory where `srcds_linux` is located.  
3. Put the `holylib.vdf` into the `garrysmod/addons/` directory.  
4. Put the `gmsv_holylib_linux.so` into the `garrysmod/lua/bin/` directory.
5. Add `-usegh` to the servers startup params.  
If you use a panel like Pterodactyl or something similar, you can use the gamemode field(in most cases) like this: `sandbox -usegh`  

If you already had a `ghostinj.dll`, you can rename it to `ghostinj2.dll` and it will be loaded by holylib's ghostinj.  

## How to Install (Linux 64x)
1. Download the `holylib.vdf` and `gmsv_holylib_linux.so` from the latest release.  
2. Put the `holylib.vdf` into the `garrysmod/addons/` directory.  
3. Put the `gmsv_holylib_linux.so` into the `garrysmod/lua/bin/` directory.  

## Next Update
\- [+] `vprof` now works on 64x  
\- [+] Added `HolyLib.SetSignonState`, `HolyLib.InvalidateBoneCache` function and the `HolyLib:PostEntityConstructor` hook.
\- [+] Added `steamworks.ForceAuthenticate` to steamworks module.  
\- [+] `HolyLib:On[Generic/Model]PrecacheFail` hooks now also allow you to change the fallback.  
\- [#] Model and Generic precache now fallsback to `-1` instead of `0` by default.  
\- \- [+] Added `holylib_precache_[model/generic]fallback` to change the fallback if wanted.  
\- \- [#] Models now will be errors if they fail to precache.  
\- [#] `util.AsyncCompress/Decompress` now use two seperate threadpools(You can change their size with the new convars).  
\- [#] Fixed a small reference leak in `util.AsyncCompress/Decompress`. The function was leaked so it's not too big.  
\- [#] Extented vprof to include two more functions that call hooks.  
\- \- `gameevent.Listen` callbacks and Gamemode startup hooks should now also show up.  
\- \- Gamemode startup hooks that are now included are: `GM:CreateTeams`, `GM:PreGamemodeLoaded`, `GM:OnGamemodeLoaded`, `GM:PostGamemodeLoaded` and `GM:Initialize`.  
\- [#] Fixed a bug in `vprof` that caused CallWithArgs to show up.  
\- \- shouldn't be a thing since `CallFinish` should always be called after it.  
\- [#] `VoiceData:GetUncompressedData` got the `decompressSize` argument.  
\- [#] Fixed `VoiceData:GetUncompressedData` always erroring (https://github.com/RaphaelIT7/gmod-holylib/pull/11)  

You can see all changes here:  
https://github.com/RaphaelIT7/gmod-holylib/compare/Release0.5...main

### QoL updates
- [#] Cleaned up module code a bit.  

## ToDo
\- Finish 64x (`pvs`, `sourcetv`, `surffix`)  
\- Find out why ConVars are so broken. (Serverside `path` command breaks :<)  
\- Look into filesystem handle optimization  
\- Try to fix that one complex NW2 bug. NOTE: It seems to be related to baseline updates (Entity Creation/Deletion)  
\- Also fix `physenv` module.  
\- \- Add a hook for physics simulation & possibly stopping it.  
\- extent `vprof` module to add a vprof lua object. Should allow one to use vprof in lua.  
\- Look into StaticPropMgr stuff. May be interresting.  
\- Add a bind to `CAI_NetworkManager::BuildNetworkGraph` or `StartRebuild`  
\- Possibly allow on to force workshop download on next level change.  

# The Navigator  
[Modules](https://github.com/RaphaelIT7/gmod-holylib#modules)  
\- [holylib](https://github.com/RaphaelIT7/gmod-holylib#holylib-1)  
\- [gameevent](https://github.com/RaphaelIT7/gmod-holylib#gameevent)  
\- [threadpoolfix](https://github.com/RaphaelIT7/gmod-holylib#threadpoolfix)  
\- [precachefix](https://github.com/RaphaelIT7/gmod-holylib#precachefix)  
\- [stringtable](https://github.com/RaphaelIT7/gmod-holylib#stringtable)  
\- \- [INetworkStringTable](https://github.com/RaphaelIT7/gmod-holylib#inetworkstringtable)  
\- [pvs](https://github.com/RaphaelIT7/gmod-holylib#pvs)  
\- [surffix](https://github.com/RaphaelIT7/gmod-holylib#surffix)  
\- [filesystem](https://github.com/RaphaelIT7/gmod-holylib#filesystem)  
\- [util](https://github.com/RaphaelIT7/gmod-holylib#util)  
\- [concommand](https://github.com/RaphaelIT7/gmod-holylib#concommand)  
\- [vprof](https://github.com/RaphaelIT7/gmod-holylib#vprof)  
\- [sourcetv](https://github.com/RaphaelIT7/gmod-holylib#sourcetv)  
\- \- [HLTVClient](https://github.com/RaphaelIT7/gmod-holylib#hltvclient)  
\- [bitbuf](https://github.com/RaphaelIT7/gmod-holylib#bitbuf)  
\- \- [bf_read](https://github.com/RaphaelIT7/gmod-holylib#bf_read)  
\- \- [bf_write](https://github.com/RaphaelIT7/gmod-holylib#bf_write)  
\- [networking](https://github.com/RaphaelIT7/gmod-holylib#networking)  
\- [steamworks](https://github.com/RaphaelIT7/gmod-holylib#steamworks)  
\- [systimer](https://github.com/RaphaelIT7/gmod-holylib#systimer)  
\- [pas](https://github.com/RaphaelIT7/gmod-holylib#pas)  
\- [voicechat](https://github.com/RaphaelIT7/gmod-holylib#voicechat)  
\- \- [VoiceData](https://github.com/RaphaelIT7/gmod-holylib#voicedata)  

[Unfinished Modules](https://github.com/RaphaelIT7/gmod-holylib#unfinished-modules)  
\- [bass](https://github.com/RaphaelIT7/gmod-holylib#bass)  
\- [serverplugins](https://github.com/RaphaelIT7/gmod-holylib#serverplugins)  

[Issues implemented / fixed](https://github.com/RaphaelIT7/gmod-holylib/edit/main/README.md#issues-implemented--fixed)  
[Some things for later](https://github.com/RaphaelIT7/gmod-holylib/edit/main/README.md#some-things-for-later)  

# Possible Issues

## Models have prediction errors/don't load properly
This is most likely cause by the filesystem prediction.  
You can use `holylib_filesystem_showpredictionerrors` to see any predictions that failed.  
You can solve this by setting `holylib_filesystem_predictexistance 0`.  
The convar was disabled by default now because of this.  

# Modules
Each module has its own convar `holylib_enable_[module name]` which allows you to enable/disable specific modules.  
You can add `-holylib_enable_[module name] 0` to the startup to disable modules on startup.  

Every module also has his `holylib_debug_[module name]` version and command line option like above, but not all modules use it.  
The modules that use that convar have it listed in their ConVars chapter.  
All hooks that get called use `hook.Run`. Gmod calls `gamemode.Call`.  

## holylib
This module contains the HolyLib library.   

### Functions

#### bool HolyLib.Reconnect(Player ply)
Player ply - The Player to reconnect.  

Returns `true` if the player was successfully reconnected.  

#### HolyLib.HideServer(bool hide)
bool hide - `true` to hide the server from the serverlist.  
Will just set the `hide_server` convar in the future.  

#### HolyLib.FadeClientVolume(Player ply, number fadePercent, number fadeOutSeconds, number holdTime, number fadeInSeconds)
Fades out the clients volume.  
Internally just runs `soundfade` with the given settings on the client.  

#### HolyLib.ServerExecute()
Forces all queried commands to be executed/processed.  

#### bool HolyLib.IsMapValid(string mapName)
Returns `true` if the given map is valid.  

#### bf_write HolyLib.EntityMessageBegin(Entity ent, bool reliable)
Allows you to create an entity message.  

NOTE: If the `bitbuf` module is disabled, it will throw a lua error!

#### bf_write HolyLib.UserMessageBegin(IRecipientFilter filter, string usermsg)
Allows you to create any registered usermessage.  

NOTE: If the `bitbuf` module is disabled, it will throw a lua error!

#### HolyLib.MessageEnd()
Finishes the active Entity/Usermessage.  
If you don't call this, the message won't be sent! (And the engine might throw a tantrum)

#### HolyLib.BroadcastCustomMessage(number type, string name, bf_write buffer)
Sends a custom net message to all clients.  
This function allows you send any netmessage you want.  
You should know what your doing since this function doesn't validate anything.  
You can find all valid types in the [protocol.h](https://github.com/RaphaelIT7/gmod-holylib/blob/main/source/sourcesdk/protocol.h#L86-L145)  

#### HolyLib.SendCustomMessage(number type, string name, bf_write buffer, Player ply)
Same as BroadcastCustomMessage but it only sends it to the specific player.  

#### HolyLib.InvalidateBoneCache(Entity ent)
Invalidates the bone cache of the given entity.  

> NOTE: Only uses this on Entities that are Animated / Inherit the CBaseAnimating class. Or else it will throw a Lua error.  

#### bool HolyLib.SetSignonState(Player/number ply/userid, number signOnState, number spawnCount)
Sets the SignOnState for the given client.  
Returns `true` on success.  

### Hooks

#### string HolyLib:GetGModTags()
Allows you to override the server tags.  
Return nothing / nil to not override them.  

Example:
```lua
hook.Add("HolyLib:GetGModTags", "Example", function()
	local tags = {
		"gm:sandbox", -- Gamemode name. (Not title!)
		"gmc:sandbox", -- Gamemode category
		"ver:"..VERSION, -- Server version
		"loc:de", -- server location
		"hltv:1", -- mark the server as a hltv server.
		"gmws:123456789", -- Gamemode workshop id (Do collectons work? idk)
	}

	return table.concat(tags, " ")
end)
```

#### HolyLib:PostEntityConstructor(Entity ent, String className)
Called before `CBaseEntity::PostConstructor` is called.  
This should allow you to set the `EFL_SERVER_ONLY` flag properly.  

## gameevent
This module contains additional functions for the gameevent library.  
With the Add/Get/RemoveClient* functions you can control the gameevents that are networked to a client which can be useful.  

### Functions

#### (int or table) gameevent.GetListeners(string name)
string name(optional) - The event to return the count of listeners for.  
If name is not a string, it will return a table containing all events and their listener count:  
```lua
{
	["player_spawn"] = 1
}
```

> NOTE: Can return `nil` on failure.  

#### bool gameevent.RemoveListener(string name)
string name - The event to remove the Lua gameevent listener from.  

Returns `true` if the listener was successfully removed from the given event.

#### table gameevent.GetClientListeners(Player ply or nil)
Returns a table containing all gameevents the given client is listening to.  
If not given a player, it will return a table containing all players and the gameevent they're listening to.  

#### bool gameevent.RemoveClientListener(Player ply, string eventName or nil)
Removes the player from listening to the given gameevent.  
If the given gameevent is `nil` it will remove the player from all gameevents.  

Returns `true` on success.  

#### bool (**Disabled**)gameevent.AddClientListener(Player ply, String eventName)
Adds the given player to listen to the given event.  

Returns `true` on success.  

#### IGameEvent gameevent.Create(String eventName, bool force = false)
Creates the given gameevent.  
Can return `nil` on failure.  

#### bool gameevent.FireEvent(IGameEvent event, bool bDontBroadcast = false)
Fires the given gameevent.  
If `bDontBroadcast` is specified, it won't be networked to players.  

#### bool gameevent.FireClientEvent(IGameEvent event, Player ply)
Fires the given event for only the given player.  

#### IGameEvent gameevent.DuplicateEvent(IGameEvent event)
Duplicates the given event.  

#### gameevent.BlockCreation(string name, bool block)
Blocks/Unblocks the creation of the given gameevent.   

### IGameEvent

#### string IGameEvent:\_\_tostring()
Returns the a formated string.  
Format: `IGameEvent [%s]`  
`%s` -> Gameevent name

#### IGameEvent:\_\_gc()
Deletes the gameevent internally.  

#### bool IGameEvent:IsValid()
Returns `true` if the gameevent is valid.  

#### bool IGameEvent:IsEmpty()
Returns `true` if the gameevent is empty.  

#### bool IGameEvent:IsReliable()
Returns `true` if the gameevent is reliable.  

#### bool IGameEvent:IsLocal()
Returns `true` if the gameevent is only local/serverside.  
This means it won't be networked to players.  

#### string IGameEvent:GetName()
Returns the gameevent name.  

#### bool IGameEvent:GetBool(string key, bool fallback)
Returns the bool for the given key and if not found it returns the fallback.  

#### number IGameEvent:GetInt(string key, number fallback)
Returns the int for the given key and if not found it returns the fallback.  

#### number IGameEvent:GetFloat(string key, number fallback)
Returns the float for the given key and if not found it returns the fallback.  

#### string IGameEvent:GetString(string key, string fallback)
Returns the string for the given key and if not found it returns the fallback.  

#### IGameEvent:SetBool(string key, bool value)
Sets the bool for the given key.  

#### IGameEvent:SetInt(string key, number value)
Sets the int for the given key.  

#### IGameEvent:SetFloat(string key, number value)
Sets the float for the given key.  

#### IGameEvent:SetString(string key, string value)
Sets the string for the given key.  

### Hooks

#### bool HolyLib:PreProcessGameEvent(Player ply, table gameEvents, number plyIndex)
Called when the client sends the gameevent list it wants to listen to.  
Return `true` to stop the engine from future processing this list.  

NOTE: This and the hook below may be called with a NULL player.  
it is caused by the player registering the gameevents before he spawned/got an entity.  
Because of this, the third argument was added.  

#### HolyLib:PostProcessGameEvent(Player ply, table gameEvents, number plyIndex)
Called after the engine processed the received gameevent list.  

### ConVars

#### holylib_gameevent_callhook (default `1`)
If enabled, it will call the gameevent hooks.  

#### holylib_debug_gameevent (default `0`)
My debug stuff :> It'll never be important for you.  

## threadpoolfix
This module modifies `CThreadPool::ExecuteToPriority` to not call `CThreadPool::SuspendExecution` when it doesn't have any jobs.  
This is a huge speed improvement for adding searchpaths / mounting legacy addons.  
> NOTE: This requires the `ghostinj.dll` to be installed!

## precachefix
This module removes the host error when the model or generic precache stringtable overflows.  
Instead it will throw a warning.  

If these stringtables overflow, expect the models that failed to precache to be an error.  

### Hooks

#### HolyLib:OnModelPrecache(string model, number idx)
string model - The model that failed to precache.  
number idx - The index the model was precache in.  

#### number HolyLib:OnModelPrecacheFail(string model)
string model - The model that failed to precache.  
Return a index number to let the engine fallback to that model or return `nil` to just let it become an error model.  

#### HolyLib:OnGenericPrecache(string file, number idx)
string file - The file that failed to precache.  
number idx - The index the file was precache in.  

#### number HolyLib:OnGenericPrecacheFail(string file)
string file - The file that failed to precache.  
Return a index number to let the engine fallback to that generic or return `nil` to just let it be.  
Idk if it's a good Idea to play with it's fallback.  

### ConVars

#### holylib_precache_modelfallback(default `-1`)
The fallback index if a model fails to precache.  
`-1` = Error Model  
`0` = Invisible Model  

#### holylib_precache_genericfallback(default `-1`)
The fallback index if a generic fails to precache.  

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

#### table INetworkStringTable:GetAllStrings() 
Returns a table containing all the strings.  

#### number INetworkStringTable:FindStringIndex(string pStr) 
string pStr - The string to find the index of  

Returns the index of the given string.  

#### INetworkStringTable:DeleteAllStrings(bool nukePrecache = false)
Deletes all strings from the stringtable.  
If `nukePrecache` is `true`, it will remove all precache data for the given stringtable.  

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
> NOTE: This also removes the precache data if you delete something from `modelprecache` or so.  

#### bool INetworkStringTable:IsValid()
Returns `true` if the stringtable is still valid.  

#### INetworkStringTable:SetStringUserData(number index, string data, number length = NULL)
Sets the UserData for the given index.  
`length` is optional and should normally not be used. It could cause crashes is not used correctly!  

How this could be useful:  
```lua
local dataTablePaths = {
	"/lua/", -- The order is also important!
	-- Add paths that are frequently used to the top so there the first one to be checked for a file.  
	-- This will improve the performance because it will reduce the number of paths it checks for a file.

	"/addons/exampleaddon/lua/", -- Each legacy addon that has a lua folder has it's path.  
	"/workshop/lua/",
	"/workshop/gamemode/",
	"/gamemode/",
	"/workshop/gamemodes/base/entities/",
	"/gamemodes/base/entities/",
	"/workshop/gamemodes/sandbox/entities/",
	"/gamemodes/sandbox/entities/",
}

local client_lua_files = stringtable.FindTable("client_lua_files")
client_lua_files:SetStringUserData(0, table.concat(dataTablePaths, ";")) -- Set our new datatable paths.
-- Reducing the amount of paths will improve clientside filesystem performance
-- You can see all datapack paths by running "datapack_paths" clientside or printing it like this:
-- print(client_lua_files:GetStringUserData(0))
-- NOTE: Changes to the datatable path will only affect the lcl search path clientside.
```

#### string, number INetworkStringTable:GetStringUserData(number index)
Returns the userdata of the given index.  
Second return value is the length/size of the data.  

#### INetworkStringTable:SetNumberUserData(number index, number value)
Sets the number as userdata for the given string.  

#### number INetworkStringTable:GetNumberUserData(number index)
Returns the userdata of the given index. It needs to be a int!  

#### INetworkStringTable:SetPrecacheUserData(number index, number value)
Sets the `CPrecacheUserData` and sets the flags of it to the given value.  

#### number INetworkStringTable:GetPrecacheUserData(number index)
Returns the flags of `CPrecacheUserData`.  

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

> NOTE: Only use this function inside the `HolyLib:PostCheckTransmit` hook!  

#### pvs.RemoveAllEntityFromTransmit()
Removes all Entities from being transmitted.  

> NOTE: Only use this function inside the `HolyLib:PostCheckTransmit` hook!  

#### pvs.AddEntityToTransmit(Entity ent or table ents, bool always)
table ents - A sequential table containing all the entities that should be transmitted.  
bool always - If the entity should always be transmitted? (Verify)  

Adds the given Entity to be transmitted.

> NOTE: Only use this function inside the `HolyLib:PostCheckTransmit` hook!  

#### (REMOVED AGAIN) pvs.IsEmptyBaseline()
Returns `true` if the baseline is empty.  
This should always be the case after a full update.  

> NOTE: Only use this function inside the `HolyLib:PostCheckTransmit` hook!  
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

#### HolyLib:PostCheckTransmit(Entity ply, table entities)
entity ply - The player that everything is transmitted to.  
table enitites - The Entities that get transmitted. Only available if `holylib_pvs_postchecktransmit` is set to `2` or higher.  

> NOTE: This hook is only called when `holylib_pvs_postchecktransmit` is enabled!

### ConVars

#### holylib_pvs_postchecktransmit (default `0`)
If enabled, it will add/call the `HolyLib:PostCheckTransmit` hook.  
If set to `2` it will also pass a table containing all entitites to the hook (The second argument)  

## surffix
This module ports over [Momentum Mod's](https://github.com/momentum-mod/game/blob/develop/mp/src/game/shared/momentum/mom_gamemovement.cpp#L2393-L2993) surf fixes.  

### ConVars

#### sv_ramp_fix (default `1`)
If enabled, it will enable additional checks to make sure that the player is not stuck in a ramp.  

#### sv_ramp_initial_retrace_length (default `0.2`, max `5`)

#### sv_ramp_bumpcount (default `8`, max `32`)

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

#### holylib_filesystem_predictpath (default `1`)
If enabled, it will try to predict the path for a file.  

Example:  
Your loading a model.  
First you load the `example.mdl` file.  
Then you load the `example.phy` file.   
Here we can check if the `example.mdl` file is in the searchcache.  
If so, we try to use the searchpath of that file for the `.phy` file and since all model files should be in the same folder, this will work for most cases.  
If we fail to predict a path, it will end up using one additional search path.  

#### holylib_filesystem_predictexistance (default `0`)
If enabled, it will try to predict the path of a file, but if the file doesn't exist in the predicted path, we'll just say it doesn't exist.  
Doesn't rely on `holylib_filesystem_predictpath` but it also works with it together.  

The logic for prediction is exactly the same as `holylib_filesystem_predictpath` but it will just stop checking if it doesn't find a file in the predicted path instead of checking then all other searchpaths.  

#### holylib_filesystem_fixgmodpath (default `1`)
If enabled, it will fix up weird gamemode paths like sandbox/gamemode/sandbox/gamemode which gmod likes to use.  
Currently it fixes these paths:  
- `[Active gamemode]/gamemode/[anything]/[active gamemode]/gamemode/` -> (Example: `sandbox/gamemode/spawnmenu/sandbox/gamemode/spawnmenu/`)  
- `include/include/`  

#### (EXPERIMENTAL) holylib_filesystem_cachefilehandle (default `0`)
If enabled, it will cache the file handle and return it if needed.  
> NOTE: This will probably cause issues if you open the same file multiple times.  
> WARNING: This is a noticeable improvement, but it seems to break .bsp files :/  

#### holylib_debug_filesystem (default `0`)
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

#### holylib_filesystem_showpredictionerrors
Shows all files that were predicted to not exist.  

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

#### number filesystem.Size(string fileName, string gamePath = "GAME")
Returns the size of the given file.  

#### number filesystem.Time(string fileName, string gamePath = "GAME")
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

#### number filesystem.TimeCreated(string fileName, string gamePath = "GAME")
Returns the time the given file was created.  
Will return `nil` if the file wasn't found.  

#### number filesystem.TimeAccessed(string fileName, string gamePath = "GAME")
Returns the time the given file was last accessed.  
Will return `nil` if the file wasn't found.  

## util
This module adds two new functions to the `util` library.  

### Functions

#### util.AsyncCompress(string data, number level = 5, number dictSize = 65536, function callback)
Works like util.Compress but it's async and allows you to set the level and dictSize.  
The defaults for level and dictSize are the same as gmod's util.Compress.  

Instead of making a copy of the data, we keep a reference to it and use it raw!  
So please don't modify it while were compressing / decompressing it or else something might break.  

#### util.AsyncCompress(string data, function callback)
Same as above, but uses the default values for level and dictSize.  

#### util.AsyncDecompress(string data, function callback)
Works like util.Decompress but it's async.  

#### string util.FancyTableToJSON(table tbl, bool pretty, bool ignorecycle)
ignorecycle - If `true` it won't throw a lua error when you have a table that is recursive/cycle.  

Convers the given table to json.  
Unlike Gmod's version, this function will turn the numbers to an integer if they are one/fit one.  

## ConVars

### holylib_util_compressthreads(default `1`)
The number of threads to use for `util.AsyncCompress`.  
When changing it, it will wait for all queried jobs to first finish before changing the size.  

### holylib_util_decompressthreads(default `1`)
The number of threads to use for `util.AsyncDecompress`.  

> NOTE: Decompressing seems to be far faster than compressing so it doesn't need as many threads.  

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

> NOTE: The lua library is named `cvar` because the `cvars` library is fully declared in Lua and were running before it even exists.   

#### Functions

##### table cvar.GetAll()
Returns a sequential table with all ConVar's that exist.  

##### bool cvar.SetValue(string name, string value)
Set the convat to the given value.
Returns `true` on success.

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

#### sourcetv.FireEvent(IGameEvent event)
Fires the gameevent for all hltv clients / broadcasts it.  

### HLTVClient
This is a metatable that is pushed by this module. It contains the functions listed below  

#### string HLTVClient:\_\_tostring()
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

#### bool (Experimental) HLTVClient:SendLua(string code)
Sends the given code to the client to be executed.  
Returns `true` on success.  

NOTE: This function was readded back experimentally. It wasn't tested yet. It's still broken but doesn't crash  

#### HLTVClient:FireEvent(IGameEvent event)  
Fires/sends the gameevent to this specific client.  

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
This module adds a `bf_read` and `bf_write` class.  

### Functions

#### bf_read bitbuf.CopyReadBuffer(bf_read buffer)
Copies the given buffer into a new one.  
Useful if you want to save the data received by a client.  

#### bf_read bibuf.CreateReadBuffer(string data)
Creates a read buffer from the given data.  
Useful if you want to read the userdata of the instancebaseline stringtable.  

#### bf_write bitbuf.CreateWriteBuffer(number size or string data)
Create a write buffer with the given size or with the given data.  

### bf_read
This class will later be used to read net messages from HLTV clients.  
> ToDo: Finish the documentation below and make it more detailed.  

#### string bf_read:\_\_tostring()
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

> NOTE: This is only available for the 32x!    

#### bool bf_read:IsOverflowed()
Returns `true` if the buffer is overflowed.  

#### number bf_read:PeekUBitLong(number numBits)

#### number bf_read:ReadBitAngle(number numBits)

#### angle bf_read:ReadBitAngles()
Reads and Angle.  

#### number bf_read:ReadBitCoord()

> NOTE: This is only available for the 32x!    

#### number bf_read:ReadBitCoordBits()

> NOTE: This is only available for the 32x!    

#### number bf_read:ReadBitCoordMP(bool bIntegral, bool bLowPrecision)

> NOTE: This is only available for the 32x!    

#### number bf_read:ReadBitCoordMPBits(bool bIntegral, bool bLowPrecision)

> NOTE: This is only available for the 32x!    

#### number bf_read:ReadBitFloat()

#### number bf_read:ReadBitLong(number numBits, bool bSigned)
Reads a number with the given number of bits.  

> NOTE: This is only available for the 32x!    

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

#### bf_read:Reset()
Resets the current position and resets the overflow flag.  

#### bool bf_read:Seek(number iPos)
Sets the current position to the given position.  
Returns `true` on success.  

#### bool bf_read:SeekRelative(number iPos)
Sets the current position to the given position relative to the current position.
Basicly `newPosition = currentPosition + iPos`    
Returns `true` on success.  

### bf_write  

#### string bf_write:\_\_tostring()
Returns the a formated string.  
Format: `bf_write [%i]`  
`%i` -> size of data in bits.  

#### bf_write:\_\_gc()
Deletes the buffer internally.  

#### bool bf_write:IsValid()
returns `true` if the buffer is still valid.  

#### string bf_write:GetData()
Returns the written data.  

#### number bf_write:GetNumBytesWritten()
Returns the number of bytes written.  

#### number bf_write:GetNumBytesLeft()  
Returns the number of bytes that are unwritten.  

#### number bf_write:GetNumBitsWritten()
Returns the number of bits written.  

#### number bf_write:GetNumBitsLeft()
Returns the number of bits left.  

#### number bf_write:GetMaxNumBits()
Returns the maximum number of bits that can be written.  
#### bool bf_write:IsOverflowed()
Returns `true` if the buffer is overflowed.  

#### bf_write:Reset()
Resets the buffer.  

#### string bf_write:GetDebugName()
Returns the debug name.  

#### bf_write:SetDebugName(string debugName)
Sets the debug name.  
You should keep a reference to the string.  

#### bf_write:SeekToBit(number bit)
Seeks to the given bit.  

#### bf_write:WriteFloat(number value)
Writes a float.  

#### bf_write:WriteChar(number value)
Writes a char.  

#### bf_write:WriteByte(number value)
Writes a byte.  

#### bf_write:WriteLong(number value)
Writes a long.  

#### bf_write:WriteLongLong(number value)
Writes a long long.  

#### bf_write:WriteBytes(string data)
Writes the given bytes to the buffer.  

#### bf_write:WriteOneBit(bool value)
Writes one bit.  

#### bf_write:WriteOneBitAt(number bit, bool value)
Sets the given bit to the given value.  

#### bf_write:WriteShort(number value)
Writes a short.  

#### bf_write:WriteWord(number value)
Writes a word.  

#### bf_write:WriteSignedVarInt32(number value)

#### bf_write:WriteSignedVarInt64(number value)

#### bf_write:WriteVarInt32(number value)

#### bf_write:WriteVarInt64(number value)

#### bf_write:WriteUBitVar(number value)

#### bf_write:WriteUBitLong(number value, number bits, bool signed)

#### bf_write:WriteBitAngle(number value, number bits)

#### bf_write:WriteBitAngles(Angle ang)
Writes an Angle. (`QAngle` internally).  

#### bf_write:WriteBitVec3Coord(Vector vec)
Writes a Vector.  

#### bf_write:WriteBitVec3Normal(Vector vec)

#### bool bf_write:WriteBits(string data, number bits)
Writes the given number of bits from the data into this buffer.  
Returns `true` on success.  

#### bool bf_write:WriteBitsFromBuffer(bf_read buffer, number bits)
Writes the given number of bits from the given buffer into this buffer.  
Returns `true` on success.  

> NOTE: The current position for the given buffer will change as we internally read from it!

#### bf_write:WriteBitNormal(number value)

#### bf_write:WriteBitLong(number value)

#### bf_write:WriteBitFloat(number value)

#### bf_write:WriteBitCoord(number value)

#### bf_write:WriteBitCoordMP(number value, bool bIntegral, bool bLowPrecision)

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

#### steamworks.ForceActivate()
Calls the `SV_InitGameServerSteam` function to activate the steam server exactly like the engine does it.  

#### bool steamworks.ForceAuthenticate(number userID)
Marks the given client as Authenticated even if they aren't.  

### Hooks

#### HolyLib:OnSteamDisconnect(number result)  
Called when our Steam server loses connection to steams servers.  

#### HolyLib:OnSteamConnect()  
Called when our Steam server successfully connected to steams servers.  

## systimer
This module is just the timer library, but it doesn't use CurTime.  
> NOTE: Timer repetitions are limited to an unsigned int.  

### Functions

#### bool systimer.Adjust(string name, number delay, number reps = nil, function callback = nil)  
Adjusts the timer with the given name with the new values.  
Returns `true` if the timer was successfully adjusted.  

#### systimer.Check()  
Does nothing / deprecated.  

#### systimer.Create(string name, number delay, number reps, function callback)  
Creates a timer with the given name.  

#### bool systimer.Exists(string name)  
Returns `true` if the given timer exists.  

#### systimer.Remove(string name)  
Removes the given timer.  

#### number systimer.RepsLeft(string name)  
Returns the number of reps left for the given timer.  
Returns `0` if the timer wasn't found or the reps are below `0`.  

#### systimer.Simple(number delay, function callback)  
Creates a simple timer.  

#### bool systimer.Start(string name)  
Returns `true` if the given timer was successfully started again.  

#### bool systimer.Stop(string name)  
Returns `true` if the given timer was successfully stopped.  

#### number systimer.TimeLeft(string name)  
Returns the time left until the given timer is executed again.  
Returns `0` if the timer wasn't found.  

#### bool systimer.Toggle(string name)  
Toggles the given timer.  
Returns `true` if the timer was activated/started again.  

#### bool systimer.Pause(string name)  
Returns `true` if the given timer was successfully paused.  

#### bool systimer.UnPause(string name)  
Unpauses the given timer.  
Unlike systimer.Start this won't reset the time left until it executes again.  

## pas
This module plans to add a few PAS related functions like `table pas.FindInPAS(Vector pos or Entity ent)`.  
If you got an Idea for a function to add, feel free to comment it into [its issue](https://github.com/RaphaelIT7/gmod-holylib/issues/1).

### Functions

#### table pas.FindInPAS(Entity/Vector vec)
Returns a sequential table containing all entities in that PAS.  

#### bool pas.TestPAS(Entity/Vector pas, Entity/Vector hearPos)
Tests if the give hear position is inside the given pas.  
Returns `true` if it is.  

#### bool pas.CheckBoxInPAS(Vector mins, Vector maxs, Vector pas)
Checks if the given pox is inside the PAS.  
Returns `true` if it is.  

## voicechat
This module contains a few things to control the voicechat.  
You could probably completly rework the voicechat with this since you could completly handle it in lua.  

### Functions

#### voicechat.SendEmptyData(Player ply, number playerSlot = ply:EntIndex()-1)
Sends an empty voice packet.  
If `playerSlot` isn't specified it will use the player slot of the given player.  

#### voicechat.SendVoiceData(Player ply, VoiceData data)
Sends the given VoiceData to the given player.  
It won't do any processing, it will just send it as it is.  

#### voicechat.BroadcastVoiceData(VoiceData data, table plys = nil)
Same as `voicechat.SendVoiceData` but broadcasts it to all players.  
If given a table it will only send it to thoes players.  

#### voicechat.ProcessVoiceData(Player ply, VoiceData data)
Let's the server process the VoiceData like it was received from the client.  
This can be a bit performance intense.  
NOTE: This will ignore the set player slot!  

#### bool voicechat.IsHearingClient(Player ply, Player targetPly)
Returns `true` if `ply` can hear the `targetPly`.  

#### bool voicechat.IsProximityHearingClient(Player ply, Player targetPly)
Returns `true` if `ply` can hear the `targetPly` in it's proximity.  

#### VoiceData voicedata.CreateVoiceData(number playerSlot = 0, string data = NULL, number dataLength = NULL)
Creates a new VoiceData object.  
> NOTE: All Arguments are optional!

### VoiceData
VoiceData is a userdata value that is used to manage the voicedata.  

#### string VoiceData:\_\_tostring()
Returns `VoiceData [%i(Player Slot)][%i(Length/Size)]`.  

#### VoiceData:\_\_gc()
Garbage collection. Deletes the voice data internally.  

#### var VoiceData:\_\_index()
Index.  

#### bool VoiceData:IsValid()
Returns `true` if the VoiceData is still valid.  

#### string VoiceData:GetData()
Returns the raw compressed voice data.  

#### number VoiceData:GetLength()
Returns the length of the data.  

#### number VoiceData:GetPlayerSlot()
Returns the slot of the player this voicedata is originally from.  

#### string VoiceData:GetUncompressedData(number decompressSize = 20000)
number decompressSize - The number of bytes to allocate for decompression.  

Returns the uncompressed voice data.  

#### bool VoiceData:GetProximity()
Returns if the VoiceData is in proximity.  

#### VoiceData:SetData(string data, number length = nil)
Sets the new voice data.  

#### VoiceData:SetLength(number length)
Sets the new length of the data.  

#### VoiceData:SetPlayerSlot(number slot)
Sets the new player slot.  

#### VoiceData:SetProximity(bool bProximity)
Sets if the VoiceData is in proximity.  

### Hooks

#### HolyLib:PreProcessVoiceChat(Player ply, VoiceData data)
Called before the voicedata is processed.  
Return `true` to stop the engine from processing it.  

Example to record and play back voices.  
```lua
concommand.Add("record_me", function()
	hook.Remove("Think", "VoiceChat_Example") -- Doesn't like to play back while recording :^
	voiceTbl = {}
	voiceStartTick=engine.TickCount()
	hook.Add("HolyLib:PreProcessVoiceChat", "VoiceChat_Example", function(ply, voiceData)
		if !voiceTbl[ply] then
			voiceTbl[ply] = {}
		end

		voiceTbl[ply][engine.TickCount() - voiceStartTick] = voiceData 
		-- We save the tick delays since the voice data isn't sent every frame and has random delays.
	end)
end)

concommand.Add("stop_record", function()
	hook.Remove("HolyLib:PreProcessVoiceChat", "VoiceChat_Example")
end)

concommand.Add("play_me", function(ply)
	if !voiceTbl[ply] then
		ply:ChatPrint("You first need to record.")
		return
	end

	if !player.GetBots()[1] then
		RunConsoleCommand("bot")
	end

	hook.Remove("HolyLib:PreProcessVoiceChat", "VoiceChat_Example")

	voiceIdx = 0
	hook.Add("Think", "VoiceChat_Example", function()
		if !IsValid(ply) then
			hook.Remove("Think", "VoiceChat_Example")
			return
		end

		voiceIdx = voiceIdx + 1 
		local voiceData = voiceTbl[ply][voiceIdx]
		-- We play it back in the exact same tick delays we received it in to not speed it up affect it in any way.
		if voiceData then 
			voicechat.ProcessVoiceData(player.GetBots()[1], voiceData)
		end 
	end)
end)
```

The Engine's voicechat but in Lua:
```lua
hook.Add("HolyLib:PreProcessVoiceChat", "VoiceCHat_Engine", function(pClient, voiceData)
	if GetConVar("voice_debugfeedbackfrom"):GetBool() then
		print(string.format("Sending voice from: %s - playerslot: %d", pClient:Name(), pClient:EntIndex()))
	end

	local iLength = voiceData:GetLength()
	for _, pDestClient in player.Iterator() do
		local bSelf = pDestClient == pClient
		local bHearsPlayer = voicechat.IsHearingClient(pDestClient, pClient)

		voiceData:SetProximity(voicechat.IsProximityHearingClient(pDestClient, pClient))

		if !bHearsPlayer && !bSelf then continue end

		voiceData:SetLength(iLength)

		if !bHearsPlayer then
			-- Still send something, just zero length (this is so the client
			-- can display something that shows knows the server knows it's talking).
			voiceData:SetLength(0)
		end

		voicechat.SendVoiceData(pDestClient, voiceData)
	end

	return true -- Stop Engine handling since we fully handled it
end)
```

### ConVars

#### holylib_voicechat_hooks(default `1`)
If enabled, the VoiceChat hooks will be called.  

# Unfinished Modules

## bass
This module will add functions related to the bass dll.  
Does someone have the bass libs for `2.4.15`? I can't find them :<  

### Functions

#### bass.PlayFile(string filePath, string flags, function callback)
callback - function(IGMODAudioChannel channel, int errorCode, string error)  
Creates a IGMODAudioChannel for the given file.  

#### bass.PlayURL(string URL, string flags, function callback)
callback - function(IGMODAudioChannel channel, int errorCode, string error)  
Creates a IGMODAudioChannel for the given url.  

## serverplugins
This module adds two new `IServerPluginCallbacks` functions:  
`virtual void OnLuaInit( GarrysMod::Lua::ILuaInterface* LUA )`  
`virtual void OnLuaShutdown( GarrysMod::Lua::ILuaInterface* LUA )`  

For a plugin to be loaded by HolyLib, you need to expose HolyLib's Interface.  
You can expose both HolyLib's and the normal interface since the order of the virtual functions are the same.  
This should already work but I never tested it completly yet (Or I forgot that I did test it).  

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
`cvar.GetAll` -> https://github.com/Facepunch/garrysmod-requests/issues/341  
`sourcetv` module -> https://github.com/Facepunch/garrysmod-requests/issues/2298  
`bitbuf` module(unfinished) -> https://github.com/Facepunch/garrysmod-requests/issues/594  
`HLTV` class / `sourcetv` module -> https://github.com/Facepunch/garrysmod-requests/issues/2237  
`surffix` module -> https://github.com/Facepunch/garrysmod-requests/issues/2306
`filesystem.TimeCreated` & `filesystem.TimeAccessed` -> https://github.com/Facepunch/garrysmod-requests/issues/1633  
`systimer` module -> https://github.com/Facepunch/garrysmod-requests/issues/1671  
`pas` module -> https://github.com/Facepunch/garrysmod-requests/issues/140  
`HolyLib.InvalidateBoneCache` -> https://github.com/Facepunch/garrysmod-requests/issues/1920  
`HolyLib:PostEntityConstructor` -> https://github.com/Facepunch/garrysmod-requests/issues/2440  

# Things planned to add:
https://github.com/Facepunch/garrysmod-requests/issues/1884  
(Maybe)https://github.com/Facepunch/garrysmod-requests/issues/132  
https://github.com/Facepunch/garrysmod-requests/issues/77  
(Maybe)https://github.com/Facepunch/garrysmod-requests/issues/603  
https://github.com/Facepunch/garrysmod-requests/issues/439  
https://github.com/Facepunch/garrysmod-requests/issues/1323  
(Should be possible?)https://github.com/Facepunch/garrysmod-requests/issues/756  
(Gonna make a seperate ConVar for it)https://github.com/Facepunch/garrysmod-requests/issues/2120  
https://github.com/Facepunch/garrysmod-requests/issues/1472  
(Maybe)https://github.com/Facepunch/garrysmod-requests/issues/2129  
(Maybe)https://github.com/Facepunch/garrysmod-requests/issues/1962  
(Maybe)https://github.com/Facepunch/garrysmod-requests/issues/1699    

# Some things for later

## NW Networking  
NW uses a usermessage `NetworkedVar`  
Write order:  
- WriteLong -> Entity handle  
- Char -> Type ID  
- String -> Var name (Planned to change to 12bits key index in next net compact)  
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