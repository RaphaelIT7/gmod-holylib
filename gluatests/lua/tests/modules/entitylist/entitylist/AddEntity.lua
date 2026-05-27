return {
    groupName = "EntityList:AddEntity",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( FindMetaTable("EntityList").AddEntity ).to.beA( "function" )
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

                local entities = ents.GetAll()
                for _, ent in ipairs(entities) do
                	entityList:AddEntity(ent)
                end

                expect( #entityList:GetEntities() ).toNot.equal( 0 )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
            	local entityList = CreateEntityList()
				local entities = ents.GetAll()
				local function AddEntity()
					for _, ent in ipairs(entities) do
						entityList:AddEntity(ent)
					end
				end
				
                HolyLib_RunPerformanceTest("EntityList:AddEntity", AddEntity)
            end
        },
    }
}
