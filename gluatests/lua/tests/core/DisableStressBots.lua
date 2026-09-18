return {
    groupName = "HolyLib manages to properly disable the sv_stressbots ConVar",
    cases = {
        {
            name = "Is _HOLYLIB_CORE.DisableStressBots function existent",
            func = function()
                expect( _HOLYLIB_CORE.DisableStressBots ).to.beA( "function" )
            end
        },
        {
            name = "Disables sv_stressbots correctly",
            func = function()
                local convar = GetConVar("sv_stressbots")
                local wasEnabled = convar and convar:GetBool() or false

                local success = _HOLYLIB_CORE.DisableStressBots()

                expect( success ).to.beTrue()
                expect( convar:GetBool() ).to.beFalse()

                if wasEnabled then
                    _HOLYLIB_CORE.EnableStressBots()
                end
            end
        },
    }
}
