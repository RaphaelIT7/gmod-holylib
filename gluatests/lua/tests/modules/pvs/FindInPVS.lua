return {
    groupName = "pvs.FindInPVS",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("pvs"),
            func = function()
                expect( pvs.FindInPVS ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("pvs"),
            func = function()
                expect( pvs ).to.beA( "nil" )
            end
        },
        {
            name = "Properly returns nil if the convar couldn't be found",
            when = HolyLib_IsModuleEnabled("pvs") and player.GetCount() > 0,
            func = function()
                local entities = pvs.FindInPVS(player.GetAll()[1]:GetPos())

                expect( entities ).to.beA("table")
                expect( entities ).toNot.equal(0)
            end
        },
        {
            name = "Performance when convar exists",
            when = HolyLib_IsModuleEnabled("pvs") and player.GetCount() > 0,
            func = function()
                HolyLib_RunPerformanceTest("pvs.FindInPVS", pvs.FindInPVS, player.GetAll()[1]:GetPos())
            end
        },
    }
}
