return {
    groupName = "jit.markFFITypeAsValidUserData",
    cases = {
        {
            name = "Function exists on the jit table",
            when = HolyLib_IsModuleEnabled("luajit"),
            func = function()
                expect( jit.markFFITypeAsValidUserData ).to.beA( "function" )
            end
        },
        {
            name = "Function does not exist when the module is disabled",
            when = not HolyLib_IsModuleEnabled("luajit"),
            func = function()
                expect( jit.markFFITypeAsValidUserData ).to.beA( "nil" )
            end
        },
        {
            name = "Returns nothing and does not error when called without any arguments",
            when = HolyLib_IsModuleEnabled("luajit"),
            func = function()
                local result = jit.markFFITypeAsValidUserData()
                expect( result ).to.beNil()
            end
        },
        {
            name = "Returns nothing and does not error when called with the wrong argument types",
            when = HolyLib_IsModuleEnabled("luajit"),
            func = function()
                local result = jit.markFFITypeAsValidUserData( "not a number", "not cdata" )
                expect( result ).to.beNil()
            end
        },
        {
            name = "Returns nothing and does not error when called with plausible looking arguments",
            when = HolyLib_IsModuleEnabled("luajit"),
            func = function()
                local result = jit.markFFITypeAsValidUserData( TypeID( Vector() ), 0 )
                expect( result ).to.beNil()
            end
        },
    }
}
