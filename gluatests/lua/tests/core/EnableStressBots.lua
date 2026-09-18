return {
    groupName = "HolyLib manages to properly enable the sv_stressbots ConVar",
    cases = {
        {
            name = "Is _HOLYLIB_CORE.EnableStressBots function existent",
            func = function()
                expect( _HOLYLIB_CORE.EnableStressBots ).to.beA( "function" )
            end
        },
        {
            name = "Enables sv_stressbots correctly",
            func = function()
                local convar = GetConVar("sv_stressbots")
                local wasEnabled = convar and convar:GetBool() or false

                local success = _HOLYLIB_CORE.EnableStressBots()

                expect( success ).to.beTrue()
                expect( convar:GetBool() ).to.beTrue()

                if not wasEnabled then
                    _HOLYLIB_CORE.DisableStressBots()
                end
            end
        },
    }
}
