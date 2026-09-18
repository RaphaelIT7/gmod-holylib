return {
    groupName = "autorefresh.RemoveFileFromRefresh",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                expect( autorefresh.RemoveFileFromRefresh ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                expect( autorefresh ).to.beA( "nil" )
            end
        },
        {
            name = "Requires a string argument",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                expect( autorefresh.RemoveFileFromRefresh ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non-string argument",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                expect( autorefresh.RemoveFileFromRefresh, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Returns false for a file that was never added",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                expect( autorefresh.RemoveFileFromRefresh( "AutoRefresh_RemoveFileFromRefresh_Test" ) ).to.beFalse()
            end
        },
        {
            name = "Returns true when removing a previously added file, false the second time",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                local fileName = "AutoRefresh_RemoveFileFromRefresh_Test"

                autorefresh.AddFileToRefresh( fileName )

                expect( autorefresh.RemoveFileFromRefresh( fileName ) ).to.beTrue()
                expect( autorefresh.RemoveFileFromRefresh( fileName ) ).to.beFalse()
            end
        },
        {
            name = "Performance calling with a file that isn't tracked",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                local fileName = "AutoRefresh_RemoveFileFromRefresh_Test"
                HolyLib_RunPerformanceTest("autorefresh.RemoveFileFromRefresh", function() autorefresh.RemoveFileFromRefresh( fileName ) end)
            end
        },
    }
}
