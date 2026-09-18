return {
    groupName = "LuaInterface:SetName",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( FindMetaTable("LuaInterface").SetName ).to.beA( "function" )
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
            name = "Properly sets the name",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                local interfaceObj = luathreads.CreateInterface()

                interfaceObj:SetName( "LuaInterface_SetName_Test" )
                expect( interfaceObj:GetName() ).to.equal( "LuaInterface_SetName_Test" )
            end
        },
        {
            name = "Requires a string argument",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                local interfaceObj = luathreads.CreateInterface()

                expect( interfaceObj.SetName, interfaceObj ).to.errWith( "bad argument #2 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Errors when called on something that isn't a LuaInterface",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( FindMetaTable("LuaInterface").SetName, 5, "LuaInterface_SetName_Test" ).to.errWith( "bad argument #1 to '?' (Tried to use something that wasn't a LuaInterface!)" )
            end
        },
    }
}
