return {
    groupName = "CreateEntityList",
    cases = {
        --[[ Function Signature ]]--
        --#region
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
                local list = CreateEntityList()
                expect( list ).to.beA( "EntityList" )
            end
        },
    }
}
