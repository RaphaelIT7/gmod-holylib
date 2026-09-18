return {
    groupName = "HolyLib.ServerExecute",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.ServerExecute ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib ).to.beA( "nil" )
            end
        },
        {
            name = "Can be called without erroring",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                HolyLib.ServerExecute()
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                HolyLib_RunPerformanceTest("HolyLib.ServerExecute", function() HolyLib.ServerExecute() end)
            end
        },
    }
}
