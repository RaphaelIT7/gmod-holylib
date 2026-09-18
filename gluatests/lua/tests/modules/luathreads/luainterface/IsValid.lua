return {
    groupName = "LuaInterface:IsValid",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( FindMetaTable("LuaInterface").IsValid ).to.beA( "function" )
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
            name = "Returns true for a freshly created interface",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                local interfaceObj = luathreads.CreateInterface()

                expect( interfaceObj:IsValid() ).to.beTrue()
            end
        },
        {
            name = "Returns false for something that isn't a LuaInterface",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( FindMetaTable("LuaInterface").IsValid( 5 ) ).to.beFalse()
            end
        },
    }
}
