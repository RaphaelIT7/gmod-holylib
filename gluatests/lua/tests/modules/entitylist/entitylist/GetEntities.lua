return {
    groupName = "EntityList:GetEntities",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( FindMetaTable("EntityList").GetEntities ).to.beA( "function" )
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

                entityList:AddEntity(game.GetWorld())

                expect( #entityList:GetEntities() ).to.equal( 1 )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
            	local entityList = CreateEntityList()

            	entityList:SetEntities(ents.GetAll())

                HolyLib_RunPerformanceTest("EntityList:GetEntities", entityList.GetEntities, entityList)
            end
        },
    }
}
