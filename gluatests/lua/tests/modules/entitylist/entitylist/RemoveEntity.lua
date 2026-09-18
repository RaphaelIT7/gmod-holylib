return {
    groupName = "EntityList:RemoveEntity",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( FindMetaTable("EntityList").RemoveEntity ).to.beA( "function" )
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

                entityList:AddEntity( game.GetWorld() )
                expect( #entityList:GetEntities() ).to.equal( 1 )

                entityList:RemoveEntity( game.GetWorld() )
                expect( #entityList:GetEntities() ).to.equal( 0 )
            end
        },
        {
            name = "Doesn't error when removing an entity that isn't in the list",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                local entityList = CreateEntityList()

                entityList:RemoveEntity( game.GetWorld() )
                expect( #entityList:GetEntities() ).to.equal( 0 )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                local entityList = CreateEntityList()
                local entities = ents.GetAll()
                local function RemoveEntity()
                    entityList:AddEntities(entities)
                    for _, ent in ipairs(entities) do
                        entityList:RemoveEntity(ent)
                    end
                end

                HolyLib_RunPerformanceTest("EntityList:RemoveEntity", RemoveEntity)
            end
        },
    }
}
