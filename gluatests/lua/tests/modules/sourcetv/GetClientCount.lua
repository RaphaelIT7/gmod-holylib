return {
    groupName = "sourcetv.GetClientCount",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv.GetClientCount ).to.beA( "function" )
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
            name = "Returns 0 when no SourceTV server exists",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv.GetClientCount() ).to.equal( 0 )
            end
        },
    }
}
