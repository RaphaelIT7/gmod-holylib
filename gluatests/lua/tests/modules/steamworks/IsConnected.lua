return {
    groupName = "steamworks.IsConnected",
    cases = {
        {
            name = "Function exists on the built-in steamworks table",
            when = HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( steamworks.IsConnected ).to.beA( "function" )
            end
        },
        {
            name = "Function doesn't exist",
            when = not HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( steamworks.IsConnected ).to.beA( "nil" )
            end
        },
        {
            name = "Doesn't error and returns a boolean",
            when = HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( type( steamworks.IsConnected() ) ).to.equal( "boolean" )
            end
        },
        {
            name = "Returns a consistent value across calls",
            when = HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( steamworks.IsConnected() ).to.equal( steamworks.IsConnected() )
            end
        },
        {
            name = "Ignores extra arguments instead of erroring",
            when = HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( type( steamworks.IsConnected( "extra", 123 ) ) ).to.equal( "boolean" )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                HolyLib_RunPerformanceTest("steamworks.IsConnected", function() steamworks.IsConnected() end)
            end
        },
    }
}
