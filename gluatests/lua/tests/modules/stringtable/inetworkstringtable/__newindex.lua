return {
    groupName = "INetworkStringTable:__newindex",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( FindMetaTable("INetworkStringTable").__newindex ).to.beA( "function" )
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
            name = "Sets the value on the lua table",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                stringtable.AllowCreation( true )
                local table = stringtable.CreateStringTable( GetTestStringTableName(), 4096, 0, 0 )
                stringtable.AllowCreation( false )

                table.test = "Hello World"
                expect( table.test ).to.equal( "Hello World" )
                expect( table:GetTable().test ).to.equal( "Hello World" )

                stringtable.RemoveTable( table )
            end
        },
        {
            name = "Errors when called on something that isn't a INetworkStringTable",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( FindMetaTable("INetworkStringTable").__newindex, 5, "test", "value" ).to.errWith( "bad argument #1 to '?' (Tried to use something that wasn't a INetworkStringTable!)" )
            end
        },
    }
}
