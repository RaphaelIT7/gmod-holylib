return {
    groupName = "HolyLib manages to properly Push HolyLib Referenced UserData to Lua",
    cases = {
        {
            name = "Is _HOLYLIB_CORE.PushReferencedTestUserData function existent",
            func = function()
                expect( _HOLYLIB_CORE.PushReferencedTestUserData ).to.beA( "function" )
            end
        },
        {
            name = "Pushes an UserData object to Lua correctly",
            func = function()
                local userdata = _HOLYLIB_CORE.PushReferencedTestUserData()
                expect( userdata ).to.beA( "_HOLYLIB_CORE_TEST_REFERENCED" )
            end
        },
        {
            name = "Performance",
            func = function()
                HolyLib_RunPerformanceTest("_HOLYLIB_CORE.PushReferencedTestUserData", function() _HOLYLIB_CORE.PushReferencedTestUserData() end)
            end
        },
    }
}
