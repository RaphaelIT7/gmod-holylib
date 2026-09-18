return {
    groupName = "INetworkStringTable:GetTable",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( FindMetaTable("INetworkStringTable").GetTable ).to.beA( "function" )
            end
        },
        {
            name = "Metatable doesn't exist",
            when = not HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( FindMetaTable("INetworkStringTable") ).to.beA( "nil" )
            end
        },
        {
            name = "Returns the lua table",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                stringtable.AllowCreation( true )
                local table = stringtable.CreateStringTable( GetTestStringTableName(), 4096, 0, 0 )
                stringtable.AllowCreation( false )

                table.test = "Hello World"
                expect( table:GetTable().test ).to.equal( "Hello World" )

                table:GetTable().test = "Hello World 2"
                expect( table.test ).to.equal( "Hello World 2" )

                stringtable.RemoveTable( table )
            end
        },
        {
            name = "Errors when called on something that isn't a INetworkStringTable",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( FindMetaTable("INetworkStringTable").GetTable, 5 ).to.errWith( "bad argument #1 to '?' (Tried to use something that wasn't a INetworkStringTable!)" )
            end
        },
    }
}
