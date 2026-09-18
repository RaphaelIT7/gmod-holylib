return {
    groupName = "HolyLib.FadeClientVolume",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.FadeClientVolume ).to.beA( "function" )
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
            name = "Errors instead of crashing when given a NULL entity",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.FadeClientVolume, NULL, 1, 1, 1, 1 ).to.errWith( "bad argument #1 to '?' (Tried to use a NULL Entity!)" )
            end
        },
        {
            name = "Errors when called without the fade arguments",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                local bot = MakeTestBot()

                expect( HolyLib.FadeClientVolume, bot ).to.errWith( "bad argument #2 to '?' (number expected, got no value)" )

                bot:Kick()
            end
        },
        {
            name = "Returns nothing",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                local bot = MakeTestBot()

                local ret = HolyLib.FadeClientVolume( bot, 0.5, 1, 2, 3 )
                expect( ret ).to.beNil()

                bot:Kick()
            end
        },
    }
}
