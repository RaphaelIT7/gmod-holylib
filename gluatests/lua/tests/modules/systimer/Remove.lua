return {
    groupName = "systimer.Remove",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Remove ).to.beA( "function" )
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
                expect( systimer.Remove ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non string name",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Remove, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Doesn't error when the timer doesn't exist and returns nothing",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local ret = systimer.Remove( "SysTimer_Remove_Test" )
                expect( ret ).to.beNil()
            end
        },
        {
            name = "Makes Exists() return false for a removed timer",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Remove_Test"
                systimer.Create( name, 100, 1, function() end )
                expect( systimer.Exists( name ) ).to.beTrue()

                systimer.Remove( name )

                expect( systimer.Exists( name ) ).to.beFalse()
            end
        },
        {
            name = "Prevents a pending timer from ever firing",
            when = HolyLib_IsModuleEnabled("systimer"),
            async = true,
            timeout = 2,
            func = function()
                local name = "SysTimer_Remove_Test"
                local fired = false

                systimer.Create( name, 0.15, 1, function() fired = true end )
                systimer.Remove( name )

                timer.Simple( 0.4, function()
                    expect( fired ).to.beFalse()
                    expect( systimer.Exists( name ) ).to.beFalse()

                    done()
                end )
            end
        },
    }
}
