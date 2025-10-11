return {
    groupName = "HolyLib manages to properly Get Entities from Lua",
    cases = {
        {
            name = "Is _HOLYLIB_CORE.GetEntity function existent",
            func = function()
                expect( _HOLYLIB_CORE.GetEntity ).to.beA( "function" )
            end
        },
        {
            name = "Pushes an Entity to Lua correctly",
            func = function()
                local success = _HOLYLIB_CORE.GetEntity(game.GetWorld()) -- Returns false or crashes if anything was wrong

                expect( success ).to.beTrue()
            end
        },
        {
            name = "Performance",
            func = function()
                HolyLib_RunPerformanceTest("_HOLYLIB_CORE.RawGetEntity", _HOLYLIB_CORE.RawGetEntity, game.GetWorld())
            end
        },
    }
}
