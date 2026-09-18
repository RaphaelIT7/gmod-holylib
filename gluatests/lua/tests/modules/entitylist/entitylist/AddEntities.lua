return {
    groupName = "EntityList:AddEntities",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( FindMetaTable("EntityList").AddEntities ).to.beA( "function" )
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

                local entities = entityList:GetEntities()
                expect( entities ).to.beA( "table" )

                expect( #entities ).to.equal( 0 )

                entityList:AddEntities( ents.GetAll() )

                expect( #entityList:GetEntities() ).to.equal( #ents.GetAll() )
            end
        },
        {
            name = "Doesn't add duplicate entries for entities already in the list",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                local entityList = CreateEntityList()

                entityList:AddEntity( game.GetWorld() )
                expect( #entityList:GetEntities() ).to.equal( 1 )

                entityList:AddEntities( { game.GetWorld() } )
                expect( #entityList:GetEntities() ).to.equal( 1 )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                local entityList = CreateEntityList()
                local entities = ents.GetAll()

                HolyLib_RunPerformanceTest("EntityList:AddEntities", function() entityList:AddEntities(entities) end)
            end
        },
    }
}
