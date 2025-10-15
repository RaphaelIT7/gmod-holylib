return {
    groupName = "EntityList:SetEntities",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( FindMetaTable("EntityList").SetEntities ).to.beA( "function" )
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

                entityList:SetEntities(ents.GetAll())

                expect( #entityList:GetEntities() ).toNot.equal( 0 )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
            	local entityList = CreateEntityList()

                HolyLib_RunPerformanceTest("EntityList:GetEntities", entityList.SetEntities, entityList, ents.GetAll())
            end
        },
    }
}
