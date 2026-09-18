return {
    groupName = "INetworkStringTable:SetTick",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( FindMetaTable("INetworkStringTable").SetTick ).to.beA( "function" )
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
            name = "Sets the tick, or throws the unsafe-code error",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                stringtable.AllowCreation( true )
                local table = stringtable.CreateStringTable( GetTestStringTableName(), 4096, 0, 0 )
                stringtable.AllowCreation( false )

                local ok, err = pcall( table.SetTick, table, 100 )

                if ok then
                    expect( table:IsValid() ).to.beTrue()
                else
                    expect( err ).to.equal( "Tried to use a unsafe code function while -holylib_allowunsafe is not active!" )
                end

                stringtable.RemoveTable( table )
            end
        },
        {
            name = "Errors when called on something that isn't a INetworkStringTable",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( FindMetaTable("INetworkStringTable").SetTick, 5, 0 ).to.errWith( "bad argument #1 to '?' (Tried to use something that wasn't a INetworkStringTable!)" )
            end
        },
    }
}
