return {
    groupName = "systimer.Destroy",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Destroy ).to.beA( "function" )
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
            name = "Errors when called with no arguments",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Destroy ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non string name",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Destroy, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Doesn't error when the timer doesn't exist and returns nothing",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local ret = systimer.Destroy( "SysTimer_Destroy_Test" )
                expect( ret ).to.beNil()
            end
        },
        {
            name = "Removes an existing timer, same as systimer.Remove (Destroy is just an alias)",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Destroy_Test"
                systimer.Create( name, 100, 1, function() end )
                expect( systimer.Exists( name ) ).to.beTrue()

                systimer.Destroy( name )

                expect( systimer.Exists( name ) ).to.beFalse()
            end
        },
    }
}
