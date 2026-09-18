return {
    groupName = "HolyLib.HideServer",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.HideServer ).to.beA( "function" )
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
            name = "Errors when called without a boolean",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.HideServer ).to.errWith( "bad argument #1 to '?' (boolean expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non-boolean",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.HideServer, "true" ).to.errWith( "bad argument #1 to '?' (boolean expected, got string)" )
            end
        },
        {
            name = "Toggles the hide_server convar",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                local convar = GetConVar( "hide_server" )
                expect( convar ).toNot.beNil()

                local old = convar:GetBool()

                HolyLib.HideServer( true )
                expect( convar:GetBool() ).to.beTrue()

                HolyLib.HideServer( false )
                expect( convar:GetBool() ).to.beFalse()

                HolyLib.HideServer( old )
            end
        },
    }
}
