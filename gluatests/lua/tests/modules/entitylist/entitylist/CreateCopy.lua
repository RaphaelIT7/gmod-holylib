return {
    groupName = "EntityList:CreateCopy",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( FindMetaTable("EntityList").CreateCopy ).to.beA( "function" )
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
            name = "Returns a new EntityList object with the same entities",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                local entityList = CreateEntityList()
                entityList:SetEntities( ents.GetAll() )

                local copy = entityList:CreateCopy()
                expect( copy ).to.beA( "EntityList" )
                expect( copy ).toNot.equal( entityList )

                expect( #copy:GetEntities() ).to.equal( #entityList:GetEntities() )
            end
        },
        {
            name = "Modifying the copy doesn't affect the original list",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                local entityList = CreateEntityList()
                entityList:AddEntity( game.GetWorld() )

                local copy = entityList:CreateCopy()
                expect( #copy:GetEntities() ).to.equal( 1 )

                copy:RemoveEntity( game.GetWorld() )
                expect( #copy:GetEntities() ).to.equal( 0 )
                expect( #entityList:GetEntities() ).to.equal( 1 )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                local entityList = CreateEntityList()
                entityList:SetEntities( ents.GetAll() )

                HolyLib_RunPerformanceTest("EntityList:CreateCopy", function() entityList:CreateCopy() end)
            end
        },
    }
}
