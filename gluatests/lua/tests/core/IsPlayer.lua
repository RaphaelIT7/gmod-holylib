return {
    groupName = "HolyLib manages to properly detect Player entities in Lua",
    cases = {
        {
            name = "Is _HOLYLIB_CORE.IsPlayer function existent",
            func = function()
                expect( _HOLYLIB_CORE.IsPlayer ).to.beA( "function" )
            end
        },
        {
            name = "Returns false for a non-Player Entity",
            func = function()
                expect( _HOLYLIB_CORE.IsPlayer(game.GetWorld()) ).to.beFalse()
            end
        },
        {
            name = "Returns true for a Player Entity",
            when = player.GetCount() > 0,
            func = function()
                expect( _HOLYLIB_CORE.IsPlayer(player.GetAll()[1]) ).to.beTrue()
            end
        },
        {
            name = "Errors instead of crashing when given a NULL Entity",
            func = function()
                expect( _HOLYLIB_CORE.IsPlayer, NULL ).to.errWith( "bad argument #1 to '?' (Tried to use a NULL Entity!)" )
            end
        },
        {
            name = "Performance",
            when = player.GetCount() > 0,
            func = function()
                local ply = player.GetAll()[1]
                HolyLib_RunPerformanceTest("_HOLYLIB_CORE.IsPlayer", function() _HOLYLIB_CORE.IsPlayer(ply) end)
            end
        },
    }
}
