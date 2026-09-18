return {
    groupName = "steamworks.GetGameServerSteamID",
    cases = {
        {
            name = "Function exists on the built-in steamworks table",
            when = HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( steamworks.GetGameServerSteamID ).to.beA( "function" )
            end
        },
        {
            name = "Function doesn't exist",
            when = not HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( steamworks.GetGameServerSteamID ).to.beA( "nil" )
            end
        },
        {
            name = "Returns the steamid64 as a string",
            when = HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                local steamID64 = steamworks.GetGameServerSteamID()

                expect( steamID64 ).to.beA( "string" )
                expect( tonumber( steamID64 ) ).toNot.equal( nil )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                HolyLib_RunPerformanceTest("steamworks.GetGameServerSteamID", function() steamworks.GetGameServerSteamID() end)
            end
        },
    }
}
