return {
    groupName = "autorefresh.AddFileToRefresh",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                expect( autorefresh.AddFileToRefresh ).to.beA( "function" )
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
                expect( autorefresh.AddFileToRefresh ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non-string argument",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                expect( autorefresh.AddFileToRefresh, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Adds a new file and returns true, returns false for a duplicate",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                local fileName = "AutoRefresh_AddFileToRefresh_Test"

                autorefresh.RemoveFileFromRefresh( fileName )

                expect( autorefresh.AddFileToRefresh( fileName ) ).to.beTrue()
                expect( autorefresh.AddFileToRefresh( fileName ) ).to.beFalse()

                expect( autorefresh.RemoveFileFromRefresh( fileName ) ).to.beTrue()
            end
        },
        {
            name = "Performance calling with an already tracked file",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                local fileName = "AutoRefresh_AddFileToRefresh_Test"
                autorefresh.AddFileToRefresh( fileName )

                HolyLib_RunPerformanceTest("autorefresh.AddFileToRefresh", function() autorefresh.AddFileToRefresh( fileName ) end)

                autorefresh.RemoveFileFromRefresh( fileName )
            end
        },
    }
}
