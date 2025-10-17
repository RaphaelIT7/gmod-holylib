# Holylib

A library that contains some functions and optimizations for gmod.<br>
If you need any function, make an issue for it, and I'll look into it.<br>
When HolyLib was installed correctly, the variable `_HOLYLIB` should be set to `true` in Lua. (NOTE: This was **added** in the upcoming `0.8` release)<br>

## Windows
So currently to get it working on Windows, I would have to redo most of the hooks, and It would also take a good while.<br>
Because of this, I'm not going to make it currently. I'm gonna slowly start adding all symbols and then someday I'm going to redo most hooks.<br>
There will be a release but it will only contain some parts since many things don't work yet.<br>

> [!NOTE]
> I'm not actively testing windows, so if I accidentally broke it, open a issue since I most likely didn't know about it.<br>

## How to Install (Linux 32x)
1. Download the `ghostinj.dll`, `holylib.vdf` and `gmsv_holylib_linux.so` from the latest release.<br>
2. Put the `ghostinj.dll` into the main directory where `srcds_linux` is located.<br>
3. Put the `holylib.vdf` into the `garrysmod/addons/` directory.<br>
4. Put the `gmsv_holylib_linux.so` into the `garrysmod/lua/bin/` directory.
5. Add `-usegh` to the servers startup params.<br>
If you use a panel like Pterodactyl or something similar, you can use the gamemode field(in most cases) like this: `sandbox -usegh`<br>

If you already had a `ghostinj.dll`, you can rename it to `ghostinj2.dll` and it will be loaded by holylib's ghostinj.<br>

## How to Install (Linux 64x)
1. Download the `holylib.vdf` and `gmsv_holylib_linux.so` from the latest release.<br>
2. Put the `holylib.vdf` into the `garrysmod/addons/` directory.<br>
3. Put the `gmsv_holylib_linux.so` into the `garrysmod/lua/bin/` directory.<br>

## Custom Builds (Linux Only)
You can fork this repository and use the `Build Custom Version` to create a custom HolyLib version which only contains specific functions.<br>
I heard from some that they don't want a huge DLL of which they only use a few functions of,<br>
so by making a custom build anyone can include just the stuff they actually want and need, nothing else.<br>

## How to update (Newer GhostInj)

> [!NOTE]
> Don't try to use plugin_unload since it might not work because of things like the networking module which currently can't be unloaded.<br>
> To update holylib you will always need to shutdown the server.<br>

1. Append `_updated` to the new file to have a result like this `gmsv_holylib_linux_updated.so`.<br>
2. Upload the file into the `lua/bin` folder
3. Restart the server normally.<br>
On the next startup the ghostinj will update holylib to use the new file.<br>
This is done by first deleting the current `gmsv_holylib_linux[64].so` and then renaming the `_updated.so` essentially replacing the original file.<br>

## How to update (Older GhostInj versions)

1. Shutdown the server
2. Upload the new file
3. Enjoy it

