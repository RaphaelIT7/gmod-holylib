return {
    groupName = "cvar.Find",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("cvars"),
            func = function()
                expect( cvar.Find ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("cvars"),
            func = function()
                expect( cvar ).to.beA( "nil" )
            end
        },
        {
            name = "Properly finds the convar",
            when = HolyLib_IsModuleEnabled("cvars"),
            func = function()
                local cheats = cvar.Find("sv_cheats")

                expect( cheats ).toNot.beNil()
                expect( cheats:GetName() ).to.equal( "sv_cheats" )
            end
        },
        {
            name = "Properly returns nil if the convar couldn't be found",
            when = HolyLib_IsModuleEnabled("cvars"),
            func = function()
                local cheats = cvar.Find("sv_cheats123")

                expect( cheats ).to.beNil()
            end
        },
        {
            name = "Performance when convar exists",
            when = HolyLib_IsModuleEnabled("cvars"),
            func = function()
                HolyLib_RunPerformanceTest("cvar.Find(Hit)", cvar.Find, "sv_cheats")
            end
        },
        {
            name = "Performance when convar doesn't exist",
            when = HolyLib_IsModuleEnabled("cvars"),
            func = function()
                HolyLib_RunPerformanceTest("cvar.Find(Miss)", cvar.Find, "sv_cheatsssss")
            end
        },
    }
}
