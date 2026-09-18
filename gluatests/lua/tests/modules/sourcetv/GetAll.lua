return {
    groupName = "sourcetv.GetAll",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv.GetAll ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv ).to.beA( "nil" )
            end
        },
        {
            name = "Returns an empty table when no SourceTV server exists",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                local clients = sourcetv.GetAll()

                expect( clients ).to.beA( "table" )
                expect( table.Count( clients ) ).to.equal( 0 )
            end
        },
    }
}
