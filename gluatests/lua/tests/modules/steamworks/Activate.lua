return {
    groupName = "steamworks.Activate",
    cases = {
        {
            name = "Function exists on the built-in steamworks table",
            when = HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( steamworks.Activate ).to.beA( "function" )
            end
        },
        {
            name = "Function doesn't exist",
            when = not HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( steamworks.Activate ).to.beA( "nil" )
            end
        },
        -- Not invoked: real, network-dependent Steam reconnect; unsafe for CI.
    }
}
