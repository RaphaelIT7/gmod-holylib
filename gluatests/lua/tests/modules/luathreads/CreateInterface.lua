return {
    groupName = "luathreads.CreateInterface",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( luathreads.CreateInterface ).to.beA( "function" )
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
            name = "Properly creates a valid LuaInterface with the expected defaults",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                local interfaceObj = luathreads.CreateInterface()

                expect( interfaceObj ).toNot.beNil()
                expect( interfaceObj:IsValid() ).to.beTrue()
                expect( interfaceObj:GetName() ).to.equal( "NONAME" )
                expect( interfaceObj:CanThink() ).to.beFalse()
            end
        },
    }
}
