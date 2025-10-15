return {
    groupName = "stringtable.SetAllowClientSideAddString",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( stringtable.SetAllowClientSideAddString ).to.beA( "function" )
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
            name = "Properly enables clientside AddString",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                stringtable.AllowCreation( true )
                local table = stringtable.CreateStringTable( GetTestStringTableName(), 4096, 0, 0 )
                stringtable.AllowCreation( false )

                expect( table:IsValid() ).to.beTrue()

                stringtable.SetAllowClientSideAddString( table, true )

                expect( table:IsClientSideAddStringAllowed() ).to.beTrue()

                stringtable.RemoveTable( table )
            end
        },
    }
}
