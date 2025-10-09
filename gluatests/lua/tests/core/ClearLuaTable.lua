return {
    groupName = "HolyLib manages to properly Push Entities to Lua",
    cases = {
        {
            name = "Is _HOLYLIB_CORE.ClearLuaTable function existent",
            func = function()
                expect( _HOLYLIB_CORE.ClearLuaTable ).to.beA( "function" )
            end
        },
        {
            name = "Clears Lua Table correctly",
            func = function()
                local userdata = _HOLYLIB_CORE.PushTestUserData()
                userdata.example = "Test"
                expect( userdata.example ).to.equal( "Test" )

                _HOLYLIB_CORE.ClearLuaTable(userdata)
                expect( userdata.example ).to.beNil()
            end
        },
        {
            name = "Performance",
            func = function()
                local userdata = _HOLYLIB_CORE.PushTestUserData()
                HolyLib_RunPerformanceTest("_HOLYLIB_CORE.ClearLuaTable", _HOLYLIB_CORE.ClearLuaTable, userdata)
            end
        },
    }
}
