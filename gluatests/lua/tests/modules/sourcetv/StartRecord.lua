return {
    groupName = "sourcetv.StartRecord",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv.StartRecord ).to.beA( "function" )
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
            name = "Requires a string argument",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv.StartRecord ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non-string argument",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv.StartRecord, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Returns RECORD_NOSOURCETV when no SourceTV server exists",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv.StartRecord( "SourceTV_StartRecord_Test" ) ).to.equal( sourcetv.RECORD_NOSOURCETV )
            end
        },
        {
            name = "Returns RECORD_NOSOURCETV instead of RECORD_INVALIDPATH for an invalid path when no SourceTV server exists",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv.StartRecord( "../../invalid/path" ) ).to.equal( sourcetv.RECORD_NOSOURCETV )
            end
        },
    }
}
