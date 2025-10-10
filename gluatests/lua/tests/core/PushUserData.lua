return {
    groupName = "HolyLib manages to properly Push HolyLib UserData to Lua",
    cases = {
        {
            name = "Is _HOLYLIB_CORE.PushTestUserData function existent",
            func = function()
                expect( _HOLYLIB_CORE.PushTestUserData ).to.beA( "function" )
            end
        },
        {
            name = "Pushes an UserData object to Lua correctly",
            func = function()
                local userdata = _HOLYLIB_CORE.PushTestUserData()
                expect( userdata ).to.beA( "_HOLYLIB_CORE_TEST" )
            end
        },
        {
            name = "Performance",
            func = function()
                HolyLib_RunPerformanceTest("_HOLYLIB_CORE.PushTestUserData", _HOLYLIB_CORE.PushTestUserData)
            end
        },
    }
}
