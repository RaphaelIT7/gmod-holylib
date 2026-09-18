return {
    groupName = "pas.CheckBoxInPAS",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("pas"),
            func = function()
                expect( pas.CheckBoxInPAS ).to.beA( "function" )
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
            name = "Properly checks a box around the given origin",
            when = HolyLib_IsModuleEnabled("pas") and player.GetCount() > 0,
            func = function()
                local pos = player.GetAll()[1]:GetPos()
                local mins = pos - Vector(16, 16, 16)
                local maxs = pos + Vector(16, 16, 16)

                expect( pas.CheckBoxInPAS(mins, maxs, pos) ).to.beTrue()
            end
        },
        {
            name = "Properly checks a box around the given origin entity",
            when = HolyLib_IsModuleEnabled("pas") and player.GetCount() > 0,
            func = function()
                local ply = player.GetAll()[1]
                local pos = ply:GetPos()
                local mins = pos - Vector(16, 16, 16)
                local maxs = pos + Vector(16, 16, 16)

                expect( pas.CheckBoxInPAS(mins, maxs, ply) ).to.beTrue()
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("pas") and player.GetCount() > 0,
            func = function()
                local pos = player.GetAll()[1]:GetPos()
                local mins = pos - Vector(16, 16, 16)
                local maxs = pos + Vector(16, 16, 16)
                HolyLib_RunPerformanceTest("pas.CheckBoxInPAS", function() pas.CheckBoxInPAS(mins, maxs, pos) end)
            end
        },
    }
}
