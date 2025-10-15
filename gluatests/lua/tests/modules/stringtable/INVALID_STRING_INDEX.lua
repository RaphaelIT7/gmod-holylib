return {
    groupName = "stringtable.INVALID_STRING_INDEX",
    cases = {
        {
            name = "Value exists on table",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( stringtable.INVALID_STRING_INDEX ).to.beA( "number" )
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
            name = "Has the right value",
            when = HolyLib_IsModuleEnabled("stringtable"),
            func = function()
                expect( stringtable.INVALID_STRING_INDEX ).to.equal( 65535 )
            end
        },
    }
}
