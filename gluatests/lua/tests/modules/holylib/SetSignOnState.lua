return {
    groupName = "HolyLib.SetSignOnState",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.SetSignOnState ).to.beA( "function" )
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
                expect( HolyLib.SetSignOnState, NULL, 0 ).to.errWith( "bad argument #1 to '?' (Tried to use a NULL Entity!)" )
            end
        },
        {
            name = "Errors when called without a signOnState number",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.SetSignOnState, 999999999 ).to.errWith( "bad argument #2 to '?' (number expected, got no value)" )
            end
        },
        {
            name = "Returns false for an unknown userid",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.SetSignOnState( 999999999, 0 ) ).to.beFalse()
            end
        },
        {
            name = "rawSet directly assigns the signon state and returns true",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                local bot = MakeTestBot()

                expect( HolyLib.SetSignOnState( bot, 6, 0, true ) ).to.beTrue()

                bot:Kick()
            end
        },
        {
            name = "rawSet also accepts a UserID number instead of a Player",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                local bot = MakeTestBot()

                expect( HolyLib.SetSignOnState( bot:UserID(), 6, 0, true ) ).to.beTrue()

                bot:Kick()
            end
        },
    }
}
