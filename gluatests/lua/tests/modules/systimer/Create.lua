return {
    groupName = "systimer.Create",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Create ).to.beA( "function" )
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
                expect( systimer.Create ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non string name",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Create, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Errors when the delay is missing",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Create, "SysTimer_Create_Test" ).to.errWith( "bad argument #2 to '?' (number expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non number delay",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Create, "SysTimer_Create_Test", "soon" ).to.errWith( "bad argument #2 to '?' (number expected, got string)" )
            end
        },
        {
            name = "Errors when the repetitions are missing",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Create, "SysTimer_Create_Test", 1 ).to.errWith( "bad argument #3 to '?' (number expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non number repetitions",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Create, "SysTimer_Create_Test", 1, "one" ).to.errWith( "bad argument #3 to '?' (number expected, got string)" )
            end
        },
        {
            name = "Errors when the callback is missing",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Create, "SysTimer_Create_Test", 1, 1 ).to.errWith( "bad argument #4 to '?' (function expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non function callback",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Create, "SysTimer_Create_Test", 1, 1, "notafunction" ).to.errWith( "bad argument #4 to '?' (function expected, got string)" )
            end
        },
        {
            name = "Returns nothing and registers the timer so that Exists() finds it",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Create_Test"
                local ret = systimer.Create( name, 100, 1, function() end )

                expect( ret ).to.beNil()
                expect( systimer.Exists( name ) ).to.beTrue()

                systimer.Remove( name )
            end
        },
        {
            name = "Fires the callback once after the delay elapses",
            when = HolyLib_IsModuleEnabled("systimer"),
            async = true,
            timeout = 2,
            func = function()
                local name = "SysTimer_Create_Test"

                systimer.Create( name, 0.1, 1, function()
                    done()
                end )
            end
        },
        {
            name = "Re-creating a timer with the same name reuses it and replaces the callback",
            when = HolyLib_IsModuleEnabled("systimer"),
            async = true,
            timeout = 2,
            func = function()
                local name = "SysTimer_Create_Test"
                local firstFired, secondFired = false, false

                systimer.Create( name, 5, 1, function() firstFired = true end )
                systimer.Create( name, 0.1, 1, function() secondFired = true end )

                timer.Simple( 0.3, function()
                    expect( secondFired ).to.beTrue()
                    expect( firstFired ).to.beFalse()

                    done()
                end )
            end
        },
        {
            name = "A repetitions value of 0 keeps firing (wraps to a huge unsigned value instead of stopping)",
            when = HolyLib_IsModuleEnabled("systimer"),
            async = true,
            timeout = 2,
            func = function()
                local name = "SysTimer_Create_Test"
                local fireCount = 0

                systimer.Create( name, 0.05, 0, function()
                    fireCount = fireCount + 1
                    if fireCount >= 3 then
                        systimer.Remove( name )
                        done()
                    end
                end )
            end
        },
        {
            name = "Recreating a one-shot timer from within its own callback fires it again",
            when = HolyLib_IsModuleEnabled("systimer"),
            async = true,
            timeout = 2,
            func = function()
                local name = "SysTimer_Create_Test"
                local fireCount = 0
                local callback

                callback = function()
                    fireCount = fireCount + 1
                    if fireCount == 1 then
                        systimer.Create( name, 0.1, 1, callback )
                    end
                end

                systimer.Create( name, 0.1, 1, callback )

                timer.Simple( 0.5, function()
                    expect( fireCount ).to.equal( 2 )
                    expect( systimer.Exists( name ) ).to.beFalse()

                    done()
                end )
            end
        },
    }
}
