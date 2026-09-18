return {
    groupName = "steamworks.ForceActivate",
    cases = {
        {
            name = "Function exists on the built-in steamworks table",
            when = HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( steamworks.ForceActivate ).to.beA( "function" )
            end
        },
        {
            name = "Function doesn't exist",
            when = not HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( steamworks.ForceActivate ).to.beA( "nil" )
            end
        },
        -- Not invoked: forces a real Steam game server reconnect; unsafe for CI.
    }
}
