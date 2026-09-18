return {
    groupName = "LuaInterface:__tostring",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( FindMetaTable("LuaInterface").__tostring ).to.beA( "function" )
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
            name = "Returns the right value using the default name",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                local interfaceObj = luathreads.CreateInterface()

                expect( tostring( interfaceObj ) ).to.equal( "LuaInterface [NONAME]" )
            end
        },
        {
            name = "Returns the right value after SetName was used",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                local interfaceObj = luathreads.CreateInterface()
                interfaceObj:SetName( "LuaInterface_SetName_Test" )

                expect( tostring( interfaceObj ) ).to.equal( "LuaInterface [LuaInterface_SetName_Test]" )
            end
        },
        {
            name = "Returns 'LuaInterface [NULL]' for something that isn't a LuaInterface",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( FindMetaTable("LuaInterface").__tostring( 5 ) ).to.equal( "LuaInterface [NULL]" )
            end
        },
    }
}
