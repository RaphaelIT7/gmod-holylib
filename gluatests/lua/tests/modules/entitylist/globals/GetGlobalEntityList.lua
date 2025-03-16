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
            end
        },
    }
}
