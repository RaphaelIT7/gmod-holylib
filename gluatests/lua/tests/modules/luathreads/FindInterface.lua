return {
    groupName = "luathreads.FindInterface",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( luathreads.FindInterface ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( luathreads ).to.beA( "nil" )
            end
        },
        {
            name = "Requires a string argument",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( luathreads.FindInterface ).to.errWith( "bad argument #1 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Returns nil when no interface with that name exists",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( luathreads.FindInterface( "LuaThreads_FindInterface_Test" ) ).to.beNil()
            end
        },
        {
            name = "Finds a previously named interface by name",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                local interfaceObj = luathreads.CreateInterface()
                interfaceObj:SetName( "LuaThreads_FindInterface_Test" )

                expect( luathreads.FindInterface( "LuaThreads_FindInterface_Test" ) ).to.equal( interfaceObj )
            end
        },
    }
}
