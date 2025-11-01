local exampleTable = {
    [1] = 1,
    [2] = 2,
    [3] = 3,
    ["Test"] = "Hello World", -- Stuff like that once crashed, see https://github.com/RaphaelIT7/gmod-holylib/commit/3d0ab6c2a132f8e96a3248dfb73650d67a6c4554
    [4] = 4,
    [5] = 5,
    [6] = {
        [1] = Vector(1, 2, 3),
        [2] = Vector(4, 5, 6),
        [3] = Vector(7, 8, 9),
    },
    [7] = math.huge,
    [8] = {
        [1] = Angle(1, 2, 3),
        [2] = Angle(4, 5, 6),
        [3] = Angle(7, 8, 9),
    },
}

return {
    groupName = "util.FancyTableToJSON",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.FancyTableToJSON ).to.beA( "function" )
            end
        },
        {
            name = "Metatable doesn't exist",
            when = not HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.FancyTableToJSON ).to.beA( "nil" ) -- util table always exists since its created by gmod
            end
        },
        {
            name = "Returns a proper result",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                local actualTable = util.FancyTableToJSON( exampleTable )

                expect( actualTable ).to.equal( "{\"1\":1,\"2\":2,\"3\":3,\"4\":4,\"5\":5,\"6\":[\"[1 2 3]\",\"[4 5 6]\",\"[7 8 9]\"],\"7\":null,\"8\":[\"{1 2 3}\",\"{4 5 6}\",\"{7 8 9}\"],\"Test\":\"Hello World\"}" )
            end
        },
        {
            name = "Results includes null",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                local actualTable = util.FancyTableToJSON( { math.huge } )

                expect( actualTable ).to.equal( "[null]" ) -- We differ from gmod, as Gmod hides null / would be []
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                HolyLib_RunPerformanceTest("util.FancyTableToJSON", util.FancyTableToJSON, exampleTable)
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                HolyLib_RunPerformanceTest("util.TableToJSON(GMOD)", util.TableToJSON, exampleTable)
            end
        },
    }
}
