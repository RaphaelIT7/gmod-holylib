return {
    groupName = "EntityList:GetTable",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( FindMetaTable("EntityList").GetTable ).to.beA( "function" )
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
                expect( entityList:GetTable().test ).to.equal( "Hello World" )

                entityList:GetTable().test = "Hello World 2"
                expect( entityList.test ).to.equal( "Hello World 2" )
            end
        },
    }
}
