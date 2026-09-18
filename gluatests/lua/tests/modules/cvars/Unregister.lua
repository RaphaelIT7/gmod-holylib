return {
    groupName = "cvar.Unregister",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("cvars"),
            func = function()
                expect( cvar.Unregister ).to.beA( "function" )
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
            name = "Properly unregisters a ConVar given its name",
            when = HolyLib_IsModuleEnabled("cvars"),
            func = function()
                CreateConVar( "Cvars_Unregister_Test", "1", FCVAR_NONE, "Test convar for cvar.Unregister" )

                expect( cvar.Find( "Cvars_Unregister_Test" ) ).toNot.beNil()

                cvar.Unregister( "Cvars_Unregister_Test" )

                expect( cvar.Find( "Cvars_Unregister_Test" ) ).to.beNil()
            end
        },
        {
            name = "Properly unregisters a ConVar given the ConVar itself",
            when = HolyLib_IsModuleEnabled("cvars"),
            func = function()
                CreateConVar( "Cvars_Unregister_Test", "1", FCVAR_NONE, "Test convar for cvar.Unregister" )

                local convar = cvar.Find( "Cvars_Unregister_Test" )
                expect( convar ).toNot.beNil()

                cvar.Unregister( convar )

                expect( cvar.Find( "Cvars_Unregister_Test" ) ).to.beNil()
            end
        },
        {
            name = "Errors when the ConVar/ConCommand couldn't be found",
            when = HolyLib_IsModuleEnabled("cvars"),
            func = function()
                expect( cvar.Unregister, "Cvars_Unregister_Test" ).to.errWith( "bad argument #1 to '?' (Failed to find ConVar/ConCommand!)" )
            end
        },
    }
}
