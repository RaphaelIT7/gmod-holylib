return {
    groupName = "LuaInterface:__newindex",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( FindMetaTable("LuaInterface").__newindex ).to.beA( "function" )
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
                expect( interfaceObj.test ).to.equal( "Hello World" )
                expect( interfaceObj:GetTable().test ).to.equal( "Hello World" )
            end
        },
    }
}
