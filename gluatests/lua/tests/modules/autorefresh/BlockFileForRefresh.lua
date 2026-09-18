return {
    groupName = "autorefresh.BlockFileForRefresh",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                expect( autorefresh.BlockFileForRefresh ).to.beA( "function" )
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
                expect( autorefresh.BlockFileForRefresh ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non-string argument",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                expect( autorefresh.BlockFileForRefresh, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Blocking, double blocking, unblocking and double unblocking a file",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                local fileName = "AutoRefresh_BlockFileForRefresh_Test"

                autorefresh.BlockFileForRefresh( fileName, false )

                expect( autorefresh.BlockFileForRefresh( fileName, true ) ).to.beTrue()
                expect( autorefresh.BlockFileForRefresh( fileName, true ) ).to.beFalse()

                expect( autorefresh.BlockFileForRefresh( fileName, false ) ).to.beTrue()
                expect( autorefresh.BlockFileForRefresh( fileName, false ) ).to.beFalse()
            end
        },
        {
            name = "Omitting the block argument behaves like passing false",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                local fileName = "AutoRefresh_BlockFileForRefresh_Test"

                expect( autorefresh.BlockFileForRefresh( fileName ) ).to.beFalse()
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                local fileName = "AutoRefresh_BlockFileForRefresh_Test"

                HolyLib_RunPerformanceTest("autorefresh.BlockFileForRefresh", function() autorefresh.BlockFileForRefresh( fileName, true ) end)

                autorefresh.BlockFileForRefresh( fileName, false )
            end
        },
    }
}
