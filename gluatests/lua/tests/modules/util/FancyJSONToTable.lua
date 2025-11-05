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
        { -- This the second reported crash from: https://github.com/RaphaelIT7/gmod-holylib/issues/101
            name = "Returns a proper result",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                local expectedTable = {
                    {
                        a = 1,
                        b = 2,
                    }
                }

                -- Input forces it to enter IsArray, which then recursively calls to IsObject internally as this focus on testing if the stack is properly taken account of
                -- Formerly, this was screwed up as it handled pushing values wrong pushing more than one value onto the stack resulting in a stack leak and other weird behavior.
                local actualTable = util.FancyJSONToTable( '[{"a":1,"b":2}]' )

                expect( actualTable ).to.deepEqual( expectedTable )
            end
        },
        { -- This the third reported issue from (it just never stops :sob:): https://github.com/RaphaelIT7/gmod-holylib/issues/101
            name = "Returns a proper result",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                local expectedTable = {
                	name = "[Important: HolyLibIsGoated]",
                    pos = Vector(4, 2, 0)
                }

                -- Input uses [ ] though does not focus on being a vector, instead its just a normal string using it
                -- This focuses on the internal validation ensuring no garbage vectors are returned (which they previously were)
                local actualTable = util.FancyJSONToTable( '{"pos": "[4 2 0]", "name": "[Important: HolyLibIsGoated]"}' )

                expect( actualTable ).to.deepEqual( expectedTable )
            end
        },
        {
            name = "Returns a proper result",
            when = HolyLib_IsModuleEnabled("util"),
            func = function()
                local expectedTable = {
                    [1] = "a",
                    [2] = "b",
                    ["0YHolyLibIsS0C00L"] = "c",
                }

                local actualTable = util.FancyJSONToTable( '{"1":"a","2":"b","0YHolyLibIsS0C00L":"c"}' )

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
