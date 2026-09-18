return {
    groupName = "systimer.TimeLeft",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.TimeLeft ).to.beA( "function" )
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
                expect( systimer.TimeLeft ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non string name",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.TimeLeft, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Returns 0 for a timer that doesn't exist",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.TimeLeft( "SysTimer_TimeLeft_Test" ) ).to.equal( 0 )
            end
        },
        {
            name = "Returns a sane number of seconds remaining for an active timer",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_TimeLeft_Test"
                systimer.Create( name, 1, 1, function() end )

                local timeLeft = systimer.TimeLeft( name )
                expect( timeLeft > 0 and timeLeft <= 1 ).to.beTrue()

                systimer.Remove( name )
            end
        },
        {
            name = "Decreases the longer the timer has been running",
            when = HolyLib_IsModuleEnabled("systimer"),
            async = true,
            timeout = 2,
            func = function()
                local name = "SysTimer_TimeLeft_Test"
                systimer.Create( name, 100, 1, function() end )

                local first = systimer.TimeLeft( name )
                timer.Simple( 0.2, function()
                    local second = systimer.TimeLeft( name )
                    expect( second < first ).to.beTrue()

                    systimer.Remove( name )
                    done()
                end )
            end
        },
        {
            name = "Pause() keeps TimeLeft() reporting a sane remaining time, like Stop()",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_TimeLeft_Test"
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
