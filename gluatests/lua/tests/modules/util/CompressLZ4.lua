return {
    groupName = "util.CompressLZ4",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.CompressLZ4 ).to.beA( "function" )
            end
        },
        {
            name = "Metatable doesn't exist",
            when = not HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.CompressLZ4 ).to.beA( "nil" )
            end
        },
        {
            name = "Errors when no data is given",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.CompressLZ4 ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when data isn't a string",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.CompressLZ4, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Errors when accelerationLevel isn't a number",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.CompressLZ4, "Hello World", "notanumber" ).to.errWith( "bad argument #2 to '?' (number expected, got string)" )
            end
        },
        {
            name = "Returns nil for an empty string",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.CompressLZ4( "" ) ).to.beNil()
            end
        },
        {
            name = "Returns a compressed string that can be decompressed back",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                local data = string.rep("HolyLib was here! ", 100)
                local compressed = util.CompressLZ4( data )

                expect( compressed ).toNot.beNil()
                expect( util.DecompressLZ4( compressed ) ).to.equal( data )
            end
        },
        {
            name = "Works with a custom acceleration level",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                local data = string.rep("HolyLib was here! ", 100)
                local compressed = util.CompressLZ4( data, 8 )

                expect( compressed ).toNot.beNil()
                expect( util.DecompressLZ4( compressed ) ).to.equal( data )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                local data = string.rep("HolyLib was here! ", 100)
                HolyLib_RunPerformanceTest("util.CompressLZ4", function() util.CompressLZ4(data) end)
            end
        },
    }
}
