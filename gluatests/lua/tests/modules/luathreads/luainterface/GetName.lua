return {
    groupName = "LuaInterface:GetName",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( FindMetaTable("LuaInterface").GetName ).to.beA( "function" )
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
            name = "Returns 'NONAME' by default",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                local interfaceObj = luathreads.CreateInterface()

                expect( interfaceObj:GetName() ).to.equal( "NONAME" )
            end
        },
        {
            name = "Errors when called on something that isn't a LuaInterface",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( FindMetaTable("LuaInterface").GetName, 5 ).to.errWith( "bad argument #1 to '?' (Tried to use something that wasn't a LuaInterface!)" )
            end
        },
    }
}
