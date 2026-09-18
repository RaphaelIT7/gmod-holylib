return {
    groupName = "HolyLib exposes a Crash function for crash-handler testing",
    cases = {
        {
            name = "Is _HOLYLIB_CORE.Crash function existent",
            func = function()
                expect( _HOLYLIB_CORE.Crash ).to.beA( "function" )
            end
        },
    }
}
