return {
    groupName = "systimer.RepsLeft",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.RepsLeft ).to.beA( "function" )
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
                expect( systimer.RepsLeft ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non string name",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.RepsLeft, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Returns 0 for a timer that doesn't exist",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                expect( systimer.RepsLeft( "SysTimer_RepsLeft_Test" ) ).to.equal( 0 )
            end
        },
        {
            name = "Returns the repetitions the timer was created with",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_RepsLeft_Test"
                systimer.Create( name, 100, 5, function() end )

                expect( systimer.RepsLeft( name ) ).to.equal( 5 )

                systimer.Remove( name )
            end
        },
        {
            name = "Returns 0 for a timer created with 0 repetitions, even though it will repeat forever",
            when = HolyLib_IsModuleEnabled("systimer"),
            func = function()
                local name = "SysTimer_RepsLeft_Test"
                systimer.Create( name, 100, 0, function() end )

                expect( systimer.RepsLeft( name ) ).to.equal( 0 )

                systimer.Remove( name )
            end
        },
        {
            name = "Counts down after each firing and hits 0 once the timer is removed",
            when = HolyLib_IsModuleEnabled("systimer"),
            async = true,
            timeout = 2,
            func = function()
                local name = "SysTimer_RepsLeft_Test"
                local fireCount = 0

                systimer.Create( name, 0.05, 2, function()
                    fireCount = fireCount + 1

                    if fireCount == 1 then
                        expect( systimer.RepsLeft( name ) ).to.equal( 1 )
                    elseif fireCount == 2 then
                        timer.Simple( 0, function()
                            expect( systimer.Exists( name ) ).to.beFalse()
                            expect( systimer.RepsLeft( name ) ).to.equal( 0 )
                            done()
                        end )
                    end
                end )

                expect( systimer.RepsLeft( name ) ).to.equal( 2 )
            end
        },
    }
}
