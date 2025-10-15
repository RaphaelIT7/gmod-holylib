return {
    groupName = "filesystem.Time",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("filesystem"),
            func = function()
                expect( filesystem.Time ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("filesystem"),
            func = function()
                expect( filesystem ).to.beA( "nil" )
            end
        },
        {
            name = "Properly works",
            when = HolyLib_IsModuleEnabled("filesystem"),
            func = function()
                expect( filesystem.Time("garrysmod.ver", "MOD") ).toNot.equal(0)
                expect( filesystem.Time("garrysmod.verrrr", "MOD") ).to.equal(0)
            end
        },
        {
            name = "Performance when file exists",
            when = HolyLib_IsModuleEnabled("filesystem"),
            func = function()
                HolyLib_RunPerformanceTest("filesystem.Time(Hit)", filesystem.Time, "garrysmod.ver", "GAME")
            end
        },
        {
            name = "Performance when file is missing",
            when = HolyLib_IsModuleEnabled("filesystem"),
            func = function()
                HolyLib_RunPerformanceTest("filesystem.Time(Miss)", filesystem.Time, "garrysmod.verrrr", "GAME")
            end
        },
    }
}
