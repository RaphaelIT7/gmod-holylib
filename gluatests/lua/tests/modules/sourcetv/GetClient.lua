return {
    groupName = "sourcetv.GetClient",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv.GetClient ).to.beA( "function" )
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
            name = "Returns nil when no SourceTV server exists",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv.GetClient( 0 ) ).to.beNil()
            end
        },
        {
            name = "Returns nil without erroring when called without any arguments",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                local result = sourcetv.GetClient()

                expect( result ).to.beNil()
            end
        },
    }
}
