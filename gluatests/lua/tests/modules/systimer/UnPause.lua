return {
    groupName = "systimer.UnPause",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.UnPause ).to.beA( "function" )
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
                expect( systimer.UnPause ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non string name",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.UnPause, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Returns false for a timer that doesn't exist",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.UnPause( "SysTimer_UnPause_Test" ) ).to.beFalse()
            end
        },
        {
            name = "Returns true and reactivates a paused timer",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_UnPause_Test"
                systimer.Create( name, 100, 1, function() end )
                systimer.Pause( name )

                expect( systimer.UnPause( name ) ).to.beTrue()

                systimer.Remove( name )
            end
        },
        {
            name = "Returns false when un-pausing a timer that was never paused",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_UnPause_Test"
                systimer.Create( name, 100, 1, function() end )

                expect( systimer.UnPause( name ) ).to.beFalse()

                systimer.Remove( name )
            end
        },
        {
            name = "Resumes firing after being paused",
            when = HolyLib_IsModuleEnabled("systimer"),
            async = true,
            timeout = 2,
            func = function()
                local name = "SysTimer_UnPause_Test"
                local fired = false

                systimer.Create( name, 0.15, 1, function() fired = true end )
                systimer.Pause( name )

                timer.Simple( 0.3, function()
                    expect( fired ).to.beFalse()

                    systimer.UnPause( name )
                    timer.Simple( 0.3, function()
                        expect( fired ).to.beTrue()

                        done()
                    end )
                end )
            end
        },
        {
            name = "Preserves the remaining time when un-pausing, matching its own documented behavior",
            when = HolyLib_IsModuleEnabled("systimer"),
            async = true,
            timeout = 4,
            func = function()
                local name = "SysTimer_UnPause_Test"
                systimer.Create( name, 2, 1, function() end )

                timer.Simple( 1.5, function()
                    local beforePause = systimer.TimeLeft( name )
                    systimer.Pause( name )
                    local result = systimer.UnPause( name )
                    local afterUnpause = systimer.TimeLeft( name )

                    expect( result ).to.beTrue()
                    expect( math.abs( afterUnpause - beforePause ) < beforePause * 0.5 ).to.beTrue()

                    systimer.Remove( name )
                    done()
                end )
            end
        },
    }
}
