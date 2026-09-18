return {
    groupName = "HolyLib.GetLadder",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.GetLadder ).to.beA( "function" )
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
                expect( HolyLib.GetLadder, NULL ).to.errWith( "bad argument #1 to '?' (Tried to use a NULL Entity!)" )
            end
        },
        {
            name = "Returns NULL for a bot that isn't on a ladder",
            when = HolyLib_IsModuleEnabled("HolyLib") and IS_BASE_BRANCH,
            func = function()
                local bot = MakeTestBot()

                expect( HolyLib.GetLadder( bot ) ).to.equal( NULL )

                bot:Kick()
            end
        },
    }
}
