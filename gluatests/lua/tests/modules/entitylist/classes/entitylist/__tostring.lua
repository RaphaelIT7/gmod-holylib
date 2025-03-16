return {
    groupName = "EntityList:__tostring",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( FindMetaTable("EntityList").__tostring ).to.beA( "function" )
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
                expect( entityList:__tostring() ).to.equal( "EntityList [0]" )
            end
        },
    }
}
