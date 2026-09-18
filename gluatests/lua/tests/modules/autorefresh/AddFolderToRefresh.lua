return {
    groupName = "autorefresh.AddFolderToRefresh",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                expect( autorefresh.AddFolderToRefresh ).to.beA( "function" )
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
                expect( autorefresh.AddFolderToRefresh ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non-string argument",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                expect( autorefresh.AddFolderToRefresh, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Returns false for a folder that doesn't exist",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                expect( autorefresh.AddFolderToRefresh( "AutoRefresh_AddFolderToRefresh_Test" ) ).to.beFalse()
            end
        },
        {
            name = "Doesn't error when the recursive argument is omitted",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                local ok = pcall( autorefresh.AddFolderToRefresh, "AutoRefresh_AddFolderToRefresh_Test" )

                expect( ok ).to.beTrue()
            end
        },
        {
            name = "Returns a boolean when pointed at a real folder and can be cleaned up",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                local folder = "garrysmod"

                expect( autorefresh.AddFolderToRefresh( folder, false ) ).to.beA( "boolean" )

                autorefresh.RemoveFolderFromRefresh( folder, false )
            end
        },
    }
}
