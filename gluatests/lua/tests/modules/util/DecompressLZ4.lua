return {
    groupName = "util.DecompressLZ4",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.DecompressLZ4 ).to.beA( "function" )
            end
        },
        {
            name = "Metatable doesn't exist",
            when = not HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.DecompressLZ4 ).to.beA( "nil" )
            end
        },
        {
            name = "Errors when no data is given",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.DecompressLZ4 ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when data isn't a string",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.DecompressLZ4, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Returns nil for data that is too short to be valid",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.DecompressLZ4( "short" ) ).to.beNil()
            end
        },
        {
            name = "Returns nil for data with an invalid header",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.DecompressLZ4( "ThisIsDefinitelyNotValidLZ4Data" ) ).to.beNil()
            end
        },
        {
            name = "Successfully decompresses data produced by util.CompressLZ4",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                local data = string.rep("HolyLib was here! ", 100)
                local compressed = util.CompressLZ4( data )

                expect( util.DecompressLZ4( compressed ) ).to.equal( data )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                local compressed = util.CompressLZ4( string.rep("HolyLib was here! ", 100) )
                HolyLib_RunPerformanceTest("util.DecompressLZ4", function() util.DecompressLZ4(compressed) end)
            end
        },
    }
}
