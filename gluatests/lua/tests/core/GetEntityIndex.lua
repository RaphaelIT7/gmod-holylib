return {
    groupName = "HolyLib has the correct CBaseEdict layout",
    cases = {
        {
            name = "Is _HOLYLIB_CORE.GetEntityIndex function existent",
            func = function()
                expect( _HOLYLIB_CORE.GetEntityIndex ).to.beA( "function" )
            end
        },
        { -- We test this due to GMod apparently switching on the VALVE_LITTLE_ENDIAN flag which changes the CBaseEdict fields!
            name = "Finds the correct Entity Index",
            when = player.GetCount() > 0,
            func = function()
                local ply = player.GetAll()[1]
                local holylibIndex = _HOLYLIB_CORE.GetEntityIndex(ply)
                local gmodIndex = ply:EntIndex()

                expect( holylibIndex ).to.equal( gmodIndex )
            end
        },
    }
}
