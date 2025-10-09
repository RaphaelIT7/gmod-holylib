return {
    groupName = "HolyLib manages to properly Push Entities to Lua",
    cases = {
        {
            name = "Is _HOLYLIB_CORE.PushEntity function existent",
            func = function()
                expect( _HOLYLIB_CORE.PushEntity ).to.beA( "function" )
            end
        },
        {
            name = "Pushes an Entity to Lua correctly",
            func = function()
                local ent = _HOLYLIB_CORE.PushEntity()
                expect( ent ).to.beA( "Entity" )
                expect( ent ).to.equal( game.GetWorld() )
                expect( ent:IsWorld() ).to.beTrue()
            end
        },
        {
            name = "Performance",
            func = function()
                HolyLib_RunPerformanceTest("_HOLYLIB_CORE.PushEntity", _HOLYLIB_CORE.PushEntity)
            end
        },
    }
}
