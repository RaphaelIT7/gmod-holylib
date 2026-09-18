return {
    groupName = "systimer.Stop",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Stop ).to.beA( "function" )
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
                expect( systimer.Stop ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non string name",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Stop, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Returns false for a timer that doesn't exist",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Stop( "SysTimer_Stop_Test" ) ).to.beFalse()
            end
        },
        {
            name = "Returns true when stopping an active timer, false when stopping it again",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Stop_Test"
                systimer.Create( name, 100, 1, function() end )

                expect( systimer.Stop( name ) ).to.beTrue()
                expect( systimer.Stop( name ) ).to.beFalse()

                systimer.Remove( name )
            end
        },
        {
            name = "Unlike Pause(), keeps TimeLeft() reporting a sane remaining time",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Stop_Test"
                systimer.Create( name, 50, 1, function() end )

                local activeTimeLeft = systimer.TimeLeft( name )
                systimer.Stop( name )
                local stoppedTimeLeft = systimer.TimeLeft( name )

                expect( math.abs( stoppedTimeLeft - activeTimeLeft ) < activeTimeLeft * 0.5 ).to.beTrue()

                systimer.Remove( name )
            end
        },
        {
            name = "Prevents the timer from firing while stopped",
            when = HolyLib_IsModuleEnabled("systimer"),
            async = true,
            timeout = 2,
            func = function()
                local name = "SysTimer_Stop_Test"
                local fired = false

                systimer.Create( name, 0.15, 1, function() fired = true end )
                systimer.Stop( name )

                timer.Simple( 0.4, function()
                    expect( fired ).to.beFalse()
                    expect( systimer.Exists( name ) ).to.beTrue()

                    systimer.Remove( name )
                    done()
                end )
            end
        },
    }
}
