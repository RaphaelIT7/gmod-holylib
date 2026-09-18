return {
    groupName = "luathreads.GetInterfaces",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                expect( luathreads.GetInterfaces ).to.beA( "function" )
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
            name = "Returns a table containing every existing LuaInterface",
            when = HolyLib_IsModuleEnabled("luathreads"),
            func = function()
                local before = luathreads.GetInterfaces()
                local beforeCount = #before

                local interfaceObj = luathreads.CreateInterface()

                local after = luathreads.GetInterfaces()
                expect( #after ).to.equal( beforeCount + 1 )

                local found = false
                for _, entry in ipairs( after ) do
                    if entry == interfaceObj then
                        found = true
                        break
                    end
                end

                expect( found ).to.beTrue()
            end
        },
    }
}
