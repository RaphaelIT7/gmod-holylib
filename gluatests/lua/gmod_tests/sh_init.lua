-- Helpers and utilities for the gmod_tests test suite
-- (TODO: Do these belong somewhere else)
AddCSLuaFile()

WHEN_NO_HUMANS = function()
    return #player.GetHumans() == 0
end

IS_BASE_BRANCH = BRANCH != "x86-64"
IS_64BIT_BRANCH = BRANCH == "x86-64"

-- NOTE: The names could be wrong. I took them from here: https://github.com/RaphaelIT7/obsolete-source-engine/blob/gmod/public/tier1/iconvar.h#L41
FCVAR_DEVELOPMENTONLY = bit.lshift( 1, 1 )
FCVAR_HIDDEN = bit.lshift( 1, 4 )
FCVAR_INTERNAL_USE = bit.lshift( 1, 15 )
FCVAR_RELOAD_MATERIALS = bit.lshift( 1, 20 )
FCVAR_RELOAD_TEXTURES = bit.lshift( 1, 21 )
FCVAR_MATERIAL_SYSTEM_THREAD = bit.lshift( 1, 23 )
FCVAR_ACCESSIBLE_FROM_THREADS = bit.lshift( 1, 25 )
FCVAR_AVAILABLE1 = bit.lshift( 1, 26 )
FCVAR_AVAILABLE2 = bit.lshift( 1, 27 )

function HolyLib_IsModuleEnabled(name)
	return GetConVar("holylib_enable_" .. name):GetBool()
end

require("reqwest")
HTTP = reqwest or HTTP

-- ConVar's don't work since thoes would need to exist before it tries to set all values from the command line. (Maybe make a gmod request? idk)
-- local loki_host = CreateConVar("holylib_loki_host", "", {FCVAR_DONTRECORD, FCVAR_PROTECTED, FCVAR_UNLOGGED}, "Loki host secret.")
-- local loki_api = CreateConVar("holylib_loki_api", "", {FCVAR_DONTRECORD, FCVAR_PROTECTED, FCVAR_UNLOGGED}, "Loki api key secret.")

local loki_host = file.Read("loki_host.txt", "MOD")
local loki_api = file.Read("loki_api.txt", "MOD")
function HolyLib_RunPerformanceTest(name, callback, ...)
    if not loki_host or loki_host == "" then
        print("Skipping performance test \"" .. name .. "\" since were missing Loki.")
        return
    end

    local startTime = SysTime()
    local totalCalls = 0
    while (SysTime() - startTime) < 2 do -- We spend a total of 2 seconds to run these
        callback(...)
        totalCalls = totalCalls + 1
    end
    
    local totalTime = SysTime() - startTime -- Should almost always be 1 second
    local timePerCall = totalTime / totalCalls
    print("Finished performance test for \"" .. name .. "\". Took " .. totalTime .. "s with a total of " .. totalCalls .." calls (" .. timePerCall .. "s per call)")

    HTTP({
        blocking = true,
        failed = function(reason)
            print("Failed to send performance results to Loki!", reason)
        end,
        success = function(code, body, headers)
            print("Successfully sent performance results to Loki :3", code)
        end,
        method = "POST",
        url = string.Trim(loki_host) .. "/loki/api/v1/push",
        type = "application/json",
        headers = {
            ["X-Api-Key"] = string.Trim(loki_api),
        },
        body = util.TableToJSON({
            streams = {
                {
                    stream = {
                        run_number = _HOLYLIB_RUN_NUMBER,
                        branch = _HOLYLIB_BRANCH,
                    },
                    values = {
                        {
                            string.format("%.0f", os.time() * 1000 * 1000 * 1000), -- Need nano time
                            util.TableToJSON({
                                ["totalCalls"] = totalCalls,
                                ["totalTime"] = totalTime,
                                ["gmodBranch"] = BRANCH,
                                ["name"] = name,
                            })
                        }
                    }
                }
            }
        })
    })
end

if SERVER then
    --- Makes an entity for test purposes
    --- @param class string? The class of the entity
    --- @param model string? The model of the entity
    --- @param shouldSpawn boolean? Whether the entity should be :Spawn()'d
    --- @param shouldFreeze boolean? Whether the entity should be :Freeze()'d (only valid if shouldSpawn is true)
    MakeTestEntity = function( class, model, shouldSpawn, shouldFreeze )
        shouldSpawn = shouldSpawn == true

        local ent = ents.Create( class or "prop_physics" )
        ent:SetModel( model or "models/props_c17/oildrum001.mdl" )

        if shouldSpawn then
            ent:Spawn()

            if shouldFreeze then
                local physObj = ent:GetPhysicsObject()
                physObj:EnableMotion( false )
            end
        end

        return ent
    end

    local botCounter = 0
    --- Makes a bot for test purposes
    --- @param name string? The name of the bot
    MakeTestBot = function( name )
        botCounter = botCounter + 1

        name = name or ("Bot " .. botCounter)
        return player.CreateNextBot( name )
    end

    --- @class TestEntityConfig
    --- @field class string? The class of the entity
    --- @field model string? The model of the Entity
    --- @field shouldSpawn boolean? Whether the entity should be :Spawn()'d
    --- @field shouldFreeze boolean? Whether the entity should be :Freeze()'d (only valid if shouldSpawn is true)
    --- @field createdCallback fun(ent: Entity)? A callback to run after the entity is created

    --- Sets up a testGroup to make a test ent for each test, and remove it after each test
    --- @param testGroup table The test group to modify
    --- @param config TestEntityConfig? The configuration for the test entity
    WithTestEntity = function( testGroup, config )
        config = config or {}

        testGroup.beforeEach = function( state )
            state.ent = MakeTestEntity( config.class, config.model, config.shouldSpawn, config.shouldFreeze )

            local cb = config.createdCallback
            if cb then cb( state.ent ) end
        end

        testGroup.afterEach = function( state )
            SafeRemoveEntity( state.ent )
        end

        return testGroup
    end

    --- @class TestBotConfig
    --- @field name? string The name of the bot
    --- @field createdCallback fun(ply: Player)? A callback to run after the bot is created

    --- Sets up a testGroup to make a test bot for each test, and remove it after each test
    --- @param testGroup table The test group to modify
    --- @param config TestBotConfig? The configuration for the test bot
    WithTestBot = function( testGroup, config )
        config = config or {}

        testGroup.beforeEach = function( state )
            state.bot = MakeTestBot( config.name )

            local cb = config.createdCallback
            if cb then cb( state.bot ) end
        end

        testGroup.afterEach = function( state )
            local bot = state.bot

            if not IsValid( bot ) then return end
            bot:Kick()
        end
    end

    -- string table need unique names as else if one of the tests fail they might collide with each other.
    GetTestStringTableName = function()
        StringTableCounter = (StringTableCounter or 0) + 1

        return "testTable-" .. StringTableCounter
    end
end

-- Helper utility to isolate test groups
-- If any test group has `yes = true`, then only test groups that have `yes = true` will be run
hook.Add( "GLuaTest_StartedTestRun", "Yes", function( testGroups )
    local hasYes = false
    local groupCount = #testGroups

    local toRemove = {}

    for i = 1, groupCount do
        local group = testGroups[i]

        if group.yes then
            hasYes = true
        else
            table.insert( toRemove, 1, i )
        end
    end

    if not hasYes then return end

    for _, i in ipairs( toRemove ) do
        table.remove( testGroups, i )
    end
end )