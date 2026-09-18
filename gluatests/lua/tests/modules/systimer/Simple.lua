return {
    groupName = "systimer.Simple",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Simple ).to.beA( "function" )
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
                expect( systimer.Simple ).to.errWith( "bad argument #1 to '?' (number expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non number delay",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Simple, "soon" ).to.errWith( "bad argument #1 to '?' (number expected, got string)" )
            end
        },
        {
            name = "Errors when the callback is missing",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Simple, 1 ).to.errWith( "bad argument #2 to '?' (function expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non function callback",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Simple, 1, "notafunction" ).to.errWith( "bad argument #2 to '?' (function expected, got string)" )
            end
        },
        {
            name = "Returns nothing",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local ret = systimer.Simple( 100, function() end )
                expect( ret ).to.beNil()
            end
        },
        {
            name = "Fires the callback once after the delay elapses",
            when = HolyLib_IsModuleEnabled("systimer"),
            async = true,
            timeout = 2,
            func = function()
                systimer.Simple( 0.1, function()
                    done()
                end )
            end
        },
        {
            name = "Is anonymous - it can't be found, adjusted, paused or removed by name",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local fired = false
                systimer.Simple( 100, function() fired = true end )

                expect( systimer.Exists( "SysTimer_Simple_Test" ) ).to.beFalse()
            end
        },
    }
}
