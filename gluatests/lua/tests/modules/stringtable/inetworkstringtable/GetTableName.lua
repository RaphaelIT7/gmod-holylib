return {
    groupName = "INetworkStringTable:GetTableName",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( FindMetaTable("INetworkStringTable").GetTableName ).to.beA( "function" )
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
            name = "Returns the name given at creation",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                stringtable.AllowCreation( true )
                local tableName = GetTestStringTableName()
                local table = stringtable.CreateStringTable( tableName, 4096, 0, 0 )
                stringtable.AllowCreation( false )

                expect( table:GetTableName() ).to.equal( tableName )

                stringtable.RemoveTable( table )
            end
        },
        {
            name = "Errors when called on something that isn't a INetworkStringTable",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( FindMetaTable("INetworkStringTable").GetTableName, 5 ).to.errWith( "bad argument #1 to '?' (Tried to use something that wasn't a INetworkStringTable!)" )
            end
        },
    }
}
