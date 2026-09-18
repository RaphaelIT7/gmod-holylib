return {
    groupName = "HolyLib.GetRegistry",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.GetRegistry ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib ).to.beA( "nil" )
            end
        },
        {
            name = "Returns the Lua registry table, or throws the exact unsafe-code error",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                local ok, result = pcall( HolyLib.GetRegistry )

                if ok then
                    expect( result ).to.beA( "table" )
                    expect( next( result ) ).toNot.beNil()
                else
                    expect( result ).to.equal( "Tried to use a unsafe code function while -holylib_allowunsafe is not active!" )
                end
            end
        },
    }
}
