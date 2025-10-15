return {
    groupName = "stringtable.SetLocked",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( stringtable.SetLocked ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( stringtable ).to.beA( "nil" )
            end
        },
        {
            name = "Sets the right value",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                stringtable.SetLocked( false )

                expect( stringtable.IsLocked() ).to.beFalse()

                stringtable.SetLocked( true )

                expect( stringtable.IsLocked() ).to.beTrue()

                stringtable.SetLocked( false )

                expect( stringtable.IsLocked() ).to.beFalse()
            end
        },
    }
}
