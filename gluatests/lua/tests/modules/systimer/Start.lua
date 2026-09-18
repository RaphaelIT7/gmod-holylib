return {
    groupName = "systimer.Start",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Start ).to.beA( "function" )
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
                expect( systimer.Start ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non string name",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Start, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Returns false for a timer that doesn't exist",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Start( "SysTimer_Start_Test" ) ).to.beFalse()
            end
        },
        {
            name = "Returns false when starting a timer that is already active",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Start_Test"
                systimer.Create( name, 100, 1, function() end )

                expect( systimer.Start( name ) ).to.beFalse()

                systimer.Remove( name )
            end
        },
        {
            name = "Returns true and preserves the remaining time when starting a Stop()'d timer",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Start_Test"
                systimer.Create( name, 10, 1, function() end )

                systimer.Stop( name )
                local beforeStart = systimer.TimeLeft( name )
                local started = systimer.Start( name )
                local afterStart = systimer.TimeLeft( name )

                expect( started ).to.beTrue()
                expect( math.abs( afterStart - beforeStart ) < beforeStart * 0.5 ).to.beTrue()

                systimer.Remove( name )
            end
        },
        {
            name = "Returns true and preserves the remaining time when starting a Pause()'d timer",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Start_Test"
                systimer.Create( name, 10, 1, function() end )

                systimer.Pause( name )
                local beforeStart = systimer.TimeLeft( name )
                local started = systimer.Start( name )
                local afterStart = systimer.TimeLeft( name )

                expect( started ).to.beTrue()
                expect( math.abs( afterStart - beforeStart ) < beforeStart * 0.5 ).to.beTrue()

                systimer.Remove( name )
            end
        },
        {
            name = "Prevents firing until Start() is called again after Stop()",
            when = HolyLib_IsModuleEnabled("systimer"),
            async = true,
            timeout = 2,
            func = function()
                local name = "SysTimer_Start_Test"
                local fired = false

                systimer.Create( name, 0.15, 1, function() fired = true end )
                systimer.Stop( name )

                timer.Simple( 0.3, function()
                    expect( fired ).to.beFalse()

                    systimer.Start( name )
                    timer.Simple( 0.3, function()
                        expect( fired ).to.beTrue()

                        done()
                    end )
                end )
            end
        },
    }
}
