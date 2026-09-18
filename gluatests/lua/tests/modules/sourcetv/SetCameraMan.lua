return {
    groupName = "sourcetv.SetCameraMan",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv.SetCameraMan ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( sourcetv ).to.beA( "nil" )
            end
        },
        {
            name = "Returns nothing and does not error when called without any arguments",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                local result = sourcetv.SetCameraMan()

                expect( result ).to.beNil()
            end
        },
        {
            name = "Returns nothing and does not error when called with an Entity argument",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                local result = sourcetv.SetCameraMan( NULL )

                expect( result ).to.beNil()
            end
        },
    }
}
