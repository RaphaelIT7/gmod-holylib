return {
    groupName = "CHLTVClient:SetCameraMan",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( FindMetaTable("CHLTVClient").SetCameraMan ).to.beA( "function" )
            end
        },
        {
            name = "Metatable doesn't exist",
            when = not HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( FindMetaTable("CHLTVClient") ).to.beA( "nil" )
            end
        },
        {
            name = "Errors when self is not a valid CHLTVClient",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                local setCameraMan = FindMetaTable("CHLTVClient").SetCameraMan

                expect( setCameraMan, "not a client" ).to.errWith( "bad argument #1 to '?' (Tried to use something that wasn't a CHLTVClient!)" )
            end
        },
        {
            name = "Errors when called without any arguments",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                local setCameraMan = FindMetaTable("CHLTVClient").SetCameraMan

                expect( setCameraMan ).to.errWith( "bad argument #1 to '?' (Tried to use something that wasn't a CHLTVClient!)" )
            end
        },
    }
}
