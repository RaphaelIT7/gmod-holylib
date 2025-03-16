return {
    groupName = "EntityList:__newindex",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( FindMetaTable("EntityList").__newindex ).to.beA( "function" )
            end
        },
        {
            name = "Metatable doesn't exist",
            when = not HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( FindMetaTable("EntityList") ).to.beA( "nil" )
            end
        },
        {
            name = "Sets the right value",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                local entityList = CreateEntityList()

                entityList.test = "Hello World"
                expect( entityList.test ).to.equal( "Hello World" )
                expect( entityList:GetTable().test ).to.equal( "Hello World" )
            end
        },
    }
}
