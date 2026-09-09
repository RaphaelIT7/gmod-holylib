return {
    groupName = "pvs.TestPVS",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("pvs"),
            func = function()
                expect( pvs.TestPVS ).to.beA( "function" )
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
            name = "Properly compares two Vectors",
            when = HolyLib_IsModuleEnabled("pvs") and player.GetCount() > 0,
            func = function()
                local pos = player.GetAll()[1]:GetPos()

                expect( pvs.TestPVS(pos, pos) ).to.beTrue()
            end
        },
        {
            name = "Errors instead of crashing when given an invalid Entity",
            when = HolyLib_IsModuleEnabled("pvs") and player.GetCount() > 0,
            func = function()
                local pos = player.GetAll()[1]:GetPos()

                expect( pvs.TestPVS, pos, NULL ).to.errWith( "bad argument #2 to '?' (Tried to use a NULL Entity!)" )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("pvs") and player.GetCount() > 0,
            func = function()
                local pos = player.GetAll()[1]:GetPos()
                HolyLib_RunPerformanceTest("pvs.TestPVS", function() pvs.TestPVS(pos, pos) end)
            end
        },
    }
}
