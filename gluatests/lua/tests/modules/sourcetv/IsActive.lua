return {
    groupName = "sourcetv.IsActive",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv.IsActive ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv ).to.beA( "nil" )
            end
        },
        {
            name = "Returns false when no SourceTV server exists",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv.IsActive() ).to.beFalse()
            end
        },
    }
}
