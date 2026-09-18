return {
    groupName = "unholylib.KillLua",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("unholylib"),
            func = function()
                expect( unholylib.KillLua ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("unholylib"),
            func = function()
                expect( unholylib ).to.beA( "nil" )
            end
        },
    }
}
