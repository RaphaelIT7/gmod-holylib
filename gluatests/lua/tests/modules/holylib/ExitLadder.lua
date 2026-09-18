return {
    groupName = "HolyLib.ExitLadder",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.ExitLadder ).to.beA( "function" )
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
                expect( HolyLib.ExitLadder, NULL ).to.errWith( "bad argument #1 to '?' (Tried to use a NULL Entity!)" )
            end
        },
        {
            name = "Forces a bot off the ladder without erroring, even if it isn't on one",
            when = HolyLib_IsModuleEnabled("HolyLib") and IS_BASE_BRANCH,
            func = function()
                local bot = MakeTestBot()

                HolyLib.ExitLadder( bot )

                bot:Kick()
            end
        },
    }
}
