return {
    groupName = "HolyLib manages to properly Push Entities to Lua",
    cases = {
        {
            name = "Is _HOLYLIB_CORE.GetTestUserData function existent",
            func = function()
                expect( _HOLYLIB_CORE.GetTestUserData ).to.beA( "function" )
            end
        },
        {
            name = "Properly got the valid pointer to our userdata",
            func = function()
                local userdata = _HOLYLIB_CORE.PushTestUserData()
                expect( userdata ).to.beA( "_HOLYLIB_CORE_TEST" )

                -- If we got a garbage pointer, this will return false or crash.
                expect( _HOLYLIB_CORE.GetTestUserData(userdata) ).to.beTrue()
            end
        },
        {
            name = "__newindex and __index functions properly work on userdata",
            func = function()
                local userdata = _HOLYLIB_CORE.PushTestUserData()
                expect( userdata ).to.beA( "_HOLYLIB_CORE_TEST" )

                userdata.example = "Hello World"

                expect( userdata.example ).to.equal( "Hello World" )
            end
        },
        {
            name = "Performance",
            func = function()
                local userdata = _HOLYLIB_CORE.PushTestUserData()
                HolyLib_RunPerformanceTest("_HOLYLIB_CORE.GetTestUserData", _HOLYLIB_CORE.RawGetTestUserData, userdata)

                HolyLib_RunPerformanceTest("_HOLYLIB_CORE.__newindex", function(userdata) userdata.example = "Hello World" end, userdata)
                HolyLib_RunPerformanceTest("_HOLYLIB_CORE.__index", function(userdata) return userdata.example end, userdata)
            end
        },
    }
}
