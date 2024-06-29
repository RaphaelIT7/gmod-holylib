# Holylib

A library that will contain some functions and optimizations for gmod.  
If you need any function, make an issue for it, and I'll look into it.  

# Modules

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

## serverplugins
This module adds two new `IServerPluginCallbacks` functions:  
`virtual void OnLuaInit( GarrysMod::Lua::ILuaInterface* LUA )`  
`virtual void OnLuaShutdown( GarrysMod::Lua::ILuaInterface* LUA )`  

## sourcetv
This module plans to add a new `sourcetv` library and a new class `HLTVPlayer` will allow a SourceTV client to send net messages to the server.

# Things planned to add:
https://github.com/Facepunch/garrysmod-requests/issues/1884  
https://github.com/Facepunch/garrysmod-requests/issues/2298  
https://github.com/Facepunch/garrysmod-requests/issues/2237  
https://github.com/Facepunch/garrysmod-requests/issues/2165  
https://github.com/Facepunch/garrysmod-requests/issues/82  
(Maybe)https://github.com/Facepunch/garrysmod-requests/issues/132  
https://github.com/Facepunch/garrysmod-requests/issues/245  
