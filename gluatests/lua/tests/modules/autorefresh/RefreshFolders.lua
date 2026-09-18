return {
    groupName = "autorefresh.RefreshFolders",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                expect( autorefresh.RefreshFolders ).to.beA( "function" )
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
            -- No perf test: module is unstable and can crash (see README).
            name = "Can be called without arguments and returns nothing",
            when = HolyLib_IsModuleEnabled("autorefresh"),
            func = function()
                expect( autorefresh.RefreshFolders() ).to.beNil()
            end
        },
    }
}
