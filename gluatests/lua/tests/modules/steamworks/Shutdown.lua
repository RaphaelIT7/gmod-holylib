return {
    groupName = "steamworks.Shutdown",
    cases = {
        {
            name = "Function exists on the built-in steamworks table",
            when = HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( steamworks.Shutdown ).to.beA( "function" )
            end
        },
        {
            name = "Function doesn't exist",
            when = not HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( steamworks.Shutdown ).to.beA( "nil" )
            end
        },
        -- Not invoked: logs the server off Steam entirely; unsafe for CI.
    }
}
