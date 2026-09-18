return {
    groupName = "systimer.Adjust",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Adjust ).to.beA( "function" )
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
                expect( systimer.Adjust ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non string name",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Adjust, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Errors when the delay is missing",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Adjust, "SysTimer_Adjust_Test" ).to.errWith( "bad argument #2 to '?' (number expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non number delay",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Adjust, "SysTimer_Adjust_Test", "soon" ).to.errWith( "bad argument #2 to '?' (number expected, got string)" )
            end
        },
        {
            name = "Returns false when the timer doesn't exist",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Adjust( "SysTimer_Adjust_Test", 5 ) ).to.beFalse()
            end
        },
        {
            name = "Adjusts the delay of an existing timer",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Adjust_Test"
                systimer.Create( name, 100, 1, function() end )

                local before = systimer.TimeLeft( name )
                local ok = systimer.Adjust( name, 5 )
                local after = systimer.TimeLeft( name )

                expect( ok ).to.beTrue()
                expect( after < before ).to.beTrue()

                systimer.Remove( name )
            end
        },
        {
            name = "Adjusts the repetitions of an existing timer",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Adjust_Test"
                systimer.Create( name, 100, 1, function() end )

                systimer.Adjust( name, 100, 7 )
                expect( systimer.RepsLeft( name ) ).to.equal( 7 )

                systimer.Remove( name )
            end
        },
        {
            name = "Ignores a non number repetitions argument and a non function callback argument",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Adjust_Test"
                systimer.Create( name, 100, 5, function() end )

                local ok = systimer.Adjust( name, 50, "not a number", "not a function" )

                expect( ok ).to.beTrue()
                expect( systimer.RepsLeft( name ) ).to.equal( 5 )

                systimer.Remove( name )
            end
        },
        {
            name = "Replaces the callback of an existing timer",
            when = HolyLib_IsModuleEnabled("systimer"),
            async = true,
            timeout = 2,
            func = function()
                local name = "SysTimer_Adjust_Test"
                local oldFired, newFired = false, false

                systimer.Create( name, 0.5, 1, function() oldFired = true end )
                local ok = systimer.Adjust( name, 0.05, 1, function() newFired = true end )
                expect( ok ).to.beTrue()

                timer.Simple( 0.3, function()
                    expect( newFired ).to.beTrue()
                    expect( oldFired ).to.beFalse()
                    expect( systimer.Exists( name ) ).to.beFalse()

                    done()
                end )
            end
        },
    }
}
