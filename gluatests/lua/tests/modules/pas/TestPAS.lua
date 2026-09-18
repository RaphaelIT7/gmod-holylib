return {
    groupName = "pas.TestPAS",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("pas"),
            func = function()
                expect( pas.TestPAS ).to.beA( "function" )
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
            name = "Properly compares two Vectors",
            when = HolyLib_IsModuleEnabled("pas") and player.GetCount() > 0,
            func = function()
                local pos = player.GetAll()[1]:GetPos()

                expect( pas.TestPAS(pos, pos) ).to.beTrue()
            end
        },
        {
            name = "Properly compares an Entity's position",
            when = HolyLib_IsModuleEnabled("pas") and player.GetCount() > 0,
            func = function()
                local ply = player.GetAll()[1]

                expect( pas.TestPAS(ply, ply) ).to.beTrue()
            end
        },
        {
            name = "Errors instead of crashing when given an invalid Entity as the second argument",
            when = HolyLib_IsModuleEnabled("pas") and player.GetCount() > 0,
            func = function()
                local pos = player.GetAll()[1]:GetPos()

                expect( pas.TestPAS, pos, NULL ).to.errWith( "bad argument #2 to '?' (Tried to use a NULL Entity!)" )
            end
        },
        {
            name = "Errors instead of crashing when given an invalid Entity as the first argument",
            when = HolyLib_IsModuleEnabled("pas") and player.GetCount() > 0,
            func = function()
                local pos = player.GetAll()[1]:GetPos()

                expect( pas.TestPAS, NULL, pos ).to.errWith( "bad argument #1 to '?' (Tried to use a NULL Entity!)" )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("pas") and player.GetCount() > 0,
            func = function()
                local pos = player.GetAll()[1]:GetPos()
                HolyLib_RunPerformanceTest("pas.TestPAS", function() pas.TestPAS(pos, pos) end)
            end
        },
    }
}
