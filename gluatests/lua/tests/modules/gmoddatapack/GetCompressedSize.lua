return {
    groupName = "gmoddatapack.GetCompressedSize",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("gmoddatapack"),
            func = function()
                expect( gmoddatapack.GetCompressedSize ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("gmoddatapack"),
            func = function()
                expect( gmoddatapack ).to.beA( "nil" )
            end
        },
        {
            name = "Requires a string argument",
            when = HolyLib_IsModuleEnabled("gmoddatapack"),
            func = function()
                expect( gmoddatapack.GetCompressedSize ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non-string, non-number argument",
            when = HolyLib_IsModuleEnabled("gmoddatapack"),
            func = function()
                expect( gmoddatapack.GetCompressedSize, true ).to.errWith( "bad argument #1 to '?' (string expected, got boolean)" )
            end
        },
        {
            name = "Returns nil for a file that was never sent to clients",
            when = HolyLib_IsModuleEnabled("gmoddatapack"),
            func = function()
                expect( gmoddatapack.GetCompressedSize( "GmodDataPack_GetCompressedSize_Test" ) ).to.beNil()
            end
        },
    }
}
