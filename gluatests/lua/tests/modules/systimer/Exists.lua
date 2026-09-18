return {
    groupName = "systimer.Exists",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Exists ).to.beA( "function" )
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
                expect( systimer.Exists ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non string name",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Exists, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Returns false for a timer that was never created",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Exists( "SysTimer_Exists_Test" ) ).to.beFalse()
            end
        },
        {
            name = "Returns true right after creating a timer",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Exists_Test"
                systimer.Create( name, 100, 1, function() end )

                expect( systimer.Exists( name ) ).to.beTrue()

                systimer.Remove( name )
            end
        },
        {
            name = "Returns false after the timer is removed",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Exists_Test"
                systimer.Create( name, 100, 1, function() end )
                systimer.Remove( name )

                expect( systimer.Exists( name ) ).to.beFalse()
            end
        },
        {
            name = "Returns false after a single-repetition timer fired and was auto removed",
            when = HolyLib_IsModuleEnabled("systimer"),
            async = true,
            timeout = 2,
            func = function()
                local name = "SysTimer_Exists_Test"

                systimer.Create( name, 0.1, 1, function() end )

                timer.Simple( 0.3, function()
                    expect( systimer.Exists( name ) ).to.beFalse()

                    done()
                end )
            end
        },
    }
}
