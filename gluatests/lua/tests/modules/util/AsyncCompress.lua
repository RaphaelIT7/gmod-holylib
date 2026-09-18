return {
    groupName = "util.AsyncCompress",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncCompress ).to.beA( "function" )
            end
        },
        {
            name = "Metatable doesn't exist",
            when = not HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncCompress ).to.beA( "nil" )
            end
        },
        {
            name = "Errors when no data is given",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncCompress ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when data isn't a string",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncCompress, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Errors when no callback is given",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncCompress, "Hello World" ).to.errWith( "bad argument #4 to '?' (function expected, got no value)" )
            end
        },
        {
            name = "Errors when level isn't a number",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncCompress, "Hello World", "notanumber" ).to.errWith( "bad argument #2 to '?' (number expected, got string)" )
            end
        },
        {
            name = "Errors when dictSize isn't a number",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncCompress, "Hello World", 5, "notanumber" ).to.errWith( "bad argument #3 to '?' (number expected, got string)" )
            end
        },
        {
            name = "Compresses data asynchronously using the 2 argument form",
            when = HolyLib_IsModuleEnabled("util"),
            async = true,
            timeout = 2,
            func = function()
                local data = string.rep("HolyLib was here! ", 100)

                util.AsyncCompress( data, function( compressed )
                    expect( compressed ).toNot.beNil()
                    expect( compressed ).toNot.equal( data )
                    expect( util.Decompress( compressed ) ).to.equal( data )

                    done()
                end )
            end
        },
        {
            name = "Compresses data asynchronously using the 4 argument form",
            when = HolyLib_IsModuleEnabled("util"),
            async = true,
            timeout = 2,
            func = function()
                local data = string.rep("HolyLib was here! ", 100)

                util.AsyncCompress( data, 9, 65536, function( compressed )
                    expect( compressed ).toNot.beNil()
                    expect( compressed ).toNot.equal( data )
                    expect( util.Decompress( compressed ) ).to.equal( data )

                    done()
                end )
            end
        },
    }
}
