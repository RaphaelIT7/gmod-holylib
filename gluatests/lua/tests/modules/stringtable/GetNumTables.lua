return {
    groupName = "stringtable.GetNumTables",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( stringtable.GetNumTables ).to.beA( "function" )
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
            name = "Properly reports the numbers",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                local startCount = stringtable.GetNumTables()

                stringtable.AllowCreation( true )
                local table = stringtable.CreateStringTable( GetTestStringTableName(), 4096, 0, 0 )
                stringtable.AllowCreation( false )

                expect( table:IsValid() ).to.beTrue()
                expect( stringtable.GetNumTables() ).to.equal( startCount + 1 )

                stringtable.RemoveTable( table )

                expect( stringtable.GetNumTables() ).to.equal( startCount )
            end
        },
    }
}
