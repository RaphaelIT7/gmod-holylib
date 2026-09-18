return {
    groupName = "autorefresh.RemoveFolderFromRefresh",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                expect( autorefresh.RemoveFolderFromRefresh ).to.beA( "function" )
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
                expect( autorefresh.RemoveFolderFromRefresh ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non-string argument",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                expect( autorefresh.RemoveFolderFromRefresh, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Returns false for a folder that was never watched",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                expect( autorefresh.RemoveFolderFromRefresh( "AutoRefresh_RemoveFolderFromRefresh_Test" ) ).to.beFalse()
            end
        },
        {
            name = "Removing a folder matches whether it was actually added",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                local folder = "garrysmod"
                local added = autorefresh.AddFolderToRefresh( folder, false )
                local removed = autorefresh.RemoveFolderFromRefresh( folder, false )

                if added then
                    expect( removed ).to.beTrue()
                else
                    expect( removed ).to.beFalse()
                end
            end
        },
    }
}
