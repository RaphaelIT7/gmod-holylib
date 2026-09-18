return {
    groupName = "unholylib.SetCurTime",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("unholylib"),
            func = function()
                expect( unholylib.SetCurTime ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("unholylib"),
            func = function()
                expect( unholylib ).to.beA( "nil" )
            end
        },
        {
            name = "Changes CurTime to the given value",
            when = HolyLib_IsModuleEnabled("unholylib"),
            func = function()
                local original = CurTime()

                unholylib.SetCurTime( original + 1000 )

                expect( math.abs( CurTime() - ( original + 1000 ) ) < 0.01 ).to.beTrue()

                unholylib.SetCurTime( original )
                expect( math.abs( CurTime() - original ) < 0.01 ).to.beTrue()
            end
        },
    }
}
