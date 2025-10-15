return {
    groupName = "filesystem.Exists",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("filesystem"),
            func = function()
                expect( filesystem.Exists ).to.beA( "function" )
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
                expect( filesystem.Exists("garrysmod.ver", "MOD") ).to.beTrue()
                expect( filesystem.Exists("garrysmod.verrrr", "MOD") ).to.beFalse()
            end
        },
        {
            name = "Performance when file exists",
            when = HolyLib_IsModuleEnabled("filesystem"),
            func = function()
                HolyLib_RunPerformanceTest("filesystem.Exists(Hit)", filesystem.Exists, "garrysmod.ver", "GAME")
            end
        },
        {
            name = "Performance when file is missing",
            when = HolyLib_IsModuleEnabled("filesystem"),
            func = function()
                HolyLib_RunPerformanceTest("filesystem.Exists(Miss)", filesystem.Exists, "garrysmod.verrrrr", "GAME")
            end
        },
    }
}
