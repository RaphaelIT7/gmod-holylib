return {
    groupName = "HolyLib managed to be loaded",
    cases = {
        {
            name = "Is _HOLYLIB set in _G",
            func = function()
                expect( _HOLYLIB ).to.beTrue()
            end
        },
    }
}
