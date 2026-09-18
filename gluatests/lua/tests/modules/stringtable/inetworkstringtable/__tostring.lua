return {
    groupName = "INetworkStringTable:__tostring",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( FindMetaTable("INetworkStringTable").__tostring ).to.beA( "function" )
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
            name = "Returns the table name",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                stringtable.AllowCreation( true )
                local tableName = GetTestStringTableName()
                local table = stringtable.CreateStringTable( tableName, 4096, 0, 0 )
                stringtable.AllowCreation( false )

                expect( table:__tostring() ).to.equal( "INetworkStringTable [" .. tableName .. "]" )

                stringtable.RemoveTable( table )
            end
        },
        {
            name = "Returns [NULL] after the table was removed",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                stringtable.AllowCreation( true )
                local table = stringtable.CreateStringTable( GetTestStringTableName(), 4096, 0, 0 )
                stringtable.AllowCreation( false )

                stringtable.RemoveTable( table )

                expect( table:__tostring() ).to.equal( "INetworkStringTable [NULL]" )
            end
        },
        {
            name = "Returns [NULL] when called on something that isn't a INetworkStringTable",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( FindMetaTable("INetworkStringTable").__tostring( 5 ) ).to.equal( "INetworkStringTable [NULL]" )
            end
        },
    }
}
