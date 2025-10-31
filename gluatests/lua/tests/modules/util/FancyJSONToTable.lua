return {
    groupName = "util.FancyJSONToTable",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.FancyJSONToTable ).to.beA( "function" )
            end
        },
        {
            name = "Metatable doesn't exist",
            when = not HolyLib_IsModuleEnabled("util"),
            func = function()
                expect( util.FancyJSONToTable ).to.beA( "nil" ) -- util table always exists since its created by gmod
            end
        },
        { -- This one is a former crash: https://github.com/RaphaelIT7/gmod-holylib/issues/101
            name = "Returns a proper result",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                local expectedTable = {
                    {
                        [1] = 1,
                        [2] = 2,
                        [3] = 3,
                    }
                }

                local actualTable = util.FancyJSONToTable( "[[1, 2, 3]]" )

                expect( actualTable ).to.deepEqual( expectedTable )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                HolyLib_RunPerformanceTest("util.FancyJSONToTable", util.FancyJSONToTable, "[[1, 2, 3, 4, 5, 6, 7, 8, 9, 10], [\"Hello World\", \"World2\", \"World3\"]]")
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                HolyLib_RunPerformanceTest("util.JSONToTable(GMOD)", util.JSONToTable, "[[1, 2, 3, 4, 5, 6, 7, 8, 9, 10], [\"Hello World\", \"World2\", \"World3\"]]")
            end
        },
    }
}
