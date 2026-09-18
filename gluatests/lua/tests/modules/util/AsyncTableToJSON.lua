return {
    groupName = "util.AsyncTableToJSON",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncTableToJSON ).to.beA( "function" )
            end
        },
        {
            name = "Metatable doesn't exist",
            when = not HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncTableToJSON ).to.beA( "nil" )
            end
        },
        {
            name = "Errors when no table is given",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncTableToJSON ).to.errWith( "bad argument #1 to '?' (table expected, got no value)" )
            end
        },
        {
            name = "Errors when the first argument isn't a table",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncTableToJSON, true ).to.errWith( "bad argument #1 to '?' (table expected, got boolean)" )
            end
        },
        {
            name = "Errors when no callback is given",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncTableToJSON, {} ).to.errWith( "bad argument #2 to '?' (function expected, got no value)" )
            end
        },
        {
            name = "Errors when the callback isn't a function",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.AsyncTableToJSON, {}, true ).to.errWith( "bad argument #2 to '?' (function expected, got boolean)" )
            end
        },
        {
            name = "Errors when the luajit module isn't enabled",
            when = HolyLib_IsModuleEnabled("util") and not HolyLib_IsModuleEnabled("luajit"),
            func = function()
                expect( util.AsyncTableToJSON, {}, function() end ).to.errWith( "This function is not functional without the luajit module enabled!" )
            end
        },
        {
            name = "Converts a table to JSON asynchronously",
            when = HolyLib_IsModuleEnabled("util") and HolyLib_IsModuleEnabled("luajit"),
            async = true,
            timeout = 2,
            func = function()
                local tbl = {
                    [1] = 1,
                    [2] = 2,
                    [3] = 3,
                    ["Test"] = "Hello World",
                }

                util.AsyncTableToJSON( tbl, function( json )
                    expect( json ).to.beA( "string" )
                    expect( json ).to.equal( util.FancyTableToJSON( tbl ) )

                    done()
                end )
            end
        },
        {
            name = "Supports the pretty print flag",
            when = HolyLib_IsModuleEnabled("util") and HolyLib_IsModuleEnabled("luajit"),
            async = true,
            timeout = 2,
            func = function()
                local tbl = {
                    ["Test"] = "Hello World",
                }

                util.AsyncTableToJSON( tbl, function( json )
                    expect( json ).to.equal( util.FancyTableToJSON( tbl, true ) )
                    expect( json ).toNot.equal( util.FancyTableToJSON( tbl, false ) )

                    done()
                end, true )
            end
        },
    }
}
