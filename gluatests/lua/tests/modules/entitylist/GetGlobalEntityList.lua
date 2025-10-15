return {
    groupName = "GetGlobalEntityList",
    cases = {
        {
            name = "Function exists globally",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( GetGlobalEntityList ).to.beA( "function" )
            end
        },
        {
            name = "Function doesn't exists globally",
            when = not HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                expect( GetGlobalEntityList ).to.beA( "nil" )
            end
        },
        {
            name = "Returns an Table object",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                local entities = GetGlobalEntityList()
                expect( entities ).to.beA( "table" )

                for _, ent in ipairs(entities) do -- Ensure nothing Invalid was pushed
                	expect( ent:IsValid() or ent == game.GetWorld() ).to.beTrue()
                end
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("entitylist"),
            func = function()
                HolyLib_RunPerformanceTest("GetGlobalEntityList", GetGlobalEntityList)
            end
        },
    }
}
