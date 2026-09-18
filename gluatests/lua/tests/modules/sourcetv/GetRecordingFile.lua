return {
    groupName = "sourcetv.GetRecordingFile",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv.GetRecordingFile ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv ).to.beA( "nil" )
            end
        },
        {
            name = "Returns nil when no SourceTV server is recording",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv.GetRecordingFile() ).to.beNil()
            end
        },
    }
}
