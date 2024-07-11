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

## precachefix
This module removes the host error when the model or generic precache stringtable overflows. Instead it will throw a warning.  
If these stringtables overflow, expect the models that failed to precache to be errors.  

# Unfinished Modules

## serverplugins
This module adds two new `IServerPluginCallbacks` functions:  
`virtual void OnLuaInit( GarrysMod::Lua::ILuaInterface* LUA )`  
`virtual void OnLuaShutdown( GarrysMod::Lua::ILuaInterface* LUA )`  

## sourcetv
This module plans to add a new `sourcetv` library and a new class `HLTVPlayer` will allow a SourceTV client to send net messages to the server.

## stringtable
This module plans to add a new library called `stringtable` later on, which will contain all functions to handle stringtables,  
and it will a hook for when the stringtables are created, since they are created while Lua was already loaded.

### Functions

#### (int or table) stringtable.GetSize(string tablename)
string name(optional) - The stringtable to get the size of.  
Returns nil if the stringtable wasn't found.

# Things planned to add:
https://github.com/Facepunch/garrysmod-requests/issues/1884  
https://github.com/Facepunch/garrysmod-requests/issues/2298  
https://github.com/Facepunch/garrysmod-requests/issues/2237  
https://github.com/Facepunch/garrysmod-requests/issues/2165  
https://github.com/Facepunch/garrysmod-requests/issues/82  
(Maybe)https://github.com/Facepunch/garrysmod-requests/issues/132  
https://github.com/Facepunch/garrysmod-requests/issues/245  
