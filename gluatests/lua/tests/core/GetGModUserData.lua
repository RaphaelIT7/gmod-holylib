return {
    groupName = "HolyLib manages to properly Get GMod UserData from Lua",
    cases = {
        {
            name = "Is _HOLYLIB_CORE.GetGModVector function existent",
            func = function()
                expect( _HOLYLIB_CORE.GetGModVector ).to.beA( "function" )
            end
        },
        {
            name = "Pushes an Entity to Lua correctly",
            func = function()
                local success = _HOLYLIB_CORE.GetGModVector(Vector(1, 2, 3)) -- Returns false or crashes if anything was wrong. We check for x = 1, y = 2, z = 3

                expect( success ).to.beTrue()
            end
        },
        {
            name = "Performance",
            func = function()
                HolyLib_RunPerformanceTest("_HOLYLIB_CORE.RawGetGModVector", _HOLYLIB_CORE.RawGetGModVector, Vector(1, 2, 3))
            end
        },
    }
}
