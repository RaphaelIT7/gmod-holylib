return {
    groupName = "systimer.Toggle",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Toggle ).to.beA( "function" )
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
                expect( systimer.Toggle ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non string name",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Toggle, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Returns false for a timer that doesn't exist",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.Toggle( "SysTimer_Toggle_Test" ) ).to.beFalse()
            end
        },
        {
            name = "Toggling an active timer deactivates it and returns the new (inactive) state",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Toggle_Test"
                systimer.Create( name, 100, 1, function() end )

                expect( systimer.Toggle( name ) ).to.beFalse()
                expect( systimer.Exists( name ) ).to.beTrue()

                systimer.Remove( name )
            end
        },
        {
            name = "Toggling an inactive timer reactivates it and returns true",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Toggle_Test"
                systimer.Create( name, 100, 1, function() end )

                systimer.Toggle( name )
                expect( systimer.Toggle( name ) ).to.beTrue()

                systimer.Remove( name )
            end
        },
        {
            name = "Preserves the remaining time across an off/on toggle, like Stop()/Start()",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_Toggle_Test"
                systimer.Create( name, 10, 1, function() end )

                local before = systimer.TimeLeft( name )
                systimer.Toggle( name )
                systimer.Toggle( name )
                local after = systimer.TimeLeft( name )

                expect( math.abs( after - before ) < before * 0.5 ).to.beTrue()

                systimer.Remove( name )
            end
        },
        {
            name = "Prevents firing while toggled off, resumes firing once toggled back on",
            when = HolyLib_IsModuleEnabled("systimer"),
            async = true,
            timeout = 2,
            func = function()
                local name = "SysTimer_Toggle_Test"
                local fired = false

                systimer.Create( name, 0.15, 1, function() fired = true end )
                systimer.Toggle( name )

                timer.Simple( 0.3, function()
                    expect( fired ).to.beFalse()

                    systimer.Toggle( name )
                    timer.Simple( 0.3, function()
                        expect( fired ).to.beTrue()

                        done()
                    end )
                end )
            end
        },
    }
}
