return {
    groupName = "bitbuf.CreateReadBuffer",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("bitbuf"),
            func = function()
                expect( bitbuf.CreateReadBuffer ).to.beA( "function" )
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
                local buf = bitbuf.CreateReadBuffer("Test123")

                expect( buf:IsValid() ).to.beTrue()
                expect( buf ).to.beA( "bf_read" )
            end
        },
        {
            name = "Properly creates a buffer from a string",
            when = HolyLib_IsModuleEnabled("bitbuf"),
            func = function()
                HolyLib_RunPerformanceTest("bitbuf.CreateReadBuffer", bitbuf.CreateReadBuffer, "Test123")
            end
        },
    }
}
