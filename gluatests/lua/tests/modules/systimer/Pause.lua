return {
    groupName = "systimer.Pause",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Pause ).to.beA( "function" )
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
                expect( systimer.Pause ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non string name",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Pause, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Returns false for a timer that doesn't exist",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Pause( "SysTimer_Pause_Test" ) ).to.beFalse()
            end
        },
        {
            name = "Returns true when pausing an active timer, false when pausing it again",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Pause_Test"
                systimer.Create( name, 100, 1, function() end )

                expect( systimer.Pause( name ) ).to.beTrue()
                expect( systimer.Pause( name ) ).to.beFalse()

                systimer.Remove( name )
            end
        },
        {
            name = "Prevents the timer from firing while paused",
            when = HolyLib_IsModuleEnabled("systimer"),
            async = true,
            timeout = 2,
            func = function()
                local name = "SysTimer_Pause_Test"
                local fired = false

                systimer.Create( name, 0.15, 1, function() fired = true end )
                systimer.Pause( name )

                timer.Simple( 0.4, function()
                    expect( fired ).to.beFalse()
                    expect( systimer.Exists( name ) ).to.beTrue()

                    systimer.Remove( name )
                    done()
                end )
            end
        },
        {
            name = "Leaves TimeLeft() reporting the remaining time, not a raw clock reading",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Pause_Test"
                systimer.Create( name, 50, 1, function() end )

                local activeTimeLeft = systimer.TimeLeft( name )
                systimer.Pause( name )
                local pausedTimeLeft = systimer.TimeLeft( name )

                expect( math.abs( pausedTimeLeft - activeTimeLeft ) < activeTimeLeft * 0.5 ).to.beTrue()

                systimer.Remove( name )
            end
        },
    }
}
