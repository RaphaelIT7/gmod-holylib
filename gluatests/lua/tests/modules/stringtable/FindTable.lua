return {
    groupName = "stringtable.FindTable",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( stringtable.FindTable ).to.beA( "function" )
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
            name = "Properly reports the right table",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                stringtable.AllowCreation( true )
                local tableName = GetTestStringTableName()
                local table = stringtable.CreateStringTable( tableName, 4096, 0, 0 )
                stringtable.AllowCreation( false )

                expect( table:IsValid() ).to.beTrue()
                expect( stringtable.FindTable( tableName ) ).to.equal( table )

                stringtable.RemoveTable( table )
            end
        },
    }
}
