return {
    groupName = "EntityList:__index",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( FindMetaTable("EntityList").__index ).to.beA( "function" )
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
            name = "Returns the right value",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                local entityList = CreateEntityList()

                entityList.test = "Hello World"
                expect( entityList.test ).to.equal( "Hello World" )
            end
        },
    }
}
