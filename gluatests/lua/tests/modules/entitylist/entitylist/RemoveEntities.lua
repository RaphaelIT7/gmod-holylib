return {
    groupName = "EntityList:RemoveEntities",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( FindMetaTable("EntityList").RemoveEntities ).to.beA( "function" )
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
            name = "Returns proper results",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                local entityList = CreateEntityList()

                local entities = ents.GetAll()
                entityList:AddEntities( entities )
                expect( #entityList:GetEntities() ).to.equal( #entities )

                entityList:RemoveEntities( entities )
                expect( #entityList:GetEntities() ).to.equal( 0 )
            end
        },
        {
            name = "Doesn't error when removing entities that aren't in the list",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                local entityList = CreateEntityList()

                entityList:RemoveEntities( ents.GetAll() )
                expect( #entityList:GetEntities() ).to.equal( 0 )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                local entityList = CreateEntityList()
                local entities = ents.GetAll()
                entityList:AddEntities( entities )

                HolyLib_RunPerformanceTest("EntityList:RemoveEntities", function() entityList:RemoveEntities(entities) end)
            end
        },
    }
}
