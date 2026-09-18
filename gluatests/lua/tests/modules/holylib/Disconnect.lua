return {
    groupName = "HolyLib.Disconnect",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.Disconnect ).to.beA( "function" )
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
                expect( HolyLib.Disconnect, NULL, "HolyLib_Disconnect_Test" ).to.errWith( "bad argument #1 to '?' (Tried to use a NULL Entity!)" )
            end
        },
        {
            name = "Errors when called without a reason string",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.Disconnect, 999999999 ).to.errWith( "bad argument #2 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Returns false for an unknown userid",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.Disconnect( 999999999, "test" ) ).to.beFalse()
            end
        },
        {
            name = "Immediately disconnects a bot and returns true",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                local bot = MakeTestBot()

                local ok, err = pcall( function()
                    expect( HolyLib.Disconnect( bot, "HolyLib_Disconnect_Test", false, true ) ).to.beTrue()
                    expect( IsValid( bot ) ).to.beFalse()
                end )

                if IsValid( bot ) then
                    bot:Kick()
                end

                if not ok then
                    error( err, 0 )
                end
            end
        },
    }
}
