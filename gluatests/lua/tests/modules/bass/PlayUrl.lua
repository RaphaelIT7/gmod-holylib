return {
    groupName = "bass.PlayURL",

    cases = {
        {
            name = "Function exists when module enabled",
            when = HolyLib_IsModuleEnabled( "bass" ),
            func = function()
                expect( bass.PlayURL ).to.beA( "function" )
            end
        },
        {
            name = "Function is nil when module disabled",
            when = not HolyLib_IsModuleEnabled( "bass" ),
            func = function()
                expect( bass ).to.beA( "nil" )
            end
        },
    }
}