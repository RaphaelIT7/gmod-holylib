return {
    groupName = "HolyLib manages to properly Get Players from Lua",
    cases = {
        {
            name = "Is _HOLYLIB_CORE.GetPlayer function existent",
            func = function()
                expect( _HOLYLIB_CORE.GetPlayer ).to.beA( "function" )
            end
        },
    }
}
