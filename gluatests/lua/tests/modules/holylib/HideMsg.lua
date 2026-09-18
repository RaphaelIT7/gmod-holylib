return {
    groupName = "HolyLib.HideMsg",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.HideMsg ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib ).to.beA( "nil" )
            end
        },
        {
            name = "Errors when called without a string",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.HideMsg ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Can be called to add and remove an entry without erroring",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                HolyLib.HideMsg( "HolyLib_HideMsg_Test", false )
                HolyLib.HideMsg( "HolyLib_HideMsg_Test", true )
            end
        },
    }
}
