return {
    groupName = "LuaInterface:GetTable",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( FindMetaTable("LuaInterface").GetTable ).to.beA( "function" )
            end
        },
        {
            name = "Metatable doesn't exist",
            when = not HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( FindMetaTable("LuaInterface") ).to.beA( "nil" )
            end
        },
        {
            name = "Sets the right value",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                local interfaceObj = luathreads.CreateInterface()

                interfaceObj.test = "Hello World"
                expect( interfaceObj:GetTable().test ).to.equal( "Hello World" )

                interfaceObj:GetTable().test = "Hello World 2"
                expect( interfaceObj.test ).to.equal( "Hello World 2" )
            end
        },
        {
            name = "Errors when called on something that isn't a LuaInterface",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( FindMetaTable("LuaInterface").GetTable, 5 ).to.errWith( "bad argument #1 to '?' (Tried to use something that wasn't a LuaInterface!)" )
            end
        },
    }
}
