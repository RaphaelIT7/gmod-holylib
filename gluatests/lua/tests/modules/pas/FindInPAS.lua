return {
    groupName = "pas.FindInPAS",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("pas"),
            func = function()
                expect( pas.FindInPAS ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("pas"),
            func = function()
                expect( pas ).to.beA( "nil" )
            end
        },
        {
            name = "Properly returns nil if the convar couldn't be found",
            when = HolyLib_IsModuleEnabled("pas") and player.GetCount() > 0,
            func = function()
                local entities = pas.FindInPAS(player.GetAll()[1]:GetPos())

                expect( entities ).to.beA("table")
                expect( entities ).toNot.equal(0)
            end
        },
        {
            name = "Performance when convar exists",
            when = HolyLib_IsModuleEnabled("pas") and player.GetCount() > 0,
            func = function()
                HolyLib_RunPerformanceTest("pas.FindInPAS", pas.FindInPAS, player.GetAll()[1]:GetPos())
            end
        },
    }
}
