return {
    groupName = "filesystem.Size",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("filesystem"),
            func = function()
                expect( filesystem.Size ).to.beA( "function" )
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
                expect( filesystem.Size("garrysmod.ver", "MOD") ).toNot.equal(0)
                expect( filesystem.Size("garrysmod.verrrr", "MOD") ).to.equal(0)
            end
        },
        {
            name = "Performance when file exists",
            when = HolyLib_IsModuleEnabled("filesystem"),
            func = function()
                HolyLib_RunPerformanceTest("filesystem.Size(Hit)", filesystem.Size, "garrysmod.ver", "GAME")
            end
        },
        {
            name = "Performance when file is missing",
            when = HolyLib_IsModuleEnabled("filesystem"),
            func = function()
                HolyLib_RunPerformanceTest("filesystem.Size(Miss)", filesystem.Size, "garrysmod.verrrr", "GAME")
            end
        },
    }
}
