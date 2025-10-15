return {
    groupName = "filesystem.IsDir",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("filesystem"),
            func = function()
                expect( filesystem.IsDir ).to.beA( "function" )
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
                expect( filesystem.IsDir("lua", "MOD") ).to.beTrue()
                expect( filesystem.IsDir("luaaaaa", "MOD") ).to.beFalse()
            end
        },
        {
            name = "Performance when folder exists",
            when = HolyLib_IsModuleEnabled("filesystem"),
            func = function()
                HolyLib_RunPerformanceTest("filesystem.IsDir(Hit)", filesystem.IsDir, "lua", "GAME")
            end
        },
        {
            name = "Performance when folder is missing",
            when = HolyLib_IsModuleEnabled("filesystem"),
            func = function()
                HolyLib_RunPerformanceTest("filesystem.IsDir(Miss)", filesystem.IsDir, "luarrrr", "GAME")
            end
        },
    }
}
