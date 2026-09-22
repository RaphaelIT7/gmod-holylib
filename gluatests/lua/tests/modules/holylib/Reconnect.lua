return {
    groupName = "HolyLib.Reconnect",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.Reconnect ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib ).to.beA( "nil" )
            end
        },
        {
            name = "Errors instead of crashing when given a NULL entity",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.Reconnect, NULL ).to.errWith( "bad argument #1 to '?' (Tried to use a NULL Entity!)" )
            end
        },
        {
            name = "Errors when given a non-Player entity",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                local ent = MakeTestEntity()

                expect( HolyLib.Reconnect, ent ).to.errWith( "bad argument #1 to '?' (Entity is not a player!)" )

                SafeRemoveEntity( ent )
            end
        },
        {
            name = "Returns false for a bot (bots have no net channel)",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                local bot = MakeTestBot()

                -- Bots get a CNetChan when sv_stressbots is enabled so this can actually pass
                expect( HolyLib.Reconnect( bot ) ).to.equal( GetConVar("sv_stressbots"):GetBool() )

                if IsValid( bot ) then
                	bot:Kick()
                end
            end
        },
    }
}
