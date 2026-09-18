return {
    groupName = "jit.getMetaTableByID",
    cases = {
        {
            name = "Function exists on the jit table",
            when = HolyLib_IsModuleEnabled("luajit"),
            func = function()
                expect( jit.getMetaTableByID ).to.beA( "function" )
            end
        },
        {
            name = "Function does not exist when the module is disabled",
            when = not HolyLib_IsModuleEnabled("luajit"),
            func = function()
                expect( jit.getMetaTableByID ).to.beA( "nil" )
            end
        },
        {
            name = "Returns nothing and does not error when called without any arguments",
            when = HolyLib_IsModuleEnabled("luajit"),
            func = function()
                local result = jit.getMetaTableByID()
                expect( result ).to.beNil()
            end
        },
        {
            name = "Returns nil instead of the metatable for a valid, in-use metaID",
            when = HolyLib_IsModuleEnabled("luajit"),
            func = function()
                local vectorTypeID = TypeID( Vector() )
                local result = jit.getMetaTableByID( vectorTypeID )
                expect( result ).to.beNil()
            end
        },
        {
            name = "Returns nothing and does not error for an out of range metaID",
            when = HolyLib_IsModuleEnabled("luajit"),
            func = function()
                local result = jit.getMetaTableByID( 255 )
                expect( result ).to.beNil()
            end
        },
    }
}
