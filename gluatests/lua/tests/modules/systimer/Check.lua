return {
    groupName = "systimer.Check",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Check ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer ).to.beA( "nil" )
            end
        },
        {
            name = "Does nothing and returns no values (deprecated no-op)",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local ret = systimer.Check()
                expect( ret ).to.beNil()
            end
        },
        {
            name = "Doesn't validate or error regardless of the arguments passed",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local ret = systimer.Check( 1, "two", true, nil, {} )
                expect( ret ).to.beNil()
            end
        },
    }
}
