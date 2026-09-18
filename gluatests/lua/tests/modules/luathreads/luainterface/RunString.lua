return {
    groupName = "LuaInterface:RunString",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( FindMetaTable("LuaInterface").RunString ).to.beA( "function" )
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
            name = "Queues a RunString task without erroring",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                local interfaceObj = luathreads.CreateInterface()

                interfaceObj:RunString( "print('hello from a luathreads task')" )
            end
        },
        {
            name = "Requires a string argument (leaks a RunStringTask on failure, see note above)",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                local interfaceObj = luathreads.CreateInterface()

                expect( interfaceObj.RunString, interfaceObj ).to.errWith( "bad argument #2 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when called on something that isn't a LuaInterface",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( FindMetaTable("LuaInterface").RunString, 5, "print('hi')" ).to.errWith( "bad argument #1 to '?' (Tried to use something that wasn't a LuaInterface!)" )
            end
        },
    }
}
