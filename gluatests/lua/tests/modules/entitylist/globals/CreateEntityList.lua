return {
    groupName = "CreateEntityList",
    cases = {
        {
            name = "Function exists globally",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( CreateEntityList ).to.beA( "function" )
            end
        },
        {
            name = "Function doesn't exists globally",
            when = not HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( CreateEntityList ).to.beA( "nil" )
            end
        },
        {
            name = "Returns an Table object",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                local entityList = CreateEntityList()
                expect( entityList ).to.beA( "EntityList" )
            end
        },
    }
}
