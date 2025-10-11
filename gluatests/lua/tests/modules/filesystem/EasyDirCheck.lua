return {
    groupName = "FileSystem:EasyDirCheck",
    cases = {
        {
            name = "Skips directory check if folder name contains a dot after the last /",
            when = HolyLib_IsModuleEnabled( "HolyLib" ) && HolyLib_IsModuleEnabled( "filesystem" ), -- We depend on HolyLib for convar specific tests. We could switch to use HolyTest / include it which would remove this dependency
            func = function(state)
                RunConsoleCommand("holylib_filesystem_easydircheck", "0")
                HolyLib.ServerExecute()

                expect(file.IsDir("addons/gluatests/test.folder", "MOD")).to.beTrue()
                expect(file.IsDir("addons/gluatests/test.folder/folder2", "MOD")).to.beTrue()

                RunConsoleCommand("holylib_filesystem_easydircheck", "1")
                HolyLib.ServerExecute()

                expect(file.IsDir("addons/gluatests/test.folder", "MOD")).to.beFalse() -- It's expected behavior for this to fail.
                expect(file.IsDir("addons/gluatests/test.folder/folder2", "MOD")).to.beTrue()

                RunConsoleCommand("holylib_filesystem_easydircheck", GetConVar("holylib_filesystem_easydircheck"):GetDefault()) -- Restoring to default
                HolyLib.ServerExecute()
            end,
        },
    }
}
