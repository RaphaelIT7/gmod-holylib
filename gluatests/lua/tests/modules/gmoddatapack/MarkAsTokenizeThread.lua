return {
    groupName = "gmoddatapack.MarkAsTokenizeThread",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("gmoddatapack"),
            func = function()
                expect( gmoddatapack.MarkAsTokenizeThread ).to.beA( "function" )
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
            name = "Doesn't error when called",
            when = HolyLib_IsModuleEnabled("gmoddatapack"),
            func = function()
                gmoddatapack.MarkAsTokenizeThread()
            end
        },
        {
            name = "Returns nothing",
            when = HolyLib_IsModuleEnabled("gmoddatapack"),
            func = function()
                expect( gmoddatapack.MarkAsTokenizeThread() ).to.beNil()
            end
        },
        {
            name = "Doesn't error when called multiple times in a row",
            when = HolyLib_IsModuleEnabled("gmoddatapack"),
            func = function()
                gmoddatapack.MarkAsTokenizeThread()
                gmoddatapack.MarkAsTokenizeThread()
                gmoddatapack.MarkAsTokenizeThread()
            end
        },
    }
}
