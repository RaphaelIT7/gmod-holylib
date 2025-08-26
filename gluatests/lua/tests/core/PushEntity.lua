return {
    groupName = "HolyLib manages to properly Push Entities to Lua",
    cases = {
        {
            name = "Is HolyLib.__PushEntity function existent",
            func = function()
                expect( HolyLib.__PushEntity ).to.beA( "function" )
            end
        },
        {
            name = "Pushes an Entity to Lua correctly",
            func = function()
                local ent = HolyLib.__PushEntity()
                expect( ent ).to.beA( "Entity" )
                expect( ent ).to.equal( game.GetWorld() )
                expect( ent:IsWorld() ).to.beTrue()
            end
        },
    }
}
