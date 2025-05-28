return {
    groupName = "stringtable.CreateStringTableEx",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( stringtable.CreateStringTableEx ).to.beA( "function" )
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
            name = "Properly creates a table",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                stringtable.AllowCreation( true )
                local table = stringtable.CreateStringTableEx( GetTestStringTableName(), 4096, 0, 0, false )

                expect( table:IsValid() ).to.beTrue()
                expect( table:IsFilenames() ).to.beFalse()

                stringtable.RemoveTable( table )

                local table = stringtable.CreateStringTableEx( GetTestStringTableName(), 4096, 0, 0, true )
                stringtable.AllowCreation( false )

                expect( table:IsFilenames() ).to.beTrue()

                stringtable.RemoveTable( table )
            end
        },
    }
}
