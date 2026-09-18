return {
    groupName = "util.AsyncDecompress",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncDecompress ).to.beA( "function" )
            end
        },
        {
            name = "Metatable doesn't exist",
            when = not HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncDecompress ).to.beA( "nil" )
            end
        },
        {
            name = "Errors when no data is given",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncDecompress ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when data isn't a string",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncDecompress, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Errors when no callback is given",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncDecompress, "Hello World" ).to.errWith( "bad argument #2 to '?' (function expected, got no value)" )
            end
        },
        {
            name = "Errors when callback isn't a function",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncDecompress, "Hello World", true ).to.errWith( "bad argument #2 to '?' (function expected, got boolean)" )
            end
        },
        {
            name = "Errors when ratio isn't a number",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncDecompress, "Hello World", function() end, "notanumber" ).to.errWith( "bad argument #3 to '?' (number expected, got string)" )
            end
        },
        {
            name = "Decompresses previously compressed data asynchronously",
            when = HolyLib_IsModuleEnabled("util"),
            async = true,
            timeout = 2,
            func = function()
                local data = string.rep("HolyLib was here! ", 100)
                __Decompressed_Data = util.Compress( data ) -- must not be GCd!

                util.AsyncDecompress( __Decompressed_Data, function( decompressed )
                    expect( decompressed ).to.equal( data )

                    done()
                end )
            end
        },
        {
            name = "Returns nil for data that is too short to be valid",
            when = HolyLib_IsModuleEnabled("util"),
            async = true,
            timeout = 2,
            func = function()
            	__Decompresse_Short = "short" -- must not be GCd!
                util.AsyncDecompress( __Decompresse_Short, function( decompressed )
                    expect( decompressed ).to.beNil()

                    done()
                end )
            end
        },
        {
            name = "Returns nil when ratio is 0",
            when = HolyLib_IsModuleEnabled("util"),
            async = true,
            timeout = 2,
            func = function()
                local data = string.rep("HolyLib was here! ", 100)
                __Decompressed_ratio = util.Compress( data ) -- must not be GCd!

                util.AsyncDecompress( __Decompressed_ratio, function( decompressed )
                    expect( decompressed ).to.beNil()

                    done()
                end, 0 )
            end
        },
    }
}
