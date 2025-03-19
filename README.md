# Holylib

A library that contains some functions and optimizations for gmod.  
If you need any function, make an issue for it, and I'll look into it.  

## Windows
So currently to get it working on Windows, I would have to redo most of the hooks, and It would also take a good while.  
Because of this, I'm not going to make it currently. I'm gonna slowly start adding all symbols and then someday I'm going to redo most hooks.  
There will be a release but it will only contain some parts since many things don't work yet.  

> [!NOTE]
> I'm not actively testing windows, so if I accidentally broke it, open a issue since I most likely didn't know about it.  

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

## How to update (Newer GhostInj)

> [!NOTE]
> Don't try to use plugin_unload since it might not work because of things like the networking module which currently can't be unloaded.  
> To update holylib you will always need to shutdown the server.  

1. Append `_updated` to the new file to have a result like this `gmsv_holylib_linux_updated.so`.  
2. Upload the file into the `lua/bin` folder
3. Restart the server normally.  
On the next startup the ghostinj will update holylib to use the new file.  
This is done by first deleting the current `gmsv_holylib_linux[64].so` and then renaming the `_updated.so` essentially replacing the original file.

## How to update (Older GhostInj versions)

1. Shutdown the server
2. Upload the new file
3. Enjoy it

## What noticable things does HolyLib bring?
\- A huge Lua API providing deep access to the engine.  
\- Implements some bug fixes (some bug fixes were brought into gmod itself).  
\- \- `ShouldCollide` won't break the entire physics engine and instead a warning is thrown `holylib: Someone forgot to call Entity:CollisionRulesChanged!`.  
\- Lua Hooks are shown in vprof results (done in the `vprof` module)  
\- Ported a networking improvement over from [sigsegv-mvm](https://github.com/rafradek/sigsegv-mvm/blob/910b92456c7578a3eb5dff2a7e7bf4bc906677f7/src/mod/perf/sendprop_optimize.cpp#L35-L144) improving performance & memory usage (done in the `networking` module)  
\- Ported [Momentum Mod](https://github.com/momentum-mod)'s surfing improvements (done in the `surffix` module)  
\- Heavily improved the filesystem (done in the `filesystem` module)  
\- Greatly improved Gmod's `GMod::Util::IsPhysicsObjectValid` function improving performance especially when many physics objects exist (done in the `physenv` module)  
\- (Disabled by default) Updated LuaJIT version (done in the `luajit` module)  
\- Improved ConVar's find code improving performance (done in the `cvars` module)  

## Next Update
\- [+] Added (Experimentally)`luajit` module.  
\- [+] Added `httpserver` module.  
\- [+] Added `entitylist` module.  
\- \- [+] Many `pvs.*` function accept now a `EntityList`.  
\- \- [+] `pas.TestPAS` accepts a `EntityList`.  
\- \- [#] Improved `pas.FindInPAS` performance by using it internally if it's enabled.  
\- [+] Added many things to `physenv` module.  
\- \- [+] Added `physcollide` library.  
\- \- [+] Added more functions to `physenv` to create physic environments.  
\- \- [+] Added `HolyLib:OnPhysFrame` hook.  
\- [+] Added `gameserver` module.  
\- [+] Added the `HolyLib:PreCheckTransmit`, `HolyLib:OnPlayerGot[On/Off]Ladder`, `HolyLib:OnMoveTypeChange` hook.  
\- [+] Added `HolyLib:OnSourceTVStartNewShot`, `HolyLib:OnSourceTVClientDisconnect` hook to `sourcetv` module.  
\- [+] Added `CHLTVClient:SetCameraMan` and `sourcetv.SetCameraMan` to `sourcetv` module.  
\- [+] Added `INetworkStringTable:GetTable`, `CHLTVClient:GetTable`, `VoiceData:GetTable`, `IGameEvent:GetTable`, `bf_read:GetTable`, `bf_write:GetTable`, `IGModAudioChannel:GetTable` functions.  
\- [+] Added `pvs.TestPVS`, `pvs.FindInPVS` and `pvs.ForceFullUpdate` functions to `pvs` module.  
\- [+] Added `HolyLib.GetRegistry`, `HolyLib.ExitLadder`, `HolyLib.GetLadder` and `HolyLib.Disconnect` to `holylib` module.  
\- [+] Added `cvar.Find` to `cvars` module.  
\- [+] Exposed `IHolyUtil` interface and added `IHolyLib::PreLoad` and `IHolyLib:GetHolyUtil`.  
\- [+] Added (Experimental)`holylib_filesystem_savesearchcache` optimization to filesystem module.  
\- [+] Added Windows support for `bitbuf`, `cvars`, (partially)`filesystem`, `pas`, `util`, `voicechat` and (partially)`vprof`  
\- [+] Added `lua/autorun/_holylib` folder.  
\- \- Files inside that folder are loaded and executed before **any** gmod script runs, only the c++ functions exist at this point.  
\- [+] Added `bf_write:WriteString` to `bitbuf` module.  
\- [+] Added `IGModAudioChannel:FFT` to `bass` module.  
\- [+] Added `VoiceStream` class and related functions to `voicechat` module.  
\- [#] Fixed many issues with the `bass` module. It is acutally usable.  
\- [#] Improved performance by replacing SetTable with RawSet.  
\- [#] Added missing calls to the deconstructors for `CHLTVClient` and `CNetworkStringTable`.  
\- \- These missing calls could have caused some bugs or memory leaks.  
\- [#] Fixed a bug with sourcetv where `CHLTVClients` could be NULL while being valid (#15)  
\- [#] Fixed many Windows crashes allowing it to start again.  
\- [#] Fixed Stack leak with `pvs.AddEntityToPVS` and `pvs.SetPreventTransmitBulk` when given a table.  
\- [#] Fixed Stack issue with `pvs.RemoveEntityFromTransmit` when given a EntityList.  
\- [#] Optimized `Util::Push_Entity` and replaced `LUA->Raw[Get/Set]` usage with faster `Util::Raw[GetI/SetI]`.  
\- [#] Optimized `GMod_Util_IsPhysicsObjectValid` to solve a regression introduced by new `physenv` changes.  
\- [#] Small networking optimizations.  
\- [#] Optimized `CCvar::FindVar` (50x faster in average).  
\- [#] Fixed `pvs.RemoveAllEntityFromTransmit` possibly causing stack issues.  
\- [#] Completely changed how we Push/Manage Lua userdata to reduce memory leaks.  
\- [#] Fixed a filesystem issue which could rarely result in a crash. (Issue: #23)  
\- [-] Removed `holylib_filesystem_optimizedfixpath` since it was implemented into gmod itself.  

You can see all changes here:  
https://github.com/RaphaelIT7/gmod-holylib/compare/Release0.6...main

### Existing Lua API Changes
\- [#] Flipped `INetworkStringTable:AddString` arguments.  
\- [#] All `pvs.FL_EDICT_` enums changed.  
\- [#] Made `HolyLib.SetSignOnState` third arg optional and added `rawSet` option.  
\- [#] Renamed `HLTVClient` to `CHLTVClient`.  
\- [#] Renamed `HLTVClient:GetSlot` to `HLTVClient:GetPlayerSlot`.  
\- [#] Renamed `VProfCounter:Name` to `VProfCounter:GetName`.  
\- [#] Renamed `HolyLib:PhysicsLag` to `HolyLib:OnPhysicsLag`.  
\- [#] VoiceData given from `HolyLib:PreProcessVoiceChat` becomes invalid after the hook call, use `VoiceData:CreateCopy()` in case you want to store it.  
\- [-] Removed `HolyLib.BroadcastCustomMessage` (Replaced by `gameserver.BroadcastMessage`)  
\- [-] Removed `HolyLib.SendCustomMessage` (Replaced by `CBaseClient:SendNetMsg`)  
\- [-] Removed `HolyLib:PostCheckTransmit` second argument (Use `pvs.GetEntitesFromTransmit`)  

### QoL updates
\- [+] Added comments to some interfaces.  
\- [+] Added Windows 32 & 64x to workflow build.  
\- [+] Updated ghostinj to allow for easier updating.  
\- [#] Cleaned up code a bit  
\- [#] Switched away from the ILuaBase. All Lua functions now use ILuaInterface.  
\- [#] Fixed `plugin_load` causing holylib to load improperly.  
\- [#] Documented which platforms each module support.  
\- [#] Removed unused convars.  
\- [#] Unregistering the plugin properly so that it can also be loaded afterwards again.  
\- [#] Fixed some memory leaks.  

## ToDo

\- Finish 64x (`pvs`, `sourcetv`, `surffix`)  
\- Find out why ConVars are so broken. (Serverside `path` command breaks :<)  
\- Look into filesystem handle optimization  
\- Try to fix that one complex NW2 bug. NOTE: It seems to be related to baseline updates (Entity Creation/Deletion)  
\- Look into StaticPropMgr stuff. May be interresting.  
\- Add a bind to `CAI_NetworkManager::BuildNetworkGraph` or `StartRebuild`  
\- Possibly allow on to force workshop download on next level change.  
\- GO thru everything and use a more consistant codestyle. I created quiet the mess.  
\- Reduce/Remove the usage of g_Lua since our code should work later with multiple ILuaInterfaces.  
\- test/become compatible with vphysics-jolt (I'm quite sure that the `physenv` isn't compatible).  
\- Check out `holylib_filesystem_predictexistance` as it seamingly broke, reportidly works in `0.6`.  

# New Documentation
Currently I'm working on implementing a better wiki that will replace this huge readme later.  
It may already contain a few more details to some functions than this readme.  
Wiki: https://holylib.raphaelit7.com/

> [!NOTE]
> The Wiki is still in development currently, **but** most/all of the documentation is **already** done.  

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
\- \- [VProfCounter](https://github.com/RaphaelIT7/gmod-holylib#vprofcounter)  
\- \- [VProfNode](https://github.com/RaphaelIT7/gmod-holylib#vprofnode)  
\- [sourcetv](https://github.com/RaphaelIT7/gmod-holylib#sourcetv)  
\- \- [CHLTVClient](https://github.com/RaphaelIT7/gmod-holylib#chltvclient)  
\- [bitbuf](https://github.com/RaphaelIT7/gmod-holylib#bitbuf)  
\- \- [bf_read](https://github.com/RaphaelIT7/gmod-holylib#bf_read)  
\- \- [bf_write](https://github.com/RaphaelIT7/gmod-holylib#bf_write)  
\- [networking](https://github.com/RaphaelIT7/gmod-holylib#networking)  
\- [steamworks](https://github.com/RaphaelIT7/gmod-holylib#steamworks)  
\- [systimer](https://github.com/RaphaelIT7/gmod-holylib#systimer)  
\- [pas](https://github.com/RaphaelIT7/gmod-holylib#pas)  
\- [voicechat](https://github.com/RaphaelIT7/gmod-holylib#voicechat)  
\- \- [VoiceData](https://github.com/RaphaelIT7/gmod-holylib#voicedata)  
\- [physenv](https://github.com/RaphaelIT7/gmod-holylib#physenv)  
\- \- [CPhysCollide](https://github.com/RaphaelIT7/gmod-holylib#cphyscollide)  
\- \- [CPhysPolySoup](https://github.com/RaphaelIT7/gmod-holylib#cphyspolysoup)  
\- \- [CPhysConvex](https://github.com/RaphaelIT7/gmod-holylib#cphysconvex)  
\- \- [IPhysicsCollisionSet](https://github.com/RaphaelIT7/gmod-holylib#iphysicscollisionset)  
\- \- [IPhysicsEnvironment](https://github.com/RaphaelIT7/gmod-holylib#iphysicsenvironment)  
\- [bass](https://github.com/RaphaelIT7/gmod-holylib#bass)  
\- \- [IGModAudioChannel](https://github.com/RaphaelIT7/gmod-holylib#igmodaudiochannel)  
\- [entitylist](https://github.com/RaphaelIT7/gmod-holylib#entitylist)  
\- [httpserver](https://github.com/RaphaelIT7/gmod-holylib#httpserver)  
\- \- [HttpServer](https://github.com/RaphaelIT7/gmod-holylib#httpserver)  
\- \- [HttpRequest](https://github.com/RaphaelIT7/gmod-holylib#httprequest)  
\- \- [HttpResponse](https://github.com/RaphaelIT7/gmod-holylib#httpresponse)  
\- [luajit](https://github.com/RaphaelIT7/gmod-holylib#luajit)  
\- [gameserver](https://github.com/RaphaelIT7/gmod-holylib#gameserver)  
\- \- [CBaseClient](https://github.com/RaphaelIT7/gmod-holylib#cbaseclient)  
\- \- [CGameClient](https://github.com/RaphaelIT7/gmod-holylib#cgameclient)  
\- \- [Singleplayer](https://github.com/RaphaelIT7/gmod-holylib#singleplayer)  
\- \- [128+ Players](https://github.com/RaphaelIT7/gmod-holylib#128-players)  

[Unfinished Modules](https://github.com/RaphaelIT7/gmod-holylib#unfinished-modules)  
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

## Debug Stuff
There is `-holylib_startdisabled` which will cause all modules to be disabled on startup.  
And with `holylib_toggledetour` you can block specific detours from being created.  

## holylib
This module contains the HolyLib library.   

Supports: Linux32 | LINUX64  

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
A direct engine bind to `IVEngineServer::FadeClientVolume`  

#### HolyLib.ServerExecute()
Forces all queried commands to be executed/processed.  
A direct engine bind to `IVEngineServer::ServerExecute`  

#### bool HolyLib.IsMapValid(string mapName)
Returns `true` if the given map is valid.  

#### bf_write HolyLib.EntityMessageBegin(Entity ent, bool reliable)
Allows you to create an entity message.  
A direct engine bind to `IVEngineServer::EntityMessageBegin`  

> [!NOTE]
> If the `bitbuf` module is disabled, it will throw a lua error!

#### bf_write HolyLib.UserMessageBegin(IRecipientFilter filter, string usermsg)
Allows you to create any registered usermessage.  
A direct engine bind to `IVEngineServer::UserMessageBegin`  

> [!NOTE]
> If the `bitbuf` module is disabled, it will throw a lua error!

#### HolyLib.MessageEnd()
Finishes the active Entity/Usermessage.  
If you don't call this, the message won't be sent! (And the engine might throw a tantrum)
A direct engine bind to `IVEngineServer::MessageEnd`  

#### HolyLib.InvalidateBoneCache(Entity ent)
Invalidates the bone cache of the given entity.  

> [!NOTE]
> Only uses this on Entities that are Animated / Inherit the CBaseAnimating class. Or else it will throw a Lua error.  

#### bool HolyLib.SetSignOnState(Player ply / number userid, number signOnState, number spawnCount = 0, bool rawSet = false)
Sets the SignOnState for the given client.  
Returns `true` on success.  

> [!NOTE]
> This function does normally **not** directly set the SignOnState.  
> Instead it calls the responsible function for the given SignOnState like for `SIGNONSTATE_PRESPAWN` it will call `SpawnPlayer` on the client.  
> Set the `rawSet` to `true` if you want to **directly** set the SignOnState.    

#### (Experimental - 32x safe only) HolyLib.ExitLadder(Player ply)
Forces the player off the ladder.  

#### (Experimental - 32x safe only) Entity HolyLib.GetLadder(Player ply)
Returns the Ladder the player is currently on.  

#### table HolyLib.GetRegistry()
Returns the lua regirsty.  
Same like [debug.getregistry()](https://wiki.facepunch.com/gmod/debug.getregistry) before it was nuked.  

#### bool HolyLib.Disconnect(Player ply / number userid, string reason, bool silent = false, bool nogameevent = false)
silent - Silently closes the net channel and sends no disconnect to the client.  
nogameevent - Stops the `player_disconnect` event from being created.  
Disconnects the given player from the server.  

> [!NOTE]
> Unlike Gmod's version which internally calls the `kickid` command, we directly call the `Disconnect` function with no delay.  

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

> [!NOTE]
> This may currently not work.

#### HolyLib:OnPlayerGotOnLadder(Entity ladder, Entity ply)
Called when a gets onto a ladder -> Direct bind to `CFuncLadder::PlayerGotOn`  

#### HolyLib:OnPlayerGotOffLadder(Entity ladder, Entity ply)
Called when a gets off a ladder -> Direct bind to `CFuncLadder::PlayerGotOff`  

#### HolyLib:OnMoveTypeChange(Entity ent, number old_moveType, number new_moveType, number moveCollide)
Called when the movetype is about to change.  
If you call `Entity:SetMoveType` on the current entity inside this hook, it would have no effect.  

## gameevent
This module contains additional functions for the gameevent library.  
With the Add/Get/RemoveClient* functions you can control the gameevents that are networked to a client which can be useful.  

Supports: Linux32 | LINUX64  

### Functions

#### (number or table) gameevent.GetListeners(string name)
string name(optional) - The event to return the count of listeners for.  
If name is not a string, it will return a table containing all events and their listener count:  
```lua
{
	["player_spawn"] = 1
}
```

> [!NOTE]
> Can return `nil` on failure.  

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

> [!NOTE]
> This and the hook below may be called with a NULL player.  
> it is caused by the player registering the gameevents before he spawned/got an entity.  
> Because of this, the third argument was added.  

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
> [!NOTE]
> This requires the `ghostinj.dll` to be installed!

Supports: Linux32 | LINUX64  

> [!NOTE]
> This currently does nothing since all fixes were added to Garry's Mod itself.  

## precachefix
This module removes the host error when the model or generic precache stringtable overflows.  
Instead it will throw a warning.  

If these stringtables overflow, expect the models that failed to precache to be an error.  

Supports: Linux32 | LINUX64  

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

Supports: Linux32 | LINUX64  

> [!NOTE]
> For now, all functions below are just a bind to their C++ versions -> [INetworkStringTable](https://github.com/RaphaelIT7/obsolete-source-engine/blob/gmod/public/networkstringtabledefs.h#L32)  

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

#### bool stringtable.IsLocked()
Returns if the stringtable is locked or not.  

#### stringtable.AllowCreation(bool bAllowCreation)
Sets whether you're allowed to create stringtable.  

Example:  
```lua
stringtable.AllowCreation(true)
stringtable.CreateStringTable("example", 8192)
stringtable.AllowCreation(false)
```

> [!NOTE]
> Please use the `HolyLib:OnStringTableCreation` hook to add custom stringtables.  

#### stringtable.RemoveTable(INetworkStringTable table)
Deletes that specific stringtable.  

> [!WARNING]
> Deleting a stringtable is **NOT** networked!  
> In addition, it might be unsafe and result in crashes when it's a engine created stringtable.  
> Generally, only use it when you **KNOW** what your doing.  

### INetworkStringTable
This is a metatable that is pushed by this module. It contains the functions listed below  

#### string INetworkStringTable:\_\_tostring()
Returns `INetworkStringTable [NULL]` if given a invalid table.  
Normally returns `INetworkStringTable [tableName]`.  

#### INetworkStringTable:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any INetworkStringTable:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `INetworkStringTable:GetTable()`.  

#### table INetworkStringTable:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

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

The current tick to used for networking

#### bool INetworkStringTable:ChangedSinceTick(number tick) 
number tick - The tick to set the stringtable to  

Returns whether or not the stringtable changed since that tick.  

#### number INetworkStringTable:AddString(string value, bool isServer = true) 
string value - The string to add  
bool isServer - Weather or not the server is adding this value? (Idk, added it so you have more control.)  

Returns the index of the added string.  

#### string INetworkStringTable:GetString(number index) 
number index - The index to get the string from  

Returns the string from that index  

#### table INetworkStringTable:GetAllStrings() 
Returns a table containing all the strings.  

#### number INetworkStringTable:FindStringIndex(string value) 
string value - The string to find the index of  

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

> [!NOTE]
> If there are already more entries than the new limit, they won't be removed.  
> (This could cause issues, so make sure you know what you're doing.)  

#### bool INetworkStringTable:DeleteString(number index)
Deletes the given string at the given index.  

Returns `true` if the string was deleted.  

> [!NOTE]
> Currently this deletes all strings and readds all except the one at the given index. This is slow and I need to improve it later.  
> This also removes the precache data if you delete something from `modelprecache` or so.  

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
Supports: Linux32  

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

#### pvs.AddEntityToPVS(Entity ent / table ents / EntityList list)
Adds the given entity to the PVS  

#### pvs.OverrideStateFlags(Entity ent / table ents / EntityList list, number flags, bool engine)
table ents - A sequential table containing all the entities which states flags should be overridden.  
bool engine - Allows you to set the edict flags directly. It won't make sure that the value is correct!  
Overrides the StateFlag for this Snapshot.  
The value will be reset in the next one.  
> [!NOTE]
> If you use engine, you should know what your doing or else it might cause a crash.  

#### pvs.SetStateFlags(Entity ent / table ents / EntityList list, number flags, bool engine)
table ents - A sequential table containing all the entities which states should be set.  
bool engine - Allows you to set the edict flags directly. It won't make sure that the value is correct!  
Sets the State flag for this entity.  
Unlike `OverrideStateFlag`, this won't be reset after the snapshot.  
> [!NOTE]
> If you use engine, you should know what your doing or else it might cause a crash.  

#### number/table pvs.GetStateFlags(Entity ent / table ents / EntityList list, bool engine)
bool engine - Allows you to get all edict flags instead of only the ones for networking.  
Returns the state flags for this entity.  

#### bool pvs.RemoveEntityFromTransmit(Entity ent / table ents / EntityList list)
table ents - A sequential table containing all the entities that should be removed from being transmitted.  
Returns true if the entity or all entities were successfully removed from being transmitted.  

> [!NOTE]
> Only use this function inside the `HolyLib:[Pre/Post]CheckTransmit` hook!  

#### pvs.RemoveAllEntityFromTransmit()
Removes all Entities from being transmitted.  

> [!NOTE]
> Only use this function inside the `HolyLib:[Pre/Post]CheckTransmit` hook!  

#### pvs.AddEntityToTransmit(Entity ent / table ents / EntityList list, bool always)
table ents - A sequential table containing all the entities that should be transmitted.  
bool always - If the entity should always be transmitted? (Verify)  

Adds the given Entity to be transmitted.

> [!NOTE]
> Only use this function inside the `HolyLib:[Pre/Post]CheckTransmit` hook!  

#### (REMOVED AGAIN) pvs.IsEmptyBaseline()
Returns `true` if the baseline is empty.  
This should always be the case after a full update.  

> [!NOTE]
> Only use this function inside the `HolyLib:[Pre/Post]CheckTransmit` hook!  

> [!IMPORTANT] 
> This function was removed since I can't get it to work / It would be a bit more complicated than first anticipated.  

#### pvs.SetPreventTransmitBulk(Entity ent / table ents / EntityList list, Player ply / table plys / RecipientFilter filter, bool notransmit)
table ents - A sequential table containing all the entities that should be affected.  
table plys - A sequential table containing all the players that it should set it for.  
bool notransmit - If the entity should stop being transmitted.  

Adds the given Entity to be transmitted.  

#### bool / table pvs.TestPVS(Entity ent / Vector origin, Entity ent / Vector pos / EntityList list)
Returns `true` if the given entity / position is inside the PVS of the given origin.  
If given a EntityList, it will return a table wich contains the result for each entity.  
The key will be the entity and the value is the result.  

#### table pvs.FindInPVS(Entity ent / Vector pos)
Returns a table containing all entities that are inside the pvs.  

#### pvs.ForceFullUpdate(Player ply)
Forces a full update for the specific client.  

#### table pvs.GetEntitesFromTransmit()
Returns a table containing all entities that will be networked.  

### Enums

#### pvs.FL_EDICT_DONTSEND = 1  
The Entity won't be networked.  

#### pvs.FL_EDICT_ALWAYS = 2
The Entity will always be networked.  

#### pvs.FL_EDICT_PVSCHECK = 4
The Entity will only be networked if it's inside the PVS.  

#### pvs.FL_EDICT_FULLCHECK = 8
The Entity's `ShouldTransmit` function will be called, and its return value will be used.  

### Hooks

#### bool HolyLib:PreCheckTransmit(Entity ply)
Called before the transmit checks are done.  
Return `true` to cancel it.  

You could do the transmit stuff yourself inside this hook.  

#### HolyLib:PostCheckTransmit(Entity ply)
entity ply - The player that everything is transmitted to.  

## surffix
This module ports over [Momentum Mod's](https://github.com/momentum-mod/game/blob/develop/mp/src/game/shared/momentum/mom_gamemovement.cpp#L2393-L2993) surf fixes.  

Supports: Linux32  

### ConVars

#### sv_ramp_fix (default `1`)
If enabled, it will enable additional checks to make sure that the player is not stuck in a ramp.  

#### sv_ramp_initial_retrace_length (default `0.2`, max `5`)
Amount of units used in offset for retraces  

#### sv_ramp_bumpcount (default `8`, max `32`)
Helps with fixing surf/ramp bugs

## filesystem
This module contains multiple optimizations for the filesystem and a lua library.  
Supports: Linux32 | Windows32  

> [!NOTE]
> On Windows32 it will only add the Lua functions.  
> The optimizations aren't implemented yet.  

### Functions
This module also adds a `filesystem` library which should generally be faster than gmod's functions, because gmod has some weird / slow things in them.  
It also gives you full access to the filesystem and doesn't restrict you to specific directories.  

#### (FSASYNC Enum) filesystem.AsyncRead(string fileName, string gamePath, function callback(string fileName, string gamePath, FSASYNC status, string content), bool sync)
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

#### bool filesystem.Rename(string oldFileName, string newFileName, string gamePath = "DATA")
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
Will return `0` if the file wasn't found.  

#### number filesystem.TimeAccessed(string fileName, string gamePath = "GAME")
Returns the time the given file was last accessed.  
Will return `0` if the file wasn't found.  

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
> [!NOTE]
> This will probably cause issues if you open the same file multiple times.  

> [!WARNING]
> This is a noticeable improvement, but it seems to break .bsp files :/  

### (EXPERIMENTAL) holylib_filesystem_savesearchcache (default `1`)
If enabled, the search cache will be written into a file and loaded on startup to improve startup times

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

#### holylib_filesystem_writesearchcache
Writes the search cache into a file.  

#### holylib_filesystem_readsearchcache
Reads the search cache from a file.  

#### holylib_filesystem_dumpabsolutesearchcache
Prints the absolute search cache.  

## util
This module adds two new functions to the `util` library.  

Supports: Linux32 | Linux64 | Windows32 | Windows64  

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

> [!NOTE]
> Decompressing seems to be far faster than compressing so it won't need as many threads.  

## concommand
This module unblocks `quit` and `exit` for `RunConsoleCommand`.  

Supports: Linux32 | Linux64  

### ConVars

#### holylib_concommand_disableblacklist (default `0`)
If enabled, it completely disables the concommand/convar blacklist.  

## vprof
This module adds VProf to gamemode calls, adds two convars and an entire library.

Supports: Linux32 | Linux64 | Windows32  

> [!NOTE]
> On Windows this doesn't currently show the hooks in vprof itself.  
> It only adds the functions for now.  

### Functions

#### vprof.Start()
Starts vprof.  

#### vprof.Stop()
Stops vprof.  

#### bool vprof.AtRoot()
Returns `true` if is vprof scope currently is at it's root node.  

#### VProfCounter vprof.FindOrCreateCounter(string name, number group)
Returns the given vprof counter or creates it if it's missing.  

> [!NOTE]
> If the vprof counter limit is hit, it will return a dummy vprof counter!

#### VProfCounter vprof.GetCounter(number index)
Returns the counter by it's index.  
Returns nothing if the counter doesn't exist.  

#### number vprof.GetNumCounters()
Returns the number of counters that exist.  

#### vprof.ResetCounters()
Resets all counters back to `0`.  

#### number vprof.GetTimeLastFrame()
Returns the time the last frame took.  

#### number vprof.GetPeakFrameTime()
Returns the peak time of the root node.  

#### number vprof.GetTotalTimeSampled()
Returns the total time of the root node.  

#### number vprof.GetDetailLevel()
Returns the current detail level.  

#### number vprof.NumFramesSampled()
Returns the number of frames it sampled.  

#### vprof.HideBudgetGroup(string name, bool hide = false)
Hides/Unhides the given budget group.  

#### number vprof.GetNumBudgetGroups()
Returns the number of budget groups.  
There is a limit on how many there can be.  

#### number vprof.BudgetGroupNameToBudgetGroupID(string name)
Returns the budget group id by the given name.  

#### string vprof.GetBudgetGroupName(number index)
Returns the name of the budget group by the given index.  

#### number vprof.GetBudgetGroupFlags(number index)
Returns the budget group flags by the index.  

#### Color vprof.GetBudgetGroupColor(number index)
Returns the color of the given budget group.  

#### VProfNode vprof.GetRoot()
Returns the root node.  

#### VProfNode vprof.GetCurrentNode()
Returns the current node.  

#### bool vprof.IsEnabled()
Returns `true` if vprof is enabled/running.  

#### vprof.MarkFrame()
If vprof is enabled, it will call MarkFrame on the root node.  

#### vprof.EnterScope(string name, number detailLevel, string budgetGroup)
Enters a new scope.  

#### vprof.ExitScope()
Leaves the current scope.  

#### vprof.Pause()
Pauses vprof.  

#### vprof.Resume()
Resumes vprof.  

#### vprof.Reset()
Resets vprof.  

#### vprof.ResetPeaks()
Resets all peaks.  

#### vprof.Term()
Terminates vprof and frees the memory of everything.
This will invalidate all `VProfCounter` and `VProfNode`.  
This means that if you try to use one that you stored in lua, it could crash!  

> [!NOTE]
> This should probably never be used.  

### VProfCounter
This object represents a vprof counter.  
It internally only contains a string and a pointer to the counter value.  

#### string VProfCounter:\_\_tostring()
If called on a invalid object, it will return `VProfCounter [NULL]`.  
Normally returns `VProfCounter [name][value]`.  

> [!NOTE]
> A VProfCounter currently will NEVER become NULL, instead if something called vprof.Term you'll probably crash when using the class.

#### string VProfCounter:Name()
Returns the name of the counter.  

#### VProfCounter:Set(number value)
Sets the counter to the given value.  

#### number VProfCounter:Get()
Returns the current value of the counter.  

#### VProfCounter:Increment()
Increments the counter by one.  

#### VProfCounter:Decrement()
Decrements the counter by one.  

### VProfNode
This object basicly fully exposes the `CVProfNode` class.  

#### string VProfNode:\_\_tostring()
If called on a invalid object, it will return `VProfNode [NULL]`.  
Normally returns `VProfNode [name]`.  

> [!NOTE]
> A VProfNode currently will NEVER become NULL, instead if something called vprof.Term you'll probably crash when using the class.

#### string VProfNode:GetName()
Returns the name of this node.  

#### number VProfNode:GetBudgetGroupID()
Returns the budget group id of the node.  

#### number VProfNode:GetCurTime()
Returns the current time of this node.  

#### number VProfNode:GetCurTimeLessChildren()
Returns the current time of this node but without it's children.  

#### number VProfNode:GetPeakTime()
Returns the peak time of this node.  

#### number VProfNode:GetPrevTime()
Returns the previous time of this node.  

#### number VProfNode:GetPrevTimeLessChildren()
Returns the previous time of this node but without it's children.  

#### number VProfNode:GetTotalTime()
Returns the total time of this node.  

#### number VProfNode:GetTotalTimeLessChildren()
Returns the total time of this node but without it's children.  

#### number VProfNode:GetCurCalls()
Returns the amount of times this node was in scope in this frame.  

#### VProfNode VProfNode:GetChild()
Returns the child node of this node.  

#### VProfNode VProfNode:GetParent()
Returns the parent node of this node.  

#### VProfNode VProfNode:GetSibling()
Returns the next sibling of this node.  

#### VProfNode VProfNode:GetPrevSibling()
Returns the previous silbling of this node.  

#### number VProfNode:GetL2CacheMisses()
Returns the number of L2 cache misses. Does this even work?  

#### number VProfNode:GetPrevL2CacheMissLessChildren()
Returns the previous number of L2 cache misses without the children.  

#### number VProfNode:GetPrevLoadHitStoreLessChildren()
Does nothing / returns always 0.  

#### number VProfNode:GetTotalCalls()
Returns how often this node was in scope.  

#### VProfNode VProfNode:GetSubNode(string name, number detailLevel, string budgetGroup)
Returns or creates the VProfNode by the given name.  

#### number VProfNode:GetClientData()
Returns the set client data.  

#### VProfNode:MarkFrame()
Marks the frame.  
It will move all current times into their previous variants and reset them to `0`.  
It will also call MarkFrame on any children or sibling.  

#### VProfNode:ClearPrevTime()
Clears the previous time.  

#### VProfNode:Pause()
Pauses this node.  

#### VProfNode:Resume()
Resumes this node.  

#### VProfNode:Reset()
Resets this node.  

#### VProfNode:ResetPeak()
Resets the peak of this node.  

#### VProfNode:SetBudgetGroupID(number budgetGroupID)
Sets the new budget group by it's id.  

#### VProfNode:SetCurFrameTime(number frameTime)
Sets the current frametime.  

#### VProfNode:SetClientData(number data)
Sets the client data...  

#### VProfNode:EnterScope()
Enters the scope.  

#### VProfNode:ExitScope()
Exists the scope.  

### Enums
Theses are the CounterGroup_t enums.  

#### vprof.COUNTER_GROUP_DEFAULT = 0

#### vprof.COUNTER_GROUP_NO_RESET = 1

#### vprof.COUNTER_GROUP_TEXTURE_GLOBAL = 2

#### vprof.COUNTER_GROUP_TEXTURE_PER_FRAME = 3

#### vprof.COUNTER_GROUP_TELEMETRY = 4

### ConVars

#### holylib_vprof_exportreport (default `1`)
If enabled, vprof results will be dumped into a file in the vprof/ folder  

### cvars
This module adds one function to the `cvars` library.  

Supports: Linux32 | Linux64 | Windows32 | Windows64  

> [!NOTE]
> The lua library is named `cvar` because the `cvars` library is fully declared in Lua and were running before it even exists.   

#### Functions

##### table cvar.GetAll()
Returns a sequential table with all ConVar's that exist.  

##### bool cvar.SetValue(string name, string value)
Set the convat to the given value.  
Returns `true` on success.  

##### cvar.Unregister(ConVar cvar / string name)
Unregisters the given convar.  

#### ConVar cvar.Find(string name)
Returns the found ConVar or `nil` on failure.  
Unlike Gmod's `GetConVar_Internal` there are no restrictions placed on it.  

## sourcetv
This module adds a new `sourcetv` library and a new class `CHLTVPlayer`.

Supports: Linux32  

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

Tries to start a new recording.  
Returns one of the `RECORD_` enums.  

#### string sourcetv.GetRecordingFile()
Returns the filename of the current recording. or `nil`.  

#### number sourcetv.StopRecord()  
Stops the active record.  

Returns one of the `RECORD_` enums.  
If successfully stopped, it will return `sourcetv.RECORD_OK`.

#### table sourcetv.GetAll()
Returns a table that contains all HLTV clients. It will return `nil` on failure.  

#### CHLTVClient sourcetv.GetClient(number slot)
Returns the CHLTVClient at that slot or `nil` on failure.  

#### sourcetv.FireEvent(IGameEvent event, bool allowOverride = false)
Fires the gameevent for all hltv clients / broadcasts it.  
If `allowOverride` is set to true, it internally won't block any events like `hltv_cameraman`, `hltv_chase` and `hltv_fixed`.  

#### sourcetv.SetCameraMan(number entIndex / Entity ent)  
Sends the `hltv_cameraman` event all clients and blocks the `HLTVDirector` from changing the view.  
Call it with `0` / `NULL` to reset it and let the `HLTVDirector` take control again.  

> [!NOTE]
> This won't set it for new clients. You have to call it again for thoes.  

### CHLTVClient
This is a metatable that is pushed by this module. It contains the functions listed below  
This class is based off the `CBaseClient` class and inherits its functions.  

#### string CHLTVClient:\_\_tostring()
Returns the a formated string.  
Format: `CHLTVClient [%i][%s]`  
`%i` -> UserID  
`%s` -> ClientName  

#### CHLTVClient:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any CHLTVClient:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `CHLTVClient:GetTable()`.  

#### table CHLTVClient:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

#### CHLTVClient:SetCameraMan(number entIndex / Entity ent)  
Sends the `hltv_cameraman` event to the specific client and blocks the `HLTVDirector` from changing the view.  
Call it with `0` / `NULL` to reset it and let the `HLTVDirector` take control again.  

### Enums

#### sourcetv.RECORD_OK = 0
The recording was started.  

#### sourcetv.RECORD_NOSOURCETV = -1  
SourceTV is not active!  

#### sourcetv.RECORD_NOTMASTER = -2  
the sourcetv server is not the master!  

#### sourcetv.RECORD_ACTIVE = -3  
there already is an active record!  

> [!NOTE]
> Should we allow multiple active record? I think I could implement it. If wanted, make a request for it.  

#### sourcetv.RECORD_NOTACTIVE = -4  
there is no active recording to stop!  

#### sourcetv.RECORD_INVALIDPATH = -5  
The filepath for the recording is invalid!  

#### sourcetv.RECORD_FILEEXISTS = -6  
A file with that name already exists!  

### Hooks

#### HolyLib:OnSourceTVNetMessage(CHLTVClient client, bf_read buffer)
Called when a CHLTVClient sends a net message to the server.  

Example:  
```lua
util.AddNetworkString("Example")
hook.Add("HolyLib:OnSourceTVNetMessage", "Example", function(client, bf)
	local name = util.NetworkIDToString(bf:ReadShort())) -- Reads the header
	print(name, bf:ReadString())
end)

---- Client

net.Start("Example")
	net.WriteString("Hello World from CHLTVClient");
net.SendToServer()
```

#### bool HolyLib:OnSourceTVCommand(CHLTVClient client, string cmd, table args, string argumentString)
Called when a CHLTVClient sends a command.  
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

#### bool HolyLib:OnSourceTVStartNewShot()
Called when SourceTV tries to start a new shot.  
Return `true` to cancel it.  

#### bool HolyLib:OnSourceTVClientDisconnect(CHLTVClient client)
Called when a client disconnects from the sourcetv server.  

### ConVars

#### holylib_sourcetv_allownetworking (default `0`)
If enabled, HLTV Clients can send net messages to the server and `HolyLib:OnSourceTVNetMessage` will be called.  

#### holylib_sourcetv_allowcommands (default `0`)
If enabled, HLTV Clients can send commands to the server and `HolyLib:OnSourceTVCommand` will be called.  

## bitbuf
This module adds a `bf_read` and `bf_write` class.  

Supports: Linux32 | Linux64 | Windows32 | Windows64  

### Functions

#### bf_read bitbuf.CopyReadBuffer(bf_read buffer)
Copies the given buffer into a new one.  
Useful if you want to save the data received by a client.  

> [!NOTE]
> The size is clamped internally between a minimum of `4` bytes and a maximum of `262144` bytes.

#### bf_read bitbuf.CreateReadBuffer(string data)
Creates a read buffer from the given data.  
Useful if you want to read the userdata of the instancebaseline stringtable.  

> [!NOTE]
> The size is clamped internally between a minimum of `4` bytes and a maximum of `262144` bytes.

#### bf_write bitbuf.CreateWriteBuffer(number size or string data)
Create a write buffer with the given size or with the given data.  

> [!NOTE]
> The size is clamped internally between a minimum of `4` bytes and a maximum of `262144` bytes.

### bf_read
This class will later be used to read net messages from HLTV clients.  
> ToDo: Finish the documentation below and make it more detailed.  

#### string bf_read:\_\_tostring()
Returns the a formated string.  
Format: `bf_read [%i]`  
`%i` -> size of data in bits.  

#### bf_read:\_\_gc()
Deletes the buffer internally.  

#### bf_read:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any bf_read:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `bf_read:GetTable()`.  

#### table bf_read:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

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

> [!NOTE]
> This is only available for the 32x!    

#### bool bf_read:IsOverflowed()
Returns `true` if the buffer is overflowed.  

#### number bf_read:PeekUBitLong(number bits)
Peaks to the given bit(doesn't change current position) and returns the value.

#### number bf_read:ReadBitAngle(number bits)
Peaks to the given bit(doesn't change current position) and returns the value.

#### angle bf_read:ReadBitAngles()
Reads and Angle.  

#### number bf_read:ReadBitCoord()

> [!NOTE]
> This is only available for the 32x!    

#### number bf_read:ReadBitCoordBits()

> [!NOTE]
> This is only available for the 32x!    

#### number bf_read:ReadBitCoordMP(bool integral = false, bool lowPrecision = false)

> [!NOTE]
> This is only available for the 32x!    

#### number bf_read:ReadBitCoordMPBits(bool integral = false, bool lowPrecision = false)

> [!NOTE]
> This is only available for the 32x!    

#### number bf_read:ReadBitFloat()

#### number bf_read:ReadBitLong(number bits, bool signed = false)
Reads a number with the given number of bits.  

> [!NOTE]
> This is only available for the 32x!    

#### number bf_read:ReadBitNormal()

#### string bf_read:ReadBits(number bits)
Reads the given number of bits.  

#### vector bf_read:ReadBitVec3Coord()
Reads a Vector.  

#### vector bf_read:ReadBitVec3Normal()
Reads a normalizted Vector.

#### number bf_read:ReadByte()
Reads a byte.  

#### string bf_read:ReadBytes(number bytes)
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

#### number bf_read:ReadSBitLong(number bits)
Reads a number with the given amout of bits.  

#### number bf_read:ReadShort()
Reads a short.  

#### number bf_read:ReadSignedVarInt32()

#### number bf_read:ReadSignedVarInt64()

#### string bf_read:ReadString()
Reads a null terminated string.  

#### number bf_read:ReadUBitLong(number bits)
Read a number with the given amount of bits.  

#### number bf_read:ReadUBitVar()

#### number bf_read:ReadVarInt32()

#### number bf_read:ReadVarInt64()

#### number bf_read:ReadWord()

#### bf_read:Reset()
Resets the current position and resets the overflow flag.  

#### bool bf_read:Seek(number pos)
Sets the current position to the given position.  
Returns `true` on success.  

#### bool bf_read:SeekRelative(number pos)
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

#### bf_write:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any bf_write:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `bf_write:GetTable()`.  

#### table bf_write:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

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

> [!WARNING]
> You should keep a reference to the string.  

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
Writes the given bytes.  

#### bf_write:WriteOneBit(bool value = false)
Writes one bit.  

#### bf_write:WriteOneBitAt(number bit, bool value = false)
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

#### bf_write:WriteUBitLong(number value, number bits, bool signed = false)

#### bf_write:WriteBitAngle(number value, number bits)

#### bf_write:WriteBitAngles(Angle ang)
Writes a Angle. (`QAngle` internally).  

#### bf_write:WriteBitVec3Coord(Vector vec)
Writes a Vector.  

#### bf_write:WriteBitVec3Normal(Vector vec)
Writes a normalized Vector

#### bool bf_write:WriteBits(string data, number bits)
Writes the given number of bits from the data into this buffer.  
Returns `true` on success.  

#### bool bf_write:WriteBitsFromBuffer(bf_read buffer, number bits)
Writes the given number of bits from the given buffer into this buffer.  
Returns `true` on success.  

> [!NOTE]
> The current position for the given buffer will change as we internally read from it!

#### bf_write:WriteBitNormal(number value)

#### bf_write:WriteBitLong(number value)

#### bf_write:WriteBitFloat(number value)

#### bf_write:WriteBitCoord(number value)

#### bf_write:WriteBitCoordMP(number value, bool integral = false, bool lowPrecision = false)

#### bf_write:WriteString(string value)
Writes a null terminated string

## Networking
This module tries to optimize anything related to networking.  
Currently, this only has one optimization which was ported from [sigsegv-mvm](https://github.com/rafradek/sigsegv-mvm/blob/910b92456c7578a3eb5dff2a7e7bf4bc906677f7/src/mod/perf/sendprop_optimize.cpp#L35-L144) into here.  

Supports: Linux32 | Linux64  

## steamworks
This module adds a few functions related to steam.  

Supports: Linux32 | Linux64  

### Functions

#### steamworks.Shutdown()
Shutdowns the Steam Server.  

#### steamworks.Activate()
Starts/Activates the Steam Server.  

#### bool steamworks.IsConnected()
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

#### bool HolyLib:OnNotifyClientConnect(number nextUserID, string ip, string steamID64, number authResult) 
authResult - The steam auth result, `0` if steam accepts the client.  

| authResult | Description |
|-------|------|
| `0` | Ticket is valid for this game and this steamID |
| `1` | Ticket is not valid |
| `2` | A ticket has already been submitted for this steamID |
| `3` | Ticket is from an incompatible interface version |
| `4` | Ticket is not for this game |
| `5` | Ticket has expired |

Called when a client wants to authenticate through steam.  

Return `true` to forcefully authenticate his steamid.  
Return `false` to block his authentication.
Return nothing to let the server normally handle it.

### ConVars

#### holylib_steamworks_allow_duplicate_steamid(default `0`)
If enabled, the same steamid can be used multiple times.

> [!NOTE]
> `HolyLib:OnNotifyClientConnect` already accounts for this,  
> if a steamid is already used when you approve a request it will still allow the player to join.

## systimer
This module is just the timer library, but it doesn't use CurTime.  
> [!NOTE]
> Timer repetitions are limited to an unsigned int.  

Supports: Linux32 | Linux64 | Windows32 | Windows64  

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

Supports: Linux32 | Linux64 | Windows32 | Windows64  

### Functions

#### table pas.FindInPAS(Entity ent / Vector vec)
Returns a sequential table containing all entities in that PAS.  

#### bool / table pas.TestPAS(Entity ent / Vector pas / EntityList list, Entity / Vector hearPos)
Tests if the give hear position is inside the given pas.  
Returns `true` if it is.  

If given a EntityList, it will return a table wich contains the result for each entity.  
The key will be the entity and the value is the result.  

#### bool pas.CheckBoxInPAS(Vector mins, Vector maxs, Vector pas)
Checks if the given pox is inside the PAS.  
Returns `true` if it is.  

## voicechat
This module contains a few things to control the voicechat.  
You could probably completly rework the voicechat with this since you could completly handle it in lua.  

Supports: Linux32 | Linux64 | Windows32  

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
> [!NOTE]
> This will ignore the set player slot!  

#### bool voicechat.IsHearingClient(Player ply, Player targetPly)
Returns `true` if `ply` can hear the `targetPly`.  

#### bool voicechat.IsProximityHearingClient(Player ply, Player targetPly)
Returns `true` if `ply` can hear the `targetPly` in it's proximity.  

#### VoiceData voicedata.CreateVoiceData(number playerSlot = 0, string data = NULL, number dataLength = NULL)
Creates a new VoiceData object.  
> [!NOTE]
> All Arguments are optional!

#### VoiceStream voicechat.CreateVoiceStream()
Creates a empty VoiceStream.  

#### VoiceStream, number(statusCode) voicechat.LoadVoiceStream(string fileName, string gamePath = "DATA", bool async = false, function callback = nil)
callback = `function(VoiceStream loadedStream, number statusCode)`
statusCode = `-2 = File not found, -1 = Invalid type, 0 = None, 1 = Done`  

Tries to load a VoiceStream from the given file.  
If `async` is specified it **WONT** return **anything** and the `callback` will be **required**.  

#### number(statusCode) voicechat.SaveVoiceStream(VoiceStream stream, string fileName, string gamePath = "DATA", bool async = false, function callback = nil)
callback = `function(VoiceStream loadedStream, number statusCode)`
statusCode = `-2 = File not found, -1 = Invalid type, 0 = None, 1 = Done`  

Tries to save a VoiceStream into the given file.  
If `async` is specified it **WONT** return **anything** and the `callback` will be **required**.  

> [!NOTE]
> It should be safe to modify/use the VoiceStream while it's saving async **BUT** you should try to avoid doing that.

####

### VoiceData
VoiceData is a userdata value that is used to manage the voicedata.  

#### string VoiceData:\_\_tostring()
Returns `VoiceData [Player Slot][Length/Size]`.  

#### VoiceData:\_\_gc()
Garbage collection. Deletes the voice data internally.  

#### VoiceData:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any VoiceData:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `VoiceData:GetTable()`.  

#### table VoiceData:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

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

#### VoiceData VoiceData:CreateCopy()
Creates a exact copy of the voice data.  

### VoiceStream
VoiceStream is a userdata value that internally contains multiple VoiceData for specific ticks.  

#### string VoiceStream:\_\_tostring()
Returns `VoiceStream [entires]`.  

#### VoiceStream:\_\_gc()
Garbage collection. Deletes the voice data internally.  

#### VoiceStream:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any VoiceStream:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `VoiceStream:GetTable()`.  

#### table VoiceStream:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

#### bool VoiceStream:IsValid()
Returns `true` if the VoiceData is still valid.  

#### table VoiceStream:GetData()
Returns a table, with the tick as key and **copy** of the VoiceData as value.  

> [!NOTE]
> The returned VoiceData is just a copy,  
> modifying them won't affect the internally stored VoiceData.
> Call `VoiceStream:SetData` or `VoiceStream:SetIndex` after you modified it to update it.  

#### VoiceStream:SetData(table data)
Sets the VoiceStream from the given table.  

#### number VoiceStream:GetCount()
Returns the number of VoiceData it stores.  

#### VoiceData VoiceStream:GetIndex(number index)
Returns a **copy** of the VoiceData for the given index or `nil`.  

> [!NOTE]
> The returned VoiceData is just a copy,  
> modifying them won't affect the internally stored VoiceData.
> Call `VoiceStream:SetData` or `VoiceStream:SetIndex` after you modified it to update it.  

#### VoiceStream:SetIndex(number index, VoiceData data)
Create a copy of the given VoiceData and sets it onto the specific index and overrides any data thats already present.  

### Hooks

#### bool HolyLib:PreProcessVoiceChat(Player ply, VoiceData data)
Called before the voicedata is processed.  
Return `true` to stop the engine from processing it.  

> [!NOTE]
> After the hook the `VoiceData` becomes **invalid**, if you want to store it call `VoiceData:CreateCopy()` and use the returned VoiceData.

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

		voiceTbl[ply][engine.TickCount() - voiceStartTick] = voiceData:CreateCopy()
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

#### holylib_voicechat_threads(default `1`)
The number of threads the voicechat can use for async stuff.  
voicechat.LoadVoiceStream and voicechat.SaveVoiceStream currently use it,  
originally I expected thoes both functions to be far slower but they turned out to be quite fast.

## physenv
This module fixes https://github.com/Facepunch/garrysmod-issues/issues/642 and adds a few small things.  

Supports: Linux32 | Windows32  

> [!NOTE]
> Will soon have support for Windows & Linux64  
> Windows currently likes to crash :/  

### Functions

#### physenv.SetLagThreshold(number ms)
The lag threshold(time in ms) which if exceeded will cause it to call the `HolyLib:PhysicsLag` hook.  

> [!NOTE]
> Only works on Linux32  

#### number physenv.GetLagThreshold()
Returns the lag threshold in ms.  

> [!NOTE]
> Only works on Linux32  

#### physenv.SetPhysSkipType(IVP_SkipType type)
Sets the skiptype for the current simulation.  
This is reset after the simulation ended.  

> [!NOTE]
> Only works on Linux32  

#### IPhysicsEnvironment physenv.CreateEnvironment()
Creates a new physics environment.  
It will apply all settings from the main environment on the newly created on to match.  

> [!NOTE]
> If you notice Physics objects that have a invalid entity, report this. But this should normally never happen.  
> In this case, this means that something is failing to remove physics objects and it will cause random crashes.  

#### IPhysicsEnvironment physenv.GetActiveEnvironmentByIndex(number index)
Returns the physics environment by the given index.  

#### physenv.DestroyEnvironment(IPhysicsEnvironment environment)
Destroys the given physics environment.  

#### IPhysicsEnvironment physenv.GetCurrentEnvironment()
Returns the currently simulating environment.  

#### physenv.EnablePhysHook(bool shouldCall)
Enables/Disables the `HolyLib:OnPhysFrame` hook.  

#### IPhysicsCollisionSet physenv.FindCollisionSet(number index)
Returns the collision set by the given index.  

> [!NOTE]
> Only 32 collision sets can exist at the same time!   

#### IPhysicsCollisionSet physenv.FindOrCreateCollisionSet(number index)
Returns the collision set by the given index or creates it if needed.  

> [!NOTE]
> Only 32 collision sets can exist at the same time!  

#### physenv.DestroyAllCollisionSets()
Destroys all collision sets.  

### Functions (physcollide)

#### CPhysCollide physcollide.BBoxToCollide(Vector mins, Vector maxs)
Creates a CPhysCollide from the given mins & maxs.  

#### CPhysConvex physcollide.BBoxToConvex(Vector minx, Vector maxs)
Creates a CPhysConvex from the given mins & maxs.  

#### CPhysCollide physcollide.ConvertConvexToCollide(CPhysConvex convex)
Converts the given convex to a CPhysCollide and deletes it.  

#### CPhysCollide physcollide.ConvertPolysoupToCollide(CPhysPolySoup soup)
Converts the given polysoup to a CPhysCollide and deletes it.  

#### physcollide.ConvexFree(CPhysConvex convex)
Frees the given CPhysConvex if it wan't used/converted.  

#### CPhysPolySoup physcollide.PolysoupCreate()
Creates a CPhysPolySoup.  

#### physcollide.PolysoupAddTriangle(CPhysPolySoup soup, Vector a, Vector b, Vector c, number materialIndex)
Adds a triangle to the polysoup.  

#### physcollide.PolysoupDestroy(CPhysPolySoup soup)
Frees the given CPhysPolySoup if it wasn't used/converted.  

#### Vector(mins), Vector(maxs) physcollide.CollideGetAABB(CPhysCollide collide, Vector origin, Angle rotation)
Returns the AABB of the given CPhysCollide.  

#### Vector physcollide.CollideGetExtent(CPhysCollide collide, Vector origin, Angle rotation, Vector direction)
Get the support map for a collide in the given direction.  

#### Vector physcollide.CollideGetMassCenter(CPhysCollide collide)
Gets the mass center of the CPhysCollide.  

#### Vector physcollide.CollideGetOrthographicAreas(CPhysCollide collide)
get the approximate cross-sectional area projected orthographically on the bbox of the collide

> [!NOTE]
> These are fractional areas - unitless.  Basically this is the fraction of the OBB on each axis that  
> would be visible if the object were rendered orthographically.  
> This has been precomputed when the collide was built or this function will return 1,1,1

#### number physcollide.CollideIndex(CPhysCollide collide)
Returns the index of the physics model.  

#### physcollide.CollideSetMassCenter(CPhysCollide collide, Vector massCenter)
Sets the new mass center of the CPhysCollide.  

#### physcollide.CollideSetOrthographicAreas(CPhysCollide collide, Vector area)
I have no Idea....   

#### number physcollide.CollideSize(CPhysCollide collide)
Returns the memory size of the CPhysCollide.  

#### number physcollide.CollideSurfaceArea(CPhysCollide collide)
Computes the surface area of the CPhysCollide.  

#### number physcollide.CollideVolume(CPhysCollide collide)
Computes the volume of the CPhysCollide.  

#### string physcollide.CollideWrite(CPhysCollide collide, bool swap)
Serializes the CPhysCollide and returns the data containing it.  

### CPhysCollide physcollide.UnserializeCollide(string data, number index)
Unserializes the given data into a CPhysCollide.  

#### number physcollide.ConvexSurfaceArea(CPhysConvex convex)
Computes the surface area of the convex.  

#### number physcollide.ConvexVolume(CPhysConvex convex)
Computes the volume of the convex.  

#### ICollisionQuery physcollide.CreateQueryModel(CPhysCollide collide)
Creates a ICollisionQuery from the given CPhysCollide.  

#### physcollide.DestroyQueryModel(ICollisionQuery query)
Destroys the given ICollisionQuery.  

#### physcollide.DestroyCollide(CPhysCollide collide)
Destroys the given CPhysCollide.  

### objectparams_t table structure.
This table structure is used by a few functions.  

> [!NOTE]
> You don't need to set all fields.
> Every single field has a default.  

#### number damping = 0.1
#### number dragCoefficient = 1.0
#### bool enableCollisions = true
#### number inertia = 1.0
#### number mass = 1.0
#### Vector massCenterOverride = nil
#### string name = "DEFAULT"
#### number rotdamping = 0.1
#### number rotInertiaLimit = 0.05
#### number volume = 0.0

### CPhysCollide

#### CPhysCollide:\_\_tostring()
Returns `CPhysCollide [NULL]` if invalid.  
Else it returns `CPhysCollide`.  

#### CPhysCollide:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any CPhysCollide:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `CPhysCollide:GetTable()`.  

#### table CPhysCollide:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

#### bool CPhysCollide:IsValid()
Returns `true` if the CPhysCollide is still valid.   

### CPhysPolySoup

#### CPhysPolySoup:\_\_tostring()
Returns `CPhysPolySoup [NULL]` if invalid.  
Else it returns `CPhysPolySoup`.  

#### CPhysPolySoup:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any CPhysPolySoup:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `CPhysPolySoup:GetTable()`.  

#### table CPhysPolySoup:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

#### bool CPhysPolySoup:IsValid()
Returns `true` if the CPhysPolySoup is still valid.  

### CPhysConvex

#### CPhysConvex:\_\_tostring()
Returns `CPhysConvex [NULL]` if invalid.  
Else it returns `CPhysConvex`.  

#### CPhysConvex:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any CPhysConvex:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `CPhysConvex:GetTable()`.  

#### table CPhysConvex:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

#### bool CPhysConvex:IsValid()
Returns `true` if the CPhysConvex is still valid.  

### ICollisionQuery

#### ICollisionQuery:\_\_tostring()
Returns `ICollisionQuery [NULL]` if invalid.  
Else it returns `ICollisionQuery`.  

#### ICollisionQuery:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any ICollisionQuery:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `ICollisionQuery:GetTable()`.  

#### table ICollisionQuery:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

#### bool ICollisionQuery:IsValid()
Returns `true` if the collision query is still valid.  

#### number ICollisionQuery:ConvexCount()
Returns the number of Convexes.  

#### number ICollisionQuery:TriangleCount(number convexIndex)
Returns the number of triangles for the given convex index.  

#### number ICollisionQuery:GetTriangleMaterialIndex(number convexIndex, number triangleIndex)
Returns the material index for the given triangle.  

#### ICollisionQuery:SetTriangleMaterialIndex(number convexIndex, number triangleIndex, number materialIndex)
Sets the material index of the given triangle index.

#### Vector, Vector, Vector ICollisionQuery:GetTriangleVerts(number convexIndex, number triangleIndex)
Returns the three vectors that bukd the triangle at the given index.  

#### ICollisonQuery:SetTriangleVerts(number convexIndex, number triangleIndex, Vector a, Vector b, Vector c)
Sets the three Vectors that build the triangle at the given index.  

### IPhysicsCollisionSet

#### IPhysicsCollisionSet:\_\_tostring()
Returns `IPhysicsCollisionSet [NULL]` if invalid.  
Else it returns `IPhysicsCollisionSet`.  

#### IPhysicsCollisionSet:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any IPhysicsCollisionSet:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `IPhysicsCollisionSet:GetTable()`.  

#### table IPhysicsCollisionSet:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

#### bool IPhysicsCollisionSet:IsValid()
Returns `true` if the collisionset is still valid.  

#### IPhysicsCollisionSet:EnableCollisions(number index1, number index2)
Marks collisions to be enabled for the two indexes.  

#### IPhysicsCollisionSet:DisableCollisions(number index1, number index2)
Marks collisions to be disabled for the two indexes.  

#### bool IPhysicsCollisionSet:ShouldCollide(number index1, number index2)
Returns `true` if the collision between the two objects are enabled.  

### IPhysicsEnvironment

#### IPhysicsEnvironment:\_\_tostring()
Returns `IPhysicsEnvironment [NULL]` if invalid.  
Else it returns `IPhysicsEnvironment`.  

#### IPhysicsEnvironment:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any IPhysicsEnvironment:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `IPhysicsEnvironment:GetTable()`.  

#### table IPhysicsEnvironment:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

#### bool IPhysicsEnvironment:IsValid()
Returns `true` if the environment is still valid.  

#### bool IPhysicsEnvironment:TransferObject(IPhysicsObject obj, IPhysicsEnvironment newEnvironment)
Transfers the physics object from this environment to the new environment.  

> [!WARNING]
> You shouldn't transfer players or vehicles.  

#### IPhysicsEnvironment:SetGravity(Vector newGravity)
Sets the new gravity in `source_unit/s^2`  

#### Vector IPhysicsEnvironment:GetGravity()
Returns the current gravity in  `source_unit/s^2`  

#### IPhysicsEnvironment:SetAirDensity(number airdensity)
Sets the new air density.  

#### number IPhysicsEnvironment:GetAirDensity()
Returns the current air density.  

#### IPhysicsEnvironment:SetPerformanceSettings(PhysEnvPerformanceSettings settings)
Sets the new performance settings.  
Use the [PhysEnvPerformanceSettings](https://wiki.facepunch.com/gmod/Structures/PhysEnvPerformanceSettings) structure.  

#### table IPhysicsEnvironment:GetPerformanceSettings()
Returns the current performance settings.  
The table will use the [PhysEnvPerformanceSettings](https://wiki.facepunch.com/gmod/Structures/PhysEnvPerformanceSettings) structure.  

#### number IPhysicsEnvironment:GetNextFrameTime()
returns the current simulation clock's value at the next frame.  
This is an absolute time.  

#### number IPhysicsEnvironment:GetSimulationTime()
returns the current simulation clock's value.  
This is an absolute time.  

#### IPhysicsEnvironment:SetSimulationTimestep(number timeStep)
Sets the next simulation timestep.  

#### number IPhysicsEnvironment:SetSimulationTimestep()
returns the next simulation timestep.  

#### number IPhysicsEnvironment:GetActiveObjectCount()
returns the number of active physics objects.  

#### table IPhysicsEnvironment:GetActiveObjects()
returns a table containing all active physics objects.  

#### table IPhysicsEnvironment:GetObjectList()
returns a table containing all physics objects.  

#### bool IPhysicsEnvironment:IsInSimulation()
returns true if the current physics environment is in simulation.  

#### IPhysicsEnvironment:ResetSimulationClock()
resets the simulation clock.  

#### IPhysicsEnvironment:CleanupDeleteList()
cleans the delete list?  

#### IPhysicsEnvironment:SetQuickDelete(bool quickDelete)
Sets quick delete?  

#### IPhysicsEnvironment:EnableDeleteQueue(bool deleteQueue)
Enables/Disables the delete queue.  

#### IPhysicsEnvironment:Simulate(number deltaTime, bool onlyEntities = false)
Simulates the given delta time in the environment.  
If `onlyEntities` is set, it will only update the Entities, without simulating the physics environment.  

#### IPhysicsObject IPhysicsEnvironment:CreatePolyObject(CPhysCollide collide, number materialIndex, Vector origin, Angle angles, table objectparams_t)
Creates a new IPhysicsObject in the environment.  

#### IPhysicsObject IPhysicsEnvironment:CreatePolyObjectStatic(CPhysCollide collide, number materialIndex, Vector origin, Angle angles, table objectparams_t)
Creates a new static IPhysicsObject in the environment.  

#### IPhysicsObject IPhysicsEnvironment:CreateSphereObject(number radius, number materialIndex, Vector origin, Angle angles, table objectparams_t, bool static = false)
Creates a new perfect sphere IPhysicsObject in the environment.  

#### IPhysicsEnvironment:DestroyObject(IPhysicsObject object)
Destroys the given physics object.  

#### bool IPhysicsEnvironment:IsCollisionModelUsed(CPhysCollide collide)
Returns true if it uses a collision model.  
This function is internally use for debugging?  

#### IPhysicsEnvironment:SetObjectEventHandler(function onObjectWake(IPhysicsObject obj), function onObjectSleep(IPhysicsObject obj))
Allows you to add callbacks when physics objects wake up or go to sleep in the environment.  

#### IPhysicsObject IPhysicsEnvironment:CreateWorldPhysics()
Creates the world physics object and also adds all static props.  

### Enums
Theses are the IVP_SkipType enums.  

#### physenv.IVP_NoSkip = 0
Let the simulation run normally.  

#### physenv.IVP_SkipImpact = 1
Skip all impact calls.  

#### physenv.IVP_SkipSimulation = 2
Skip the entire simulation.  

> [!NOTE]
> Players that collide with props will be randomly teleported!  

### Hooks

#### IVP_SkipType HolyLib:OnPhysicsLag(number simulationTime)
Called when the physics simulaton is taking longer than the set lag threshold.  

You can freeze all props here and then return `physenv.IVP_SkipSimulation` to skip the simulation for this tick if someone is trying to crash the server.  

> [!NOTE]
> Only works on Linux32  

#### bool HolyLib:OnPhysFrame(number deltaTime)  
Called when the physics are about to be simulated.  
Return `true` to stop the engine from doing anything.  

> [!NOTE]
> This hook first needs to be enabled by calling `physenv.EnablePhysHook(true)`  

Example of pausing the physics simulation while still allowing the entities to be updated and moved with the physgun:  
```lua
physenv.EnablePhysHook(true)
local mainEnv = physenv.GetActiveEnvironmentByIndex(0)
hook.Add("HolyLib:OnPhysFrame", "Example", function(deltaTime)
	mainEnv:Simulate(deltaTime, true) -- the second argument will only cause the entities to update.
    return true -- We stop the engine from running the simulation itself again as else it will result in issue like "Reset physics clock" being spammed
end)
```

## (Experimental) bass
This module will add functions related to the bass dll.  
Does someone have the bass libs for `2.4.15`? I can't find them :<  
`.mp3` files most likely won't work.  

Since this module is experimental, it's disabled by default.  
You can enable it with `holylib_enable_bass 1`  

Supports: Linux32 | Linux64  

### Functions

#### bass.PlayFile(string filePath, string flags, function callback)
callback - function(IGMODAudioChannel channel, number errorCode, string error)  
Creates a IGMODAudioChannel for the given file.  

#### bass.PlayURL(string URL, string flags, function callback)
callback - function(IGMODAudioChannel channel, number errorCode, string error)  
Creates a IGMODAudioChannel for the given url.  

### IGModAudioChannel

#### string IGModAudioChannel:\_\_tostring()
Returns `IGModAudioChannel [NULL]` if invalid.  
Else it returns `IGModAudioChannel [FileName/URL]`.  

#### IGModAudioChannel:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any IGModAudioChannel:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `IGModAudioChannel:GetTable()`.  

#### table IGModAudioChannel:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

#### IGModAudioChannel:\_\_gc()
ToDo / Doesn nothing yet.  

#### IGModAudioChannel:Destroy()
Destroys the audio channel.  

#### IGModAudioChannel:Stop()
Stops the channel.  

#### IGModAudioChannel:Pause()
Pauses the channel.  

#### IGModAudioChannel:Play()
Plays the channel.  

#### IGModAudioChannel:SetVolume(number volume)
Sets the volume of the channel.  
(It's serverside... how does volume even play a role)  

#### number IGModAudioChannel:GetVolume()
Returns the volume of the channel.  

#### IGModAudioChannel:SetPlaybackRate(number playbackRate)
Sets the playback rate of the channel.  

#### number IGModAudioChannel:GetPlaybackRate()
Returns the playback rate of the channel.  

#### IGModAudioChannel:SetTime(number time)
Sets the time of the channel.  

#### number IGModAudioChannel:GetTime()
Returns the time of the channel.  

#### number IGModAudioChannel:GetBufferedTime()
Returns the buffered time of the channel.  

> [!NOTE]
> If it's playing a file, it will just return the length of it.  

#### number IGModAudioChannel:GetState()
Returns the state of the channel.  

#### IGModAudioChannel:SetLooping(bool looping)
Sets looping for the channel.  

#### bool IGModAudioChannel:IsLooping()
Returns `true` if the channel will loop.  

#### bool IGModAudioChannel:IsOnline()
Returns `true` if were playing a URL.  

#### bool IGModAudioChannel:Is3D()
Returns `true` if the channel is a 3D one.  

#### bool IGModAudioChannel:IsBlockStreamed()
Returns `true` if the sound is received in chunks.  

#### bool IGModAudioChannel:IsValid()
Returns `true` if the channel is valid.  

#### string IGModAudioChannel:GetFileName()
Returns the filename or URL the channel is playing.  

#### number IGModAudioChannel:GetSampleRate()
Returns the samplerate of the channel.  

#### number IGModAudioChannel:GetBitsPerSample()
Returns the bits per sample.  

#### number IGModAudioChannel:GetAverageBitRate()
Returns the average bit rate.  

#### number, number IGModAudioChannel:GetLevel()
Returns the level of the channel.  
First value is left, second is right.  

#### IGModAudioChannel:SetChannelPan(number pan)
Sets the channel pan.  

#### number IGModAudioChannel:GetChannelPan()
Returns the channel pan.  

#### string IGModAudioChannel:GetTags()
Returns the tag of the channel.  

#### IGModAudioChannel:Restart()
Restarts the channel.  

#### (Soon)IGModAudioChannel:FFT(table output, number fft)
Computes the DFT of the sound channel.  
What even is that.  

## entitiylist
This module just adds a lua class.  
Only use their functions after entities were created or you might be missing entities in the returned tables!  

Supports: Linux32 | Linux64 | Windows32 | Windows64  

### Functions

#### EntityList CreateEntityList()
Creates a new EntityList.

#### table GetGlobalEntityList()
Returns all entities that are in the global entity list.  

### EntityList
This class should remove some overhead to improve performance since you can pass it to some functions.  
It is also used by other modules to improve their performance if they have to loop thru all entities or push entities to lua.  

#### string EntityList:\_\_tostring()
Returns `EntityList [NULL]` if given invalid list.  
Normally returns `EntityList [Entity number]`.  

#### EntityList:\_\_gc()
If called, it will free and nuke the entity list.  
You should never use it after this was called.  

#### EntityList:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any EntityList:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `EntityList:GetTable()`.  

#### table EntityList:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

> [!NOTE]
> On All other Objects this is named `:GetTable`

#### bool EntityList:IsValid()
Returns `true` if the EntityList is valid.  

#### table EntityList:GetEntities()
Returns a table with all the entities.  

#### EntityList:SetEntities(table entities)
Sets the EntityList from the given table.  

#### EntityList:AddEntities(table entities)
Adds the entities from the given table to the Entity list.  

#### EntityList:RemoveEntities(table entities)
Removes the entities that are in the table from the Entity list.  

#### EntityList:AddEntity(Entity ent)
Adds the given entity to the list.  

#### EntityList:RemoveEntity(Entity ent)
Removes the given entity from the list.  

## httpserver
This module adds a library with functions to create and run a httpserver.  

### Functions

#### HttpServer httpserver.Create()
Creates a new HTTPServer.  

#### httpserver.Destroy(HttpServer server)
Destroys the given http server.  

#### table httpserver.GetAll()
Returns a table containing all existing HttpServer in case you lose a reference.

#### HttpServer httpserver.FindByName(string name)
Returns the HttpServer with the given name set by `HttpServer:SetName()` or returns `nil` on failure.

### HttpServer
This class represents a created HttpServer.

#### string HttpServer:\_\_tostring()
Returns `HttpServer [NULL]` if given invalid list.  
Normally returns `HttpServer [Address:Port - Name]`.  

#### HttpServer:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any HttpServer:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `HttpServer:GetTable()`.  

#### table HttpServer:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

#### bool HttpServer:IsValid()
Returns `true` if the HttpServer is valid.  

#### HttpServer:Start(string address, number port)
This will start or restart the HTTP Server, and it will listen on the given address + port.  
NOTE: If a Method function was called like HttpServer:Get after HttpServer:Start was called, you need to call HttpServer:Start again!

#### HttpServer:Stop()
This stops the HTTP Server.

#### HttpServer:Think()
Goes through all requests and calls their callbacks or deletes them after they were sent out.

> [!NOTE]
> This is already internally called every frame, so you don't need to call this.

#### bool HttpServer:IsRunning()
Returns true if the HTTPServer is running.

#### HttpServer:SetTCPnodelay(bool noDelay)
Sets whether a delay should be added to tcp or not.

#### HttpServer:SetReadTimeout(number sec, number usec)
Sets the maximum amount of time before a read times out.

#### HttpServer:SetWriteTimeout(number sec, number usec)
Sets the maximum amount of time before a write times out.

#### HttpServer:SetPayloadMaxLength(number maxLength)
Sets the maximum payload length.

#### HttpServer:SetKeepAliveTimeout(number sec)
Sets the maximum time a connection is kept alive.

#### HttpServer:SetKeepAliveMaxCount(number amount)
Sets the maximum amount of connections that can be kept alive at the same time.

#### HttpServer:SetThreadSleep(number sleepTime)
The number of ms threads sleep before checking again if a request was handled.  
Useful to raise it when you let requests wait for a while.

#### HttpServer:SetMountPoint(string mountPoint, string folder)
This mounts the given folder to the given path.

> [!NOTE]
> You can call this multiple times for the same path to mount multiple folders to it.

#### HttpServer:RemoveMountPoint(string mountPoint)
This removes all mounts for the given path.

#### number HttpServer:GetPort()
Returns the port that was originally passed to `HttpServer:Start()`

#### string HttpServer:GetAddress()
Returns the address that was originally passed to `HttpServer:Start()`

#### string HttpServer:GetName()
Returns the name set by `HttpServer:SetName()`, defaults to `NONAME`

#### HttpServer:SetName(string name)
Sets the name of the HttpServer.

### Method Functions
All Method functions add a listener for the given path and the given method, like this:
```lua
HttpServer:Get("/public", function(_, response)
  print("Public GET request")
  response.SetContent("You sent a GET request to a public site.", "text/plain")
end, false)

HttpServer:Get("/private", function(_, response)
  print("Private GET request")
  response.SetContent("You sent a GET request to a private site.", "text/plain")
end, true)
```

If you enable the IP Whitelist, only requests sent by connected players are processed.

#### HttpServer:Get(string path, function (HttpRequest, HttpResponse), bool ipWhitelist)
#### HttpServer:Put(string path, function (HttpRequest, HttpResponse), bool ipWhitelist)
#### HttpServer:Post(string path, function (HttpRequest, HttpResponse), bool ipWhitelist)
#### HttpServer:Patch(string path, function (HttpRequest, HttpResponse), bool ipWhitelist)
#### HttpServer:Delete(string path, function (HttpRequest, HttpResponse), bool ipWhitelist)
#### HttpServer:Options(string path, function (HttpRequest, HttpResponse), bool ipWhitelist)

### HttpRequest
A incoming Http Request.

#### string HttpRequest:\_\_tostring()
Returns `HttpRequest [NULL]` if given invalid list.  
Normally returns `HttpRequest`.  

#### HttpRequest:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any HttpRequest:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `HttpRequest:GetTable()`.  

#### table HttpRequest:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

#### bool HttpRequest:IsValid()
Returns `true` if the HttpRequest is valid.  

#### bool HttpRequest.HasHeader(string key)
returns true if the client has the given key in the header.

#### bool HttpRequest.HasParam(string key)
returns true if the client has the given key in the parameters.

#### string HttpRequest.GetHeader(string key)
returns the value of the given key from the header.

#### string HttpRequest:GetParam(string key)
returns the value of the given key from the parameters.

#### string HttpRequest:GetBody()
The body of the HTTP Request.

#### string HttpRequest:GetRemoteAddr()
the IP Address of the Person who sent the HTTP Request

#### number HttpRequest:GetRemotePort()
The Port the HTTP request was received from.

#### string HttpRequest:GetLocalAddr()

#### number HttpRequest:GetLocalPort()

#### string HttpRequest:GetMethod()
The HTTP Method that was used like GET or PUT.

#### number HttpRequest:GetAuthorizationCount()

#### number HttpRequest:GetContentLength()
The length of the HTTP Request content.

#### CBaseClient HttpRequest:GetClient()
Returns the client who sent the HTTP Request or `nil` if it didn't find it.  

#### Player HttpRequest:GetPlayer()
Returns the player who sent the HTTP Request or `nil` if it didn't find it.  

### HttpResponse
A Http Response.

#### string HttpResponse:\_\_tostring()
Returns `HttpResponse [NULL]` if given invalid list.  
Normally returns `HttpResponse`.  

#### HttpResponse:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any HttpResponse:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `HttpResponse:GetTable()`.  

#### table HttpResponse:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

#### bool HttpResponse:IsValid()
Returns `true` if the HttpResponse is valid.  

#### HttpResponse:SetContent(string content, string contentType = "text/plain")
Sets the content like this:
```lua
Response:SetContent("Hello World", "text/plain")
```

#### HttpResponse:SetRedirect(string url, number code = 302)
Redirects one to the given URL and returns the given code.

#### HttpResponse:SetHeader(string key, string value)
Sets the given value for the given key in the header.

## luajit
This module updates luajit to a newer version.

> [!NOTE]
> By default this module is disabled.
> You can enable it by adding `-holylib_enable_luajit 1` to the command line.

> [!WARNING]
> It's **not** recommended to enable/disable it at runtime!  

The `ffi` and `string.buffer` packages are already added when enabled.  
It also restores `debug.setlocal`, `debug.setupvalue`, `debug.upvalueid` and `debug.upvaluejoin`.  

## gameserver
This module adds a library that exposes the `CBaseServer` and `CBaseClient`.  

### Functions

#### number gameserver.GetNumClients()
returns current number of clients  

#### number gameserver.GetNumProxies()
returns current number of attached HLTV proxies  

#### number gameserver.GetNumFakeClients()
returns current number of fake clients/bots  

#### number gameserver.GetMaxClients()
returns current client limit  

#### number gameserver.GetUDPPort()
returns the udp port the server is running on.  

#### CGameClient gameserver.GetClient(number playerSlot)
Returns the CGameClient at that player slot or `nil` on failure.  

#### number gameserver.GetClientCount()
returns client count for iteration  

#### table gameserver.GetAll()
Returns a table that contains all game clients. It will return `nil` on failure.  

#### number gameserver.GetTime()
Returns the current time/curtime  

#### number gameserver.GetTick()
Returns the current tick  

#### number gameserver.GetTickInterval()
Returns the current tick interval  

#### string gameserver.GetName()
Returns the current server/host name.  
Should be the same as `GetConVar("hostname"):GetString()`

#### string gameserver.GetMapName()
Returns the current map name  

#### number gameserver.GetSpawnCount()

#### number gameserver.GetNumClasses()

#### number gameserver.GetClassBits()

#### bool gameserver.IsActive()

#### bool gameserver.IsLoading()

#### bool gameserver.IsDedicated()

#### bool gameserver.IsPaused()

#### bool gameserver.IsMultiplayer()

#### bool gameserver.IsPausable()

#### bool gameserver.IsHLTV()

#### string gameserver.GetPassword()
Returns the current server password.  

#### gameserver.SetMaxClients(number maxClients)
Sets the max client count.  
Should do nothing.  

#### gameserver.SetPaused(bool paused = false)
Pauses/Unpauses the server.  

> [!NOTE]
> Verify if this actually works

#### gameserver.SetPassword(string password)
Sets the server password.  

#### gameserver.BroadcastMessage(number type, string name, bf_write buffer)
Sends a custom net message to all clients.  
This function allows you send any netmessage you want.  
You should know what your doing since this function doesn't validate anything.  
You can find all valid types in the [protocol.h](https://github.com/RaphaelIT7/gmod-holylib/blob/main/source/sourcesdk/protocol.h#L86-L145)  

> [!NOTE]
> This was formerly known as `HolyLib.BroadcastCustomMessage`

Example usage, we enable `sv_cheats` clientside while having it disabled serverside.  
```lua
local bf = bitbuf.CreateWriteBuffer(8) -- Create a 8 bytes / 64 bits buffer.

bf:WriteByte(1) -- How many convars were sending
bf:WriteString("sv_cheats") -- ConVar name
bf:WriteString("1") -- ConVar value

-- You can use CBaseClient:SendNetMsg to send it to specific clients.
gameserver.BroadcastMessage(5, "NET_SetConVar", bf) -- 5 = net_SetConVar / net message type.
```

#### number gameserver.CalculateCPUUsage()
Calculates and returns the CPU Usage.

#### number gameserver.ApproximateProcessMemoryUsage()
Approximates the memory usage of the server in bytes.  
It isn't really related to the gameserver itself, but since it has CalculateCPUUsage I want to keep them close.

#### number gameserver.SendConnectionlessPacket(bf_write bf, string ip)
ip - The target ip. Format `ip:port`  
Sends out a connectionless packet to the target ip.
Returns the length of the sent data or `-1` on failure.

> [!NOTE]
> It's expected that **YOU** already added the connectionless header, this was done to not have to copy the buffer.  
> `bf:WriteLong(-1) -- Write this as the first thing. This is the CONNECTIONLESS_HEADER`

### CBaseClient
This class represents a client.

#### CBaseClient:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.  

#### any CBaseClient:\_\_index(string key)
Internally seaches first in the metatable table for the key.  
If it fails to find it, it will search in the lua table before returning.  
If you try to get multiple values from the lua table, just use `CBaseClient:GetTable()`.  

#### table CBaseClient:GetTable()
Returns the lua table of this object.  
You can store variables into it.  

#### number CBaseClient:GetPlayerSlot()
Returns the slot of the client. Use this for `sourcetv.GetClient`.  

#### number CBaseClient:GetUserID()
Returns the userid of the client.  

#### string CBaseClient:GetName()
Returns the name of the client.

#### string CBaseClient:GetSteamID()
Returns the steamid of the client.

#### CBaseClient:Reconnect()
Reconnects the client.

#### CBaseClient:ClientPrint(string message)
Prints the given message into the client's console.  

> [!NOTE]
> It **won't** add `\n` to the end, you have to add it yourself. 

#### bool CBaseClient:IsValid()
Returns `true` if the client is still valid.  

#### bool (Experimental) CBaseClient:SendLua(string code)
Sends the given code to the client to be executed.  
Returns `true` on success.  

> [!NOTE]
> This function was readded back experimentally. It wasn't tested yet. It's still broken but doesn't crash  

#### (Experimental) CBaseClient:FireEvent(IGameEvent event)  
Fires/sends the gameevent to this specific client.  

#### number CBaseClient:GetFriendsID()
Returns the friend id.  

#### string CBaseClient:GetFriendsName()
Returns the friend name.  

#### number CBaseClient:GetClientChallenge()
Returns the client challenge number.  

#### CBaseClient:SetReportThisFakeClient(bool reportClient = false)

#### bool CBaseClient:ShouldReportThisFakeClient()

#### CBaseClient:Inactivate()
Inactivates the client.  

> [!WARNING]
> Know what your doing when using it!

#### CBaseClient:Disconnect(string reason, bool silent = false, bool nogameevent = false)
silent - Silently closes the net channel and sends no disconnect to the client.
nogameevent - Stops the `player_disconnect` event from being created.

Disconnects the client.  

#### CBaseClient:SetRate(number rate)

#### number CBaseClient:GetRate()

#### CBaseClient:SetUpdateRate(number rate)

#### number CBaseClient:GetUpdateRate()

#### CBaseClient:Clear()
Clear the client/frees the CBaseClient for the next player to use.  

> [!WARNING]
> Know what your doing when using it!

#### CBaseClient:DemoRestart()

#### number CBaseClient:GetMaxAckTickCount()

#### CBaseClient:ExecuteStringCommand(string command)
Executes the given command as if the client himself ran it.  

#### CBaseClient:SendNetMsg(number type, string name, bf_write buffer)
Same as `gameserver.BroadcastMessage` but it only sends it to the specific player.  

> [!NOTE]
> This function was formerly known as `HolyLib.SendCustomMessage`  

#### bool CBaseClient:IsConnected()

#### bool CBaseClient:IsSpawned()

#### bool CBaseClient:IsActive()

#### number CBaseClient:GetSignonState()

#### bool CBaseClient:IsFakeClient()

#### bool CBaseClient:IsHLTV()

#### bool CBaseClient:IsHearingClient(number playerSlot)
Works like `voicechat.IsHearingClient` but only takes the direct player slot.  

#### bool CBaseClient:IsProximityHearingClient(number playerSlot)
Works like `voicechat.IsProximityHearingClient` but only takes the direct player slot.  

#### CBaseClient:SetMaxRoutablePayloadSize(number playloadSize)

#### CBaseClient:UpdateAcknowledgedFramecount(number tick)

#### bool CBaseClient:ShouldSendMessages(number tick)

#### CBaseClient:UpdateSendState()

#### bool CBaseClient:SetSignonState(number signOnState, number spawnCount = 0, bool rawSet = false)
Sets the SignOnState for the given client.  
Returns `true` on success.  

> [!NOTE]
> This function does normally **not** directly set the SignOnState.  
> Instead it calls the responsible function for the given SignOnState like for `SIGNONSTATE_PRESPAWN` it will call `SpawnPlayer` on the client.  
> Set the `rawSet` to `true` if you want to **directly** set the SignOnState.  

> [!NOTE]
> This function was formerly known as `HolyLib.SetSignOnState`

#### CBaseClient:UpdateUserSettings(bf_write buffer)

#### CBaseClient:SendServerInfo()

#### CBaseClient:SendSignonData()

#### CBaseClient:SpawnPlayer()

#### CBaseClient:ActivatePlayer()

#### CBaseClient:SetName(string name)
Sets the new name of the client.  

#### CBaseClient:SetUserCVar(string convar, string value)

#### CBaseClient:FreeBaselines()

#### CBaseClient:OnRequestFullUpdate()
Forces the client to go through a full update(also fires the gameevent).  

### bool CBaseClient:SetSteamID(string steamID64)
Sets the SteamID of the client.  
Returns `true` on success.  

> [!NOTE]
> Gmod seamingly has some backup code inside `CBaseClient::ProcessClientInfo`,  
> that kicks a player with `Server connection error, please try again` if they don't have a valid steamid.

---

### CBaseClient (CNetChannel functions)
Now come network channel related functions to the CBaseClient

> [!NOTE]
> All functions are dependant on the CNetChannel of their client.
> No CNetChannel == Lua Error

#### bool CBaseClient:GetProcessingMessages()

#### bool CBaseClient:GetClearedDuringProcessing()

#### number CBaseClient:GetOutSequenceNr()

#### number CBaseClient:GetInSequenceNr()

#### number CBaseClient:GetOutSequenceNrAck()

#### number CBaseClient:GetOutReliableState()

#### number CBaseClient:GetInReliableState()

#### number CBaseClient:GetChokedPackets()

#### bf_write CBaseClient:GetStreamReliable()
Returns the reliable stream used by net messages.

#### bf_write CBaseClient:GetStreamUnreliable()
Returns the unreliable stream used by net messages.

#### bf_write CBaseClient:GetStreamVoice()
Returns the voice stream used by the voice chat.

#### number CBaseClient:GetStreamSocket()

#### number CBaseClient:GetMaxReliablePayloadSize()

#### number CBaseClient:GetLastReceived()

#### number CBaseClient:GetConnectTime()

#### number CBaseClient:GetClearTime()

#### number CBaseClient:GetTimeout()

#### CBaseClient:SetTimeout(number seconds)
Sets the time in seconds before the client is marked as timing out.

#### bool CBaseClient:Transmit(bool onlyReliable = false, number fragments = -1, bool freeSubChannels = false)
Transmit any pending data to the client.  
Returns `true` on success.

> [!WARNING]
> Transmitting data to the client causes the client's prediction to **reset and cause prediction errors!**.

Exampe usage of this function:
```lua
concommand.Add("nukechannel", function(ply)
   	local string = ""
    for k = 1, 65532 do
        string = string .. "a"
    end
    util.AddNetworkString("Example")
    for k = 1, 10 do
        net.Start("Example", false)
            net.WriteString(string)
        net.Broadcast()
        gameserver.GetClient(ply:EntIndex()-1):Transmit() -- Forces the message to be transmitted directly avoiding a overflow.
    end 
end)
```

##### freeSubChannels argument
Marks all sub channel's of the client's net channel as freed allowing data to be transmitted again.  
It's a possible speed improvement yet fragments may get lost & cause issues / it's unsafe.  

Example code showing off the speed difference.
```lua
hook.Add("Think", "Example", function()
	for _, client in ipairs(gameserver.GetAll()) do
		if !client or !client:IsValid() or client:GetSignonState() != 3 then
			continue
		end

		for k=1, 20 do
			client:Transmit(false, 7, true) -- Causes net message fragments to be transmitted.
		end
	end
end)
```
the result is visible when downloading a map from the server(no workshop, no fastdl)  

https://github.com/user-attachments/assets/f700dae9-28a9-4c1f-b237-cb0547226d6a

#### (REMOVED) number CBaseClient:HasQueuedPackets()

#### bool CBaseClient:ProcessStream()
Processes all pending incoming net messages.  
Returns `true` on success.

#### CBaseClient:SetMaxBufferSize(bool reliable = false, number bytes, bool voice = false)
Resizes the specified buffer to the given size in bytes.  

> [!NOTE]
> All data inside that stream is discarded, make sure everything was sent out.

Example usage of this function.
```lua
concommand.Add("biggerBuffer", function(ply)
	local client = gameserver.GetClient(ply:EntIndex()-1)
    client:Transmit() -- Send everything out or else we lose data
    client:SetMaxBufferSize(true, 524288) -- We resize the reliable stream
end)
```

### CGameClient
This class inherits CBaseClient.

#### string CGameClient:\_\_tostring()
Returns the a formated string.  
Format: `CGameClient [%i][%s]`  
`%i` -> UserID  
`%s` -> ClientName  

### Hooks

#### bool HolyLib:OnSetSignonState(CGameClient client, number state, number spawnCount)
Called when the engine is about to change the client's SignonState.  
Return `true` to stop the engine.  

#### bool HolyLib:OnChannelOverflow(CGameClient client)
Called when the client's net channel is overflowed.
Return `true` to stop the engine from disconnecting the client.  

Example on how to handle a overflow:
```lua
hook.Add("HolyLib:OnChannelOverflow", "Example", function(client)
	local bf = client:GetStreamReliable()
	file.Write("stream.txt", bf:GetData())
	bf:Reset() -- Reset the stream, data may get lost, but the client won't be dropped
	return true -- Return true to stop the engine from disconnecting the client
end)
```

#### HolyLib:OnPlayerChangedSlot(number oldPlayerSlot, number newPlayerSlot)
Called **after** a player was moved to a different slot.  
This happens when a player on a player slot above 128 tries to spawn.  

Why is this done? Because any player above 128 is utterly unstable and can only stabily exist as a CGameClient.  
if a CBasePlayer entity is created on a slot above 128 expect stability issues!

#### HolyLib:OnClientDisconnect(CGameClient client)
Called when a client disconnects.

#### bool HolyLib:ProcessConnectionlessPacket(bf_read buffer, string ip)
Called when a connectionless packet is received.  
Return `true` to mark the packet as handled.  

Won't be called if `holylib_gameserver_connectionlesspackethook` is set to `0`.  

Example of retrieving the `A2S_INFO` from a Source Engine Server.
```lua
function AskForServerInfo(targetIP, challenge)
	local bf = bitbuf.CreateWriteBuffer(64)

	bf:WriteLong(-1) -- CONNECTIONLESS_HEADER
	bf:WriteByte(string.byte("T")) -- A2S_INFO Header
	bf:WriteString("Source Engine Query") -- Null terminated string

	if challenge then
		bf:WriteLong(challenge) -- Challange response if we got a S2C_CHALLENGE
	end

	gameserver.SendConnectionlessPacket(bf, targetIP)
end

function GetServerInfo(targetIP)
	AskForServerInfo(targetIP)

	hook.Add("HolyLib:ProcessConnectionlessPacket", "ProcessResponse", function(bf, ip)
		if ip != targetIP then return end

		local msgHeader = bf:ReadByte()
		if msgHeader == string.byte("A") then -- It responded with a S2C_CHALLENGE
			AskForServerInfo(targetIP, bf:ReadLong())
			return true
		end

		local a2s_info = {
			Header = msgHeader,
			Protocol = bf:ReadByte(),
			Name = bf:ReadString(),
			Map = bf:ReadString(),
			Folder = bf:ReadString(),
			Game = bf:ReadString(),
			ID = bf:ReadShort(),
			Players = bf:ReadByte(),
			MaxPlayers = bf:ReadByte(),
			Bots = bf:ReadByte(),
			ServerType = bf:ReadByte(),
			Environment = string.char(bf:ReadByte()),
			Visibility = bf:ReadByte(),
			VAC = bf:ReadByte(),
			Version = bf:ReadString(),
			ExtraDataFlag = bf:ReadByte(),
		}

		if bit.band(a2s_info.ExtraDataFlag, 0x80) != 0 then
			a2s_info.Port = bf:ReadShort()
		end

		if bit.band(a2s_info.ExtraDataFlag, 0x10) != 0 then
			a2s_info.SteamID = bf:ReadLongLong()
		end

		if bit.band(a2s_info.ExtraDataFlag, 0x40) != 0 then
			a2s_info.SourceTVPort = bf:ReadShort()
			a2s_info.SourceTVName = bf:ReadString()
		end

		if bit.band(a2s_info.ExtraDataFlag, 0x20) != 0 then
			a2s_info.Tags = bf:ReadString()
		end

		if bit.band(a2s_info.ExtraDataFlag, 0x01) != 0 then
			a2s_info.GameID = bf:ReadLongLong()
		end

		PrintTable(a2s_info)
	end)
end

--- Example call
GetServerInfo("xxx.xxx.xxx.xxx:27015")
```

Output:
```txt
["Bots"]        =       0
["Environment"] =       108
["Folder"]      =       garrysmod
["Game"]        =       Sandbox
["Header"]      =       73
["ID"]  =       4000
["Map"] =       gm_flatgrass
["MaxPlayers"]  =       128
["Name"]        =       RaphaelIT7's Testing Hell
["Players"]     =       0
["Protocol"]    =       17
["ServerType"]  =       100
["VAC"] =       1
["Visibility"]  =       1
```

Example of retrieving the `A2S_PLAYER` from a Source Engine Server.
```lua
function AskForServerPlayerInfo(targetIP, challenge)
	local bf = bitbuf.CreateWriteBuffer(64)

	bf:WriteLong(-1) -- CONNECTIONLESS_HEADER
	bf:WriteByte(string.byte("U")) -- A2S_PLAYER Header

	bf:WriteUBitLong(challenge or -1, 32) -- Challange response if we got a S2C_CHALLENGE

	gameserver.SendConnectionlessPacket(bf, targetIP)
end

function GetServerPlayerInfo(targetIP)
	AskForServerPlayerInfo(targetIP)

	hook.Add("HolyLib:ProcessConnectionlessPacket", "ProcessResponse", function(bf, ip)
		if ip != targetIP then return end

		local msgHeader = bf:ReadByte()
		if msgHeader == string.byte("A") then -- It responded with a S2C_CHALLENGE
			AskForServerPlayerInfo(targetIP, bf:ReadUBitLong(32))
			return true
		end

		local a2s_player = {
			Header = string.char(msgHeader),
			Players = bf:ReadByte(),
		}

		for k=1, a2s_player.Players do
			local entry = {}
			a2s_player[k] = entry

			entry.Index = bf:ReadByte()
			entry.Name = bf:ReadString()
			entry.Score = bf:ReadLong()
			entry.Duration = bf:ReadFloat()
		end

		PrintTable(a2s_player)
	end)
end

--- Example call
GetServerPlayerInfo("xxx.xxx.xxx.xxx:27015")
```

Output:
```txt
[1]:
                ["Duration"]    =       40.337387084961
                ["Index"]       =       0
                ["Name"]        =       Raphael
                ["Score"]       =       0
["Header"]      =       D
["Players"]     =       1
```

### ConVars

#### holylib_gameserver_disablespawnsafety (default `0`)
If enabled, players can spawn on slots above 128 but this WILL cause stability and many other issues!  
Added to satisfy curiosity & test around with slots above 128.

#### holylib_gameserver_connectionlesspackethook (default `1`)
If enabled, the HolyLib:ProcessConnectionlessPacket hook is active and will be called.

### Singleplayer
This module allows you to have a 1 slot / a singleplayer server.  
Why? I don't know, but you can.  

### 128+ Players
Yes, with this module you can go above 128 Players **BUT** it will currently crash.  
This is useful when you make a queue, players **can** connect and use all slots above 128 **but** they **can't** spawn when 128 players are already playing.  
Use the `HolyLib:OnSetSignonState` to keep players at the `SIGNONSTATE_NEW` until a slot is freed/one of the 128 disconnects.  

### Player Queue System
Using this module's functionality you can implement a player queue were players wait in the loading screen until they spawn when a slot gets free.

Example implementation:
```lua
playerQueue = playerQueue or {
	count = 0
}

hook.Add("HolyLib:OnSetSignonState", "Example", function(cl, state, c)
	print(cl, state, c)

	local maxSlots = 128 -- Can't exceed 128 players. If you want to only have 100 players, lower it but NEVER go above 128
	local fullServer = player.GetCount() >= maxSlots
	if fullServer and state == SIGNONSTATE_PRESPAWN then -- REQUIRED to be SIGNONSTATE_PRESPAWN
		if not playerQueue[cl] then
			playerQueue[cl] = true
			playerQueue.count = playerQueue.count + 1
			playerQueue[playerQueue.count] = cl
		end

		return true -- Stop the engine from continuing/spawning the player
	end
end)

hook.Add("HolyLib:OnClientDisconnect", "Example", function(client)
	timer.Simple(0, function() -- Just to be sure that the client was really disconnected.
		if playerQueue.count <= 0 then return end

		if client:IsValid() then
			print("Client isn't empty?!? client: " .. client)
			return
		end

		local nextPlayer = playerQueue[1]
		playerQueue[nextPlayer] = nil
		table.remove(playerQueue, 1)
		playerQueue.count = playerQueue.count - 1

		nextPlayer:SpawnPlayer() -- Spawn the client, HolyLib handles the moving of the client.
	end)
end)

hook.Add("HolyLib:OnPlayerChangedSlot", "Example", function(oldPlayerSlot, newPlayerSlot)
	print("Client was moved from slot " .. oldPlayerSlot .. " to slot " .. newPlayerSlot)
end)
```

# Unfinished Modules

## serverplugins
This module adds two new `IServerPluginCallbacks` functions:  
`virtual void OnLuaInit( GarrysMod::Lua::ILuaInterface* LUA )`  
`virtual void OnLuaShutdown( GarrysMod::Lua::ILuaInterface* LUA )`  

For a plugin to be loaded by HolyLib, you need to expose HolyLib's Interface.  
You can expose both HolyLib's and the normal interface since the order of the virtual functions are the same.  
This should already work but I never tested it completly yet (Or I forgot that I did test it).  

Supports: Linux32 | Linux64  

> [!NOTE]
> Windows doesn't have plugins / we won't support it there.  

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
`physenv` module -> https://github.com/Facepunch/garrysmod-issues/issues/642  
`physenv` module -> https://github.com/Facepunch/garrysmod-requests/issues/2522  
`physenv.EnablePhysHook` -> https://github.com/Facepunch/garrysmod-requests/issues/2541  

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
- String -> Var name (Planned to change to 12bits key index in next net compact. Update: This wasn't done -> https://github.com/Facepunch/garrysmod-issues/issues/5687#issuecomment-2438274430)  
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

## `client_lua_files` Stringtable behavior
Key `0` contains **always** all datapack paths(`;` is used as a seperator).  
Example:
```lua
local client_lua_files = stringtable.FindTable("client_lua_files")
local pathData = client_lua_files:GetStringUserData(0)
local pathTable = string.Split(pathData, ";")
PrintTable(pathTable)
```

### Multiplayer
All other keys are a lua filename.  

### Singleplayer
The engine merges all lua files into a single/multiple keys named `singleplayer_files%i`.  
Inside the string's userdata a long string is stored representing all files(`:` is used as a seperator).  
Example(Won't work on a dedicated server):
```lua
local client_lua_files = stringtable.FindTable("client_lua_files")
local fileData = client_lua_files:GetStringUserData(client_lua_files:FindStringIndex("singleplayer_files0"))
local fileTable = string.Split(fileData, ":")
PrintTable(fileTable)
```