## What noticable things does HolyLib bring?
\- A huge Lua API providing deep access to the engine.<br>
\- Implements some bug fixes (some bug fixes were brought into gmod itself).<br>
\- \- `ShouldCollide` won't break the entire physics engine and instead a warning is thrown `holylib: Someone forgot to call Entity:CollisionRulesChanged!`.<br>
\- Lua Hooks are shown in vprof results (done in the `vprof` module)<br>
\- Ported a networking improvement over from [sigsegv-mvm](https://github.com/rafradek/sigsegv-mvm/blob/910b92456c7578a3eb5dff2a7e7bf4bc906677f7/src/mod/perf/sendprop_optimize.cpp#L35-L144) improving performance & memory usage (done in the `networking` module)<br>
\- Ported [Momentum Mod](https://github.com/momentum-mod)'s surfing improvements (done in the `surffix` module)<br>
\- Heavily improved the filesystem (done in the `filesystem` module)<br>
\- Greatly improved Gmod's `GMod::Util::IsPhysicsObjectValid` function improving performance especially when many physics objects exist (done in the `physenv` module)<br>
\- (Disabled by default) Updated LuaJIT version (done in the `luajit` module)<br>
\- Improved ConVar's find code improving performance (done in the `cvars` module)<br>
\- Server <-> Server connections using net channels (done in the `gameserver` module)<br>
\- `CVoiceGameMgr` optimization to heavily reduce calls of the `GM:PlayerCanHearPlayersVoice` (done in the `voicechat` module)<br>
\- Noticably improved `CServerGameEnts::CheckTransmit` (done in the `networking` module)<br>

> [!NOTE]
> The threadpoolfix module currently does nothing as all of the fixes it contained were implemented into the Garry's Mod itself.

## Next Update
\- [+] Any files in `lua/autorun/_holylua/` are loaded by HolyLib on startup.<br>
\- [+] Added a new modules `luathreads`, `networkthreading`, `soundscape`<br>
\- [+] Added `NS_` enums to `gameserver` module.<br>
\- [+] Added missing `CNetChan:Shutdown` function to the `gameserver` module.<br>
\- [+] Added LZ4 compression for newly implemented net channel.<br>
\- [+] Added `util.CompressLZ4` & `util.DecompressLZ4` to `util` module.<br>
\- [+] Implemented a custom `CNetChan` for faster Server <-> Server connections. See https://github.com/RaphaelIT7/gmod-holylib/issues/42<br>
\- [+] Added `HolyLib.ReceiveClientMessage` to `HolyLib` module.<br>
\- [+] Added `physenv.IVP_NONE` flag to `physenv` module.<br>
\- [+] Added a few stringtable related functions to the `stringtable` module.<br>
\- [+] Added a new hook `HolyLib:OnClientTimeout` to the `gameserver` module.<br>
\- [+] Optimized `GM:PlayerCanHearPlayersVoice` by **only** calling it for actively speaking players/when a voice packet is received.<br>
\- [+] Added `voicechat.LoadVoiceStreamFromWaveString`, `voicechat.ApplyEffect`, `voicechat.IsPlayerTalking` & `voicechat.LastPlayerTalked` to the `voicechat` module.<br>
\- [+] Added `VoiceStream:ResetTick`, `VoiceStream:GetNextTick`, `VoiceStream:GetCurrentTick`, `VoiceStream:GetPreviousTick` to the `voicechat` module.<br>
\- [+] Added `util.FancyJSONToTable` & `util.AsyncTableToJSON` to the `util` module.<br>
\- [+] Added `gameserver.GetClientByUserID` & `gameserver.GetClientBySteamID` to the `gameserver` module.<br>
\- [+] Added a config system allowing one to set convars without using the command line.<br>
\- [+] Added `IPhysicsEnvironment:SetInSimulation` to the `physenv` module.<br>
\- [+] Added `HttpResponse:SetStatusCode` to `httpserver` module. (See https://github.com/RaphaelIT7/gmod-holylib/pull/62)<br>
\- [+] Added `HttpRequest:GetPathParam` to `httpserver` module. (See https://github.com/RaphaelIT7/gmod-holylib/pull/63)<br>
\- [+] Added `HttpServer:AddProxyAddress` to `httpserver` module.<br>
\- [+] Added `bitbuf.CreateStackReadBuffer` & `bitbuf.CreateStackWriteBuffer` to `bitbuf` module.<br>
\- [+] Added a fallback method for HolyLib's internal `Util::PushEntity` function in case a Gmod update breaks our offsets which previously lead to undefined behavior<br>
\- [+] Added a `ILuaThreadedCall` to call all modules Think function when HolyLib is loaded as a binary module/loaded using `require("holylib")`<br>
\- [+] Added a new DLL system if anything wants to be loaded with HolyLib. (See: [example-module-dll](https://github.com/RaphaelIT7/gmod-holylib/tree/f937ba454b4d86edfc72df9cb3f8a689d7de2571/example-module-dll))<br>
\- [#] Added some more safeguards to `IPhysicsEnvironment:Simulate` to prevent one from simulating a environment that is already being simulated.<br>
\- [#] Highly optimized `util` module's json code to be noticably faster and use noticably less memory.<br>
\- [#] Better support for multiple Lua states<br>
\- \- This required most of the lua setup to be changed >:(<br>
\- [#] Solved a few possible stack issues<br>
\- [#] Fixed a crash after a map change. See https://github.com/RaphaelIT7/gmod-holylib/issues/41<br>
\- [#] Update internal `netadr` stuct to now properly support `loopback` and `localhost` inputs for IP's.<br>
\- [#] Possibly fixed memory issues caused by `IGModAudioChannel`'s being deleted & having undefined behavior.<br>
\- [#] Fixed `HolyLib:OnPhysicsLag` possibly being called recursively causing a crash.<br>
\- [#] Fixed every function from the `physenv` module not accepting gmod's PhysObj.<br>
\- [#] Fixed a crash caused by `comcommand` module since Gmod's `ConCommand_IsBlocked` changed. (See https://github.com/RaphaelIT7/gmod-holylib/issues/45)<br>
\- [#] Fixed a few Windows issues.<br>
\- \- [+] Added support for the `stringtable` module on Windows.<br>
\- \- [#] Fixed entitylist not being loaded on Windows<br>
\- [#] Solved a few Bass bugs.<br>
\- [#] Fixed possible undefined behavor & two buffer overflows in `gameserver` module.<br>
\- [#] Updated `VoiceStream` file structure to also save the server tickrate and include a file version.<br>
\- \- The `VoiceStream` now properly updates the tickCount for the saved `VoiceData` to scale the tickCount when the server changed tickrate which should ensure the audio remains usable.<br>
\- [#] Fixed a issue with the `gameserver` moduel causing random `Lost Connection` disconnects (See https://github.com/RaphaelIT7/gmod-holylib/issues/51)<br>
\- [#] Extented `networking` module to include some new things.<br>
\- \- Made some optimization for `PackEntities_Normal` reducing overhead<br>
\- \- Slightly optimized our own implementation of `CServerGameEnts::CheckTransmit`<br>
\- \- Added `holylib_networking_fastpath` which will use a transmit cache for `CServerGameEnts::CheckTransmit` as a noticable optimization.<br>
\- \- Added `holylib_networking_maxviewmodels` allowing one to limit view models to `1` for each player instead of each having `3` of which `2` often remain unused.<br>
\- \- Added `holylib_networking_transmit_all_weapons`<br>
\- \- Added `holylib_networking_transmit_all_weapons_to_owner`<br>
\- [#] Slightly improved memory usage & performance for UserData created by HolyLib<br>
\- [#] Updated `VoiceStream` `Load/Save` function to be able to read/write `.wav` files<br>
\- [#] Fixed `IModule::ServerActivate` not being called when being loaded as a binary module<br>
\- [#] Fixed `HolyLib:ProcessConnectionlessPacket` being called for SourceTV packets<br>
\- [#] Fixed `gameserver.SendConnectionlessPacket` crashing instead of throwing a lua error when NET_SendPacket couldn't be loaded<br>
\- [#] Fixed `HttpServer` not properly shutting down and possibly crashing<br>
\- [#] Fixed `CGameClient` & `CHLTVClient` possibly crashing when accessed after the client already disconnected<br>
\- [#] Reduced HolyLib's userdata size by 16 bytes.<br>
\- [#] Improved `bitbuf.CreateStackReadBuffer` thanks to our userdata changes making it 3x faster<br>
\- [#] Fixed possible memory leaks when using `bf_read` and `bf_write`<br>
\- [#] Tried to improve out of memory handling of `bitbuf.Create[Read/Write]Buffer` and `bf_read:ReadString()` functions<br>
\- [#] Fixed a regression with `util.FancyTableToJSON` crashing with the `0.8-pre` build when it falsely tried to become sequential while being already non-sequential. (Reported by @Noahbg)<br>
\- [#] Fixed absolute search cache causing files from any search path to be returned / destroying seperation between search paths (See https://github.com/RaphaelIT7/gmod-holylib/issues/83)<br>
\- [#] Fixed HolyLua being unable to register any metatable causing crashes when trying to use them.<br>
\- [#] Fixed some small memory leaks in HolyLibs CLuaInterface class<br>
\- [#] Fixed `steamworks.ForceAuthenticate` being silently broken<br>
\- [#] Fully seperated HolyLib's core from all modules allowing anyone to remove modules they don't want.<br>
\- [#] Removed all dependencies modules had on each other allowing each module to compile without requiring another one.<br>
\- [#] Fixed some issues in `luathreads` module that caused either crashes or simply were bugs<br>
\- [#] Fixed lua error handler used by any `CLuaInterface` created by HolyLib failing (`error in error handler`)<br>
\- [#] Moved `HolyLua` from HolyLib's core into a module to seperate it and allow anyone to remove it<br>
\- [#] Fixed some memory leaks from our own `CLuaInterface` since on shutdown they never cleared up on removal/shutdown<br>
\- [#] Added a speedup for pushing networked entities to Lua (On 64x pushing entities became 2.6x faster)<br>
\- [#] Fixed some issues where `Vector`s & `Angle`s pushed from HolyLib would be the original one instead of a copy causing issues like corruption when modified<br>
\- [-] Removed some unused code of former fixes that were implemented into Gmod<br>

You can see all changes/commits here:<br>
https://github.com/RaphaelIT7/gmod-holylib/compare/Release0.7...main

### Existing Lua API Changes
\- [+] Added third `protocolVersion` argument to `gameserver.CreateNetChannel`<br>
\- [+] Added fourth `socket`(use `NS_` enums) argument to `gameserver.CreateNetChannel` & `gameserver.SendConnectionlessPacket`<br>
\- [+] Added second and thrid arguments to `HolyLib:OnPhysicsLag` providing the entities it was working on when it triggered.<br>
\- [+] Added `voicechat.SaveVoiceStream` 4th argument `returnWaveData` (previously the 4th argument was `async` but that one was removed)<br>
\- [+] Added `directData` argument to `VoiceStream:GetData`, `VoiceStream:GetIndex`, `VoiceStream:SetIndex` and `VoiceStream:SetData`<br>
\- [+] Added overflow checks for `gameserver.BroadcastMessage`, `CNetChan:SendMessage` and `CBaseClient:SendNetMsg` when you try to use a overflowed buffer<br>
\- [+] Added a few more arguments to `HolyLib:OnPhysicsLag` like `phys1`, `phys2`, `recalcPhys`, `callerFunction` and the arguments `ent1` & `ent2` were removed since you can call `PhysObj:GetEntity`<br>
\- [#] Fixed `addonsystem.ShouldMount` & `addonsystem.SetShouldMount` `workshopID` arguments being a number when they should have been a string.<br>
\- [#] Changed `VoiceData:GetUncompressedData` to now returns a statusCode/a number on failure instead of possibly returning a garbage string.<br>
\- [#] Limited `HttpServer:SetName` to have a length limit of `64` characters.<br>
\- [#] Fixed `IGModAudioChannel:IsValid` throwing a error when it's NULL instead of returning false.<br>
\- [#] Fixed `HttpServer:SetWriteTimeout` using the wrong arguments. (See https://github.com/RaphaelIT7/gmod-holylib/pull/65)<br>
\- [#] Fixed `bf_read:ReadBytes` and `bf_read:ReadBits` both failing to push the string properly to lua.<br>
\- [#] Changed `voicechat.SaveVoiceStream` & `voicechat.LoadVoiceStream` to remove their 4th `sync` argument, if a callback is provided it will be async, else it'll run sync<br>
\- [#] Renamed `HolyLib:OnPhysFrame` to `HolyLib:PrePhysFrame`<br>
\- [-] Removed `VoiceData:GetUncompressedData` decompress size argument<br>
\- [-] Removed `CBaseClient:Transmit` third argument `fragments`.<br>
\- [-] Removed `gameserver.CalculateCPUUsage` and `gameserver.ApproximateProcessMemoryUsage` since they never worked.<br>

### QoL updates
\- [#] Changed some console message to be more consistent.<br>
\- [#] Solved a possible crash caused by a `CGameClient` disconnecting after `g_Lua` became `NULL`<br>

## ToDo

\- Finish 64x (`pvs`, `sourcetv`, `surffix`)<br>
\- Find out why ConVars are so broken. (Serverside `path` command breaks :<)<br>
\- Look into filesystem handle optimization<br>
\- Try to fix that one complex NW2 bug. NOTE: It seems to be related to baseline updates (Entity Creation/Deletion)<br>
\- Look into StaticPropMgr stuff. May be interresting.<br>
\- Add a bind to `CAI_NetworkManager::BuildNetworkGraph` or `StartRebuild`<br>
\- Possibly allow on to force workshop download on next level change.<br>
\- GO thru everything and use a more consistant codestyle. I created quiet the mess.<br>
\- test/become compatible with vphysics-jolt (I'm quite sure that the `physenv` isn't compatible).<br>
\- Check out `holylib_filesystem_predictexistance` as it seamingly broke, reportidly works in `0.6`.<br>
\- Check if gmod userdata pushed from HolyLib to Lua is invalid in the 0.7 release<br>
\- `IModule::ServerActivate` is not called when were loaded using `require("holylib")`<br>

# New Documentation
Currently I'm working on implementing a better wiki that will replace this huge readme later.<br>
It may already contain a few more details to some functions than this readme.<br>
Wiki: https://holylib.raphaelit7.com/

> [!NOTE]
> The Wiki is still in development currently, **but** most/all of the documentation is **already** done.<br>

# The Navigator<br>
[Modules](https://github.com/RaphaelIT7/gmod-holylib#modules)<br>
\- [holylib](https://github.com/RaphaelIT7/gmod-holylib#holylib-1)<br>
\- [gameevent](https://github.com/RaphaelIT7/gmod-holylib#gameevent)<br>
\- [threadpoolfix](https://github.com/RaphaelIT7/gmod-holylib#threadpoolfix)<br>
\- [precachefix](https://github.com/RaphaelIT7/gmod-holylib#precachefix)<br>
\- [stringtable](https://github.com/RaphaelIT7/gmod-holylib#stringtable)<br>
\- \- [INetworkStringTable](https://github.com/RaphaelIT7/gmod-holylib#inetworkstringtable)<br>
\- [pvs](https://github.com/RaphaelIT7/gmod-holylib#pvs)<br>
\- [surffix](https://github.com/RaphaelIT7/gmod-holylib#surffix)<br>
\- [filesystem](https://github.com/RaphaelIT7/gmod-holylib#filesystem)<br>
\- [util](https://github.com/RaphaelIT7/gmod-holylib#util)<br>
\- [concommand](https://github.com/RaphaelIT7/gmod-holylib#concommand)<br>
\- [vprof](https://github.com/RaphaelIT7/gmod-holylib#vprof)<br>
\- \- [VProfCounter](https://github.com/RaphaelIT7/gmod-holylib#vprofcounter)<br>
\- \- [VProfNode](https://github.com/RaphaelIT7/gmod-holylib#vprofnode)<br>
\- [sourcetv](https://github.com/RaphaelIT7/gmod-holylib#sourcetv)<br>
\- \- [CHLTVClient](https://github.com/RaphaelIT7/gmod-holylib#chltvclient)<br>
\- [bitbuf](https://github.com/RaphaelIT7/gmod-holylib#bitbuf)<br>
\- \- [bf_read](https://github.com/RaphaelIT7/gmod-holylib#bf_read)<br>
\- \- [bf_write](https://github.com/RaphaelIT7/gmod-holylib#bf_write)<br>
\- [networking](https://github.com/RaphaelIT7/gmod-holylib#networking)<br>
\- [steamworks](https://github.com/RaphaelIT7/gmod-holylib#steamworks)<br>
\- [systimer](https://github.com/RaphaelIT7/gmod-holylib#systimer)<br>
\- [pas](https://github.com/RaphaelIT7/gmod-holylib#pas)<br>
\- [voicechat](https://github.com/RaphaelIT7/gmod-holylib#voicechat)<br>
\- \- [VoiceData](https://github.com/RaphaelIT7/gmod-holylib#voicedata)<br>
\- [physenv](https://github.com/RaphaelIT7/gmod-holylib#physenv)<br>
\- \- [CPhysCollide](https://github.com/RaphaelIT7/gmod-holylib#cphyscollide)<br>
\- \- [CPhysPolySoup](https://github.com/RaphaelIT7/gmod-holylib#cphyspolysoup)<br>
\- \- [CPhysConvex](https://github.com/RaphaelIT7/gmod-holylib#cphysconvex)<br>
\- \- [IPhysicsCollisionSet](https://github.com/RaphaelIT7/gmod-holylib#iphysicscollisionset)<br>
\- \- [IPhysicsEnvironment](https://github.com/RaphaelIT7/gmod-holylib#iphysicsenvironment)<br>
\- [bass](https://github.com/RaphaelIT7/gmod-holylib#bass)<br>
\- \- [IGModAudioChannel](https://github.com/RaphaelIT7/gmod-holylib#igmodaudiochannel)<br>
\- [entitylist](https://github.com/RaphaelIT7/gmod-holylib#entitylist)<br>
\- [httpserver](https://github.com/RaphaelIT7/gmod-holylib#httpserver)<br>
\- \- [HttpServer](https://github.com/RaphaelIT7/gmod-holylib#httpserver)<br>
\- \- [HttpRequest](https://github.com/RaphaelIT7/gmod-holylib#httprequest)<br>
\- \- [HttpResponse](https://github.com/RaphaelIT7/gmod-holylib#httpresponse)<br>
\- [luajit](https://github.com/RaphaelIT7/gmod-holylib#luajit)<br>
\- [gameserver](https://github.com/RaphaelIT7/gmod-holylib#gameserver)<br>
\- \- [CBaseClient](https://github.com/RaphaelIT7/gmod-holylib#cbaseclient)<br>
\- \- [CGameClient](https://github.com/RaphaelIT7/gmod-holylib#cgameclient)<br>
\- \- [Singleplayer](https://github.com/RaphaelIT7/gmod-holylib#singleplayer)<br>
\- \- [128+ Players](https://github.com/RaphaelIT7/gmod-holylib#128-players)<br>
\- [autorefresh](https://github.com/RaphaelIT7/gmod-holylib#autorefresh)<br>
\- [soundscape](https://github.com/RaphaelIT7/gmod-holylib#soundscape)<br>
\- [networkthreading](https://github.com/RaphaelIT7/gmod-holylib#networkthreading)<br>

[Unfinished Modules](https://github.com/RaphaelIT7/gmod-holylib#unfinished-modules)<br>
\- [serverplugins](https://github.com/RaphaelIT7/gmod-holylib#serverplugins)<br>

[Issues implemented / fixed](https://github.com/RaphaelIT7/gmod-holylib/edit/main/README.md#issues-implemented--fixed)<br>
[Some things for later](https://github.com/RaphaelIT7/gmod-holylib/edit/main/README.md#some-things-for-later)<br>

# Possible Issues

## Models have prediction errors/don't load properly
This is most likely cause by the filesystem prediction.<br>
You can use `holylib_filesystem_showpredictionerrors` to see any predictions that failed.<br>
You can solve this by setting `holylib_filesystem_predictexistance 0`.<br>
The convar was disabled by default now because of this.<br>

## HolyLib internally caches the TValue's of metatables
This is like keeping a reference though faster but also more unsafe.<br>
If a metatable is replaced with another table / the GC Object internally changes<br>
HolyLib will most definetly experience issues / crashes<br>
though generally this should never happen unless you touch the Lua registry yourself<br>

## Compatibility
HolyLib should be compatible with most other binary modules<br>
though there are some cases where they conflict like if their detouring the same function<br>
Other binary modules can check if HolyLib is loaded by checking if the command line option `-holylibexists` exists<br>
C++: `CommandLine()->FindParm("-holylibexists")`<br>

Setting/Adding `-holylibexists` to the command line yourself will result in HolyLib refusing to load!<br>
This is to prevent HolyLib from loading multiple times if multiple versions exist / in cases where multiple DLLs include HolyLib<br>

# Modules
Each module has its own convar `holylib_enable_[module name]` which allows you to enable/disable specific modules.<br>
You can add `-holylib_enable_[module name] 0` to the startup to disable modules on startup.<br>

Every module also has his `holylib_debug_[module name]` version and command line option like above, but not all modules use it.<br>
The modules that use that convar have it listed in their ConVars chapter.<br>
All hooks that get called use `hook.Run`. Gmod calls `gamemode.Call`.<br>

## Debug Stuff
There is `-holylib_startdisabled` which will cause all modules to be disabled on startup.<br>
And with `holylib_toggledetour` you can block specific detours from being created.<br>

## holylib
This module contains the HolyLib library.<br> 

Supports: Linux32 | LINUX64<br>

### Functions

#### bool HolyLib.Reconnect(Player ply)
Player ply - The Player to reconnect.<br>

Returns `true` if the player was successfully reconnected.<br>

#### HolyLib.HideServer(bool hide)
bool hide - `true` to hide the server from the serverlist.<br>
Will just set the `hide_server` convar in the future.<br>

#### HolyLib.FadeClientVolume(Player ply, number fadePercent, number fadeOutSeconds, number holdTime, number fadeInSeconds)
Fades out the clients volume.<br>
Internally just runs `soundfade` with the given settings on the client.<br>
A direct engine bind to `IVEngineServer::FadeClientVolume`<br>

#### HolyLib.ServerExecute()
Forces all queried commands to be executed/processed.<br>
A direct engine bind to `IVEngineServer::ServerExecute`<br>

#### bool HolyLib.IsMapValid(string mapName)
Returns `true` if the given map is valid.<br>

#### bf_write HolyLib.EntityMessageBegin(Entity ent, bool reliable)
Allows you to create an entity message.<br>
A direct engine bind to `IVEngineServer::EntityMessageBegin`<br>

> [!NOTE]
> If the `bitbuf` module is disabled, it will throw a lua error!

#### bf_write HolyLib.UserMessageBegin(IRecipientFilter filter, string usermsg)
Allows you to create any registered usermessage.<br>
A direct engine bind to `IVEngineServer::UserMessageBegin`<br>

> [!NOTE]
> If the `bitbuf` module is disabled, it will throw a lua error!

#### HolyLib.MessageEnd()
Finishes the active Entity/Usermessage.<br>
If you don't call this, the message won't be sent! (And the engine might throw a tantrum)<br>
A direct engine bind to `IVEngineServer::MessageEnd`<br>

#### HolyLib.InvalidateBoneCache(Entity ent)
Invalidates the bone cache of the given entity.<br>

> [!NOTE]
> Only uses this on Entities that are Animated / Inherit the CBaseAnimating class. Or else it will throw a Lua error.<br>

#### bool HolyLib.SetSignOnState(Player ply / number userid, number signOnState, number spawnCount = 0, bool rawSet = false)
Sets the SignOnState for the given client.<br>
Returns `true` on success.<br>

> [!NOTE]
> This function does normally **not** directly set the SignOnState.<br>
> Instead it calls the responsible function for the given SignOnState like for `SIGNONSTATE_PRESPAWN` it will call `SpawnPlayer` on the client.<br>
> Set the `rawSet` to `true` if you want to **directly** set the SignOnState.<br><br>

#### (Experimental - 32x safe only) HolyLib.ExitLadder(Player ply)
Forces the player off the ladder.<br>

#### (Experimental - 32x safe only) Entity HolyLib.GetLadder(Player ply)
Returns the Ladder the player is currently on.<br>

#### table HolyLib.GetRegistry()
Returns the lua regirsty.<br>
Same like [debug.getregistry()](https://wiki.facepunch.com/gmod/debug.getregistry) before it was nuked.<br>

#### bool HolyLib.Disconnect(Player ply / number userid, string reason, bool silent = false, bool nogameevent = false)
silent - Silently closes the net channel and sends no disconnect to the client.<br>
nogameevent - Stops the `player_disconnect` event from being created.<br>
Disconnects the given player from the server.<br>

> [!NOTE]
> Unlike Gmod's version which internally calls the `kickid` command, we directly call the `Disconnect` function with no delay.<br>

#### HolyLib.ReceiveClientMessage(number userID, Entity ent, bf_read buffer, number bits)
`bits` - should always be the value of `bf_read:GetNumBits()`<br>
`ent` - The entity to use as the sender, should be a player.<br>

Allows you to fake client messages.<br>
Internally its a direct binding to `IServerGameClients::GMOD_ReceiveClientMessage`<br>

Example of faking a net message:<br>
```lua
net.Receive("Example", function(len, ply)
<br><br>print("Received example message: " .. tostring(ply) .. " (" .. len .. ")")
<br><br>print("Message contained: " .. net.ReadString())
end)

local bf = bitbuf.CreateWriteBuffer(64)
bf:WriteUBitLong(0, 8) -- The message type. 0 = Lua net message

bf:WriteUBitLong(util.AddNetworkString("Example"), 16) -- Header for net.ReadHeader
bf:WriteString("Hello World") -- Message content

local readBF = bitbuf.CreateReadBuffer(bf:GetData()) -- Make it a read buffer.
local entity = Entity(0) -- We can use the world but normally we shouldn't.
local userID = entity:IsPlayer() and entity:UserID() or -1
HolyLib.ReceiveClientMessage(userID, entity, readBF, readBF:GetNumBits())
```

### Hooks

#### string HolyLib:GetGModTags()
Allows you to override the server tags.<br>
Return nothing / nil to not override them.<br>

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
Called before `CBaseEntity::PostConstructor` is called.<br>
This should allow you to set the `EFL_SERVER_ONLY` flag properly.<br>

> [!NOTE]
> This may currently not work.

#### HolyLib:OnPlayerGotOnLadder(Entity ladder, Entity ply)
Called when a gets onto a ladder -> Direct bind to `CFuncLadder::PlayerGotOn`<br>

#### HolyLib:OnPlayerGotOffLadder(Entity ladder, Entity ply)
Called when a gets off a ladder -> Direct bind to `CFuncLadder::PlayerGotOff`<br>

#### HolyLib:OnMoveTypeChange(Entity ent, number old_moveType, number new_moveType, number moveCollide)
Called when the movetype is about to change.<br>
If you call `Entity:SetMoveType` on the current entity inside this hook, it would have no effect.<br>

#### HolyLib:OnMapChange(string levelName, string landmarkName)
> [!NOTE]
> This currently works only `LINUX32`

Called right before a level transition occurs, this hook allows you to react to map changes initiated via changelevel.
- string levelName — The name of the level we are changing to.
- string levelLandmark — The name of the landmark (if any) otherwise, an empty string.
Example usage:
```lua
hook.Add("HolyLib:OnMapChange", "HelloThere", function(levelName, landmarkName)
    print("Next level name: " .. levelName)
    print("Using landmark: " .. landmarkName) 
end)
```

## gameevent
This module contains additional functions for the gameevent library.<br>
With the Add/Get/RemoveClient* functions you can control the gameevents that are networked to a client which can be useful.<br>

Supports: Linux32 | LINUX64<br>

### Functions

#### (number or table) gameevent.GetListeners(string name)
string name(optional) - The event to return the count of listeners for.<br>
If name is not a string, it will return a table containing all events and their listener count:<br>
```lua
{
	["player_spawn"] = 1
}
```

> [!NOTE]
> Can return `nil` on failure.<br>

#### bool gameevent.RemoveListener(string name)
string name - The event to remove the Lua gameevent listener from.<br>

Returns `true` if the listener was successfully removed from the given event.

#### table gameevent.GetClientListeners(Player ply or nil)
Returns a table containing all gameevents the given client is listening to.<br>
If not given a player, it will return a table containing all players and the gameevent they're listening to.<br>

#### bool gameevent.RemoveClientListener(Player ply, string eventName or nil)
Removes the player from listening to the given gameevent.<br>
If the given gameevent is `nil` it will remove the player from all gameevents.<br>

Returns `true` on success.<br>

#### bool (**Disabled**)gameevent.AddClientListener(Player ply, String eventName)
Adds the given player to listen to the given event.<br>

Returns `true` on success.<br>

#### IGameEvent gameevent.Create(String eventName, bool force = false)
Creates the given gameevent.<br>
Can return `nil` on failure.<br>

#### bool gameevent.FireEvent(IGameEvent event, bool bDontBroadcast = false)
Fires the given gameevent.<br>
If `bDontBroadcast` is specified, it won't be networked to players.<br>

#### bool gameevent.FireClientEvent(IGameEvent event, Player ply)
Fires the given event for only the given player.<br>

#### IGameEvent gameevent.DuplicateEvent(IGameEvent event)
Duplicates the given event.<br>

#### gameevent.BlockCreation(string name, bool block)
Blocks/Unblocks the creation of the given gameevent.<br> 

### IGameEvent

#### string IGameEvent:\_\_tostring()
Returns the a formated string.<br>
Format: `IGameEvent [%s]`<br>
`%s` -> Gameevent name

#### IGameEvent:\_\_gc()
Deletes the gameevent internally.<br>

#### bool IGameEvent:IsValid()
Returns `true` if the gameevent is valid.<br>

#### bool IGameEvent:IsEmpty()
Returns `true` if the gameevent is empty.<br>

#### bool IGameEvent:IsReliable()
Returns `true` if the gameevent is reliable.<br>

#### bool IGameEvent:IsLocal()
Returns `true` if the gameevent is only local/serverside.<br>
This means it won't be networked to players.<br>

#### string IGameEvent:GetName()
Returns the gameevent name.<br>

#### bool IGameEvent:GetBool(string key, bool fallback)
Returns the bool for the given key and if not found it returns the fallback.<br>

#### number IGameEvent:GetInt(string key, number fallback)
Returns the int for the given key and if not found it returns the fallback.<br>

#### number IGameEvent:GetFloat(string key, number fallback)
Returns the float for the given key and if not found it returns the fallback.<br>

#### string IGameEvent:GetString(string key, string fallback)
Returns the string for the given key and if not found it returns the fallback.<br>

#### IGameEvent:SetBool(string key, bool value)
Sets the bool for the given key.<br>

#### IGameEvent:SetInt(string key, number value)
Sets the int for the given key.<br>

#### IGameEvent:SetFloat(string key, number value)
Sets the float for the given key.<br>

#### IGameEvent:SetString(string key, string value)
Sets the string for the given key.<br>

### Hooks

#### bool HolyLib:PreProcessGameEvent(Player ply, table gameEvents, number plyIndex)
Called when the client sends the gameevent list it wants to listen to.<br>
Return `true` to stop the engine from future processing this list.<br>

> [!NOTE]
> This and the hook below may be called with a NULL player.<br>
> it is caused by the player registering the gameevents before he spawned/got an entity.<br>
> Because of this, the third argument was added.<br>

#### HolyLib:PostProcessGameEvent(Player ply, table gameEvents, number plyIndex)
Called after the engine processed the received gameevent list.<br>

### ConVars

#### holylib_gameevent_callhook (default `1`)
If enabled, it will call the gameevent hooks.<br>

#### holylib_debug_gameevent (default `0`)
My debug stuff :> It'll never be important for you.<br>

## threadpoolfix
This module modifies `CThreadPool::ExecuteToPriority` to not call `CThreadPool::SuspendExecution` when it doesn't have any jobs.<br>
This is a huge speed improvement for adding searchpaths / mounting legacy addons.<br>
> [!NOTE]
> This requires the `ghostinj.dll` to be installed!

Supports: Linux32 | LINUX64<br>

> [!NOTE]
> This currently does nothing since all fixes were added to Garry's Mod itself.<br>

## precachefix
This module removes the host error when the model or generic precache stringtable overflows.<br>
Instead it will throw a warning.<br>

If these stringtables overflow, expect the models that failed to precache to be an error.<br>

Supports: Linux32 | LINUX64<br>

### Hooks

#### HolyLib:OnModelPrecache(string model, number idx)
string model - The model that failed to precache.<br>
number idx - The index the model was precache in.<br>

#### number HolyLib:OnModelPrecacheFail(string model)
string model - The model that failed to precache.<br>
Return a index number to let the engine fallback to that model or return `nil` to just let it become an error model.<br>

#### HolyLib:OnGenericPrecache(string file, number idx)
string file - The file that failed to precache.<br>
number idx - The index the file was precache in.<br>

#### number HolyLib:OnGenericPrecacheFail(string file)
string file - The file that failed to precache.<br>
Return a index number to let the engine fallback to that generic or return `nil` to just let it be.<br>
Idk if it's a good Idea to play with it's fallback.<br>

### ConVars

#### holylib_precache_modelfallback(default `-1`)
The fallback index if a model fails to precache.<br>
`-1` = Error Model<br>
`0` = Invisible Model<br>

#### holylib_precache_genericfallback(default `-1`)
The fallback index if a generic fails to precache.<br>

## stringtable
This module adds a new library called `stringtable`, which will contain all functions to handle stringtables,<br>
and it will a hook for when the stringtables are created, since they are created while Lua was already loaded.<br>

Supports: Linux32 | LINUX64<br>

> [!NOTE]
> For now, all functions below are just a bind to their C++ versions -> [INetworkStringTable](https://github.com/RaphaelIT7/obsolete-source-engine/blob/gmod/public/networkstringtabledefs.h#L32)<br>

### Functions

#### INetworkStringTable stringtable.CreateStringTable(string tablename, number maxentries, number userdatafixedsize = 0, number userdatanetworkbits = 0)
string tablename - The name for the stringtable we want to create.<br>
number maxentries - The maximal amount of entries.<br>
number userdatafixedsize(default `0`) - The size of the userdata.<br>
number userdatanetworkbits(default `0`) - The networkbits to use for the userdata.<br>

Returns `nil` or a `INetworkStringTable`.<br>

#### stringtable.RemoveAllTables()
Nuke all stringtables. BOOOM<br>

#### INetworkStringTable stringtable.FindTable(string tablename)
string tablename - The table to search for<br>

Returns `nil` or the `INetworkStringTable`<br>

#### INetworkStringTable stringtable.GetTable(number tableid)
number tableid - The tableid of the table to get<br>

Returns `nil` or the `INetworkStringTable`<br>

#### number stringtable.GetNumTables()
Returns the number of stringtables that exist.<br>

#### INetworkStringTable stringtable.CreateStringTableEx(string tablename, number maxentries, number userdatafixedsize = 0, number userdatanetworkbits = 0, bool bIsFilenames = false )
string tablename - The name for the stringtable we want to create.<br>
number maxentries - The maximal amount of entries.<br>
number userdatafixedsize(default `0`) - The size of the userdata.<br>
number userdatanetworkbits(default `0`) - The networkbits to use for the userdata.<br>
bool bIsFilenames(default `false`) - If the stringtable will contain filenames.<br>

#### stringtable.SetAllowClientSideAddString(INetworkStringTable table, bool bAllowClientSideAddString = false)
Allows/Denies the client from adding additional strings to the table clientside.<br>

#### bool stringtable.IsCreationAllowed()
Returns whether you're allowed to create stringtables.<br>

#### bool stringtable.IsLocked()
Returns if all stringtables are locked or not.<br>

#### stringtable.SetLocked(bool bLocked = false)
Locks or Unlocks all stringtables.<br>

#### stringtable.AllowCreation(bool bAllowCreation = false)
Sets whether you're allowed to create stringtable.<br>

Example:<br>
```lua
stringtable.AllowCreation(true)
stringtable.CreateStringTable("example", 8192)
stringtable.AllowCreation(false)
```

> [!NOTE]
> Please use the `HolyLib:OnStringTableCreation` hook to add custom stringtables.<br>

#### stringtable.RemoveTable(INetworkStringTable table)
Deletes that specific stringtable.<br>

> [!WARNING]
> Deleting a stringtable is **NOT** networked!<br>
> In addition, it might be unsafe and result in crashes when it's a engine created stringtable.<br>
> Generally, only use it when you **KNOW** what your doing.<br>

### INetworkStringTable
This is a metatable that is pushed by this module. It contains the functions listed below<br>

#### string INetworkStringTable:\_\_tostring()
Returns `INetworkStringTable [NULL]` if given a invalid table.<br>
Normally returns `INetworkStringTable [tableName]`.<br>

#### INetworkStringTable:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any INetworkStringTable:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `INetworkStringTable:GetTable()`.<br>

#### table INetworkStringTable:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### string INetworkStringTable:GetTableName() 
Returns the name of the stringtable<br>

#### number INetworkStringTable:GetTableId() 
Returns the id of the stringtable<br>

#### number INetworkStringTable:GetNumStrings() 
Returns the number of strings this stringtable has<br>

#### number INetworkStringTable:GetMaxStrings() 
Returns the maximum number of string this stringtable has<br>

#### number INetworkStringTable:GetEntryBits() 
ToDo: I have no idea<br>

#### INetworkStringTable:SetTick(number tick) 
number tick - The tick to set the stringtable to<br>

The current tick to used for networking

#### bool INetworkStringTable:ChangedSinceTick(number tick) 
number tick - The tick to set the stringtable to<br>

Returns whether or not the stringtable changed since that tick.<br>

#### number INetworkStringTable:AddString(string value, bool isServer = true) 
string value - The string to add<br>
bool isServer - Weather or not the server is adding this value? (Idk, added it so you have more control.)<br>

Returns the index of the added string.<br>

#### string INetworkStringTable:GetString(number index) 
number index - The index to get the string from<br>

Returns the string from that index<br>

#### table INetworkStringTable:GetAllStrings() 
Returns a table containing all the strings.<br>

#### number INetworkStringTable:FindStringIndex(string value) 
string value - The string to find the index of<br>

Returns the index of the given string.<br>

#### INetworkStringTable:DeleteAllStrings(bool nukePrecache = false)
Deletes all strings from the stringtable.<br>
If `nukePrecache` is `true`, it will remove all precache data for the given stringtable.<br>

#### INetworkStringTable:SetMaxEntries(number maxEntries)
number maxEntries - The new limit for entries.<br>

Sets the new Entry limit for that stringtable.<br>

The new limit will work but for some stringtables, it might cause issues, where the limits are hardcoded clientside.<br>
A list of known stringtables with hardcoded limits:<br>
- `modelprecache` -> Limited by `CClientState::model_precache`<br>
- `genericprecache` -> Limited by `CClientState::generic_precache`<br>
- `soundprecache` -> Limited by `CClientState::sound_precache`<br>
- `decalprecache` -> Limited by `CClientState::decal_precache`<br>
- `networkvars` -> Limited by the internal net message used.<br>

> [!NOTE]
> If there are already more entries than the new limit, they won't be removed.<br>
> (This could cause issues, so make sure you know what you're doing.)<br>

#### bool INetworkStringTable:DeleteString(number index)
Deletes the given string at the given index.<br>

Returns `true` if the string was deleted.<br>

> [!NOTE]
> Currently this deletes all strings and readds all except the one at the given index. This is slow and I need to improve it later.<br>
> This also removes the precache data if you delete something from `modelprecache` or so.<br>

#### bool INetworkStringTable:IsValid()
Returns `true` if the stringtable is still valid.<br>

#### INetworkStringTable:SetStringUserData(number index, string data, number length = NULL)
Sets the UserData for the given index.<br>
`length` is optional and should normally not be used. It could cause crashes is not used correctly!<br>

How this could be useful:<br>
```lua
local dataTablePaths = {
	"/lua/", -- The order is also important!
	-- Add paths that are frequently used to the top so there the first one to be checked for a file.<br>
	-- This will improve the performance because it will reduce the number of paths it checks for a file.

	"/addons/exampleaddon/lua/", -- Each legacy addon that has a lua folder has it's path.<br>
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
Returns the userdata of the given index.<br>
Second return value is the length/size of the data.<br>

#### INetworkStringTable:SetNumberUserData(number index, number value)
Sets the number as userdata for the given string.<br>

#### number INetworkStringTable:GetNumberUserData(number index)
Returns the userdata of the given index. It needs to be a int!<br>

#### INetworkStringTable:SetPrecacheUserData(number index, number value)
Sets the `CPrecacheUserData` and sets the flags of it to the given value.<br>

#### number INetworkStringTable:GetPrecacheUserData(number index)
Returns the flags of `CPrecacheUserData`.<br>

#### bool INetworkStringTable:IsClientSideAddStringAllowed()
Returns `true` if the client is allowed to add strings to this table.<br>

#### INetworkStringTable:SetAllowClientSideAddString(bool bAllowClientSideAddString = false)
Allows/Denies the client from adding additional strings to the table clientside.<br>

> [!NOTE]
> This is a direct alias to `stringtable.SetAllowClientSideAddString`

#### bool INetworkStringTable:IsLocked()
Returns if this specific table is locked or not.<br>

#### INetworkStringTable:SetLocked(bool bLocked = false)
Locks or Unlocks this specific table.<br>

#### bool INetworkStringTable:IsFilenames()
Returns `true` if the table stores filenames.<br>

> [!NOTE]
> This value can only be set when creating the table using `stringtable.CreateStringTableEx` for creation.

### Enums
This module adds these enums<br>

#### number stringtable.INVALID_STRING_INDEX
This value is returned if the index of a string is invalid, like if INetworkStringTable:AddString fails.<br>

### Hooks
This module calls these hooks from (`hook.Run`)<br>

#### HolyLib:OnStringTableCreation()
You can create stringtables inside this hook.<br>
If you want to create stringtables outside this hook, use `stringtable.AllowCreation`<br>

## pvs
This adds a bunch of PVS related functions.<br>
Supports: Linux32<br>

### Functions

#### pvs.ResetPVS()
Resets the current PVS.<br>

#### bool pvs.CheckOriginInPVS(Vector vec)
Checks if the given position is inside the current PVS.<br>

#### pvs.AddOriginToPVS(Vector vec)
Adds the given Origin to the PVS. Gmod already has this function.<br>

#### number pvs.GetClusterCount()
Returns the number of clusters that exist.<br>

#### number pvs.GetClusterForOrigin(Vector vec)
Returns the cluster id of the given origin.<br>

#### bool pvs.CheckAreasConnected(number area1, number area2)
Returns whether or not the given areas are connected.<br>
We don't validate if you pass valid areas!<br>

#### number pvs.GetArea(Vector vec)
Returns the area id of the given origin.<br>

#### pvs.GetPVSForCluster(number clusterID)
Sets the current PVS to that of the given cluster.<br>
We don't validate if the passed cluster id is valid!<br>

#### bool pvs.CheckBoxInPVS(Vector mins, Vector maxs)
Returns whether or not the given box is inside the PVS.<br>

#### pvs.AddEntityToPVS(Entity ent / table ents / EntityList list)
Adds the given entity to the PVS<br>

#### pvs.OverrideStateFlags(Entity ent / table ents / EntityList list, number flags, bool engine)
table ents - A sequential table containing all the entities which states flags should be overridden.<br>
bool engine - Allows you to set the edict flags directly. It won't make sure that the value is correct!<br>
Overrides the StateFlag for this Snapshot.<br>
The value will be reset in the next one.<br>
> [!NOTE]
> If you use engine, you should know what your doing or else it might cause a crash.<br>

#### pvs.SetStateFlags(Entity ent / table ents / EntityList list, number flags, bool engine)
table ents - A sequential table containing all the entities which states should be set.<br>
bool engine - Allows you to set the edict flags directly. It won't make sure that the value is correct!<br>
Sets the State flag for this entity.<br>
Unlike `OverrideStateFlag`, this won't be reset after the snapshot.<br>
> [!NOTE]
> If you use engine, you should know what your doing or else it might cause a crash.<br>

#### number/table pvs.GetStateFlags(Entity ent / table ents / EntityList list, bool engine)
bool engine - Allows you to get all edict flags instead of only the ones for networking.<br>
Returns the state flags for this entity.<br>

#### bool pvs.RemoveEntityFromTransmit(Entity ent / table ents / EntityList list)
table ents - A sequential table containing all the entities that should be removed from being transmitted.<br>
Returns true if the entity or all entities were successfully removed from being transmitted.<br>

> [!NOTE]
> Only use this function inside the `HolyLib:[Pre/Post]CheckTransmit` hook!<br>

#### pvs.RemoveAllEntityFromTransmit()
Removes all Entities from being transmitted.<br>

> [!NOTE]
> Only use this function inside the `HolyLib:[Pre/Post]CheckTransmit` hook!<br>

#### pvs.AddEntityToTransmit(Entity ent / table ents / EntityList list, bool always)
table ents - A sequential table containing all the entities that should be transmitted.<br>
bool always - If the entity should always be transmitted? (Verify)<br>

Adds the given Entity to be transmitted.

> [!NOTE]
> Only use this function inside the `HolyLib:[Pre/Post]CheckTransmit` hook!<br>

#### (REMOVED AGAIN) pvs.IsEmptyBaseline()
Returns `true` if the baseline is empty.<br>
This should always be the case after a full update.<br>

> [!NOTE]
> Only use this function inside the `HolyLib:[Pre/Post]CheckTransmit` hook!<br>

> [!IMPORTANT] 
> This function was removed since I can't get it to work / It would be a bit more complicated than first anticipated.<br>

#### pvs.SetPreventTransmitBulk(Entity ent / table ents / EntityList list, Player ply / table plys / RecipientFilter filter, bool notransmit)
table ents - A sequential table containing all the entities that should be affected.<br>
table plys - A sequential table containing all the players that it should set it for.<br>
bool notransmit - If the entity should stop being transmitted.<br>

Adds the given Entity to be transmitted.<br>

#### bool / table pvs.TestPVS(Entity ent / Vector origin, Entity ent / Vector pos / EntityList list)
Returns `true` if the given entity / position is inside the PVS of the given origin.<br>
If given a EntityList, it will return a table wich contains the result for each entity.<br>
The key will be the entity and the value is the result.<br>

#### table pvs.FindInPVS(Entity ent / Vector pos)
Returns a table containing all entities that are inside the pvs.<br>

#### pvs.ForceFullUpdate(Player ply)
Forces a full update for the specific client.<br>

#### table pvs.GetEntitesFromTransmit()
Returns a table containing all entities that will be networked.<br>

### Enums

#### pvs.FL_EDICT_DONTSEND = 1<br>
The Entity won't be networked.<br>

#### pvs.FL_EDICT_ALWAYS = 2
The Entity will always be networked.<br>

#### pvs.FL_EDICT_PVSCHECK = 4
The Entity will only be networked if it's inside the PVS.<br>

#### pvs.FL_EDICT_FULLCHECK = 8
The Entity's `ShouldTransmit` function will be called, and its return value will be used.<br>

### Hooks

#### bool HolyLib:PreCheckTransmit(Entity ply)
Called before the transmit checks are done.<br>
Return `true` to cancel it.<br>

You could do the transmit stuff yourself inside this hook.<br>

#### HolyLib:PostCheckTransmit(Entity ply)
entity ply - The player that everything is transmitted to.<br>

## surffix
This module ports over [Momentum Mod's](https://github.com/momentum-mod/game/blob/develop/mp/src/game/shared/momentum/mom_gamemovement.cpp#L2393-L2993) surf fixes.<br>

Supports: Linux32<br>

### ConVars

#### sv_ramp_fix (default `1`)
If enabled, it will enable additional checks to make sure that the player is not stuck in a ramp.<br>

#### sv_ramp_initial_retrace_length (default `0.2`, max `5`)
Amount of units used in offset for retraces<br>

#### sv_ramp_bumpcount (default `8`, max `32`)
Helps with fixing surf/ramp bugs<br>

## filesystem
This module contains multiple optimizations for the filesystem and a lua library.<br>
Supports: Linux32 | Windows32<br>

> [!NOTE]
> On Windows32 it will only add the Lua functions.<br>
> The optimizations aren't implemented yet.<br>

### Functions
This module also adds a `filesystem` library which should generally be faster than gmod's functions, because gmod has some weird / slow things in them.<br>
It also gives you full access to the filesystem and doesn't restrict you to specific directories.<br>

#### (FSASYNC Enum) filesystem.AsyncRead(string fileName, string gamePath, function callback(string fileName, string gamePath, FSASYNC status, string content), bool sync)
Reads a file async and calls the callback with the contents.<br>

#### filesystem.CreateDir(string dirName, string gamePath = "DATA")
Creates a directory in the given path.<br>

#### filesystem.Delete(string fileName, string gamePath = "DATA")
Deletes the given file.<br>

#### bool filesystem.Exists(string fileName, string gamePath)
Returns `true` if the given file exists.<br>

#### table(Files), table(Folders) filesystem.Find(string filePath, string gamePath, string sorting = "nameasc")
Finds and returns a table containing all files and folders in the given path.<br>

#### bool filesystem.IsDir(string fileName, string gamePath)
Returns `true` if the given file is a directory.<br>

#### File filesystem.Open(string fileName, string fileMode, string gamePath = "GAME")
Opens the given file or returns `nil` on failure.<br>

#### bool filesystem.Rename(string oldFileName, string newFileName, string gamePath = "DATA")
Renames the given file and returns `true` on success.<br>

#### number filesystem.Size(string fileName, string gamePath = "GAME")
Returns the size of the given file.<br>

#### number filesystem.Time(string fileName, string gamePath = "GAME")
Returns the unix time of the last modification of the given file / folder.<br>

#### filesystem.AddSearchPath(string folderPath, string gamePath, bool addToTail = false)
Adds the given folderPath to the gamePath searchpaths.<br>

#### bool filesystem.RemoveSearchPath(string folderPath, string gamePath)
Removes the given searchpath and returns `true` on success.<br>

#### filesystem.RemoveSearchPaths(string gamePath)
Removes all searchpaths with that gamePath<br>

Example:<br>
`filesystem.RemoveSearchPaths("GAME")` -- Removes all `GAME` searchpaths.<br>

#### filesystem.RemoveAllSearchPaths()
Removes all searchpaths.<br>

#### string filesystem.RelativePathToFullPath(string filePath, string gamePath)
Returns the full path for the given file or `nil` on failure.<br>

#### string filesystem.FullPathToRelativePath(string fullPath, string gamePath = nil)
Returns the relative path for the given file.<br>

#### number filesystem.TimeCreated(string fileName, string gamePath = "GAME")
Returns the time the given file was created.<br>
Will return `0` if the file wasn't found.<br>

#### number filesystem.TimeAccessed(string fileName, string gamePath = "GAME")
Returns the time the given file was last accessed.<br>
Will return `0` if the file wasn't found.<br>

### ConVars

#### holylib_filesystem_easydircheck (default `0`)
If enabled, it will check if the file contains a `.` after the last `/`.<br>
If so it will cause `CBaseFileSystem::IsDirectory` to return false since we assume it's a file.<br>
This will cause `file.IsDir` to fail on folders with names like these `test/test1.23`.<br>

#### holylib_filesystem_searchcache (default `1`)
If enabled, it will cause the filesystem to use a cache for the searchpaths.<br>
When you try to open a file with a path like `GAME` which has multiple searchpaths, it will check each one until its found.<br>
Now, the first time it searches for it, if it finds it, we add the file and the searchpath to a cache and the next time the same file is searched for, we try to use our cache search path.<br>

This will improve `file.Open`, `file.Time` and `file.Exists`.<br>
The more searchpaths exist, the bigger the improvement for that path will be.<br>
Example (I got 101 legacy addons):<br>
```
lua_run local a = SysTime() for k=1, 1000 do file.Exists("garrysmod.ver", "GAME") end print(SysTime()-a)

Disabled: 1.907318733
Enabled: 0.035948700999995
```

You also can test it using the `MOD` path.<br>
The performance of `file.Exists` for any search path and `MOD` should be somewhat near each other since, it checks the same amount of searchpaths while this is enabled.<br>
```
lua_run local a = SysTime() for k=1, 1000 do file.Exists("garrysmod.ver", "GAME") end print(SysTime()-a)
0.033513544999998

lua_run local a = SysTime() for k=1, 1000 do file.Exists("garrysmod.ver", "MOD") end print(SysTime()-a)
0.037827891999996
```
##### NOTES
- If the file doesn't exist, it will still go thru all search paths to search for it again!<br>
- I don't know if this has any bugs, but while using this for ~1 Month on a server, I didn't find any issues.<br>
- It will also improve the `MOD` search path since it also has multiple search paths.<br>

#### holylib_filesystem_earlysearchcache (default `1`)
If enabled, it will check the searchcache inside `CBaseFileSystem::OpenForRead`.<br>

#### holylib_filesystem_forcepath (default `1`)
If enabled, it will force the pathID for specific files.<br>

#### holylib_filesystem_splitgamepath (default `1`)
If enabled, it will split each `GAME` path into multiple search paths, depending on it's contents.<br>
Then when you try to find a file with the `GAME` search path, it will change the pathID to the content path.<br>

Example:<br>
File: `cfg/game.cfg`<br>
Path: `GAME`<br>
becomes:<br>
File: `cfg/game.cfg`<br>
Path: `CONTENT_CONFIGS`<br><br>

This will reduce the amount of searchpaths it has to go through which improves performance.<br>

Content paths:<br>
- `materials/` -> `CONTENT_MATERIALS`<br>
- `models/` -> `CONTENT_MODELS`<br>
- `sound/` -> `CONTENT_SOUNDS`<br>
- `maps/` -> `CONTENT_MAPS`<br>
- `resource/` -> `CONTENT_RESOURCE`<br>
- `scripts/` -> `CONTENT_SCRIPTS`<br>
- `cfg/` -> `CONTENT_CONFIGS`<br>
- `gamemodes/` -> `LUA_GAMEMODES`<br>
- `lua/includes/` -> `LUA_INCLUDES`<br>

#### holylib_filesystem_splitluapath (default `0`)
Does the same for `lsv` to save performance.<br>

> BUG: This currently breaks workshop addons.<br>

Lua paths:<br>
- `sandbox/` -> `LUA_GAMEMODE_SANDBOX`<br>
- `effects/` -> `LUA_EFFECTS`<br>
- `entities/` -> `LUA_ENTITIES`<br>
- `weapons/` -> `LUA_WEAPONS`<br>
- `lua/derma/` -> `LUA_DERMA`<br>
- `lua/drive/` -> `LUA_DRIVE`<br>
- `lua/entities/` -> `LUA_LUA_ENTITIES`<br>
- `vgui/` -> `LUA_VGUI`<br>
- `postprocess/` -> `LUA_POSTPROCESS`<br>
- `matproxy/` -> `LUA_MATPROXY`<br>
- `autorun/` -> `LUA_AUTORUN`<br>

#### holylib_filesystem_splitfallback (default `1`)
If enabled, it will fallback to the original searchpath if it failed to find something in the split path.<br>
This is quite slow, so disabling this will improve performance to find files that doesn't exist.<br>

#### holylib_filesystem_predictpath (default `1`)
If enabled, it will try to predict the path for a file.<br>

Example:<br>
Your loading a model.<br>
First you load the `example.mdl` file.<br>
Then you load the `example.phy` file.<br> 
Here we can check if the `example.mdl` file is in the searchcache.<br>
If so, we try to use the searchpath of that file for the `.phy` file and since all model files should be in the same folder, this will work for most cases.<br>
If we fail to predict a path, it will end up using one additional search path.<br>

#### holylib_filesystem_predictexistance (default `0`)
If enabled, it will try to predict the path of a file, but if the file doesn't exist in the predicted path, we'll just say it doesn't exist.<br>
Doesn't rely on `holylib_filesystem_predictpath` but it also works with it together.<br>

The logic for prediction is exactly the same as `holylib_filesystem_predictpath` but it will just stop checking if it doesn't find a file in the predicted path instead of checking then all other searchpaths.<br>

#### holylib_filesystem_fixgmodpath (default `1`)
If enabled, it will fix up weird gamemode paths like sandbox/gamemode/sandbox/gamemode which gmod likes to use.<br>
Currently it fixes these paths:<br>
- `[Active gamemode]/gamemode/[anything]/[active gamemode]/gamemode/` -> (Example: `sandbox/gamemode/spawnmenu/sandbox/gamemode/spawnmenu/`)<br>
- `include/include/`<br>

#### (EXPERIMENTAL) holylib_filesystem_cachefilehandle (default `0`)
If enabled, it will cache the file handle and return it if needed.<br>
> [!NOTE]
> This will probably cause issues if you open the same file multiple times.<br>

> [!WARNING]
> This is a noticeable improvement, but it seems to break .bsp files :/<br>

### (EXPERIMENTAL) holylib_filesystem_savesearchcache (default `1`)
If enabled, the search cache will be written into a file and loaded on startup to improve startup times

#### holylib_debug_filesystem (default `0`)
If enabled, it will print all filesyste suff.<br>

### ConCommands

#### holylib_filesystem_dumpsearchcache
Dumps the searchcache into the console.<br>
ToDo: Allow one to dump it into a file.<br>

#### holylib_filesystem_getpathfromid
Dumps the path for the given searchpath id.<br>
The id is the one listed with each file in the dumped searchcache.<br>

#### holylib_filesystem_nukesearchcache
Nukes the searchcache.<br>

#### holylib_filesystem_showpredictionerrors
Shows all files that were predicted to not exist.<br>

#### holylib_filesystem_writesearchcache
Writes the search cache into a file.<br>

#### holylib_filesystem_readsearchcache
Reads the search cache from a file.<br>

#### holylib_filesystem_dumpabsolutesearchcache
Prints the absolute search cache.<br>

## util
This module adds two new functions to the `util` library.<br>

Supports: Linux32 | Linux64 | Windows32 | Windows64<br>

### Functions

#### util.AsyncCompress(string data, number level = 5, number dictSize = 65536, function callback)
Works like util.Compress but it's async and allows you to set the level and dictSize.<br>
The defaults for level and dictSize are the same as gmod's util.Compress.<br>

Instead of making a copy of the data, we keep a reference to it and use it raw!<br>
So please don't modify it while were compressing / decompressing it or else something might break.<br>

#### util.AsyncCompress(string data, function callback)
Same as above, but uses the default values for level and dictSize.<br>

#### util.AsyncDecompress(string data, function callback, number ratio = 0.98)
ratio - The maximum decompression ratio allowed.<br>
By default 0.98 -> 0.2MB are can be decompressed to 10MB but not further.<br>

Works like util.Decompress but it's async.<br>

#### string util.FancyTableToJSON(table tbl, bool pretty, bool ignorecycle)
ignorecycle - If `true` it won't throw a lua error when you have a table that is recursive/cycle.<br>

Convers the given table to json.<br>
Unlike Gmod's version, this function will turn the numbers to an integer if they are one/fit one.<br>
This version is noticably faster than Gmod's version and uses less memory in the process.<br>

#### table util.FancyJSONToTable(string json)
Convers the json into a table.<br>
This version is noticably faster than Gmod's version and uses less memory in the process.<br>

#### string util.CompressLZ4(string data, number accelerationLevel = 1)
Compresses the given data using [LZ4](https://github.com/lz4/lz4)<br>
Returns `nil` on failure.<br>

#### string util.DecompressLZ4(string data)
Decompresses the given data using [LZ4](https://github.com/lz4/lz4)<br>
Returns `nil` on failure. 

#### util.AsyncTableToJSON(table tbl, function callback, bool pretty = false)
callback = `function(json) end`

Turns the given table into a json string just like `util.FancyTableToJSON` but it will do this on a different thread.<br>

> [!WARNING]
> By giving the table to this function you make a promise to **not** modify the table while the json string is being created!<br>
> This is because we don't copy the table, we instead copy the pointer into a new Lua state where we then iterate/access it from another state/thread and if you **modify** it in any way you will experience a crash.<br>
> You still can access the table in that time, you are just not allowed to modify it.<br>
> It was observed that you can kinda modify the table though if you add new elements/the size of the table changes while its still creating the json string you will crash.<br>

> [!NOTE]
> This function requires the `luajit` module to be enabled.<br>

## ConVars

### holylib_util_compressthreads(default `1`)
The number of threads to use for `util.AsyncCompress`.<br>
When changing it, it will wait for all queried jobs to first finish before changing the size.<br>

### holylib_util_decompressthreads(default `1`)
The number of threads to use for `util.AsyncDecompress`.<br>

### holylib_util_jsonthreads(default `1`)
The number of threads to use for `util.AsyncTableToJSON`.<br>

> [!NOTE]
> Decompressing seems to be far faster than compressing so it won't need as many threads.<br>

## concommand
This module unblocks `quit` and `exit` for `RunConsoleCommand`.<br>

Supports: Linux32 | Linux64<br>

### ConVars

#### holylib_concommand_disableblacklist (default `0`)
If enabled, it completely disables the concommand/convar blacklist.<br>

## vprof
This module adds VProf to gamemode calls, adds two convars and an entire library.

Supports: Linux32 | Linux64 | Windows32<br>

> [!NOTE]
> On Windows this doesn't currently show the hooks in vprof itself.<br>
> It only adds the functions for now.<br>

### Functions

#### vprof.Start()
Starts vprof.<br>

#### vprof.Stop()
Stops vprof.<br>

#### bool vprof.AtRoot()
Returns `true` if is vprof scope currently is at it's root node.<br>

#### VProfCounter vprof.FindOrCreateCounter(string name, number group)
Returns the given vprof counter or creates it if it's missing.<br>

> [!NOTE]
> If the vprof counter limit is hit, it will return a dummy vprof counter!

#### VProfCounter vprof.GetCounter(number index)
Returns the counter by it's index.<br>
Returns nothing if the counter doesn't exist.<br>

#### number vprof.GetNumCounters()
Returns the number of counters that exist.<br>

#### vprof.ResetCounters()
Resets all counters back to `0`.<br>

#### number vprof.GetTimeLastFrame()
Returns the time the last frame took.<br>

#### number vprof.GetPeakFrameTime()
Returns the peak time of the root node.<br>

#### number vprof.GetTotalTimeSampled()
Returns the total time of the root node.<br>

#### number vprof.GetDetailLevel()
Returns the current detail level.<br>

#### number vprof.NumFramesSampled()
Returns the number of frames it sampled.<br>

#### vprof.HideBudgetGroup(string name, bool hide = false)
Hides/Unhides the given budget group.<br>

#### number vprof.GetNumBudgetGroups()
Returns the number of budget groups.<br>
There is a limit on how many there can be.<br>

#### number vprof.BudgetGroupNameToBudgetGroupID(string name)
Returns the budget group id by the given name.<br>

#### string vprof.GetBudgetGroupName(number index)
Returns the name of the budget group by the given index.<br>

#### number vprof.GetBudgetGroupFlags(number index)
Returns the budget group flags by the index.<br>

#### Color vprof.GetBudgetGroupColor(number index)
Returns the color of the given budget group.<br>

#### VProfNode vprof.GetRoot()
Returns the root node.<br>

#### VProfNode vprof.GetCurrentNode()
Returns the current node.<br>

#### bool vprof.IsEnabled()
Returns `true` if vprof is enabled/running.<br>

#### vprof.MarkFrame()
If vprof is enabled, it will call MarkFrame on the root node.<br>

#### vprof.EnterScope(string name, number detailLevel, string budgetGroup)
Enters a new scope.<br>

#### vprof.ExitScope()
Leaves the current scope.<br>

#### vprof.Pause()
Pauses vprof.<br>

#### vprof.Resume()
Resumes vprof.<br>

#### vprof.Reset()
Resets vprof.<br>

#### vprof.ResetPeaks()
Resets all peaks.<br>

#### vprof.Term()
Terminates vprof and frees the memory of everything.<br>
This will invalidate all `VProfCounter` and `VProfNode`.<br>
This means that if you try to use one that you stored in lua, it could crash!<br>

> [!NOTE]
> This should probably never be used.<br>

### VProfCounter
This object represents a vprof counter.<br>
It internally only contains a string and a pointer to the counter value.<br>

#### string VProfCounter:\_\_tostring()
If called on a invalid object, it will return `VProfCounter [NULL]`.<br>
Normally returns `VProfCounter [name][value]`.<br>

> [!NOTE]
> A VProfCounter currently will NEVER become NULL, instead if something called vprof.Term you'll probably crash when using the class.<br>

#### string VProfCounter:Name()
Returns the name of the counter.<br>

#### VProfCounter:Set(number value)
Sets the counter to the given value.<br>

#### number VProfCounter:Get()
Returns the current value of the counter.<br>

#### VProfCounter:Increment()
Increments the counter by one.<br>

#### VProfCounter:Decrement()
Decrements the counter by one.<br>

### VProfNode
This object basicly fully exposes the `CVProfNode` class.<br>

#### string VProfNode:\_\_tostring()
If called on a invalid object, it will return `VProfNode [NULL]`.<br>
Normally returns `VProfNode [name]`.<br>

> [!NOTE]
> A VProfNode currently will NEVER become NULL, instead if something called vprof.Term you'll probably crash when using the class.<br>

#### string VProfNode:GetName()
Returns the name of this node.<br>

#### number VProfNode:GetBudgetGroupID()
Returns the budget group id of the node.<br>

#### number VProfNode:GetCurTime()
Returns the current time of this node.<br>

#### number VProfNode:GetCurTimeLessChildren()
Returns the current time of this node but without it's children.<br>

#### number VProfNode:GetPeakTime()
Returns the peak time of this node.<br>

#### number VProfNode:GetPrevTime()
Returns the previous time of this node.<br>

#### number VProfNode:GetPrevTimeLessChildren()
Returns the previous time of this node but without it's children.<br>

#### number VProfNode:GetTotalTime()
Returns the total time of this node.<br>

#### number VProfNode:GetTotalTimeLessChildren()
Returns the total time of this node but without it's children.<br>

#### number VProfNode:GetCurCalls()
Returns the amount of times this node was in scope in this frame.<br>

#### VProfNode VProfNode:GetChild()
Returns the child node of this node.<br>

#### VProfNode VProfNode:GetParent()
Returns the parent node of this node.<br>

#### VProfNode VProfNode:GetSibling()
Returns the next sibling of this node.<br>

#### VProfNode VProfNode:GetPrevSibling()
Returns the previous silbling of this node.<br>

#### number VProfNode:GetL2CacheMisses()
Returns the number of L2 cache misses. Does this even work?<br>

#### number VProfNode:GetPrevL2CacheMissLessChildren()
Returns the previous number of L2 cache misses without the children.<br>

#### number VProfNode:GetPrevLoadHitStoreLessChildren()
Does nothing / returns always 0.<br>

#### number VProfNode:GetTotalCalls()
Returns how often this node was in scope.<br>

#### VProfNode VProfNode:GetSubNode(string name, number detailLevel, string budgetGroup)
Returns or creates the VProfNode by the given name.<br>

#### number VProfNode:GetClientData()
Returns the set client data.<br>

#### VProfNode:MarkFrame()
Marks the frame.<br>
It will move all current times into their previous variants and reset them to `0`.<br>
It will also call MarkFrame on any children or sibling.<br>

#### VProfNode:ClearPrevTime()
Clears the previous time.<br>

#### VProfNode:Pause()
Pauses this node.<br>

#### VProfNode:Resume()
Resumes this node.<br>

#### VProfNode:Reset()
Resets this node.<br>

#### VProfNode:ResetPeak()
Resets the peak of this node.<br>

#### VProfNode:SetBudgetGroupID(number budgetGroupID)
Sets the new budget group by it's id.<br>

#### VProfNode:SetCurFrameTime(number frameTime)
Sets the current frametime.<br>

#### VProfNode:SetClientData(number data)
Sets the client data...<br>

#### VProfNode:EnterScope()
Enters the scope.<br>

#### VProfNode:ExitScope()
Exists the scope.<br>

### Enums
Theses are the CounterGroup_t enums.<br>

#### vprof.COUNTER_GROUP_DEFAULT = 0

#### vprof.COUNTER_GROUP_NO_RESET = 1

#### vprof.COUNTER_GROUP_TEXTURE_GLOBAL = 2

#### vprof.COUNTER_GROUP_TEXTURE_PER_FRAME = 3

#### vprof.COUNTER_GROUP_TELEMETRY = 4

### ConVars

#### holylib_vprof_exportreport (default `1`)
If enabled, vprof results will be dumped into a file in the vprof/ folder<br>

### cvars
This module adds one function to the `cvars` library.<br>

Supports: Linux32 | Linux64 | Windows32 | Windows64<br>

> [!NOTE]
> The lua library is named `cvar` because the `cvars` library is fully declared in Lua and were running before it even exists.<br> 

#### Functions

##### table cvar.GetAll()
Returns a sequential table with all ConVar's that exist.<br>

##### bool cvar.SetValue(string name, string value)
Set the convat to the given value.<br>
Returns `true` on success.<br>

##### cvar.Unregister(ConVar cvar / string name)
Unregisters the given convar.<br>

#### ConVar cvar.Find(string name)
Returns the found ConVar or `nil` on failure.<br>
Unlike Gmod's `GetConVar_Internal` there are no restrictions placed on it.<br>

## sourcetv
This module adds a new `sourcetv` library and a new class `CHLTVPlayer`.

Supports: Linux32<br>

### Functions

#### bool sourcetv.IsActive()
Returns `true` if sourcetv is active.

#### bool sourcetv.IsRecording()
Returns `true` if sourcetv is recording.<br>

#### bool sourcetv.IsMasterProxy()
Returns `true` if sourcetv server is the master proxy.<br>

#### bool sourcetv.IsRelay()
Returns `true` if the sourcetv server is a relay.<br>

#### number sourcetv.GetClientCount()
Returns the number of sourctv clients connected.<br>

#### number sourcetv.GetHLTVSlot()<br>
Returns the slot of the sourcetv client/bot.<br>

#### number sourcetv.StartRecord(string fileName)
string fileName - The name for the recording.<br>

Tries to start a new recording.<br>
Returns one of the `RECORD_` enums.<br>

#### string sourcetv.GetRecordingFile()
Returns the filename of the current recording. or `nil`.<br>

#### number sourcetv.StopRecord()<br>
Stops the active record.<br>

Returns one of the `RECORD_` enums.<br>
If successfully stopped, it will return `sourcetv.RECORD_OK`.

#### table sourcetv.GetAll()
Returns a table that contains all HLTV clients. It will return `nil` on failure.<br>

#### CHLTVClient sourcetv.GetClient(number slot)
Returns the CHLTVClient at that slot or `nil` on failure.<br>

#### sourcetv.FireEvent(IGameEvent event, bool allowOverride = false)
Fires the gameevent for all hltv clients / broadcasts it.<br>
If `allowOverride` is set to true, it internally won't block any events like `hltv_cameraman`, `hltv_chase` and `hltv_fixed`.<br>

#### sourcetv.SetCameraMan(number entIndex / Entity ent)<br>
Sends the `hltv_cameraman` event all clients and blocks the `HLTVDirector` from changing the view.<br>
Call it with `0` / `NULL` to reset it and let the `HLTVDirector` take control again.<br>

> [!NOTE]
> This won't set it for new clients. You have to call it again for thoes.<br>

### CHLTVClient
This is a metatable that is pushed by this module. It contains the functions listed below<br>
This class is based off the `CBaseClient` class and inherits its functions.<br>

#### string CHLTVClient:\_\_tostring()
Returns the a formated string.<br>
Format: `CHLTVClient [%i][%s]`<br>
`%i` -> UserID<br>
`%s` -> ClientName<br>

#### CHLTVClient:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any CHLTVClient:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `CHLTVClient:GetTable()`.<br>

#### table CHLTVClient:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### CHLTVClient:SetCameraMan(number entIndex / Entity ent)<br>
Sends the `hltv_cameraman` event to the specific client and blocks the `HLTVDirector` from changing the view.<br>
Call it with `0` / `NULL` to reset it and let the `HLTVDirector` take control again.<br>

### Enums

#### sourcetv.RECORD_OK = 0
The recording was started.<br>

#### sourcetv.RECORD_NOSOURCETV = -1<br>
SourceTV is not active!<br>

#### sourcetv.RECORD_NOTMASTER = -2<br>
the sourcetv server is not the master!<br>

#### sourcetv.RECORD_ACTIVE = -3<br>
there already is an active record!<br>

> [!NOTE]
> Should we allow multiple active record? I think I could implement it. If wanted, make a request for it.<br>

#### sourcetv.RECORD_NOTACTIVE = -4<br>
there is no active recording to stop!<br>

#### sourcetv.RECORD_INVALIDPATH = -5<br>
The filepath for the recording is invalid!<br>

#### sourcetv.RECORD_FILEEXISTS = -6<br>
A file with that name already exists!<br>

### Hooks

#### HolyLib:OnSourceTVNetMessage(CHLTVClient client, bf_read buffer)
Called when a CHLTVClient sends a net message to the server.<br>

Example:<br>
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
Called when a CHLTVClient sends a command.<br>
return `true` to tell it that the command was handled in Lua.<br>

Example:<br>
```lua
hook.Add("HolyLib:OnSourceTVCommand", "Example", function(client, name, args, argString)
	if name == "Example" then
		client:ClientPrint("Hello World from HLTVServer\n")
		return true
	end
end)
```

#### bool HolyLib:OnSourceTVStartNewShot()
Called when SourceTV tries to start a new shot.<br>
Return `true` to cancel it.<br>

#### bool HolyLib:OnSourceTVClientDisconnect(CHLTVClient client)
Called when a client disconnects from the sourcetv server.<br>

### ConVars

#### holylib_sourcetv_allownetworking (default `0`)
If enabled, HLTV Clients can send net messages to the server and `HolyLib:OnSourceTVNetMessage` will be called.<br>

#### holylib_sourcetv_allowcommands (default `0`)
If enabled, HLTV Clients can send commands to the server and `HolyLib:OnSourceTVCommand` will be called.<br>

## bitbuf
This module adds a `bf_read` and `bf_write` class.<br>

Supports: Linux32 | Linux64 | Windows32 | Windows64<br>

### Functions

#### bf_read bitbuf.CopyReadBuffer(bf_read buffer)
Copies the given buffer into a new one.<br>
Useful if you want to save the data received by a client.<br>

> [!NOTE]
> The size is clamped internally between a minimum of `4` bytes and a maximum of `262144` bytes.

#### bf_read bitbuf.CreateReadBuffer(string data)
Creates a read buffer from the given data.<br>
Useful if you want to read the userdata of the instancebaseline stringtable.<br>

> [!NOTE]
> The size is clamped internally between a minimum of `4` bytes and a maximum of `262144` bytes.

#### bf_write bitbuf.CreateWriteBuffer(number size or string data)
Create a write buffer with the given size or with the given data.<br>

> [!NOTE]
> The size is clamped internally between a minimum of `4` bytes and a maximum of `262144` bytes.

#### bf_read bitbuf.CreateStackReadBuffer(string data, function callback)
callback = `function(bf) end`<br>
Creates a read buffer from the given data allocated on the stack making it faster.<br>
Useful if you want to read the userdata of the instancebaseline stringtable.<br>

> [!WARNING]
> The buffer will be stack allocated, do NOT call this function recursively and the buffer is **only** valid inside the callback function.<br>
> This is because you could cause a crash if you were to create too many stack allocated buffers!<br>

> [!NOTE]
> The size is clamped internally between a minimum of `4` bytes and a maximum of `65536` bytes.

#### bf_write bitbuf.CreateStackWriteBuffer(number size or string data, function callback)
callback = `function(bf) end`<br>
Create a write buffer with the given size or with the given data allocated on the stack making it faster.<br>

> [!WARNING]
> The buffer will be stack allocated, do NOT call this function recursively and the buffer is **only** valid inside the callback function.<br>
> This is because you could cause a crash if you were to create too many stack allocated buffers!<br>

> [!NOTE]
> The size is clamped internally between a minimum of `4` bytes and a maximum of `65536` bytes.

### bf_read
This class will later be used to read net messages from HLTV clients.<br>
> ToDo: Finish the documentation below and make it more detailed.<br>

#### string bf_read:\_\_tostring()
Returns the a formated string.<br>
Format: `bf_read [%i]`<br>
`%i` -> size of data in bits.<br>

#### bf_read:\_\_gc()
Deletes the buffer internally.<br>

#### bf_read:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any bf_read:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `bf_read:GetTable()`.<br>

#### table bf_read:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### number bf_read:GetNumBitsLeft()
Returns the number of bits left.<br>

#### number bf_read:GetNumBitsRead()
Returns the number of bits read.

#### number bf_read:GetNumBits()
Returns the size of the data in bits.<br>

#### number bf_read:GetNumBytesLeft()
Returns the number of bytes left.<br>

#### number bf_read:GetNumBytesRead()
Returns the number of bytes read.<br>

#### number bf_read:GetNumBytes()
Returns the size of the data in bytes.<br>

#### number bf_read:GetCurrentBit()
Returns the current position/bit.

> [!NOTE]
> This is only available for the 32x!<br><br>

#### bool bf_read:IsOverflowed()
Returns `true` if the buffer is overflowed.<br>

#### number bf_read:PeekUBitLong(number bits)
Peaks to the given bit(doesn't change current position) and returns the value.

#### number bf_read:ReadBitAngle(number bits)
Peaks to the given bit(doesn't change current position) and returns the value.

#### angle bf_read:ReadBitAngles()
Reads and Angle.<br>

#### number bf_read:ReadBitCoord()

> [!NOTE]
> This is only available for the 32x!<br><br>

#### number bf_read:ReadBitCoordBits()

> [!NOTE]
> This is only available for the 32x!<br><br>

#### number bf_read:ReadBitCoordMP(bool integral = false, bool lowPrecision = false)

> [!NOTE]
> This is only available for the 32x!<br><br>

#### number bf_read:ReadBitCoordMPBits(bool integral = false, bool lowPrecision = false)

> [!NOTE]
> This is only available for the 32x!<br><br>

#### number bf_read:ReadBitFloat()

#### number bf_read:ReadBitLong(number bits, bool signed = false)
Reads a number with the given number of bits.<br>

> [!NOTE]
> This is only available for the 32x!<br><br>

#### number bf_read:ReadBitNormal()

#### string bf_read:ReadBits(number bits)
Reads the given number of bits.<br>

#### vector bf_read:ReadBitVec3Coord()
Reads a Vector.<br>

#### vector bf_read:ReadBitVec3Normal()
Reads a normalizted Vector.

#### number bf_read:ReadByte()
Reads a byte.<br>

#### string bf_read:ReadBytes(number bytes)
Reads the given number of bytes.<br>

#### number bf_read:ReadChar()
Reads a char.<br>

#### number bf_read:ReadFloat()
Reads a float.<br>

#### number bf_read:ReadLong()
Reads a long.<br>

#### string bf_read:ReadLongLong()
Reads a long long.<br>

#### bool bf_read:ReadOneBit()
Reads one bit.<br>

#### number bf_read:ReadSBitLong(number bits)
Reads a number with the given amout of bits.<br>

#### number bf_read:ReadShort()
Reads a short.<br>

#### number bf_read:ReadSignedVarInt32()

#### number bf_read:ReadSignedVarInt64()

#### string bf_read:ReadString()
Reads a null terminated string.<br>

#### number bf_read:ReadUBitLong(number bits)
Read a number with the given amount of bits.<br>

#### number bf_read:ReadUBitVar()

#### number bf_read:ReadVarInt32()

#### number bf_read:ReadVarInt64()

#### number bf_read:ReadWord()

#### bf_read:Reset()
Resets the current position and resets the overflow flag.<br>

#### bool bf_read:Seek(number pos)
Sets the current position to the given position.<br>
Returns `true` on success.<br>

#### bool bf_read:SeekRelative(number pos)
Sets the current position to the given position relative to the current position.
Basicly `newPosition = currentPosition + iPos`<br><br>
Returns `true` on success.<br>

### bf_write<br>

#### string bf_write:\_\_tostring()
Returns the a formated string.<br>
Format: `bf_write [%i]`<br>
`%i` -> size of data in bits.<br>

#### bf_write:\_\_gc()
Deletes the buffer internally.<br>

#### bf_write:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any bf_write:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `bf_write:GetTable()`.<br>

#### table bf_write:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### bool bf_write:IsValid()
returns `true` if the buffer is still valid.<br>

#### string bf_write:GetData()
Returns the written data.<br>

#### number bf_write:GetNumBytesWritten()
Returns the number of bytes written.<br>

#### number bf_write:GetNumBytesLeft()<br>
Returns the number of bytes that are unwritten.<br>

#### number bf_write:GetNumBitsWritten()
Returns the number of bits written.<br>

#### number bf_write:GetNumBitsLeft()
Returns the number of bits left.<br>

#### number bf_write:GetMaxNumBits()
Returns the maximum number of bits that can be written.<br>

#### bool bf_write:IsOverflowed()
Returns `true` if the buffer is overflowed.<br>

#### bf_write:Reset()
Resets the buffer.<br>

#### string bf_write:GetDebugName()
Returns the debug name.<br>

#### bf_write:SetDebugName(string debugName)
Sets the debug name.<br>

> [!WARNING]
> You should keep a reference to the string.<br>
> If the GC removes the string, you will experience that GetDebugName will return junk!<br>

#### bf_write:SeekToBit(number bit)
Seeks to the given bit.<br>

#### bf_write:WriteFloat(number value)
Writes a float.<br>

#### bf_write:WriteChar(number value)
Writes a char.<br>

#### bf_write:WriteByte(number value)
Writes a byte.<br>

#### bf_write:WriteLong(number value)
Writes a long.<br>

#### bf_write:WriteLongLong(number/string value)
Writes a long long.<br>

#### bf_write:WriteBytes(string data)
Writes the given bytes.<br>

#### bf_write:WriteOneBit(bool value = false)
Writes one bit.<br>

#### bf_write:WriteOneBitAt(number bit, bool value = false)
Sets the given bit to the given value.<br>

#### bf_write:WriteShort(number value)
Writes a short.<br>

#### bf_write:WriteWord(number value)
Writes a word.<br>

#### bf_write:WriteSignedVarInt32(number value)

#### bf_write:WriteSignedVarInt64(number value)

#### bf_write:WriteVarInt32(number value)

#### bf_write:WriteVarInt64(number value)

#### bf_write:WriteUBitVar(number value)

#### bf_write:WriteUBitLong(number value, number bits, bool signed = false)

#### bf_write:WriteBitAngle(number value, number bits)

#### bf_write:WriteBitAngles(Angle ang)
Writes a Angle. (`QAngle` internally).<br>

#### bf_write:WriteBitVec3Coord(Vector vec)
Writes a Vector.<br>

#### bf_write:WriteBitVec3Normal(Vector vec)
Writes a normalized Vector

#### bool bf_write:WriteBits(string data, number bits)
Writes the given number of bits from the data into this buffer.<br>
Returns `true` on success.<br>

#### bool bf_write:WriteBitsFromBuffer(bf_read buffer, number bits)
Writes the given number of bits from the given buffer into this buffer.<br>
Returns `true` on success.<br>

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
This module tries to optimize anything related to networking.<br>

it contains two optimizatiosn which were ported from [sigsegv-mvm](https://github.com/rafradek/sigsegv-mvm/blob/910b92456c7578a3eb5dff2a7e7bf4bc906677f7/src/mod/perf/sendprop_optimize.cpp#L35-L144) into here.<br>
Thoes optimiations are for `AllocChangeFrameList` and `SendTable_CullPropsFromProxies`.<br>

Additionally, there are a few further improvements implemented for:
\- `PackEntities_Normal`<br>
\- `CBaseEntity::GMOD_SetShouldPreventTransmitToPlayer`<br>
\- `CBaseEntity::GMOD_ShouldPreventTransmitToPlayer`<br>
\- `CServerGameEnts::CheckTransmit`<br>

Supports: Linux32 | Linux64<br>

### Convars

#### holylib_networking_fastpath(default `0`)
If enabled, it will cache the transmit data for players in the same engine area.<br>
This can noticably improve performance of `CServerGameEnts::CheckTransmit` especially if players are in the same area.<br>
In game, you can see the area you're in using `r_ShowViewerArea 1` which is used for the caching.<br>

How to test around with it:<br>
1. Spawn a bot so that he becomes the first player<br>
2. Enable `sv_stressbots 1`<br>
3. Enable this convar.<br>
4. **Only now** join the server so that you become the second player.<br>
The order is important since now, the bot will be the first player and will be calculated normally, while you as the second player will hit the cache.<br>
5. Test around and see if maybe anything is broken, like entities not being transmitted or such.<br>

#### holylib_networking_fasttransmit(default `1`)
If enabled, it will use our own version of `CServerGameEnts::CheckTransmit` which should be slighly faster.<br>
It also slightly improves `PackEntities_Normal` performance.<br>
This is **required** to be enabled if you intent on using `holylib_networking_fastpath`<br>

#### holylib_networking_maxviewmodels(default `3`)
Determines how many view models a player can have.<br>
By default each player has 3 view models, only the first one is really used.<br>

#### holylib_networking_transmit_all_weapons(default `1`)
If enabled, all weapons are networked if the owner is inside the PVS saving performance.<br>
This is default source engine behavior.<br>

#### holylib_networking_transmit_all_weapons_to_owner(default `1`)
If enabled, all weapons are networked to the owner.<br>
This may be useful if you have `holylib_networking_transmit_all_weapons` set to `0` but still want that each player have all their own weapons updated.<br>

> [!NOTE]
> If both `holylib_networking_transmit_all_weapons` and `holylib_networking_transmit_all_weapons_to_owner` are set to `0`, only the active weapon of the player will be networked.<br>

## steamworks
This module adds a few functions related to steam.<br>

Supports: Linux32 | Linux64<br>

### Functions

#### steamworks.Shutdown()
Shutdowns the Steam Server.<br>

#### steamworks.Activate()
Starts/Activates the Steam Server.<br>

#### bool steamworks.IsConnected()
Returns `true` if the Steam Server is connected.<br>

#### steamworks.ForceActivate()
Calls the `SV_InitGameServerSteam` function to activate the steam server exactly like the engine does it.<br>

#### bool steamworks.ForceAuthenticate(number userID)
Marks the given client as Authenticated even if they aren't.<br>

### Hooks

#### HolyLib:OnSteamDisconnect(number result)<br>
Called when our Steam server loses connection to steams servers.<br>

#### HolyLib:OnSteamConnect()<br>
Called when our Steam server successfully connected to steams servers.<br>

#### bool HolyLib:OnNotifyClientConnect(number nextUserID, string ip, string steamID64, number authResult) 
authResult - The steam auth result, `0` if steam accepts the client.<br>

| authResult | Description |
|-------|------|
| `0` | Ticket is valid for this game and this steamID |
| `1` | Ticket is not valid |
| `2` | A ticket has already been submitted for this steamID |
| `3` | Ticket is from an incompatible interface version |
| `4` | Ticket is not for this game |
| `5` | Ticket has expired |

Called when a client wants to authenticate through steam.<br>

Return `true` to forcefully authenticate his steamid.<br>
Return `false` to block his authentication.
Return nothing to let the server normally handle it.

### ConVars

#### holylib_steamworks_allow_duplicate_steamid(default `0`)
If enabled, the same steamid can be used multiple times.

> [!NOTE]
> `HolyLib:OnNotifyClientConnect` already accounts for this,<br>
> if a steamid is already used when you approve a request it will still allow the player to join.

## systimer
This module is just the timer library, but it doesn't use CurTime.<br>
> [!NOTE]
> Timer repetitions are limited to an unsigned int.<br>

Supports: Linux32 | Linux64 | Windows32 | Windows64<br>

### Functions

#### bool systimer.Adjust(string name, number delay, number reps = nil, function callback = nil)<br>
Adjusts the timer with the given name with the new values.<br>
Returns `true` if the timer was successfully adjusted.<br>

#### systimer.Check()<br>
Does nothing / deprecated.<br>

#### systimer.Create(string name, number delay, number reps, function callback)<br>
Creates a timer with the given name.<br>

#### bool systimer.Exists(string name)<br>
Returns `true` if the given timer exists.<br>

#### systimer.Remove(string name)<br>
Removes the given timer.<br>

#### number systimer.RepsLeft(string name)<br>
Returns the number of reps left for the given timer.<br>
Returns `0` if the timer wasn't found or the reps are below `0`.<br>

#### systimer.Simple(number delay, function callback)<br>
Creates a simple timer.<br>

#### bool systimer.Start(string name)<br>
Returns `true` if the given timer was successfully started again.<br>

#### bool systimer.Stop(string name)<br>
Returns `true` if the given timer was successfully stopped.<br>

#### number systimer.TimeLeft(string name)<br>
Returns the time left until the given timer is executed again.<br>
Returns `0` if the timer wasn't found.<br>

#### bool systimer.Toggle(string name)<br>
Toggles the given timer.<br>
Returns `true` if the timer was activated/started again.<br>

#### bool systimer.Pause(string name)<br>
Returns `true` if the given timer was successfully paused.<br>

#### bool systimer.UnPause(string name)<br>
Unpauses the given timer.<br>
Unlike systimer.Start this won't reset the time left until it executes again.<br>

## pas
This module plans to add a few PAS related functions like `table pas.FindInPAS(Vector pos or Entity ent)`.<br>
If you got an Idea for a function to add, feel free to comment it into [its issue](https://github.com/RaphaelIT7/gmod-holylib/issues/1).

Supports: Linux32 | Linux64 | Windows32 | Windows64<br>

### Functions

#### table pas.FindInPAS(Entity ent / Vector vec)
Returns a sequential table containing all entities in that PAS.<br>

#### bool / table pas.TestPAS(Entity ent / Vector pas / EntityList list, Entity / Vector hearPos)
Tests if the give hear position is inside the given pas.<br>
Returns `true` if it is.<br>

If given a EntityList, it will return a table wich contains the result for each entity.<br>
The key will be the entity and the value is the result.<br>

#### bool pas.CheckBoxInPAS(Vector mins, Vector maxs, Vector pas)
Checks if the given pox is inside the PAS.<br>
Returns `true` if it is.<br>

## voicechat
This module contains a few things to control the voicechat.<br>
You could probably completly rework the voicechat with this since you could completly handle it in lua.<br>

Supports: Linux32 | Linux64 | Windows32<br>

### Optimizations

This module improves `GM:PlayerCanHearPlayersVoice` by calling it only for actively speaking players.<br>
The hook is additionally NOT called for all players which would result in `currentPlayers * currentPlayers` calls of the hook.<br>
Instead now its called only once for the player that is speaking resulting in `1 * currentPlayers` calls of the hook.<br>

> [!NOTE]
> If you have any issues with this optimization, check if `sv_alltalk` is enabled as the lua hook is **not** called when the convar is enabled!<br>
> In the default Gmod behavior, it would get called even when `sv_alltalk` was enabled which changed with our optimization.<br>

### Functions

#### voicechat.SendEmptyData(Player ply, number playerSlot = ply:EntIndex()-1)
Sends an empty voice packet.<br>
If `playerSlot` isn't specified it will use the player slot of the given player.<br>

#### voicechat.SendVoiceData(Player ply, VoiceData data)
Sends the given VoiceData to the given player.<br>
It won't do any processing, it will just send it as it is.<br>

#### voicechat.BroadcastVoiceData(VoiceData data, table plys = nil)
Same as `voicechat.SendVoiceData` but broadcasts it to all players.<br>
If given a table it will only send it to thoes players.<br>

#### voicechat.ProcessVoiceData(Player ply, VoiceData data)
Let's the server process the VoiceData like it was received from the client.<br>
This can be a bit performance intense.<br>
> [!NOTE]
> This will ignore the set player slot!<br>

#### bool voicechat.IsHearingClient(Player ply, Player targetPly)
Returns `true` if `ply` can hear the `targetPly`.<br>

#### bool voicechat.IsProximityHearingClient(Player ply, Player targetPly)
Returns `true` if `ply` can hear the `targetPly` in it's proximity.<br>

#### VoiceData voicedata.CreateVoiceData(number playerSlot = 0, string data = NULL, number dataLength = NULL)
Creates a new VoiceData object.<br>
> [!NOTE]
> All Arguments are optional!

#### VoiceStream voicechat.CreateVoiceStream()
Creates a empty VoiceStream.<br>

#### VoiceStream, number(statusCode) voicechat.LoadVoiceStream(string fileName, string gamePath = "DATA", function callback = nil)
callback = `function(VoiceStream loadedStream, number statusCode)`
statusCode = `-4 = Invalid file, -3 = Invalid version, -2 = File not found, -1 = Invalid type, 0 = None, 1 = Done`<br>

Tries to load a VoiceStream from the given file.<br>
If a `callback` is specified it **WONT** return **anything** and the `callback` will be called, as it will execute everythign **async**.<br>
If you want it to **not** run async, simply provide **no** callback function<br>

> [!NOTE]
> This function also supports `.wav` files to load from since `0.8`

#### VoiceStream, number(statusCode) voicechat.LoadVoiceStreamFromWaveString(string waveData, function callback = nil, bool promiseToNeverModify = false)
callback = `function(VoiceStream loadedStream, number statusCode)`
statusCode = `-4 = Invalid file, -3 = Invalid version, -2 = File not found, -1 = Invalid type, 0 = None, 1 = Done`<br>
promiseToNeverModify = If set to `true`, it will reference the waveData instead of copying it reducing memory usage and improving speed though you need to keep your promise of never modifying it while it's in use!<br>

Tries to load a VoiceStream from the given data.<br>
If a `callback` is specified it **WONT** return **anything** and the `callback` will be called, as it will execute everythign **async**.<br>
If you want it to **not** run async, simply provide **no** callback function<br>

#### number(statusCode), string(waveData) voicechat.SaveVoiceStream(VoiceStream stream, string fileName = nil, string gamePath = "DATA", function callback = nil, bool returnWaveData = false)
callback = `function(VoiceStream loadedStream, number statusCode)`
statusCode = `-4 = Invalid file, -3 = Invalid version, -2 = File not found, -1 = Invalid type, 0 = None, 1 = Done`<br>
returnWaveData = If set to true (or if `fileName` is nil), the function will when running in sync return the waveData as a string, or if async will return the waveData as a fourth argument after `statusCode` in the callback.<br>

Argument overload version (will **always** return wave data): `voicechat.SaveVoiceStream(VoiceStream stream, function callback = nil)`

Tries to save a VoiceStream into the given file.<br>
If a `callback` is specified it **WONT** return **anything** and the `callback` will be called, as it will execute everythign **async**.<br>
If you want it to **not** run async, simply provide **no** callback function<br>

> [!NOTE]
> It should be safe to modify/use the VoiceStream while it's saving async **BUT** you should try to avoid doing that. <br>
> This function also supports `.wav` files to write the data into since `0.8`.<br>
> You should **always** inform your players if you save their voice! <br>
> You can set both `fileName` and `returnWaveData` which will cause it to be written to disk and the data to be returned<br>
> If `fileName` and `returnWaveData` are both not set then it will error as atleast one of them needs to be enabled.<br>

### bool voicedata.IsPlayerTalking(Player ply/number playerSlot)
Returns `true` if the player is currently talking.<br>

> [!NOTE]
> This value does NOT reset if a player disconnect meaning on empty slots the value of the last player there will remain stored.

### number voicedata.LastPlayerTalked(Player ply/number playerSlot)
Returns when the player last talked.

> [!NOTE]
> This value does NOT reset if a player disconnect meaning on empty slots the value of the last player there will remain stored.

#### bool voicechat.ApplyEffect(table effectData, VoiceStream/VoiceData stream, function callback = nil)
callback = `function(VoiceStream/Voicedata data, bool success)`

Example effectData:
```lua
{
	ContinueOnFailure = true, -- If you process a VoiceStream and it fails to apply a effect for some reason it will still proceed and ignore the failure

	-- Volume effect
	EffectName = "Volume",
	Volume = 1.0, -- The volume for the audio
}
```

Applies the given effectData to the given Data/Stream.<br>
If a `callback` is specified it **WONT** return **anything** and the `callback` will be called, as it will execute everythign **async**.<br>
If you want it to **not** run async, simply provide **no** callback function, it will return `true` on success<br>

> [!NOTE]
> It should be safe to modify/use the VoiceStream while it's being modified async **BUT** you should try to avoid doing that.

####

### VoiceData
VoiceData is a userdata value that is used to manage the voicedata.<br>

#### string VoiceData:\_\_tostring()
Returns `VoiceData [Player Slot][Length/Size]`.<br>

#### VoiceData:\_\_gc()
Garbage collection. Deletes the voice data internally.<br>

#### VoiceData:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any VoiceData:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `VoiceData:GetTable()`.<br>

#### table VoiceData:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### bool VoiceData:IsValid()
Returns `true` if the VoiceData is still valid.<br>

#### string VoiceData:GetData()
Returns the raw compressed voice data.<br>

#### number VoiceData:GetLength()
Returns the length of the data.<br>

#### number VoiceData:GetPlayerSlot()
Returns the slot of the player this voicedata is originally from.<br>

#### string VoiceData:GetUncompressedData()
number decompressSize - The number of bytes to allocate for decompression.<br>

Returns the uncompressed voice data or an empty string if the VoiceData had no data.<br>
On failure it will return the number for the status code, see the list below:<br>
```cpp
// Error codes for use with the voice functions
enum EVoiceResult
{
	k_EVoiceResultOK = 0,
	k_EVoiceResultNotInitialized = 1,
	k_EVoiceResultNotRecording = 2,
	k_EVoiceResultNoData = 3,
	k_EVoiceResultBufferTooSmall = 4,
	k_EVoiceResultDataCorrupted = 5,
	k_EVoiceResultRestricted = 6,
	k_EVoiceResultUnsupportedCodec = 7,
	k_EVoiceResultReceiverOutOfDate = 8,
	k_EVoiceResultReceiverDidNotAnswer = 9,
};
```

#### bool VoiceData:GetProximity()
Returns if the VoiceData is in proximity.<br>

#### VoiceData:SetData(string data, number length = nil)
Sets the new voice data.<br>

#### VoiceData:SetLength(number length)
Sets the new length of the data.<br>

#### VoiceData:SetPlayerSlot(number slot)
Sets the new player slot.<br>

#### VoiceData:SetProximity(bool bProximity)
Sets if the VoiceData is in proximity.<br>

#### VoiceData VoiceData:CreateCopy()
Creates a exact copy of the voice data.<br>

### VoiceStream
VoiceStream is a userdata value that internally contains multiple VoiceData for specific ticks.<br>

#### string VoiceStream:\_\_tostring()
Returns `VoiceStream [entires]`.<br>

#### VoiceStream:\_\_gc()
Garbage collection. Deletes the voice data internally.<br>

#### VoiceStream:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any VoiceStream:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `VoiceStream:GetTable()`.<br>

#### table VoiceStream:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### bool VoiceStream:IsValid()
Returns `true` if the VoiceData is still valid.<br>

#### table VoiceStream:GetData(bool directData = false)
directData - If true it will return the VoiceData itself **instead of** creating a copy, if you modify it you will change the VoiceData stored in the stream!<br>
(This argument will reduce memory usage & should improve performance slightly)<br>

Returns a table, with the tick as key and **copy** of the VoiceData as value.<br>

> [!NOTE]
> The returned VoiceData is just a copy,<br>
> modifying them won't affect the internally stored VoiceData.
> Call `VoiceStream:SetData` or `VoiceStream:SetIndex` after you modified it to update it.<br>

#### VoiceStream:SetData(table data, bool directData = false)
directData - If true it will store the VoiceData itself **instead of** creating a copy that would be saved, if you modify the VoiceData after you called this, you will change the VoiceData stored in the stream!<br>
(This argument will reduce memory usage & should improve performance slightly)<br>

Sets the VoiceStream from the given table.<br>

#### number VoiceStream:GetCount()
Returns the number of VoiceData it stores.<br>

#### VoiceData VoiceStream:GetIndex(number index, bool directData = false)
directData - If true it will return the VoiceData itself **instead of** creating a copy, if you modify it you will change the VoiceData stored in the stream!<br>
(This argument will reduce memory usage & should improve performance slightly)<br>

Returns a **copy** of the VoiceData for the given index or `nil`.<br>

> [!NOTE]
> The returned VoiceData is just a copy,<br>
> modifying them won't affect the internally stored VoiceData.
> Call `VoiceStream:SetData` or `VoiceStream:SetIndex` after you modified it to update it.<br>

#### VoiceStream:SetIndex(number index, VoiceData data, bool directData = false)
directData - If true it will store the VoiceData itself **instead of** creating a copy that would be saved, if you modify the VoiceData after you called this, you will change the VoiceData stored in the stream!<br>
(This argument will reduce memory usage & should improve performance slightly)<br>

Create a copy of the given VoiceData and sets it onto the specific index and overrides any data thats already present.<br>

### VoiceStream functions for playback
These functions mainly make it easier to play the VoiceData.<br>
you can have a Think hook and call `VoiceStream:GetNextTick` once per Tick and either it returns a VoiceData for that Tick or it returns `nil`.<br>
This way you don't need to keep track of a counter yourself to play the VoiceData from.<br>

#### int (previousTick) VoiceStream:ResetTick(number resetTick = 0)
Resets the current tick of the voicestream back to the given value or `0`<br>
Returns the tick it was previously at<br>

#### VoiceData VoiceStream:GetNextTick(bool directData = false)
directData - If true it will set the VoiceData itself **instead of** creating a copy that would be saved, if you modify the VoiceData after you called this, you will change the VoiceData stored in the stream!<br>
(This argument will reduce memory usage & should improve performance slightly)<br>

Returns the VoiceData of the next tick and increments the internal tick counter by one.<br>

#### VoiceData VoiceStream:GetCurrentTick(bool directData = false)
directData - If true it will set the VoiceData itself **instead of** creating a copy that would be saved, if you modify the VoiceData after you called this, you will change the VoiceData stored in the stream!<br>
(This argument will reduce memory usage & should improve performance slightly)<br>

Returns the VoiceData of the current tick without changing the internal tick count<br>

#### VoiceData VoiceStream:GetPreviousTick(bool directData = false)
directData - If true it will set the VoiceData itself **instead of** creating a copy that would be saved, if you modify the VoiceData after you called this, you will change the VoiceData stored in the stream!<br>
(This argument will reduce memory usage & should improve performance slightly)<br>

Returns the VoiceData of the previous tick and decrements the internal tick count by one<br>

### Hooks

#### bool HolyLib:PreProcessVoiceChat(Player ply, VoiceData data)
Called before the voicedata is processed.<br>
Return `true` to stop the engine from processing it.<br>

> [!NOTE]
> After the hook the `VoiceData` becomes **invalid**, if you want to store it call `VoiceData:CreateCopy()` and use the returned VoiceData.

Example to record and play back voices.<br>
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
If enabled, the VoiceChat hooks will be called.<br>

#### holylib_voicechat_threads(default `1`)
The number of threads the voicechat can use for async stuff.<br>
voicechat.LoadVoiceStream and voicechat.SaveVoiceStream currently use it,<br>
originally I expected thoes both functions to be far slower but they turned out to be quite fast.

### holylib_voicechat_updateinterval(default `0.1`)
How often we call PlayerCanHearPlayersVoice for the actively talking players.<br>
This interval is unique to each player<br>

### holylib_voicechat_managerupdateinterval(default `0.1`)
How often we loop through all players to check their voice states,<br>
We still check the player's interval to reduce calls if they already have been updated in the last x(your defined interval) seconds.<br>

### holylib_voicechat_stopdelay(default `1`)
How many seconds before a player is marked as stopped talking.

### holylib_voicechat_canhearhimself(default `1`)
We assume that the player can hear himself and won't call `GM:PlayerCanHearPlayersVoice` for the talking player saving one call.<br>

## physenv
This module fixes https://github.com/Facepunch/garrysmod-issues/issues/642 and adds a few small things.<br>

Supports: Linux32 | Windows32<br>

> [!NOTE]
> Will soon have support for Windows & Linux64<br>
> Windows currently likes to crash :/<br>

### Functions

#### physenv.SetLagThreshold(number ms)
The lag threshold(time in ms) which if exceeded will cause it to call the `HolyLib:OnPhysicsLag` hook.<br>

> [!NOTE]
> Only works on Linux32<br>

#### number physenv.GetLagThreshold()
Returns the lag threshold in ms.<br>

> [!NOTE]
> Only works on Linux32<br>

#### physenv.SetPhysSkipType(IVP_SkipType type)
Sets the skiptype for the current simulation.<br>
This is reset after the simulation ended.<br>

> [!NOTE]
> Only works on Linux32<br>

#### IPhysicsEnvironment physenv.CreateEnvironment()
Creates a new physics environment.<br>
It will apply all settings from the main environment on the newly created on to match.<br>

> [!NOTE]
> If you notice Physics objects that have a invalid entity, report this. But this should normally never happen.<br>
> In this case, this means that something is failing to remove physics objects and it will cause random crashes.<br>

#### IPhysicsEnvironment physenv.GetActiveEnvironmentByIndex(number index)
Returns the physics environment by the given index.<br>

#### physenv.DestroyEnvironment(IPhysicsEnvironment environment)
Destroys the given physics environment.<br>

#### IPhysicsEnvironment physenv.GetCurrentEnvironment()
Returns the currently simulating environment or `nil` if not inside a simulation.<br>

#### physenv.EnablePhysHook(bool shouldCall)
Enables/Disables the `HolyLib:OnPhysFrame` hook.<br>

#### IPhysicsCollisionSet physenv.FindCollisionSet(number index)
Returns the collision set by the given index.<br>

> [!NOTE]
> Only 32 collision sets can exist at the same time!<br> 

#### IPhysicsCollisionSet physenv.FindOrCreateCollisionSet(number index)
Returns the collision set by the given index or creates it if needed.<br>

> [!NOTE]
> Only 32 collision sets can exist at the same time!<br>

#### physenv.DestroyAllCollisionSets()
Destroys all collision sets.<br>

### Functions (physcollide)

#### CPhysCollide physcollide.BBoxToCollide(Vector mins, Vector maxs)
Creates a CPhysCollide from the given mins & maxs.<br>

#### CPhysConvex physcollide.BBoxToConvex(Vector minx, Vector maxs)
Creates a CPhysConvex from the given mins & maxs.<br>

#### CPhysCollide physcollide.ConvertConvexToCollide(CPhysConvex convex)
Converts the given convex to a CPhysCollide and deletes it.<br>

#### CPhysCollide physcollide.ConvertPolysoupToCollide(CPhysPolySoup soup)
Converts the given polysoup to a CPhysCollide and deletes it.<br>

#### physcollide.ConvexFree(CPhysConvex convex)
Frees the given CPhysConvex if it wan't used/converted.<br>

#### CPhysPolySoup physcollide.PolysoupCreate()
Creates a CPhysPolySoup.<br>

#### physcollide.PolysoupAddTriangle(CPhysPolySoup soup, Vector a, Vector b, Vector c, number materialIndex)
Adds a triangle to the polysoup.<br>

#### physcollide.PolysoupDestroy(CPhysPolySoup soup)
Frees the given CPhysPolySoup if it wasn't used/converted.<br>

#### Vector(mins), Vector(maxs) physcollide.CollideGetAABB(CPhysCollide collide, Vector origin, Angle rotation)
Returns the AABB of the given CPhysCollide.<br>

#### Vector physcollide.CollideGetExtent(CPhysCollide collide, Vector origin, Angle rotation, Vector direction)
Get the support map for a collide in the given direction.<br>

#### Vector physcollide.CollideGetMassCenter(CPhysCollide collide)
Gets the mass center of the CPhysCollide.<br>

#### Vector physcollide.CollideGetOrthographicAreas(CPhysCollide collide)
get the approximate cross-sectional area projected orthographically on the bbox of the collide

> [!NOTE]
> These are fractional areas - unitless.<br>Basically this is the fraction of the OBB on each axis that<br>
> would be visible if the object were rendered orthographically.<br>
> This has been precomputed when the collide was built or this function will return 1,1,1

#### number physcollide.CollideIndex(CPhysCollide collide)
Returns the index of the physics model.<br>

#### physcollide.CollideSetMassCenter(CPhysCollide collide, Vector massCenter)
Sets the new mass center of the CPhysCollide.<br>

#### physcollide.CollideSetOrthographicAreas(CPhysCollide collide, Vector area)
I have no Idea....<br> 

#### number physcollide.CollideSize(CPhysCollide collide)
Returns the memory size of the CPhysCollide.<br>

#### number physcollide.CollideSurfaceArea(CPhysCollide collide)
Computes the surface area of the CPhysCollide.<br>

#### number physcollide.CollideVolume(CPhysCollide collide)
Computes the volume of the CPhysCollide.<br>

#### string physcollide.CollideWrite(CPhysCollide collide, bool swap)
Serializes the CPhysCollide and returns the data containing it.<br>

### CPhysCollide physcollide.UnserializeCollide(string data, number index)
Unserializes the given data into a CPhysCollide.<br>

#### number physcollide.ConvexSurfaceArea(CPhysConvex convex)
Computes the surface area of the convex.<br>

#### number physcollide.ConvexVolume(CPhysConvex convex)
Computes the volume of the convex.<br>

#### ICollisionQuery physcollide.CreateQueryModel(CPhysCollide collide)
Creates a ICollisionQuery from the given CPhysCollide.<br>

#### physcollide.DestroyQueryModel(ICollisionQuery query)
Destroys the given ICollisionQuery.<br>

#### physcollide.DestroyCollide(CPhysCollide collide)
Destroys the given CPhysCollide.<br>

### objectparams_t table structure.
This table structure is used by a few functions.<br>

> [!NOTE]
> You don't need to set all fields.
> Every single field has a default.<br>

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
Returns `CPhysCollide [NULL]` if invalid.<br>
Else it returns `CPhysCollide`.<br>

#### CPhysCollide:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any CPhysCollide:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `CPhysCollide:GetTable()`.<br>

#### table CPhysCollide:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### bool CPhysCollide:IsValid()
Returns `true` if the CPhysCollide is still valid.<br> 

### CPhysPolySoup

#### CPhysPolySoup:\_\_tostring()
Returns `CPhysPolySoup [NULL]` if invalid.<br>
Else it returns `CPhysPolySoup`.<br>

#### CPhysPolySoup:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any CPhysPolySoup:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `CPhysPolySoup:GetTable()`.<br>

#### table CPhysPolySoup:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### bool CPhysPolySoup:IsValid()
Returns `true` if the CPhysPolySoup is still valid.<br>

### CPhysConvex

#### CPhysConvex:\_\_tostring()
Returns `CPhysConvex [NULL]` if invalid.<br>
Else it returns `CPhysConvex`.<br>

#### CPhysConvex:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any CPhysConvex:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `CPhysConvex:GetTable()`.<br>

#### table CPhysConvex:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### bool CPhysConvex:IsValid()
Returns `true` if the CPhysConvex is still valid.<br>

### ICollisionQuery

#### ICollisionQuery:\_\_tostring()
Returns `ICollisionQuery [NULL]` if invalid.<br>
Else it returns `ICollisionQuery`.<br>

#### ICollisionQuery:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any ICollisionQuery:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `ICollisionQuery:GetTable()`.<br>

#### table ICollisionQuery:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### bool ICollisionQuery:IsValid()
Returns `true` if the collision query is still valid.<br>

#### number ICollisionQuery:ConvexCount()
Returns the number of Convexes.<br>

#### number ICollisionQuery:TriangleCount(number convexIndex)
Returns the number of triangles for the given convex index.<br>

#### number ICollisionQuery:GetTriangleMaterialIndex(number convexIndex, number triangleIndex)
Returns the material index for the given triangle.<br>

#### ICollisionQuery:SetTriangleMaterialIndex(number convexIndex, number triangleIndex, number materialIndex)
Sets the material index of the given triangle index.

#### Vector, Vector, Vector ICollisionQuery:GetTriangleVerts(number convexIndex, number triangleIndex)
Returns the three vectors that bukd the triangle at the given index.<br>

#### ICollisonQuery:SetTriangleVerts(number convexIndex, number triangleIndex, Vector a, Vector b, Vector c)
Sets the three Vectors that build the triangle at the given index.<br>

### IPhysicsCollisionSet

#### IPhysicsCollisionSet:\_\_tostring()
Returns `IPhysicsCollisionSet [NULL]` if invalid.<br>
Else it returns `IPhysicsCollisionSet`.<br>

#### IPhysicsCollisionSet:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any IPhysicsCollisionSet:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `IPhysicsCollisionSet:GetTable()`.<br>

#### table IPhysicsCollisionSet:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### bool IPhysicsCollisionSet:IsValid()
Returns `true` if the collisionset is still valid.<br>

#### IPhysicsCollisionSet:EnableCollisions(number index1, number index2)
Marks collisions to be enabled for the two indexes.<br>

#### IPhysicsCollisionSet:DisableCollisions(number index1, number index2)
Marks collisions to be disabled for the two indexes.<br>

#### bool IPhysicsCollisionSet:ShouldCollide(number index1, number index2)
Returns `true` if the collision between the two objects are enabled.<br>

### IPhysicsEnvironment

#### IPhysicsEnvironment:\_\_tostring()
Returns `IPhysicsEnvironment [NULL]` if invalid.<br>
Else it returns `IPhysicsEnvironment`.<br>

#### IPhysicsEnvironment:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any IPhysicsEnvironment:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `IPhysicsEnvironment:GetTable()`.<br>

#### table IPhysicsEnvironment:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### bool IPhysicsEnvironment:IsValid()
Returns `true` if the environment is still valid.<br>

#### bool IPhysicsEnvironment:TransferObject(IPhysicsObject obj, IPhysicsEnvironment newEnvironment)
Transfers the physics object from this environment to the new environment.<br>

> [!WARNING]
> You shouldn't transfer players or vehicles.<br>

#### IPhysicsEnvironment:SetGravity(Vector newGravity)
Sets the new gravity in `source_unit/s^2`<br>

#### Vector IPhysicsEnvironment:GetGravity()
Returns the current gravity in<br>`source_unit/s^2`<br>

#### IPhysicsEnvironment:SetAirDensity(number airdensity)
Sets the new air density.<br>

#### number IPhysicsEnvironment:GetAirDensity()
Returns the current air density.<br>

#### IPhysicsEnvironment:SetPerformanceSettings(PhysEnvPerformanceSettings settings)
Sets the new performance settings.<br>
Use the [PhysEnvPerformanceSettings](https://wiki.facepunch.com/gmod/Structures/PhysEnvPerformanceSettings) structure.<br>

#### table IPhysicsEnvironment:GetPerformanceSettings()
Returns the current performance settings.<br>
The table will use the [PhysEnvPerformanceSettings](https://wiki.facepunch.com/gmod/Structures/PhysEnvPerformanceSettings) structure.<br>

#### number IPhysicsEnvironment:GetNextFrameTime()
returns the current simulation clock's value at the next frame.<br>
This is an absolute time.<br>

#### number IPhysicsEnvironment:GetSimulationTime()
returns the current simulation clock's value.<br>
This is an absolute time.<br>

#### IPhysicsEnvironment:SetSimulationTimestep(number timeStep)
Sets the next simulation timestep.<br>

#### number IPhysicsEnvironment:SetSimulationTimestep()
returns the next simulation timestep.<br>

#### number IPhysicsEnvironment:GetActiveObjectCount()
returns the number of active physics objects.<br>

#### table IPhysicsEnvironment:GetActiveObjects()
returns a table containing all active physics objects.<br>

#### table IPhysicsEnvironment:GetObjectList()
returns a table containing all physics objects.<br>

#### bool IPhysicsEnvironment:IsInSimulation()
returns true if the current physics environment is in simulation.<br>

#### IPhysicsEnvironment:SetInSimulation(bool simulating = false)
Sets if the current physics environment is in simulation or not.<br>

#### IPhysicsEnvironment:ResetSimulationClock()
resets the simulation clock.<br>

#### IPhysicsEnvironment:CleanupDeleteList()
cleans the delete list?<br>

#### IPhysicsEnvironment:SetQuickDelete(bool quickDelete)
Sets quick delete?<br>

#### IPhysicsEnvironment:EnableDeleteQueue(bool deleteQueue)
Enables/Disables the delete queue.<br>

#### IPhysicsEnvironment:Simulate(number deltaTime, bool onlyEntities = false)
Simulates the given delta time in the environment.<br>
If `onlyEntities` is set, it will only update the Entities, without simulating the physics environment.<br>

#### IPhysicsObject IPhysicsEnvironment:CreatePolyObject(CPhysCollide collide, number materialIndex, Vector origin, Angle angles, table objectparams_t)
Creates a new IPhysicsObject in the environment.<br>

#### IPhysicsObject IPhysicsEnvironment:CreatePolyObjectStatic(CPhysCollide collide, number materialIndex, Vector origin, Angle angles, table objectparams_t)
Creates a new static IPhysicsObject in the environment.<br>

#### IPhysicsObject IPhysicsEnvironment:CreateSphereObject(number radius, number materialIndex, Vector origin, Angle angles, table objectparams_t, bool static = false)
Creates a new perfect sphere IPhysicsObject in the environment.<br>

#### IPhysicsEnvironment:DestroyObject(IPhysicsObject object)
Destroys the given physics object.<br>

#### bool IPhysicsEnvironment:IsCollisionModelUsed(CPhysCollide collide)
Returns true if it uses a collision model.<br>
This function is internally use for debugging?<br>

#### IPhysicsEnvironment:SetObjectEventHandler(function onObjectWake(IPhysicsObject obj), function onObjectSleep(IPhysicsObject obj))
Allows you to add callbacks when physics objects wake up or go to sleep in the environment.<br>

#### IPhysicsObject IPhysicsEnvironment:CreateWorldPhysics()
Creates the world physics object and also adds all static props.<br>

### Enums
Theses are the IVP_SkipType enums.<br>

#### physenv.IVP_NONE = -1
Do nothing and call the `HolyLib:OnPhysicsLag` hook again if it trigger again.<br>
This can be useful if you want the `HolyLib:OnPhysicsLag` hook to run multiple times in the same simulation frame.<br>

#### physenv.IVP_NoSkip = 0
Let the simulation run normally.<br>

#### physenv.IVP_SkipImpact = 1
Skip all impact calls.<br>

#### physenv.IVP_SkipSimulation = 2
Skip the entire simulation.<br>

> [!NOTE]
> Players that collide with props will be randomly teleported!<br>

### Hooks

#### IVP_SkipType HolyLib:OnPhysicsLag(number simulationTime, PhysObj phys1, PhysObj phys2, table recalcPhys, string callerFunction)
callerFunction - This will be the internal IVP function in which the lag was detected and caued this Lua call<br>
recalcPhys - A table containing all physics objects that currently are recalculating collisions. **Avoid modifying them!**<br>
All arguments like `phys1`, `phys2` can be nil! (not NULL)<br>

Called when the physics simulaton is taking longer than the set lag threshold.<br>
It provides the two entities it was currently working on when the hook was triggered,<br>
most likely they will be the oney causing the lag BUT it should NOT be taken for granted!<br>

You can freeze all props here and then return `physenv.IVP_SkipSimulation` to skip the simulation for this tick if someone is trying to crash the server.<br>

> [!WARNING]
> Avoid modifying the `recalcPhys` PhysObjects as changing collision rules on it can possibly be unstable!<br>
> The table of `recalcPhys` has both key and value set for PhysObjects allowing you to easily check it like this: `if not recalcPhys[pObjectToModify] then`<br>

> [!NOTE]
> Only works on Linux32<br>
> By default its called only **ONCE** per simulation frame, you can return `physenv.IVP_NONE` to get it triggered multiple times in the same frame.<br>

#### bool HolyLib:PrePhysFrame(number deltaTime)<br>
Called when the physics are about to be simulated.<br>
Return `true` to stop the engine from doing anything.<br>

> [!NOTE]
> This hook first needs to be enabled by calling `physenv.EnablePhysHook(true)`<br>

Example of pausing the physics simulation while still allowing the entities to be updated and moved with the physgun:<br>
```lua
physenv.EnablePhysHook(true)
local mainEnv = physenv.GetActiveEnvironmentByIndex(0)
hook.Add("HolyLib:PrePhysFrame", "Example", function(deltaTime)
	mainEnv:Simulate(deltaTime, true) -- the second argument will only cause the entities to update.
<br><br>return true -- We stop the engine from running the simulation itself again as else it will result in issue like "Reset physics clock" being spammed
end)
```

### HolyLib:PostPhysFrame(number deltaTime, number simulationTime)
Called after physics were simulated and how long it took to simulate in milliseconds.

## (Experimental) bass
This module will add functions related to the bass dll.<br>
Does someone have the bass libs for `2.4.15`? I can't find them :<<br>
`.mp3` files most likely won't work.<br>

Since this module is experimental, it's disabled by default.<br>
You can enable it with `holylib_enable_bass 1`<br>

Supports: Linux32 | Linux64<br>

### Functions

#### bass.PlayFile(string filePath, string flags, function callback)
callback - function(IGMODAudioChannel channel, number errorCode, string error)<br>
Creates a IGMODAudioChannel for the given file.<br>

#### bass.PlayURL(string URL, string flags, function callback)
callback - function(IGMODAudioChannel channel, number errorCode, string error)<br>
Creates a IGMODAudioChannel for the given url.<br>

### IGModAudioChannel

#### string IGModAudioChannel:\_\_tostring()
Returns `IGModAudioChannel [NULL]` if invalid.<br>
Else it returns `IGModAudioChannel [FileName/URL]`.<br>

#### IGModAudioChannel:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any IGModAudioChannel:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `IGModAudioChannel:GetTable()`.<br>

#### table IGModAudioChannel:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### IGModAudioChannel:\_\_gc()
ToDo / Doesn nothing yet.<br>

#### IGModAudioChannel:Destroy()
Destroys the audio channel.<br>

#### IGModAudioChannel:Stop()
Stops the channel.<br>

#### IGModAudioChannel:Pause()
Pauses the channel.<br>

#### IGModAudioChannel:Play()
Plays the channel.<br>

#### IGModAudioChannel:SetVolume(number volume)
Sets the volume of the channel.<br>
(It's serverside... how does volume even play a role)<br>

#### number IGModAudioChannel:GetVolume()
Returns the volume of the channel.<br>

#### IGModAudioChannel:SetPlaybackRate(number playbackRate)
Sets the playback rate of the channel.<br>

#### number IGModAudioChannel:GetPlaybackRate()
Returns the playback rate of the channel.<br>

#### IGModAudioChannel:SetTime(number time)
Sets the time of the channel.<br>

#### number IGModAudioChannel:GetTime()
Returns the time of the channel.<br>

#### number IGModAudioChannel:GetBufferedTime()
Returns the buffered time of the channel.<br>

> [!NOTE]
> If it's playing a file, it will just return the length of it.<br>

#### number IGModAudioChannel:GetState()
Returns the state of the channel.<br>

#### IGModAudioChannel:SetLooping(bool looping)
Sets looping for the channel.<br>

#### bool IGModAudioChannel:IsLooping()
Returns `true` if the channel will loop.<br>

#### bool IGModAudioChannel:IsOnline()
Returns `true` if were playing a URL.<br>

#### bool IGModAudioChannel:Is3D()
Returns `true` if the channel is a 3D one.<br>

#### bool IGModAudioChannel:IsBlockStreamed()
Returns `true` if the sound is received in chunks.<br>

#### bool IGModAudioChannel:IsValid()
Returns `true` if the channel is valid.<br>

#### string IGModAudioChannel:GetFileName()
Returns the filename or URL the channel is playing.<br>

#### number IGModAudioChannel:GetSampleRate()
Returns the samplerate of the channel.<br>

#### number IGModAudioChannel:GetBitsPerSample()
Returns the bits per sample.<br>

#### number IGModAudioChannel:GetAverageBitRate()
Returns the average bit rate.<br>

#### number, number IGModAudioChannel:GetLevel()
Returns the level of the channel.<br>
First value is left, second is right.<br>

#### IGModAudioChannel:SetChannelPan(number pan)
Sets the channel pan.<br>

#### number IGModAudioChannel:GetChannelPan()
Returns the channel pan.<br>

#### string IGModAudioChannel:GetTags()
Returns the tag of the channel.<br>

#### IGModAudioChannel:Restart()
Restarts the channel.<br>

#### (Soon)IGModAudioChannel:FFT(table output, number fft)
Computes the DFT of the sound channel.<br>
What even is that.<br>

## entitiylist
This module just adds a lua class.<br>
Only use their functions after entities were created or you might be missing entities in the returned tables!<br>

Supports: Linux32 | Linux64 | Windows32 | Windows64<br>

### Functions

#### EntityList CreateEntityList()
Creates a new EntityList.

#### table GetGlobalEntityList()
Returns all entities that are in the global entity list.<br>

### EntityList
This class should remove some overhead to improve performance since you can pass it to some functions.<br>
It is also used by other modules to improve their performance if they have to loop thru all entities or push entities to lua.<br>

#### string EntityList:\_\_tostring()
Returns `EntityList [NULL]` if given invalid list.<br>
Normally returns `EntityList [Entity number]`.<br>

#### EntityList:\_\_gc()
If called, it will free and nuke the entity list.<br>
You should never use it after this was called.<br>

#### EntityList:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any EntityList:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `EntityList:GetTable()`.<br>

#### table EntityList:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

> [!NOTE]
> On All other Objects this is named `:GetTable`

#### bool EntityList:IsValid()
Returns `true` if the EntityList is valid.<br>

#### table EntityList:GetEntities()
Returns a table with all the entities.<br>

#### EntityList:SetEntities(table entities)
Sets the EntityList from the given table.<br>

#### EntityList:AddEntities(table entities)
Adds the entities from the given table to the Entity list.<br>

#### EntityList:RemoveEntities(table entities)
Removes the entities that are in the table from the Entity list.<br>

#### EntityList:AddEntity(Entity ent)
Adds the given entity to the list.<br>

#### EntityList:RemoveEntity(Entity ent)
Removes the given entity from the list.<br>

## httpserver
This module adds a library with functions to create and run a httpserver.<br>

### Functions

#### HttpServer httpserver.Create()
Creates a new HTTPServer.<br>

#### httpserver.Destroy(HttpServer server)
Destroys the given http server.<br>

#### table httpserver.GetAll()
Returns a table containing all existing HttpServer in case you lose a reference.

#### HttpServer httpserver.FindByName(string name)
Returns the HttpServer with the given name set by `HttpServer:SetName()` or returns `nil` on failure.

### HttpServer
This class represents a created HttpServer.

#### string HttpServer:\_\_tostring()
Returns `HttpServer [NULL]` if given invalid list.<br>
Normally returns `HttpServer [Address:Port - Name]`.<br>

#### HttpServer:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any HttpServer:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `HttpServer:GetTable()`.<br>

#### table HttpServer:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### bool HttpServer:IsValid()
Returns `true` if the HttpServer is valid.<br>

#### HttpServer:Start(string address, number port)
This will start or restart the HTTP Server, and it will listen on the given address + port.<br>
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
The number of ms threads sleep before checking again if a request was handled.<br>
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
Sets the name of the HttpServer.<br>
The length of the name is limited to 64 characters.

#### (Experimental) HttpServer:AddPreparedResponse(number userID, string path, string method, table headers, function callback)
callback - function(HttpResponse response)<br>

Adds a prepared response for the given userID.<br>
A prepared response won't make any lua call when it matches a incoming request, multiple responses can be queued as their responsed in order.<br>
The callback function provides the HttpResponse object which you should use to set all return values.<br>

> [!NOTE]
> This is fully experimental.<br>
> Currently it doesn't have any real use except to remove the Thread overhead but I plan to make it more useful later.<br>

#### HttpServer:AddProxyAddress(string proxyAddress, string headerName, bool useSecondAddress)
useSecondAddress - If the given header provides two addresses seperated by a `,` it will use the second address. (proxies love to be funny. Now give me my Address!)<br>

Registers the given proxyAddress to be recognized, in which case it will look for the given header name and use it as the real address of the request sender.

Example:
```lua
local ip = game.GetIPAddress()
ip = ip:sub(0, ip:find(":") - 1) -- remove the port from the address

randomHttpServer:AddProxyAddress(ip, "X-Real-IP") -- This is an example where an nginx proxy running on the same host would pass a request to the http server with the X-Real-IP header being the client address
```

### Method Functions
All Method functions add a listener for the given path and the given method, like this:<br>
```lua
HttpServer:Get("/public", function(_, response)
<br>print("Public GET request")
<br>response.SetContent("You sent a GET request to a public site.", "text/plain")
end, false)

HttpServer:Get("/private", function(_, response)
<br>print("Private GET request")
<br>response.SetContent("You sent a GET request to a private site.", "text/plain")
end, true)
```

If you enable the IP Whitelist, only requests sent by connected players are processed.<br>

#### HttpServer:Get(string path, function (HttpRequest, HttpResponse), bool ipWhitelist)
#### HttpServer:Put(string path, function (HttpRequest, HttpResponse), bool ipWhitelist)
#### HttpServer:Post(string path, function (HttpRequest, HttpResponse), bool ipWhitelist)
#### HttpServer:Patch(string path, function (HttpRequest, HttpResponse), bool ipWhitelist)
#### HttpServer:Delete(string path, function (HttpRequest, HttpResponse), bool ipWhitelist)
#### HttpServer:Options(string path, function (HttpRequest, HttpResponse), bool ipWhitelist)

### HttpRequest
A incoming Http Request.

#### string HttpRequest:\_\_tostring()
Returns `HttpRequest [NULL]` if given invalid list.<br>
Normally returns `HttpRequest`.<br>

#### HttpRequest:\_\_gc()
When garbage collected, the request will be marked as hanled.<br>

#### HttpRequest:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any HttpRequest:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `HttpRequest:GetTable()`.<br>

#### table HttpRequest:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### bool HttpRequest:IsValid()
Returns `true` if the HttpRequest is valid.<br>

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
Returns the client who sent the HTTP Request or `nil` if it didn't find it.<br>

#### Player HttpRequest:GetPlayer()
Returns the player who sent the HTTP Request or `nil` if it didn't find it.<br>

#### string HttpRequest:GetPathParam(string param)
Returns the value of the given parameter or `nil` if it wasn't found.<br>

#### string HttpRequest:MarkHandled()
Marks this request as handled, invalidating this object and the linked `HttpResponse`<br>
This function is meant to be used when you `return true` in the HttpServer:[Get/Put/OtherStuff] callback function allowing you to delay a response.<br>

### HttpResponse
A Http Response.

#### string HttpResponse:\_\_tostring()
Returns `HttpResponse [NULL]` if given invalid list.<br>
Normally returns `HttpResponse`.<br>

#### HttpResponse:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any HttpResponse:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `HttpResponse:GetTable()`.<br>

#### table HttpResponse:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### bool HttpResponse:IsValid()
Returns `true` if the HttpResponse is valid.<br>

#### HttpResponse:SetContent(string content, string contentType = "text/plain")
Sets the content like this:
```lua
Response:SetContent("Hello World", "text/plain")
```

#### HttpResponse:SetRedirect(string url, number code = 302)
Redirects one to the given URL and returns the given code.

#### HttpResponse:SetHeader(string key, string value)
Sets the given value for the given key in the header.

#### HttpResponse:SetStatusCode(number statusCode)
Sets the status code of the response.<br>
The code is clamped between 100 and 600 internally, you cannot go above or below as else the status code won't be set!<br>

## luajit
This module updates luajit to a newer version.

> [!NOTE]
> By default this module is disabled.
> You can enable it by adding `-holylib_enable_luajit 1` to the command line.

> [!WARNING]
> It's **not** recommended to enable/disable it at runtime!<br>

The `string.buffer` package is already added by default.<br>

### FFI - cdata
cdata is slightly different as we changed it's typeID to be `7` instead of `10` since `10` is already used for `Vector` so we mark it to be `7(UserData)`.<br>

### Functions

#### table jit.getffi()
If ffi is enabled in the config, then this will return a valid table, else it will return nothing.<br>

#### jit.markFFITypeAsValidUserData(cdata data)
If ffi is enabled in the config, then this will allow you to mark cdata to be compatible with gmod allowing you to mimic types.<br>

#### jit.registerCreateHolyLibCDataFunc(string cTypeName)
A **Internall** function used by HolyLib to register the given cdata as the base userdata created & used by all holylib classes.<br>

#### table jit.getMetaTableByID(int metaID)
Pushes the metatable by their given metaID or returns `nil`.

### table jit.require(string name)
LuaJIT's default require function, this function does **not** exist when ffi is disabled.<br>

### debug.setreadonly(table tbl, bool readOnly = false)
Forces a table to become read only, meaning it cannot be modified in any way.<br>
This readonly logic was added into our LuaJIT build and does **not** exist in the normal LuaJIT.<br>

### bool debug.isreadonly(table tbl)
Checks if the table is set to be read only.

### debug.setblocked(function func)
Marks the function to be inaccessable by any debug function & `setfenv` & `getfenv`.<br>
This function is used internally for the FFI Scripts executed by HolyLib to prevent access to FFI functions when their disabled.<br>

> [!NOTE]
> Once set this intentionally cannot be reverted.

### bool debug.isblocked(function func)
Checks if the function is set to be inaccessable by any debug function.<br>

### Config

#### `luajit.enableFFI = false`
If set to `true`, `jit.require` will exist and `jit.getffi` will return ffi.<br>

#### `luajit.keepRemovedDebugFunctions = false`
If set to `true`, all debug function listed below are restored.<br>
`debug.setlocal`, `debug.setupvalue`, `debug.upvalueid` and `debug.upvaluejoin`<br>

## gameserver
This module adds a library that exposes the `CBaseServer` and `CBaseClient`.<br>

### Functions

#### number gameserver.GetNumClients()
returns current number of clients<br>

#### number gameserver.GetNumProxies()
returns current number of attached HLTV proxies<br>

#### number gameserver.GetNumFakeClients()
returns current number of fake clients/bots<br>

#### number gameserver.GetMaxClients()
returns current client limit<br>

#### number gameserver.GetUDPPort()
returns the udp port the server is running on.<br>

#### CGameClient gameserver.GetClient(number playerSlot)
Returns the CGameClient at that player slot or `nil` on failure.<br>

#### CGameClient gameserver.GetClientByUserID(number userID)
Returns the CGameClient for the given userID or `nil` on failure.<br>

#### CGameClient gameserver.GetClientBySteamID(string steamID)
Returns the CGameClient for the given steamID or `nil` on failure. (steamID not steamID64!)<br>

#### number gameserver.GetClientCount()
returns client count for iteration of `gameserver.GetClient`<br>

> [!NOTE]
> This will include inactive `CGameClient`'s since the engine re-uses them and doesn't delete them on disconnect.<br>
> If you want to get the count of active client's use `gameserver.GetNumClients()`

#### table gameserver.GetAll()
Returns a table that contains all game clients. It will return `nil` on failure.<br>

#### number gameserver.GetTime()
Returns the current time/curtime<br>

#### number gameserver.GetTick()
Returns the current tick<br>

#### number gameserver.GetTickInterval()
Returns the current tick interval<br>

#### string gameserver.GetName()
Returns the current server/host name.<br>
Should be the same as `GetConVar("hostname"):GetString()`

#### string gameserver.GetMapName()
Returns the current map name<br>

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
Returns the current server password.<br>

#### gameserver.SetMaxClients(number maxClients)
Sets the max client count.<br>
Should do nothing.<br>

#### gameserver.SetPaused(bool paused = false)
Pauses/Unpauses the server.<br>

> [!NOTE]
> Verify if this actually works

#### gameserver.SetPassword(string password)
Sets the server password.<br>

#### gameserver.BroadcastMessage(number type, string name, bf_write buffer)
Sends a custom net message to all clients.<br>
This function allows you send any netmessage you want.<br>
You should know what your doing since this function doesn't validate anything.<br>
You can find all valid types in the [protocol.h](https://github.com/RaphaelIT7/gmod-holylib/blob/main/source/sourcesdk/protocol.h#L86-L145)<br>

> [!NOTE]
> This was formerly known as `HolyLib.BroadcastCustomMessage`

Example usage, we enable `sv_cheats` clientside while having it disabled serverside.<br>
```lua
local bf = bitbuf.CreateWriteBuffer(64) -- Create a 64 bytes buffer.

bf:WriteByte(1) -- How many convars were sending
bf:WriteString("sv_cheats") -- ConVar name
bf:WriteString("1") -- ConVar value

-- You can use CBaseClient:SendNetMsg to send it to specific clients.
gameserver.BroadcastMessage(5, "NET_SetConVar", bf) -- 5 = net_SetConVar / net message type.
```

#### number gameserver.SendConnectionlessPacket(bf_write bf, string ip, bool useDNS = false, number socket = NS_SERVER)
ip - The target ip. Format `ip:port`<br>
Sends out a connectionless packet to the target ip.
Returns the length of the sent data or `-1` on failure.

> [!NOTE]
> It's expected that **YOU** already added the connectionless header, this was done to not have to copy the buffer.<br>
> `bf:WriteLong(-1) -- Write this as the first thing. This is the CONNECTIONLESS_HEADER`

Example on how to send a loopback packet:
```lua
hook.Add("HolyLib:ProcessConnectionlessPacket", "LoopbackExample", function(bf, ip)
	if ip != "loopback" then return end

	print("We got our own packet: " .. bf:ReadString())
	return true
end)

local bf = bitbuf.CreateWriteBuffer(64)
bf:WriteLong(-1)
bf:WriteString("Hello World")

-- We use NS_CLIENT as a socket because then the packet is queued into the Server loopback queue.
gameserver.SendConnectionlessPacket(bf, "loopback:" .. gameserver.GetUDPPort(), false, gameserver.NS_CLIENT)
```

#### CNetChan gameserver.CreateNetChannel(string ip, bool useDNS = false, number protocolVersion = 1, number socket = NS_SERVER)
ip - The target ip. Format `ip:port`<br>
Creates a net channel for the given ip.
Returns the channel or `nil` on failure.<br>

Example implementation of creating a working connection between two servers:
```lua
local REQUEST_CHANNEL = string.byte("z")
function BuildNetChannel(target, status) -- status should not be set when called
	local bf = bitbuf.CreateWriteBuffer(64)

	bf:WriteLong(-1) -- CONNECTIONLESS_HEADER
	bf:WriteByte(REQUEST_CHANNEL) -- Our header
<br><br>bf:WriteByte(status or 0) -- 0 = We requested it.

	gameserver.SendConnectionlessPacket(bf, target)
end

function IncomingNetMessage(channel, bf, length)
	print("Received a message at " .. tostring(channel), bf, length)
end

netChannels = netChannels or {}
hook.Add("HolyLib:ProcessConnectionlessPacket", "ProcessResponse", function(bf, ip)
	local header = bf:ReadByte()
	if header != REQUEST_CHANNEL then return end

	local status = bf:ReadByte()

	local netChannel = gameserver.CreateNetChannel(ip)
<br><br>netChannel:SetMessageCallback(function(bf, length)
<br><br>	IncomingNetMessage(netChannel, bf, length)
<br><br>end)
<br><br>table.insert(netChannels, netChannel)

<br><br>if status == 0 then
<br><br>	print("Created a requested net channel to " .. ip)

<br><br>	BuildNetChannel(ip, 1) -- Respond to the sender to confirm creation.
<br><br>elseif status == 1 then
<br><br>	print("Created our net channel to " .. ip)
<br><br>end
<br><br>
	return true
end)

function SendNetMessage(target, bf, reliable)
	for _, channel in ipairs(netChannels) do
		if not channel:IsValid() then continue end
		if channel:GetName() != target then continue end

		return channel:SendMessage(bf, reliable)
	end

	return false
end

hook.Add("Think", "UpdateNetChannels", function()
	for _, channel in ipairs(netChannels) do
		if not channel:IsValid() then continue end

		channel:ProcessStream() -- process any incomming messages
		channel:Transmit() -- Transmit out a update.
	end
end)

-- Install the script on two servers.
-- call BuildNetChannel with the target on one of the servers and on both servers a net channel is created

BuildNetChannel("127.0.0.1:27015")
```

#### CNetChan gameserver.RemoveNetChannel(CNetChan channel)
Removes/Destroys a net channel invalidating it.

#### table[CNetChan] gameserver.GetCreatedNetChannels()
Returns a table containing all net channels created by gameserver.CreateNetChannel.<br>

#### gameserver.NS_CLIENT = 0
Client socket.

#### gameserver.NS_SERVER = 1
Server socket.

#### gameserver.NS_HLTV = 2
HLTV socket.

### CBaseClient
This class represents a client.

#### CBaseClient:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any CBaseClient:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `CBaseClient:GetTable()`.<br>

#### table CBaseClient:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### number CBaseClient:GetPlayerSlot()
Returns the slot of the client. Use this for `sourcetv.GetClient`.<br>

#### number CBaseClient:GetUserID()
Returns the userid of the client.<br>

#### string CBaseClient:GetName()
Returns the name of the client.

#### string CBaseClient:GetSteamID()
Returns the steamid of the client.

#### CBaseClient:Reconnect()
Reconnects the client.

#### CBaseClient:ClientPrint(string message)
Prints the given message into the client's console.<br>

> [!NOTE]
> It **won't** add `\n` to the end, you have to add it yourself. 

#### bool CBaseClient:IsValid()
Returns `true` if the client is still valid.<br>

#### bool (Experimental) CBaseClient:SendLua(string code)
Sends the given code to the client to be executed.<br>
Returns `true` on success.<br>

> [!NOTE]
> This function was readded back experimentally. It wasn't tested yet. It's still broken but doesn't crash<br>

#### (Experimental) CBaseClient:FireEvent(IGameEvent event)<br>
Fires/sends the gameevent to this specific client.<br>

#### number CBaseClient:GetFriendsID()
Returns the friend id.<br>

#### string CBaseClient:GetFriendsName()
Returns the friend name.<br>

#### number CBaseClient:GetClientChallenge()
Returns the client challenge number.<br>

#### CBaseClient:SetReportThisFakeClient(bool reportClient = false)

#### bool CBaseClient:ShouldReportThisFakeClient()

#### CBaseClient:Inactivate()
Inactivates the client.<br>

> [!WARNING]
> Know what your doing when using it!

#### CBaseClient:Disconnect(string reason, bool silent = false, bool nogameevent = false)
silent - Silently closes the net channel and sends no disconnect to the client.
nogameevent - Stops the `player_disconnect` event from being created.

Disconnects the client.<br>

#### CBaseClient:SetRate(number rate)

#### number CBaseClient:GetRate()

#### CBaseClient:SetUpdateRate(number rate)

#### number CBaseClient:GetUpdateRate()

#### CBaseClient:Clear()
Clear the client/frees the CBaseClient for the next player to use.<br>

> [!WARNING]
> Know what your doing when using it!

#### CBaseClient:DemoRestart()

#### number CBaseClient:GetMaxAckTickCount()

#### CBaseClient:ExecuteStringCommand(string command)
Executes the given command as if the client himself ran it.<br>

#### CBaseClient:SendNetMsg(number type, string name, bf_write buffer)
Same as `gameserver.BroadcastMessage` but it only sends it to the specific player.<br>

> [!NOTE]
> This function was formerly known as `HolyLib.SendCustomMessage`<br>

#### bool CBaseClient:IsConnected()

#### bool CBaseClient:IsSpawned()

#### bool CBaseClient:IsActive()

#### number CBaseClient:GetSignonState()

#### bool CBaseClient:IsFakeClient()

#### bool CBaseClient:IsHLTV()

#### bool CBaseClient:IsHearingClient(number playerSlot)
Works like `voicechat.IsHearingClient` but only takes the direct player slot.<br>

#### bool CBaseClient:IsProximityHearingClient(number playerSlot)
Works like `voicechat.IsProximityHearingClient` but only takes the direct player slot.<br>

#### CBaseClient:SetMaxRoutablePayloadSize(number playloadSize)

#### CBaseClient:UpdateAcknowledgedFramecount(number tick)

#### bool CBaseClient:ShouldSendMessages(number tick)

#### CBaseClient:UpdateSendState()

#### bool CBaseClient:SetSignonState(number signOnState, number spawnCount = 0, bool rawSet = false)
Sets the SignOnState for the given client.<br>
Returns `true` on success.<br>

> [!NOTE]
> This function does normally **not** directly set the SignOnState.<br>
> Instead it calls the responsible function for the given SignOnState like for `SIGNONSTATE_PRESPAWN` it will call `SpawnPlayer` on the client.<br>
> Set the `rawSet` to `true` if you want to **directly** set the SignOnState.<br>

> [!NOTE]
> This function was formerly known as `HolyLib.SetSignOnState`

#### CBaseClient:UpdateUserSettings(bf_write buffer)

#### CBaseClient:SendServerInfo()

#### CBaseClient:SendSignonData()

#### CBaseClient:SpawnPlayer()

#### CBaseClient:ActivatePlayer()

#### CBaseClient:SetName(string name)
Sets the new name of the client.<br>

#### CBaseClient:SetUserCVar(string convar, string value)

#### CBaseClient:FreeBaselines()

#### CBaseClient:OnRequestFullUpdate()
Forces the client to go through a full update(also fires the gameevent).<br>

### bool CBaseClient:SetSteamID(string steamID64)
Sets the SteamID of the client.<br>
Returns `true` on success.<br>

> [!NOTE]
> Gmod seamingly has some backup code inside `CBaseClient::ProcessClientInfo`,<br>
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
Sets the time in seconds before the client is marked as timing out.<br>

#### bool CBaseClient:Transmit(bool onlyReliable = false, bool freeSubChannels = false)
Transmit any pending data to the client.<br>
Returns `true` on success.<br>

> [!WARNING]
> Transmitting data to the client causes the client's prediction to **reset and cause prediction errors!**.<br>

Exampe usage of this function:<br>
```lua
concommand.Add("nukechannel", function(ply)
<br> 	local string = ""
<br><br>for k = 1, 65532 do
<br><br><br><br>string = string .. "a"
<br><br>end
<br><br>util.AddNetworkString("Example")
<br><br>for k = 1, 10 do
<br><br><br><br>net.Start("Example", false)
<br><br><br><br><br><br>net.WriteString(string)
<br><br><br><br>net.Broadcast()
<br><br><br><br>gameserver.GetClient(ply:EntIndex()-1):Transmit() -- Forces the message to be transmitted directly avoiding a overflow.
<br><br>end 
end)
```

##### freeSubChannels argument
Marks all sub channel's of the client's net channel as freed allowing data to be transmitted again.<br>
It's a possible speed improvement yet fragments may get lost & cause issues / it's unsafe.<br>

Example code showing off the speed difference.<br>
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
the result is visible when downloading a map from the server(no workshop, no fastdl)<br>

https://github.com/user-attachments/assets/f700dae9-28a9-4c1f-b237-cb0547226d6a

#### (REMOVED) number CBaseClient:HasQueuedPackets()

#### bool CBaseClient:ProcessStream()
Processes all pending incoming net messages.<br>
Returns `true` on success.

#### CBaseClient:SetMaxBufferSize(bool reliable = false, number bytes, bool voice = false)
Resizes the specified buffer to the given size in bytes.<br>

> [!NOTE]
> All data inside that stream is discarded, make sure everything was sent out.<br>

Example usage of this function.<br>
```lua
concommand.Add("biggerBuffer", function(ply)
	local client = gameserver.GetClient(ply:EntIndex()-1)
<br><br>client:Transmit() -- Send everything out or else we lose data
<br><br>client:SetMaxBufferSize(true, 524288) -- We resize the reliable stream
end)
```

### CGameClient
This class inherits CBaseClient.

#### string CGameClient:\_\_tostring()
Returns the a formated string.<br>
Format: `CGameClient [%i][%s]`<br>
`%i` -> UserID<br>
`%s` -> ClientName<br>

### CNetChan
This class represents a client.

#### string CNetChan:\_\_tostring()
Returns the a formated string.<br>
Format: `CNetChan [%s]`<br>
`%s` -> channel name[ip:port]<br>

#### CNetChan:\_\_newindex(string key, any value)
Internally implemented and will set the values into the lua table.<br>

#### any CNetChan:\_\_index(string key)
Internally seaches first in the metatable table for the key.<br>
If it fails to find it, it will search in the lua table before returning.<br>
If you try to get multiple values from the lua table, just use `CNetChan:GetTable()`.<br>

#### table CNetChan:GetTable()
Returns the lua table of this object.<br>
You can store variables into it.<br>

#### bool CNetChan:IsValid()
Returns `true` if the channel is still valid.<br>

#### number CNetChan:GetAvgLoss(number flow)

#### number CNetChan:GetAvgChoke(number flow)

#### number CNetChan:GetAvgData(number flow)

#### number CNetChan:GetAvgLatency(number flow)

#### number CNetChan:GetAvgPackets(number flow)

#### number CNetChan:GetChallengeNr()

#### string CNetChan:GetAddress()
Returns the Address as `ip:port`

#### number CNetChan:GetDataRate()

#### number CNetChan:GetDropNumber()

#### CNetChan:SetChoked()

#### CNetChan:SetFileTransmissionMode(boolean backgroundTransmission = false)
If `true` files will be transmitted using a single fragment.<br>

#### CNetChan:SetCompressionMode(boolean compression = false)
If `true` enables compression.

#### CNetChan:SetDataRate(number rate)

#### number CNetChan:GetTime()

#### number CNetChan:GetTimeConnected()

#### number CNetChan:GetTimeoutSeconds()

#### number CNetChan:GetTimeSinceLastReceived()

#### number CNetChan:GetTotalData(number flow)

#### number CNetChan:GetBufferSize()

#### number CNetChan:GetProtocolVersion()

#### string CNetChan:GetName()
Returns the name of the channel.<br>
Normally `ip:port`

#### boolean CNetChan:GetProcessingMessages()
Returns `true` if it's currently processing messages.<br>

#### boolean CNetChan:GetClearedDuringProcessing()

#### number CNetChan:GetOutSequenceNr()

#### number CNetChan:GetInSequenceNr()

#### number CNetChan:GetOutSequenceNrAck()

#### number CNetChan:GetOutReliableState()

#### number CNetChan:GetInReliableState()

#### number CNetChan:GetChokedPackets()

#### bf_write CNetChan:GetStreamReliable()
Returns the reliable stream used by net messages.

#### bf_write CNetChan:GetStreamUnreliable()
Returns the unreliable stream used by net messages.

#### bf_write CNetChan:GetStreamVoice()
Returns the voice stream used by the voice chat.

#### number CNetChan:GetStreamSocket()

#### number CNetChan:GetMaxReliablePayloadSize()

#### number CNetChan:GetLastReceived()

#### number CNetChan:GetConnectTime()

#### number CNetChan:GetClearTime()

#### number CNetChan:GetTimeout()

#### CNetChan:SetTimeout(number seconds)
Sets the time in seconds before the channel is marked as timing out.

#### bool CNetChan:Transmit(bool onlyReliable = false, number fragments = -1, bool freeSubChannels = false)
Transmit any pending data to the channel.<br>
Returns `true` on success.

##### freeSubChannels argument
Marks all sub channel's of the channel as freed allowing data to be transmitted again.<br>
It's a possible speed improvement yet fragments may get lost & cause issues / it's unsafe.<br>

#### bool CNetChan:ProcessStream()
Processes all pending incoming net messages.<br>
Returns `true` on success.

#### CNetChan:SetMaxBufferSize(bool reliable = false, number bytes, bool voice = false)
Resizes the specified buffer to the given size in bytes.<br>

> [!NOTE]
> All data inside that stream is discarded, make sure everything was sent out.

#### CNetChan:Shutdown(string reason = nil)
Shuts down the channel.

#### CNetChan:SendMessage(bf_write buffer, boolean reliable = false)
Sends out the given buffer as a message.

#### CNetChan:SetMessageCallback(function callback)
callback -> `function(CNetChan channel/self, bf_read buffer, number length)`<br>

Sets the callback function for any incomming messages.<br>

#### function CNetChan:GetMessageCallback()
Returns the current message callback function.<br>

#### CNetChan:SetConnectionStartCallback(function callback)
callback -> `function(CNetChan channel/self`<br>

Sets the callback function for when the connection was established.<br>

#### function CNetChan:GetConnectionStartCallback()
Returns the current connection start callback function.<br>

#### CNetChan:SetConnectionClosingCallback(function callback)
callback -> `function(CNetChan channel/self, string reason)`<br>

Sets the callback function for when a connection is closed.<br>

#### function CNetChan:GetConnectionClosingCallback()
Returns the current connection closing callback function.<br>

#### CNetChan:SetConnectionCrashedCallback(function callback)
callback -> `function(CNetChan channel/self, string reason)`<br>

Sets the callback function for when a connection has crashed.<br>

#### function CNetChan:GetConnectionCrashedCallback()
Returns the current connection crashed callback function.<br>

### Hooks

#### bool HolyLib:OnSetSignonState(CGameClient client, number state, number spawnCount)
Called when the engine is about to change the client's SignonState.<br>
Return `true` to stop the engine.<br>

#### bool HolyLib:OnChannelOverflow(CGameClient client)
Called when the client's net channel is overflowed.
Return `true` to stop the engine from disconnecting the client.<br>

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
Called **after** a player was moved to a different slot.<br>
This happens when a player on a player slot above 128 tries to spawn.<br>

Why is this done? Because any player above 128 is utterly unstable and can only stabily exist as a CGameClient.<br>
if a CBasePlayer entity is created on a slot above 128 expect stability issues!<br>

#### HolyLib:OnClientDisconnect(CGameClient client)
Called when a client disconnects.

#### bool HolyLib:ProcessConnectionlessPacket(bf_read buffer, string ip)
Called when a connectionless packet is received.<br>
Return `true` to mark the packet as handled.<br>

Won't be called if `holylib_gameserver_connectionlesspackethook` is set to `0`.<br>

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

		if msgHeader != string.byte("I") then return end -- We didn't receive a A2S_INFO

		local a2s_info = {
			Header = string.char(msgHeader),
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
		return true
	end)
end

--- Example call
GetServerInfo("xxx.xxx.xxx.xxx:27015")
```

Output:
```txt
["Bots"]<br><br><br><br>=<br><br><br> 0
["Environment"] =<br><br><br> l
["ExtraDataFlag"]<br><br><br> =<br><br><br> 177
["Folder"]<br><br><br>=<br><br><br> garrysmod
["Game"]<br><br><br><br>=<br><br><br> Sandbox
["GameID"]<br><br><br>=<br><br><br> 4000
["Header"]<br><br><br>=<br><br><br> I
["ID"]<br>=<br><br><br> 4000
["Map"] =<br><br><br> gm_flatgrass
["MaxPlayers"]<br>=<br><br><br> 128
["Name"]<br><br><br><br>=<br><br><br> RaphaelIT7's Testing Hell
["Players"]<br><br> =<br><br><br> 0
["Port"]<br><br><br><br>=<br><br><br> 32108
["Protocol"]<br><br>=<br><br><br> 17
["ServerType"]<br>=<br><br><br> 100
["SteamID"]<br><br> =<br><br><br> [steamid]
["Tags"]<br><br><br><br>=<br><br><br><br>gm:sandbox gmc:other ver:250321
["VAC"] =<br><br><br> 1
["Version"]<br><br> =<br><br><br> 2024.10.29
["Visibility"]<br>=<br><br><br> 1
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

		if msgHeader != string.byte("D") then return end -- We didn't receive a A2S_PLAYER

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
		return true
	end)
end

--- Example call
GetServerPlayerInfo("xxx.xxx.xxx.xxx:27015")
```

Output:
```txt
[1]:
<br><br><br><br><br><br><br><br>["Duration"]<br><br>=<br><br><br> 40.337387084961
<br><br><br><br><br><br><br><br>["Index"]<br><br><br> =<br><br><br> 0
<br><br><br><br><br><br><br><br>["Name"]<br><br><br><br>=<br><br><br> Raphael
<br><br><br><br><br><br><br><br>["Score"]<br><br><br> =<br><br><br> 0
["Header"]<br><br><br>=<br><br><br> D
["Players"]<br><br> =<br><br><br> 1
```

#### number HolyLib:OnClientTimeout(CBaseClient pClient)
Called when a client is marked as timing out.<br>
Return a time in seconds to extent his timeout duration.<br>
If `0` or a number below `0` is returned, the client will be kicked normally for timing out.<br>

### ConVars

#### holylib_gameserver_disablespawnsafety (default `0`)
If enabled, players can spawn on slots above 128 but this WILL cause stability and many other issues!<br>
Added to satisfy curiosity & test around with slots above 128.

#### holylib_gameserver_connectionlesspackethook (default `1`)
If enabled, the HolyLib:ProcessConnectionlessPacket hook is active and will be called.

### sv_filter_nobanresponse (default `0`)
If enabled, a blocked ip won't be informed that its even blocked.

### Singleplayer
This module allows you to have a 1 slot / a singleplayer server.<br>
Why? I don't know, but you can.<br>

### 128+ Players
Yes, with this module you can go above 128 Players **BUT** it will currently crash.<br>
This is useful when you make a queue, players **can** connect and use all slots above 128 **but** they **can't** spawn when 128 players are already playing.<br>
Use the `HolyLib:OnSetSignonState` to keep players at the `SIGNONSTATE_NEW` until a slot is freed/one of the 128 disconnects.<br>

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

## autorefresh
The Autorefresh module currently provides functionalities regarding the in-built lua file autorefresh system.

Supports: Linux32 | LINUX64

> [!WARNING]
> This module is not yet finished and untested and is unstable/can easily crash on usage!

### Functions

#### bool(success) autorefresh.AddFileToRefresh(string filePath)
Adds the given file to the autorefresh list.

> [!NOTE]
> Adding single files is really slow and at high numbers can hurt performance! Use `autorefresh.AddFolderToRefresh` instead.

#### bool(success) autorefresh.RemoveFileFromRefresh(string filePath)
Removes the given file from the autorefresh list.

> [!NOTE]
> This **only** works on files added by `autorefresh.AddFileToRefresh`

#### bool(success) autorefresh.AddFolderToRefresh(string folderPath, bool recursive = false)
Adds the given folder to be watched for any file changes to autorefresh.

#### bool(success) autorefresh.RemoveFolderFromRefresh(string folderPath, bool recursive = false)
Removes the given folder from being watched for any file changes.

#### bool(success) autorefresh.BlockFileForRefresh(string filePatg, bool blockFile = false)
blockFile - `true` to block the file from autorefresh, `false` to unblock it.<br>
Blocks or unblocks the given file from being autorefreshed.

### autorefresh.RefreshFolders()
Resets autorefresh.<br>
Useful when you added/removed folders.

#### 

### Hooks

#### bool HolyLib:PreLuaAutoRefresh(string filePath, string fileName)
Called before a Lua file is being refreshed. If `true` is returned it will deny the refresh of the lua file.
- string filePath — is the filePath provided relative to the garrysmod folder
- string filename — is the filename without the extension
```lua
hook.Add("HolyLib:PreLuaAutoRefresh", "ExamplePreAutoRefresh", function(filePath, fileName)
    print("[BEFORE] FileChanged: " .. filePath .. filename)
        
    if filename == "bogos" then
        print("Denying Refresh")
    	return true -- prevent refresh
    end
end)
```

#### HolyLib:PostLuaAutoRefresh(string filePath, string fileName)
Called after a Lua file is refreshed. 
Note that if a refresh is being denied by PreLuaAutorefresh or DenyLuaAutoRefresh, this hook won't be called.
```lua
hook.Add("HolyLib:PostLuaAutoRefresh", "ExamplePostAutoRefresh", function(filePath, fileName)
    print("[AFTER] FileChanged: " .. filePath .. filename)
end)
```

## soundscape
The Soundscape module was added in `0.8` and allows you to get information about soundscapes and to control them for players.

Supports: Linux32

> [!WARNING]
> This module is not yet finished and untested

> [!NOTE]
> This mostly is for env_soundscape and will not do much for trigger_soundscape as thoes work noticably different.

### Functions

#### Entity soundscape.GetActiveSoundScape(Player player)
Returns the currently active soundscape for the given player

#### number soundscape.GetActiveSoundScapeIndex(Player player)
Returns the currently active soundscape index for the given player or `-1` if they have no soundscape active.

#### table(number - vector) soundscape.GetActivePositions(Player player)
Returns a table containing all positions used for local sounds that are linked to the currently active soundscape.<br>
Will return an empty table if the client has no active soundscape.

#### soundscape.SetActiveSoundscape(Player player, Entity soundscape)
Sets the soundscape entity for the given player, use soundscape.BlockEngineChanges as else the engine might override it.

> [!NOTE]
> The given entity must be a `env_soundscape` or else it will throw an error!

#### soundscape.BlockEngineChanges(Player player, bool shouldBlock = false)
Sets if the engine should be blocked from changing the soundscape of the given player.

#### table(number - string) soundscape.GetAll()
Returns a table containing all soundscapes that exist.<br>
key will be the soundscape index and value will be the name of the soundscape<br>
The table can possibly be non-sequential so use `pairs` instead of `ipairs` when iterating.<br>

#### string soundscape.GetNameByIndex(number soundscapeIndex)
Returns the name of the given soundscapeIndex or returns `nil` if not found.<br>

#### number soundscape.GetIndexByName(string name)
Returns the index of the given soundscape name or returns `nil` if not found.<br>

#### table(number - entity) soundscape.GetAllEntities()
Returns a sequential table containing all soundscape entities.<br>

#### soundscape.SetCurrentSoundscape(Entity soundscape)
Sets the soundscape entity for the currently active soundscapeUpdate.<br>
Only works inside the `HolyLib:OnSoundScapeUpdateForPlayer` hook!<br>

> [!NOTE]
> The given entity must be a `env_soundscape` or else it will throw an error!

#### soundscape.SetCurrentDistance(number distance)
Sets the distance for the currently active soundscapeUpdate.<br>
Only works inside the `HolyLib:OnSoundScapeUpdateForPlayer` hook!<br>
Mostly only useful to influence which soundscape could be selected.<br>

#### soundscape.SetCurrentPlayerPosition(Vector position)
Sets the player position for the currently active soundscapeUpdate.<br>
Only works inside the `HolyLib:OnSoundScapeUpdateForPlayer` hook!<br>
Mostly only useful to influence which soundscape could be selected.<br>

### Hooks

#### bool HolyLib:OnSoundScapeUpdateForPlayer(Entity currentSoundscape, Player currentPlayer)
Called before a soundscape tries to update for the given player.<br>
Return `true` to block the call and any further calls for this tick and additionally,<br>
the currently set soundscape entity for the active soundscapeUpdate will be applied to the player.<br>
You can set it using `soundscape.SetCurrentSoundscape` inside the hook.<br>

### ConVars

#### holylib_soundscape_updateplayerhook(default `0`)
If enabled, the `HolyLib:OnSoundScapeUpdateForPlayer` will be called.

## networkthreading
The Networkthreading module was added in `0.8` and starts a networking thread that will handle all incoming packets, filtering them and preparing them for the main thread for processing.<br>
The main purpose is simply moving filtering and prepaing of packets off the main thread and only doing the processing on it.<br>

Supports: Linux32

### ConVars

#### holylib_networkthreading_parallelprocessing(default `1`)
If enabled, some packets will be processed by the networking thread instead of the main thread.

> [!NOTE]
> This can cause the `HolyLib:ProcessConnectionlessPacket` to not be called for the affected packets!

# Unfinished Modules

## serverplugins
This module adds two new `IServerPluginCallbacks` functions:<br>
`virtual void OnLuaInit( GarrysMod::Lua::ILuaInterface* LUA )`<br>
`virtual void OnLuaShutdown( GarrysMod::Lua::ILuaInterface* LUA )`<br>

For a plugin to be loaded by HolyLib, you need to expose HolyLib's Interface.<br>
You can expose both HolyLib's and the normal interface since the order of the virtual functions are the same.<br>
This should already work but I never tested it completly yet (Or I forgot that I did test it).<br>

Supports: Linux32 | Linux64<br>

> [!NOTE]
> Windows doesn't have plugins / we won't support it there.<br>

# Issues implemented / fixed
`gameevent.GetListeners` -> https://github.com/Facepunch/garrysmod-requests/issues/2377<br>
`stringtable.FindTable("modelprecache"):GetNumStrings()` -> https://github.com/Facepunch/garrysmod-requests/issues/82<br>
`threadpoolfix` -> https://github.com/Facepunch/garrysmod-issues/issues/5932<br>
`HolyLib.Reconnect(ply)` -> https://github.com/Facepunch/garrysmod-requests/issues/2089<br>
`pvs.AddEntityToPVS(ent)` -> https://github.com/Facepunch/garrysmod-requests/issues/245<br>
`util.AsyncCompress(data, nil, nil, callback)` -> https://github.com/Facepunch/garrysmod-requests/issues/2165<br> 
It now throws a warning instead of crashing -> https://github.com/Facepunch/garrysmod-issues/issues/56<br>
`HolyLib.Reconnect(ply)` -> https://github.com/Facepunch/garrysmod-requests/issues/2089<br>
`concommand` module -> https://github.com/Facepunch/garrysmod-requests/issues/1534<br>
`vprof` module -> https://github.com/Facepunch/garrysmod-requests/issues/2374<br>
`cvar.GetAll` -> https://github.com/Facepunch/garrysmod-requests/issues/341<br>
`sourcetv` module -> https://github.com/Facepunch/garrysmod-requests/issues/2298<br>
`bitbuf` module(unfinished) -> https://github.com/Facepunch/garrysmod-requests/issues/594<br>
`HLTV` class / `sourcetv` module -> https://github.com/Facepunch/garrysmod-requests/issues/2237<br>
`surffix` module -> https://github.com/Facepunch/garrysmod-requests/issues/2306<br>
`filesystem.TimeCreated` & `filesystem.TimeAccessed` -> https://github.com/Facepunch/garrysmod-requests/issues/1633<br>
`systimer` module -> https://github.com/Facepunch/garrysmod-requests/issues/1671<br>
`pas` module -> https://github.com/Facepunch/garrysmod-requests/issues/140<br>
`HolyLib.InvalidateBoneCache` -> https://github.com/Facepunch/garrysmod-requests/issues/1920<br>
`HolyLib:PostEntityConstructor` -> https://github.com/Facepunch/garrysmod-requests/issues/2440<br>
`physenv` module -> https://github.com/Facepunch/garrysmod-issues/issues/642<br>
`physenv` module -> https://github.com/Facepunch/garrysmod-requests/issues/2522<br>
`physenv.EnablePhysHook` -> https://github.com/Facepunch/garrysmod-requests/issues/2541<br>

# Things that were fixed in gmod
https://github.com/Facepunch/garrysmod-issues/issues/6019<br>
https://github.com/Facepunch/garrysmod-issues/issues/5932#issuecomment-2420392562<br>
https://github.com/Facepunch/garrysmod-issues/issues/6031<br>

# Things planned to add:
https://github.com/Facepunch/garrysmod-requests/issues/1884<br>
(Maybe)https://github.com/Facepunch/garrysmod-requests/issues/132<br>
https://github.com/Facepunch/garrysmod-requests/issues/77<br>
(Maybe)https://github.com/Facepunch/garrysmod-requests/issues/603<br>
https://github.com/Facepunch/garrysmod-requests/issues/439<br>
https://github.com/Facepunch/garrysmod-requests/issues/1323<br>
(Should be possible?)https://github.com/Facepunch/garrysmod-requests/issues/756<br>
(Gonna make a seperate ConVar for it)https://github.com/Facepunch/garrysmod-requests/issues/2120<br>
https://github.com/Facepunch/garrysmod-requests/issues/1472<br>
(Maybe)https://github.com/Facepunch/garrysmod-requests/issues/2129<br>
(Maybe)https://github.com/Facepunch/garrysmod-requests/issues/1962<br>
(Maybe)https://github.com/Facepunch/garrysmod-requests/issues/1699<br><br>

# Some things for later

## NW Networking<br>
NW uses a usermessage `NetworkedVar`<br>
Write order:<br>
- WriteLong -> Entity handle<br>
- Char -> Type ID<br>
- String -> Var name (Planned to change to 12bits key index in next net compact. Update: This wasn't done -> https://github.com/Facepunch/garrysmod-issues/issues/5687#issuecomment-2438274430)<br>
- (Value) -> Var value. (Depends on what type it is)<br>

## NW2 Networking<br>
NW2 writes it to the baseline.<br>
Write order:<br>
- 12 bits - Number of net messages networked.<br>
- 1 bit - Magic bool. Idk if true, it wil call `GM:EntityNetworkedVarChanged` (Verify)<br>

loop:<br>
- 12 bits - var key index<br>
- 3 bits - var type<br>
- x bits -var value<br>

## `client_lua_files` Stringtable behavior
Key `0` contains **always** all datapack paths(`;` is used as a seperator).<br>
Example:
```lua
local client_lua_files = stringtable.FindTable("client_lua_files")
local pathData = client_lua_files:GetStringUserData(0)
local pathTable = string.Split(pathData, ";")
PrintTable(pathTable)
```

### Multiplayer
All other keys are a lua filename.<br>

### Singleplayer
The engine merges all lua files into a single/multiple keys named `singleplayer_files%i`.<br>
Inside the string's userdata a long string is stored representing all files(`:` is used as a seperator).<br>
Example(Won't work on a dedicated server):
```lua
local client_lua_files = stringtable.FindTable("client_lua_files")
local fileData = client_lua_files:GetStringUserData(client_lua_files:FindStringIndex("singleplayer_files0"))
local fileTable = string.Split(fileData, ":")
PrintTable(fileTable)
```

## Usage with vphysics jolt
Currently there might be bugs when combining holylib with VPhysics-Jolt.<br>
This mainly affects the `physenv` module and most other modules should be completely fine.<br>
Only VPhysics-Jolt builds from https://github.com/RaphaelIT7/VPhysics-Jolt will be suppored for now due to holylib requiring extented functionality.<br>
