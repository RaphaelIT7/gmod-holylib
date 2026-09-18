return {
    groupName = "sourcetv.FireEvent",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv.FireEvent ).to.beA( "function" )
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
            name = "Returns nothing and does not error when called without any arguments",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                local result = sourcetv.FireEvent()

                expect( result ).to.beNil()
            end
        },
    }
}
