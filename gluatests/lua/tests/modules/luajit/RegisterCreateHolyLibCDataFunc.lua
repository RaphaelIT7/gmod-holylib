return {
    groupName = "jit.registerCreateHolyLibCDataFunc",
    cases = {
        {
            name = "Function exists on the jit table",
            when = HolyLib_IsModuleEnabled("luajit"),
            func = function()
                expect( jit.registerCreateHolyLibCDataFunc ).to.beA( "function" )
            end
        },
        {
            name = "Function does not exist when the module is disabled",
            when = not HolyLib_IsModuleEnabled("luajit"),
            func = function()
                expect( jit.registerCreateHolyLibCDataFunc ).to.beA( "nil" )
            end
        },
        {
            name = "Returns nothing and does not error when called without any arguments",
            when = HolyLib_IsModuleEnabled("luajit"),
            func = function()
                local result = jit.registerCreateHolyLibCDataFunc()
                expect( result ).to.beNil()
            end
        },
        {
            name = "Returns nothing and does not error when called with the wrong argument types",
            when = HolyLib_IsModuleEnabled("luajit"),
            func = function()
                local result = jit.registerCreateHolyLibCDataFunc( 123, "not a table", "not a function" )
                expect( result ).to.beNil()
            end
        },
        {
            name = "Returns nothing and does not error when called with plausible looking arguments",
            when = HolyLib_IsModuleEnabled("luajit"),
            func = function()
                local result = jit.registerCreateHolyLibCDataFunc( "int", {}, function() end )
                expect( result ).to.beNil()
            end
        },
    }
}
