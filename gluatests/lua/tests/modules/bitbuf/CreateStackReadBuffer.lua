return {
    groupName = "bitbuf.CreateStackReadBuffer",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("bitbuf"),
            func = function()
                expect( bitbuf.CreateStackReadBuffer ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("bitbuf"),
            func = function()
                expect( bitbuf ).to.beA( "nil" )
            end
        },
        {
            name = "Properly creates a buffer from a string",
            when = HolyLib_IsModuleEnabled("bitbuf"),
            func = function()
                local buf = bitbuf.CreateStackReadBuffer("Test123", function( buf ) -- It's not async.
                    expect( buf:IsValid() ).to.beTrue()
                    expect( buf ).to.beA( "bf_read" )
                end)
            end
        },
        {
            name = "Properly creates a buffer from a string",
            when = HolyLib_IsModuleEnabled("bitbuf"),
            func = function()
                HolyLib_RunPerformanceTest("bitbuf.CreateStackReadBuffer", bitbuf.CreateStackReadBuffer, "Test123", function() end)
            end
        },
    }
}
