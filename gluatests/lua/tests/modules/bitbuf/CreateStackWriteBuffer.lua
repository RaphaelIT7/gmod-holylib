return {
    groupName = "bitbuf.CreateStackWriteBuffer",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("bitbuf"),
            func = function()
                expect( bitbuf.CreateStackWriteBuffer ).to.beA( "function" )
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
            name = "Properly creates a bf_write buffer, not a bf_read one",
            when = HolyLib_IsModuleEnabled("bitbuf"),
            func = function()
                bitbuf.CreateStackWriteBuffer(64, function( buf )
                    expect( buf:IsValid() ).to.beTrue()
                    expect( buf ).to.beA( "bf_write" )

                    buf:WriteByte(123)-- Should be fine now

                    collectgarbage() -- Since it's stack allocated this should not hit our buffer in any way
                end)
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("bitbuf"),
            func = function()
                local callback = function() end
                HolyLib_RunPerformanceTest("bitbuf.CreateStackWriteBuffer", function() bitbuf.CreateStackWriteBuffer(64, callback) end)
            end
        },
    }
}
