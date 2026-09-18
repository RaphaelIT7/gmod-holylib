return {
    groupName = "HolyLib.EntityMessageBegin",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.EntityMessageBegin ).to.beA( "function" )
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
                expect( HolyLib.EntityMessageBegin, NULL, false ).to.errWith( "bad argument #1 to '?' (Tried to use a NULL Entity!)" )
            end
        },
        {
            name = "Returns a bf_write buffer that can be written to and finished",
            when = HolyLib_IsModuleEnabled("HolyLib") and HolyLib_IsModuleEnabled("bitbuf"),
            func = function()
                local ent = MakeTestEntity( nil, nil, true )

                local bf = HolyLib.EntityMessageBegin( ent, false )

                local ok, err = pcall( function()
                    expect( bf ).to.beA( "bf_write" )
                    expect( bf:IsValid() ).to.beTrue()

                    bf:WriteByte( 123 )
                end )

                HolyLib.MessageEnd()
                SafeRemoveEntity( ent )

                if not ok then
                    error( err, 0 )
                end
            end
        },
    }
}
