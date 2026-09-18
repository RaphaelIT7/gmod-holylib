return {
    groupName = "LuaInterface:CanThink",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( FindMetaTable("LuaInterface").CanThink ).to.beA( "function" )
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
            name = "Returns false for a freshly created interface",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                local interfaceObj = luathreads.CreateInterface()

                expect( interfaceObj:CanThink() ).to.beFalse()
            end
        },
        {
            name = "Errors when called on something that isn't a LuaInterface",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( FindMetaTable("LuaInterface").CanThink, 5 ).to.errWith( "bad argument #1 to '?' (Tried to use something that wasn't a LuaInterface!)" )
            end
        },
    }
}
