return {
    groupName = "cvar.SetValue",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("cvars"),
            func = function()
                expect( cvar.SetValue ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("cvars"),
            func = function()
                expect( cvar ).to.beA( "nil" )
            end
        },
        {
            name = "Properly creates a buffer from a string",
            when = HolyLib_IsModuleEnabled("cvars"),
            func = function()
            	local cheats = GetConVar( "sv_cheats" )
            	local val = cheats:GetString()

                expect( cvar.SetValue("sv_cheats", "10") ).to.beTrue()

                expect( cheats:GetString() ).to.equal( "10" )

                cvar.SetValue( "sv_cheats", val )
            end
        },
    }
}
