return {
    groupName = "LuaInterface:__index",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( FindMetaTable("LuaInterface").__index ).to.beA( "function" )
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
            name = "Returns registered methods from the meta table",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                local interfaceObj = luathreads.CreateInterface()

                expect( interfaceObj.GetName ).to.beA( "function" )
            end
        },
        {
            name = "Returns the right value from the user table",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                local interfaceObj = luathreads.CreateInterface()

                interfaceObj.test = "Hello World"
                expect( interfaceObj.test ).to.equal( "Hello World" )
            end
        },
    }
}
