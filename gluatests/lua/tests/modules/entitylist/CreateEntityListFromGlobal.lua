return {
    groupName = "CreateEntityListFromGlobal",
    cases = {
        {
            name = "Function exists globally",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( CreateEntityListFromGlobal ).to.beA( "function" )
            end
        },
        {
            name = "Function doesn't exists globally",
            when = not HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( CreateEntityListFromGlobal ).to.beA( "nil" )
            end
        },
        {
            name = "Returns an EntityList object",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                local entityList = CreateEntityListFromGlobal()
                expect( entityList ).to.beA( "EntityList" )
            end
        },
        {
            name = "Actually copies the entities from the global entity list",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                local globalEntities = GetGlobalEntityList()
                local entityList = CreateEntityListFromGlobal()
                expect( #entityList:GetEntities() ).to.equal( #globalEntities )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                HolyLib_RunPerformanceTest("CreateEntityListFromGlobal", function() CreateEntityListFromGlobal() end)
            end
        },
    }
}
