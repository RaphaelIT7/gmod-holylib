return {
    groupName = "stringtable.IsCreationAllowed",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( stringtable.IsCreationAllowed ).to.beA( "function" )
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
            name = "Gives the proper results",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                stringtable.AllowCreation( false )

                expect( stringtable.IsCreationAllowed() ).to.beFalse()

                stringtable.AllowCreation( true )

                expect( stringtable.IsCreationAllowed() ).to.beTrue()

                stringtable.AllowCreation( false )

                expect( stringtable.IsCreationAllowed() ).to.beFalse()
            end
        },
    }
}
