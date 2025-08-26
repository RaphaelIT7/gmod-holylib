return {
    groupName = "HolyLib manages to properly Get Entities from Lua",
    cases = {
        {
            name = "Is HolyLib.__GetEntity function existent",
            func = function()
                expect( HolyLib.__GetEntity ).to.beA( "function" )
            end
        },
        {
            name = "Pushes an Entity to Lua correctly",
            func = function()
                local success = HolyLib.__GetEntity(game.GetWorld()) -- Returns false or crashes if anything was wrong

                expect( success ).to.beTrue()
            end
        },
    }
}
