return {
    groupName = "HolyLib manages to properly Push Entities to Lua",
    cases = {
        {
            name = "Is _HOLYLIB_CORE.GetModuleData function existent",
            func = function()
                expect( _HOLYLIB_CORE.GetModuleData ).to.beA( "function" )
            end
        },
        {
            name = "Internally verifies the pointer it got",
            func = function()
                -- If we got a garbage pointer, this will return false or crash.
                expect( _HOLYLIB_CORE.GetModuleData() ).to.beTrue()
            end
        },
        {
            name = "Performance",
            func = function()
                HolyLib_RunPerformanceTest("_HOLYLIB_CORE.GetModuleData", _HOLYLIB_CORE.RawGetModuleData)
            end
        },
    }
}
